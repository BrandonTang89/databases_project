#pragma once
#include "DataTuple.hpp"
#include "Relation.hpp"
#include "common.hpp"
#include <chrono>
#include <flat_map>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static std::flat_map<TransactionState, std::string> transaction_state_names = {
    {TransactionState::READY, "READY"},
    {TransactionState::EXECUTING_QUERY, "EXECUTING_QUERY"},
    {TransactionState::EXECUTING_ADD, "EXECUTING_ADD"},
    {TransactionState::EXECUTING_DELETE, "EXECUTING_DELETE"},
    {TransactionState::COMMITTED, "COMMITTED"},
    {TransactionState::ABORTED, "ABORTED"},
};

class Transaction {
  friend class Relation;
  std::ostream &out;
  TID tid;
  TransactionState state{TransactionState::READY};
  std::unordered_set<Lock *> held_locks; // for cleanup on finish

  // Command start time
  std::chrono::high_resolution_clock::time_point command_start_time;

  // State to Rollback
  std::unordered_map<DataTuple *, bool> original_alive;

  // Suspended State for add/delete operations
  Relation *target_relation{nullptr};
  std::vector<std::pair<int, int>> incoming_tuples;
  size_t incoming_index{0}; // next tuple to process
  size_t num_modified{0};

  // Suspended State for query operations
  std::vector<QueryAtom> query;
  size_t query_index{0}; // next atom to process
  std::unordered_map<RelName, std::vector<int>>
      query_results; // intermediate results for query execution

  bool is_suspended() const;
  void print_time_taken() const;

  // Stores the original alive status of the tuple if it hasn't been stored
  // already.
  void store_original(DataTuple *tp);

  void release_locks() {
    for (Lock *lock : held_locks) {
      lock->release(tid);
    }
    held_locks.clear();
  }

  StatusCode resume_edit();
public:
  const TID &get_tid() const { return tid; }
  Transaction(std::ostream &output_stream, const TID &transaction_id)
      : out(output_stream), tid(transaction_id) {}
  StatusCode resume();
  StatusCode start_edit(Relation *rel, const std::string &csv_file, bool newAlive);
  StatusCode start_query(std::vector<QueryAtom> query_atoms);
  // StatusCode resume_query();
  StatusCode commit();
  StatusCode rollback();
};
