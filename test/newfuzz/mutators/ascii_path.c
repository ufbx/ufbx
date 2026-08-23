#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc-inl.h"

typedef struct afl_state_t afl_state_t;

static const char *const fragments[] = {
    "/",
    "./",
    "../",
    "a/",
};

typedef struct {
    unsigned char *mutated_out;
    size_t mutated_out_cap;
    uint32_t rng;
} ascii_path_mutator;

typedef struct {
    size_t start;
    size_t end;
} ascii_path_span;

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static size_t random_below(ascii_path_mutator *mut, size_t limit)
{
    uint32_t x = mut->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mut->rng = x;
    return x % limit;
}

static int find_path(const unsigned char *buf, size_t size, size_t *pos, ascii_path_span *path)
{
    size_t i = *pos;

    while (i < size) {
        if (buf[i] != '"') {
            i++;
            continue;
        }

        size_t start = ++i;
        int escaped = 0;
        int has_slash = 0;
        while (i < size) {
            unsigned char c = buf[i++];
            has_slash |= c == '/';
            if (escaped) {
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == '"') {
                *pos = i;
                if (has_slash) {
                    path->start = start;
                    path->end = i - 1;
                    return 1;
                }
                break;
            }
        }
    }

    *pos = size;
    return 0;
}

void *afl_custom_init(afl_state_t *afl, unsigned int seed)
{
    ascii_path_mutator *mut = calloc(1, sizeof(*mut));
    (void)afl;
    mut->rng = seed ? seed : 1;
    return mut;
}

void afl_custom_splice_optout(ascii_path_mutator *mut)
{
    (void)mut;
}

const char *afl_custom_describe(ascii_path_mutator *mut, size_t max_description_len)
{
    (void)mut;
    (void)max_description_len;
    return "ascii_path";
}

size_t afl_custom_fuzz(ascii_path_mutator *mut, unsigned char *buf, size_t buf_size,
    unsigned char **out_buf, unsigned char *add_buf, size_t add_buf_size, size_t max_size)
{
    ascii_path_span path;
    size_t pos = 0;
    size_t path_count = 0;

    (void)add_buf;
    (void)add_buf_size;

    while (find_path(buf, buf_size, &pos, &path)) {
        path_count++;
    }
    if (path_count == 0 || buf_size >= max_size) {
        return 0;
    }

    size_t path_index = random_below(mut, path_count);
    pos = 0;
    for (size_t i = 0; i <= path_index; i++) {
        find_path(buf, buf_size, &pos, &path);
    }

    size_t insert_pos = path.start + random_below(mut, path.end - path.start + 1);
    const char *fragment = fragments[random_below(mut, ARRAY_COUNT(fragments))];
    size_t fragment_size = strlen(fragment);
    size_t available = max_size - buf_size;
    if (fragment_size > available) {
        fragment = "/";
        fragment_size = 1;
    }
    size_t max_repeats = available / fragment_size;
    if (max_repeats > 256) {
        max_repeats = 256;
    }
    size_t repeats = random_below(mut, max_repeats) + 1;
    size_t insert_size = fragment_size * repeats;
    size_t out_size = buf_size + insert_size;
    if (out_size > mut->mutated_out_cap) {
        afl_realloc((void**)&mut->mutated_out, out_size);
        mut->mutated_out_cap = out_size;
    }

    memcpy(mut->mutated_out, buf, insert_pos);
    for (size_t i = 0; i < repeats; i++) {
        memcpy(mut->mutated_out + insert_pos + i * fragment_size, fragment, fragment_size);
    }
    memcpy(mut->mutated_out + insert_pos + insert_size, buf + insert_pos, buf_size - insert_pos);

    *out_buf = mut->mutated_out;
    return out_size;
}

void afl_custom_deinit(ascii_path_mutator *mut)
{
    free(mut->mutated_out);
    free(mut);
}
