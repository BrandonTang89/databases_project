#include "DeadlockDetector.hpp"
#include "Transaction.hpp"
#include <functional>
#include <optional>

bool DeadlockDetector::dfs_tx(const TID &tid) {
  auto it = visited_tx.find(tid);
  if (it != visited_tx.end()) {
    if (it->second == VisitState::VISITING)
      return true;
    if (it->second == VisitState::VISITED)
      return false;
  }
  visited_tx.emplace(tid, VisitState::VISITING);
  Transaction &tx = transactions.at(tid);
  for (Lock *lock : tx.required_locks) {
    for (const TID &holding_tid : lock->current_holders()) {
      if (holding_tid == tid) {
        continue; // Don't consider locks held by the transaction itself
      }

      if (dfs_tx(holding_tid)) {
        cycle.emplace_back(std::cref(tid));
        return true;
      }
    }
  }
  visited_tx.emplace(tid, VisitState::VISITED);
  return false;
}

std::optional<TID> DeadlockDetector::detect_cycle(const TID &start_tid) {
  visited_tx.clear();
  cycle.clear();
  bool has_cycle = dfs_tx(start_tid);

  if (has_cycle) {
    // locate the youngest transaction in the cycle to abort
    const TID *victim = &cycle[0].get();
    for (const auto &tid_ref : cycle) {
      const TID &tid = tid_ref.get();
      if (transactions.at(tid).txBornAt > transactions.at(*victim).txBornAt) {
        victim = &tid;
      }
    }
    return *victim;
  }
  return std::nullopt;
}
