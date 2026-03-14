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
      std::println(out, "ERROR: transaction '{}' does not exist", tid);
      return false;
    }

    std::println(std::cerr, "todo: commit transaction {}", tid);
    transactions.erase(it);
    return true;
  }

  bool rollback_transaction(const TID &tid) {
    auto it = transactions.find(tid);
    if (it == transactions.end()) {
      std::println(out, "ERROR: transaction '{}' does not exist", tid);
      return false; // No such transaction
    }

    std::println(std::cerr, "todo: rollback transaction {}", tid);
    transactions.erase(it);
    return true;
  }

  // Returns false if the transaction does not exist or the file cannot be
  // opened.  On success prints the number of imported tuples and returns true.
  bool add_data(const TID &tid, const RelName &rel_name,
                const std::string &csv_file) {
    if (transactions.find(tid) == transactions.end()) {
      std::println(out, "ERROR: transaction '{}' does not exist", tid);
      return false;
    }

    std::ifstream file(csv_file);
    if (!file.is_open()) {
      std::println(out, "ERROR: cannot open file '{}'", csv_file);
      return false;
    }

    // Create relation if it does not yet exist.
    Relation &rel = relations[rel_name];

    int imported = 0;
    std::string line;
    int line_no = 0;
    while (std::getline(file, line)) {
      ++line_no;
      auto [ok, left, right] = parse_csv_line(line);
      if (!ok) {
        std::println(out, "WARNING: skipping malformed line {} in '{}': {}",
                     line_no, csv_file, line);
        continue;
      }
      // Duplicate check via the left-to-right index.
      if (tuple_exists(rel, left, right)) {
        continue; // duplicate — skip silently
      }
      DataTuple *tp = rel.tuples.emplace(left, right);
      rel.leftToRightIndex[left].tuples.push_back(tp);
      rel.rightToLeftIndex[right].tuples.push_back(tp);
      ++imported;
    }

    std::println(out, "Imported {} tuple(s) into relation '{}'", imported,
                 rel_name);
    return true;
  }

  // Returns false if the transaction does not exist, the relation does not
  // exist, or the file cannot be opened.  On success prints the number of
  // deleted tuples and returns true.
  bool delete_data(const TID &tid, const RelName &rel_name,
                   const std::string &csv_file) {
    if (transactions.find(tid) == transactions.end()) {
      std::println(out, "ERROR: transaction '{}' does not exist", tid);
      return false;
    }

    if (relations.find(rel_name) == relations.end()) {
      std::println(out, "ERROR: relation '{}' does not exist", rel_name);
      return false;
    }

    std::ifstream file(csv_file);
    if (!file.is_open()) {
      std::println(out, "ERROR: cannot open file '{}'", csv_file);
      return false;
    }

    std::string line;
    int line_no = 0;
    int deleted = 0;
    while (std::getline(file, line)) {
      ++line_no;
      auto [ok, left, right] = parse_csv_line(line);
      if (!ok) {
        std::println(out, "WARNING: skipping malformed line {} in '{}': {}",
                     line_no, csv_file, line);
        continue;
      }
      // Mark matching alive tuples as dead (logical delete).
      // Missing tuples are silently skipped.
      deleted += mark_deleted(rel_name, left, right);
    }

    std::println(out, "Deleted {} tuple(s) from relation '{}'", deleted,
                 rel_name);
    return true;
  }

  bool query(const TID &tid, std::span<const QueryAtom> body) {
    auto it = transactions.find(tid);
    if (it == transactions.end()) {
      std::println(out, "ERROR: transaction '{}' does not exist", tid);
      return false;
    }
    return true;
  }

  bool resume_transaction(const TID &tid) {
    if (transactions.find(tid) == transactions.end()) {
      std::println(out, "ERROR: transaction '{}' does not exist", tid);
      return false;
    }

    std::println(std::cerr, "todo: resume transaction {}", tid);
    return true;
  }

private:
  // Returns true when an alive tuple (left, right) already exists in rel.
  static bool tuple_exists(const Relation &rel, int left, int right) {
    auto it = rel.leftToRightIndex.find(left);
    if (it == rel.leftToRightIndex.end())
      return false;
    for (const DataTuple *tp : it->second.tuples) {
      if (tp->alive && tp->right == right)
        return true;
    }
    return false;
  }

  struct ParseResult {
    bool ok;
    int left;
    int right;
  };

  // Parse a single "a,b" CSV line.  Returns ok=false for any malformed input.
  static ParseResult parse_csv_line(const std::string &line) {
    // Trim trailing CR so Windows line-endings (\r\n) are handled.
    std::string_view sv = line;
    if (!sv.empty() && sv.back() == '\r')
      sv.remove_suffix(1);
    if (sv.empty())
      return {false, 0, 0};

    auto comma = sv.find(',');
    if (comma == std::string_view::npos)
      return {false, 0, 0};

    std::string_view lhs = sv.substr(0, comma);
    std::string_view rhs = sv.substr(comma + 1);

    // Reject anything with a second comma.
    if (rhs.find(',') != std::string_view::npos)
      return {false, 0, 0};

    int left{}, right{};
    auto [lend, lerr] =
        std::from_chars(lhs.data(), lhs.data() + lhs.size(), left);
    if (lerr != std::errc{} || lend != lhs.data() + lhs.size())
      return {false, 0, 0};

    auto [rend, rerr] =
        std::from_chars(rhs.data(), rhs.data() + rhs.size(), right);
    if (rerr != std::errc{} || rend != rhs.data() + rhs.size())
      return {false, 0, 0};

    return {true, left, right};
  }

  // Walk the StableVector of a relation and mark the first alive tuple that
  // matches (left, right) as dead.  Returns 1 if a tuple was deleted, else 0.
  // NOTE: StableVector has no public iterator yet, so we walk through the
  //       leftToRightIndex to find candidate pointers.
  int mark_deleted(const RelName &rel_name, int left, int right) {
    auto &rel = relations.at(rel_name);
    auto it = rel.leftToRightIndex.find(left);
    if (it == rel.leftToRightIndex.end())
      return 0;
    for (DataTuple *tp : it->second.tuples) {
      if (tp->alive && tp->right == right) {
        tp->alive = false;
        return 1;
      }
    }
    return 0;
  }
};