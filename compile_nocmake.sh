#!/usr/bin/env bash
set -euo pipefail

if [[ ! -d "build/release" ]]; then
  mkdir -p "build/release"
fi

gcc \
  -std=c++23 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -Wshadow \
  -Wnon-virtual-dtor \
  -Wold-style-cast \
  -Wcast-align \
  -Wunused \
  -Woverloaded-virtual \
  -Wconversion \
  -Wsign-conversion \
  -Wdouble-promotion \
  -Wformat=2 \
  -g \
  -O3 \
  -DNDEBUG \
  -march=native \
  -fno-plt \
  -flto \
  source/main.cpp \
  source/parsing_functions.cpp \
  source/csv_parsing_functions.cpp \
  source/Database.cpp \
  source/Transaction.cpp \
  source/Relation.cpp \
  source/Stage.cpp \
  source/DeadlockDetector.cpp \
  -lstdc++ \
  -o build/release/databases_project
