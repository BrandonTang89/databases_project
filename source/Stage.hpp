#pragma once

#include "DataTuple.hpp"
#include "Group.hpp"
#include "Relation.hpp"
#include "StableVector.hpp"
#include "common.hpp"

class Stage {
public:
  size_t stage_idx;
  Transaction &tx;

  QueryAtom *atom{nullptr};
  Relation *rel{nullptr};
  Stage *previous{nullptr};
  size_t num_output_vars{0};
  size_t num_input_vars{0};
  std::vector<uint32_t> *input{nullptr};
  std::vector<uint32_t> output{};

  bool left_is_const;
  bool right_is_const;
  bool left_is_var;
  bool right_is_var;

  size_t var_idx = 0;
  size_t var2_idx = 0;
  uint32_t const_val = 0;
  Group *group;

  bool group_iter_valid = false;
  decltype(Group::tuples)::iterator group_iter;
  decltype(Relation::tuples)::iterator rel_iter;

  Stage(size_t stage_index, Transaction &trx);

  enum class StageType {
    INITIAL,
    CONST_CONST,
    GROUP_FILTER,
    GROUP_PRODUCT,
    RELATION_FILTER,
    RELATION_PRODUCT,
    JOIN_LEFT,
    JOIN_RIGHT,
  } type;

  static std::string_view stage_type_name(StageType t) {
    switch (t) {
    case StageType::INITIAL:
      return "INITIAL";
    case StageType::CONST_CONST:
      return "CONST_CONST";
    case StageType::GROUP_FILTER:
      return "GROUP_FILTER";
    case StageType::GROUP_PRODUCT:
      return "GROUP_PRODUCT";
    case StageType::RELATION_FILTER:
      return "RELATION_FILTER";
    case StageType::RELATION_PRODUCT:
      return "RELATION_PRODUCT";
    case StageType::JOIN_LEFT:
      return "JOIN_LEFT";
    case StageType::JOIN_RIGHT:
      return "JOIN_RIGHT";
    }
    assert(false && "invalid stage type");
    return "";
  }

  PipelineStatus next();
  PipelineStatus next_const_const();
  PipelineStatus next_group_filter();
  PipelineStatus next_group_product();
  PipelineStatus next_relation_filter();
  PipelineStatus next_relation_product();
  PipelineStatus next_join_left();  // join on R(existing, new)
  PipelineStatus next_join_right(); // join on R(new, existing)

  std::vector<uint32_t> *get_out_channel() {
    if (type == StageType::GROUP_FILTER || type == StageType::RELATION_FILTER ||
        type == StageType::CONST_CONST) {
      return input;
    } else {
      return &output;
    }
  }

private:
  void group_setup();
};
