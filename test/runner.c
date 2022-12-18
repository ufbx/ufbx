#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stdbool.h>
void ufbxt_assert_fail_imp(const char *file, uint32_t line, const char *expr, bool fatal);
static void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr) {
	ufbxt_assert_fail_imp(file, line, expr, true);
}

#include "../ufbx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#if defined(UFBXT_STACK_LIMIT)
	static int ufbxt_main_argc;
	static char **ufbxt_main_argv;
	static int ufbxt_main_return;
	#if defined(_WIN32)
		#define NOMINMAX
		#define WIN32_LEAN_AND_MEAN
		#include <Windows.h>

		#define UFBXT_THREAD_ENTRYPOINT DWORD WINAPI ufbxt_win32_entry(LPVOID _param)
		#define ufbxt_thread_return() return 0

		UFBXT_THREAD_ENTRYPOINT;
		static bool ufbxt_run_thread() {
			HANDLE handle = CreateThread(NULL, (SIZE_T)(UFBXT_STACK_LIMIT), &ufbxt_win32_entry, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION , NULL);
			if (handle == NULL) return false;
			WaitForSingleObject(handle, INFINITE);
			CloseHandle(handle);
			return true;
		}
	#else
		#include <pthread.h>

		#define UFBXT_THREAD_ENTRYPOINT void *ufbxt_pthread_entry(void *param)
		#define ufbxt_thread_return() return 0

		UFBXT_THREAD_ENTRYPOINT;
		static bool ufbxt_run_thread() {
			pthread_attr_t attr;
			pthread_t thread;
			if (pthread_attr_init(&attr)) return false;
			if (pthread_attr_setstacksize(&attr, (size_t)(UFBXT_STACK_LIMIT))) return false;
			if (pthread_create(&thread, &attr, ufbxt_pthread_entry, NULL)) return false;
			if (pthread_join(thread, NULL)) return false;
			return true;
		}
	#endif
#endif

// -- Thread local

#define UFBXT_HAS_THREADLOCAL 1

#if defined(_MSC_VER)
	#define ufbxt_threadlocal __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
	#define ufbxt_threadlocal __thread
#else
	#define ufbxt_threadlocal
	#undef UFBXT_HAS_THREADLOCAL
	#define UFBXT_HAS_THREADLOCAL 0
#endif

#ifndef USE_SETJMP
#if !defined(__wasm__) && UFBXT_HAS_THREADLOCAL
	#define USE_SETJMP 1
#else
	#define USE_SETJMP 0
#endif
#endif

#if USE_SETJMP

#include <setjmp.h>

#define ufbxt_jmp_buf jmp_buf
#define ufbxt_setjmp(env) setjmp(env)
#define ufbxt_longjmp(env, status, file, line, expr) longjmp(env, status)

#else

#define ufbxt_jmp_buf int
#define ufbxt_setjmp(env) (0)

static void ufbxt_longjmp(int env, int value, const char *file, uint32_t line, const char *expr)
{
	fprintf(stderr, "\nAssertion failed: %s:%u: %s\n", file, line, expr);
	exit(1);
}

#endif

#define CPUTIME_IMPLEMENTATION
#include "cputime.h"

#if defined(_OPENMP)
	#include <omp.h>
#else
	static int omp_get_thread_num() { return 0; }
	static int omp_get_num_threads() { return 1; }
#endif

// -- Test framework

#define ufbxt_memory_context(data) \
	ufbxt_make_memory_context(data, (uint32_t)sizeof(data) - 1)
#define ufbxt_memory_context_values(data) \
	ufbxt_make_memory_context_values(data, (uint32_t)sizeof(data) - 1)

#define ufbxt_assert(cond) do { \
		if (!(cond)) ufbxt_assert_fail_imp(__FILE__, __LINE__, #cond, true); \
	} while (0)

#define ufbxt_soft_assert(cond) do { \
		if (!(cond)) ufbxt_assert_fail_imp(__FILE__, __LINE__, #cond, false); \
	} while (0)

#define ufbxt_assert_eq(a, b, size) do { \
		ufbxt_assert_eq_test(a, b, size, __FILE__, __LINE__, \
			"ufbxt_assert_eq(" #a ", " #b ", " #size ")"); \
	} while (0)

#include "check_scene.h"
#include "testing_utils.h"

typedef struct {
	int failed;
	const char *file;
	uint32_t line;
	const char *expr;
} ufbxt_fail;

typedef struct {
	const char *group;
	const char *name;
	void (*func)(void);

	ufbxt_fail fail;
} ufbxt_test;

ufbxt_test *g_current_test;
uint64_t g_bechmark_begin_tick;

ufbx_error g_error;
ufbxt_jmp_buf g_test_jmp;
int g_verbose;

char g_log_buf[16*1024];
uint32_t g_log_pos;

char g_hint[8*1024];

bool g_skip_print_ok = false;
int g_skip_obj_test = false;

bool g_no_fuzz = false;

typedef struct {
	size_t step;
	char *test_name;
	uint8_t patch_value;
	uint32_t patch_offset;
	uint32_t temp_limit;
	uint32_t result_limit;
	uint32_t truncate_length;
	uint32_t cancel_step;
	const char *description;
} ufbxt_check_line;

static ufbxt_check_line g_checks[32768];

bool g_expect_fail = false;
size_t g_expect_fail_count = 0;

ufbxt_threadlocal ufbxt_jmp_buf *t_jmp_buf;

void ufbxt_assert_fail_imp(const char *file, uint32_t line, const char *expr, bool fatal)
{
	if (!fatal && g_expect_fail) {
		g_expect_fail_count++;
		return;
	}

	if (t_jmp_buf) {
		ufbxt_longjmp(*t_jmp_buf, 1, file, line, expr);
	}

	printf("FAIL\n");
	fflush(stdout);

	g_current_test->fail.failed = 1;
	g_current_test->fail.file = file;
	g_current_test->fail.line = line;
	g_current_test->fail.expr = expr;

	ufbxt_longjmp(g_test_jmp, 1, file, line, expr);
}

