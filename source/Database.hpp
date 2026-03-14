#pragma once
#include "ConflictGraph.hpp"
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
  std::unordered_map<std::string, Relation> relations;
  std::unordered_map<TID, Transaction> transactions;
  ConflictGraph conflict_graph;

  // Call after control is returned from a transaction operation.
  void on_control(const TID &tid, StatusCode status) {
    if (status == StatusCode::SUSPENDED) {
      std::println(out, "Transaction {} was suspended.", tid);
      conflict_graph.process(tid);
    }
  }

public:
  Database(std::ostream &output_stream = std::cout) : out(output_stream) {}

  bool begin_transaction(const TID &tid);

  bool resume_transaction(const TID &tid);

  bool commit_transaction(const TID &tid);

  bool rollback_transaction(const TID &tid);

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