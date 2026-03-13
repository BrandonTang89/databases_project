#pragma once
#include "common.hpp"

class Transaction {
  std::ostream &out;

public:
  Transaction(std::ostream &output_stream) : out(output_stream) {}
  StatusCode run() { return StatusCode::SUCCESS; };
};