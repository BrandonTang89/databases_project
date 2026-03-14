#include "Transaction.hpp"
#include "Relation.hpp"
#include "common.hpp"
#include "csv_parsing_functions.hpp"
#include <flat_map>
#include <iostream>
#include <print>
#include <vector>

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
  command_start_time = std::chrono::high_resolution_clock::now();
  query = std::move(query_atoms);
  query_index = 0;
  state = TransactionState::EXECUTING_QUERY;
  return StatusCode::SUSPENDED;
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