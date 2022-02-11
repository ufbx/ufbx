#!/usr/bin/env bash

if [ "$1" == "build-ufbx" ]; then
    afl-clang-fast -static ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx
elif [ "$1" == "build-ufbx-32" ]; then
    afl-clang-fast -static ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_32
elif [ "$1" == "build-ufbx-asan" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DDISCRETE_ALLOCATIONS ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_asan
elif [ "$1" == "build-ufbx-asan-32" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DDISCRETE_ALLOCATIONS ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_asan_32
elif [ "$1" == "build-cache" ]; then
    afl-clang-fast -static ../../misc/fuzz_cache_persist.c -lm -o fuzz_cache
elif [ "$1" == "build-cache-32" ]; then
    afl-clang-fast -static ../../misc/fuzz_cache_persist.c -lm -o fuzz_cache_32
elif [ "$1" == "build-cache-asan" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DDISCRETE_ALLOCATIONS ../../misc/fuzz_cache_persist.c -lm -o fuzz_cache_asan
elif [ "$1" == "build-cache-asan-32" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DDISCRETE_ALLOCATIONS ../../misc/fuzz_cache_persist.c -lm -o fuzz_cache_asan_32
fi

if [ "$1" == "fuzz" ]; then
    cp -r cases "cases_$2"
    afl-fuzz -i "cases_$2" -o "findings_$2" -t 1000 -m 2000 "./$2"
fi
