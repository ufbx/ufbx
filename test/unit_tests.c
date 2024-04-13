#include "../ufbx.c"
#include "../ufbx.h"

#include <stdio.h>
#include <string.h>

void test_assert_imp(bool cond, const char *message, int line)
{
	if (!cond) {
		fprintf(stderr, "unit_tests.c:%d: Assertion failed: %s\n", line, message);
		exit(1);
	}
}

#define test_assert(cond) test_assert_imp((cond), #cond, __LINE__)

typedef struct  {
	uint32_t a, b;
} uint_pair;

typedef enum {
	SORT_STABLE_MACRO,	
	SORT_STABLE_FUNCTION,	
	SORT_UNSTABLE_FUNCTION,	

	SORT_MODE_COUNT,
} sort_mode;

static size_t g_linear_size = 2;
static sort_mode g_sort_mode = SORT_STABLE_MACRO;

static bool uint_less(void *user, const void *va, const void *vb)
{
	uint32_t a = *(const uint32_t*)va, b = *(const uint32_t*)vb;
	return a < b;
}

static bool pair_less_a(void *user, const void *va, const void *vb)
{
	uint_pair a = *(const uint_pair*)va, b = *(const uint_pair*)vb;
	return a.a < b.a;
}

static bool pair_less_b(void *user, const void *va, const void *vb)
{
	uint_pair a = *(const uint_pair*)va, b = *(const uint_pair*)vb;
	return a.b < b.b;
}

static bool str_less(void *user, const void *va, const void *vb)
{
	const char *a = *(const char**)va, *b = *(const char**)vb;
	return strcmp(a, b) < 0;
}

void sort_uints(uint32_t *test_data, uint32_t *test_tmp, size_t test_size)
{
	switch (g_sort_mode) {
		case SORT_STABLE_MACRO:
			ufbxi_macro_stable_sort(uint32_t, g_linear_size, test_data, test_tmp, test_size, (*a < *b));
			break;
		case SORT_STABLE_FUNCTION:
			ufbxi_stable_sort(sizeof(uint32_t), g_linear_size, test_data, test_tmp, test_size, &uint_less, NULL);
			break;
		case SORT_UNSTABLE_FUNCTION:
			ufbxi_unstable_sort(test_data, test_size, sizeof(uint32_t), &uint_less, NULL);
			break;
		default:
			test_assert(0);
			break;
	}
}

void sort_pairs_by_a(uint_pair *data, uint_pair *tmp, size_t size)
{
	switch (g_sort_mode) {
		case SORT_STABLE_MACRO:
			ufbxi_macro_stable_sort(uint_pair, g_linear_size, data, tmp, size, (a->a < b->a));
			break;
		case SORT_STABLE_FUNCTION:
			ufbxi_stable_sort(sizeof(uint_pair), g_linear_size, data, tmp, size, &pair_less_a, NULL);
			break;
		case SORT_UNSTABLE_FUNCTION:
			ufbxi_unstable_sort(data, size, sizeof(uint_pair), &pair_less_a, NULL);
			break;
		default:
			test_assert(0);
			break;
	}
}

void sort_pairs_by_b(uint_pair *data, uint_pair *tmp, size_t size)
{
	switch (g_sort_mode) {
		case SORT_STABLE_MACRO:
			ufbxi_macro_stable_sort(uint_pair, g_linear_size, data, tmp, size, (a->b < b->b));
			break;
		case SORT_STABLE_FUNCTION:
			ufbxi_stable_sort(sizeof(uint_pair), g_linear_size, data, tmp, size, &pair_less_b, NULL);
			break;
		case SORT_UNSTABLE_FUNCTION:
			ufbxi_unstable_sort(data, size, sizeof(uint_pair), &pair_less_b, NULL);
			break;
		default:
			test_assert(0);
			break;
	}
}

size_t find_uint(uint32_t *test_data, size_t test_size, uint32_t value)
{
	size_t index = SIZE_MAX;

	ufbxi_macro_lower_bound_eq(uint32_t, g_linear_size, &index, test_data, 0, test_size,
		(*a < value),
		(*a == value));

	return index;
}

size_t find_uint_end(uint32_t *test_data, size_t test_size, size_t test_begin, uint32_t value)
{
	size_t index = SIZE_MAX;
	ufbxi_macro_upper_bound_eq(uint32_t, g_linear_size, &index, test_data, test_begin, test_size, (*a == value));
	return index;
}

size_t find_pair_by_a(uint_pair *data, size_t size, uint32_t value)
{
	size_t pair_ix = SIZE_MAX;
	ufbxi_macro_lower_bound_eq(uint_pair, g_linear_size, &pair_ix, data, 0, size,
		(a->a < value),
		(a->a == value));
	return pair_ix;
}

void sort_strings(const char **data, void *tmp, size_t size)
{
	switch (g_sort_mode) {
		case SORT_STABLE_MACRO:
			ufbxi_macro_stable_sort(const char*, g_linear_size, data, tmp, size, (strcmp(*a, *b) < 0));
			break;
		case SORT_STABLE_FUNCTION:
			ufbxi_stable_sort(sizeof(const char*), g_linear_size, data, tmp, size, &str_less, NULL);
			break;
		case SORT_UNSTABLE_FUNCTION:
			ufbxi_unstable_sort(data, size, sizeof(const char*), &str_less, NULL);
			break;
	}
}

size_t find_first_string(const char **data, size_t size, const char *str)
{
	size_t str_index;

	ufbxi_macro_lower_bound_eq(const char*, g_linear_size, &str_index, data, 0, size,
		(strcmp(*a, str) < 0),
		(strcmp(*a, str) == 0));

	return str_index;
}

static uint32_t xorshift32(uint32_t *state)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return *state = x;
}

