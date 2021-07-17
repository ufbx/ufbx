#!/usr/bin/env bash
set -x

clang -lm -coverage -g -std=gnu99 ufbx.c test/runner.c -o build/cov-runner
build/cov-runner -d data
llvm-cov gcov ufbx runner -b
chmod +x misc/llvm_gcov.sh
lcov --directory . --base-directory . --gcov-tool misc/llvm_gcov.sh --rc lcov_branch_coverage=1 --capture -o coverage.lcov
