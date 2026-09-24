cmake_minimum_required(VERSION 3.15)

option(USE_AFL "Build the fuzz target with AFL instrumentation" ON)
option(USE_ASAN "Build the fuzz target with AddressSanitizer" ON)
option(USE_32BIT "Build the fuzz target in 32-bit mode" OFF)
option(FUZZ_ASCII "Accept ASCII FBX inputs" OFF)
option(FUZZ_BINARY "Accept binary FBX inputs" OFF)

project(ufbx_newfuzz C)

if(USE_AFL)
    find_path(AFL_INCLUDE_DIR alloc-inl.h
        HINTS "$ENV{AFL_PATH}"
        PATH_SUFFIXES include include/afl afl)
    if(NOT AFL_INCLUDE_DIR)
        message(FATAL_ERROR
            "Could not find the AFL++ headers. Set AFL_INCLUDE_DIR to the directory "
            "containing alloc-inl.h")
    endif()

    function(add_afl_mutator name)
            set(source "${CMAKE_CURRENT_SOURCE_DIR}/mutators/${name}.c")
            set(output "${CMAKE_CURRENT_BINARY_DIR}/${name}${CMAKE_SHARED_LIBRARY_SUFFIX}")
            add_custom_command(
                OUTPUT "${output}"
                COMMAND "${MUTATOR_C_COMPILER}" -shared -fPIC -Wall -O3
                    -I "${AFL_INCLUDE_DIR}" "${source}" -o "${output}"
                DEPENDS "${source}"
                VERBATIM)
            add_custom_target(${name} ALL DEPENDS "${output}")
    endfunction()

    add_afl_mutator(ascii_integer)
    add_afl_mutator(ascii_string)
endif()

if(USE_AFL)
    set(CMAKE_C_COMPILER afl-clang-fast)
    set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;AFL_USE_ASAN=1")
    set(CMAKE_C_LINKER_LAUNCHER "${CMAKE_COMMAND};-E;env;AFL_USE_ASAN=1")
endif()

add_executable(newfuzz newfuzz.c ../../ufbx.c)
target_include_directories(newfuzz PRIVATE ../..)
target_compile_definitions(newfuzz PRIVATE
    $<$<BOOL:${FUZZ_ASCII}>:FUZZ_ASCII>
    $<$<BOOL:${FUZZ_BINARY}>:FUZZ_BINARY>)

if(USE_AFL)
    target_compile_definitions(newfuzz PRIVATE USE_AFL)
endif()

if(USE_ASAN AND NOT USE_AFL)
    # AFL requires compiling with `AFL_USE_ASAN` environment variable
    target_compile_options(newfuzz PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(newfuzz PRIVATE -fsanitize=address)
endif()

if(USE_32BIT AND NOT MSVC)
    target_compile_options(newfuzz PRIVATE -m32)
    target_link_options(newfuzz PRIVATE -m32)
endif()

if(UNIX)
    target_link_libraries(newfuzz PRIVATE m)
endif()
