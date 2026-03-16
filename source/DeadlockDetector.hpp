#pragma once

#include "Lock.hpp"
#include "common.hpp"
#include <map>
#include <set>

class DeadlockDetector {
  enum class VisitState { UNVISITED, VISITING, VISITED };
  std::map<TID, VisitState> visited_tx; // (tid, parent)
  std::unordered_map<TID, Transaction> &transactions;
  std::vector<TID> cycle; // populated if a cycle is detected

  // Returns whether a cycle was detected
  bool dfs_tx(const TID tid);

public:
  DeadlockDetector(std::unordered_map<TID, Transaction> &txs)
      : transactions(txs) {}

  std::optional<TID> detect_cycle(const TID &start_tid);
};