static ufbx_real xorshift32_real(uint32_t *state)
{
	uint32_t u = xorshift32(state);
	return (ufbx_real)u * (ufbx_real)2.3283064365386962890625e-10;
}

void generate_linear(uint32_t *dst, size_t size, uint32_t start, uint32_t delta)
{
	for (size_t i = 0; i < size; i++) {
		dst[i] = start;
		start += delta;
	}
}

void generate_random(uint32_t *dst, size_t size, uint32_t seed, uint32_t mod)
{
	uint32_t state = seed | 1;
	for (size_t i = 0; i < size; i++) {
		dst[i] = xorshift32(&state) % mod;
	}
}

#define MAX_SORT_SIZE 2048

void test_sort(uint32_t *data, size_t size)
{
	test_assert(size <= MAX_SORT_SIZE);
	static size_t call_count = 0;
	static uint32_t uint_tmp_buffer[MAX_SORT_SIZE];
	static uint_pair pair_buffer[MAX_SORT_SIZE];
	static uint_pair pair_tmp_buffer[MAX_SORT_SIZE];
	call_count++;

	for (size_t i = 0; i < size; i++) {
		pair_buffer[i].a = data[i];
		pair_buffer[i].b = (uint32_t)i;
	}

	sort_uints(data, uint_tmp_buffer, size);
	sort_pairs_by_a(pair_buffer, pair_tmp_buffer, size);

	for (size_t i = 1; i < size; i++) {
		test_assert(data[i - 1] <= data[i]);
		test_assert(pair_buffer[i - 1].a <= pair_buffer[i].a);
		if (pair_buffer[i - 1].a == pair_buffer[i].a) {
			if (g_sort_mode != SORT_UNSTABLE_FUNCTION) {
				test_assert(pair_buffer[i - 1].b < pair_buffer[i].b);
			}
		}
	}

	for (size_t i = 0; i < size; i++) {
		uint32_t value = data[i];

		size_t index = find_uint(data, size, value);
		test_assert(index <= i);
		test_assert(data[index] == value);
		test_assert(index == find_pair_by_a(pair_buffer, size, value));
		if (index > 0) {
			test_assert(data[index - 1] < value);
		}

		size_t end = find_uint_end(data, size, index, value);
		test_assert(end > i);
		test_assert(data[end - 1] == value);
		if (end < size) {
			test_assert(data[end] > value);
		}
	}
	test_assert(find_uint(data, size, UINT32_MAX) == SIZE_MAX);

	sort_pairs_by_b(pair_buffer, pair_tmp_buffer, size);
	for (size_t i = 0; i < size; i++) {
		test_assert(pair_buffer[i].b == (uint32_t)i);
	}
}

void test_sort_strings(uint32_t *data, size_t size)
{
	assert(size <= MAX_SORT_SIZE);
	static size_t call_count = 0;
	static const char *str_buffer[MAX_SORT_SIZE];
	static char *str_tmp_buffer[MAX_SORT_SIZE];
	static char str_data_buffer[MAX_SORT_SIZE * 32];

	char *data_ptr = str_data_buffer, *data_end = data_ptr + sizeof(str_data_buffer);
	for (size_t i = 0; i < size; i++) {
		int len = snprintf(data_ptr, data_end - data_ptr, "%u", data[i]);
		test_assert(len > 0);
		str_buffer[i] = data_ptr;
		data_ptr += len + 1;
	}

	sort_strings(str_buffer, str_tmp_buffer, size);

	for (size_t i = 1; i < size; i++) {
		test_assert(strcmp(str_buffer[i - 1], str_buffer[i]) <= 0);
	}

	char find_str[128];
	for (size_t i = 0; i < size; i++) {
		int len = snprintf(find_str, sizeof(find_str), "%u", data[i]);
		test_assert(len > 0);

		size_t index = find_first_string(str_buffer, size, find_str);
		test_assert(index < size);
		test_assert(strcmp(str_buffer[index], find_str) == 0);
		if (index > 0) {
			test_assert(strcmp(str_buffer[index - 1], find_str) < 0);
		}
	}
}

void test_sorts()
{
	static uint32_t sort_buffer[MAX_SORT_SIZE];

	for (int i = 0; i < SORT_MODE_COUNT; i++) {
		g_sort_mode = (sort_mode)i;
		g_linear_size = 2;

		while (g_linear_size <= 64) {
			printf("test_sorts(): sort_mode=%d linear_size=%zu\n",
				(int)g_sort_mode, g_linear_size);

			for (size_t size = 0; size < MAX_SORT_SIZE; size += 1+ size/128 + size/512*32) {
				generate_linear(sort_buffer, size, 0, +1);
				test_sort(sort_buffer, size);
				generate_linear(sort_buffer, size, (uint32_t)size, -1);
				test_sort(sort_buffer, size);
				generate_random(sort_buffer, size, (uint32_t)size, 1+size%10);
				test_sort(sort_buffer, size);
				generate_random(sort_buffer, size, (uint32_t)size, UINT32_MAX);
				test_sort(sort_buffer, size);
			}

			{
				size_t size = MAX_SORT_SIZE;
				generate_linear(sort_buffer, size, 0, +1);
				test_sort_strings(sort_buffer, size);
				generate_linear(sort_buffer, size, (uint32_t)size, -1);
				test_sort_strings(sort_buffer, size);
				generate_random(sort_buffer, size, (uint32_t)size, 1+size%10);
				test_sort_strings(sort_buffer, size);
				generate_random(sort_buffer, size, (uint32_t)size, UINT32_MAX);
				test_sort_strings(sort_buffer, size);
			}

			g_linear_size += 1+g_linear_size/8;
		}
	}
}

int main(int argc, char **argv)
{
	test_sorts();

	return 0;
}
