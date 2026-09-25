#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "alloc-inl.h"

typedef struct afl_state_t afl_state_t;

typedef struct {
    const char *data;
    size_t size;
} interesting_string;

static const interesting_string interesting_values[] = {
    { "\"\"", 2 },
    { "\"\xff\"", 3 },
};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    uint32_t start;
    uint32_t end;
} string_span;

typedef struct {
    unsigned char *mutated_out;
    size_t mutated_out_cap;
    string_span *strings;
    size_t strings_cap;
    size_t string_count;
    size_t counter;
} ascii_string_mutator;

static void find_strings(ascii_string_mutator *mut, const unsigned char *buf, size_t buf_size)
{
    size_t i = 0;

    mut->string_count = 0;

    size_t max_strings = buf_size / 8 + 1;
    if (max_strings > mut->strings_cap) {
        mut->strings = realloc(mut->strings, max_strings * sizeof(*mut->strings));
        mut->strings_cap = max_strings;
    }

    while (i < buf_size && mut->string_count < max_strings) {
        if (buf[i] != '"') {
            i++;
            continue;
        }

        size_t start = i;
        i++;

        while (i < buf_size && buf[i] != '"') i++;
        if (i >= buf_size) break;
        i++;

        mut->strings[mut->string_count].start = (uint32_t)start;
        mut->strings[mut->string_count].end = (uint32_t)i;
        mut->string_count++;
    }
}

void *afl_custom_init(afl_state_t *afl, unsigned int seed)
{
    ascii_string_mutator *mut = calloc(1, sizeof(*mut));

    (void)afl;
    (void)seed;

    return mut;
}

void afl_custom_splice_optout(ascii_string_mutator *mut)
{
    (void)mut;
}

const char *afl_custom_describe(ascii_string_mutator *mut, size_t max_description_len)
{
    (void)mut;
    (void)max_description_len;
    return "ascii_string";
}

unsigned int afl_custom_fuzz_count(ascii_string_mutator *mut, const unsigned char *buf, size_t buf_size)
{
    find_strings(mut, buf, buf_size);
    mut->counter = 0;

    if (mut->string_count > UINT_MAX / ARRAY_COUNT(interesting_values)) {
        return UINT_MAX;
    }
    return (unsigned int)(mut->string_count * ARRAY_COUNT(interesting_values));
}

size_t afl_custom_fuzz(ascii_string_mutator *mut, unsigned char *buf, size_t buf_size,
    unsigned char **out_buf, unsigned char *add_buf, size_t add_buf_size, size_t max_size)
{
    size_t string_index = mut->counter / ARRAY_COUNT(interesting_values);
    size_t value_index = mut->counter % ARRAY_COUNT(interesting_values);

    if (string_index >= mut->string_count) {
        return 0;
    }

    mut->counter++;

    string_span span = mut->strings[string_index];

    const unsigned char *value = interesting_values[value_index].data;
    size_t value_size = interesting_values[value_index].size;
    size_t old_size = span.end - span.start;
    size_t out_size = buf_size - old_size + value_size;
    if (out_size > max_size) {
        return 0;
    }

    if (out_size > mut->mutated_out_cap) {
        afl_realloc(
            (void **)&mut->mutated_out,
            out_size);

        mut->mutated_out_cap = out_size;
    }

    memcpy(mut->mutated_out, buf, span.start);
    memcpy(mut->mutated_out + span.start, value, value_size);
    memcpy(mut->mutated_out + span.start + value_size, buf + span.end, buf_size - span.end);
    *out_buf = mut->mutated_out;

    return out_size;
}

void afl_custom_deinit(ascii_string_mutator *mut)
{
    free(mut->mutated_out);
    free(mut->strings);
    free(mut);
}