void ufbxt_logf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (g_log_pos < sizeof(g_log_buf)) {
		g_log_pos += vsnprintf(g_log_buf + g_log_pos,
			sizeof(g_log_buf) - g_log_pos, fmt, args);
		if (g_log_pos < sizeof(g_log_buf)) {
			g_log_buf[g_log_pos] = '\n';
			g_log_pos++;
		}
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

void ufbxt_log_flush(bool print_always)
{
	if ((g_verbose || print_always) && g_log_pos > 0) {
		int prev_newline = 1;
		for (uint32_t i = 0; i < g_log_pos; i++) {
			if (i >= sizeof(g_log_buf)) break;
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
	}
	g_log_pos = 0;
}

void ufbxt_log_error(ufbx_error *err)
{
	if (!err) return;
	ufbxt_logf("Error: %s", err->description.data);
	for (size_t i = 0; i < err->stack_size; i++) {
		ufbx_error_frame *f = &err->stack[i];
		ufbxt_logf("Line %u %s: %s", f->source_line, f->function.data, f->description.data);
	}
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

// -- Test allocator

typedef struct {
	size_t offset;
	size_t bytes_allocated;
	union {
		bool *freed_ptr;
		size_t size_and_align[2];
	};

	char data[1024 * 1024];
} ufbxt_allocator;

static void *ufbxt_alloc(void *user, size_t size)
{
	ufbxt_allocator *ator = (ufbxt_allocator*)user;
	ator->bytes_allocated += size;
	if (size < 1024 && sizeof(ator->data) - ator->offset >= size) {
		void *ptr = ator->data + ator->offset;
		ator->offset = (ator->offset + size + 7) & ~(size_t)0x7;
		return ptr;
	} else {
		return malloc(size);
	}
}

static void ufbxt_free(void *user, void *ptr, size_t size)
{
	ufbxt_allocator *ator = (ufbxt_allocator*)user;
	ator->bytes_allocated -= size;
	if ((uintptr_t)ptr >= (uintptr_t)ator->data
		&& (uintptr_t)ptr < (uintptr_t)(ator->data + sizeof(ator->data))) {
		// Nop
	} else {
		free(ptr);
	}
}

static void ufbxt_free_allocator(void *user)
{
	ufbxt_allocator *ator = (ufbxt_allocator*)user;
	ufbxt_assert(ator->bytes_allocated == 0);
	*ator->freed_ptr = true;
	free(ator);
}

char data_root[256];

static uint32_t g_file_version = 0;
static const char *g_file_type = NULL;
static bool g_fuzz = false;
static bool g_sink = false;
static bool g_allow_non_thread_safe = false;
static bool g_all_byte_values = false;
static bool g_dedicated_allocs = false;
static bool g_fuzz_no_patch = false;
static bool g_fuzz_no_truncate = false;
static bool g_fuzz_no_cancel = false;
static bool g_fuzz_no_buffer = false;
static int g_patch_start = 0;
static int g_fuzz_quality = 16;
static int g_heavy_fuzz_quality = -1;
static size_t g_fuzz_step = SIZE_MAX;
static size_t g_fuzz_file = SIZE_MAX;
static size_t g_deflate_opt = SIZE_MAX;

const char *g_fuzz_test_name = NULL;

void ufbxt_init_allocator(ufbx_allocator_opts *ator, bool *freed_ptr)
{
	ator->memory_limit = 0x4000000; // 64MB

	if (g_dedicated_allocs) {
		*freed_ptr = true;
		return;
	}

	ufbxt_allocator *at = (ufbxt_allocator*)malloc(sizeof(ufbxt_allocator));
	ufbxt_assert(at);
	at->offset = 0;
	at->bytes_allocated = 0;
	at->freed_ptr = freed_ptr;
	*freed_ptr = false;

	ator->allocator.user = at;
	ator->allocator.alloc_fn = &ufbxt_alloc;
	ator->allocator.free_fn = &ufbxt_free;
	ator->allocator.free_allocator_fn = &ufbxt_free_allocator;
}

static bool ufbxt_begin_fuzz()
{
	if (g_fuzz) {
		if (!g_skip_print_ok) {
			printf("FUZZ\n");
			g_skip_print_ok = true;
		}
		return true;
	} else {
		return false;
	}
}

static void ufbxt_begin_expect_fail()
{
	ufbxt_assert(!g_expect_fail);
	g_expect_fail = true;
	g_expect_fail_count = 0;
}

static size_t ufbxt_end_expect_fail()
{
	ufbxt_assert(g_expect_fail);
	ufbxt_assert(g_expect_fail_count > 0);
	g_expect_fail = false;
	return g_expect_fail_count;
}

typedef struct {
	size_t calls_left;
} ufbxt_cancel_ctx;

ufbx_progress_result ufbxt_cancel_progress(void *user, const ufbx_progress *progress)
{
	ufbxt_cancel_ctx *ctx = (ufbxt_cancel_ctx*)user;
	return --ctx->calls_left > 0 ? UFBX_PROGRESS_CONTINUE : UFBX_PROGRESS_CANCEL;
}

int ufbxt_test_fuzz(const char *filename, void *data, size_t size, const ufbx_load_opts *default_opts, size_t step, int offset, size_t temp_limit, size_t result_limit, size_t truncate_length, size_t cancel_step)
{
	if (g_fuzz_step < SIZE_MAX && step != g_fuzz_step) return 1;

	#if UFBXT_HAS_THREADLOCAL
		t_jmp_buf = (ufbxt_jmp_buf*)calloc(1, sizeof(ufbxt_jmp_buf));
	#endif

	int ret = 1;
	if (!ufbxt_setjmp(*t_jmp_buf)) {

		ufbx_load_opts opts = { 0 };
		ufbxt_cancel_ctx cancel_ctx = { 0 };

		if (default_opts) {
			opts = *default_opts;
		}

		opts.load_external_files = true;
		opts.filename.data = filename;
		opts.filename.length = SIZE_MAX;

		bool temp_freed = false, result_freed = false;
		ufbxt_init_allocator(&opts.temp_allocator, &temp_freed);
		ufbxt_init_allocator(&opts.result_allocator, &result_freed);

		opts.temp_allocator.allocation_limit = temp_limit;
		opts.result_allocator.allocation_limit = result_limit;

		if (temp_limit > 0) {
			opts.temp_allocator.huge_threshold = 1;
		}

		if (result_limit > 0) {
			opts.result_allocator.huge_threshold = 1;
		}

		if (cancel_step > 0) {
			cancel_ctx.calls_left = cancel_step;
			opts.progress_cb.fn = &ufbxt_cancel_progress;
			opts.progress_cb.user = &cancel_ctx;
			opts.progress_interval_hint = 1;
		}

		if (g_dedicated_allocs) {
			opts.temp_allocator.huge_threshold = 1;
			opts.result_allocator.huge_threshold = 1;
		}

		if (truncate_length > 0) size = truncate_length;

		ufbx_error error;
		ufbx_scene *scene = ufbx_load_memory(data, size, &opts, &error);
		if (scene) {
			ufbxt_check_scene(scene);
			ufbx_free_scene(scene);
		} else {

			// Collect hit checks
			for (size_t i = 0; i < error.stack_size; i++) {
				ufbx_error_frame frame = error.stack[i];
				ufbxt_check_line *check = &g_checks[frame.source_line];
				if (check->test_name && strcmp(g_fuzz_test_name, check->test_name) != 0) continue;
				if (check->step && check->step < step) continue;

				#pragma omp critical(check)
				{
					bool ok = check->step == 0 || check->step > step;
					if (check->test_name && strcmp(g_fuzz_test_name, check->test_name) != 0) ok = false;

					if (ok) {
						if (!check->test_name) {
							size_t name_len = strlen(g_fuzz_test_name) + 1;
							check->test_name = (char*)malloc(name_len);
							if (check->test_name) {
								memcpy(check->test_name, g_fuzz_test_name, name_len);
							}
						}
						if (offset < 0) {
							check->patch_offset = UINT32_MAX;
							check->patch_value = 0;
						} else {
							check->patch_offset = offset + 1;
							check->patch_value = ((uint8_t*)data)[offset];
						}
						check->step = step;
						check->temp_limit = (uint32_t)temp_limit;
						check->result_limit = (uint32_t)result_limit;
						check->truncate_length = (uint32_t)truncate_length;
						check->cancel_step = (uint32_t)cancel_step;
						check->description = frame.description.data;
					}
				}
			}
		}

		ufbxt_assert(temp_freed);
		ufbxt_assert(result_freed);

	} else {
		ret = 0;
	}

	#if UFBXT_HAS_THREADLOCAL
		free(t_jmp_buf);
		t_jmp_buf = NULL;
	#endif

	return ret;

}

typedef struct {
	const char *name;
	uint32_t line;
	int32_t patch_offset;
	uint8_t patch_value;
	uint32_t temp_limit;
	uint32_t result_limit;
	uint32_t truncate_length;
	uint32_t cancel_step;
	const char *description;
} ufbxt_fuzz_check;

// Generated by running `runner --fuzz`
// Take both normal and `UFBX_REGRESSION` builds, combine results and use `sort -u` to remove duplciates.
// From commit 2f5c74f
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "blender_279_ball_0_obj", 13909, -1, 0, 0, 31, 0, 0, "props.data" },
	{ "blender_279_ball_0_obj", 13909, -1, 0, 0, 62, 0, 0, "props.data" },
	{ "blender_279_ball_0_obj", 13926, -1, 0, 246, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "blender_279_ball_0_obj", 14072, -1, 0, 2166, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "blender_279_ball_0_obj", 14072, -1, 0, 237, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "blender_279_ball_0_obj", 14327, -1, 0, 134, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "blender_279_ball_0_obj", 14327, -1, 0, 1389, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "blender_279_ball_0_obj", 14484, -1, 0, 0, 0, 3099, 0, "uc->obj.num_tokens >= 2" },
	{ "blender_279_ball_0_obj", 14487, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "blender_279_ball_0_obj", 14487, -1, 0, 1350, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "blender_279_ball_0_obj", 14502, -1, 0, 123, 0, 0, 0, "material" },
	{ "blender_279_ball_0_obj", 14502, -1, 0, 1352, 0, 0, 0, "material" },
	{ "blender_279_ball_0_obj", 14509, -1, 0, 125, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "blender_279_ball_0_obj", 14509, -1, 0, 1359, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "blender_279_ball_0_obj", 14820, -1, 0, 692, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "blender_279_ball_0_obj", 14835, -1, 0, 0, 0, 54, 0, "uc->obj.num_tokens >= 2" },
	{ "blender_279_ball_0_obj", 14838, -1, 0, 13, 0, 0, 0, "lib.data" },
	{ "blender_279_ball_0_obj", 14838, -1, 0, 688, 0, 0, 0, "lib.data" },
	{ "blender_279_ball_0_obj", 14842, -1, 0, 123, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 14842, -1, 0, 1350, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 14861, -1, 0, 0, 62, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "blender_279_ball_0_obj", 14861, -1, 0, 246, 0, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "blender_279_ball_0_obj", 14876, -1, 0, 2181, 0, 0, 0, "prop" },
	{ "blender_279_ball_0_obj", 14876, -1, 0, 238, 0, 0, 0, "prop" },
	{ "blender_279_ball_0_obj", 14879, -1, 0, 0, 15, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "blender_279_ball_0_obj", 14879, -1, 0, 2183, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "blender_279_ball_0_obj", 14912, -1, 0, 0, 16, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->va..." },
	{ "blender_279_ball_0_obj", 14912, -1, 0, 2185, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->va..." },
	{ "blender_279_ball_0_obj", 14913, -1, 0, 2187, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &prop->v..." },
	{ "blender_279_ball_0_obj", 14999, -1, 0, 2166, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "blender_279_ball_0_obj", 14999, -1, 0, 237, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "blender_279_ball_0_obj", 15006, -1, 0, 0, 62, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 15006, -1, 0, 246, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 15007, -1, 0, 2178, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 15015, -1, 0, 2181, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "blender_279_ball_0_obj", 15015, -1, 0, 238, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "blender_279_ball_0_obj", 15019, -1, 0, 0, 33, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 15059, -1, 0, 2162, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "blender_279_ball_0_obj", 15059, -1, 0, 236, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "blender_279_ball_0_obj", 15093, -1, 0, 2166, 0, 0, 0, "ok" },
	{ "blender_279_ball_0_obj", 15093, -1, 0, 237, 0, 0, 0, "ok" },
	{ "blender_279_ball_0_obj", 15104, -1, 0, 2162, 0, 0, 0, "ufbxi_obj_load_mtl(uc)" },
	{ "blender_279_ball_0_obj", 15104, -1, 0, 236, 0, 0, 0, "ufbxi_obj_load_mtl(uc)" },
	{ "blender_279_ball_0_obj", 5502, -1, 0, 2166, 0, 0, 0, "new_buffer" },
	{ "blender_279_ball_0_obj", 5502, -1, 0, 237, 0, 0, 0, "new_buffer" },
	{ "blender_279_ball_7400_binary", 11392, 12516, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_smoothing.da..." },
	{ "blender_279_default_obj", 14525, 481, 48, 0, 0, 0, 0, "min_index < uc->obj.tmp_vertices[attrib].num_items / st..." },
	{ "blender_279_sausage_7400_binary", 11761, -1, 0, 3089, 0, 0, 0, "skin" },
	{ "blender_279_sausage_7400_binary", 11761, -1, 0, 712, 0, 0, 0, "skin" },
	{ "blender_279_sausage_7400_binary", 11793, -1, 0, 3150, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_7400_binary", 11793, -1, 0, 735, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_7400_binary", 11799, 23076, 0, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "blender_279_sausage_7400_binary", 11810, 23900, 0, 0, 0, 0, 0, "transform->size >= 16" },
	{ "blender_279_sausage_7400_binary", 11811, 24063, 0, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "blender_279_sausage_7400_binary", 12169, 21748, 0, 0, 0, 0, 0, "matrix->size >= 16" },
	{ "blender_279_sausage_7400_binary", 12511, -1, 0, 3089, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 12511, -1, 0, 712, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 12513, 23076, 0, 0, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 15962, -1, 0, 13665, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "blender_279_sausage_7400_binary", 18001, -1, 0, 13660, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_7400_binary", 18001, -1, 0, 4415, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_7400_binary", 18046, -1, 0, 0, 382, 0, 0, "skin->vertices.data" },
	{ "blender_279_sausage_7400_binary", 18046, -1, 0, 0, 764, 0, 0, "skin->vertices.data" },
	{ "blender_279_sausage_7400_binary", 18050, -1, 0, 0, 383, 0, 0, "skin->weights.data" },
	{ "blender_279_sausage_7400_binary", 18050, -1, 0, 0, 766, 0, 0, "skin->weights.data" },
	{ "blender_279_sausage_7400_binary", 18105, -1, 0, 13665, 0, 0, 0, "ufbxi_sort_skin_weights(uc, skin)" },
	{ "blender_279_sausage_7400_binary", 18278, -1, 0, 13666, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "blender_279_sausage_7400_binary", 18278, -1, 0, 4417, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "blender_279_unicode_6100_ascii", 13014, 432, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "blender_279_uv_sets_6100_ascii", 11456, -1, 0, 0, 63, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 11456, -1, 0, 3146, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 11462, -1, 0, 3148, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 11462, -1, 0, 728, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 11553, -1, 0, 3152, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 11553, -1, 0, 729, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 11556, -1, 0, 3156, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 11556, -1, 0, 731, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 15326, -1, 0, 13058, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "blender_279_uv_sets_6100_ascii", 18563, -1, 0, 13054, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 18563, -1, 0, 3845, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 18570, -1, 0, 13056, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 18570, -1, 0, 3846, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 18571, -1, 0, 13058, 0, 0, 0, "ufbxi_sort_tmp_material_textures(uc, mat_texs, num_mate..." },
	{ "blender_279_uv_sets_6100_ascii", 6506, -1, 0, 3152, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 6506, -1, 0, 729, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 6510, -1, 0, 3154, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 6510, -1, 0, 730, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_282_suzanne_and_transform_obj", 14317, -1, 0, 0, 2, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "blender_282_suzanne_and_transform_obj", 14317, -1, 0, 0, 4, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "blender_293_instancing_obj", 13959, -1, 0, 8037, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "fuzz_0018", 13572, 810, 0, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "fuzz_0070", 4158, -1, 0, 32, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0070", 4158, -1, 0, 746, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0272", 10533, -1, 0, 2224, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "fuzz_0272", 10534, -1, 0, 452, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "fuzz_0393", 10710, -1, 0, 0, 137, 0, 0, "index_data" },
	{ "fuzz_0393", 10710, -1, 0, 0, 274, 0, 0, "index_data" },
	{ "fuzz_0561", 12507, -1, 0, 2216, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0561", 12507, -1, 0, 451, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "marvelous_quad_7200_binary", 20273, -1, 0, 0, 273, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "max2009_blob_5800_ascii", 12746, 164150, 114, 0, 0, 0, 0, "Unknown slope mode" },
	{ "max2009_blob_5800_ascii", 12776, 164903, 98, 0, 0, 0, 0, "Unknown weight mode" },
	{ "max2009_blob_5800_ascii", 12789, 164150, 116, 0, 0, 0, 0, "Unknown key mode" },
	{ "max2009_blob_5800_ascii", 8548, -1, 0, 15576, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 8548, -1, 0, 4417, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 8556, -1, 0, 0, 116, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 8556, -1, 0, 15578, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 8618, 131240, 45, 0, 0, 0, 0, "Bad array dst type" },
	{ "max2009_blob_5800_ascii", 8674, -1, 0, 23169, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 8674, -1, 0, 6973, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 9433, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "max2009_blob_5800_binary", 10391, -1, 0, 8, 0, 0, 0, "ufbxi_insert_fbx_id(uc, fbx_id, element_id)" },
	{ "max2009_blob_5800_binary", 10412, -1, 0, 3336, 0, 0, 0, "conn" },
	{ "max2009_blob_5800_binary", 10412, -1, 0, 9838, 0, 0, 0, "conn" },
	{ "max2009_blob_5800_binary", 12659, -1, 0, 3391, 0, 0, 0, "curve" },
	{ "max2009_blob_5800_binary", 12659, -1, 0, 9978, 0, 0, 0, "curve" },
	{ "max2009_blob_5800_binary", 12661, -1, 0, 3393, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max2009_blob_5800_binary", 12661, -1, 0, 9985, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max2009_blob_5800_binary", 12666, 119084, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "max2009_blob_5800_binary", 12669, 119104, 255, 0, 0, 0, 0, "curve->keyframes.data" },
	{ "max2009_blob_5800_binary", 12683, 119110, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max2009_blob_5800_binary", 12769, 119110, 16, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "max2009_blob_5800_binary", 12794, 119102, 3, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max2009_blob_5800_binary", 12843, 119102, 1, 0, 0, 0, 0, "data == data_end" },
	{ "max2009_blob_5800_binary", 12860, 114022, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "max2009_blob_5800_binary", 12871, 119084, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "max2009_blob_5800_binary", 12914, -1, 0, 3331, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 12914, -1, 0, 9827, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 12915, -1, 0, 3336, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max2009_blob_5800_binary", 12915, -1, 0, 9838, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max2009_blob_5800_binary", 12918, 119084, 0, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "max2009_blob_5800_binary", 12931, 114858, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "max2009_blob_5800_binary", 12940, 114022, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "max2009_blob_5800_binary", 12973, 114022, 0, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 13227, -1, 0, 2519, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 13227, -1, 0, 576, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 13235, -1, 0, 0, 140, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 13235, -1, 0, 0, 280, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 13276, -1, 0, 107, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 13276, -1, 0, 1178, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 13283, -1, 0, 0, 44, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 13283, -1, 0, 0, 88, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 13291, -1, 0, 1765, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 13291, -1, 0, 310, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 13298, -1, 0, 0, 188, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 13298, -1, 0, 0, 94, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 13331, -1, 0, 2507, 0, 0, 0, "mesh" },
	{ "max2009_blob_5800_binary", 13331, -1, 0, 572, 0, 0, 0, "mesh" },
	{ "max2009_blob_5800_binary", 13342, 9030, 37, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "max2009_blob_5800_binary", 13352, -1, 0, 0, 1328, 0, 0, "index_data" },
	{ "max2009_blob_5800_binary", 13352, -1, 0, 0, 664, 0, 0, "index_data" },
	{ "max2009_blob_5800_binary", 13375, 58502, 255, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "max2009_blob_5800_binary", 13406, -1, 0, 0, 138, 0, 0, "set" },
	{ "max2009_blob_5800_binary", 13406, -1, 0, 0, 276, 0, 0, "set" },
	{ "max2009_blob_5800_binary", 13410, 65596, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, uv_info, (ufbx_vert..." },
	{ "max2009_blob_5800_binary", 13418, 56645, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MaterialAssignation, \"C\",..." },
	{ "max2009_blob_5800_binary", 13420, 56700, 78, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material.dat..." },
	{ "max2009_blob_5800_binary", 13449, 6207, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 13450, -1, 0, 0, 139, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 13450, -1, 0, 2516, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 13451, -1, 0, 2519, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 13451, -1, 0, 576, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 13452, -1, 0, 2526, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 13452, -1, 0, 578, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 13483, 818, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 13484, -1, 0, 0, 43, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 13484, -1, 0, 1164, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 13492, -1, 0, 104, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 13492, -1, 0, 1167, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 13493, -1, 0, 1173, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 13493, -1, 0, 365, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 13496, -1, 0, 105, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13496, -1, 0, 1174, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13501, -1, 0, 106, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13501, -1, 0, 1176, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13508, -1, 0, 107, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13508, -1, 0, 1178, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13510, -1, 0, 1765, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13510, -1, 0, 310, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13514, 6207, 0, 0, 0, 0, 0, "ufbxi_read_legacy_mesh(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13521, -1, 0, 111, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max2009_blob_5800_binary", 13521, -1, 0, 1187, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max2009_blob_5800_binary", 13530, -1, 0, 2604, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13530, -1, 0, 600, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13554, -1, 0, 3, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max2009_blob_5800_binary", 13554, -1, 0, 662, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max2009_blob_5800_binary", 13561, -1, 0, 4, 0, 0, 0, "root" },
	{ "max2009_blob_5800_binary", 13561, -1, 0, 859, 0, 0, 0, "root" },
	{ "max2009_blob_5800_binary", 13563, -1, 0, 869, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 13563, -1, 0, 9, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 13574, 113392, 1, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "max2009_blob_5800_binary", 13578, 818, 0, 0, 0, 0, 0, "ufbxi_read_legacy_model(uc, node)" },
	{ "max2009_blob_5800_binary", 13583, -1, 0, 0, 3550, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "max2009_blob_5800_binary", 13583, -1, 0, 0, 7100, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "max2009_blob_5800_binary", 15347, -1, 0, 3693, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max2009_blob_5800_binary", 15430, -1, 0, 3693, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "max2009_blob_5800_binary", 15535, -1, 0, 10798, 0, 0, 0, "new_prop" },
	{ "max2009_blob_5800_binary", 15535, -1, 0, 3694, 0, 0, 0, "new_prop" },
	{ "max2009_blob_5800_binary", 15552, -1, 0, 0, 355, 0, 0, "elem->props.props.data" },
	{ "max2009_blob_5800_binary", 15552, -1, 0, 0, 710, 0, 0, "elem->props.props.data" },
	{ "max2009_blob_5800_binary", 17816, -1, 0, 0, 411, 0, 0, "mat->face_indices.data" },
	{ "max2009_blob_5800_binary", 17816, -1, 0, 0, 822, 0, 0, "mat->face_indices.data" },
	{ "max2009_blob_5800_binary", 17859, -1, 0, 10798, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "max2009_blob_5800_binary", 17859, -1, 0, 3694, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "max2009_blob_5800_binary", 18226, -1, 0, 0, 410, 0, 0, "materials" },
	{ "max2009_blob_5800_binary", 18226, -1, 0, 0, 820, 0, 0, "materials" },
	{ "max2009_blob_5800_binary", 18271, -1, 0, 0, 411, 0, 0, "ufbxi_finalize_mesh_material(&uc->result, &uc->error, m..." },
	{ "max2009_blob_5800_binary", 18271, -1, 0, 0, 822, 0, 0, "ufbxi_finalize_mesh_material(&uc->result, &uc->error, m..." },
	{ "max2009_blob_5800_binary", 18335, -1, 0, 11041, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "max2009_blob_5800_binary", 18335, -1, 0, 3809, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "max2009_blob_5800_binary", 18359, -1, 0, 11136, 0, 0, 0, "aprop" },
	{ "max2009_blob_5800_binary", 18359, -1, 0, 3841, 0, 0, 0, "aprop" },
	{ "max2009_blob_5800_binary", 7385, -1, 0, 0, 0, 80100, 0, "val" },
	{ "max2009_blob_5800_binary", 7387, 80062, 17, 0, 0, 0, 0, "type == 'S' || type == 'R'" },
	{ "max2009_blob_5800_binary", 7396, 80082, 1, 0, 0, 0, 0, "d->data" },
	{ "max2009_blob_5800_binary", 7402, -1, 0, 0, 119, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max2009_blob_5800_binary", 7402, -1, 0, 2459, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max2009_blob_5800_binary", 9451, -1, 0, 42, 0, 0, 0, "ufbxi_retain_toplevel(uc, &uc->legacy_node)" },
	{ "max2009_blob_5800_binary", 9451, -1, 0, 966, 0, 0, 0, "ufbxi_retain_toplevel(uc, &uc->legacy_node)" },
	{ "max2009_blob_6100_binary", 10504, -1, 0, 1357, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_6100_binary", 11406, 149477, 3, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material.dat..." },
	{ "max2009_blob_6100_binary", 12372, -1, 0, 2107, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 12372, -1, 0, 372, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 12374, -1, 0, 1060, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 12374, -1, 0, 4493, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max7_blend_cube_5000_binary", 10929, -1, 0, 1715, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 10929, -1, 0, 317, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 12882, -1, 0, 1804, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "max7_blend_cube_5000_binary", 13333, 2350, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "max7_cube_5000_binary", 13541, -1, 0, 1239, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &uc->legacy_implicit_anim_l..." },
	{ "max7_cube_5000_binary", 13541, -1, 0, 137, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &uc->legacy_implicit_anim_l..." },
	{ "max7_cube_5000_binary", 13543, 942, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc..." },
	{ "max7_cube_5000_binary", 13592, -1, 0, 0, 106, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "max7_cube_5000_binary", 13592, -1, 0, 4122, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "max7_cube_5000_binary", 13594, -1, 0, 1218, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 13594, -1, 0, 4124, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 13597, -1, 0, 1222, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &stack_info.fbx_id)" },
	{ "max7_cube_5000_binary", 13597, -1, 0, 4133, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &stack_info.fbx_id)" },
	{ "max7_cube_5000_binary", 13599, -1, 0, 1223, 0, 0, 0, "stack" },
	{ "max7_cube_5000_binary", 13599, -1, 0, 4135, 0, 0, 0, "stack" },
	{ "max7_cube_5000_binary", 13601, -1, 0, 1225, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "max7_cube_5000_binary", 13601, -1, 0, 4142, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "max7_skin_5000_binary", 13245, -1, 0, 1776, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 13245, -1, 0, 342, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 13252, 2420, 136, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "max7_skin_5000_binary", 13263, 4378, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "max7_skin_5000_binary", 13264, 4544, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "max7_skin_5000_binary", 13306, -1, 0, 2189, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 13306, -1, 0, 497, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 13318, -1, 0, 0, 102, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 13318, -1, 0, 0, 51, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 13456, 2361, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max7_skin_5000_binary", 13457, -1, 0, 1775, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max7_skin_5000_binary", 13458, 2420, 136, 0, 0, 0, 0, "ufbxi_read_legacy_link(uc, child, &fbx_id, name.data)" },
	{ "max7_skin_5000_binary", 13461, -1, 0, 1785, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 13461, -1, 0, 346, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 13464, -1, 0, 1787, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 13464, -1, 0, 347, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 13465, -1, 0, 1794, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 13465, -1, 0, 349, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 13467, -1, 0, 1796, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 13467, -1, 0, 350, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 13512, -1, 0, 2189, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max7_skin_5000_binary", 13512, -1, 0, 497, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max_cache_box_7500_binary", 20157, -1, 0, 663, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 20327, -1, 0, 663, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_cache_box_7500_binary", 20454, -1, 0, 664, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "max_cache_box_7500_binary", 20483, -1, 0, 665, 0, 0, 0, "chan" },
	{ "max_cache_box_7500_binary", 20513, -1, 0, 0, 220, 0, 0, "cc->cache.channels.data" },
	{ "max_cache_box_7500_binary", 20513, -1, 0, 0, 440, 0, 0, "cc->cache.channels.data" },
	{ "max_cache_box_7500_binary", 20557, -1, 0, 0, 219, 0, 0, "cc->cache.frames.data" },
	{ "max_cache_box_7500_binary", 20557, -1, 0, 0, 438, 0, 0, "cc->cache.frames.data" },
	{ "max_cache_box_7500_binary", 20559, -1, 0, 664, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "max_cache_box_7500_binary", 20560, -1, 0, 0, 440, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "max_cache_box_7500_binary", 20560, -1, 0, 665, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "max_cache_box_7500_binary", 20564, -1, 0, 0, 221, 0, 0, "cc->imp" },
	{ "max_cache_box_7500_binary", 20564, -1, 0, 0, 442, 0, 0, "cc->imp" },
	{ "max_cache_box_7500_binary", 20785, -1, 0, 2951, 0, 0, 0, "files" },
	{ "max_curve_line_7500_ascii", 11672, 8302, 43, 0, 0, 0, 0, "points->size % 3 == 0" },
	{ "max_curve_line_7500_binary", 11665, -1, 0, 2214, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 11665, -1, 0, 426, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 11670, 13861, 255, 0, 0, 0, 0, "points" },
	{ "max_curve_line_7500_binary", 11671, 13985, 56, 0, 0, 0, 0, "points_index" },
	{ "max_curve_line_7500_binary", 11693, -1, 0, 0, 140, 0, 0, "line->segments.data" },
	{ "max_curve_line_7500_binary", 11693, -1, 0, 0, 280, 0, 0, "line->segments.data" },
	{ "max_curve_line_7500_binary", 12501, 13861, 255, 0, 0, 0, 0, "ufbxi_read_line(uc, node, &info)" },
	{ "max_geometry_transform_6100_binary", 10472, -1, 0, 1868, 0, 0, 0, "geo_node" },
	{ "max_geometry_transform_6100_binary", 10472, -1, 0, 317, 0, 0, 0, "geo_node" },
	{ "max_geometry_transform_6100_binary", 10473, -1, 0, 1875, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max_geometry_transform_6100_binary", 10473, -1, 0, 564, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max_geometry_transform_6100_binary", 10477, -1, 0, 0, 108, 0, 0, "props" },
	{ "max_geometry_transform_6100_binary", 10477, -1, 0, 0, 54, 0, 0, "props" },
	{ "max_geometry_transform_6100_binary", 10488, -1, 0, 1876, 0, 0, 0, "ufbxi_connect_oo(uc, geo_fbx_id, node_fbx_id)" },
	{ "max_geometry_transform_6100_binary", 10488, -1, 0, 319, 0, 0, 0, "ufbxi_connect_oo(uc, geo_fbx_id, node_fbx_id)" },
	{ "max_geometry_transform_6100_binary", 10492, -1, 0, 1878, 0, 0, 0, "extra" },
	{ "max_geometry_transform_6100_binary", 10492, -1, 0, 320, 0, 0, 0, "extra" },
	{ "max_geometry_transform_6100_binary", 10507, -1, 0, 1868, 0, 0, 0, "ufbxi_setup_geometry_transform_helper(uc, elem_node, in..." },
	{ "max_geometry_transform_6100_binary", 10507, -1, 0, 317, 0, 0, 0, "ufbxi_setup_geometry_transform_helper(uc, elem_node, in..." },
	{ "max_geometry_transform_6100_binary", 15156, -1, 0, 722, 0, 0, 0, "elements" },
	{ "max_geometry_transform_6100_binary", 15160, -1, 0, 723, 0, 0, 0, "tmp_connections" },
	{ "max_geometry_transform_6100_binary", 15163, -1, 0, 724, 0, 0, 0, "pre_connections" },
	{ "max_geometry_transform_6100_binary", 15166, -1, 0, 725, 0, 0, 0, "instance_counts" },
	{ "max_geometry_transform_6100_binary", 15172, -1, 0, 726, 0, 0, 0, "fbx_ids" },
	{ "max_geometry_transform_6100_binary", 21143, -1, 0, 1, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, str, 0)" },
	{ "max_geometry_transform_6100_binary", 21161, -1, 0, 1, 0, 0, 0, "ufbxi_fixup_opts_string(uc, &uc->opts.geometry_transfor..." },
	{ "max_geometry_transform_6100_binary", 21213, -1, 0, 722, 0, 0, 0, "ufbxi_pre_finalize_scene(uc)" },
	{ "max_geometry_transform_instances_7700_binary", 15237, -1, 0, 2927, 0, 0, 0, "ufbxi_setup_geometry_transform_helper(uc, node, fbx_id)" },
	{ "max_geometry_transform_instances_7700_binary", 15237, -1, 0, 642, 0, 0, 0, "ufbxi_setup_geometry_transform_helper(uc, node, fbx_id)" },
	{ "max_geometry_transform_types_obj", 15287, -1, 0, 30, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_geometry_transform_types_obj", 15311, -1, 0, 27, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_geometry_transform_types_obj", 15625, -1, 0, 27, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "max_geometry_transform_types_obj", 17896, -1, 0, 30, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "max_quote_6100_ascii", 17925, -1, 0, 1392, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 17925, -1, 0, 5440, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 17927, -1, 0, 5442, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 17941, -1, 0, 0, 175, 0, 0, "node->all_attribs.data" },
	{ "max_quote_6100_ascii", 17941, -1, 0, 0, 350, 0, 0, "node->all_attribs.data" },
	{ "max_quote_6100_binary", 11396, 8983, 36, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "max_quote_6100_binary", 11399, 9030, 36, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_visibility.d..." },
	{ "max_shadergraph_7700_ascii", 8260, -1, 0, 1096, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, replacement)" },
	{ "max_shadergraph_7700_ascii", 8260, -1, 0, 4772, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, replacement)" },
	{ "max_texture_mapping_6100_binary", 16939, -1, 0, 10758, 0, 0, 0, "copy" },
	{ "max_texture_mapping_6100_binary", 16939, -1, 0, 2715, 0, 0, 0, "copy" },
	{ "max_texture_mapping_6100_binary", 16947, -1, 0, 0, 660, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_texture_mapping_6100_binary", 16947, -1, 0, 10760, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_texture_mapping_6100_binary", 16999, -1, 0, 10758, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 16999, -1, 0, 2715, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 17086, -1, 0, 0, 1318, 0, 0, "shader" },
	{ "max_texture_mapping_6100_binary", 17086, -1, 0, 0, 659, 0, 0, "shader" },
	{ "max_texture_mapping_6100_binary", 17118, -1, 0, 10758, 0, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_texture_mapping_6100_binary", 17118, -1, 0, 2715, 0, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_texture_mapping_6100_binary", 17130, -1, 0, 0, 677, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "max_texture_mapping_6100_binary", 17130, -1, 0, 10862, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "max_texture_mapping_6100_binary", 17173, -1, 0, 10762, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_texture_mapping_6100_binary", 17190, -1, 0, 0, 1322, 0, 0, "shader->inputs.data" },
	{ "max_texture_mapping_6100_binary", 17190, -1, 0, 0, 661, 0, 0, "shader->inputs.data" },
	{ "max_texture_mapping_6100_binary", 17435, -1, 0, 10968, 0, 0, 0, "dst" },
	{ "max_texture_mapping_6100_binary", 17435, -1, 0, 2745, 0, 0, 0, "dst" },
	{ "max_texture_mapping_6100_binary", 17455, -1, 0, 10980, 0, 0, 0, "dst" },
	{ "max_texture_mapping_6100_binary", 17514, -1, 0, 2753, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_texture_mapping_6100_binary", 17525, -1, 0, 10979, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_texture_mapping_6100_binary", 18662, -1, 0, 10758, 0, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_texture_mapping_6100_binary", 18662, -1, 0, 2715, 0, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_texture_mapping_7700_binary", 16976, -1, 0, 2340, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_texture_mapping_7700_binary", 16976, -1, 0, 9444, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_transformed_skin_6100_binary", 12717, 63310, 98, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max_transformed_skin_6100_binary", 15169, -1, 0, 2030, 0, 0, 0, "modify_not_supported" },
	{ "maya_arnold_textures_6100_binary", 12189, -1, 0, 6386, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_arnold_textures_6100_binary", 12199, -1, 0, 1514, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 12199, -1, 0, 6243, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 12213, -1, 0, 1516, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 12213, -1, 0, 6250, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 12228, -1, 0, 0, 343, 0, 0, "bindings->prop_bindings.data" },
	{ "maya_arnold_textures_6100_binary", 12228, -1, 0, 0, 686, 0, 0, "bindings->prop_bindings.data" },
	{ "maya_arnold_textures_6100_binary", 12230, -1, 0, 6386, 0, 0, 0, "ufbxi_sort_shader_prop_bindings(uc, bindings->prop_bind..." },
	{ "maya_arnold_textures_6100_binary", 12542, -1, 0, 1349, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 12542, -1, 0, 5457, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 12544, -1, 0, 1514, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 12544, -1, 0, 6243, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 18450, -1, 0, 1754, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_arnold_textures_6100_binary", 18450, -1, 0, 6942, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_auto_clamp_7100_ascii", 8403, -1, 0, 3181, 0, 0, 0, "v" },
	{ "maya_auto_clamp_7100_ascii", 8403, -1, 0, 724, 0, 0, 0, "v" },
	{ "maya_auto_clamp_7100_ascii", 8613, -1, 0, 3200, 0, 0, 0, "v" },
	{ "maya_cache_sine_6100_binary", 12519, -1, 0, 1216, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_cache_sine_6100_binary", 12519, -1, 0, 4851, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_cache_sine_6100_binary", 12564, -1, 0, 1284, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_cache_sine_6100_binary", 12564, -1, 0, 5097, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_cache_sine_6100_binary", 18134, -1, 0, 1471, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_cache_sine_6100_binary", 18134, -1, 0, 5700, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_cache_sine_6100_binary", 18135, -1, 0, 1472, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->raw..." },
	{ "maya_cache_sine_6100_binary", 18135, -1, 0, 5704, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->raw..." },
	{ "maya_cache_sine_6100_binary", 18280, -1, 0, 1476, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 18280, -1, 0, 5716, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 20224, -1, 0, 0, 247, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_cache_sine_6100_binary", 20224, -1, 0, 0, 494, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_cache_sine_6100_binary", 20229, -1, 0, 0, 250, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_cache_sine_6100_binary", 20274, -1, 0, 0, 251, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_cache_sine_6100_binary", 20303, -1, 0, 1487, 0, 0, 0, "doc" },
	{ "maya_cache_sine_6100_binary", 20307, -1, 0, 0, 247, 0, 0, "xml_ok" },
	{ "maya_cache_sine_6100_binary", 20307, -1, 0, 0, 494, 0, 0, "xml_ok" },
	{ "maya_cache_sine_6100_binary", 20315, -1, 0, 0, 253, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_cache_sine_6100_binary", 20331, -1, 0, 0, 494, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 20331, -1, 0, 1487, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 20390, -1, 0, 0, 253, 0, 0, "ufbxi_cache_try_open_file(cc, filename, ((void *)0), &f..." },
	{ "maya_cache_sine_6100_binary", 20537, -1, 0, 1486, 0, 0, 0, "filename_data" },
	{ "maya_cache_sine_6100_binary", 20544, -1, 0, 0, 494, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, ((void *)0..." },
	{ "maya_cache_sine_6100_binary", 20544, -1, 0, 1487, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, ((void *)0..." },
	{ "maya_cache_sine_6100_binary", 20552, -1, 0, 0, 253, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_cache_sine_6100_binary", 20775, -1, 0, 1483, 0, 0, 0, "file" },
	{ "maya_cache_sine_6100_binary", 20775, -1, 0, 5736, 0, 0, 0, "file" },
	{ "maya_cache_sine_6100_binary", 20785, -1, 0, 1485, 0, 0, 0, "files" },
	{ "maya_cache_sine_6100_binary", 20793, -1, 0, 0, 494, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_cache_sine_6100_binary", 20793, -1, 0, 1486, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_cache_sine_6100_binary", 21238, -1, 0, 1483, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_cache_sine_6100_binary", 21238, -1, 0, 5736, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_cache_sine_6100_binary", 5963, -1, 0, 1488, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_cache_sine_6100_binary", 5998, -1, 0, 1488, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_cache_sine_6100_binary", 6152, -1, 0, 1488, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_cache_sine_6100_binary", 6216, -1, 0, 1487, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 6222, -1, 0, 1488, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, ((void *)0))" },
	{ "maya_character_6100_binary", 12276, -1, 0, 47240, 0, 0, 0, "character" },
	{ "maya_character_6100_binary", 12557, -1, 0, 47240, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_character_7500_binary", 12276, -1, 0, 6220, 0, 0, 0, "character" },
	{ "maya_character_7500_binary", 12557, -1, 0, 6220, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_color_sets_6100_binary", 11324, -1, 0, 0, 154, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 11324, -1, 0, 0, 77, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 11371, 9966, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 11376, 16081, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 11379, 15524, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cone_6100_binary", 11382, 15571, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease.data,..." },
	{ "maya_constraint_zoo_6100_binary", 12302, -1, 0, 12976, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 12302, -1, 0, 3507, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 12559, -1, 0, 12976, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 12559, -1, 0, 3507, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 16785, -1, 0, 14697, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 16785, -1, 0, 4000, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 18756, -1, 0, 14697, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 18756, -1, 0, 4000, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 18762, -1, 0, 0, 315, 0, 0, "constraint->targets.data" },
	{ "maya_constraint_zoo_6100_binary", 18762, -1, 0, 0, 630, 0, 0, "constraint->targets.data" },
	{ "maya_cube_6100_ascii", 3225, -1, 0, 1700, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_ascii", 7939, -1, 0, 0, 0, 0, 57, "ufbxi_report_progress(uc)" },
	{ "maya_cube_6100_ascii", 8079, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_cube_6100_ascii", 8079, -1, 0, 662, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_cube_6100_ascii", 8156, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 8156, -1, 0, 662, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 8175, -1, 0, 6, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 8175, -1, 0, 669, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 8204, 514, 0, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_cube_6100_ascii", 8211, 4541, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_cube_6100_ascii", 8270, 190, 0, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_cube_6100_ascii", 8290, 190, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 8327, -1, 0, 1743, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8327, -1, 0, 273, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8359, 4998, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 8399, -1, 0, 1700, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8399, -1, 0, 258, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8422, 4870, 46, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 8434, 514, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 8440, 174, 0, 0, 0, 0, 0, "depth == 0" },
	{ "maya_cube_6100_ascii", 8450, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_cube_6100_ascii", 8454, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_cube_6100_ascii", 8454, -1, 0, 664, 0, 0, 0, "name" },
	{ "maya_cube_6100_ascii", 8459, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_cube_6100_ascii", 8459, -1, 0, 665, 0, 0, 0, "node" },
	{ "maya_cube_6100_ascii", 8483, -1, 0, 1694, 0, 0, 0, "arr" },
	{ "maya_cube_6100_ascii", 8483, -1, 0, 255, 0, 0, 0, "arr" },
	{ "maya_cube_6100_ascii", 8500, -1, 0, 1696, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_cube_6100_ascii", 8500, -1, 0, 256, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_cube_6100_ascii", 8504, -1, 0, 1698, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_cube_6100_ascii", 8504, -1, 0, 257, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_cube_6100_ascii", 8533, 4870, 46, 0, 0, 0, 0, "ufbxi_ascii_read_float_array(uc, (char)arr_type, &num_r..." },
	{ "maya_cube_6100_ascii", 8535, 4998, 45, 0, 0, 0, 0, "ufbxi_ascii_read_int_array(uc, (char)arr_type, &num_rea..." },
	{ "maya_cube_6100_ascii", 8580, -1, 0, 0, 3, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_cube_6100_ascii", 8580, -1, 0, 690, 0, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_cube_6100_ascii", 8610, -1, 0, 2398, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8610, -1, 0, 492, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8611, -1, 0, 1772, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8614, -1, 0, 1948, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8642, -1, 0, 1735, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8721, -1, 0, 1795, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_ascii", 8721, -1, 0, 285, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_ascii", 8737, -1, 0, 675, 0, 0, 0, "node->vals" },
	{ "maya_cube_6100_ascii", 8737, -1, 0, 8, 0, 0, 0, "node->vals" },
	{ "maya_cube_6100_ascii", 8747, 174, 11, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_cube_6100_ascii", 8754, -1, 0, 20, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_ascii", 8754, -1, 0, 714, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_ascii", 9285, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_cube_6100_ascii", 9285, -1, 0, 662, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_cube_6100_ascii", 9302, 100, 33, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_cube_6100_binary", 10005, -1, 0, 0, 96, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_cube_6100_binary", 10005, -1, 0, 2332, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_cube_6100_binary", 10017, 35, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_cube_6100_binary", 10165, 954, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object)" },
	{ "maya_cube_6100_binary", 10172, -1, 0, 1062, 0, 0, 0, "tmpl" },
	{ "maya_cube_6100_binary", 10172, -1, 0, 72, 0, 0, 0, "tmpl" },
	{ "maya_cube_6100_binary", 10173, 1022, 0, 0, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_cube_6100_binary", 10200, -1, 0, 0, 25, 0, 0, "uc->templates" },
	{ "maya_cube_6100_binary", 10200, -1, 0, 0, 50, 0, 0, "uc->templates" },
	{ "maya_cube_6100_binary", 10252, -1, 0, 2079, 0, 0, 0, "ptr" },
	{ "maya_cube_6100_binary", 10252, -1, 0, 394, 0, 0, 0, "ptr" },
	{ "maya_cube_6100_binary", 10287, -1, 0, 2074, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_cube_6100_binary", 10288, -1, 0, 0, 57, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_cube_6100_binary", 10288, -1, 0, 2075, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_cube_6100_binary", 10301, -1, 0, 46, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 10301, -1, 0, 986, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 10329, -1, 0, 2084, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 10329, -1, 0, 395, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 10344, -1, 0, 42, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_6100_binary", 10344, -1, 0, 978, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_6100_binary", 10345, -1, 0, 43, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_6100_binary", 10345, -1, 0, 980, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_6100_binary", 10349, -1, 0, 44, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 10349, -1, 0, 982, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 10357, -1, 0, 45, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 10357, -1, 0, 984, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 10359, -1, 0, 46, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 10359, -1, 0, 986, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 10371, -1, 0, 2754, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_6100_binary", 10371, -1, 0, 581, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_6100_binary", 10372, -1, 0, 2756, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_6100_binary", 10372, -1, 0, 582, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_6100_binary", 10376, -1, 0, 2758, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 10376, -1, 0, 583, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 10386, -1, 0, 2760, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 10386, -1, 0, 584, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 10391, -1, 0, 2762, 0, 0, 0, "ufbxi_insert_fbx_id(uc, fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 10402, -1, 0, 2113, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 10402, -1, 0, 407, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 10503, -1, 0, 2115, 0, 0, 0, "elem_node" },
	{ "maya_cube_6100_binary", 10503, -1, 0, 408, 0, 0, 0, "elem_node" },
	{ "maya_cube_6100_binary", 10504, -1, 0, 2123, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 10628, 7295, 255, 0, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 10629, 7448, 71, 0, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_cube_6100_binary", 10645, 7345, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MappingInformationType, \"C..." },
	{ "maya_cube_6100_binary", 10696, 9992, 14, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_cube_6100_binary", 10731, 7377, 14, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_cube_6100_binary", 10741, 10572, 255, 0, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 10783, -1, 0, 2111, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 10790, -1, 0, 2112, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 10960, -1, 0, 0, 124, 0, 0, "mesh->faces.data" },
	{ "maya_cube_6100_binary", 10960, -1, 0, 0, 62, 0, 0, "mesh->faces.data" },
	{ "maya_cube_6100_binary", 10986, 6763, 0, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_cube_6100_binary", 10998, -1, 0, 0, 126, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_6100_binary", 10998, -1, 0, 0, 63, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_6100_binary", 11212, -1, 0, 2100, 0, 0, 0, "mesh" },
	{ "maya_cube_6100_binary", 11212, -1, 0, 403, 0, 0, 0, "mesh" },
	{ "maya_cube_6100_binary", 11233, 6763, 23, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_cube_6100_binary", 11243, -1, 0, 0, 438, 0, 0, "index_data" },
	{ "maya_cube_6100_binary", 11243, -1, 0, 0, 876, 0, 0, "index_data" },
	{ "maya_cube_6100_binary", 11261, 7000, 23, 0, 0, 0, 0, "Non-negated last index" },
	{ "maya_cube_6100_binary", 11270, -1, 0, 0, 122, 0, 0, "edges" },
	{ "maya_cube_6100_binary", 11270, -1, 0, 0, 61, 0, 0, "edges" },
	{ "maya_cube_6100_binary", 11280, 7000, 0, 0, 0, 0, 0, "Edge index out of bounds" },
	{ "maya_cube_6100_binary", 11303, 6763, 0, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "maya_cube_6100_binary", 11318, -1, 0, 2107, 0, 0, 0, "bitangents" },
	{ "maya_cube_6100_binary", 11318, -1, 0, 405, 0, 0, 0, "bitangents" },
	{ "maya_cube_6100_binary", 11319, -1, 0, 2109, 0, 0, 0, "tangents" },
	{ "maya_cube_6100_binary", 11319, -1, 0, 406, 0, 0, 0, "tangents" },
	{ "maya_cube_6100_binary", 11323, -1, 0, 0, 128, 0, 0, "mesh->uv_sets.data" },
	{ "maya_cube_6100_binary", 11323, -1, 0, 0, 64, 0, 0, "mesh->uv_sets.data" },
	{ "maya_cube_6100_binary", 11333, 7295, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 11339, 8164, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 11347, 9038, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 11359, 9906, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 11386, 10525, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cube_6100_binary", 11389, 10572, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing.da..." },
	{ "maya_cube_6100_binary", 11404, 10799, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cube_6100_binary", 11409, 10846, 255, 0, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_cube_6100_binary", 11439, 7283, 0, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "maya_cube_6100_binary", 11483, 10070, 0, 0, 0, 0, 0, "mesh->uv_sets.count == num_uv" },
	{ "maya_cube_6100_binary", 11485, 8321, 0, 0, 0, 0, 0, "num_bitangents_read == num_bitangents" },
	{ "maya_cube_6100_binary", 11486, 9195, 0, 0, 0, 0, 0, "num_tangents_read == num_tangents" },
	{ "maya_cube_6100_binary", 11548, -1, 0, 2111, 0, 0, 0, "ufbxi_sort_uv_sets(uc, mesh->uv_sets.data, mesh->uv_set..." },
	{ "maya_cube_6100_binary", 11549, -1, 0, 2112, 0, 0, 0, "ufbxi_sort_color_sets(uc, mesh->color_sets.data, mesh->..." },
	{ "maya_cube_6100_binary", 12056, -1, 0, 2541, 0, 0, 0, "material" },
	{ "maya_cube_6100_binary", 12056, -1, 0, 517, 0, 0, 0, "material" },
	{ "maya_cube_6100_binary", 12329, -1, 0, 2079, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "maya_cube_6100_binary", 12329, -1, 0, 394, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "maya_cube_6100_binary", 12335, -1, 0, 0, 59, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_cube_6100_binary", 12335, -1, 0, 2081, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_cube_6100_binary", 12348, -1, 0, 2084, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_cube_6100_binary", 12348, -1, 0, 395, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_cube_6100_binary", 12355, -1, 0, 2086, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_cube_6100_binary", 12355, -1, 0, 396, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_cube_6100_binary", 12365, -1, 0, 0, 120, 0, 0, "attrib_info.props.props.data" },
	{ "maya_cube_6100_binary", 12365, -1, 0, 0, 60, 0, 0, "attrib_info.props.props.data" },
	{ "maya_cube_6100_binary", 12370, 6763, 23, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_cube_6100_binary", 12410, -1, 0, 2113, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_cube_6100_binary", 12410, -1, 0, 407, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_cube_6100_binary", 12416, 15140, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_cube_6100_binary", 12425, 1331, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_cube_6100_binary", 12431, 15140, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_cube_6100_binary", 12458, -1, 0, 0, 57, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_cube_6100_binary", 12458, -1, 0, 2074, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_cube_6100_binary", 12461, 1442, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_cube_6100_binary", 12466, 6763, 23, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, type_st..." },
	{ "maya_cube_6100_binary", 12468, -1, 0, 2115, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_cube_6100_binary", 12468, -1, 0, 408, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_cube_6100_binary", 12524, -1, 0, 2541, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_cube_6100_binary", 12524, -1, 0, 517, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_cube_6100_binary", 12562, -1, 0, 0, 96, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cube_6100_binary", 12562, -1, 0, 2332, 0, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cube_6100_binary", 12580, 16292, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_cube_6100_binary", 12637, -1, 0, 2705, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 12637, -1, 0, 565, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 12952, -1, 0, 2754, 0, 0, 0, "stack" },
	{ "maya_cube_6100_binary", 12952, -1, 0, 581, 0, 0, 0, "stack" },
	{ "maya_cube_6100_binary", 12953, 16506, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"S\", &stack->name)" },
	{ "maya_cube_6100_binary", 12956, -1, 0, 2763, 0, 0, 0, "layer" },
	{ "maya_cube_6100_binary", 12956, -1, 0, 585, 0, 0, 0, "layer" },
	{ "maya_cube_6100_binary", 12958, -1, 0, 2770, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_cube_6100_binary", 12958, -1, 0, 587, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_cube_6100_binary", 12963, 16533, 65, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_ReferenceTime, \"LL\", &beg..." },
	{ "maya_cube_6100_binary", 12983, 16459, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_cube_6100_binary", 12987, 16506, 0, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_cube_6100_binary", 13009, 0, 76, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_cube_6100_binary", 13010, 35, 1, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "maya_cube_6100_binary", 13023, -1, 0, 41, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_cube_6100_binary", 13023, -1, 0, 779, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_cube_6100_binary", 13035, -1, 0, 976, 0, 0, 0, "root_name" },
	{ "maya_cube_6100_binary", 13044, -1, 0, 42, 0, 0, 0, "root" },
	{ "maya_cube_6100_binary", 13044, -1, 0, 978, 0, 0, 0, "root" },
	{ "maya_cube_6100_binary", 13046, -1, 0, 47, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 13046, -1, 0, 988, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 13050, 59, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_cube_6100_binary", 13051, 954, 1, 0, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_cube_6100_binary", 13054, 954, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_cube_6100_binary", 13058, 0, 0, 0, 0, 0, 0, "uc->top_node" },
	{ "maya_cube_6100_binary", 13060, 1331, 1, 0, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_cube_6100_binary", 13063, 16288, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_cube_6100_binary", 13064, 16292, 1, 0, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_cube_6100_binary", 13069, 16309, 64, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_cube_6100_binary", 13070, 16459, 1, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_cube_6100_binary", 13074, 16470, 65, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_cube_6100_binary", 13638, -1, 0, 0, 143, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 13638, -1, 0, 2900, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 13639, -1, 0, 2902, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &uc->sce..." },
	{ "maya_cube_6100_binary", 13647, -1, 0, 0, 144, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 13647, -1, 0, 2903, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 13648, -1, 0, 2905, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &uc->sce..." },
	{ "maya_cube_6100_binary", 15156, -1, 0, 2906, 0, 0, 0, "elements" },
	{ "maya_cube_6100_binary", 15160, -1, 0, 2908, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 15163, -1, 0, 2910, 0, 0, 0, "pre_connections" },
	{ "maya_cube_6100_binary", 15166, -1, 0, 2912, 0, 0, 0, "instance_counts" },
	{ "maya_cube_6100_binary", 15169, -1, 0, 2914, 0, 0, 0, "modify_not_supported" },
	{ "maya_cube_6100_binary", 15172, -1, 0, 2915, 0, 0, 0, "fbx_ids" },
	{ "maya_cube_6100_binary", 15287, -1, 0, 2943, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 15311, -1, 0, 2929, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 15347, -1, 0, 2921, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 15367, -1, 0, 2919, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 15367, -1, 0, 631, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 15371, -1, 0, 0, 147, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_cube_6100_binary", 15371, -1, 0, 0, 294, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_cube_6100_binary", 15428, -1, 0, 0, 148, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_cube_6100_binary", 15428, -1, 0, 0, 296, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_cube_6100_binary", 15430, -1, 0, 2921, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "maya_cube_6100_binary", 15431, -1, 0, 2922, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_dst.da..." },
	{ "maya_cube_6100_binary", 15570, -1, 0, 2923, 0, 0, 0, "node_ids" },
	{ "maya_cube_6100_binary", 15570, -1, 0, 632, 0, 0, 0, "node_ids" },
	{ "maya_cube_6100_binary", 15573, -1, 0, 2925, 0, 0, 0, "node_ptrs" },
	{ "maya_cube_6100_binary", 15573, -1, 0, 633, 0, 0, 0, "node_ptrs" },
	{ "maya_cube_6100_binary", 15584, -1, 0, 2927, 0, 0, 0, "node_offsets" },
	{ "maya_cube_6100_binary", 15584, -1, 0, 634, 0, 0, 0, "node_offsets" },
	{ "maya_cube_6100_binary", 15625, -1, 0, 2929, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "maya_cube_6100_binary", 15629, -1, 0, 2930, 0, 0, 0, "p_offset" },
	{ "maya_cube_6100_binary", 15629, -1, 0, 635, 0, 0, 0, "p_offset" },
	{ "maya_cube_6100_binary", 15703, -1, 0, 2944, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 15703, -1, 0, 641, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 15712, -1, 0, 0, 155, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 15712, -1, 0, 0, 310, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 15725, -1, 0, 2946, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 15725, -1, 0, 642, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 15734, -1, 0, 0, 156, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 15734, -1, 0, 0, 312, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 15800, -1, 0, 2948, 0, 0, 0, "((ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_cube_6100_binary", 15800, -1, 0, 643, 0, 0, 0, "((ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_cube_6100_binary", 15810, -1, 0, 0, 159, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 15810, -1, 0, 0, 318, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 15919, -1, 0, 2954, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 15933, -1, 0, 2955, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 17838, -1, 0, 0, 145, 0, 0, "uc->scene.elements.data" },
	{ "maya_cube_6100_binary", 17838, -1, 0, 0, 290, 0, 0, "uc->scene.elements.data" },
	{ "maya_cube_6100_binary", 17843, -1, 0, 0, 146, 0, 0, "element_data" },
	{ "maya_cube_6100_binary", 17843, -1, 0, 0, 292, 0, 0, "element_data" },
	{ "maya_cube_6100_binary", 17847, -1, 0, 2917, 0, 0, 0, "element_offsets" },
	{ "maya_cube_6100_binary", 17847, -1, 0, 630, 0, 0, 0, "element_offsets" },
	{ "maya_cube_6100_binary", 17858, -1, 0, 2919, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_cube_6100_binary", 17858, -1, 0, 631, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_cube_6100_binary", 17860, -1, 0, 2923, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_cube_6100_binary", 17860, -1, 0, 632, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_cube_6100_binary", 17866, -1, 0, 2933, 0, 0, 0, "typed_offsets" },
	{ "maya_cube_6100_binary", 17866, -1, 0, 636, 0, 0, 0, "typed_offsets" },
	{ "maya_cube_6100_binary", 17871, -1, 0, 0, 149, 0, 0, "typed_elems->data" },
	{ "maya_cube_6100_binary", 17871, -1, 0, 0, 298, 0, 0, "typed_elems->data" },
	{ "maya_cube_6100_binary", 17883, -1, 0, 0, 154, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_cube_6100_binary", 17883, -1, 0, 0, 308, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_cube_6100_binary", 17896, -1, 0, 2943, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "maya_cube_6100_binary", 17946, -1, 0, 2944, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_cube_6100_binary", 17946, -1, 0, 641, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_cube_6100_binary", 17988, -1, 0, 2946, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_cube_6100_binary", 17988, -1, 0, 642, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_cube_6100_binary", 18166, -1, 0, 0, 157, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_cube_6100_binary", 18166, -1, 0, 0, 314, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_cube_6100_binary", 18218, -1, 0, 2948, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_cube_6100_binary", 18218, -1, 0, 643, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_cube_6100_binary", 18320, -1, 0, 2950, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_cube_6100_binary", 18320, -1, 0, 644, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_cube_6100_binary", 18324, -1, 0, 0, 161, 0, 0, "stack->anim.layers.data" },
	{ "maya_cube_6100_binary", 18324, -1, 0, 0, 322, 0, 0, "stack->anim.layers.data" },
	{ "maya_cube_6100_binary", 18338, -1, 0, 0, 162, 0, 0, "layer_desc" },
	{ "maya_cube_6100_binary", 18338, -1, 0, 0, 324, 0, 0, "layer_desc" },
	{ "maya_cube_6100_binary", 18410, -1, 0, 2952, 0, 0, 0, "aprop" },
	{ "maya_cube_6100_binary", 18410, -1, 0, 645, 0, 0, 0, "aprop" },
	{ "maya_cube_6100_binary", 18414, -1, 0, 0, 163, 0, 0, "layer->anim_props.data" },
	{ "maya_cube_6100_binary", 18414, -1, 0, 0, 326, 0, 0, "layer->anim_props.data" },
	{ "maya_cube_6100_binary", 18416, -1, 0, 2954, 0, 0, 0, "ufbxi_sort_anim_props(uc, layer->anim_props.data, layer..." },
	{ "maya_cube_6100_binary", 18693, -1, 0, 2955, 0, 0, 0, "ufbxi_sort_material_textures(uc, material->textures.dat..." },
	{ "maya_cube_6100_binary", 18773, -1, 0, 0, 164, 0, 0, "descs" },
	{ "maya_cube_6100_binary", 18773, -1, 0, 0, 328, 0, 0, "descs" },
	{ "maya_cube_6100_binary", 21182, -1, 0, 1, 0, 0, 0, "ufbxi_load_strings(uc)" },
	{ "maya_cube_6100_binary", 21183, -1, 0, 1, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_cube_6100_binary", 21183, -1, 0, 575, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_cube_6100_binary", 21189, 0, 76, 0, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_cube_6100_binary", 21193, 0, 76, 0, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_cube_6100_binary", 21196, -1, 0, 0, 143, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_6100_binary", 21196, -1, 0, 2900, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_6100_binary", 21213, -1, 0, 2906, 0, 0, 0, "ufbxi_pre_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 21218, -1, 0, 2917, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 21218, -1, 0, 630, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 21260, -1, 0, 0, 165, 0, 0, "imp" },
	{ "maya_cube_6100_binary", 21260, -1, 0, 0, 330, 0, 0, "imp" },
	{ "maya_cube_6100_binary", 2812, 6765, 255, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_cube_6100_binary", 2817, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 2862, -1, 0, 50, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 2862, -1, 0, 999, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 2918, -1, 0, 665, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3173, -1, 0, 662, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3645, -1, 0, 1, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 3703, -1, 0, 2, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 4234, -1, 0, 694, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "maya_cube_6100_binary", 4259, -1, 0, 695, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 4262, -1, 0, 0, 4, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 4262, -1, 0, 0, 8, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 4276, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "maya_cube_6100_binary", 4302, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 4306, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 4327, -1, 0, 0, 57, 0, 0, "str" },
	{ "maya_cube_6100_binary", 4327, -1, 0, 671, 0, 0, 0, "str" },
	{ "maya_cube_6100_binary", 4345, -1, 0, 2902, 0, 0, 0, "p_blob->data" },
	{ "maya_cube_6100_binary", 5464, -1, 0, 0, 0, 0, 1, "result != UFBX_PROGRESS_CANCEL" },
	{ "maya_cube_6100_binary", 5483, -1, 0, 0, 0, 1, 0, "!uc->eof" },
	{ "maya_cube_6100_binary", 5485, 36, 255, 0, 0, 0, 0, "uc->read_fn" },
	{ "maya_cube_6100_binary", 5569, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_cube_6100_binary", 5646, 36, 255, 0, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_cube_6100_binary", 7413, -1, 0, 0, 0, 7040, 0, "val" },
	{ "maya_cube_6100_binary", 7416, -1, 0, 0, 0, 6793, 0, "val" },
	{ "maya_cube_6100_binary", 7453, 10670, 13, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 7454, 7000, 25, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 7457, 6763, 25, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 7479, 6765, 255, 0, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 7501, -1, 0, 0, 0, 27, 0, "header" },
	{ "maya_cube_6100_binary", 7522, 24, 255, 0, 0, 0, 0, "num_values64 <= 0xffffffffui32" },
	{ "maya_cube_6100_binary", 7540, -1, 0, 3, 0, 0, 0, "node" },
	{ "maya_cube_6100_binary", 7540, -1, 0, 662, 0, 0, 0, "node" },
	{ "maya_cube_6100_binary", 7544, -1, 0, 0, 0, 40, 0, "name" },
	{ "maya_cube_6100_binary", 7546, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_cube_6100_binary", 7546, -1, 0, 664, 0, 0, 0, "name" },
	{ "maya_cube_6100_binary", 7562, -1, 0, 1722, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 7562, -1, 0, 261, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 7571, -1, 0, 0, 0, 6780, 0, "data" },
	{ "maya_cube_6100_binary", 7721, 6765, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_binary", 7722, 6763, 25, 0, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_cube_6100_binary", 7738, -1, 0, 6, 0, 0, 0, "vals" },
	{ "maya_cube_6100_binary", 7738, -1, 0, 671, 0, 0, 0, "vals" },
	{ "maya_cube_6100_binary", 7746, -1, 0, 0, 0, 87, 0, "data" },
	{ "maya_cube_6100_binary", 7799, 213, 255, 0, 0, 0, 0, "str" },
	{ "maya_cube_6100_binary", 7809, -1, 0, 0, 4, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_cube_6100_binary", 7809, -1, 0, 694, 0, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_cube_6100_binary", 7824, 164, 0, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_cube_6100_binary", 7829, 22, 1, 0, 0, 0, 0, "Bad value type" },
	{ "maya_cube_6100_binary", 7840, 66, 4, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_cube_6100_binary", 7842, 36, 255, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_cube_6100_binary", 7854, 58, 93, 0, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_cube_6100_binary", 7859, 70, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_cube_6100_binary", 7868, -1, 0, 20, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 7868, -1, 0, 716, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 8448, 0, 76, 0, 0, 0, 0, "Expected a 'Name:' token" },
	{ "maya_cube_6100_binary", 8791, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 8791, -1, 0, 0, 2, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 8792, -1, 0, 5, 0, 0, 0, "((ufbx_dom_node**)ufbxi_push_size_copy((&uc->tmp_dom_no..." },
	{ "maya_cube_6100_binary", 8792, -1, 0, 667, 0, 0, 0, "((ufbx_dom_node**)ufbxi_push_size_copy((&uc->tmp_dom_no..." },
	{ "maya_cube_6100_binary", 8807, -1, 0, 6, 0, 0, 0, "result" },
	{ "maya_cube_6100_binary", 8807, -1, 0, 669, 0, 0, 0, "result" },
	{ "maya_cube_6100_binary", 8813, -1, 0, 671, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst->nam..." },
	{ "maya_cube_6100_binary", 8818, -1, 0, 0, 303, 0, 0, "val" },
	{ "maya_cube_6100_binary", 8818, -1, 0, 0, 606, 0, 0, "val" },
	{ "maya_cube_6100_binary", 8846, -1, 0, 681, 0, 0, 0, "val" },
	{ "maya_cube_6100_binary", 8846, -1, 0, 9, 0, 0, 0, "val" },
	{ "maya_cube_6100_binary", 8863, -1, 0, 0, 3, 0, 0, "dst->values.data" },
	{ "maya_cube_6100_binary", 8863, -1, 0, 0, 6, 0, 0, "dst->values.data" },
	{ "maya_cube_6100_binary", 8868, -1, 0, 28, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 8868, -1, 0, 740, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 8873, -1, 0, 0, 25, 0, 0, "dst->children.data" },
	{ "maya_cube_6100_binary", 8873, -1, 0, 0, 50, 0, 0, "dst->children.data" },
	{ "maya_cube_6100_binary", 8883, -1, 0, 0, 116, 0, 0, "children" },
	{ "maya_cube_6100_binary", 8883, -1, 0, 0, 58, 0, 0, "children" },
	{ "maya_cube_6100_binary", 8890, -1, 0, 5, 0, 0, 0, "ufbxi_retain_dom_node(uc, node, &uc->dom_parse_toplevel..." },
	{ "maya_cube_6100_binary", 8890, -1, 0, 667, 0, 0, 0, "ufbxi_retain_dom_node(uc, node, &uc->dom_parse_toplevel..." },
	{ "maya_cube_6100_binary", 8897, -1, 0, 0, 1462, 0, 0, "nodes" },
	{ "maya_cube_6100_binary", 8897, -1, 0, 0, 731, 0, 0, "nodes" },
	{ "maya_cube_6100_binary", 8900, -1, 0, 0, 1464, 0, 0, "dom_root" },
	{ "maya_cube_6100_binary", 8900, -1, 0, 0, 732, 0, 0, "dom_root" },
	{ "maya_cube_6100_binary", 8915, -1, 0, 678, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 8915, -1, 0, 9, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 9253, -1, 0, 0, 0, 1, 0, "header" },
	{ "maya_cube_6100_binary", 9292, 0, 76, 0, 0, 0, 0, "uc->version > 0" },
	{ "maya_cube_6100_binary", 9304, 35, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_cube_6100_binary", 9331, 0, 76, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_cube_6100_binary", 9333, 22, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_cube_6100_binary", 9342, -1, 0, 0, 1460, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "maya_cube_6100_binary", 9342, -1, 0, 0, 730, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "maya_cube_6100_binary", 9352, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 9352, -1, 0, 665, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 9356, -1, 0, 5, 0, 0, 0, "ufbxi_retain_toplevel(uc, node)" },
	{ "maya_cube_6100_binary", 9356, -1, 0, 667, 0, 0, 0, "ufbxi_retain_toplevel(uc, node)" },
	{ "maya_cube_6100_binary", 9371, 39, 19, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_cube_6100_binary", 9379, -1, 0, 1028, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 9379, -1, 0, 60, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 9383, -1, 0, 1145, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &node->children[i])" },
	{ "maya_cube_6100_binary", 9383, -1, 0, 93, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &node->children[i])" },
	{ "maya_cube_6100_binary", 9402, 35, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_pars..." },
	{ "maya_cube_6100_binary", 9410, -1, 0, 678, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &uc->top_child)" },
	{ "maya_cube_6100_binary", 9410, -1, 0, 9, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &uc->top_child)" },
	{ "maya_cube_6100_binary", 9473, -1, 0, 1, 0, 0, 0, "ufbxi_push_string_imp(&uc->string_pool, str->data, str-..." },
	{ "maya_cube_6100_binary", 9758, -1, 0, 41, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_cube_6100_binary", 9758, -1, 0, 779, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_cube_6100_binary", 9762, -1, 0, 781, 0, 0, 0, "pooled" },
	{ "maya_cube_6100_binary", 9765, -1, 0, 783, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 9784, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_cube_6100_binary", 9784, -1, 0, 575, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_cube_6100_binary", 9787, -1, 0, 577, 0, 0, 0, "pooled" },
	{ "maya_cube_6100_binary", 9790, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 9790, -1, 0, 579, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 9874, 1442, 0, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_cube_6100_binary", 9949, -1, 0, 2077, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 9949, -1, 0, 393, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 9991, -1, 0, 0, 116, 0, 0, "props->props.data" },
	{ "maya_cube_6100_binary", 9991, -1, 0, 0, 58, 0, 0, "props->props.data" },
	{ "maya_cube_6100_binary", 9994, 1442, 0, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_cube_6100_binary", 9997, -1, 0, 2077, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_cube_6100_binary", 9997, -1, 0, 393, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_cube_7100_ascii", 8682, 8925, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'I')" },
	{ "maya_cube_7100_ascii", 8685, 8929, 11, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_cube_7100_ascii", 8710, 8935, 33, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_cube_7100_binary", 10034, 797, 0, 0, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_cube_7100_binary", 10146, 3549, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_cube_7100_binary", 10179, 4105, 0, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_cube_7100_binary", 10191, -1, 0, 0, 58, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_cube_7100_binary", 10191, -1, 0, 1319, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_cube_7100_binary", 10194, 4176, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_cube_7100_binary", 10517, -1, 0, 2933, 0, 0, 0, "elem" },
	{ "maya_cube_7100_binary", 10517, -1, 0, 650, 0, 0, 0, "elem" },
	{ "maya_cube_7100_binary", 12444, 12333, 255, 0, 0, 0, 0, "(info.fbx_id & (0x8000000000000000ULL)) == 0" },
	{ "maya_cube_7100_binary", 12493, 12362, 0, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_cube_7100_binary", 12532, -1, 0, 2933, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "maya_cube_7100_binary", 12532, -1, 0, 650, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "maya_cube_7100_binary", 12534, -1, 0, 2951, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_cube_7100_binary", 12534, -1, 0, 656, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_cube_7100_binary", 13028, 59, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_cube_7100_binary", 13029, 3549, 1, 0, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_cube_7100_binary", 13076, 2241, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "maya_cube_7100_binary", 13081, 18890, 74, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ((void *)0))" },
	{ "maya_cube_7100_binary", 2861, 16067, 1, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_cube_7100_binary", 5551, -1, 0, 0, 0, 0, 1434, "ufbxi_report_progress(uc)" },
	{ "maya_cube_7100_binary", 5674, -1, 0, 0, 0, 12392, 0, "uc->read_fn" },
	{ "maya_cube_7100_binary", 5682, -1, 0, 0, 0, 0, 1434, "ufbxi_resume_progress(uc)" },
	{ "maya_cube_7100_binary", 7606, 12382, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_cube_7100_binary", 7613, 16067, 1, 0, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_7100_binary", 7626, 12379, 99, 0, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_cube_7100_binary", 7642, -1, 0, 0, 0, 12392, 0, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_cube_7100_binary", 7699, 12384, 1, 0, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_cube_7100_binary", 7702, 12384, 255, 0, 0, 0, 0, "Bad array encoding" },
	{ "maya_cube_7100_binary", 9877, 6091, 0, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_cube_7400_ascii", 8102, -1, 0, 0, 0, 9568, 0, "c != '\\0'" },
	{ "maya_cube_7400_ascii", 8693, -1, 0, 0, 0, 9568, 0, "ufbxi_ascii_skip_until(uc, '}')" },
	{ "maya_cube_7500_binary", 13567, 24, 0, 0, 0, 0, 0, "ufbxi_parse_legacy_toplevel(uc)" },
	{ "maya_cube_7500_binary", 21191, 24, 0, 0, 0, 0, 0, "ufbxi_read_legacy_root(uc)" },
	{ "maya_cube_7500_binary", 9435, 24, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_cube_big_endian_6100_binary", 7204, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 7204, -1, 0, 662, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 7514, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 7514, -1, 0, 664, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 9267, -1, 0, 3, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 9267, -1, 0, 662, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_7100_binary", 7273, -1, 0, 2332, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 7273, -1, 0, 453, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 7707, -1, 0, 2332, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7100_binary", 7707, -1, 0, 453, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7500_binary", 7505, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_7500_binary", 7505, -1, 0, 664, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_obj", 13856, -1, 0, 0, 12, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_big_endian_obj", 13856, -1, 0, 0, 24, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_big_endian_obj", 13873, -1, 0, 0, 13, 0, 0, "uv_set" },
	{ "maya_cube_big_endian_obj", 13873, -1, 0, 0, 26, 0, 0, "uv_set" },
	{ "maya_cube_big_endian_obj", 13937, -1, 0, 60, 0, 0, 0, "mesh" },
	{ "maya_cube_big_endian_obj", 13937, -1, 0, 966, 0, 0, 0, "mesh" },
	{ "maya_cube_big_endian_obj", 13955, -1, 0, 61, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "maya_cube_big_endian_obj", 13955, -1, 0, 968, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "maya_cube_big_endian_obj", 13959, -1, 0, 983, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 13966, -1, 0, 66, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "maya_cube_big_endian_obj", 13966, -1, 0, 984, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "maya_cube_big_endian_obj", 13967, -1, 0, 67, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "maya_cube_big_endian_obj", 13967, -1, 0, 986, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "maya_cube_big_endian_obj", 13981, -1, 0, 0, 2, 0, 0, "groups" },
	{ "maya_cube_big_endian_obj", 13981, -1, 0, 0, 4, 0, 0, "groups" },
	{ "maya_cube_big_endian_obj", 14021, -1, 0, 3, 0, 0, 0, "root" },
	{ "maya_cube_big_endian_obj", 14021, -1, 0, 662, 0, 0, 0, "root" },
	{ "maya_cube_big_endian_obj", 14023, -1, 0, 672, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 14023, -1, 0, 8, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 14098, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_cube_big_endian_obj", 14104, -1, 0, 1055, 0, 0, 0, "new_data" },
	{ "maya_cube_big_endian_obj", 14104, -1, 0, 81, 0, 0, 0, "new_data" },
	{ "maya_cube_big_endian_obj", 14164, -1, 0, 674, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "maya_cube_big_endian_obj", 14164, -1, 0, 9, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "maya_cube_big_endian_obj", 14200, -1, 0, 1055, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "maya_cube_big_endian_obj", 14200, -1, 0, 81, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "maya_cube_big_endian_obj", 14201, -1, 0, 674, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "maya_cube_big_endian_obj", 14201, -1, 0, 9, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "maya_cube_big_endian_obj", 14219, 92, 33, 0, 0, 0, 0, "offset + read_values <= uc->obj.num_tokens" },
	{ "maya_cube_big_endian_obj", 14222, -1, 0, 14, 0, 0, 0, "vals" },
	{ "maya_cube_big_endian_obj", 14222, -1, 0, 697, 0, 0, 0, "vals" },
	{ "maya_cube_big_endian_obj", 14227, 83, 46, 0, 0, 0, 0, "end == str.data + str.length" },
	{ "maya_cube_big_endian_obj", 14276, -1, 0, 1000, 0, 0, 0, "dst" },
	{ "maya_cube_big_endian_obj", 14276, -1, 0, 74, 0, 0, 0, "dst" },
	{ "maya_cube_big_endian_obj", 14318, -1, 0, 60, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 14318, -1, 0, 966, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 14355, -1, 0, 68, 0, 0, 0, "entry" },
	{ "maya_cube_big_endian_obj", 14355, -1, 0, 988, 0, 0, 0, "entry" },
	{ "maya_cube_big_endian_obj", 14368, -1, 0, 69, 0, 0, 0, "group" },
	{ "maya_cube_big_endian_obj", 14368, -1, 0, 990, 0, 0, 0, "group" },
	{ "maya_cube_big_endian_obj", 14387, -1, 0, 70, 0, 0, 0, "face" },
	{ "maya_cube_big_endian_obj", 14387, -1, 0, 992, 0, 0, 0, "face" },
	{ "maya_cube_big_endian_obj", 14396, -1, 0, 71, 0, 0, 0, "p_face_mat" },
	{ "maya_cube_big_endian_obj", 14396, -1, 0, 994, 0, 0, 0, "p_face_mat" },
	{ "maya_cube_big_endian_obj", 14401, -1, 0, 72, 0, 0, 0, "p_face_smooth" },
	{ "maya_cube_big_endian_obj", 14401, -1, 0, 996, 0, 0, 0, "p_face_smooth" },
	{ "maya_cube_big_endian_obj", 14407, -1, 0, 73, 0, 0, 0, "p_face_group" },
	{ "maya_cube_big_endian_obj", 14407, -1, 0, 998, 0, 0, 0, "p_face_group" },
	{ "maya_cube_big_endian_obj", 14414, -1, 0, 1000, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "maya_cube_big_endian_obj", 14414, -1, 0, 74, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "maya_cube_big_endian_obj", 14529, -1, 0, 0, 3, 0, 0, "data" },
	{ "maya_cube_big_endian_obj", 14529, -1, 0, 0, 6, 0, 0, "data" },
	{ "maya_cube_big_endian_obj", 14555, 71, 102, 0, 0, 0, 0, "num_indices == 0 || !required" },
	{ "maya_cube_big_endian_obj", 14567, -1, 0, 0, 18, 0, 0, "dst_indices" },
	{ "maya_cube_big_endian_obj", 14567, -1, 0, 0, 9, 0, 0, "dst_indices" },
	{ "maya_cube_big_endian_obj", 14612, -1, 0, 1057, 0, 0, 0, "meshes" },
	{ "maya_cube_big_endian_obj", 14612, -1, 0, 82, 0, 0, 0, "meshes" },
	{ "maya_cube_big_endian_obj", 14645, -1, 0, 1059, 0, 0, 0, "tmp_indices" },
	{ "maya_cube_big_endian_obj", 14645, -1, 0, 83, 0, 0, 0, "tmp_indices" },
	{ "maya_cube_big_endian_obj", 14669, -1, 0, 0, 3, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, m..." },
	{ "maya_cube_big_endian_obj", 14669, -1, 0, 0, 6, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, m..." },
	{ "maya_cube_big_endian_obj", 14686, -1, 0, 0, 12, 0, 0, "fbx_mesh->faces.data" },
	{ "maya_cube_big_endian_obj", 14686, -1, 0, 0, 6, 0, 0, "fbx_mesh->faces.data" },
	{ "maya_cube_big_endian_obj", 14687, -1, 0, 0, 14, 0, 0, "fbx_mesh->face_material.data" },
	{ "maya_cube_big_endian_obj", 14687, -1, 0, 0, 7, 0, 0, "fbx_mesh->face_material.data" },
	{ "maya_cube_big_endian_obj", 14692, -1, 0, 0, 16, 0, 0, "fbx_mesh->face_smoothing.data" },
	{ "maya_cube_big_endian_obj", 14692, -1, 0, 0, 8, 0, 0, "fbx_mesh->face_smoothing.data" },
	{ "maya_cube_big_endian_obj", 14706, 71, 102, 0, 0, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 14709, -1, 0, 0, 10, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 14709, -1, 0, 0, 20, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 14712, -1, 0, 0, 11, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 14712, -1, 0, 0, 22, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 14754, -1, 0, 0, 12, 0, 0, "ufbxi_finalize_mesh(&uc->result, &uc->error, fbx_mesh)" },
	{ "maya_cube_big_endian_obj", 14754, -1, 0, 0, 24, 0, 0, "ufbxi_finalize_mesh(&uc->result, &uc->error, fbx_mesh)" },
	{ "maya_cube_big_endian_obj", 14781, -1, 0, 674, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "maya_cube_big_endian_obj", 14781, -1, 0, 9, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "maya_cube_big_endian_obj", 14788, 83, 46, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_POSITION, 1..." },
	{ "maya_cube_big_endian_obj", 14795, 111, 9, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_COLOR, 4)" },
	{ "maya_cube_big_endian_obj", 14802, 328, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_UV, 1)" },
	{ "maya_cube_big_endian_obj", 14804, 622, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_NORMAL, 1)" },
	{ "maya_cube_big_endian_obj", 14806, -1, 0, 60, 0, 0, 0, "ufbxi_obj_parse_indices(uc)" },
	{ "maya_cube_big_endian_obj", 14806, -1, 0, 966, 0, 0, 0, "ufbxi_obj_parse_indices(uc)" },
	{ "maya_cube_big_endian_obj", 14826, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "maya_cube_big_endian_obj", 14826, -1, 0, 691, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "maya_cube_big_endian_obj", 14846, -1, 0, 0, 2, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 14846, -1, 0, 0, 4, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 14847, 71, 102, 0, 0, 0, 0, "ufbxi_obj_pop_meshes(uc)" },
	{ "maya_cube_big_endian_obj", 15101, -1, 0, 3, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "maya_cube_big_endian_obj", 15101, -1, 0, 662, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "maya_cube_big_endian_obj", 15102, 71, 102, 0, 0, 0, 0, "ufbxi_obj_parse_file(uc)" },
	{ "maya_cube_big_endian_obj", 15103, -1, 0, 0, 14, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_big_endian_obj", 15103, -1, 0, 1061, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_big_endian_obj", 21198, 71, 102, 0, 0, 0, 0, "ufbxi_obj_load(uc)" },
	{ "maya_display_layers_6100_binary", 12551, -1, 0, 1534, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_display_layers_6100_binary", 12551, -1, 0, 5731, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_display_layers_6100_binary", 18717, -1, 0, 1693, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_display_layers_6100_binary", 18717, -1, 0, 6230, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_human_ik_6100_binary", 11751, -1, 0, 11927, 0, 0, 0, "marker" },
	{ "maya_human_ik_6100_binary", 11751, -1, 0, 41356, 0, 0, 0, "marker" },
	{ "maya_human_ik_6100_binary", 12400, -1, 0, 17429, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 12400, -1, 0, 60359, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 12402, -1, 0, 11927, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 12402, -1, 0, 41356, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_7400_binary", 12483, -1, 0, 10199, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 12483, -1, 0, 2591, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 12485, -1, 0, 1832, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 12485, -1, 0, 7486, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_interpolation_modes_6100_binary", 12750, 16936, 73, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_interpolation_modes_7500_ascii", 8331, -1, 0, 848, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 8519, 291, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_node_attribute_zoo_6100_ascii", 8614, -1, 0, 5492, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 8642, -1, 0, 5921, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_binary", 11594, -1, 0, 15032, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 11594, -1, 0, 4140, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 11599, 138209, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->basis..." },
	{ "maya_node_attribute_zoo_6100_binary", 11601, 138308, 255, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 11608, 138359, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 11609, 138416, 1, 0, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 11610, 143462, 27, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 11624, -1, 0, 15234, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 11624, -1, 0, 4209, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 11629, 139478, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 11630, 139592, 1, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"ZZ\", &dimens..." },
	{ "maya_node_attribute_zoo_6100_binary", 11631, 139631, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Step, \"II\", &step_u, &ste..." },
	{ "maya_node_attribute_zoo_6100_binary", 11632, 139664, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 11645, 139691, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 11646, 139727, 1, 0, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 11647, 140321, 3, 0, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 11648, 141818, 63, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 11649, 139655, 1, 0, 0, 0, 0, "points->size / 4 == (size_t)dimension_u * (size_t)dimen..." },
	{ "maya_node_attribute_zoo_6100_binary", 11736, -1, 0, 3196, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 11736, -1, 0, 710, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 12376, -1, 0, 3196, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 12376, -1, 0, 710, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 12378, -1, 0, 1742, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12378, -1, 0, 275, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12396, -1, 0, 1963, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12396, -1, 0, 7532, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12404, -1, 0, 10390, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12404, -1, 0, 2787, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12473, -1, 0, 14094, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 12473, -1, 0, 3879, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 12497, 138209, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 12499, 139478, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 12503, -1, 0, 15652, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 12503, -1, 0, 4348, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 12505, -1, 0, 15805, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 12505, -1, 0, 4393, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 12608, -1, 0, 17393, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &src_prop..." },
	{ "maya_node_attribute_zoo_6100_binary", 12611, -1, 0, 17368, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst_prop..." },
	{ "maya_node_attribute_zoo_6100_binary", 16815, -1, 0, 0, 486, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 16815, -1, 0, 0, 972, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 16858, -1, 0, 0, 1010, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 16858, -1, 0, 0, 505, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 18307, -1, 0, 0, 486, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 18307, -1, 0, 0, 972, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 18312, -1, 0, 0, 495, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 18312, -1, 0, 0, 990, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 18313, -1, 0, 0, 496, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 18313, -1, 0, 0, 992, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 18790, -1, 0, 0, 1010, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_6100_binary", 18790, -1, 0, 0, 505, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_7500_ascii", 8331, -1, 0, 11662, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 8611, -1, 0, 3339, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 8612, -1, 0, 11663, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 8612, -1, 0, 3326, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_binary", 10287, -1, 0, 0, 325, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_node_attribute_zoo_7500_binary", 11867, -1, 0, 1749, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 11867, -1, 0, 6405, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 11872, 61038, 255, 0, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 11873, 61115, 255, 0, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 11874, 61175, 255, 0, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 11875, 61234, 255, 0, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 11876, 61292, 255, 0, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_binary", 11879, 61122, 0, 0, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_binary", 11884, 61242, 0, 0, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_binary", 11885, 61300, 0, 0, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", 11889, -1, 0, 0, 326, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 11889, -1, 0, 0, 652, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 12040, 61431, 0, 0, 0, 0, 0, "refs_left >= 0" },
	{ "maya_node_attribute_zoo_7500_binary", 12471, -1, 0, 2954, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 12471, -1, 0, 652, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 12475, -1, 0, 2711, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 12475, -1, 0, 582, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 12477, -1, 0, 2451, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 12477, -1, 0, 489, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 12479, -1, 0, 3149, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 12479, -1, 0, 703, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 12487, -1, 0, 1145, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 12487, -1, 0, 4460, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 12536, -1, 0, 1760, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 12536, -1, 0, 6440, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 12538, 61038, 255, 0, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", 15509, -1, 0, 2112, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 15509, -1, 0, 7512, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 15550, -1, 0, 2114, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 15550, -1, 0, 7516, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 7455, 61146, 109, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 7456, 61333, 103, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 7694, -1, 0, 0, 0, 0, 2942, "ufbxi_resume_progress(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 7698, -1, 0, 0, 0, 0, 2943, "res != -28" },
	{ "maya_polygon_hole_6100_binary", 11432, 9377, 37, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_polygon_hole_6100_binary", 11434, 9342, 0, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_hole.data, &..." },
	{ "maya_resampled_7500_binary", 11913, 24917, 23, 0, 0, 0, 0, "p_ref < p_ref_end" },
	{ "maya_shaderfx_pbs_material_7700_ascii", 9931, -1, 0, 1352, 0, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "maya_shaderfx_pbs_material_7700_ascii", 9931, -1, 0, 5212, 0, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "maya_texture_layers_6100_binary", 12094, -1, 0, 1451, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 12094, -1, 0, 5527, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 12103, -1, 0, 1454, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 12103, -1, 0, 5535, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 12528, -1, 0, 1451, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 12528, -1, 0, 5527, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 15868, -1, 0, 1666, 0, 0, 0, "((ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_texture_layers_6100_binary", 15868, -1, 0, 6196, 0, 0, 0, "((ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_texture_layers_6100_binary", 15875, -1, 0, 0, 267, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 15875, -1, 0, 0, 534, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 17368, -1, 0, 1676, 0, 0, 0, "textures" },
	{ "maya_texture_layers_6100_binary", 17368, -1, 0, 6219, 0, 0, 0, "textures" },
	{ "maya_texture_layers_6100_binary", 17370, -1, 0, 6221, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_texture_layers_6100_binary", 17444, -1, 0, 1673, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 17444, -1, 0, 6213, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 17464, -1, 0, 1676, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &deps, &..." },
	{ "maya_texture_layers_6100_binary", 17464, -1, 0, 6219, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &deps, &..." },
	{ "maya_texture_layers_6100_binary", 17475, -1, 0, 1677, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 17475, -1, 0, 6222, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 17483, -1, 0, 1680, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &files, ..." },
	{ "maya_texture_layers_6100_binary", 17483, -1, 0, 6228, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &files, ..." },
	{ "maya_texture_layers_6100_binary", 17487, -1, 0, 0, 273, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 17487, -1, 0, 0, 546, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 17514, -1, 0, 6207, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 17518, -1, 0, 1671, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 17518, -1, 0, 6208, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 18669, -1, 0, 1666, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_texture_layers_6100_binary", 18669, -1, 0, 6196, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_textured_cube_6100_binary", 18528, -1, 0, 1658, 0, 0, 0, "mat_texs" },
	{ "maya_textured_cube_6100_binary", 18528, -1, 0, 6245, 0, 0, 0, "mat_texs" },
	{ "maya_transform_animation_6100_binary", 12785, 17549, 11, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "mtl_fuzz_0000", 14913, -1, 0, 0, 4, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &prop->v..." },
	{ "mtl_fuzz_0000", 4345, -1, 0, 0, 4, 0, 0, "p_blob->data" },
	{ "obj_fuzz_0030", 14814, -1, 0, 28, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "obj_fuzz_0030", 14814, -1, 0, 740, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "obj_fuzz_0159", 14256, 116, 11, 0, 0, 0, 0, "index < 0xffffffffffffffffui64 / 10 - 10" },
	{ "revit_empty_7400_binary", 10582, -1, 0, 0, 258, 0, 0, "new_indices" },
	{ "revit_empty_7400_binary", 10582, -1, 0, 0, 516, 0, 0, "new_indices" },
	{ "revit_empty_7400_binary", 10665, -1, 0, 0, 258, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "revit_empty_7400_binary", 10665, -1, 0, 0, 516, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "revit_empty_7400_binary", 12566, -1, 0, 3934, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadat..." },
	{ "revit_empty_7400_binary", 12566, -1, 0, 898, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadat..." },
	{ "revit_empty_7400_binary", 7400, -1, 0, 0, 301, 0, 0, "d->data" },
	{ "revit_empty_7400_binary", 7400, -1, 0, 0, 602, 0, 0, "d->data" },
	{ "synthetic_binary_props_7500_ascii", 10525, -1, 0, 3793, 0, 0, 0, "unknown" },
	{ "synthetic_binary_props_7500_ascii", 10525, -1, 0, 946, 0, 0, 0, "unknown" },
	{ "synthetic_binary_props_7500_ascii", 10532, -1, 0, 3802, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "synthetic_binary_props_7500_ascii", 10534, -1, 0, 3803, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "synthetic_binary_props_7500_ascii", 12568, -1, 0, 3793, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_binary_props_7500_ascii", 12568, -1, 0, 946, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_binary_props_7500_ascii", 8554, -1, 0, 104, 0, 0, 0, "v->data" },
	{ "synthetic_binary_props_7500_ascii", 8554, -1, 0, 831, 0, 0, 0, "v->data" },
	{ "synthetic_blend_shape_order_7500_ascii", 10817, -1, 0, 3274, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_blend_shape_order_7500_ascii", 10868, -1, 0, 3272, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 10868, -1, 0, 755, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 10876, -1, 0, 3274, 0, 0, 0, "ufbxi_sort_blend_offsets(uc, offsets, num_offsets)" },
	{ "synthetic_broken_filename_7500_ascii", 12070, -1, 0, 3630, 0, 0, 0, "texture" },
	{ "synthetic_broken_filename_7500_ascii", 12070, -1, 0, 831, 0, 0, 0, "texture" },
	{ "synthetic_broken_filename_7500_ascii", 12123, -1, 0, 3516, 0, 0, 0, "video" },
	{ "synthetic_broken_filename_7500_ascii", 12123, -1, 0, 796, 0, 0, 0, "video" },
	{ "synthetic_broken_filename_7500_ascii", 12526, -1, 0, 3630, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 12526, -1, 0, 831, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 12530, -1, 0, 3516, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 12530, -1, 0, 796, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 13731, -1, 0, 3915, 0, 0, 0, "result" },
	{ "synthetic_broken_filename_7500_ascii", 13731, -1, 0, 923, 0, 0, 0, "result" },
	{ "synthetic_broken_filename_7500_ascii", 13751, -1, 0, 0, 256, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 13751, -1, 0, 3917, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 15776, -1, 0, 3911, 0, 0, 0, "tex" },
	{ "synthetic_broken_filename_7500_ascii", 15776, -1, 0, 921, 0, 0, 0, "tex" },
	{ "synthetic_broken_filename_7500_ascii", 15786, -1, 0, 0, 255, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 15786, -1, 0, 0, 510, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 17304, -1, 0, 3929, 0, 0, 0, "entry" },
	{ "synthetic_broken_filename_7500_ascii", 17304, -1, 0, 927, 0, 0, 0, "entry" },
	{ "synthetic_broken_filename_7500_ascii", 17307, -1, 0, 3931, 0, 0, 0, "file" },
	{ "synthetic_broken_filename_7500_ascii", 17307, -1, 0, 928, 0, 0, 0, "file" },
	{ "synthetic_broken_filename_7500_ascii", 17333, -1, 0, 0, 258, 0, 0, "files" },
	{ "synthetic_broken_filename_7500_ascii", 17333, -1, 0, 0, 516, 0, 0, "files" },
	{ "synthetic_broken_filename_7500_ascii", 17410, -1, 0, 3934, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_broken_filename_7500_ascii", 17410, -1, 0, 929, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_broken_filename_7500_ascii", 17414, -1, 0, 3936, 0, 0, 0, "states" },
	{ "synthetic_broken_filename_7500_ascii", 17414, -1, 0, 930, 0, 0, 0, "states" },
	{ "synthetic_broken_filename_7500_ascii", 17499, -1, 0, 0, 260, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 17499, -1, 0, 0, 520, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 17767, -1, 0, 3915, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 17767, -1, 0, 923, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 18505, -1, 0, 3911, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 18505, -1, 0, 921, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 18617, -1, 0, 3913, 0, 0, 0, "content_videos" },
	{ "synthetic_broken_filename_7500_ascii", 18617, -1, 0, 922, 0, 0, 0, "content_videos" },
	{ "synthetic_broken_filename_7500_ascii", 18622, -1, 0, 3915, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 18622, -1, 0, 923, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 18623, -1, 0, 3919, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 18623, -1, 0, 924, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 18664, -1, 0, 3923, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_broken_filename_7500_ascii", 18664, -1, 0, 925, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_broken_filename_7500_ascii", 18665, -1, 0, 3926, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_broken_filename_7500_ascii", 18665, -1, 0, 926, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_broken_filename_7500_ascii", 18683, -1, 0, 3929, 0, 0, 0, "ufbxi_insert_texture_file(uc, texture)" },
	{ "synthetic_broken_filename_7500_ascii", 18683, -1, 0, 927, 0, 0, 0, "ufbxi_insert_texture_file(uc, texture)" },
	{ "synthetic_broken_filename_7500_ascii", 18687, -1, 0, 0, 258, 0, 0, "ufbxi_pop_texture_files(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 18687, -1, 0, 0, 516, 0, 0, "ufbxi_pop_texture_files(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 18793, -1, 0, 3934, 0, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 18793, -1, 0, 929, 0, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_color_suzanne_0_obj", 14797, -1, 0, 15, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_0_obj", 14797, -1, 0, 697, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_1_obj", 14601, -1, 0, 2184, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "synthetic_cube_nan_6100_ascii", 8180, 4866, 45, 0, 0, 0, 0, "token->type == 'F'" },
	{ "synthetic_empty_elements_7500_ascii", 15613, 2800, 49, 0, 0, 0, 0, "depth <= num_nodes" },
	{ "synthetic_face_groups_0_obj", 14820, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "synthetic_indexed_by_vertex_7500_ascii", 10671, -1, 0, 0, 159, 0, 0, "new_index_data" },
	{ "synthetic_indexed_by_vertex_7500_ascii", 10671, -1, 0, 0, 318, 0, 0, "new_index_data" },
	{ "synthetic_missing_version_6100_ascii", 12148, -1, 0, 13460, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 12148, -1, 0, 3892, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 12172, -1, 0, 13469, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 12172, -1, 0, 3896, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 12182, -1, 0, 13471, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 12182, -1, 0, 3897, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 12398, -1, 0, 1655, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 12398, -1, 0, 251, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 12540, -1, 0, 13460, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 12540, -1, 0, 3892, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 17957, -1, 0, 0, 252, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 17957, -1, 0, 0, 504, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_mixed_attribs_0_obj", 14792, -1, 0, 83, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_0_obj", 14792, -1, 0, 998, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14649, -1, 0, 0, 16, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, 0..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14649, -1, 0, 0, 32, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, 0..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14652, -1, 0, 0, 19, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14652, -1, 0, 0, 38, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14654, -1, 0, 1113, 0, 0, 0, "color_valid" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14654, -1, 0, 119, 0, 0, 0, "color_valid" },
	{ "synthetic_node_depth_fail_7400_binary", 7493, 23, 233, 0, 0, 0, 0, "depth < 64" },
	{ "synthetic_node_depth_fail_7500_ascii", 8446, 1, 33, 0, 0, 0, 0, "depth < 64" },
	{ "synthetic_parent_directory_7700_ascii", 17710, -1, 0, 3916, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_parent_directory_7700_ascii", 17750, -1, 0, 0, 261, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 17750, -1, 0, 3917, 0, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 17764, -1, 0, 0, 261, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_parent_directory_7700_ascii", 17764, -1, 0, 3916, 0, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_partial_attrib_0_obj", 14739, -1, 0, 0, 10, 0, 0, "indices" },
	{ "synthetic_partial_attrib_0_obj", 14739, -1, 0, 0, 20, 0, 0, "indices" },
	{ "synthetic_simple_materials_0_mtl", 13926, -1, 0, 778, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "synthetic_simple_materials_0_mtl", 15007, -1, 0, 13, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "synthetic_simple_materials_0_mtl", 15019, -1, 0, 933, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "synthetic_simple_materials_0_mtl", 15111, -1, 0, 3, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "synthetic_simple_materials_0_mtl", 15111, -1, 0, 662, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "synthetic_simple_materials_0_mtl", 15112, -1, 0, 0, 1, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "synthetic_simple_materials_0_mtl", 15112, -1, 0, 674, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "synthetic_simple_materials_0_mtl", 15113, -1, 0, 680, 0, 0, 0, "ufbxi_obj_parse_mtl(uc)" },
	{ "synthetic_simple_materials_0_mtl", 15113, -1, 0, 9, 0, 0, 0, "ufbxi_obj_parse_mtl(uc)" },
	{ "synthetic_simple_materials_0_mtl", 21201, -1, 0, 3, 0, 0, 0, "ufbxi_mtl_load(uc)" },
	{ "synthetic_simple_materials_0_mtl", 21201, -1, 0, 662, 0, 0, 0, "ufbxi_mtl_load(uc)" },
	{ "synthetic_simple_textures_0_mtl", 15011, -1, 0, 101, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 0)" },
	{ "synthetic_simple_textures_0_mtl", 15011, -1, 0, 1100, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 0)" },
	{ "synthetic_string_collision_7500_ascii", 4276, -1, 0, 2213, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_texture_opts_0_mtl", 14954, -1, 0, 19, 0, 0, 0, "ufbxi_obj_parse_prop(uc, tok, start + 1, 0, &start)" },
	{ "synthetic_texture_opts_0_mtl", 14954, -1, 0, 733, 0, 0, 0, "ufbxi_obj_parse_prop(uc, tok, start + 1, 0, &start)" },
	{ "synthetic_texture_opts_0_mtl", 14964, -1, 0, 0, 22, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tex_str,..." },
	{ "synthetic_texture_split_7500_ascii", 8646, 28571, 35, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_texture_split_7500_binary", 9840, -1, 0, 0, 229, 0, 0, "dst" },
	{ "synthetic_texture_split_7500_binary", 9840, -1, 0, 0, 458, 0, 0, "dst" },
	{ "synthetic_unicode_7500_binary", 4138, -1, 0, 12, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 4138, -1, 0, 692, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 4234, -1, 0, 1140, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_unicode_7500_binary", 4245, -1, 0, 12, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "synthetic_unicode_7500_binary", 4245, -1, 0, 692, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4147, -1, 0, 3067, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4147, -1, 0, 712, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4288, -1, 0, 3067, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4288, -1, 0, 712, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "zbrush_d20_6100_binary", 10423, -1, 0, 3522, 0, 0, 0, "conn" },
	{ "zbrush_d20_6100_binary", 10423, -1, 0, 888, 0, 0, 0, "conn" },
	{ "zbrush_d20_6100_binary", 10830, -1, 0, 3526, 0, 0, 0, "shape" },
	{ "zbrush_d20_6100_binary", 10830, -1, 0, 890, 0, 0, 0, "shape" },
	{ "zbrush_d20_6100_binary", 10838, 25242, 2, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "zbrush_d20_6100_binary", 10839, 25217, 0, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "zbrush_d20_6100_binary", 10852, 25290, 2, 0, 0, 0, 0, "normals && normals->size == vertices->size" },
	{ "zbrush_d20_6100_binary", 10898, 25189, 0, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "zbrush_d20_6100_binary", 10902, -1, 0, 3502, 0, 0, 0, "deformer" },
	{ "zbrush_d20_6100_binary", 10902, -1, 0, 880, 0, 0, 0, "deformer" },
	{ "zbrush_d20_6100_binary", 10903, -1, 0, 3511, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "zbrush_d20_6100_binary", 10903, -1, 0, 884, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "zbrush_d20_6100_binary", 10908, -1, 0, 3513, 0, 0, 0, "channel" },
	{ "zbrush_d20_6100_binary", 10908, -1, 0, 885, 0, 0, 0, "channel" },
	{ "zbrush_d20_6100_binary", 10911, -1, 0, 3520, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_6100_binary", 10911, -1, 0, 887, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_6100_binary", 10915, -1, 0, 0, 101, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 10915, -1, 0, 0, 202, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 10927, -1, 0, 3522, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "zbrush_d20_6100_binary", 10927, -1, 0, 888, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "zbrush_d20_6100_binary", 10938, -1, 0, 3524, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &shape_info.fbx_id)" },
	{ "zbrush_d20_6100_binary", 10938, -1, 0, 889, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &shape_info.fbx_id)" },
	{ "zbrush_d20_6100_binary", 10942, 25217, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "zbrush_d20_6100_binary", 10944, -1, 0, 3535, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "zbrush_d20_6100_binary", 10944, -1, 0, 894, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "zbrush_d20_6100_binary", 10945, -1, 0, 3537, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "zbrush_d20_6100_binary", 10945, -1, 0, 895, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "zbrush_d20_6100_binary", 11065, -1, 0, 0, 136, 0, 0, "ids" },
	{ "zbrush_d20_6100_binary", 11065, -1, 0, 0, 68, 0, 0, "ids" },
	{ "zbrush_d20_6100_binary", 11101, -1, 0, 0, 138, 0, 0, "groups" },
	{ "zbrush_d20_6100_binary", 11101, -1, 0, 0, 69, 0, 0, "groups" },
	{ "zbrush_d20_6100_binary", 11216, 25189, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "zbrush_d20_6100_binary", 11425, 8305, 32, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "zbrush_d20_6100_binary", 11427, 8394, 33, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_group.data, ..." },
	{ "zbrush_d20_6100_binary", 11544, -1, 0, 0, 136, 0, 0, "ufbxi_assign_face_groups(&uc->result, &uc->error, mesh,..." },
	{ "zbrush_d20_6100_binary", 11544, -1, 0, 0, 68, 0, 0, "ufbxi_assign_face_groups(&uc->result, &uc->error, mesh,..." },
	{ "zbrush_d20_6100_binary", 15824, -1, 0, 1405, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "zbrush_d20_6100_binary", 15824, -1, 0, 5293, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "zbrush_d20_6100_binary", 15832, -1, 0, 0, 267, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 15832, -1, 0, 0, 534, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 15845, -1, 0, 1398, 0, 0, 0, "((ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_s..." },
	{ "zbrush_d20_6100_binary", 15845, -1, 0, 5275, 0, 0, 0, "((ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_s..." },
	{ "zbrush_d20_6100_binary", 15852, -1, 0, 0, 258, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 15852, -1, 0, 0, 516, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 15947, -1, 0, 5325, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "zbrush_d20_6100_binary", 15982, -1, 0, 5277, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "zbrush_d20_6100_binary", 18111, -1, 0, 1395, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 18111, -1, 0, 5267, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 18141, -1, 0, 1397, 0, 0, 0, "full_weights" },
	{ "zbrush_d20_6100_binary", 18141, -1, 0, 5273, 0, 0, 0, "full_weights" },
	{ "zbrush_d20_6100_binary", 18146, -1, 0, 1398, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 18146, -1, 0, 5275, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 18157, -1, 0, 5277, 0, 0, 0, "ufbxi_sort_blend_keyframes(uc, channel->keyframes.data,..." },
	{ "zbrush_d20_6100_binary", 18279, -1, 0, 1404, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 18279, -1, 0, 5291, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 18281, -1, 0, 1405, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "zbrush_d20_6100_binary", 18281, -1, 0, 5293, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "zbrush_d20_6100_binary", 18520, -1, 0, 1410, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 18520, -1, 0, 5305, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 18550, -1, 0, 1411, 0, 0, 0, "mat_tex" },
	{ "zbrush_d20_6100_binary", 18550, -1, 0, 5307, 0, 0, 0, "mat_tex" },
	{ "zbrush_d20_6100_binary", 18584, -1, 0, 0, 273, 0, 0, "texs" },
	{ "zbrush_d20_6100_binary", 18584, -1, 0, 0, 546, 0, 0, "texs" },
	{ "zbrush_d20_6100_binary", 18603, -1, 0, 1414, 0, 0, 0, "tex" },
	{ "zbrush_d20_6100_binary", 18603, -1, 0, 5314, 0, 0, 0, "tex" },
	{ "zbrush_d20_6100_binary", 18630, -1, 0, 5325, 0, 0, 0, "ufbxi_sort_videos_by_filename(uc, content_videos, num_c..." },
	{ "zbrush_d20_7500_ascii", 12140, -1, 0, 0, 256, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "zbrush_d20_7500_ascii", 12140, -1, 0, 0, 512, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "zbrush_d20_7500_ascii", 8271, -1, 0, 1476, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "zbrush_d20_7500_ascii", 8271, -1, 0, 5630, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "zbrush_d20_7500_ascii", 9858, -1, 0, 0, 256, 0, 0, "dst_blob->data" },
	{ "zbrush_d20_7500_ascii", 9858, -1, 0, 0, 512, 0, 0, "dst_blob->data" },
	{ "zbrush_d20_7500_binary", 11823, -1, 0, 1059, 0, 0, 0, "channel" },
	{ "zbrush_d20_7500_binary", 11823, -1, 0, 4179, 0, 0, 0, "channel" },
	{ "zbrush_d20_7500_binary", 11831, -1, 0, 1063, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_7500_binary", 11831, -1, 0, 4188, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_7500_binary", 12495, 32981, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 12515, -1, 0, 1047, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "zbrush_d20_7500_binary", 12515, -1, 0, 4144, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "zbrush_d20_7500_binary", 12517, -1, 0, 1059, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 12517, -1, 0, 4179, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 12240, -1, 0, 1289, 0, 0, 0, "set" },
	{ "zbrush_d20_selection_set_6100_binary", 12240, -1, 0, 4773, 0, 0, 0, "set" },
	{ "zbrush_d20_selection_set_6100_binary", 12257, -1, 0, 3786, 0, 0, 0, "sel" },
	{ "zbrush_d20_selection_set_6100_binary", 12257, -1, 0, 969, 0, 0, 0, "sel" },
	{ "zbrush_d20_selection_set_6100_binary", 12547, -1, 0, 1289, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 12547, -1, 0, 4773, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 12554, -1, 0, 3786, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 12554, -1, 0, 969, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 18722, -1, 0, 2190, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_d20_selection_set_6100_binary", 18722, -1, 0, 7782, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_polygroup_mess_0_obj", 11181, -1, 0, 0, 2036, 0, 0, "face_indices" },
	{ "zbrush_polygroup_mess_0_obj", 11181, -1, 0, 0, 4072, 0, 0, "face_indices" },
	{ "zbrush_polygroup_mess_0_obj", 14699, -1, 0, 0, 2033, 0, 0, "fbx_mesh->face_group.data" },
	{ "zbrush_polygroup_mess_0_obj", 14699, -1, 0, 0, 4066, 0, 0, "fbx_mesh->face_group.data" },
	{ "zbrush_polygroup_mess_0_obj", 14757, -1, 0, 0, 2036, 0, 0, "ufbxi_update_face_groups(&uc->result, &uc->error, fbx_m..." },
	{ "zbrush_polygroup_mess_0_obj", 14757, -1, 0, 0, 4072, 0, 0, "ufbxi_update_face_groups(&uc->result, &uc->error, fbx_m..." },
	{ "zbrush_vertex_color_obj", 13886, -1, 0, 0, 11, 0, 0, "color_set" },
	{ "zbrush_vertex_color_obj", 13886, -1, 0, 0, 22, 0, 0, "color_set" },
	{ "zbrush_vertex_color_obj", 14458, -1, 0, 26, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_obj", 14458, -1, 0, 810, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_obj", 14600, -1, 0, 1002, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_obj", 14600, -1, 0, 70, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_obj", 14601, -1, 0, 1004, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "zbrush_vertex_color_obj", 14615, -1, 0, 1002, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_obj", 14615, -1, 0, 70, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_obj", 14674, -1, 0, 0, 0, 880, 0, "min_ix < 0xffffffffffffffffui64" },
	{ "zbrush_vertex_color_obj", 14675, -1, 0, 0, 4, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "zbrush_vertex_color_obj", 14675, -1, 0, 0, 8, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "zbrush_vertex_color_obj", 14677, -1, 0, 1007, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_obj", 14677, -1, 0, 72, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_obj", 14833, -1, 0, 26, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_obj", 14833, -1, 0, 810, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_obj", 14946, -1, 0, 1035, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_obj", 14946, -1, 0, 75, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_obj", 14964, -1, 0, 1042, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tex_str,..." },
	{ "zbrush_vertex_color_obj", 14965, -1, 0, 1043, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &tex_raw..." },
	{ "zbrush_vertex_color_obj", 14969, -1, 0, 1044, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_obj", 14969, -1, 0, 76, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_obj", 14978, -1, 0, 0, 17, 0, 0, "ufbxi_obj_pop_props(uc, &texture->props.props, num_prop..." },
	{ "zbrush_vertex_color_obj", 14978, -1, 0, 0, 34, 0, 0, "ufbxi_obj_pop_props(uc, &texture->props.props, num_prop..." },
	{ "zbrush_vertex_color_obj", 14984, -1, 0, 0, 18, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop, 0)" },
	{ "zbrush_vertex_color_obj", 14984, -1, 0, 1053, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop, 0)" },
	{ "zbrush_vertex_color_obj", 14987, -1, 0, 1055, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_obj", 14987, -1, 0, 80, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_obj", 15009, -1, 0, 1035, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
	{ "zbrush_vertex_color_obj", 15009, -1, 0, 75, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
};

typedef struct {
	const char *name;
	size_t read_buffer_size;
} ufbxt_buffer_check;

static const ufbxt_buffer_check g_buffer_checks[] = {
	{ "blender_272_cube_7400_binary", 9484 },
	{ "blender_279_color_sets_7400_binary", 10255 },
	{ "blender_279_ball_7400_binary", 14303 },
	{ "blender_279_internal_textures_7400_binary", 13711 },
	{ "blender_293_textures_7400_binary", 13695 },
	{ "blender_293_embedded_textures_7400_binary", 13695 },
	{ "blender_293_material_mapping_7400_binary", 11388 },
	{ "blender_293x_nonmanifold_subsurf_7400_binary", 10447 },
	{ "blender_293_ngon_subsurf_7400_binary", 10223 },
};

static bool ufbxt_fuzz_should_skip(int iter)
{
	if ((iter >> g_fuzz_quality >> g_fuzz_quality) != 0) {
		return (iter & (iter - 1)) != 0;
	} else {
		return (iter & ((1 << g_fuzz_quality) - 1)) % ((iter >> g_fuzz_quality) + 1) != 0;
	}
}

typedef struct {
	uint64_t calls;
} ufbxt_progress_ctx;

ufbx_progress_result ufbxt_measure_progress(void *user, const ufbx_progress *progress)
{
	ufbxt_progress_ctx *ctx = (ufbxt_progress_ctx*)user;
	ctx->calls++;
	return UFBX_PROGRESS_CONTINUE;
}

void ufbxt_do_fuzz(const char *base_name, void *data, size_t size, const char *filename, bool allow_error, ufbx_file_format file_format, const ufbx_load_opts *default_opts)
{
	if (g_no_fuzz) return;

	size_t temp_allocs = 1000;
	size_t result_allocs = 500;
	size_t progress_calls = 100;

	{
		ufbxt_progress_ctx progress_ctx = { 0 };

		bool temp_freed = false, result_freed = false;

		ufbx_load_opts prog_opts = { 0 };
		if (default_opts) {
			prog_opts = *default_opts;
		}

		ufbxt_init_allocator(&prog_opts.temp_allocator, &temp_freed);
		ufbxt_init_allocator(&prog_opts.result_allocator, &result_freed);
		prog_opts.file_format = file_format;
		prog_opts.read_buffer_size = 1;
		prog_opts.temp_allocator.huge_threshold = 1;
		prog_opts.result_allocator.huge_threshold = 1;
		prog_opts.filename.data = NULL;
		prog_opts.filename.length = 0;
		prog_opts.progress_cb.fn = &ufbxt_measure_progress;
		prog_opts.progress_cb.user = &progress_ctx;
		prog_opts.progress_interval_hint = 1;

		ufbx_error prog_error;
		ufbx_scene *prog_scene = ufbx_load_memory(data, size, &prog_opts, &prog_error);
		if (!allow_error) {
			if (!prog_scene) {
				ufbxt_log_error(&prog_error);
			}
			ufbxt_assert(prog_scene);
		}

		if (prog_scene) {
			progress_calls = (size_t)progress_ctx.calls;
			temp_allocs = prog_scene->metadata.temp_allocs + 10;
			result_allocs = prog_scene->metadata.result_allocs + 10;

			ufbx_free_scene(prog_scene);
		}

		ufbxt_assert(temp_freed);
		ufbxt_assert(result_freed);
	}

	if (g_fuzz) {
		uint64_t begin = cputime_os_tick();

		size_t fail_step = 0;
		int i;

		g_fuzz_test_name = base_name;

		#pragma omp parallel for schedule(dynamic, 4)
		for (i = 0; i < (int)temp_allocs; i++) {
			if (ufbxt_fuzz_should_skip(i)) continue;
			if (omp_get_thread_num() == 0) {
				if (i % 16 == 0) {
					fprintf(stderr, "\rFuzzing temp limit %s: %d/%d", base_name, i, (int)temp_allocs);
					fflush(stderr);
				}
			}

			size_t step = 10000000 + (size_t)i;

			if (!ufbxt_test_fuzz(filename, data, size, default_opts, step, -1, (size_t)i, 0, 0, 0)) fail_step = step;
		}

		fprintf(stderr, "\rFuzzing temp limit %s: %d/%d\n", base_name, (int)temp_allocs, (int)temp_allocs);

		#pragma omp parallel for schedule(dynamic, 4)
		for (i = 0; i < (int)result_allocs; i++) {
			if (ufbxt_fuzz_should_skip(i)) continue;
			if (omp_get_thread_num() == 0) {
				if (i % 16 == 0) {
					fprintf(stderr, "\rFuzzing result limit %s: %d/%d", base_name, i, (int)result_allocs);
					fflush(stderr);
				}
			}

			size_t step = 20000000 + (size_t)i;

			if (!ufbxt_test_fuzz(filename, data, size, default_opts, step, -1, 0, (size_t)i, 0, 0)) fail_step = step;
		}

		fprintf(stderr, "\rFuzzing result limit %s: %d/%d\n", base_name, (int)result_allocs, (int)result_allocs);

		if (!g_fuzz_no_truncate) {
			#pragma omp parallel for schedule(dynamic, 4)
			for (i = 1; i < (int)size; i++) {
				if (ufbxt_fuzz_should_skip(i)) continue;
				if (omp_get_thread_num() == 0) {
					if (i % 16 == 0) {
						fprintf(stderr, "\rFuzzing truncate %s: %d/%d", base_name, i, (int)size);
						fflush(stderr);
					}
				}

				size_t step = 30000000 + (size_t)i;

				if (!ufbxt_test_fuzz(filename, data, size, default_opts, step, -1, 0, 0, (size_t)i, 0)) fail_step = step;
			}

			fprintf(stderr, "\rFuzzing truncate %s: %d/%d\n", base_name, (int)size, (int)size);
		}

		if (!g_fuzz_no_cancel) {
			#pragma omp parallel for schedule(dynamic, 4)
			for (i = 0; i < (int)progress_calls; i++) {
				if (ufbxt_fuzz_should_skip(i)) continue;
				if (omp_get_thread_num() == 0) {
					if (i % 16 == 0) {
						fprintf(stderr, "\rFuzzing cancel %s: %d/%d", base_name, i, (int)size);
						fflush(stderr);
					}
				}

				size_t step = 40000000 + (size_t)i;

				if (!ufbxt_test_fuzz(filename, data, size, default_opts, step, -1, 0, 0, 0, (size_t)i+1)) fail_step = step;
			}

			fprintf(stderr, "\rFuzzing cancel %s: %d/%d\n", base_name, (int)size, (int)size);
		}

		if (!g_fuzz_no_patch) {

			uint8_t *data_copy[256] = { 0 };

			int patch_start = g_patch_start - omp_get_num_threads() * 16;
			if (patch_start < 0) {
				patch_start = 0;
			}

			#pragma omp parallel for schedule(dynamic, 4)
			for (i = patch_start; i < (int)size; i++) {
				if (ufbxt_fuzz_should_skip(i)) continue;

				if (omp_get_thread_num() == 0) {
					if (i % 16 == 0) {
						fprintf(stderr, "\rFuzzing patch %s: %d/%d", base_name, i, (int)size);
						fflush(stderr);
					}
				}

				uint8_t **p_data_copy = &data_copy[omp_get_thread_num()];
				if (*p_data_copy == NULL) {
					*p_data_copy = malloc(size);
					memcpy(*p_data_copy, data, size);
				}
				uint8_t *data_u8 = *p_data_copy;

				size_t step = i * 10;

				uint8_t original = data_u8[i];

				if (g_all_byte_values) {
					for (uint32_t v = 0; v < 256; v++) {
						data_u8[i] = (uint8_t)v;
						if (!ufbxt_test_fuzz(filename, data_u8, size, default_opts, step + v, i, 0, 0, 0, 0)) fail_step = step + v;
					}
				} else {
					data_u8[i] = original + 1;
					if (!ufbxt_test_fuzz(filename, data_u8, size, default_opts, step + 1, i, 0, 0, 0, 0)) fail_step = step + 1;

					data_u8[i] = original - 1;
					if (!ufbxt_test_fuzz(filename, data_u8, size, default_opts, step + 2, i, 0, 0, 0, 0)) fail_step = step + 2;

					if (original != 0) {
						data_u8[i] = 0;
						if (!ufbxt_test_fuzz(filename, data_u8, size, default_opts, step + 3, i, 0, 0, 0, 0)) fail_step = step + 3;
					}

					if (original != 0xff) {
						data_u8[i] = 0xff;
						if (!ufbxt_test_fuzz(filename, data_u8, size, default_opts, step + 4, i, 0, 0, 0, 0)) fail_step = step + 4;
					}
				}


				data_u8[i] = original;
			}

			fprintf(stderr, "\rFuzzing patch %s: %d/%d\n", base_name, (int)size, (int)size);

			for (size_t i = 0; i < ufbxt_arraycount(data_copy); i++) {
				free(data_copy[i]);
			}

		}

		ufbxt_hintf("Fuzz failed on step: %zu", fail_step);
		ufbxt_assert(fail_step == 0);

		uint64_t end = cputime_os_tick();
		fprintf(stderr, ".. fuzzing done in %.2fs (quality=%d)\n", cputime_os_delta_to_sec(NULL, end - begin), g_fuzz_quality);

	} else {
		uint8_t *data_u8 = (uint8_t*)data;

		// Run a couple of known fuzz checks
		for (size_t i = 0; i < ufbxt_arraycount(g_fuzz_checks); i++) {
			const ufbxt_fuzz_check *check = &g_fuzz_checks[i];
			if (strcmp(check->name, base_name)) continue;

			uint8_t original;
			if (check->patch_offset >= 0) {
				original = data_u8[check->patch_offset];
				ufbxt_logf(".. Patch byte %u from 0x%02x to 0x%02x: %s", check->patch_offset, original, check->patch_value, check->description);
				ufbxt_assert((size_t)check->patch_offset < size);
				data_u8[check->patch_offset] = check->patch_value;
			}

			ufbx_load_opts opts = { 0 };
			ufbxt_cancel_ctx cancel_ctx = { 0 };

			if (default_opts) {
				opts = *default_opts;
			}

			opts.load_external_files = true;
			opts.filename.data = filename;
			opts.filename.length = SIZE_MAX;

			bool temp_freed = false, result_freed = false;
			ufbxt_init_allocator(&opts.temp_allocator, &temp_freed);
			ufbxt_init_allocator(&opts.result_allocator, &result_freed);

			if (check->temp_limit > 0) {
				ufbxt_logf(".. Temp limit %u: %s", check->temp_limit, check->description);
				opts.temp_allocator.allocation_limit = check->temp_limit;
				opts.temp_allocator.huge_threshold = 1;
			}

			if (check->result_limit > 0) {
				ufbxt_logf(".. Result limit %u: %s", check->result_limit, check->description);
				opts.result_allocator.allocation_limit = check->result_limit;
				opts.result_allocator.huge_threshold = 1;
			}

			size_t truncated_size = size;
			if (check->truncate_length > 0) {
				ufbxt_logf(".. Truncated length %u: %s", check->truncate_length, check->description);
				truncated_size = check->truncate_length;
			}

			if (check->cancel_step > 0) {
				cancel_ctx.calls_left = check->cancel_step;
				opts.progress_cb.fn = &ufbxt_cancel_progress;
				opts.progress_cb.user = &cancel_ctx;
				opts.progress_interval_hint = 1;
			}

			ufbx_error error;
			ufbx_scene *scene = ufbx_load_memory(data, truncated_size, &opts, &error);
			if (scene) {
				ufbxt_check_scene(scene);
				ufbx_free_scene(scene);
			}

			ufbxt_assert(temp_freed);
			ufbxt_assert(result_freed);

			if (check->patch_offset >= 0) {
				data_u8[check->patch_offset] = original;
			}
		}
	}
}

const uint32_t ufbxt_file_versions[] = { 0, 1, 2, 3, 3000, 5000, 5800, 6100, 7100, 7200, 7300, 7400, 7500, 7700 };

typedef struct ufbxt_file_iterator {
	// Input
	const char *path;
	const char *root;
	bool allow_not_found;

	// State (clear to zero)
	uint32_t version_ix;
	uint32_t format_ix;
	uint32_t num_found;
} ufbxt_file_iterator;

bool ufbxt_next_file(ufbxt_file_iterator *iter, char *buffer, size_t buffer_size)
{
	for (;;) {
		if (iter->version_ix >= ufbxt_arraycount(ufbxt_file_versions)) {
			ufbxt_assert(iter->num_found > 0 || iter->allow_not_found);
			return false;
		}

		uint32_t version = ufbxt_file_versions[iter->version_ix];
		const char *format = "";
		const char *ext = "fbx";
		switch (iter->format_ix) {
		case 0: format = "binary"; break;
		case 1: format = "ascii"; break;
		case 2: format = "mtl"; ext = "mtl"; break;
		case 3: format = "obj"; ext = "obj"; break;
		}
		snprintf(buffer, buffer_size, "%s%s_%u_%s.%s", iter->root ? iter->root : data_root, iter->path, version, format, ext);

		iter->format_ix++;
		if (iter->format_ix >= 4) {
			iter->format_ix = 0;
			iter->version_ix++;
		}

		ufbx_stream stream = { 0 };
		if (ufbx_open_file(&stream, buffer, SIZE_MAX)) {
			ufbxt_logf("%s", buffer);
			if (stream.close_fn) {
				stream.close_fn(stream.user);
			}
			iter->num_found++;
			return true;
		}
	}
}

typedef enum ufbxt_file_test_flags {
	// Alternative test for a given file, does not execute fuzz tests again.
	UFBXT_FILE_TEST_FLAG_ALTERNATIVE = 0x1,

	// Allow scene loading to fail.
	// Calls test function with `scene == NULL && load_error != NULL` on failure.
	UFBXT_FILE_TEST_FLAG_ALLOW_ERROR = 0x2,

	// Allow invalid Unicode in the file.
	UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE = 0x4,

	// This test is heavy to fuzz and is fuzzed with lower quality, use `--heavy-fuzz-quality` to control it.
	UFBXT_FILE_TEST_FLAG_HEAVY_TO_FUZZ = 0x8,

	// Allow scene loading to fail if `ufbx_load_opts.strict` is specified.
	UFBXT_FILE_TEST_FLAG_ALLOW_STRICT_ERROR = 0x10,

	// Skip tests with various `ufbx_load_opts`.
	// Useful if the file isn't particularly interesting but has a lot of content.
	UFBXT_FILE_TEST_FLAG_SKIP_LOAD_OPTS_CHECKS = 0x20,

	// Fuzz even if being an alternative test
	UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS = 0x40,

	// Pass user_opts to the fuzzer
	UFBXT_FILE_TEST_FLAG_FUZZ_OPTS = 0x80,

	// Diff even if being an alternative test
	UFBXT_FILE_TEST_FLAG_DIFF_ALWAYS = 0x100,

	// Expect the diff to fail
	UFBXT_FILE_TEST_FLAG_DIFF_EXPECT_FAIL = 0x200,

	// Expect the diff to fail for version >= 7000 files
	UFBXT_FILE_TEST_FLAG_DIFF_EXPECT_FAIL_POST_7000 = 0x400,
} ufbxt_file_test_flags;

void ufbxt_do_file_test(const char *name, void (*test_fn)(ufbx_scene *s, ufbxt_diff_error *err, ufbx_error *load_error), const char *suffix, ufbx_load_opts user_opts, ufbxt_file_test_flags flags)
{
	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s.obj", data_root, name);
	size_t obj_size = 0;
	void *obj_data = ufbxt_read_file(buf, &obj_size);
	ufbxt_obj_file *obj_file = obj_data ? ufbxt_load_obj(obj_data, obj_size, NULL) : NULL;
	free(obj_data);

	// Override g_fuzz_quality if necessary
	int prev_fuzz_quality = -1;
	if (flags & UFBXT_FILE_TEST_FLAG_HEAVY_TO_FUZZ) {
		prev_fuzz_quality = g_fuzz_quality;
		g_fuzz_quality = g_heavy_fuzz_quality;
	}

	char base_name[512];

	bool allow_error = (flags & UFBXT_FILE_TEST_FLAG_ALLOW_ERROR) != 0;
	bool alternative = (flags & UFBXT_FILE_TEST_FLAG_ALTERNATIVE) != 0;
	bool allow_strict_error = (flags & UFBXT_FILE_TEST_FLAG_ALLOW_STRICT_ERROR) != 0;
	bool skip_opts_checks = (flags & UFBXT_FILE_TEST_FLAG_SKIP_LOAD_OPTS_CHECKS) != 0;
	bool fuzz_always = (flags & UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS) != 0;
	bool diff_always = (flags & UFBXT_FILE_TEST_FLAG_DIFF_ALWAYS) != 0;

	const ufbx_load_opts *fuzz_opts = NULL;
	if ((flags & UFBXT_FILE_TEST_FLAG_FUZZ_OPTS) != 0) {
		fuzz_opts = &user_opts;
	}

	ufbx_scene *obj_scene = NULL;
	if (obj_file) {
		ufbxt_logf("%s [diff target found]", buf);
	}

	ufbxt_begin_fuzz();

	if (obj_file && !g_skip_obj_test && !alternative) {
		ufbx_load_opts obj_opts = { 0 };
		obj_opts.load_external_files = true;

		ufbx_error obj_error;
		obj_scene = ufbx_load_file(buf, &obj_opts, &obj_error);
		if (!obj_scene) {
			ufbxt_log_error(&obj_error);
			ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse .obj file");
		}
		ufbxt_assert(obj_scene->metadata.file_format == UFBX_FILE_FORMAT_OBJ);
		ufbxt_check_scene(obj_scene);

		ufbxt_diff_error err = { 0 };
		ufbxt_diff_to_obj(obj_scene, obj_file, &err, false);
		if (err.num > 0) {
			ufbx_real avg = err.sum / (ufbx_real)err.num;
			ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", avg, err.max, err.num);
		}

		size_t size = 0;
		void *data = ufbxt_read_file(buf, &size);
		ufbxt_assert(data);

		snprintf(base_name, sizeof(base_name), "%s_obj", name);
		if (!alternative || fuzz_always) {
			ufbxt_do_fuzz(base_name, data, size, buf, allow_error, UFBX_FILE_FORMAT_UNKNOWN, fuzz_opts);
		}

		free(data);
	}

	uint32_t num_opened = 0;

	for (uint32_t fi = 0; fi < 4; fi++) {
		for (uint32_t vi = 0; vi < ufbxt_arraycount(ufbxt_file_versions); vi++) {
			uint32_t version = ufbxt_file_versions[vi];
			const char *format = NULL;
			const char *ext = "fbx";
			switch (fi) {
			case 0: format = "binary"; break;
			case 1: format = "ascii"; break;
			case 2: format = "mtl"; ext = "mtl"; break;
			case 3: format = "obj"; ext = "obj"; break;
			}
			ufbxt_assert(format);

			if (suffix) {
				snprintf(buf, sizeof(buf), "%s%s_%u_%s_%s.%s", data_root, name, version, format, suffix, ext);
				snprintf(base_name, sizeof(base_name), "%s_%u_%s_%s", name, version, format, suffix);
			} else {
				snprintf(buf, sizeof(buf), "%s%s_%u_%s.%s", data_root, name, version, format, ext);
				snprintf(base_name, sizeof(base_name), "%s_%u_%s", name, version, format);
			}

			if (g_file_version && version != g_file_version) continue;
			if (g_file_type && strcmp(format, g_file_type)) continue;

			size_t size = 0;
			void *data = ufbxt_read_file(buf, &size);
			if (!data) continue;

			bool expect_diff_fail = (flags & UFBXT_FILE_TEST_FLAG_DIFF_EXPECT_FAIL) != 0;
			if ((flags & UFBXT_FILE_TEST_FLAG_DIFF_EXPECT_FAIL_POST_7000) != 0 && version >= 7000) {
				expect_diff_fail = true;
			}

			num_opened++;
			ufbxt_logf("%s", buf);

			ufbx_error error;

			ufbx_load_opts load_opts = user_opts;
			if (g_dedicated_allocs) {
				load_opts.temp_allocator.huge_threshold = 1;
				load_opts.result_allocator.huge_threshold = 1;
			}

			load_opts.evaluate_skinning = true;
			load_opts.load_external_files = true;

			if (!load_opts.filename.length) {
				load_opts.filename.data = buf;
				load_opts.filename.length = SIZE_MAX;
			}

			if (fi < 2) {
				load_opts.file_format = UFBX_FILE_FORMAT_FBX;
			} else if (fi == 2) {
				load_opts.file_format = UFBX_FILE_FORMAT_MTL;
			} else if (fi == 3) {
				load_opts.file_format = UFBX_FILE_FORMAT_OBJ;
			}

			ufbxt_progress_ctx progress_ctx = { 0 };

			ufbx_load_opts memory_opts = load_opts;
			memory_opts.progress_cb.fn = &ufbxt_measure_progress;
			memory_opts.progress_cb.user = &progress_ctx;

			uint64_t load_begin = cputime_cpu_tick();
			ufbx_scene *scene = ufbx_load_memory(data, size, &memory_opts, &error);
			uint64_t load_end = cputime_cpu_tick();

			if (scene) {
				ufbxt_check_scene(scene);
				ufbxt_assert(progress_ctx.calls >= size / 0x4000 / 2);
			} else if (!allow_error) {
				ufbxt_log_error(&error);
				ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file");
			}

			ufbxt_progress_ctx stream_progress_ctx = { 0 };

			bool temp_freed = false, result_freed = false;

			ufbx_load_opts stream_opts = load_opts;
			ufbxt_init_allocator(&stream_opts.temp_allocator, &temp_freed);
			ufbxt_init_allocator(&stream_opts.result_allocator, &result_freed);
			stream_opts.file_format = UFBX_FILE_FORMAT_UNKNOWN;
			stream_opts.read_buffer_size = 1;
			stream_opts.temp_allocator.huge_threshold = 2;
			stream_opts.result_allocator.huge_threshold = 2;
			stream_opts.filename.data = NULL;
			stream_opts.filename.length = 0;
			stream_opts.progress_cb.fn = &ufbxt_measure_progress;
			stream_opts.progress_cb.user = &stream_progress_ctx;
			stream_opts.progress_interval_hint = 1;
			stream_opts.retain_dom = true;

			if ((flags & UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE) == 0) {
				stream_opts.unicode_error_handling = UFBX_UNICODE_ERROR_HANDLING_ABORT_LOADING;
			}

			ufbx_scene *streamed_scene = ufbx_load_file(buf, &stream_opts, &error);
			if (streamed_scene) {
				ufbxt_check_scene(streamed_scene);
				ufbxt_assert(streamed_scene->dom_root);
				ufbxt_assert(streamed_scene->metadata.file_format == load_opts.file_format);
			} else if (!allow_error) {
				ufbxt_log_error(&error);
				ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse streamed file");
			}

			// Try a couple of read buffer sizes
			if (g_fuzz && !g_fuzz_no_buffer && g_fuzz_step == SIZE_MAX && (!alternative || fuzz_always)) {
				ufbxt_begin_fuzz();

				int fail_sz = -1;

				int buf_sz = 0;
				#pragma omp parallel for schedule(dynamic, 4)
				for (buf_sz = 0; buf_sz < (int)size; buf_sz++) {
					if (ufbxt_fuzz_should_skip(buf_sz)) continue;

					if (omp_get_thread_num() == 0) {
						if (buf_sz % 16 == 0) {
							fprintf(stderr, "\rFuzzing read buffer size %s: %d/%d", base_name, buf_sz, (int)size);
							fflush(stderr);
						}
					}
					#if UFBXT_HAS_THREADLOCAL
						t_jmp_buf = (ufbxt_jmp_buf*)calloc(1, sizeof(ufbxt_jmp_buf));
					#endif
					if (!ufbxt_setjmp(*t_jmp_buf)) {
						ufbx_load_opts load_opts = { 0 };
						load_opts.read_buffer_size = (size_t)buf_sz;
						ufbx_scene *buf_scene = ufbx_load_file(buf, &load_opts, NULL);
						ufbxt_assert(buf_scene);
						ufbxt_check_scene(buf_scene);
						ufbx_free_scene(buf_scene);
					} else {
						#pragma omp critical(fail_sz)
						{
							fail_sz = buf_sz;
						}
					}
					#if UFBXT_HAS_THREADLOCAL
						free(t_jmp_buf);
						t_jmp_buf = NULL;
					#endif
				}

				if (fail_sz >= 0 && !allow_error) {
					size_t error_size = 256;
					char *error = (char*)malloc(error_size);
					ufbxt_assert(error);
					snprintf(error, error_size, "Failed to parse with: read_buffer_size = %d", fail_sz);
					printf("%s: %s\n", base_name, error);
					ufbxt_assert_fail(__FILE__, __LINE__, error);
				} else {
					fprintf(stderr, "\rFuzzing read buffer size %s: %d/%d\n", base_name, (int)size, (int)size);
				}
			}

			// Ignore geometry, animations, and both

			if (!skip_opts_checks) {
				{
					ufbx_error ignore_error;
					ufbx_load_opts opts = load_opts;
					opts.ignore_geometry = true;
					ufbx_scene *ignore_scene = ufbx_load_memory(data, size, &opts, &ignore_error);
					if (ignore_scene) {
						ufbxt_check_scene(ignore_scene);
						ufbx_free_scene(ignore_scene);
					} else if (!allow_error) {
						ufbxt_log_error(&ignore_error);
						ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file ignoring geometry");
					}
				}

				{
					ufbx_error ignore_error;
					ufbx_load_opts opts = load_opts;
					opts.ignore_animation = true;
					ufbx_scene *ignore_scene = ufbx_load_memory(data, size, &opts, &ignore_error);
					if (ignore_scene) {
						ufbxt_check_scene(ignore_scene);
						ufbx_free_scene(ignore_scene);
					} else if (!allow_error) {
						ufbxt_log_error(&ignore_error);
						ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file ignoring animation");
					}
				}

				{
					ufbx_error ignore_error;
					ufbx_load_opts opts = load_opts;
					opts.ignore_embedded = true;
					ufbx_scene *ignore_scene = ufbx_load_memory(data, size, &opts, &ignore_error);
					if (ignore_scene) {
						ufbxt_check_scene(ignore_scene);
						ufbx_free_scene(ignore_scene);
					} else if (!allow_error) {
						ufbxt_log_error(&ignore_error);
						ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file ignoring embedded files");
					}
				}

				{
					ufbx_error ignore_error;
					ufbx_load_opts opts = load_opts;
					opts.ignore_geometry = true;
					opts.ignore_animation = true;
					opts.ignore_embedded = true;
					ufbx_scene *ignore_scene = ufbx_load_memory(data, size, &opts, &ignore_error);
					if (ignore_scene) {
						ufbxt_check_scene(ignore_scene);
						ufbx_free_scene(ignore_scene);
					} else if (!allow_error) {
						ufbxt_log_error(&ignore_error);
						ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file ignoring everything");
					}
				}

				// Strict mode
				{
					ufbx_load_opts strict_opts = load_opts;
					strict_opts.disable_quirks = true;
					strict_opts.strict = true;
					strict_opts.no_format_from_content = true;
					strict_opts.no_format_from_extension = true;

					ufbx_error strict_error;
					ufbx_scene *strict_scene = ufbx_load_file(buf, &strict_opts, &strict_error);
					if (strict_scene) {
						ufbxt_check_scene(strict_scene);
						ufbxt_assert(strict_scene->metadata.file_format == load_opts.file_format);
						ufbx_free_scene(strict_scene);
					} else if (!allow_error && !allow_strict_error) {
						ufbxt_log_error(&strict_error);
						ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file with strict options");
					}
				}

				// Loose mode
				{
					ufbx_load_opts loose_opts = load_opts;
					loose_opts.allow_missing_vertex_position = true;
					loose_opts.allow_nodes_out_of_root = true;
					loose_opts.allow_null_material = true;
					loose_opts.connect_broken_elements = true;
					loose_opts.generate_missing_normals = true;
					loose_opts.ignore_missing_external_files = true;

					ufbx_error loose_error;
					ufbx_scene *loose_scene = ufbx_load_file(buf, &loose_opts, &loose_error);
					if (loose_scene) {
						ufbxt_check_scene(loose_scene);
						ufbxt_assert(loose_scene->metadata.file_format == load_opts.file_format);
						ufbx_free_scene(loose_scene);
					} else if (!allow_error) {
						ufbxt_log_error(&loose_error);
						ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file with loose options");
					}
				}
			}

			if (scene) {
				ufbxt_logf(".. Loaded in %.2fms: File %.1fkB, temp %.1fkB (%zu allocs), result %.1fkB (%zu allocs)",
					cputime_cpu_delta_to_sec(NULL, load_end - load_begin) * 1e3,
					(double)size * 1e-3,
					(double)scene->metadata.temp_memory_used * 1e-3,
					scene->metadata.temp_allocs,
					(double)scene->metadata.result_memory_used * 1e-3,
					scene->metadata.result_allocs
				);

				if (fi <= 1) {
					ufbxt_assert(scene->metadata.file_format == UFBX_FILE_FORMAT_FBX);
					ufbxt_assert(scene->metadata.ascii == ((fi == 1) ? 1 : 0));
					ufbxt_assert(scene->metadata.version == version);
				} else if (fi == 2) {
					ufbxt_assert(scene->metadata.file_format == UFBX_FILE_FORMAT_MTL);
				} else if (fi == 3) {
					ufbxt_assert(scene->metadata.file_format == UFBX_FILE_FORMAT_OBJ);
				}

				ufbxt_check_scene(scene);
			}

			// Evaluate all the default animation and all stacks

			if (scene) {
				uint64_t eval_begin = cputime_cpu_tick();
				ufbx_scene *state = ufbx_evaluate_scene(scene, &scene->anim, 1.0, NULL, NULL);
				uint64_t eval_end = cputime_cpu_tick();

				ufbxt_assert(state);
				ufbxt_check_scene(state);

				ufbxt_logf(".. Evaluated in %.2fms: File %.1fkB, temp %.1fkB (%zu allocs), result %.1fkB (%zu allocs)",
					cputime_cpu_delta_to_sec(NULL, eval_end - eval_begin) * 1e3,
					(double)size * 1e-3,
					(double)state->metadata.temp_memory_used * 1e-3,
					state->metadata.temp_allocs,
					(double)state->metadata.result_memory_used * 1e-3,
					state->metadata.result_allocs
				);

				ufbx_free_scene(state);
			}

			if (scene) {
				for (size_t i = 1; i < scene->anim_stacks.count; i++) {
					ufbx_scene *state = ufbx_evaluate_scene(scene, &scene->anim_stacks.data[i]->anim, 1.0, NULL, NULL);
					ufbxt_assert(state);
					ufbxt_check_scene(state);
					ufbx_free_scene(state);
				}
			}

			ufbxt_diff_error err = { 0 };

			size_t num_failing_diff_checks = 0;
			if (scene && obj_file && (!alternative || diff_always)) {
				if (expect_diff_fail) {
					ufbxt_begin_expect_fail();
					ufbxt_diff_to_obj(scene, obj_file, &err, false);
					num_failing_diff_checks = ufbxt_end_expect_fail();
				} else {
					ufbxt_diff_to_obj(scene, obj_file, &err, false);
				}
			}

			test_fn(scene, &err, &error);

			if (err.num > 0) {
				ufbx_real avg = err.sum / (ufbx_real)err.num;
				if (expect_diff_fail) {
					ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests, %zu failing as expected)", avg, err.max, err.num, num_failing_diff_checks);
				} else {
					ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", avg, err.max, err.num);
				}
			}

			if (!alternative || fuzz_always) {
				ufbxt_do_fuzz(base_name, data, size, buf, allow_error, UFBX_FILE_FORMAT_UNKNOWN, fuzz_opts);
			}

			if ((!alternative || fuzz_always) && scene && !g_no_fuzz) {
				// Run known buffer size checks
				for (size_t i = 0; i < ufbxt_arraycount(g_buffer_checks); i++) {
					const ufbxt_buffer_check *check = &g_buffer_checks[i];
					if (strcmp(check->name, base_name)) continue;

					ufbxt_logf(".. Read buffer limit %zu");

					ufbx_load_opts load_opts = { 0 };
					load_opts.read_buffer_size = check->read_buffer_size;
					ufbx_scene *buf_scene = ufbx_load_file(buf, &load_opts, &error);
					if (!buf_scene) {
						ufbxt_log_error(&error);
					}
					ufbxt_assert(buf_scene);
					ufbxt_check_scene(buf_scene);
					ufbx_free_scene(buf_scene);
				}
			}

			ufbx_free_scene(scene);
			ufbx_free_scene(streamed_scene);

			free(data);

			ufbxt_assert(temp_freed);
			ufbxt_assert(result_freed);

		}
	}

	if (num_opened == 0) {
		ufbxt_assert_fail(__FILE__, __LINE__, "File not found");
	}

	if (obj_scene) {
		ufbx_free_scene(obj_scene);
	}

	free(obj_file);

	if (prev_fuzz_quality > 0) {
		g_fuzz_quality = prev_fuzz_quality;
	}
}

typedef struct ufbxt_inflate_opts {
	size_t fast_bits;
	bool force_fast;
	bool primary;
} ufbxt_inflate_opts;

void ufbxt_do_deflate_test(const char *name, void (*test_fn)(const ufbxt_inflate_opts *opts))
{
	size_t opt = 0;

	{
		ufbxt_inflate_opts opts = { 0 };
		opts.primary = true;
		if (g_deflate_opt == SIZE_MAX || opt == g_deflate_opt) {
			ufbxt_logf("(opt %u) default", opt, opts.fast_bits);
			test_fn(&opts);
		}
		opt++;
	}

	for (uint32_t fast_bits = 1; fast_bits <= 8; fast_bits++) {
		ufbxt_inflate_opts opts = { 0 };
		opts.fast_bits = fast_bits;
		if (g_deflate_opt == SIZE_MAX || opt == g_deflate_opt) {
			ufbxt_logf("(opt %u) fast_bits = %u", opt, fast_bits);
			test_fn(&opts);
		}
		opt++;
	}

	{
		ufbxt_inflate_opts opts = { 0 };
		opts.force_fast = true;
		if (g_deflate_opt == SIZE_MAX || opt == g_deflate_opt) {
			ufbxt_logf("(opt %u) force_fast = true", opt);
			test_fn(&opts);
		}
		opt++;
	}
}

#define UFBXT_IMPL 1
#define UFBXT_TEST(name) void ufbxt_test_fn_##name(void)
#define UFBXT_FILE_TEST_FLAGS(name, flags) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, user_opts, flags); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_PATH_FLAGS(name, path, flags) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(path, &ufbxt_test_fn_imp_file_##name, NULL, user_opts, flags); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_OPTS_FLAGS(name, get_opts, flags) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, get_opts(), flags); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_SUFFIX_FLAGS(name, suffix, flags) void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name##_##suffix(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name##_##suffix, #suffix, user_opts, flags | UFBXT_FILE_TEST_FLAG_ALTERNATIVE); } \
	void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_SUFFIX_OPTS_FLAGS(name, suffix, get_opts, flags) void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name##_##suffix(void) { \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name##_##suffix, #suffix, get_opts(), flags | UFBXT_FILE_TEST_FLAG_ALTERNATIVE); } \
	void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_ALT_FLAGS(name, file, flags) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#file, &ufbxt_test_fn_imp_file_##name, NULL, user_opts, flags | UFBXT_FILE_TEST_FLAG_ALTERNATIVE); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_OPTS_ALT_FLAGS(name, file, get_opts, flags) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbxt_do_file_test(#file, &ufbxt_test_fn_imp_file_##name, NULL, get_opts(), flags | UFBXT_FILE_TEST_FLAG_ALTERNATIVE); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_DEFLATE_TEST(name) void ufbxt_test_fn_imp_deflate_##name(const ufbxt_inflate_opts *opts); \
	void ufbxt_test_fn_deflate_##name(void) { \
	ufbxt_do_deflate_test(#name, &ufbxt_test_fn_imp_deflate_##name); } \
	void ufbxt_test_fn_imp_deflate_##name(const ufbxt_inflate_opts *opts)

#define UFBXT_FILE_TEST(name) UFBXT_FILE_TEST_FLAGS(name, 0)
#define UFBXT_FILE_TEST_PATH(name, path) UFBXT_FILE_TEST_PATH_FLAGS(name, path, 0)
#define UFBXT_FILE_TEST_OPTS(name, get_opts) UFBXT_FILE_TEST_OPTS_FLAGS(name, get_opts, 0)
#define UFBXT_FILE_TEST_SUFFIX(name, suffix) UFBXT_FILE_TEST_SUFFIX_FLAGS(name, suffix, 0)
#define UFBXT_FILE_TEST_SUFFIX_OPTS(name, suffix, get_opts) UFBXT_FILE_TEST_SUFFIX_OPTS_FLAGS(name, suffix, get_opts, 0)
#define UFBXT_FILE_TEST_ALT(name, file) UFBXT_FILE_TEST_ALT_FLAGS(name, file, 0)
#define UFBXT_FILE_TEST_OPTS_ALT(name, file, get_opts) UFBXT_FILE_TEST_OPTS_ALT_FLAGS(name, file, get_opts, 0)

#include "all_tests.h"

#undef UFBXT_IMPL
#undef UFBXT_TEST
#undef UFBXT_FILE_TEST_FLAGS
#undef UFBXT_FILE_TEST_PATH_FLAGS
#undef UFBXT_FILE_TEST_OPTS_FLAGS
#undef UFBXT_FILE_TEST_SUFFIX_FLAGS
#undef UFBXT_FILE_TEST_SUFFIX_OPTS_FLAGS
#undef UFBXT_FILE_TEST_ALT_FLAGS
#undef UFBXT_FILE_TEST_OPTS_ALT_FLAGS
#undef UFBXT_DEFLATE_TEST
#define UFBXT_IMPL 0
#define UFBXT_TEST(name) { UFBXT_TEST_GROUP, #name, &ufbxt_test_fn_##name },
#define UFBXT_FILE_TEST_FLAGS(name, flags) { UFBXT_TEST_GROUP, #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_PATH_FLAGS(name, path, flags) { UFBXT_TEST_GROUP, #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS_FLAGS(name, get_opts, flags) { UFBXT_TEST_GROUP, #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_SUFFIX_FLAGS(name, suffix, flags) { UFBXT_TEST_GROUP, #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_SUFFIX_OPTS_FLAGS(name, suffix, get_opts, flags) { UFBXT_TEST_GROUP, #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_ALT_FLAGS(name, file, flags) { UFBXT_TEST_GROUP, #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS_ALT_FLAGS(name, file, get_opts, flags) { UFBXT_TEST_GROUP, #name, &ufbxt_test_fn_file_##name },
#define UFBXT_DEFLATE_TEST(name) { UFBXT_TEST_GROUP, #name, &ufbxt_test_fn_deflate_##name },

ufbxt_test g_tests[] = {
	#include "all_tests.h"
};

typedef struct {
	const char *name;
	uint32_t num_total;
	uint32_t num_ran;
	uint32_t num_ok;
} ufbxt_test_stats;

ufbxt_test_stats g_test_groups[ufbxt_arraycount(g_tests)];
size_t g_num_groups = 0;

ufbxt_test_stats *ufbxt_get_test_group(const char *name)
{
	for (size_t i = g_num_groups; i > 0; --i) {
		ufbxt_test_stats *group = &g_test_groups[i - 1];
		if (!strcmp(group->name, name)) return group;
	}

	ufbxt_test_stats *group = &g_test_groups[g_num_groups++];
	group->name = name;
	return group;
}

int ufbxt_run_test(ufbxt_test *test)
{
	printf("%s: ", test->name);
	fflush(stdout);

	g_error.stack_size = 0;
	g_hint[0] = '\0';

	g_expect_fail = false;

	g_current_test = test;
	if (!ufbxt_setjmp(g_test_jmp)) {
		g_skip_print_ok = false;
		test->func();
		ufbxt_assert(!g_expect_fail);
		if (!g_skip_print_ok) {
			printf("OK\n");
			fflush(stdout);
		}
		return 1;
	} else {
		if (g_hint[0]) {
			printf("Hint: %s\n", g_hint);
		}
		if (g_error.stack_size) {
			ufbxt_log_error(&g_error);
		}

		return 0;
	}
}

#if defined(UFBXT_STACK_LIMIT)
int ufbxt_thread_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	uint32_t num_tests = ufbxt_arraycount(g_tests);
	uint32_t num_ok = 0;
	const char *test_filter = NULL;
	const char *test_group = NULL;

	cputime_init();

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
			g_verbose = 1;
		}
		if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--test")) {
			if (++i < argc) {
				test_filter = argv[i];
			}
		}
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--data")) {
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
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--format")) {
			if (++i < argc) g_file_version = (uint32_t)atoi(argv[i]);
			if (++i < argc) g_file_type = argv[i];
		}
		if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "--group")) {
			if (++i < argc) {
				test_group = argv[i];
			}
		}

		if (!strcmp(argv[i], "--deflate-opt")) {
			if (++i < argc) g_deflate_opt = (size_t)atoi(argv[i]);
		}

		if (!strcmp(argv[i], "--allow-non-thread-safe")) {
			g_allow_non_thread_safe = true;
		}

		if (!strcmp(argv[i], "--fuzz")) {
			g_fuzz = true;
		}

		if (!strcmp(argv[i], "--sink")) {
			g_sink = true;
		}

		if (!strcmp(argv[i], "--patch-all-byte-values")) {
			g_all_byte_values = true;
		}

		if (!strcmp(argv[i], "--patch-start")) {
			if (++i < argc) g_patch_start = atoi(argv[i]);
		}

		if (!strcmp(argv[i], "--dedicated-allocs")) {
			g_dedicated_allocs = true;
		}

		if (!strcmp(argv[i], "--skip-obj-test")) {
			g_skip_obj_test = true;
		}

		if (!strcmp(argv[i], "--fuzz-no-patch")) {
			g_fuzz_no_patch = true;
		}

		if (!strcmp(argv[i], "--fuzz-no-truncate")) {
			g_fuzz_no_truncate = true;
		}

		if (!strcmp(argv[i], "--fuzz-no-cancel")) {
			g_fuzz_no_cancel = true;
		}

		if (!strcmp(argv[i], "--fuzz-no-buffer")) {
			g_fuzz_no_buffer = true;
		}

		if (!strcmp(argv[i], "--fuzz-quality")) {
			if (++i < argc) g_fuzz_quality = atoi(argv[i]);
			if (g_fuzz_quality < 1) g_fuzz_quality = 1;
			if (g_fuzz_quality > 31) g_fuzz_quality = 31;
		}

		if (!strcmp(argv[i], "--heavy-fuzz-quality")) {
			if (++i < argc) g_heavy_fuzz_quality = atoi(argv[i]);
			if (g_heavy_fuzz_quality < 1) g_heavy_fuzz_quality = 1;
			if (g_heavy_fuzz_quality > 31) g_heavy_fuzz_quality = 31;
		}

		if (!strcmp(argv[i], "--threads")) {
			#if _OPENMP
			if (++i < argc) omp_set_num_threads(atoi(argv[i]));
			#endif
		}

		if (!strcmp(argv[i], "--fuzz-step")) {
			if (++i < argc) g_fuzz_step = (size_t)atoi(argv[i]);
		}

		if (!strcmp(argv[i], "--fuzz-file")) {
			if (++i < argc) g_fuzz_file = (size_t)atoi(argv[i]);
		}

		if (!strcmp(argv[i], "--no-fuzz")) {
			g_no_fuzz = true;
		}
	}

	// Autofill heavy fuzz quality if necessary
	if (g_heavy_fuzz_quality < 0) {
		g_heavy_fuzz_quality = g_fuzz_quality - 4;
		if (g_heavy_fuzz_quality < 1) {
			g_heavy_fuzz_quality = 1;
		}
	}

	#ifdef _OPENMP
	if (omp_get_num_threads() > 256) {
		omp_set_num_threads(256);
	}
	#else
	if (g_fuzz) {
		fprintf(stderr, "Fuzzing without threads, compile with OpenMP for better performance!\n");
	}
	#endif

	uint32_t num_ran = 0;
	for (uint32_t i = 0; i < num_tests; i++) {
		ufbxt_test *test = &g_tests[i];
		ufbxt_test_stats *group_stats = ufbxt_get_test_group(test->group);
		group_stats->num_total++;

		if (test_filter && strcmp(test->name, test_filter)) {
			continue;
		}
		if (test_group && strcmp(test->group, test_group)) {
			continue;
		}

		group_stats->num_ran++;
		num_ran++;
		bool print_always = false;
		if (ufbxt_run_test(test)) {
			num_ok++;
			group_stats->num_ok++;
		} else {
			print_always = true;
		}

		ufbxt_log_flush(print_always);
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

	if (g_verbose) {
		size_t num_skipped = 0;
		for (size_t i = 0; i < g_num_groups; i++) {
			ufbxt_test_stats *group = &g_test_groups[i];
			if (group->num_ran == 0) {
				num_skipped++;
				continue;
			}
			printf("  %s: %u/%u\n", group->name, group->num_ok, group->num_ran);
		}
		if (num_skipped > 0) {
			printf("  .. skipped %zu groups\n", num_skipped);
		}
	}

	if (g_fuzz) {
		printf("Fuzz checks:\n\nstatic const ufbxt_fuzz_check g_fuzz_checks[] = {\n");
		for (size_t i = 0; i < ufbxt_arraycount(g_checks); i++) {
			ufbxt_check_line *check = &g_checks[i];
			if (check->step == 0) continue;

			char safe_desc[60];
			size_t safe_desc_len = 0;
			for (const char *c = check->description; *c; c++) {
				if (sizeof(safe_desc) - safe_desc_len < 6) {
					safe_desc[safe_desc_len++] = '.';
					safe_desc[safe_desc_len++] = '.';
					safe_desc[safe_desc_len++] = '.';
					break;
				}
				if (*c == '"' || *c == '\\') {
					safe_desc[safe_desc_len++] = '\\';
				}
				safe_desc[safe_desc_len++] = *c;
			}
			safe_desc[safe_desc_len] = '\0';

			int32_t patch_offset = check->patch_offset != UINT32_MAX ? (int32_t)(check->patch_offset - 1) : -1;

			printf("\t{ \"%s\", %u, %d, %u, %u, %u, %u, %u, \"%s\" },\n", check->test_name,
				(uint32_t)i, patch_offset, (uint32_t)check->patch_value, (uint32_t)check->temp_limit, (uint32_t)check->result_limit, (uint32_t)check->truncate_length,
				(uint32_t)check->cancel_step, safe_desc);

			free(check->test_name);
		}
		printf("};\n");
	}

	if (g_sink) {
		printf("%u\n", ufbxt_sink);
	}

	return num_ok == num_ran ? 0 : 1;
}

#if defined(UFBXT_STACK_LIMIT)

UFBXT_THREAD_ENTRYPOINT
{
	ufbxt_main_return = ufbxt_thread_main(ufbxt_main_argc, ufbxt_main_argv);
	ufbxt_thread_return();
}

int main(int argc, char **argv)
{
	ufbxt_main_argc = argc;
	ufbxt_main_argv = argv;
	bool ok = ufbxt_run_thread();
	if (!ok) {
		fprintf(stderr, "Failed to run thread with stack size of %zu bytes\n", (size_t)(UFBXT_STACK_LIMIT));
		return 1;
	}
	return ufbxt_main_return;
}

#endif

