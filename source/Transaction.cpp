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
  } else {
    required_locks.insert(&lock);
    return false;
  }
}

Transaction::Transaction(std::ostream &output, const TID &transaction_id,
                         size_t _age,
                         std::unordered_map<RelName, Relation> &rels)
    : out(output), tid(transaction_id), txBornAt(_age), relations(rels) {}

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
  original_alive.emplace(tp, tp->alive);
}

StatusCode Transaction::resume(bool silent_resume) {
  required_locks.clear();
  if (state == TransactionState::EXECUTING_QUERY) {
    if (!silent_resume) {
      println("Resuming transaction {}.", tid);
    }
    return resume_query();
  } else if (state == TransactionState::EXECUTING_ADD ||
             state == TransactionState::EXECUTING_DELETE) {
    if (!silent_resume) {
      std::println(out, "Resuming transaction {}.", tid);
    }
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
  held_locks.reserve(held_locks.size() + incoming_tuples.size());
  original_alive.reserve(original_alive.size() + incoming_tuples.size());
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

  required_locks.emplace(&target_relation->whole_rel_lock);
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

StatusCode Transaction::start_query(std::vector<QueryAtom> query) {
  if (state != TransactionState::READY) {
    std::println(out, "ERROR: transaction is not in READY state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return StatusCode::SUCCESS;
  }
  command_start_time = std::chrono::high_resolution_clock::now();
  state = TransactionState::EXECUTING_QUERY;
  num_answers = 0;

  // Check that all relations exist before starting execution
  // Also fill in var_idx to map each variable name with an index
  size_t num_vars = 0;
  var_idx.clear();
  for (const auto &atom : query) {
    if (relations.find(atom.relation) == relations.end()) {
      std::println(out, "ERROR: relation {} does not exist.", atom.relation);
      return StatusCode::SUCCESS;
    }
    for (const auto &arg : {atom.left, atom.right}) {
      if (std::holds_alternative<Variable>(arg)) {
        const std::string &var_name = std::get<Variable>(arg).name;
        if (var_idx.find(var_name) == var_idx.end()) {
          var_idx[var_name] = num_vars++;
        }
      }
    }
  }

  // Build the stages
  this->query_atoms = std::move(query); // the stages need the query atoms
  stages.clear();
  stages.reserve(query_atoms.size() + 1);
  for (size_t i = 0; i <= query_atoms.size(); ++i) {
    // left to right order is critical to set up properly
    stages.emplace_back(i, *this); // internally will set up as needed
  }

  return resume_query();
}

StatusCode Transaction::resume_query() {
  if (state != TransactionState::EXECUTING_QUERY) {
    std::println(out, "ERROR: transaction is not in EXECUTING_QUERY state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return StatusCode::SUCCESS;
  }

  while (true) {
    PipelineStatus st = stages.back().next();
    if (st == PipelineStatus::OK) {
      num_answers++;
      for (size_t i = 0; i < stages.back().num_output_vars; ++i) {
        std::cout << stages.back().get_out_channel()->at(i);
        if (i < stages.back().num_output_vars - 1) {
          std::cout << ",";
        }
      }
      std::cout << "\n";
      continue;
    } else if (st == PipelineStatus::SUSPEND) {
      debug("Transaction is suspended while executing query.");
      return StatusCode::SUSPENDED;
    } else if (st == PipelineStatus::FINISHED) {
      state = TransactionState::READY;
      std::println(out, "Number of answers: {}", num_answers);
      print_time_taken();
      return StatusCode::SUCCESS;
    }
  }
}

StatusCode Transaction::commit() {
  if (state != TransactionState::READY) {
    std::println(out, "ERROR: transaction is not in READY state.");
    std::println(out, "Current state: {}", transaction_state_names[state]);
    return is_suspended() ? StatusCode::SUSPENDED : StatusCode::SUCCESS;
  }
  state = TransactionState::COMMITTED;
  command_start_time = std::chrono::high_resolution_clock::now();
  release_locks();
  std::println(out, "Transaction {} was committed.", tid);

  print_time_taken();
  return StatusCode::SUCCESS;
}

StatusCode Transaction::rollback(bool silent_abort) {
  state = TransactionState::ABORTED;
  if (!silent_abort) {
    // don't clobber the time if this is an internal abort
    command_start_time = std::chrono::high_resolution_clock::now();
  }

  // Rollback any modifications
  for (auto &[tp, original] : original_alive) {
    tp->alive = original;
  }
  release_locks();

  if (!silent_abort) { // otherwise don't need to say
    std::println(out, "Transaction {} was rolled back.", tid);
    print_time_taken();
  }
  return StatusCode::SUCCESS;
}
