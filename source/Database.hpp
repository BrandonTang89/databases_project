#pragma once
#include "ConflictGraph.hpp"
#include "Relation.hpp"
#include "Transaction.hpp"
#include "common.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <ostream>
#include <print>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Database {

public:
  std::ostream &out;
  std::unordered_map<std::string, Relation> relations;
  std::unordered_map<TID, Transaction> transactions;
  ConflictGraph conflict_graph;

  Database(std::ostream &output_stream = std::cout) : out(output_stream) {}

  bool begin_transaction(const TID &tid) {
    if (transactions.find(tid) != transactions.end()) {
      return false; // Transaction with this TID already exists
    }
    transactions.emplace(tid, Transaction(out));
    return true;
  }

  bool commit_transaction(const TID &tid) {
    auto it = transactions.find(tid);
    if (it == transactions.end()) {
      return false; // No such transaction
    }

    std::print("todo: commit transaction {}\n", tid);
    transactions.erase(it);
    return true;
  }

  bool rollback_transaction(const TID &tid) {
    auto it = transactions.find(tid);
    if (it == transactions.end()) {
      return false; // No such transaction
    }

    std::print("todo: rollback transaction {}\n", tid);
    transactions.erase(it);
    return true;
  }

  // Returns false if the transaction does not exist or the file cannot be
  // opened.  On success prints the number of imported tuples and returns true.
  bool add_data(const TID &tid, const RelName &rel_name,
                const std::string &csv_file) {
    if (transactions.find(tid) == transactions.end()) {
      out << "ERROR: transaction '" << tid << "' does not exist\n";
      return false;
    }

    std::ifstream file(csv_file);
    if (!file.is_open()) {
      out << "ERROR: cannot open file '" << csv_file << "'\n";
      return false;
    }

    // Create relation if it does not yet exist.
    Relation &rel = relations[rel_name];

    // Build a fast lookup set of existing tuples so we can skip duplicates.
    std::unordered_set<long long> existing;
    existing.reserve(rel.tuples.size() * 2);
    // Iterate the stable-vector by re-using a helper lambda that walks nodes.
    // Since StableVector exposes no iterators yet we track inserted pairs below.

    int imported = 0;
    std::string line;
    int line_no = 0;
    while (std::getline(file, line)) {
      ++line_no;
      auto [ok, left, right] = parse_csv_line(line);
      if (!ok) {
        out << "WARNING: skipping malformed line " << line_no
            << " in '" << csv_file << "': " << line << "\n";
        continue;
      }
      long long key = pack(left, right);
      if (existing.count(key)) {
        continue; // duplicate — skip silently
      }
      existing.insert(key);
      rel.tuples.emplace(left, right);
      ++imported;
    }

    out << "Imported " << imported << " tuple(s) into relation '" << rel_name
        << "'\n";
    return true;
  }

  // Returns false if the transaction does not exist, the relation does not
  // exist, or the file cannot be opened.  On success prints the number of
  // deleted tuples and returns true.
  bool delete_data(const TID &tid, const RelName &rel_name,
                   const std::string &csv_file) {
    if (transactions.find(tid) == transactions.end()) {
      out << "ERROR: transaction '" << tid << "' does not exist\n";
      return false;
    }

    if (relations.find(rel_name) == relations.end()) {
      out << "ERROR: relation '" << rel_name << "' does not exist\n";
      return false;
    }

    std::ifstream file(csv_file);
    if (!file.is_open()) {
      out << "ERROR: cannot open file '" << csv_file << "'\n";
      return false;
    }

    std::string line;
    int line_no = 0;
    int deleted = 0;
    while (std::getline(file, line)) {
      ++line_no;
      auto [ok, left, right] = parse_csv_line(line);
      if (!ok) {
        out << "WARNING: skipping malformed line " << line_no
            << " in '" << csv_file << "': " << line << "\n";
        continue;
      }
      // Mark matching alive tuples as dead (logical delete).
      // Missing tuples are silently skipped.
      deleted += mark_deleted(rel_name, left, right);
    }

    out << "Deleted " << deleted << " tuple(s) from relation '" << rel_name
        << "'\n";
    return true;
  }

  bool query(const TID &tid, const std::string &query_str) { return true; }
};