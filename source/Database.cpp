#include "Database.hpp"
#include "ConflictGraph.hpp"
#include "Relation.hpp"
#include "Transaction.hpp"
#include "common.hpp"
#include <iostream>
#include <print>
#include <string>
#include <unordered_map>

bool Database::begin_transaction(const TID &tid) {
  if (transactions.find(tid) != transactions.end()) {

    std::println(out, "ERROR: transaction {} already exists", tid);
    return false; // Transaction with this TID already exists
  }
  transactions.emplace(tid, Transaction(out));
  std::println(out, "Transaction {} was created.", tid);
  return true;
}

bool Database::commit_transaction(const TID &tid) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist", tid);
    return false;
  }

  std::println(std::cerr, "todo: commit transaction {}", tid);
  transactions.erase(it);
  return true;
}

bool Database::rollback_transaction(const TID &tid) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist", tid);
    return false; // No such transaction
  }

  std::println(std::cerr, "todo: rollback transaction {}", tid);
  transactions.erase(it);
  return true;
}

// Returns false if the transaction does not exist or the file cannot be
// opened.  On success prints the number of imported tuples and returns true.
bool Database::add_data(const TID &tid, const RelName &rel_name,
                        const std::string &csv_file) {
  auto transactionItr = transactions.find(tid);
  if (transactionItr == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist", tid);
    return false;
  }

  Relation &rel = relations[rel_name]; // also creates if doesn't exist
  transactionItr->second.start_add(rel, csv_file);
  return true;
}

// Returns false if the transaction does not exist, the relation does not
// exist, or the file cannot be opened.  On success prints the number of
// deleted tuples and returns true.
bool Database::delete_data(const TID &tid, const RelName &rel_name,
                           const std::string &csv_file) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist", tid);
    return false;
  }

  if (relations.find(rel_name) == relations.end()) {
    std::println(out, "ERROR: relation {} does not exist", rel_name);
    return false;
  }

  Relation &rel = relations[rel_name];
  it->second.start_delete(rel, csv_file);
  return true;
}

bool Database::query(const TID &tid,
                     [[maybe_unused]] std::span<const QueryAtom> body) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist", tid);
    return false;
  }
  return true;
}

bool Database::resume_transaction(const TID &tid) {
  if (transactions.find(tid) == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist", tid);
    return false;
  }

  std::println(std::cerr, "todo: resume transaction {}", tid);
  return true;
}
