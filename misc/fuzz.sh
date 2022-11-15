#!/usr/bin/env bash

cmd="$1"
shift 1

if [ $cmd == "build-ufbx" ]; then
    afl-clang-fast -static ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx
elif [ $cmd == "build-ufbx-32" ]; then
    afl-clang-fast -static ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_32
elif [ $cmd == "build-ufbx-asan" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DDISCRETE_ALLOCATIONS ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_asan
elif [ $cmd == "build-ufbx-asan-32" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DDISCRETE_ALLOCATIONS ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_ufbx_asan_32
elif [ $cmd == "build-cache" ]; then
    afl-clang-fast -static ../../misc/fuzz_cache_persist.c -lm -o fuzz_cache
elif [ $cmd == "build-cache-32" ]; then
    afl-clang-fast -static ../../misc/fuzz_cache_persist.c -lm -o fuzz_cache_32
elif [ $cmd == "build-cache-asan" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DDISCRETE_ALLOCATIONS ../../misc/fuzz_cache_persist.c -lm -o fuzz_cache_asan
elif [ $cmd == "build-cache-asan-32" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DDISCRETE_ALLOCATIONS ../../misc/fuzz_cache_persist.c -lm -o fuzz_cache_asan_32
elif [ $cmd == "build-obj" ]; then
    afl-clang-fast -DLOAD_OBJ -static ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_obj
elif [ $cmd == "build-obj-asan" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DLOAD_OBJ -DDISCRETE_ALLOCATIONS ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_obj_asan
elif [ $cmd == "build-mtl" ]; then
    afl-clang-fast -DLOAD_MTL -static ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_mtl
elif [ $cmd == "build-mtl-asan" ]; then
    AFL_USE_ASAN=1 afl-clang-fast -DLOAD_MTL -DDISCRETE_ALLOCATIONS ../../misc/fuzz_ufbx_persist.c -lm -o fuzz_mtl_asan
fi

name=$1
shift 1

if [ $cmd == "fuzz" ]; then
    cp -r cases "cases_$name"
    afl-fuzz  "$@" -i "cases_$name" -o "findings_$name" -t 1000 -m 2000 "./$name"
fi
