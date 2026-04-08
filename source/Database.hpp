#pragma once
#include "DeadlockDetector.hpp"
#include "Relation.hpp"
#include "Transaction.hpp"
#include "common.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>

class Database {
private:
  std::ostream &out;
  std::unordered_map<RelName, Relation> relations;
  std::unordered_map<TID, Transaction> transactions;
  DeadlockDetector deadlock_detector{transactions};
  
  /**Call after control is returned from a transaction operation.
   * deals with deadlock detection
   */
  void on_control(const TID &tid, StatusCode status);

public:
  explicit Database(std::ostream &output_stream = std::cout) : out(output_stream) {}

  bool begin_transaction(const TID &tid);

  bool resume_transaction(const TID &tid, bool silent_resume = false);

  bool commit_transaction(const TID &tid);

  bool rollback_transaction(const TID &tid, bool silent_abort = false);

  // Returns false if the transaction does not exist or the file cannot be
  // opened.  On success prints the number of imported tuples and returns true.
  bool add_data(const TID &tid, const RelName &rel_name,
                const std::string &csv_file);

  // Returns false if the transaction does not exist, the relation does not
  // exist, or the file cannot be opened.  On success prints the number of
  // deleted tuples and returns true.
  bool delete_data(const TID &tid, const RelName &rel_name,
                   const std::string &csv_file);

  // Returns false if the transaction does not exist
  bool query(const TID &tid, std::vector<QueryAtom> body);
};
