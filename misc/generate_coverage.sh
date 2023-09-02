#!/usr/bin/env bash
set -x
set -e

mkdir -p build

LLVM_COV="${LLVM_COV:-llvm-cov}"
LLVM_GCOV=$(realpath misc/llvm_gcov.sh)
chmod +x misc/llvm_gcov.sh

clang -lm -coverage -g -std=gnu99 -DNDEBUG -DUFBX_NO_ASSERT -DUFBX_DEV=1 -DUFBX_REGRESSION=1 ufbx.c test/runner.c -o build/cov-runner
build/cov-runner -d data
$LLVM_COV gcov ufbx runner -b
lcov --directory . --base-directory . --gcov-tool $LLVM_GCOV --rc lcov_branch_coverage=1 --capture -o coverage.lcov
