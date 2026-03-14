#include "Transaction.hpp"
#include "DataTuple.hpp"
#include "Relation.hpp"
#include "common.hpp"
#include "csv_parsing_functions.hpp"
#include <cstddef>
#include <flat_map>
#include <iostream>
#include <print>
#include <string_view>
#include <variant>
#include <vector>

bool Transaction::acquire(Lock &lock, LockMode mode) {
  if (lock.acquire(tid, mode)) {
    held_locks.insert(&lock);
    return true;
  }
  return false;
}

Transaction::Transaction(std::ostream &output_stream, const TID &transaction_id,
                         std::unordered_map<RelName, Relation> &relations)
    : out(output_stream), tid(transaction_id), relations(relations) {}

bool Transaction::is_suspended() const {
  return state == TransactionState::EXECUTING_QUERY ||
         state == TransactionState::EXECUTING_ADD ||
         state == TransactionState::EXECUTING_DELETE;
}

void Transaction::print_time_taken() const {
  auto now = std::chrono::high_resolution_clock::now();
  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - command_start_time)
                         .count();
  std::println(out, "Command time:      {} ms", duration_ms);
}

void Transaction::store_original(DataTuple *tp) {
  if (original_alive.find(tp) == original_alive.end()) {
    original_alive[tp] = tp->alive;
  }
}

StatusCode Transaction::resume() {
  if (state == TransactionState::EXECUTING_QUERY) {
    println("Resuming transaction {}.", tid);
    todo("implementresume query transaction");
  } else if (state == TransactionState::EXECUTING_ADD ||
             state == TransactionState::EXECUTING_DELETE) {
    println("Resuming transaction {}.", tid);
    return resume_edit();
  } else {
    std::println(out, "ERROR: transaction is not suspended");
    std::println(out, "Current state: {}", transaction_state_names[state]);
  }
  return StatusCode::SUCCESS;
}

