#pragma once
#include "Relation.hpp"
#include "common.hpp"
#include "csv_parsing_functions.hpp"
#include <flat_map>
#include <iostream>
#include <print>
#include <vector>

static std::flat_map<TransactionState, std::string> transaction_state_names = {
    {TransactionState::READY, "READY"},
    {TransactionState::SUSPENDED_QUERY, "SUSPENDED_QUERY"},
    {TransactionState::SUSPENDED_ADD, "SUSPENDED_ADD"},
    {TransactionState::SUSPENDED_DELETE, "SUSPENDED_DELETE"},
    {TransactionState::COMMITTED, "COMMITTED"},
    {TransactionState::ABORTED, "ABORTED"},
};

class Transaction {
  std::ostream &out;
  TransactionState state{TransactionState::READY};

  // tuples from file being added/removed
  std::vector<std::pair<int, int>> incoming_tuples;
  // index of next tuple to process from incoming_tuples
  size_t incoming_index{0};

public:
  Transaction(std::ostream &output_stream) : out(output_stream) {}
  StatusCode resume() {
    if (state == TransactionState::SUSPENDED_QUERY) {
      std::println(std::cerr, "todo: resume query transaction");
    } else if (state == TransactionState::SUSPENDED_ADD) {
      std::println(std::cerr, "todo: resume add transaction");
    } else if (state == TransactionState::SUSPENDED_DELETE) {
      std::println(std::cerr, "todo: resume delete transaction");
    } else {
      std::println(out, "ERROR: transaction is not suspended");
      std::println(out, "Current state: {}", transaction_state_names[state]);
    }
    return StatusCode::SUCCESS;
  };

  StatusCode start_add([[maybe_unused]] Relation &rel, const std::string &csv_file) {
    if (state != TransactionState::READY) {
      std::println(out, "ERROR: transaction is not in READY state");
      std::println(out, "Current state: {}", transaction_state_names[state]);
      return StatusCode::SUCCESS;
    }
    incoming_tuples = parse_csv_file(csv_file);
    debug("Parsed {} tuples from '{}'", incoming_tuples.size(), csv_file);
    return StatusCode::SUCCESS;
  }

  StatusCode start_delete([[maybe_unused]] Relation &rel, const std::string &csv_file) {
    if (state != TransactionState::READY) {
      std::println(out, "ERROR: transaction is not in READY state");
      std::println(out, "Current state: {}", transaction_state_names[state]);
      return StatusCode::SUCCESS;
    }
    incoming_tuples = parse_csv_file(csv_file);
    debug("Parsed {} tuples from '{}'", incoming_tuples.size(), csv_file);
    return StatusCode::SUCCESS;
  };
};