#pragma once

#include "common.hpp"
#include <functional>
#include <unordered_map>

class DeadlockDetector {
  enum class VisitState { VISITING, VISITED };
  std::unordered_map<TID, VisitState> visited_tx; // (tid, parent)
  std::unordered_map<TID, Transaction> &transactions;
  std::vector<std::reference_wrapper<const TID>> cycle; // populated if cycle found

  // Returns whether a cycle was detected
  bool dfs_tx(const TID& tid);

public:
  DeadlockDetector(std::unordered_map<TID, Transaction> &txs)
      : transactions(txs) {}

  std::optional<TID> detect_cycle(const TID &start_tid);
};