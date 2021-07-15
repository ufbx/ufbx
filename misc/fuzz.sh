#!/usr/bin/env bash

if [ "$1" == "build" ]; then
    afl-clang-fast -static ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx
elif [ "$1" == "build-32" ]; then
    afl-clang-fast -static ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_32
elif [ "$1" == "build-asan" ]; then
    AFL_USE_ASAN=1 afl-clang-fast ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_asan
elif [ "$1" == "build-asan-32" ]; then
    AFL_USE_ASAN=1 afl-clang-fast ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_asan_32
fi

if [ "$1" == "fuzz" ]; then
    afl-fuzz -i cases -o "$2_findings" -t 1000 -m 2000 "./$2"
fi
