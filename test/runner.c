#define _CRT_SECURE_NO_WARNINGS

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

// -- Timing

typedef struct {
	uint64_t os_tick;
	uint64_t cpu_tick;
} cputime_sync_point;

typedef struct {
	cputime_sync_point begin, end;
	uint64_t os_freq;
	uint64_t cpu_freq;
	double rcp_os_freq;
	double rcp_cpu_freq;
} cputime_sync_span;

extern const cputime_sync_span *cputime_default_sync;

void cputime_begin_init();
void cputime_end_init();
void cputime_init();

void cputime_begin_sync(cputime_sync_span *span);
void cputime_end_sync(cputime_sync_span *span);

uint64_t cputime_cpu_tick();
uint64_t cputime_os_tick();

double cputime_cpu_delta_to_sec(const cputime_sync_span *span, uint64_t cpu_delta);
double cputime_os_delta_to_sec(const cputime_sync_span *span, uint64_t os_delta);
double cputime_cpu_tick_to_sec(const cputime_sync_span *span, uint64_t cpu_tick);
double cputime_os_tick_to_sec(const cputime_sync_span *span, uint64_t os_tick);

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void cputime_sync_now(cputime_sync_point *sync, int accuracy)
{
	uint64_t best_delta = UINT64_MAX;
	uint64_t os_tick, cpu_tick;

	int runs = accuracy ? accuracy : 100;
	for (int i = 0; i < runs; i++) {
		LARGE_INTEGER begin, end;
		QueryPerformanceCounter(&begin);
		uint64_t cycle = __rdtsc();
		QueryPerformanceCounter(&end);

		uint64_t delta = end.QuadPart - begin.QuadPart;
		if (delta < best_delta) {
			os_tick = (begin.QuadPart + end.QuadPart) / 2;
			cpu_tick = cycle;
		}

		if (delta == 0) break;
	}

	sync->cpu_tick = cpu_tick;
	sync->os_tick = os_tick;
}

uint64_t cputime_cpu_tick()
{
	return __rdtsc();
}

uint64_t cputime_os_tick()
{
	LARGE_INTEGER res;
	QueryPerformanceCounter(&res);
	return res.QuadPart;
}

static uint64_t cputime_os_freq()
{
	LARGE_INTEGER res;
	QueryPerformanceFrequency(&res);
	return res.QuadPart;
}

static void cputime_os_wait()
{
	Sleep(1);
}

#else

#include <time.h>
// TODO: Other architectures
#include <x86intrin.h>

void cputime_sync_now(cputime_sync_point *sync, int accuracy)
{
	uint64_t best_delta = UINT64_MAX;
	uint64_t os_tick, cpu_tick;

	struct timespec begin, end;

	int runs = accuracy ? accuracy : 100;
	for (int i = 0; i < runs; i++) {
		clock_gettime(CLOCK_REALTIME, &begin);
		uint64_t cycle = (uint64_t)__rdtsc();
		clock_gettime(CLOCK_REALTIME, &end);

		uint64_t begin_ns = (uint64_t)begin.tv_sec*UINT64_C(1000000000) + (uint64_t)begin.tv_nsec;
		uint64_t end_ns = (uint64_t)end.tv_sec*UINT64_C(1000000000) + (uint64_t)end.tv_nsec;

		uint64_t delta = end_ns - begin_ns;
		if (delta < best_delta) {
			os_tick = (begin_ns + end_ns) / 2;
			cpu_tick = cycle;
		}

		if (delta == 0) break;
	}

	sync->cpu_tick = cpu_tick;
	sync->os_tick = os_tick;
}

uint64_t cputime_cpu_tick()
{
	return (uint64_t)__rdtsc();
}

uint64_t cputime_os_tick()
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts.tv_sec*UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static uint64_t cputime_os_freq()
{
	return UINT64_C(1000000000);
}

static void cputime_os_wait()
{
	struct timespec duration;
	duration.tv_sec = 0;
	duration.tv_nsec = 1000000000l;
	nanosleep(&duration, NULL);
}

#endif

static cputime_sync_span g_cputime_sync;
const cputime_sync_span *cputime_default_sync = &g_cputime_sync;

void cputime_begin_init()
{
	cputime_begin_sync(&g_cputime_sync);
}

void cputime_end_init()
{
	cputime_end_sync(&g_cputime_sync);
}

void cputime_init()
{
	cputime_begin_init();
	cputime_end_init();
}

void cputime_begin_sync(cputime_sync_span *span)
{
	cputime_sync_now(&span->begin, 0);
}

void cputime_end_sync(cputime_sync_span *span)
{
	uint64_t os_freq = cputime_os_freq();

	uint64_t min_span = os_freq / 1000;
	uint64_t os_tick = cputime_os_tick();
	while (os_tick - span->begin.os_tick <= min_span) {
		cputime_os_wait();
		os_tick = cputime_os_tick();
	}

	cputime_sync_now(&span->end, 0);
	uint64_t len_os = span->end.os_tick - span->begin.os_tick;
	uint64_t len_cpu = span->end.cpu_tick - span->begin.cpu_tick;
	double cpu_freq = (double)len_cpu / (double)len_os * (double)os_freq;

	span->os_freq = os_freq;
	span->cpu_freq = (uint64_t)cpu_freq;
	span->rcp_os_freq = 1.0 / (double)os_freq;
	span->rcp_cpu_freq = 1.0 / cpu_freq;
}

double cputime_cpu_delta_to_sec(const cputime_sync_span *span, uint64_t cpu_delta)
{
	if (!span) span = &g_cputime_sync;
	return (double)cpu_delta * span->rcp_cpu_freq;
}

