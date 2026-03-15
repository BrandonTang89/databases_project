#include "Stage.hpp"
#include "Transaction.hpp"
#include "common.hpp"
#include <variant>

Stage::Stage(size_t stage_index, Transaction &trx)
    : stage_idx(stage_index), tx(trx) {

  if (stage_idx == 0) {
    type = StageType::INITIAL;
    return;
  }

  atom = &tx.query_atoms[stage_idx - 1];
  previous = &tx.stages[stage_idx - 1];
  input = previous->get_out_channel();
  num_input_vars = previous->num_output_vars;
  rel = &tx.relations.at(atom->relation);

  left_is_const = std::holds_alternative<Constant>(atom->left);
  right_is_const = std::holds_alternative<Constant>(atom->right);
  left_is_var = std::holds_alternative<Variable>(atom->left);
  right_is_var = std::holds_alternative<Variable>(atom->right);

  if (left_is_const && right_is_const) {

    type = StageType::CONST_CONST;
    num_output_vars = num_input_vars;

  } else if ((left_is_const && right_is_var) ||
             (left_is_var && right_is_const)) {
    const auto &constant = left_is_const ? std::get<Constant>(atom->left)
                                         : std::get<Constant>(atom->right);
    const auto &variable = left_is_const ? std::get<Variable>(atom->right)
                                         : std::get<Variable>(atom->left);

    var_idx = tx.var_idx[variable.name];
    const_val = constant.value;
    group = left_is_const ? &rel->leftToRightIndex[const_val]
                          : &rel->rightToLeftIndex[const_val];
    group_setup();

  } else if (left_is_var && right_is_var) {
    var_idx = tx.var_idx[std::get<Variable>(atom->left).name];
    var2_idx = tx.var_idx[std::get<Variable>(atom->right).name];

    if (var_idx == var2_idx) {
      group = &rel->diagonalIndex;
      group_setup();
    } else {

      if (var_idx >= num_input_vars && var2_idx >= num_input_vars) {
        // both variables are new, we need to do a relation product
        type = StageType::RELATION_PRODUCT;
        tx.acquire(rel->whole_rel_lock, LockMode::SHARED);
        num_output_vars = num_input_vars + 2;
        output.resize(num_output_vars);

        if (num_input_vars == 0) {
          // produces the first tuples
          rel_iter = rel->tuples.begin();
        } else {
          // otherwise expect to start from the back...
          rel_iter = rel->tuples.end();
        }

      } else if (var_idx >= num_input_vars) {
        // var_idx is new, var2_idx is existing
        todo("right_join");
      } else if (var2_idx >= num_input_vars) {
        // var2_idx is new, var_idx is existing
        todo("left_join");
      } else {
        type = StageType::RELATION_FILTER;
        tx.acquire(rel->whole_rel_lock, LockMode::SHARED);
        num_output_vars = num_input_vars;
      }
    }
  }
}

void Stage::group_setup() {
  assert(group && "group should have been set already");
  tx.acquire(group->lock, LockMode::SHARED);

  const bool introduces_new_var = var_idx >= num_input_vars;
  if (introduces_new_var) {
    type = StageType::GROUP_PRODUCT;
    num_output_vars = num_input_vars + 1;
    output.resize(num_output_vars);

    if (num_input_vars == 0) {
      // produces the first tuples
      group_iter = group->tuples.begin();
    } else {
      // otherwise expect to start from the back...
      group_iter = group->tuples.end();
    }
  } else {
    type = StageType::GROUP_FILTER;
    num_output_vars = num_input_vars;
  }
}

PipelineStatus Stage::next() {
  switch (type) {
  case StageType::INITIAL:
    return PipelineStatus::OK;
  case StageType::CONST_CONST:
    return next_const_const();
  case StageType::GROUP_FILTER:
    return next_group_filter();
  case StageType::GROUP_PRODUCT:
    return next_group_product();
  case StageType::RELATION_FILTER:
    return next_relation_filter();
  case StageType::RELATION_PRODUCT:
    return next_relation_product();
  case StageType::JOIN_LEFT:
    todo("join_left");
    break;
    // assert(false && "JOIN_LEFT not implemented yet");
    // return execute_join_left();
  case StageType::JOIN_RIGHT:
    todo("join_right");
    break;

    // assert(false && "JOIN_RIGHT not implemented yet");
    // return execute_join_right();
  }
  assert(false && "unreachable");
}

