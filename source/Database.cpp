#include "Database.hpp"
#include "Relation.hpp"
#include "Transaction.hpp"
#include "common.hpp"
#include <cstddef>
#include <print>
#include <string>

bool Database::begin_transaction(const TID &tid) {
  if (transactions.contains(tid)) {

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

  StatusCode status = it->second.commit();
  on_control(tid, status);
  return true;
}

bool Database::rollback_transaction(const TID &tid, bool silent_abort) {
  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false; // No such transaction
  }

  it->second.rollback(silent_abort);
  return true;
}

bool Database::add_data(const TID &tid, const RelName &rel_name,
                        const std::string &csv_file) {
  auto it = transactions.find(tid);
  if (!transactions.contains(tid)) {
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false;
  }

  if (!relations.contains(rel_name)) {
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
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false;
  }

  if (!relations.contains(rel_name)) {
    std::println(out, "ERROR: relation {} does not exist.", rel_name);
    return false;
  }

  Relation &rel = relations.at(rel_name);
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

bool Database::resume_transaction(const TID &tid, bool silent_resume) {

  auto it = transactions.find(tid);
  if (it == transactions.end()) {
    std::println(out, "ERROR: transaction {} does not exist.", tid);
    return false;
  }

  Transaction &tx = it->second;
  StatusCode status = tx.resume(silent_resume);
  on_control(tid, status);

  return true;
}

void Database::on_control(const TID &tid, StatusCode status) {
  if (status == StatusCode::SUSPENDED) {
    bool suspended_tx_aborted = false;
    while (true) {
      std::optional<TID> victim_opt = deadlock_detector.detect_cycle(tid);
      if (!victim_opt) {
        break; // No deadlock detected, can just wait
      }

      TID victim = *victim_opt;
      std::println(
          out, "A deadlock was detected, and transaction {} was rolled back.",
          victim);

      rollback_transaction(victim, true);
      if (victim != tid) {
        debug("Resuming transaction {} after aborting victim {}.", tid, victim);
        resume_transaction(tid, true);
        return; // let the nested on_control deal with it
      }
      suspended_tx_aborted = true;
      break;
    }
    if (!suspended_tx_aborted) {
      std::println(out, "Transaction {} was suspended.", tid);
    }
  } else {
    transactions.at(tid).clear_required_locks();
  }
}
