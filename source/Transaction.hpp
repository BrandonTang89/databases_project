#pragma once
#include "DataTuple.hpp"
#include "DeadlockDetector.hpp"
#include "Relation.hpp"
#include "Stage.hpp"
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
  friend class Stage;
  friend class DeadlockDetector;
  std::ostream &out;
  TID tid;
  size_t txBornAt{0};
  std::unordered_map<RelName, Relation> &relations;
  TransactionState state{TransactionState::READY};
  std::unordered_set<Lock *> held_locks; // for cleanup on finish
  std::flat_set<Lock *> required_locks;  // for deadlock detection

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
  std::vector<QueryAtom> query_atoms;         // |query|
  std::flat_map<std::string, size_t> var_idx; // |num_vars|
  std::vector<Stage> stages;                  // |query|
  std::vector<size_t> input_variables; // num vars in channels[i], |query| + 1
  size_t num_answers{0};

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
  StatusCode resume_query();

public:
  bool acquire(Lock &lock, LockMode mode);
  Transaction(std::ostream &output_stream, const TID &transaction_id,
              size_t _age, std::unordered_map<RelName, Relation> &rels);

  // Requires the transaction to be in READY state.
  StatusCode start_edit(Relation *rel, const std::string &csv_file,
                        bool newAlive);

  // Requires the transaction to be in READY state
  StatusCode start_query(std::vector<QueryAtom> query_atoms);

  // Requires the transaction to be in the suspend state
  StatusCode resume(bool silent_resume = false);

  // Requires the transaction to be in READY state
  StatusCode commit();

  // Rollback can be done from any state without issue
  StatusCode rollback(bool silent_abort = false);

  /**
   * We should clear required locks upon successful completion of an operation
   * and when resuming (done manually in resume(bool))
   */
  void clear_required_locks() { required_locks.clear(); }

  const TID &get_tid() const { return tid; }
};