double cputime_os_delta_to_sec(const cputime_sync_span *span, uint64_t os_delta)
{
	if (!span) span = &g_cputime_sync;
	return (double)os_delta * span->rcp_os_freq;
}

double cputime_cpu_tick_to_sec(const cputime_sync_span *span, uint64_t cpu_tick)
{
	if (!span) span = &g_cputime_sync;
	return (double)(cpu_tick - span->begin.cpu_tick) * span->rcp_cpu_freq;
}

double cputime_os_tick_to_sec(const cputime_sync_span *span, uint64_t os_tick)
{
	if (!span) span = &g_cputime_sync;
	return (double)(os_tick - span->begin.os_tick) * span->rcp_os_freq;
}

// -- Test framework

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
uint64_t g_bechmark_begin_tick;

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
	uc->node_stack_top->node.end_pos = size;
	return uc;
}

ufbxi_context *ufbxt_make_memory_context_values(const void *data, uint32_t size)
{
	ufbxi_context *uc = ufbxt_make_memory_context(data, size);
	uc->focused_node.child_begin_pos = size;
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

void ufbxt_log_error_common(ufbx_error *err)
{
	if (!err) return;
	if (err->stack_size > 0) {
		char stack[128];
		char *ptr = stack, *end = stack + sizeof(stack);
		for (uint32_t i = 0; i < err->stack_size; i++) {
			ptr += snprintf(ptr, end - ptr, " / %s", err->stack[i]);
		}
		ufbxt_logf("%s", stack);
	}
	ufbxt_logf("(line %u) %s", err->source_line, err->desc);
}

void ufbxt_log_error(ufbxi_context *uc)
{
	ufbxt_log_error_common(uc->error);
}

void ufbxt_log_ascii_error(ufbxi_ascii *ua)
{
	ufbxt_log_error_common(ua->error);
}

void ufbxt_bechmark_begin()
{
	g_bechmark_begin_tick = cputime_cpu_tick();
}

double ufbxt_bechmark_end()
{
	uint64_t end_tick = cputime_cpu_tick();
	uint64_t delta = end_tick - g_bechmark_begin_tick;
	double sec = cputime_cpu_delta_to_sec(NULL, delta);
	double ghz = (double)cputime_default_sync->cpu_freq / 1e9;
	ufbxt_logf("%.3fms / %ukcy at %.2fGHz", sec * 1e3, (uint32_t)(delta / 1000), ghz);
	return sec;
}

char data_root[256];

void ufbxt_do_file_test(const char *name, void (*test_fn)(ufbx_scene *s))
{
	const uint32_t file_versions[] = { 6100, 7100, 7400, 7500 };

	char buf[512];

	uint32_t num_opened = 0;

	for (uint32_t vi = 0; vi < ufbxi_arraycount(file_versions); vi++) {
		for (uint32_t fi = 0; fi < 2; fi++) {
			uint32_t version = file_versions[vi];
			const char *format = fi == 1 ? "ascii" : "binary";
			snprintf(buf, sizeof(buf), "%s%s_%u_%s.fbx", data_root, name, version, format);

			FILE *file = fopen(buf, "rb");
			if (!file) continue;
			num_opened++;

			ufbxt_logf("%s", buf);

			fseek(file, 0, SEEK_END);
			size_t size = ftell(file);
			fseek(file, 0, SEEK_SET);

			char *data = malloc(size);
			ufbxt_assert(data != NULL);
			size_t num_read = fread(data, 1, size, file);
			fclose(file);

			if (num_read != size) {
				ufbxt_assert_fail(__FILE__, __LINE__, "Failed to load file");
			}

			ufbx_error error;
			ufbx_scene *scene = ufbx_load_memory(data, size, &error);
			if (!scene) {
				ufbxt_log_error_common(&error);
				ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file");
			}

			ufbxt_assert(scene->metadata.ascii == ((fi == 1) ? 1 : 0));
			ufbxt_assert(scene->metadata.version == version);

			test_fn(scene);

			ufbx_free_scene(scene);
			free(data);
		}
	}

	if (num_opened == 0) {
		ufbxt_assert_fail(__FILE__, __LINE__, "File not found");
	}
}

#define UFBXT_IMPL 1
#define UFBXT_TEST(name) void ufbxt_test_fn_##name(void)
#define UFBXT_FILE_TEST(name) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene)

#include "all_tests.h"

#undef UFBXT_IMPL
#undef UFBXT_TEST
#undef UFBXT_FILE_TEST
#define UFBXT_IMPL 0
#define UFBXT_TEST(name) { #name, &ufbxt_test_fn_##name },
#define UFBXT_FILE_TEST(name) { "file_" #name, &ufbxt_test_fn_file_##name },
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
	const char *test_filter = NULL;

	cputime_init();

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			g_verbose = 1;
		}
		if (!strcmp(argv[i], "-t")) {
			if (++i < argc) {
				test_filter = argv[i];
			}
		}
		if (!strcmp(argv[i], "-d")) {
			if (++i < argc) {
				size_t len = strlen(argv[i]);
				if (len + 2 > sizeof(data_root)) {
					fprintf(stderr, "-d: Data root too long");
					return 1;
				}
				memcpy(data_root, argv[i], len);
				char end = argv[i][len - 1];
				if (end != '/' && end != '\\') {
					data_root[len] = '/';
					data_root[len + 1] = '\0';
				}
			}
		}
	}

	uint32_t num_ran = 0;
	for (uint32_t i = 0; i < num_tests; i++) {
		ufbxt_test *test = &g_tests[i];
		if (test_filter && strcmp(test->name, test_filter)) {
			continue;
		}

		num_ran++;
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

	printf("\nTests passed: %u/%u\n", num_ok, num_ran);

	return num_ok == num_ran ? 0 : 1;
}
