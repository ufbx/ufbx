#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "alloc-inl.h"

typedef struct afl_state_t afl_state_t;

static const char *const interesting_values[] = {
    "-1", "0", "1",
    "-129", "-128", "-127",
    "127", "128", "129",
    "255", "256", "257",
    "-32769", "-32768", "-32767",
    "32767", "32768", "32769",
    "65535", "65536", "65537",
    "-2147483649", "-2147483648", "-2147483647",
    "2147483647", "2147483648", "2147483649",
    "4294967295", "4294967296", "4294967297",
    "-9223372036854775809", "-9223372036854775808", "-9223372036854775807",
    "9223372036854775807", "9223372036854775808", "9223372036854775809",
    "18446744073709551615", "18446744073709551616", "18446744073709551617",
};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    size_t start;
    size_t end;
} ascii_integer_span;

typedef struct {
    unsigned char *mutated_out;
    size_t mutated_out_cap;
    ascii_integer_span *numbers;
    size_t numbers_cap;
    size_t number_count;
    size_t counter;
} ascii_integer_mutator;

static int is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static void find_integers(ascii_integer_mutator *mut, const unsigned char *buf, size_t buf_size)
{
    size_t i = 0;

    mut->number_count = 0;
    size_t max_numbers = buf_size / 2 + 1;
    if (max_numbers > mut->numbers_cap) {
        mut->numbers = afl_realloc(mut->numbers, max_numbers * sizeof(*mut->numbers));
        mut->numbers_cap = max_numbers;
    }

    while (i < buf_size) {
        size_t start = i;
        int has_dot = 0;

        if ((buf[i] == '+' || buf[i] == '-') && i + 1 < buf_size &&
            (is_digit(buf[i + 1]) || buf[i + 1] == '.')) {
            i++;
        }

        if (!is_digit(buf[i]) && buf[i] != '.') {
            i = start + 1;
            continue;
        }

        while (i < buf_size && (is_digit(buf[i]) || buf[i] == '.')) {
            has_dot |= buf[i] == '.';
            i++;
        }

        if (!has_dot) {
            mut->numbers[mut->number_count].start = start;
            mut->numbers[mut->number_count].end = i;
            mut->number_count++;
        }
    }
}

void *afl_custom_init(afl_state_t *afl, unsigned int seed)
{
    ascii_integer_mutator *mut = calloc(1, sizeof(*mut));
    (void)afl;
    (void)seed;
    return mut;
}

void afl_custom_splice_optout(ascii_integer_mutator *mut)
{
    (void)mut;
}

const char *afl_custom_describe(ascii_integer_mutator *mut, size_t max_description_len)
{
    (void)mut;
    (void)max_description_len;
    return "ascii_integer";
}

unsigned int afl_custom_fuzz_count(ascii_integer_mutator *mut, const unsigned char *buf, size_t buf_size)
{
    find_integers(mut, buf, buf_size);
    mut->counter = 0;
    if (mut->number_count > UINT_MAX / ARRAY_COUNT(interesting_values)) {
        return UINT_MAX;
    }
    return (unsigned int)(mut->number_count * ARRAY_COUNT(interesting_values));
}

size_t afl_custom_fuzz(ascii_integer_mutator *mut, unsigned char *buf, size_t buf_size,
    unsigned char **out_buf, unsigned char *add_buf, size_t add_buf_size, size_t max_size)
{
    size_t number_index = mut->counter / ARRAY_COUNT(interesting_values);
    size_t value_index = mut->counter % ARRAY_COUNT(interesting_values);
    const char *value = interesting_values[value_index];
    size_t value_size = strlen(value);

    (void)add_buf;
    (void)add_buf_size;

    if (number_index >= mut->number_count) {
        return 0;
    }
    mut->counter++;

    ascii_integer_span span = mut->numbers[number_index];
    size_t out_size = buf_size - (span.end - span.start) + value_size;
    if (out_size > max_size) {
        return 0;
    }
    if (out_size > mut->mutated_out_cap) {
        mut->mutated_out = afl_realloc(mut->mutated_out, out_size);
        mut->mutated_out_cap = out_size;
    }

    memcpy(mut->mutated_out, buf, span.start);
    memcpy(mut->mutated_out + span.start, value, value_size);
    memcpy(mut->mutated_out + span.start + value_size, buf + span.end, buf_size - span.end);

    *out_buf = mut->mutated_out;
    return out_size;
}

void afl_custom_deinit(ascii_integer_mutator *mut)
{
    free(mut->mutated_out);
    free(mut->numbers);
    free(mut);
}
