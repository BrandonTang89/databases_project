#include "Stage.hpp"
#include "DataTuple.hpp"
#include "Transaction.hpp"
#include "common.hpp"
#include <variant>

Stage::Stage(size_t stage_index, Transaction &trx)
    : stage_idx(stage_index), tx(trx) {

  if (stage_idx == 0) {
    type = StageType::INITIAL;
    return;
  }

  channel = &tx.query_channel;
  atom = &tx.query_atoms[stage_idx - 1];
  previous = &tx.stages[stage_idx - 1];
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
    group = left_is_const ? &rel->l_to_r_index[const_val]
                          : &rel->r_to_l_index[const_val];
    group_setup();

  } else if (left_is_var && right_is_var) {
    var_idx = tx.var_idx[std::get<Variable>(atom->left).name];
    var2_idx = tx.var_idx[std::get<Variable>(atom->right).name];

    if (var_idx == var2_idx) {
      group = &rel->diagonal_index;
      group_setup();
    } else {

      if (var_idx >= num_input_vars && var2_idx >= num_input_vars) {
        // both variables are new, we need to do a relation product
        type = StageType::RELATION_PRODUCT;
        tx.acquire(rel->whole_rel_lock, LockMode::SHARED);
        // acquire never fails beacuse its on an SLock
        num_output_vars = num_input_vars + 2;

        if (num_input_vars == 0) {
          // produces the first tuples
          rel_iter = rel->tuples.begin();
        } else {
          // otherwise expect to start from the back...
          rel_iter = rel->tuples.end();
        }

      } else if (var_idx >= num_input_vars) {
        // var_idx is new, var2_idx is existing
        type = StageType::JOIN_RIGHT;
        num_output_vars = num_input_vars + 1;
      } else if (var2_idx >= num_input_vars) {
        // var2_idx is new, var_idx is existing
        type = StageType::JOIN_LEFT;
        num_output_vars = num_input_vars + 1;
      } else {
        type = StageType::RELATION_FILTER;
        tx.acquire(rel->whole_rel_lock, LockMode::SHARED);
        // acquire never fails beacuse its on an SLock

        num_output_vars = num_input_vars;
      }
    }
  }

  debug("Stage {}: {}", stage_index, stage_type_name(type));
}

void Stage::group_setup() {
  assert(group && "group should have been set already");
  tx.acquire(group->lock, LockMode::SHARED);
  // acquire never fails beacuse its on an SLock

  const bool introduces_new_var = var_idx >= num_input_vars;
  if (introduces_new_var) {
    type = StageType::GROUP_PRODUCT;
    num_output_vars = num_input_vars + 1;

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
    return next_join_left();
  case StageType::JOIN_RIGHT:
    return next_join_right();
  }
  assert(false && "invalid stage type");
  return PipelineStatus::FINISHED; // to silence compiler warning
}

PipelineStatus Stage::next_const_const() {
  const uint32_t left_const = std::get<Constant>(atom->left).value;
  const uint32_t right_const = std::get<Constant>(atom->right).value;

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
    if (call_next) {
      PipelineStatus st = previous->next();
      if (st != PipelineStatus::OK) {
        return st;
      }
    }

    // Check the new input
    DataTuple *tp{};
    if (left_is_const && right_is_var) {
      tp = group->find(const_val, (*channel)[var_idx]);
    } else if (left_is_var && right_is_const) {
      tp = group->find((*channel)[var_idx], const_val);
    } else if (left_is_var && right_is_var) {
      assert(var_idx == var2_idx && "group filter only for diagonal");
      tp = group->find((*channel)[var_idx], (*channel)[var2_idx]);
    } else {
      assert(false && "invalid setup for group filter");
    }

    // tp is null if the tuple doesn't exist in this group at all
    if (!tp) {
      continue;
    }
    if (!tx.get_read_permit(tp->lock)) {
      call_next = false;
      return PipelineStatus::SUSPEND;
    }
    call_next = true;
    if (tp->alive) {
      // passes the filter
      return PipelineStatus::OK;
    } else {
      // doesn't pass the filter, try to get the next tuple from previous stage
      continue;
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
      group_iter = group->tuples.begin();
    }

    DataTuple *tp = group_iter->second;
    if (!tx.get_read_permit(tp->lock))
      return PipelineStatus::SUSPEND;

    group_iter++;
    // Skip dead tuples
    if (!tp->alive) {
      continue;
    }
    (*channel)[var_idx] = left_is_const ? tp->right : tp->left;
    // diagonal group either left or right is okay
    return PipelineStatus::OK;
  }
}

PipelineStatus Stage::next_relation_filter() {
  while (true) {
    if (call_next) {
      PipelineStatus st = previous->next();
      if (st != PipelineStatus::OK) {
        return st;
      }
    }

    // Check the new input
    uint32_t left_val = (*channel)[var_idx];
    uint32_t right_val = (*channel)[var2_idx];
    DataTuple *tp = rel->get_tuple(left_val, right_val);
    assert(tp && "tuple should always exist");
    if (!tx.get_read_permit(tp->lock)) {
      call_next = false;
      return PipelineStatus::SUSPEND;
    }
    call_next = true;
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
      rel_iter = rel->tuples.begin();
    }

    DataTuple *tp = &*rel_iter;
    if (!tx.get_read_permit(tp->lock))
      return PipelineStatus::SUSPEND;
    rel_iter++;
    if (!tp->alive) {
      // Skip dead (never-inserted or deleted) tuples
      continue;
    }
    (*channel)[var_idx] = tp->left;
    (*channel)[var2_idx] = tp->right;
    return PipelineStatus::OK;
  }
}

PipelineStatus Stage::next_join_left() {
  while (true) {
    if (!group_iter_valid) {
      // We need to pull a new one
      PipelineStatus st = previous->next();
      if (st != PipelineStatus::OK) {
        return st;
      }
      group = &rel->l_to_r_index[(*channel)[var_idx]];
      group_iter = group->tuples.begin();
      tx.acquire(group->lock, LockMode::SHARED);
      // acquire never fails beacuse its on an SLock

      group_iter_valid = true;
    }

    while (group_iter != group->tuples.end()) {
      DataTuple *tp = group_iter->second;
      if (!tx.get_read_permit(tp->lock))
        return PipelineStatus::SUSPEND;

      group_iter++;
      // Skip dead tuples
      if (!tp->alive) {
        continue;
      }
      (*channel)[var2_idx] = tp->right;
      return PipelineStatus::OK;
    }
    group_iter_valid = false;
  }
}

PipelineStatus Stage::next_join_right() {
  while (true) {
    if (!group_iter_valid) {
      // We need to pull a new one
      PipelineStatus st = previous->next();
      if (st != PipelineStatus::OK) {
        return st;
      }
      group = &rel->r_to_l_index[(*channel)[var2_idx]];
      group_iter = group->tuples.begin();
      tx.acquire(group->lock, LockMode::SHARED);
      // acquire never fails beacuse its on an SLock

      group_iter_valid = true;
    }

    while (group_iter != group->tuples.end()) {
      DataTuple *tp = group_iter->second;
      if (!tx.get_read_permit(tp->lock))
        return PipelineStatus::SUSPEND;

      group_iter++;
      // Skip dead tuples
      if (!tp->alive) {
        continue;
      }
      (*channel)[var_idx] = tp->left;
      return PipelineStatus::OK;
    }
    group_iter_valid = false;
  }
}
