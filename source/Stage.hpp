#pragma once

#include "DataTuple.hpp"
#include "Group.hpp"
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
  std::vector<int> *input{nullptr};
  std::vector<int> output{};

  bool left_is_const;
  bool right_is_const;
  bool left_is_var;
  bool right_is_var;

  size_t var_idx = 0;
  size_t var2_idx = 0;
  int const_val = 0;
  Group *group;

  std::flat_set<DataTuple *>::iterator group_iter;
  StableVector<DataTuple>::iterator rel_iter;

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

  PipelineStatus next();
  PipelineStatus next_const_const();
  PipelineStatus next_group_filter();
  PipelineStatus next_group_product();
  PipelineStatus next_relation_filter();
  PipelineStatus next_relation_product();

  std::vector<int> *get_out_channel() {
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
