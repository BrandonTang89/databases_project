#include "DeadlockDetector.hpp"
#include "Transaction.hpp"
#include <optional>

bool DeadlockDetector::dfs_tx(const TID& tid) {
  auto it = visited_tx.find(tid);
  if (it != visited_tx.end() && it->second == VisitState::VISITING) {
    // Cycle detected, return the cycle
    return true;
  }
  if (it != visited_tx.end() && it->second == VisitState::VISITED) {
    return false;
  }
  visited_tx.insert_or_assign(tid, VisitState::VISITING);
  Transaction &tx = transactions.at(tid);
  for (Lock *lock : tx.required_locks) {
    for (const TID &holding_tid : lock->current_holders()) {
      if (holding_tid == tid) {
        continue; // Don't consider locks held by the transaction itself
      }

      if (dfs_tx(holding_tid)) {
        cycle.push_back(tid);
        return true;
      }
    }
  }
  visited_tx.insert_or_assign(tid, VisitState::VISITED);
  return false;
}

std::optional<TID> DeadlockDetector::detect_cycle(const TID &start_tid) {
  visited_tx.clear();
  cycle.clear();
  bool has_cycle = dfs_tx(start_tid);

  if (has_cycle) {
    // locate the youngest transaction in the cycle to abort
    TID victim = cycle[0];
    for (const TID &tid : cycle) {
      if (transactions.at(tid).txBornAt > transactions.at(victim).txBornAt) {
        victim = tid;
      }
    }
    return victim;
  }
  return std::nullopt;
}
