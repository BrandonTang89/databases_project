#include "Database.hpp"
#include "Relation.hpp"
#include "Transaction.hpp"
#include "common.hpp"
#include <cstddef>
#include <print>
#include <set>
#include <string>
#include <unordered_map>

bool Database::begin_transaction(const TID &tid) {
  if (transactions.find(tid) != transactions.end()) {

    std::println(out, "ERROR: transaction {} already exists.", tid);
    return false; // Transaction with this TID already exists
  }

  size_t age = transactions.size();
  transactions.emplace(tid, Transaction(out, tid, age, relations));
  std::println(out, "Transaction {} was created.", tid);
  return true;
}

bool Database::commit_transaction(const TID &tid) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false;
  }

  it->second.commit();
  return true;
}

bool Database::rollback_transaction(const TID &tid) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false; // No such transaction
  }

  it->second.rollback();
  return true;
}

bool Database::add_data(const TID &tid, const RelName &rel_name,
                        const std::string &csv_file) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false;
  }

  if (relations.find(rel_name) == relations.end()) {
    std::println(out, "Relation {} was created.", rel_name);
  }

  Relation &rel = relations[rel_name]; // also creates if doesn't exist

  StatusCode status = it->second.start_edit(&rel, csv_file, true);
  on_control(tid, status);

  return true;
}

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
  StatusCode status = it->second.start_edit(&rel, csv_file, false);
  on_control(tid, status);
  return true;
}

bool Database::query(const TID &tid, std::vector<QueryAtom> body) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false;
  }

  StatusCode status = it->second.start_query(std::move(body));
  on_control(tid, status);
  return true;
}

bool Database::resume_transaction(const TID &tid) {

  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false;
  }

  Transaction &tx = it->second;
  StatusCode status = tx.resume();
  on_control(tid, status);

  return true;
}

void Database::on_control(const TID &tid, StatusCode status) {
  if (status == StatusCode::SUSPENDED) {
    std::println(out, "Transaction {} was suspended.", tid);
    detect_and_resolve_deadlock(tid);
  }
}

void Database::detect_and_resolve_deadlock(const TID &tid) {
  std::set<std::pair<TID, Lock *>> visited_tx;    // (tid, parent)
  std::set<std::pair<Lock *, TID>> visited_locks; // (lock, parent)

}
