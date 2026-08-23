cmake_minimum_required(VERSION 3.15)

option(USE_AFL "Build the fuzz target with AFL instrumentation" ON)
option(USE_ASAN "Build the fuzz target with AddressSanitizer" ON)
option(FUZZ_ASCII "Accept ASCII FBX inputs" OFF)
option(FUZZ_BINARY "Accept binary FBX inputs" OFF)

if(USE_AFL AND NOT CMAKE_C_COMPILER AND NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{CC} AND NOT "$ENV{CC}" STREQUAL "")
        set(AFL_C_COMPILER "$ENV{CC}")
    else()
        find_program(AFL_C_COMPILER NAMES afl-clang-fast afl-clang-lto)
        if(NOT AFL_C_COMPILER)
            message(FATAL_ERROR
                "Could not find an AFL compiler. Install AFL++ or supply one with "
                "-DCMAKE_C_COMPILER=/path/to/afl-clang-fast")
        endif()
    endif()
    set(CMAKE_C_COMPILER "${AFL_C_COMPILER}" CACHE FILEPATH "C compiler" FORCE)
endif()

project(ufbx_newfuzz C)

find_path(AFL_INCLUDE_DIR alloc-inl.h
    HINTS "$ENV{AFL_PATH}"
    PATH_SUFFIXES include include/afl afl)
if(NOT AFL_INCLUDE_DIR)
    message(FATAL_ERROR
        "Could not find the AFL++ headers. Set AFL_INCLUDE_DIR to the directory "
        "containing alloc-inl.h")
endif()

find_program(MUTATOR_C_COMPILER NAMES clang)
if(NOT MUTATOR_C_COMPILER)
    message(FATAL_ERROR
        "Could not find Clang for the AFL mutators. Set MUTATOR_C_COMPILER "
        "to the Clang executable")
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
add_afl_mutator(ascii_path)

add_executable(newfuzz newfuzz.c ../../ufbx.c)
target_include_directories(newfuzz PRIVATE ../..)
target_compile_definitions(newfuzz PRIVATE
    $<$<BOOL:${FUZZ_ASCII}>:FUZZ_ASCII>
    $<$<BOOL:${FUZZ_BINARY}>:FUZZ_BINARY>)

if(USE_AFL)
    target_compile_definitions(newfuzz PRIVATE USE_AFL)
endif()

if(USE_ASAN)
    target_compile_options(newfuzz PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(newfuzz PRIVATE -fsanitize=address)
endif()

if(UNIX)
    target_link_libraries(newfuzz PRIVATE m)
endif()
