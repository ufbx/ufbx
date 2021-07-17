#!/usr/bin/env bash
set -x
set -e

LLVM_COV="${LLVM_COV:-llvm-cov}"

clang -lm -coverage -g -std=gnu99 ufbx.c test/runner.c -o build/cov-runner
build/cov-runner -d data
$LLVM_COV gcov ufbx runner -b
chmod +x misc/llvm_gcov.sh
lcov --directory . --base-directory . --gcov-tool misc/llvm_gcov.sh --rc lcov_branch_coverage=1 --capture -o coverage.lcov