PipelineStatus Stage::next_const_const() {
  const int left_const = std::get<Constant>(atom->left).value;
  const int right_const = std::get<Constant>(atom->right).value;

  DataTuple *tp = rel->get_tuple(left_const, right_const);
  assert(tp && "tuple should always exist");
  if (!tp->lock.is_held_by(tx.tid)) {
    if (!tx.acquire(tp->lock, LockMode::SHARED)) {
      return PipelineStatus::SUSPEND;
    }
  }

  if (tp->alive) {
    // Just pass through everything
    return previous->next();
  } else {
    // End it here, don't need to call previous stage
    return PipelineStatus::FINISHED;
  }
}

PipelineStatus Stage::next_group_filter() {
  while (true) {
    PipelineStatus st = previous->next();
    if (st != PipelineStatus::OK) {
      return st;
    }

    // Check the new input
    if (left_is_const && right_is_var) {
      if (group->find(const_val, input->at(var_idx))->alive) {
        // passes the filter
        return PipelineStatus::OK;
      }
    } else if (left_is_var && right_is_const) {
      if (group->find(input->at(var_idx), const_val)->alive) {
        // passes the filter
        return PipelineStatus::OK;
      }
    } else if (left_is_var && right_is_var) {
      assert(var_idx == var2_idx && "group filter only for diagonal");
      if (group->find(input->at(var_idx), input->at(var2_idx))->alive) {
        // passes the filter
        return PipelineStatus::OK;
      }
    } else {
      assert(false && "invalid setup for group filter");
    }
  }
  assert(false && "unreachable");
}

PipelineStatus Stage::next_group_product() {
  while (true) {
    if (num_input_vars == 0) {
      PipelineStatus st = previous->next();
      if (st != PipelineStatus::OK) {
        return st;
      }

      // Need to produce the first tuple to kick off the pipeline
      if (group_iter == group->tuples.end()) {
        return PipelineStatus::FINISHED;
      }
    } else if (group_iter == group->tuples.end()) {
      // The first iteration should start with the group_iter at the end
      // pull in a new tuple
      PipelineStatus st = previous->next();
      if (st != PipelineStatus::OK) {
        return st;
      }
      std::copy(input->begin(), input->end(), output.begin());
      group_iter = group->tuples.begin();
    }

    const DataTuple *tp = *group_iter;
    group_iter++;
    // Skip dead tuples
    if (!tp->alive) {
      continue;
    }
    output[var_idx] = left_is_const ? tp->right : tp->left;
    // diagonal group either left or right is okay
    return PipelineStatus::OK;
  }
}

PipelineStatus Stage::next_relation_filter() {
  while (true) {
    PipelineStatus st = previous->next();
    if (st != PipelineStatus::OK) {
      return st;
    }

    // Check the new input
    int left_val = input->at(var_idx);
    int right_val = input->at(var2_idx);
    DataTuple *tp = rel->get_tuple(left_val, right_val);
    assert(tp && "tuple should always exist");
    if (!tp->lock.is_held_by(tx.tid)) {
      if (!tx.acquire(tp->lock, LockMode::SHARED)) {
        return PipelineStatus::SUSPEND;
      }
    }

    if (tp->alive) {
      // passes the filter
      return PipelineStatus::OK;
    }
  }
}

PipelineStatus Stage::next_relation_product() {
  while (true) {
    if (num_input_vars == 0) {
      PipelineStatus st = previous->next();
      if (st != PipelineStatus::OK) {
        return st;
      }

      // Need to produce the first tuple to kick off the pipeline
      if (rel_iter == rel->tuples.end()) {
        return PipelineStatus::FINISHED;
      }
    } else if (rel_iter == rel->tuples.end()) {
      // The first iteration should start with the rel_iter at the end
      // pull in a new tuple
      PipelineStatus st = previous->next();
      if (st != PipelineStatus::OK) {
        return st;
      }
      std::copy(input->begin(), input->end(), output.begin());
      rel_iter = rel->tuples.begin();
    }

    DataTuple *tp = &*rel_iter;
    rel_iter++;
    if (!tp->alive) {
      // Skip dead (never-inserted or deleted) tuples
      continue;
    }
    output[var_idx] = tp->left;
    output[var2_idx] = tp->right;
    return PipelineStatus::OK;
  }
}