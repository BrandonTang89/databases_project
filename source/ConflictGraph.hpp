#pragma once
#include "common.hpp"
#include <unordered_map>
#include <unordered_set>

class ConflictGraph {
    std::unordered_map<TID, std::unordered_set<TID>> adjlist;
};