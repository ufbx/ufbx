#include <stdint.h>
void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr);

#define ufbx_assert(cond) do { \
		if (!(cond)) ufbxt_assert_fail(__FILE__, __LINE__, "Internal assert: " #cond); \
	} while (0)

#include "../ufbx_implementation.h"

#undef ufbx_assert

#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>

#define ufbxt_memory_context(data) \
	ufbxt_make_memory_context(data, (uint32_t)sizeof(data) - 1)
#define ufbxt_memory_context_values(data) \
	ufbxt_make_memory_context_values(data, (uint32_t)sizeof(data) - 1)

#define ufbxt_assert(cond) do { \
		if (!(cond)) ufbxt_assert_fail(__FILE__, __LINE__, #cond); \
	} while (0)

#define ufbxt_assert_eq(a, b, size) do { \
		ufbxt_assert_eq_test(a, b, size, __FILE__, __LINE__, \
			"ufbxt_assert_eq(" #a ", " #b ", " #size ")"); \
	} while (0)

typedef struct {
	int failed;
	const char *file;
	uint32_t line;
	const char *expr;
} ufbxt_fail;

typedef struct {
	const char *name;
	void (*func)(void);

	ufbxt_fail fail;
} ufbxt_test;

ufbxt_test *g_current_test;

ufbx_error g_error;
ufbxi_context g_context;
ufbxi_ascii g_ascii;
jmp_buf g_test_jmp;
int g_verbose;

char g_log_buf[8*1024];
uint32_t g_log_pos;

char g_hint[8*1024];

ufbxi_context *ufbxt_make_memory_context(const void *data, uint32_t size)
{
	char *data_copy = malloc(size + 13);
	memcpy(data_copy, data, size);
	memset(data_copy + size, 0, 13);
	ufbxi_context *uc = &g_context;
	memset(uc, 0, sizeof(ufbxi_context));
	uc->error = &g_error;
	uc->data = data_copy;
	uc->size = size + 13;
	uc->node_stack_top = uc->node_stack;
	uc->node_stack_top->end_pos = size;
	return uc;
}

ufbxi_context *ufbxt_make_memory_context_values(const void *data, uint32_t size)
{
	ufbxi_context *uc = ufbxt_make_memory_context(data, size);
	uc->focused_node.child_begin_pos = size;
	uc->focused_node.next_child_pos = size;
	uc->focused_node.end_pos = size;
	return uc;
}

ufbxi_ascii *ufbxt_ascii_context(const char *data)
{
	ufbxi_ascii *ua = &g_ascii;
	memset(ua, 0, sizeof(ufbxi_ascii));
	ua->src = data;
	ua->src_end = data + strlen(data);
	ua->error = &g_error;
	return ua;
}

void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr)
{
	printf("FAIL\n");
	fflush(stdout);

	g_current_test->fail.failed = 1;
	g_current_test->fail.file = file;
	g_current_test->fail.line = line;
	g_current_test->fail.expr = expr;

	longjmp(g_test_jmp, 1);
}

void ufbxt_logf(const char *fmt, ...)
{
	if (!g_verbose) return;

	va_list args;
	va_start(args, fmt);
	g_log_pos += vsnprintf(g_log_buf + g_log_pos,
		sizeof(g_log_buf) - g_log_pos, fmt, args);
	if (g_log_pos < sizeof(g_log_buf)) {
		g_log_buf[g_log_pos] = '\n';
		g_log_pos++;
	}
	va_end(args);
}

void ufbxt_hintf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(g_hint, sizeof(g_hint), fmt, args);
	va_end(args);
}

void ufbxt_assert_eq_test(const void *a, const void *b, size_t size, const char *file, uint32_t line, const char *expr)
{
	const char *ac = (const char *)a;
	const char *bc = (const char *)b;
	for (size_t i = 0; i < size; i++) {
		if (ac[i] == bc[i]) continue;

		ufbxt_logf("Byte offset %u: 0x%02x != 0x%02x\n", (uint32_t)i, (uint8_t)ac[i], (uint8_t)bc[i]);
		ufbxt_assert_fail(file, line, expr);
	}
}

void ufbxt_log_flush()
{
	int prev_newline = 1;
	for (uint32_t i = 0; i < g_log_pos; i++) {
		char ch = g_log_buf[i];
		if (ch == '\n') {
			putchar('\n');
			prev_newline = 1;
		} else {
			if (prev_newline) {
				putchar(' ');
				putchar(' ');
			}
			prev_newline = 0;
			putchar(ch);
		}
	}
	g_log_pos = 0;
}

void ufbxt_log_error(ufbxi_context *uc)
{
	ufbxt_logf("(line %u) %s", uc->error->source_line, uc->error->desc);
}

void ufbxt_log_ascii_error(ufbxi_ascii *ua)
{
	ufbxt_logf("(line %u) %s", ua->error->source_line, ua->error->desc);
}

#define UFBXT_IMPL 1
#define UFBXT_TEST(name) void ufbxt_test_fn_##name(void)
#include "all_tests.h"

#undef UFBXT_IMPL
#undef UFBXT_TEST
#define UFBXT_IMPL 0
#define UFBXT_TEST(name) { #name, &ufbxt_test_fn_##name },
ufbxt_test g_tests[] = {
	#include "all_tests.h"
};

int ufbxt_run_test(ufbxt_test *test)
{
	printf("%s: ", test->name);
	fflush(stdout);

	g_error.desc[0] = 0;
	g_hint[0] = '\0';

	g_current_test = test;
	if (!setjmp(g_test_jmp)) {
		test->func();
		printf("OK\n");
		fflush(stdout);
		return 1;
	} else {
		if (g_hint[0]) {
			ufbxt_logf("Hint: %s", g_hint);
		}
		if (g_error.desc[0]) {
			ufbxt_log_error(&g_context);
		}

		return 0;
	}
}

int main(int argc, char **argv)
{
	uint32_t num_tests = ufbxi_arraycount(g_tests);
	uint32_t num_ok = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			g_verbose = 1;
		}
	}

	for (uint32_t i = 0; i < num_tests; i++) {
		ufbxt_test *test = &g_tests[i];
		if (ufbxt_run_test(test)) {
			num_ok++;
		}
		ufbxt_log_flush();
	}

	if (num_ok < num_tests) {
		printf("\n");
		for (uint32_t i = 0; i < num_tests; i++) {
			ufbxt_test *test = &g_tests[i];
			if (test->fail.failed) {
				ufbxt_fail *fail = &test->fail;
				const char *file = fail->file, *find;
				find = strrchr(file, '/');
				file = find ? find + 1 : file;
				find = strrchr(file, '\\');
				file = find ? find + 1 : file;
				printf("(%s) %s:%u: %s\n", test->name,
					file, fail->line, fail->expr);
			}
		}
	}

	printf("\nTests passed: %u/%u\n", num_ok, num_tests);

	return num_ok == num_tests ? 0 : 1;
}