StatusCode Transaction::start_edit(Relation *rel, const std::string &csv_file,
                                   bool new_alive) {
  if (state != TransactionState::READY) {
    std::println(out, "ERROR: transaction is not in READY state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return StatusCode::SUCCESS;
  }
  command_start_time = std::chrono::high_resolution_clock::now();
  incoming_tuples = parse_csv_file(csv_file);
  incoming_index = 0;
  num_modified = 0;
  trace("Parsed {} tuples from '{}'", incoming_tuples.size(), csv_file);
  target_relation = rel;

  if (new_alive) {
    state = TransactionState::EXECUTING_ADD;
  } else {
    state = TransactionState::EXECUTING_DELETE;
  }
  return resume_edit();
}

StatusCode Transaction::resume_edit() {
  if (state != TransactionState::EXECUTING_ADD &&
      state != TransactionState::EXECUTING_DELETE) {
    std::println(out, "ERROR: transaction is not in EXECUTING_ADD or "
                      "EXECUTING_DELETE state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return StatusCode::SUCCESS;
  }

  bool is_adding = (state == TransactionState::EXECUTING_ADD);

  if (!target_relation->whole_rel_lock.permits(tid)) {
    debug("Transaction is waiting for whole_rel_lock");
    return StatusCode::SUSPENDED;
  }

  for (; incoming_index < incoming_tuples.size(); ++incoming_index) {
    auto &[left, right] = incoming_tuples[incoming_index];

    bool result = target_relation->edit_tuple(*this, left, right, is_adding);
    if (!result) {
      debug("Transaction is waiting to add tuple ({}, {})", left, right);
      return StatusCode::SUSPENDED;
    }
  }

  std::string_view action = is_adding ? "added:      " : "deleted:    ";
  std::println(out, "Tuples {}{}", action, num_modified);
  print_time_taken();

  state = TransactionState::READY;
  return StatusCode::SUCCESS;
}

StatusCode Transaction::start_query(std::vector<QueryAtom> query_atoms) {
  if (state != TransactionState::READY) {
    std::println(out, "ERROR: transaction is not in READY state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return StatusCode::SUCCESS;
  }

  // Check that all relations exist before starting execution
  for (const auto &atom : query_atoms) {
    if (relations.find(atom.relation) == relations.end()) {
      std::println(out, "ERROR: relation {} does not exist.", atom.relation);
      return StatusCode::SUCCESS;
    }
  }

  command_start_time = std::chrono::high_resolution_clock::now();
  query = std::move(query_atoms);
  query_index = 0;
  state = TransactionState::EXECUTING_QUERY;
  return resume_query();
}

StatusCode Transaction::resume_query() {
  if (state != TransactionState::EXECUTING_QUERY) {
    std::println(out, "ERROR: transaction is not in EXECUTING_QUERY state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return StatusCode::SUCCESS;
  }

  for (; query_index < query.size(); ++query_index) {
    debug("Processing query atom {}/{}: {}", query_index + 1, query.size(),
          query[query_index].to_string());
    const auto &atom = query[query_index];
    const auto &rel_name = atom.relation;
    Relation &rel = relations[rel_name];
    QueryArg left = atom.left;
    QueryArg right = atom.right;

    auto const_const = [&rel, this](Constant c, Constant d) {
      int l = c.value;
      int r = d.value;

      DataTuple *tp = rel.get_tuple(l, r);
      if (acquire(tp->lock, LockMode::SHARED)) {
        if (tp->alive) {
          return StatusCode::SUCCESS;
        }
      } else {
        debug("Transaction {} is waiting to acquire lock for tuple ({}, {})",
              tid, l, r);
        return StatusCode::SUSPENDED;
      }
      assert(false);
    };
    auto const_var = [&rel, this](Constant c, Variable y) {
      int l = c.value;
      Group &group = rel.leftToRightIndex[l];
      if (acquire(group.lock, LockMode::SHARED)) {
        debug("Transaction {} is waiting for left group lock for value {}", tid,
              l);
        return StatusCode::SUSPENDED;
      }

      if (query_results.contains(y.name)) {
        // this is a filter
        size_t n = query_results[y.name].size();
        std::vector<size_t> filtered_indices;
        for (size_t i = 0; i < n; i++) {
          if (group.find(l, query_results[y.name][i])) {
            filtered_indices.push_back(i);
          }
        }
        filter_query_results(filtered_indices);
      } else {
        // cartesian product
        std::vector<int> vals;
        for (DataTuple *tp : group.tuples) {
          if (tp->alive) {
            vals.push_back(tp->right);
          }
        }
        cartesian_product(y.name, vals);
      }

      return StatusCode::SUCCESS;
    };
    auto var_const = [](Variable v, Constant c) {
      std::string var_name = v.name;
      int r = c.value;
      return StatusCode::SUCCESS;
    };
    auto var_var = [](Variable v1, Variable v2) {
      std::string var_name1 = v1.name;
      std::string var_name2 = v2.name;
      return StatusCode::SUCCESS;
    };
    const auto visitor = overloaded{const_const, const_var, var_const, var_var};
    StatusCode status = std::visit(visitor, left, right);
    if (status == StatusCode::SUSPENDED) {
      return StatusCode::SUSPENDED;
    }
    query_index++;
  }

  for (const auto &[var_name, vals] : query_results) {
    std::println(out, "{}: {}", var_name, vals);
  }
  print_time_taken();

  state = TransactionState::READY;
  return StatusCode::SUCCESS;
}

StatusCode Transaction::commit() {
  if (state != TransactionState::READY) {
    std::println(out, "ERROR: transaction is not in READY state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return is_suspended() ? StatusCode::SUSPENDED : StatusCode::SUCCESS;
  }
  state = TransactionState::COMMITTED;
  release_locks();

  std::println(out, "Transaction {} committed.", tid);
  return StatusCode::SUCCESS;
}

StatusCode Transaction::rollback() {
  if (state != TransactionState::READY) {
    std::println(out, "ERROR: transaction is not in READY state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return is_suspended() ? StatusCode::SUSPENDED : StatusCode::SUCCESS;
  }
  state = TransactionState::ABORTED;

  // Rollback any modifications
  for (auto &[tp, original] : original_alive) {
    tp->alive = original;
  }
  release_locks();

  std::println(out, "Transaction {} was rolled back.", tid);
  return StatusCode::SUCCESS;
}

void Transaction::filter_query_results(
    std::span<const size_t> filtered_indices) {
  for (auto &[var_name, vals] : query_results) {
    std::vector<int> filtered_vals;
    for (size_t i : filtered_indices) {
      filtered_vals.push_back(vals[i]);
    }
    query_results_p[var_name] = std::move(filtered_vals);
  }
}

void Transaction::cartesian_product(std::string var_name,
                                    std::span<const int> vals) {

  if (query_results.empty()) {
    // if there is nothing to cross, just let query_results =vals
    query_results[var_name] = std::vector<int>(vals.begin(), vals.end());
    return;
  }

  size_t n = query_results.begin()->second.size();
  for (auto &[existing_var, existing_vals] : query_results) {
    // copy each variable |vals| times
    for (int _ : vals) {
      query_results_p[existing_var].append_range(existing_vals);
    }
  }
  for (int val : vals) {
    // put |n| copies of each val to match with each existing_vals
    for (size_t i = 0; i < n; i++) {
      query_results_p[var_name].push_back(val);
    }
  }
}