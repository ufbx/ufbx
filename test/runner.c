#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stdbool.h>
void ufbxt_assert_fail_imp(const char *file, uint32_t line, const char *expr, bool fatal);
static void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr) {
	ufbxt_assert_fail_imp(file, line, expr, true);
}

#include "../ufbx.h"

#if defined(UFBXT_THREADS)
	#define UFBX_OS_IMPLEMENTATION
	#include "../extra/ufbx_os.h"
#endif

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

#if defined(UFBXT_THREADS)
	ufbx_os_thread_pool *g_thread_pool;
#endif

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
	if (err->info_length > 0) {
		ufbxt_logf("Error: %s (%s)", err->description.data, err->info);
	} else {
		ufbxt_logf("Error: %s", err->description.data);
	}
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
// From commit 0abb0bf
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "blender_272_cube_7400_binary", 21323, -1, 0, 0, 151, 0, 0, "ufbxi_push_anim(uc, &uc->scene.anim, ((void *)0), 0)" },
	{ "blender_272_cube_7400_binary", 21323, -1, 0, 0, 302, 0, 0, "ufbxi_push_anim(uc, &uc->scene.anim, ((void *)0), 0)" },
	{ "blender_279_ball_0_obj", 15777, -1, 0, 0, 32, 0, 0, "props.data" },
	{ "blender_279_ball_0_obj", 15777, -1, 0, 0, 64, 0, 0, "props.data" },
	{ "blender_279_ball_0_obj", 15794, -1, 0, 2277, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "blender_279_ball_0_obj", 15794, -1, 0, 249, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "blender_279_ball_0_obj", 15940, -1, 0, 2182, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "blender_279_ball_0_obj", 15940, -1, 0, 240, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "blender_279_ball_0_obj", 16196, -1, 0, 136, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "blender_279_ball_0_obj", 16196, -1, 0, 1403, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "blender_279_ball_0_obj", 16366, -1, 0, 0, 0, 3099, 0, "uc->obj.num_tokens >= 2" },
	{ "blender_279_ball_0_obj", 16369, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "blender_279_ball_0_obj", 16369, -1, 0, 1360, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "blender_279_ball_0_obj", 16384, -1, 0, 124, 0, 0, 0, "material" },
	{ "blender_279_ball_0_obj", 16384, -1, 0, 1362, 0, 0, 0, "material" },
	{ "blender_279_ball_0_obj", 16391, -1, 0, 126, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "blender_279_ball_0_obj", 16391, -1, 0, 1370, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "blender_279_ball_0_obj", 16719, -1, 0, 702, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "blender_279_ball_0_obj", 16734, -1, 0, 0, 0, 54, 0, "uc->obj.num_tokens >= 2" },
	{ "blender_279_ball_0_obj", 16737, -1, 0, 14, 0, 0, 0, "lib.data" },
	{ "blender_279_ball_0_obj", 16737, -1, 0, 698, 0, 0, 0, "lib.data" },
	{ "blender_279_ball_0_obj", 16741, -1, 0, 124, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 16741, -1, 0, 1360, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 16762, -1, 0, 2277, 0, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "blender_279_ball_0_obj", 16762, -1, 0, 249, 0, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "blender_279_ball_0_obj", 16777, -1, 0, 2197, 0, 0, 0, "prop" },
	{ "blender_279_ball_0_obj", 16777, -1, 0, 241, 0, 0, 0, "prop" },
	{ "blender_279_ball_0_obj", 16780, -1, 0, 0, 16, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "blender_279_ball_0_obj", 16780, -1, 0, 2199, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "blender_279_ball_0_obj", 16813, -1, 0, 0, 17, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->va..." },
	{ "blender_279_ball_0_obj", 16813, -1, 0, 2201, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->va..." },
	{ "blender_279_ball_0_obj", 16814, -1, 0, 2203, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &prop->v..." },
	{ "blender_279_ball_0_obj", 16900, -1, 0, 2182, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "blender_279_ball_0_obj", 16900, -1, 0, 240, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "blender_279_ball_0_obj", 16907, -1, 0, 2277, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 16907, -1, 0, 249, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 16908, -1, 0, 2194, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 16916, -1, 0, 2197, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "blender_279_ball_0_obj", 16916, -1, 0, 241, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "blender_279_ball_0_obj", 16920, -1, 0, 0, 34, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 16920, -1, 0, 2347, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 16967, -1, 0, 2178, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "blender_279_ball_0_obj", 16967, -1, 0, 239, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "blender_279_ball_0_obj", 17008, -1, 0, 2182, 0, 0, 0, "ok" },
	{ "blender_279_ball_0_obj", 17008, -1, 0, 240, 0, 0, 0, "ok" },
	{ "blender_279_ball_0_obj", 17011, 55, 99, 0, 0, 0, 0, "ufbxi_obj_load_mtl()" },
	{ "blender_279_ball_0_obj", 17022, 55, 99, 0, 0, 0, 0, "ufbxi_obj_load_mtl(uc)" },
	{ "blender_279_ball_0_obj", 6098, -1, 0, 2182, 0, 0, 0, "new_buffer" },
	{ "blender_279_ball_0_obj", 6098, -1, 0, 240, 0, 0, 0, "new_buffer" },
	{ "blender_279_default_obj", 16407, 481, 48, 0, 0, 0, 0, "min_index < uc->obj.tmp_vertices[attrib].num_items / st..." },
	{ "blender_279_sausage_7400_binary", 13148, 24063, 0, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "blender_279_unicode_6100_ascii", 14808, 432, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "blender_279_uv_sets_6100_ascii", 12793, -1, 0, 0, 63, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 12793, -1, 0, 3166, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 12799, -1, 0, 3168, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 12799, -1, 0, 731, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 12890, -1, 0, 3172, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 12890, -1, 0, 732, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 12893, -1, 0, 3176, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 12893, -1, 0, 734, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 17524, -1, 0, 13150, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "blender_279_uv_sets_6100_ascii", 21113, -1, 0, 13146, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 21113, -1, 0, 3868, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 21120, -1, 0, 13148, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 21120, -1, 0, 3869, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 21121, -1, 0, 13150, 0, 0, 0, "ufbxi_sort_tmp_material_textures(uc, mat_texs, num_mate..." },
	{ "blender_279_uv_sets_6100_ascii", 7118, -1, 0, 3172, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 7118, -1, 0, 732, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 7125, -1, 0, 3174, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 7125, -1, 0, 733, 0, 0, 0, "extra" },
	{ "blender_282_suzanne_and_transform_obj", 16186, -1, 0, 0, 2, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "blender_282_suzanne_and_transform_obj", 16186, -1, 0, 0, 4, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "blender_293_instancing_obj", 15827, -1, 0, 8042, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "blender_293_instancing_obj", 16972, -1, 0, 112296, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_EXT..." },
	{ "blender_293_instancing_obj", 16972, -1, 0, 17228, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_EXT..." },
	{ "blender_293x_nonmanifold_subsurf_obj", 16298, -1, 0, 1148, 0, 0, 0, "ufbxi_obj_parse_indices(uc, begin, window)" },
	{ "blender_293x_nonmanifold_subsurf_obj", 16298, -1, 0, 93, 0, 0, 0, "ufbxi_obj_parse_indices(uc, begin, window)" },
	{ "blender_293x_nonmanifold_subsurf_obj", 16468, -1, 0, 1168, 0, 0, 0, "ufbxi_fix_index(uc, &dst_indices[i], (uint32_t)ix, num_..." },
	{ "blender_293x_nonmanifold_subsurf_obj", 16468, -1, 0, 99, 0, 0, 0, "ufbxi_fix_index(uc, &dst_indices[i], (uint32_t)ix, num_..." },
	{ "blender_293x_nonmanifold_subsurf_obj", 16705, -1, 0, 1148, 0, 0, 0, "ufbxi_obj_parse_multi_indices(uc, 2)" },
	{ "blender_293x_nonmanifold_subsurf_obj", 16705, -1, 0, 93, 0, 0, 0, "ufbxi_obj_parse_multi_indices(uc, 2)" },
	{ "fuzz_0018", 15400, 810, 0, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "fuzz_0070", 4496, -1, 0, 33, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0070", 4496, -1, 0, 756, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0272", 11790, -1, 0, 454, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "fuzz_0393", 11996, -1, 0, 0, 137, 0, 0, "index_data" },
	{ "fuzz_0393", 11996, -1, 0, 0, 274, 0, 0, "index_data" },
	{ "fuzz_0393", 12000, -1, 0, 0, 276, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "fuzz_0393", 12000, -1, 0, 456, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "fuzz_0397", 11838, -1, 0, 0, 138, 0, 0, "new_indices" },
	{ "fuzz_0397", 11838, -1, 0, 0, 276, 0, 0, "new_indices" },
	{ "fuzz_0561", 14054, -1, 0, 453, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "marvelous_quad_7200_binary", 23055, -1, 0, 0, 287, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "max2009_blob_5800_ascii", 10543, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "max2009_blob_5800_ascii", 14445, 164150, 114, 0, 0, 0, 0, "Unknown slope mode" },
	{ "max2009_blob_5800_ascii", 14475, 164903, 98, 0, 0, 0, 0, "Unknown weight mode" },
	{ "max2009_blob_5800_ascii", 14488, 164150, 116, 0, 0, 0, 0, "Unknown key mode" },
	{ "max2009_blob_5800_ascii", 9589, -1, 0, 15600, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 9589, -1, 0, 4422, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 9597, -1, 0, 0, 115, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 9597, -1, 0, 15602, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 9659, 131240, 45, 0, 0, 0, 0, "Bad array dst type" },
	{ "max2009_blob_5800_ascii", 9715, -1, 0, 23254, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 9715, -1, 0, 6998, 0, 0, 0, "v" },
	{ "max2009_blob_5800_binary", 10561, -1, 0, 43, 0, 0, 0, "ufbxi_retain_toplevel(uc, &uc->legacy_node)" },
	{ "max2009_blob_5800_binary", 10561, -1, 0, 979, 0, 0, 0, "ufbxi_retain_toplevel(uc, &uc->legacy_node)" },
	{ "max2009_blob_5800_binary", 11551, -1, 0, 9, 0, 0, 0, "ufbxi_insert_fbx_id(uc, fbx_id, element_id)" },
	{ "max2009_blob_5800_binary", 11572, -1, 0, 3363, 0, 0, 0, "conn" },
	{ "max2009_blob_5800_binary", 11572, -1, 0, 9928, 0, 0, 0, "conn" },
	{ "max2009_blob_5800_binary", 14351, -1, 0, 10086, 0, 0, 0, "curve" },
	{ "max2009_blob_5800_binary", 14351, -1, 0, 3424, 0, 0, 0, "curve" },
	{ "max2009_blob_5800_binary", 14353, -1, 0, 10094, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max2009_blob_5800_binary", 14353, -1, 0, 3426, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max2009_blob_5800_binary", 14358, 119084, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "max2009_blob_5800_binary", 14361, 119104, 255, 0, 0, 0, 0, "curve->keyframes.data" },
	{ "max2009_blob_5800_binary", 14375, 119110, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max2009_blob_5800_binary", 14468, 119110, 16, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "max2009_blob_5800_binary", 14493, 119102, 3, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max2009_blob_5800_binary", 14542, 119102, 1, 0, 0, 0, 0, "data == data_end" },
	{ "max2009_blob_5800_binary", 14563, 114022, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "max2009_blob_5800_binary", 14574, 119084, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "max2009_blob_5800_binary", 14617, -1, 0, 3357, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 14617, -1, 0, 9915, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 14618, -1, 0, 3363, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max2009_blob_5800_binary", 14618, -1, 0, 9928, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max2009_blob_5800_binary", 14621, 119084, 0, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "max2009_blob_5800_binary", 14634, 114858, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "max2009_blob_5800_binary", 14643, 114022, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "max2009_blob_5800_binary", 14706, 114022, 0, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 14768, -1, 0, 3750, 0, 0, 0, "ufbxi_sort_properties(uc, new_props, new_count)" },
	{ "max2009_blob_5800_binary", 15032, -1, 0, 2546, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 15032, -1, 0, 582, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 15040, -1, 0, 0, 139, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 15040, -1, 0, 0, 278, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 15081, -1, 0, 108, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 15081, -1, 0, 1191, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 15088, -1, 0, 0, 43, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 15088, -1, 0, 0, 86, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 15096, -1, 0, 1784, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 15096, -1, 0, 313, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 15103, -1, 0, 0, 186, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 15103, -1, 0, 0, 93, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 15136, -1, 0, 2532, 0, 0, 0, "mesh" },
	{ "max2009_blob_5800_binary", 15136, -1, 0, 577, 0, 0, 0, "mesh" },
	{ "max2009_blob_5800_binary", 15147, 9030, 37, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "max2009_blob_5800_binary", 15157, -1, 0, 0, 1326, 0, 0, "index_data" },
	{ "max2009_blob_5800_binary", 15157, -1, 0, 0, 663, 0, 0, "index_data" },
	{ "max2009_blob_5800_binary", 15180, 58502, 255, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "max2009_blob_5800_binary", 15211, -1, 0, 0, 137, 0, 0, "set" },
	{ "max2009_blob_5800_binary", 15211, -1, 0, 0, 274, 0, 0, "set" },
	{ "max2009_blob_5800_binary", 15215, 65596, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, uv_info, (ufbx_vert..." },
	{ "max2009_blob_5800_binary", 15225, 56645, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MaterialAssignation, \"C\",..." },
	{ "max2009_blob_5800_binary", 15256, 6207, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 15257, -1, 0, 0, 138, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 15257, -1, 0, 2543, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 15258, -1, 0, 2546, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 15258, -1, 0, 582, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 15259, -1, 0, 2554, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 15259, -1, 0, 584, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 15307, 818, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 15308, -1, 0, 0, 42, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 15308, -1, 0, 1176, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 15316, -1, 0, 105, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 15316, -1, 0, 1179, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 15317, -1, 0, 1186, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 15317, -1, 0, 369, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 15320, -1, 0, 106, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "max2009_blob_5800_binary", 15320, -1, 0, 1187, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "max2009_blob_5800_binary", 15325, -1, 0, 107, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 15325, -1, 0, 1189, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 15332, -1, 0, 108, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 15332, -1, 0, 1191, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 15334, -1, 0, 1784, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 15334, -1, 0, 313, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 15338, 6207, 0, 0, 0, 0, 0, "ufbxi_read_legacy_mesh(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 15345, -1, 0, 113, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max2009_blob_5800_binary", 15345, -1, 0, 1202, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max2009_blob_5800_binary", 15354, -1, 0, 2641, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 15354, -1, 0, 609, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 15378, -1, 0, 3, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max2009_blob_5800_binary", 15378, -1, 0, 670, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max2009_blob_5800_binary", 15385, -1, 0, 4, 0, 0, 0, "root" },
	{ "max2009_blob_5800_binary", 15385, -1, 0, 870, 0, 0, 0, "root" },
	{ "max2009_blob_5800_binary", 15387, -1, 0, 10, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 15387, -1, 0, 882, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 15404, 114022, 0, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "max2009_blob_5800_binary", 15406, 818, 0, 0, 0, 0, 0, "ufbxi_read_legacy_model(uc, node)" },
	{ "max2009_blob_5800_binary", 15408, -1, 0, 10960, 0, 0, 0, "ufbxi_read_legacy_settings(uc, node)" },
	{ "max2009_blob_5800_binary", 15408, -1, 0, 3750, 0, 0, 0, "ufbxi_read_legacy_settings(uc, node)" },
	{ "max2009_blob_5800_binary", 15413, -1, 0, 0, 3551, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "max2009_blob_5800_binary", 15413, -1, 0, 0, 7102, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "max2009_blob_5800_binary", 17132, -1, 0, 10986, 0, 0, 0, "pre_anim_values" },
	{ "max2009_blob_5800_binary", 17545, -1, 0, 3754, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max2009_blob_5800_binary", 17662, -1, 0, 3754, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "max2009_blob_5800_binary", 17773, -1, 0, 10999, 0, 0, 0, "new_prop" },
	{ "max2009_blob_5800_binary", 17773, -1, 0, 3755, 0, 0, 0, "new_prop" },
	{ "max2009_blob_5800_binary", 17790, -1, 0, 0, 356, 0, 0, "elem->props.props.data" },
	{ "max2009_blob_5800_binary", 17790, -1, 0, 0, 712, 0, 0, "elem->props.props.data" },
	{ "max2009_blob_5800_binary", 20331, -1, 0, 0, 413, 0, 0, "part->face_indices.data" },
	{ "max2009_blob_5800_binary", 20331, -1, 0, 0, 826, 0, 0, "part->face_indices.data" },
	{ "max2009_blob_5800_binary", 20408, -1, 0, 10999, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "max2009_blob_5800_binary", 20408, -1, 0, 3755, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "max2009_blob_5800_binary", 20796, -1, 0, 0, 411, 0, 0, "materials" },
	{ "max2009_blob_5800_binary", 20796, -1, 0, 0, 822, 0, 0, "materials" },
	{ "max2009_blob_5800_binary", 20838, -1, 0, 0, 413, 0, 0, "ufbxi_finalize_mesh_material(&uc->result, &uc->error, m..." },
	{ "max2009_blob_5800_binary", 20838, -1, 0, 0, 826, 0, 0, "ufbxi_finalize_mesh_material(&uc->result, &uc->error, m..." },
	{ "max2009_blob_5800_binary", 20891, -1, 0, 11239, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "max2009_blob_5800_binary", 20891, -1, 0, 3867, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "max2009_blob_5800_binary", 20909, -1, 0, 11334, 0, 0, 0, "aprop" },
	{ "max2009_blob_5800_binary", 20909, -1, 0, 3899, 0, 0, 0, "aprop" },
	{ "max2009_blob_5800_binary", 8024, -1, 0, 0, 0, 80100, 0, "val" },
	{ "max2009_blob_5800_binary", 8026, 80062, 17, 0, 0, 0, 0, "type == 'S' || type == 'R'" },
	{ "max2009_blob_5800_binary", 8035, 80082, 1, 0, 0, 0, 0, "d->data" },
	{ "max2009_blob_5800_binary", 8041, -1, 0, 0, 118, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max2009_blob_5800_binary", 8041, -1, 0, 2483, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max2009_blob_6100_binary", 11746, -1, 0, 1363, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_6100_binary", 13924, -1, 0, 2122, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 13924, -1, 0, 374, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 13926, -1, 0, 1064, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 13926, -1, 0, 4516, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_cube_texture_5800_binary", 15293, 710, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"S\", &video_info.name)" },
	{ "max2009_cube_texture_5800_binary", 15294, -1, 0, 1066, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &video_info.fbx_id)" },
	{ "max2009_cube_texture_5800_binary", 15294, -1, 0, 71, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &video_info.fbx_id)" },
	{ "max2009_cube_texture_5800_binary", 15297, -1, 0, 1068, 0, 0, 0, "ufbxi_read_video(uc, child, &video_info)" },
	{ "max2009_cube_texture_5800_binary", 15297, -1, 0, 72, 0, 0, 0, "ufbxi_read_video(uc, child, &video_info)" },
	{ "max2009_cube_texture_5800_binary", 15402, 710, 0, 0, 0, 0, 0, "ufbxi_read_legacy_media(uc, node)" },
	{ "max7_blend_cube_5000_binary", 12229, -1, 0, 1740, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 12229, -1, 0, 322, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 14585, -1, 0, 1840, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "max7_blend_cube_5000_binary", 15138, 2350, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "max7_cube_5000_binary", 15365, -1, 0, 1252, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &uc->legacy_implicit_anim_l..." },
	{ "max7_cube_5000_binary", 15365, -1, 0, 138, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &uc->legacy_implicit_anim_l..." },
	{ "max7_cube_5000_binary", 15367, 942, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc..." },
	{ "max7_cube_5000_binary", 15422, -1, 0, 0, 106, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "max7_cube_5000_binary", 15422, -1, 0, 4152, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "max7_cube_5000_binary", 15424, -1, 0, 1225, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 15424, -1, 0, 4154, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 15427, -1, 0, 1230, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &stack_info.fbx_id)" },
	{ "max7_cube_5000_binary", 15427, -1, 0, 4165, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &stack_info.fbx_id)" },
	{ "max7_cube_5000_binary", 15429, -1, 0, 1231, 0, 0, 0, "stack" },
	{ "max7_cube_5000_binary", 15429, -1, 0, 4167, 0, 0, 0, "stack" },
	{ "max7_cube_5000_binary", 15431, -1, 0, 1233, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "max7_cube_5000_binary", 15431, -1, 0, 4175, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "max7_skin_5000_binary", 15050, -1, 0, 1798, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 15050, -1, 0, 346, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 15057, 2420, 136, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "max7_skin_5000_binary", 15068, 4378, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "max7_skin_5000_binary", 15069, 4544, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "max7_skin_5000_binary", 15111, -1, 0, 2226, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 15111, -1, 0, 506, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 15123, -1, 0, 0, 100, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 15123, -1, 0, 0, 50, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 15263, 2361, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max7_skin_5000_binary", 15264, -1, 0, 1797, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max7_skin_5000_binary", 15265, 2420, 136, 0, 0, 0, 0, "ufbxi_read_legacy_link(uc, child, &fbx_id, name.data)" },
	{ "max7_skin_5000_binary", 15268, -1, 0, 1809, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 15268, -1, 0, 351, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 15271, -1, 0, 1811, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 15271, -1, 0, 352, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 15272, -1, 0, 1819, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 15272, -1, 0, 354, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 15274, -1, 0, 1821, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 15274, -1, 0, 355, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 15336, -1, 0, 2226, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max7_skin_5000_binary", 15336, -1, 0, 506, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max_cache_box_7500_binary", 22939, -1, 0, 3034, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 22939, -1, 0, 685, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 23109, -1, 0, 3034, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_cache_box_7500_binary", 23109, -1, 0, 685, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_curve_line_7500_ascii", 13009, 8302, 43, 0, 0, 0, 0, "points->size % 3 == 0" },
	{ "max_curve_line_7500_binary", 13002, -1, 0, 2226, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 13002, -1, 0, 428, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 13007, 13861, 255, 0, 0, 0, 0, "points" },
	{ "max_curve_line_7500_binary", 13008, 13985, 56, 0, 0, 0, 0, "points_index" },
	{ "max_curve_line_7500_binary", 13030, -1, 0, 0, 140, 0, 0, "line->segments.data" },
	{ "max_curve_line_7500_binary", 13030, -1, 0, 0, 280, 0, 0, "line->segments.data" },
	{ "max_curve_line_7500_binary", 14048, 13861, 255, 0, 0, 0, 0, "ufbxi_read_line(uc, node, &info)" },
	{ "max_geometry_transform_6100_binary", 11661, -1, 0, 3273, 0, 0, 0, "geo_node" },
	{ "max_geometry_transform_6100_binary", 11661, -1, 0, 735, 0, 0, 0, "geo_node" },
	{ "max_geometry_transform_6100_binary", 11662, -1, 0, 3282, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max_geometry_transform_6100_binary", 11662, -1, 0, 744, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max_geometry_transform_6100_binary", 11666, -1, 0, 0, 132, 0, 0, "props" },
	{ "max_geometry_transform_6100_binary", 11666, -1, 0, 0, 264, 0, 0, "props" },
	{ "max_geometry_transform_6100_binary", 11677, -1, 0, 3283, 0, 0, 0, "ufbxi_connect_oo(uc, geo_fbx_id, node_fbx_id)" },
	{ "max_geometry_transform_6100_binary", 11677, -1, 0, 738, 0, 0, 0, "ufbxi_connect_oo(uc, geo_fbx_id, node_fbx_id)" },
	{ "max_geometry_transform_6100_binary", 11681, -1, 0, 3285, 0, 0, 0, "extra" },
	{ "max_geometry_transform_6100_binary", 11681, -1, 0, 739, 0, 0, 0, "extra" },
	{ "max_geometry_transform_6100_binary", 17370, -1, 0, 3273, 0, 0, 0, "ufbxi_setup_geometry_transform_helper(uc, node, fbx_id)" },
	{ "max_geometry_transform_6100_binary", 17370, -1, 0, 735, 0, 0, 0, "ufbxi_setup_geometry_transform_helper(uc, node, fbx_id)" },
	{ "max_geometry_transform_6100_binary", 23861, -1, 0, 1, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, str, 0)" },
	{ "max_geometry_transform_6100_binary", 23899, -1, 0, 1, 0, 0, 0, "ufbxi_fixup_opts_string(uc, &uc->opts.geometry_transfor..." },
	{ "max_geometry_transform_types_obj", 17481, -1, 0, 32, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_geometry_transform_types_obj", 17509, -1, 0, 29, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_geometry_transform_types_obj", 17866, -1, 0, 29, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "max_geometry_transform_types_obj", 20445, -1, 0, 32, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "max_quote_6100_ascii", 11458, -1, 0, 1142, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_O..." },
	{ "max_quote_6100_ascii", 11458, -1, 0, 4544, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_O..." },
	{ "max_quote_6100_ascii", 20488, -1, 0, 1401, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 20488, -1, 0, 5484, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 20490, -1, 0, 5486, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 20503, -1, 0, 0, 177, 0, 0, "node->all_attribs.data" },
	{ "max_quote_6100_ascii", 20503, -1, 0, 0, 354, 0, 0, "node->all_attribs.data" },
	{ "max_quote_6100_ascii", 24003, -1, 0, 0, 191, 0, 0, "ufbxi_pop_warnings(&uc->warnings, &uc->scene.metadata.w..." },
	{ "max_quote_6100_ascii", 24003, -1, 0, 0, 382, 0, 0, "ufbxi_pop_warnings(&uc->warnings, &uc->scene.metadata.w..." },
	{ "max_quote_6100_ascii", 4287, -1, 0, 0, 191, 0, 0, "warnings->data" },
	{ "max_quote_6100_ascii", 4287, -1, 0, 0, 382, 0, 0, "warnings->data" },
	{ "max_quote_6100_binary", 12733, 8983, 36, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "max_texture_mapping_6100_binary", 19242, -1, 0, 10838, 0, 0, 0, "copy" },
	{ "max_texture_mapping_6100_binary", 19242, -1, 0, 2734, 0, 0, 0, "copy" },
	{ "max_texture_mapping_6100_binary", 19250, -1, 0, 0, 661, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_texture_mapping_6100_binary", 19250, -1, 0, 10840, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_texture_mapping_6100_binary", 19302, -1, 0, 10838, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 19302, -1, 0, 2734, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 19389, -1, 0, 0, 1320, 0, 0, "shader" },
	{ "max_texture_mapping_6100_binary", 19389, -1, 0, 0, 660, 0, 0, "shader" },
	{ "max_texture_mapping_6100_binary", 19421, -1, 0, 10838, 0, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_texture_mapping_6100_binary", 19421, -1, 0, 2734, 0, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_texture_mapping_6100_binary", 19433, -1, 0, 0, 678, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "max_texture_mapping_6100_binary", 19433, -1, 0, 10942, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "max_texture_mapping_6100_binary", 19476, -1, 0, 10842, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_texture_mapping_6100_binary", 19493, -1, 0, 0, 1324, 0, 0, "shader->inputs.data" },
	{ "max_texture_mapping_6100_binary", 19493, -1, 0, 0, 662, 0, 0, "shader->inputs.data" },
	{ "max_texture_mapping_6100_binary", 19738, -1, 0, 11048, 0, 0, 0, "dst" },
	{ "max_texture_mapping_6100_binary", 19738, -1, 0, 2764, 0, 0, 0, "dst" },
	{ "max_texture_mapping_6100_binary", 19758, -1, 0, 11060, 0, 0, 0, "dst" },
	{ "max_texture_mapping_6100_binary", 19817, -1, 0, 2772, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_texture_mapping_6100_binary", 19828, -1, 0, 11059, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_texture_mapping_6100_binary", 21212, -1, 0, 10838, 0, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_texture_mapping_6100_binary", 21212, -1, 0, 2734, 0, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_texture_mapping_7700_binary", 19279, -1, 0, 2372, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_texture_mapping_7700_binary", 19279, -1, 0, 9559, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "maya_absolute_texture_6100_binary", 15546, -1, 0, 3404, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_ABSOLUTE_FI..." },
	{ "maya_absolute_texture_6100_binary", 15546, -1, 0, 764, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_ABSOLUTE_FI..." },
	{ "maya_arnold_textures_6100_binary", 13741, -1, 0, 6445, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_arnold_textures_6100_binary", 13751, -1, 0, 1528, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 13751, -1, 0, 6301, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 13765, -1, 0, 1530, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 13765, -1, 0, 6309, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 13780, -1, 0, 0, 342, 0, 0, "bindings->prop_bindings.data" },
	{ "maya_arnold_textures_6100_binary", 13780, -1, 0, 0, 684, 0, 0, "bindings->prop_bindings.data" },
	{ "maya_arnold_textures_6100_binary", 13782, -1, 0, 6445, 0, 0, 0, "ufbxi_sort_shader_prop_bindings(uc, bindings->prop_bind..." },
	{ "maya_arnold_textures_6100_binary", 14089, -1, 0, 1361, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 14089, -1, 0, 5511, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 14091, -1, 0, 1528, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 14091, -1, 0, 6301, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 21000, -1, 0, 1770, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_arnold_textures_6100_binary", 21000, -1, 0, 7013, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_auto_clamp_7100_ascii", 9437, -1, 0, 3206, 0, 0, 0, "v" },
	{ "maya_auto_clamp_7100_ascii", 9437, -1, 0, 730, 0, 0, 0, "v" },
	{ "maya_auto_clamp_7100_ascii", 9654, -1, 0, 3225, 0, 0, 0, "v" },
	{ "maya_axes_7700_ascii", 19918, -1, 0, 0, 338, 0, 0, "indices->data" },
	{ "maya_axes_7700_ascii", 19918, -1, 0, 0, 676, 0, 0, "indices->data" },
	{ "maya_axes_7700_ascii", 19943, -1, 0, 0, 338, 0, 0, "ufbxi_flip_attrib_winding(uc, mesh, &mesh->vertex_norma..." },
	{ "maya_axes_7700_ascii", 19943, -1, 0, 0, 676, 0, 0, "ufbxi_flip_attrib_winding(uc, mesh, &mesh->vertex_norma..." },
	{ "maya_axes_7700_ascii", 19971, -1, 0, 15064, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_axes_7700_ascii", 20065, -1, 0, 0, 338, 0, 0, "ufbxi_flip_winding(uc, mesh)" },
	{ "maya_axes_7700_ascii", 20065, -1, 0, 15064, 0, 0, 0, "ufbxi_flip_winding(uc, mesh)" },
	{ "maya_axes_7700_ascii", 23980, -1, 0, 0, 338, 0, 0, "ufbxi_modify_geometry(uc)" },
	{ "maya_axes_7700_ascii", 23980, -1, 0, 15064, 0, 0, 0, "ufbxi_modify_geometry(uc)" },
	{ "maya_axes_anim_7700_ascii", 13098, -1, 0, 17186, 0, 0, 0, "skin" },
	{ "maya_axes_anim_7700_ascii", 13098, -1, 0, 5192, 0, 0, 0, "skin" },
	{ "maya_axes_anim_7700_ascii", 13130, -1, 0, 17370, 0, 0, 0, "cluster" },
	{ "maya_axes_anim_7700_ascii", 13130, -1, 0, 5251, 0, 0, 0, "cluster" },
	{ "maya_axes_anim_7700_ascii", 13136, 79676, 43, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "maya_axes_anim_7700_ascii", 13147, 82700, 43, 0, 0, 0, 0, "transform->size >= 16" },
	{ "maya_axes_anim_7700_ascii", 13721, 75362, 43, 0, 0, 0, 0, "matrix->size >= 16" },
	{ "maya_axes_anim_7700_ascii", 14058, -1, 0, 17186, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "maya_axes_anim_7700_ascii", 14058, -1, 0, 5192, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "maya_axes_anim_7700_ascii", 14060, 79676, 43, 0, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "maya_axes_anim_7700_ascii", 14066, -1, 0, 17966, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_axes_anim_7700_ascii", 14066, -1, 0, 5444, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_axes_anim_7700_ascii", 14111, -1, 0, 18162, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_axes_anim_7700_ascii", 14111, -1, 0, 5503, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_axes_anim_7700_ascii", 18246, -1, 0, 26869, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_axes_anim_7700_ascii", 20569, -1, 0, 26863, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "maya_axes_anim_7700_ascii", 20569, -1, 0, 8475, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "maya_axes_anim_7700_ascii", 20614, -1, 0, 0, 1034, 0, 0, "skin->vertices.data" },
	{ "maya_axes_anim_7700_ascii", 20614, -1, 0, 0, 517, 0, 0, "skin->vertices.data" },
	{ "maya_axes_anim_7700_ascii", 20618, -1, 0, 0, 1036, 0, 0, "skin->weights.data" },
	{ "maya_axes_anim_7700_ascii", 20618, -1, 0, 0, 518, 0, 0, "skin->weights.data" },
	{ "maya_axes_anim_7700_ascii", 20675, -1, 0, 26869, 0, 0, 0, "ufbxi_sort_skin_weights(uc, skin)" },
	{ "maya_axes_anim_7700_ascii", 20704, -1, 0, 26872, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_axes_anim_7700_ascii", 20704, -1, 0, 8478, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_axes_anim_7700_ascii", 20705, -1, 0, 26876, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->raw..." },
	{ "maya_axes_anim_7700_ascii", 20705, -1, 0, 8479, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->raw..." },
	{ "maya_axes_anim_7700_ascii", 20842, -1, 0, 26900, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "maya_axes_anim_7700_ascii", 20842, -1, 0, 8489, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "maya_axes_anim_7700_ascii", 20844, -1, 0, 26892, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_axes_anim_7700_ascii", 20844, -1, 0, 8486, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_axes_anim_7700_ascii", 22881, -1, 0, 27979, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->na..." },
	{ "maya_axes_anim_7700_ascii", 22881, -1, 0, 8683, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->na..." },
	{ "maya_axes_anim_7700_ascii", 22885, -1, 0, 27981, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->chan..." },
	{ "maya_axes_anim_7700_ascii", 22898, -1, 0, 27982, 0, 0, 0, "frame" },
	{ "maya_axes_anim_7700_ascii", 22898, -1, 0, 8684, 0, 0, 0, "frame" },
	{ "maya_axes_anim_7700_ascii", 22983, -1, 0, 27973, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_axes_anim_7700_ascii", 22983, -1, 0, 8681, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_axes_anim_7700_ascii", 23004, -1, 0, 27953, 0, 0, 0, "extra" },
	{ "maya_axes_anim_7700_ascii", 23004, -1, 0, 8677, 0, 0, 0, "extra" },
	{ "maya_axes_anim_7700_ascii", 23006, -1, 0, 0, 547, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_axes_anim_7700_ascii", 23006, -1, 0, 27955, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_axes_anim_7700_ascii", 23011, -1, 0, 0, 1100, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_axes_anim_7700_ascii", 23011, -1, 0, 0, 550, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_axes_anim_7700_ascii", 23044, -1, 0, 27965, 0, 0, 0, "cc->channels" },
	{ "maya_axes_anim_7700_ascii", 23044, -1, 0, 8680, 0, 0, 0, "cc->channels" },
	{ "maya_axes_anim_7700_ascii", 23055, -1, 0, 27967, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_axes_anim_7700_ascii", 23056, -1, 0, 0, 551, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_axes_anim_7700_ascii", 23056, -1, 0, 27968, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_axes_anim_7700_ascii", 23072, -1, 0, 27973, 0, 0, 0, "ufbxi_cache_sort_tmp_channels(cc, cc->channels, cc->num..." },
	{ "maya_axes_anim_7700_ascii", 23072, -1, 0, 8681, 0, 0, 0, "ufbxi_cache_sort_tmp_channels(cc, cc->channels, cc->num..." },
	{ "maya_axes_anim_7700_ascii", 23085, -1, 0, 27028, 0, 0, 0, "doc" },
	{ "maya_axes_anim_7700_ascii", 23085, -1, 0, 8543, 0, 0, 0, "doc" },
	{ "maya_axes_anim_7700_ascii", 23089, -1, 0, 27953, 0, 0, 0, "xml_ok" },
	{ "maya_axes_anim_7700_ascii", 23089, -1, 0, 8677, 0, 0, 0, "xml_ok" },
	{ "maya_axes_anim_7700_ascii", 23097, -1, 0, 0, 553, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_axes_anim_7700_ascii", 23097, -1, 0, 27027, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_axes_anim_7700_ascii", 23111, -1, 0, 27979, 0, 0, 0, "ufbxi_cache_load_mc(cc)" },
	{ "maya_axes_anim_7700_ascii", 23111, -1, 0, 8683, 0, 0, 0, "ufbxi_cache_load_mc(cc)" },
	{ "maya_axes_anim_7700_ascii", 23113, -1, 0, 27028, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_axes_anim_7700_ascii", 23113, -1, 0, 8543, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_axes_anim_7700_ascii", 23151, -1, 0, 27975, 0, 0, 0, "name_buf" },
	{ "maya_axes_anim_7700_ascii", 23151, -1, 0, 8682, 0, 0, 0, "name_buf" },
	{ "maya_axes_anim_7700_ascii", 23172, -1, 0, 27977, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename, ((void *)0), &f..." },
	{ "maya_axes_anim_7700_ascii", 23172, -1, 0, 8683, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename, ((void *)0), &f..." },
	{ "maya_axes_anim_7700_ascii", 23236, -1, 0, 28149, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_axes_anim_7700_ascii", 23236, -1, 0, 8727, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_axes_anim_7700_ascii", 23265, -1, 0, 28151, 0, 0, 0, "chan" },
	{ "maya_axes_anim_7700_ascii", 23265, -1, 0, 8728, 0, 0, 0, "chan" },
	{ "maya_axes_anim_7700_ascii", 23310, -1, 0, 0, 1110, 0, 0, "cc->cache.channels.data" },
	{ "maya_axes_anim_7700_ascii", 23310, -1, 0, 0, 555, 0, 0, "cc->cache.channels.data" },
	{ "maya_axes_anim_7700_ascii", 23334, -1, 0, 27025, 0, 0, 0, "filename_data" },
	{ "maya_axes_anim_7700_ascii", 23334, -1, 0, 8542, 0, 0, 0, "filename_data" },
	{ "maya_axes_anim_7700_ascii", 23341, -1, 0, 27027, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, ((void *)0..." },
	{ "maya_axes_anim_7700_ascii", 23341, -1, 0, 8543, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, ((void *)0..." },
	{ "maya_axes_anim_7700_ascii", 23349, -1, 0, 27975, 0, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_axes_anim_7700_ascii", 23349, -1, 0, 8682, 0, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_axes_anim_7700_ascii", 23354, -1, 0, 0, 1108, 0, 0, "cc->cache.frames.data" },
	{ "maya_axes_anim_7700_ascii", 23354, -1, 0, 0, 554, 0, 0, "cc->cache.frames.data" },
	{ "maya_axes_anim_7700_ascii", 23356, -1, 0, 28149, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "maya_axes_anim_7700_ascii", 23356, -1, 0, 8727, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "maya_axes_anim_7700_ascii", 23357, -1, 0, 28151, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_axes_anim_7700_ascii", 23357, -1, 0, 8728, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_axes_anim_7700_ascii", 23361, -1, 0, 0, 1112, 0, 0, "cc->imp" },
	{ "maya_axes_anim_7700_ascii", 23361, -1, 0, 0, 556, 0, 0, "cc->imp" },
	{ "maya_axes_anim_7700_ascii", 23570, -1, 0, 27021, 0, 0, 0, "file" },
	{ "maya_axes_anim_7700_ascii", 23570, -1, 0, 8540, 0, 0, 0, "file" },
	{ "maya_axes_anim_7700_ascii", 23580, -1, 0, 27023, 0, 0, 0, "files" },
	{ "maya_axes_anim_7700_ascii", 23580, -1, 0, 8541, 0, 0, 0, "files" },
	{ "maya_axes_anim_7700_ascii", 23588, -1, 0, 27025, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_axes_anim_7700_ascii", 23588, -1, 0, 8542, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_axes_anim_7700_ascii", 23991, -1, 0, 27021, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_axes_anim_7700_ascii", 23991, -1, 0, 8540, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_axes_anim_7700_ascii", 6569, -1, 0, 27030, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_axes_anim_7700_ascii", 6569, -1, 0, 8544, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_axes_anim_7700_ascii", 6604, -1, 0, 27030, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_axes_anim_7700_ascii", 6604, -1, 0, 8544, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_axes_anim_7700_ascii", 6616, -1, 0, 27053, 0, 0, 0, "ufbxi_xml_push_token_char(xc, '\\0')" },
	{ "maya_axes_anim_7700_ascii", 6688, -1, 0, 27054, 0, 0, 0, "ufbxi_xml_push_token_char(xc, c)" },
	{ "maya_axes_anim_7700_ascii", 6688, -1, 0, 8593, 0, 0, 0, "ufbxi_xml_push_token_char(xc, c)" },
	{ "maya_axes_anim_7700_ascii", 6693, -1, 0, 27056, 0, 0, 0, "ufbxi_xml_push_token_char(xc, '\\0')" },
	{ "maya_axes_anim_7700_ascii", 6697, -1, 0, 27083, 0, 0, 0, "dst->data" },
	{ "maya_axes_anim_7700_ascii", 6697, -1, 0, 8553, 0, 0, 0, "dst->data" },
	{ "maya_axes_anim_7700_ascii", 6714, -1, 0, 27054, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_T..." },
	{ "maya_axes_anim_7700_ascii", 6714, -1, 0, 8593, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_T..." },
	{ "maya_axes_anim_7700_ascii", 6725, -1, 0, 27057, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 6725, -1, 0, 8550, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 6730, -1, 0, 27059, 0, 0, 0, "tag->text.data" },
	{ "maya_axes_anim_7700_ascii", 6730, -1, 0, 8551, 0, 0, 0, "tag->text.data" },
	{ "maya_axes_anim_7700_ascii", 6737, -1, 0, 27354, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_N..." },
	{ "maya_axes_anim_7700_ascii", 6763, -1, 0, 27030, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_axes_anim_7700_ascii", 6763, -1, 0, 8544, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_axes_anim_7700_ascii", 6768, -1, 0, 27061, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 6768, -1, 0, 8552, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 6769, -1, 0, 27063, 0, 0, 0, "ufbxi_xml_read_until(xc, &tag->name, UFBXI_XML_CTYPE_NA..." },
	{ "maya_axes_anim_7700_ascii", 6769, -1, 0, 8553, 0, 0, 0, "ufbxi_xml_read_until(xc, &tag->name, UFBXI_XML_CTYPE_NA..." },
	{ "maya_axes_anim_7700_ascii", 6785, -1, 0, 27108, 0, 0, 0, "attrib" },
	{ "maya_axes_anim_7700_ascii", 6785, -1, 0, 8558, 0, 0, 0, "attrib" },
	{ "maya_axes_anim_7700_ascii", 6786, -1, 0, 27110, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->name, UFBXI_XML_CTYPE..." },
	{ "maya_axes_anim_7700_ascii", 6786, -1, 0, 8559, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->name, UFBXI_XML_CTYPE..." },
	{ "maya_axes_anim_7700_ascii", 6798, -1, 0, 27117, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->value, quote_ctype)" },
	{ "maya_axes_anim_7700_ascii", 6798, -1, 0, 8560, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->value, quote_ctype)" },
	{ "maya_axes_anim_7700_ascii", 6806, -1, 0, 27144, 0, 0, 0, "tag->attribs" },
	{ "maya_axes_anim_7700_ascii", 6806, -1, 0, 8564, 0, 0, 0, "tag->attribs" },
	{ "maya_axes_anim_7700_ascii", 6812, -1, 0, 27085, 0, 0, 0, "ufbxi_xml_parse_tag(xc, depth + 1, &closing, tag->name...." },
	{ "maya_axes_anim_7700_ascii", 6812, -1, 0, 8554, 0, 0, 0, "ufbxi_xml_parse_tag(xc, depth + 1, &closing, tag->name...." },
	{ "maya_axes_anim_7700_ascii", 6818, -1, 0, 27360, 0, 0, 0, "tag->children" },
	{ "maya_axes_anim_7700_ascii", 6818, -1, 0, 8596, 0, 0, 0, "tag->children" },
	{ "maya_axes_anim_7700_ascii", 6827, -1, 0, 27028, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 6827, -1, 0, 8543, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 6833, -1, 0, 27030, 0, 0, 0, "ufbxi_xml_parse_tag(xc, 0, &closing, ((void *)0))" },
	{ "maya_axes_anim_7700_ascii", 6833, -1, 0, 8544, 0, 0, 0, "ufbxi_xml_parse_tag(xc, 0, &closing, ((void *)0))" },
	{ "maya_axes_anim_7700_ascii", 6839, -1, 0, 27949, 0, 0, 0, "tag->children" },
	{ "maya_axes_anim_7700_ascii", 6839, -1, 0, 8675, 0, 0, 0, "tag->children" },
	{ "maya_axes_anim_7700_ascii", 6842, -1, 0, 27951, 0, 0, 0, "xc->doc" },
	{ "maya_axes_anim_7700_ascii", 6842, -1, 0, 8676, 0, 0, 0, "xc->doc" },
	{ "maya_character_6100_binary", 13828, -1, 0, 13788, 0, 0, 0, "character" },
	{ "maya_character_6100_binary", 14104, -1, 0, 13788, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_character_7500_binary", 13828, -1, 0, 22028, 0, 0, 0, "character" },
	{ "maya_character_7500_binary", 14104, -1, 0, 22028, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_color_sets_6100_binary", 12661, -1, 0, 0, 154, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 12661, -1, 0, 0, 77, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 12708, 9966, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 12713, 16081, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 12716, 15524, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_constraint_zoo_6100_binary", 13854, -1, 0, 13073, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 13854, -1, 0, 3533, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 14106, -1, 0, 13073, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 14106, -1, 0, 3533, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 19086, -1, 0, 14825, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 19086, -1, 0, 4033, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 21306, -1, 0, 14825, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 21306, -1, 0, 4033, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 21312, -1, 0, 0, 316, 0, 0, "constraint->targets.data" },
	{ "maya_constraint_zoo_6100_binary", 21312, -1, 0, 0, 632, 0, 0, "constraint->targets.data" },
	{ "maya_cube_6100_ascii", 10386, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_cube_6100_ascii", 10386, -1, 0, 670, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_cube_6100_ascii", 10403, 100, 33, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_cube_6100_ascii", 3526, -1, 0, 1715, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_ascii", 8707, -1, 0, 0, 0, 0, 57, "ufbxi_report_progress(uc)" },
	{ "maya_cube_6100_ascii", 8847, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_cube_6100_ascii", 8847, -1, 0, 670, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_cube_6100_ascii", 8987, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 8987, -1, 0, 670, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 9006, -1, 0, 6, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 9006, -1, 0, 677, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 9034, 514, 0, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_cube_6100_ascii", 9041, 4541, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_cube_6100_ascii", 9117, 12038, 35, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_cube_6100_ascii", 9137, 1138, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 9174, -1, 0, 1758, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9174, -1, 0, 275, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9193, 4998, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 9433, -1, 0, 1715, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9433, -1, 0, 260, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9458, 4870, 46, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 9473, 514, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 9479, 174, 0, 0, 0, 0, 0, "depth == 0" },
	{ "maya_cube_6100_ascii", 9489, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_cube_6100_ascii", 9493, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_cube_6100_ascii", 9493, -1, 0, 672, 0, 0, 0, "name" },
	{ "maya_cube_6100_ascii", 9498, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_cube_6100_ascii", 9498, -1, 0, 673, 0, 0, 0, "node" },
	{ "maya_cube_6100_ascii", 9522, -1, 0, 1709, 0, 0, 0, "arr" },
	{ "maya_cube_6100_ascii", 9522, -1, 0, 257, 0, 0, 0, "arr" },
	{ "maya_cube_6100_ascii", 9539, -1, 0, 1711, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_cube_6100_ascii", 9539, -1, 0, 258, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_cube_6100_ascii", 9543, -1, 0, 1713, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_cube_6100_ascii", 9543, -1, 0, 259, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_cube_6100_ascii", 9574, 4870, 46, 0, 0, 0, 0, "ufbxi_ascii_read_float_array(uc, (char)arr_type, &num_r..." },
	{ "maya_cube_6100_ascii", 9576, 4998, 45, 0, 0, 0, 0, "ufbxi_ascii_read_int_array(uc, (char)arr_type, &num_rea..." },
	{ "maya_cube_6100_ascii", 9621, -1, 0, 0, 3, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_cube_6100_ascii", 9621, -1, 0, 698, 0, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_cube_6100_ascii", 9650, -1, 0, 2413, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9650, -1, 0, 494, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9652, -1, 0, 1787, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9655, -1, 0, 1963, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9683, -1, 0, 1750, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9783, -1, 0, 1810, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_ascii", 9783, -1, 0, 287, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_ascii", 9825, -1, 0, 683, 0, 0, 0, "node->vals" },
	{ "maya_cube_6100_ascii", 9825, -1, 0, 8, 0, 0, 0, "node->vals" },
	{ "maya_cube_6100_ascii", 9835, 174, 11, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_cube_6100_ascii", 9842, -1, 0, 20, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_ascii", 9842, -1, 0, 722, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 10007, -1, 0, 686, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 10007, -1, 0, 9, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 10354, -1, 0, 0, 0, 1, 0, "header" },
	{ "maya_cube_6100_binary", 10393, 0, 76, 0, 0, 0, 0, "uc->version > 0" },
	{ "maya_cube_6100_binary", 10405, 35, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_cube_6100_binary", 10432, 0, 76, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_cube_6100_binary", 10434, 22, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_cube_6100_binary", 10443, -1, 0, 0, 1460, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "maya_cube_6100_binary", 10443, -1, 0, 0, 730, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "maya_cube_6100_binary", 10453, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 10453, -1, 0, 673, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 10457, -1, 0, 5, 0, 0, 0, "ufbxi_retain_toplevel(uc, node)" },
	{ "maya_cube_6100_binary", 10457, -1, 0, 675, 0, 0, 0, "ufbxi_retain_toplevel(uc, node)" },
	{ "maya_cube_6100_binary", 10472, 39, 19, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_cube_6100_binary", 10480, -1, 0, 1041, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 10480, -1, 0, 61, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 10484, -1, 0, 1158, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &node->children[i])" },
	{ "maya_cube_6100_binary", 10484, -1, 0, 94, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &node->children[i])" },
	{ "maya_cube_6100_binary", 10505, 35, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, tmp_buf ? tmp..." },
	{ "maya_cube_6100_binary", 10520, -1, 0, 686, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, dst)" },
	{ "maya_cube_6100_binary", 10520, -1, 0, 9, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, dst)" },
	{ "maya_cube_6100_binary", 10583, -1, 0, 1, 0, 0, 0, "ufbxi_push_string_imp(&uc->string_pool, str->data, str-..." },
	{ "maya_cube_6100_binary", 10869, -1, 0, 41, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_cube_6100_binary", 10869, -1, 0, 787, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_cube_6100_binary", 10873, -1, 0, 789, 0, 0, 0, "pooled" },
	{ "maya_cube_6100_binary", 10876, -1, 0, 791, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 10895, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_cube_6100_binary", 10895, -1, 0, 583, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_cube_6100_binary", 10898, -1, 0, 585, 0, 0, 0, "pooled" },
	{ "maya_cube_6100_binary", 10901, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 10901, -1, 0, 587, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 10985, 1442, 0, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_cube_6100_binary", 11061, -1, 0, 2092, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 11061, -1, 0, 395, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 11103, -1, 0, 0, 116, 0, 0, "props->props.data" },
	{ "maya_cube_6100_binary", 11103, -1, 0, 0, 58, 0, 0, "props->props.data" },
	{ "maya_cube_6100_binary", 11106, 1442, 0, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_cube_6100_binary", 11109, -1, 0, 2092, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_cube_6100_binary", 11109, -1, 0, 395, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_cube_6100_binary", 11152, -1, 0, 0, 96, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_cube_6100_binary", 11152, -1, 0, 2351, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_cube_6100_binary", 11170, 35, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child, ((void *)0))" },
	{ "maya_cube_6100_binary", 11318, 954, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object, ((void *)0))" },
	{ "maya_cube_6100_binary", 11325, -1, 0, 1075, 0, 0, 0, "tmpl" },
	{ "maya_cube_6100_binary", 11325, -1, 0, 73, 0, 0, 0, "tmpl" },
	{ "maya_cube_6100_binary", 11326, 1022, 0, 0, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_cube_6100_binary", 11353, -1, 0, 0, 25, 0, 0, "uc->templates" },
	{ "maya_cube_6100_binary", 11353, -1, 0, 0, 50, 0, 0, "uc->templates" },
	{ "maya_cube_6100_binary", 11405, -1, 0, 2094, 0, 0, 0, "ptr" },
	{ "maya_cube_6100_binary", 11405, -1, 0, 396, 0, 0, 0, "ptr" },
	{ "maya_cube_6100_binary", 11440, -1, 0, 2089, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_cube_6100_binary", 11441, -1, 0, 0, 57, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_cube_6100_binary", 11441, -1, 0, 2090, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_cube_6100_binary", 11453, -1, 0, 47, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 11453, -1, 0, 999, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 11483, -1, 0, 2099, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 11483, -1, 0, 397, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 11498, -1, 0, 42, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_6100_binary", 11498, -1, 0, 989, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_6100_binary", 11499, -1, 0, 43, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_6100_binary", 11499, -1, 0, 991, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_6100_binary", 11500, -1, 0, 44, 0, 0, 0, "((uint64_t*)ufbxi_push_size_copy((&uc->tmp_element_fbx_..." },
	{ "maya_cube_6100_binary", 11500, -1, 0, 993, 0, 0, 0, "((uint64_t*)ufbxi_push_size_copy((&uc->tmp_element_fbx_..." },
	{ "maya_cube_6100_binary", 11504, -1, 0, 45, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 11504, -1, 0, 995, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 11516, -1, 0, 46, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 11516, -1, 0, 997, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 11518, -1, 0, 47, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 11518, -1, 0, 999, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 11530, -1, 0, 2778, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_6100_binary", 11530, -1, 0, 585, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_6100_binary", 11531, -1, 0, 2780, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_6100_binary", 11531, -1, 0, 586, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_6100_binary", 11535, -1, 0, 2782, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 11535, -1, 0, 587, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 11545, -1, 0, 2784, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 11545, -1, 0, 588, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 11550, -1, 0, 2786, 0, 0, 0, "((uint64_t*)ufbxi_push_size_copy((&uc->tmp_element_fbx_..." },
	{ "maya_cube_6100_binary", 11550, -1, 0, 589, 0, 0, 0, "((uint64_t*)ufbxi_push_size_copy((&uc->tmp_element_fbx_..." },
	{ "maya_cube_6100_binary", 11551, -1, 0, 2788, 0, 0, 0, "ufbxi_insert_fbx_id(uc, fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 11562, -1, 0, 2129, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 11562, -1, 0, 409, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 11745, -1, 0, 2131, 0, 0, 0, "elem_node" },
	{ "maya_cube_6100_binary", 11745, -1, 0, 410, 0, 0, 0, "elem_node" },
	{ "maya_cube_6100_binary", 11746, -1, 0, 2141, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 11882, 7295, 255, 0, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 11883, 7448, 71, 0, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_cube_6100_binary", 11899, 7345, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MappingInformationType, \"C..." },
	{ "maya_cube_6100_binary", 11982, 9992, 14, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_cube_6100_binary", 12033, 7377, 14, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_cube_6100_binary", 12090, -1, 0, 2127, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 12097, -1, 0, 2128, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 12260, -1, 0, 0, 124, 0, 0, "mesh->faces.data" },
	{ "maya_cube_6100_binary", 12260, -1, 0, 0, 62, 0, 0, "mesh->faces.data" },
	{ "maya_cube_6100_binary", 12286, 6763, 0, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_cube_6100_binary", 12300, -1, 0, 0, 126, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_6100_binary", 12300, -1, 0, 0, 63, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_6100_binary", 12549, -1, 0, 2115, 0, 0, 0, "mesh" },
	{ "maya_cube_6100_binary", 12549, -1, 0, 405, 0, 0, 0, "mesh" },
	{ "maya_cube_6100_binary", 12570, 6763, 23, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_cube_6100_binary", 12580, -1, 0, 0, 438, 0, 0, "index_data" },
	{ "maya_cube_6100_binary", 12580, -1, 0, 0, 876, 0, 0, "index_data" },
	{ "maya_cube_6100_binary", 12598, 7000, 23, 0, 0, 0, 0, "Non-negated last index" },
	{ "maya_cube_6100_binary", 12607, -1, 0, 0, 122, 0, 0, "edges" },
	{ "maya_cube_6100_binary", 12607, -1, 0, 0, 61, 0, 0, "edges" },
	{ "maya_cube_6100_binary", 12617, 7000, 0, 0, 0, 0, 0, "Edge index out of bounds" },
	{ "maya_cube_6100_binary", 12640, 6763, 0, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "maya_cube_6100_binary", 12655, -1, 0, 2123, 0, 0, 0, "bitangents" },
	{ "maya_cube_6100_binary", 12655, -1, 0, 407, 0, 0, 0, "bitangents" },
	{ "maya_cube_6100_binary", 12656, -1, 0, 2125, 0, 0, 0, "tangents" },
	{ "maya_cube_6100_binary", 12656, -1, 0, 408, 0, 0, 0, "tangents" },
	{ "maya_cube_6100_binary", 12660, -1, 0, 0, 128, 0, 0, "mesh->uv_sets.data" },
	{ "maya_cube_6100_binary", 12660, -1, 0, 0, 64, 0, 0, "mesh->uv_sets.data" },
	{ "maya_cube_6100_binary", 12670, 7295, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 12676, 8164, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 12684, 9038, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 12696, 9906, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 12723, 10525, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cube_6100_binary", 12741, 10799, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cube_6100_binary", 12746, 10846, 255, 0, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_cube_6100_binary", 12776, 7283, 0, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "maya_cube_6100_binary", 12820, 10070, 0, 0, 0, 0, 0, "mesh->uv_sets.count == num_uv" },
	{ "maya_cube_6100_binary", 12822, 8321, 0, 0, 0, 0, 0, "num_bitangents_read == num_bitangents" },
	{ "maya_cube_6100_binary", 12823, 9195, 0, 0, 0, 0, 0, "num_tangents_read == num_tangents" },
	{ "maya_cube_6100_binary", 12885, -1, 0, 2127, 0, 0, 0, "ufbxi_sort_uv_sets(uc, mesh->uv_sets.data, mesh->uv_set..." },
	{ "maya_cube_6100_binary", 12886, -1, 0, 2128, 0, 0, 0, "ufbxi_sort_color_sets(uc, mesh->color_sets.data, mesh->..." },
	{ "maya_cube_6100_binary", 13589, -1, 0, 2561, 0, 0, 0, "material" },
	{ "maya_cube_6100_binary", 13589, -1, 0, 520, 0, 0, 0, "material" },
	{ "maya_cube_6100_binary", 13881, -1, 0, 2094, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "maya_cube_6100_binary", 13881, -1, 0, 396, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "maya_cube_6100_binary", 13887, -1, 0, 0, 59, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_cube_6100_binary", 13887, -1, 0, 2096, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_cube_6100_binary", 13900, -1, 0, 2099, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_cube_6100_binary", 13900, -1, 0, 397, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_cube_6100_binary", 13907, -1, 0, 2101, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_cube_6100_binary", 13907, -1, 0, 398, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_cube_6100_binary", 13917, -1, 0, 0, 120, 0, 0, "attrib_info.props.props.data" },
	{ "maya_cube_6100_binary", 13917, -1, 0, 0, 60, 0, 0, "attrib_info.props.props.data" },
	{ "maya_cube_6100_binary", 13922, 6763, 23, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_cube_6100_binary", 13962, -1, 0, 2129, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_cube_6100_binary", 13962, -1, 0, 409, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_cube_6100_binary", 13968, 15140, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_cube_6100_binary", 13978, 15140, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_cube_6100_binary", 14005, -1, 0, 0, 57, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_cube_6100_binary", 14005, -1, 0, 2089, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_cube_6100_binary", 14008, 1442, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_cube_6100_binary", 14013, 6763, 23, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, type_st..." },
	{ "maya_cube_6100_binary", 14015, -1, 0, 2131, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_cube_6100_binary", 14015, -1, 0, 410, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_cube_6100_binary", 14071, -1, 0, 2561, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_cube_6100_binary", 14071, -1, 0, 520, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_cube_6100_binary", 14109, -1, 0, 0, 96, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cube_6100_binary", 14109, -1, 0, 2351, 0, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cube_6100_binary", 14126, -1, 0, 1126, 0, 0, 0, "uc->p_element_id" },
	{ "maya_cube_6100_binary", 14126, -1, 0, 93, 0, 0, 0, "uc->p_element_id" },
	{ "maya_cube_6100_binary", 14131, 1331, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node, ((void *)0))" },
	{ "maya_cube_6100_binary", 14134, 1442, 0, 0, 0, 0, 0, "ufbxi_read_object(uc, node)" },
	{ "maya_cube_6100_binary", 14263, 16292, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node, ((void *)0))" },
	{ "maya_cube_6100_binary", 14320, -1, 0, 2729, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 14320, -1, 0, 569, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 14666, 16506, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"C\", (char**)&name)" },
	{ "maya_cube_6100_binary", 14690, -1, 0, 2778, 0, 0, 0, "stack" },
	{ "maya_cube_6100_binary", 14690, -1, 0, 585, 0, 0, 0, "stack" },
	{ "maya_cube_6100_binary", 14694, -1, 0, 0, 123, 0, 0, "stack->props.props.data" },
	{ "maya_cube_6100_binary", 14694, -1, 0, 0, 246, 0, 0, "stack->props.props.data" },
	{ "maya_cube_6100_binary", 14697, -1, 0, 2789, 0, 0, 0, "layer" },
	{ "maya_cube_6100_binary", 14697, -1, 0, 590, 0, 0, 0, "layer" },
	{ "maya_cube_6100_binary", 14699, -1, 0, 2797, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_cube_6100_binary", 14699, -1, 0, 592, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_cube_6100_binary", 14716, 16459, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node, ((void *)0))" },
	{ "maya_cube_6100_binary", 14720, 16506, 0, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_cube_6100_binary", 14761, -1, 0, 0, 143, 0, 0, "new_props" },
	{ "maya_cube_6100_binary", 14761, -1, 0, 0, 286, 0, 0, "new_props" },
	{ "maya_cube_6100_binary", 14768, -1, 0, 2926, 0, 0, 0, "ufbxi_sort_properties(uc, new_props, new_count)" },
	{ "maya_cube_6100_binary", 14803, 0, 76, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_cube_6100_binary", 14804, 35, 1, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "maya_cube_6100_binary", 14817, -1, 0, 41, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_cube_6100_binary", 14817, -1, 0, 787, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_cube_6100_binary", 14831, -1, 0, 987, 0, 0, 0, "root_name" },
	{ "maya_cube_6100_binary", 14840, -1, 0, 42, 0, 0, 0, "root" },
	{ "maya_cube_6100_binary", 14840, -1, 0, 989, 0, 0, 0, "root" },
	{ "maya_cube_6100_binary", 14842, -1, 0, 1001, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 14842, -1, 0, 48, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 14846, 59, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_cube_6100_binary", 14847, 954, 1, 0, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_cube_6100_binary", 14850, 954, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_cube_6100_binary", 14854, 0, 0, 0, 0, 0, 0, "uc->top_node" },
	{ "maya_cube_6100_binary", 14859, 1331, 1, 0, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_cube_6100_binary", 14863, 16288, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_cube_6100_binary", 14864, 16292, 1, 0, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_cube_6100_binary", 14867, 16309, 64, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_cube_6100_binary", 14868, 16459, 1, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_cube_6100_binary", 14871, 16470, 65, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_cube_6100_binary", 14881, -1, 0, 0, 143, 0, 0, "ufbxi_read_legacy_settings(uc, settings)" },
	{ "maya_cube_6100_binary", 14881, -1, 0, 2926, 0, 0, 0, "ufbxi_read_legacy_settings(uc, settings)" },
	{ "maya_cube_6100_binary", 15468, -1, 0, 0, 144, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 15468, -1, 0, 2927, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 15469, -1, 0, 2929, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &uc->sce..." },
	{ "maya_cube_6100_binary", 15477, -1, 0, 0, 145, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 15477, -1, 0, 2930, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 15478, -1, 0, 2932, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &uc->sce..." },
	{ "maya_cube_6100_binary", 17102, -1, 0, 2933, 0, 0, 0, "elements" },
	{ "maya_cube_6100_binary", 17106, -1, 0, 2935, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 17109, -1, 0, 2937, 0, 0, 0, "pre_connections" },
	{ "maya_cube_6100_binary", 17112, -1, 0, 2939, 0, 0, 0, "instance_counts" },
	{ "maya_cube_6100_binary", 17115, -1, 0, 2941, 0, 0, 0, "modify_not_supported" },
	{ "maya_cube_6100_binary", 17118, -1, 0, 2942, 0, 0, 0, "has_unscaled_children" },
	{ "maya_cube_6100_binary", 17121, -1, 0, 2943, 0, 0, 0, "has_scale_animation" },
	{ "maya_cube_6100_binary", 17124, -1, 0, 2945, 0, 0, 0, "pre_nodes" },
	{ "maya_cube_6100_binary", 17128, -1, 0, 2947, 0, 0, 0, "pre_meshes" },
	{ "maya_cube_6100_binary", 17135, -1, 0, 2948, 0, 0, 0, "fbx_ids" },
	{ "maya_cube_6100_binary", 17481, -1, 0, 2978, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 17509, -1, 0, 2964, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 17545, -1, 0, 2956, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 17565, -1, 0, 2954, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 17565, -1, 0, 637, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 17569, -1, 0, 0, 148, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_cube_6100_binary", 17569, -1, 0, 0, 296, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_cube_6100_binary", 17660, -1, 0, 0, 149, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_cube_6100_binary", 17660, -1, 0, 0, 298, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_cube_6100_binary", 17662, -1, 0, 2956, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "maya_cube_6100_binary", 17663, -1, 0, 2957, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_dst.da..." },
	{ "maya_cube_6100_binary", 17808, -1, 0, 2958, 0, 0, 0, "node_ids" },
	{ "maya_cube_6100_binary", 17808, -1, 0, 638, 0, 0, 0, "node_ids" },
	{ "maya_cube_6100_binary", 17811, -1, 0, 2960, 0, 0, 0, "node_ptrs" },
	{ "maya_cube_6100_binary", 17811, -1, 0, 639, 0, 0, 0, "node_ptrs" },
	{ "maya_cube_6100_binary", 17822, -1, 0, 2962, 0, 0, 0, "node_offsets" },
	{ "maya_cube_6100_binary", 17822, -1, 0, 640, 0, 0, 0, "node_offsets" },
	{ "maya_cube_6100_binary", 17866, -1, 0, 2964, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "maya_cube_6100_binary", 17870, -1, 0, 2965, 0, 0, 0, "p_offset" },
	{ "maya_cube_6100_binary", 17870, -1, 0, 641, 0, 0, 0, "p_offset" },
	{ "maya_cube_6100_binary", 17952, -1, 0, 2979, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 17952, -1, 0, 647, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 17961, -1, 0, 0, 156, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 17961, -1, 0, 0, 312, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 17988, -1, 0, 2981, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 17988, -1, 0, 648, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 17997, -1, 0, 0, 157, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 17997, -1, 0, 0, 314, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 18069, -1, 0, 2983, 0, 0, 0, "((ufbx_material**)ufbxi_push_size_copy((&uc->tmp_stack)..." },
	{ "maya_cube_6100_binary", 18069, -1, 0, 649, 0, 0, 0, "((ufbx_material**)ufbxi_push_size_copy((&uc->tmp_stack)..." },
	{ "maya_cube_6100_binary", 18079, -1, 0, 0, 160, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 18079, -1, 0, 0, 320, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 18188, -1, 0, 2989, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 18202, -1, 0, 2990, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 20357, -1, 0, 0, 163, 0, 0, "anim" },
	{ "maya_cube_6100_binary", 20357, -1, 0, 0, 326, 0, 0, "anim" },
	{ "maya_cube_6100_binary", 20372, -1, 0, 0, 146, 0, 0, "uc->scene.elements.data" },
	{ "maya_cube_6100_binary", 20372, -1, 0, 0, 292, 0, 0, "uc->scene.elements.data" },
	{ "maya_cube_6100_binary", 20376, -1, 0, 0, 147, 0, 0, "element_data" },
	{ "maya_cube_6100_binary", 20376, -1, 0, 0, 294, 0, 0, "element_data" },
	{ "maya_cube_6100_binary", 20380, -1, 0, 2950, 0, 0, 0, "element_offsets" },
	{ "maya_cube_6100_binary", 20380, -1, 0, 635, 0, 0, 0, "element_offsets" },
	{ "maya_cube_6100_binary", 20401, -1, 0, 2952, 0, 0, 0, "uc->tmp_element_flag" },
	{ "maya_cube_6100_binary", 20401, -1, 0, 636, 0, 0, 0, "uc->tmp_element_flag" },
	{ "maya_cube_6100_binary", 20407, -1, 0, 2954, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_cube_6100_binary", 20407, -1, 0, 637, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_cube_6100_binary", 20409, -1, 0, 2958, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_cube_6100_binary", 20409, -1, 0, 638, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_cube_6100_binary", 20415, -1, 0, 2968, 0, 0, 0, "typed_offsets" },
	{ "maya_cube_6100_binary", 20415, -1, 0, 642, 0, 0, 0, "typed_offsets" },
	{ "maya_cube_6100_binary", 20420, -1, 0, 0, 150, 0, 0, "typed_elems->data" },
	{ "maya_cube_6100_binary", 20420, -1, 0, 0, 300, 0, 0, "typed_elems->data" },
	{ "maya_cube_6100_binary", 20432, -1, 0, 0, 155, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_cube_6100_binary", 20432, -1, 0, 0, 310, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_cube_6100_binary", 20445, -1, 0, 2978, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "maya_cube_6100_binary", 20508, -1, 0, 2979, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_cube_6100_binary", 20508, -1, 0, 647, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_cube_6100_binary", 20556, -1, 0, 2981, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_cube_6100_binary", 20556, -1, 0, 648, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_cube_6100_binary", 20736, -1, 0, 0, 158, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_cube_6100_binary", 20736, -1, 0, 0, 316, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_cube_6100_binary", 20788, -1, 0, 2983, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_cube_6100_binary", 20788, -1, 0, 649, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_cube_6100_binary", 20812, -1, 0, 0, 161, 0, 0, "mesh->material_parts.data" },
	{ "maya_cube_6100_binary", 20812, -1, 0, 0, 322, 0, 0, "mesh->material_parts.data" },
	{ "maya_cube_6100_binary", 20884, -1, 0, 2985, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_cube_6100_binary", 20884, -1, 0, 650, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_cube_6100_binary", 20886, -1, 0, 0, 163, 0, 0, "ufbxi_push_anim(uc, &stack->anim, stack->layers.data, s..." },
	{ "maya_cube_6100_binary", 20886, -1, 0, 0, 326, 0, 0, "ufbxi_push_anim(uc, &stack->anim, stack->layers.data, s..." },
	{ "maya_cube_6100_binary", 20893, -1, 0, 0, 164, 0, 0, "ufbxi_push_anim(uc, &layer->anim, p_layer, 1)" },
	{ "maya_cube_6100_binary", 20893, -1, 0, 0, 328, 0, 0, "ufbxi_push_anim(uc, &layer->anim, p_layer, 1)" },
	{ "maya_cube_6100_binary", 20960, -1, 0, 2987, 0, 0, 0, "aprop" },
	{ "maya_cube_6100_binary", 20960, -1, 0, 651, 0, 0, 0, "aprop" },
	{ "maya_cube_6100_binary", 20964, -1, 0, 0, 165, 0, 0, "layer->anim_props.data" },
	{ "maya_cube_6100_binary", 20964, -1, 0, 0, 330, 0, 0, "layer->anim_props.data" },
	{ "maya_cube_6100_binary", 20966, -1, 0, 2989, 0, 0, 0, "ufbxi_sort_anim_props(uc, layer->anim_props.data, layer..." },
	{ "maya_cube_6100_binary", 21243, -1, 0, 2990, 0, 0, 0, "ufbxi_sort_material_textures(uc, material->textures.dat..." },
	{ "maya_cube_6100_binary", 23874, -1, 0, 2991, 0, 0, 0, "element_ids" },
	{ "maya_cube_6100_binary", 23874, -1, 0, 652, 0, 0, 0, "element_ids" },
	{ "maya_cube_6100_binary", 23927, -1, 0, 1, 0, 0, 0, "ufbxi_load_strings(uc)" },
	{ "maya_cube_6100_binary", 23928, -1, 0, 1, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_cube_6100_binary", 23928, -1, 0, 583, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_cube_6100_binary", 23934, 0, 76, 0, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_cube_6100_binary", 23938, 0, 76, 0, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_cube_6100_binary", 23941, -1, 0, 0, 144, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_6100_binary", 23941, -1, 0, 2927, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_6100_binary", 23958, -1, 0, 2933, 0, 0, 0, "ufbxi_pre_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 23963, -1, 0, 2950, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 23963, -1, 0, 635, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 24004, -1, 0, 2991, 0, 0, 0, "ufbxi_resolve_warning_elements(uc)" },
	{ "maya_cube_6100_binary", 24004, -1, 0, 652, 0, 0, 0, "ufbxi_resolve_warning_elements(uc)" },
	{ "maya_cube_6100_binary", 24017, -1, 0, 0, 166, 0, 0, "imp" },
	{ "maya_cube_6100_binary", 24017, -1, 0, 0, 332, 0, 0, "imp" },
	{ "maya_cube_6100_binary", 3106, 6765, 255, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_cube_6100_binary", 3111, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3156, -1, 0, 1012, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3156, -1, 0, 51, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3212, -1, 0, 673, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3474, -1, 0, 670, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3950, -1, 0, 1, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 4008, -1, 0, 2, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 4572, -1, 0, 702, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "maya_cube_6100_binary", 4597, -1, 0, 703, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 4600, -1, 0, 0, 4, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 4600, -1, 0, 0, 8, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 4614, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "maya_cube_6100_binary", 4640, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 4644, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 4665, -1, 0, 0, 57, 0, 0, "str" },
	{ "maya_cube_6100_binary", 4665, -1, 0, 679, 0, 0, 0, "str" },
	{ "maya_cube_6100_binary", 4683, -1, 0, 2929, 0, 0, 0, "p_blob->data" },
	{ "maya_cube_6100_binary", 6059, -1, 0, 0, 0, 0, 1, "result != UFBX_PROGRESS_CANCEL" },
	{ "maya_cube_6100_binary", 6078, -1, 0, 0, 0, 1, 0, "!uc->eof" },
	{ "maya_cube_6100_binary", 6080, -1, 0, 0, 0, 40, 0, "uc->read_fn || uc->data_size > 0" },
	{ "maya_cube_6100_binary", 6081, 36, 255, 0, 0, 0, 0, "uc->read_fn" },
	{ "maya_cube_6100_binary", 6172, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_cube_6100_binary", 6249, 36, 255, 0, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_cube_6100_binary", 8052, -1, 0, 0, 0, 7040, 0, "val" },
	{ "maya_cube_6100_binary", 8055, -1, 0, 0, 0, 6793, 0, "val" },
	{ "maya_cube_6100_binary", 8093, 10670, 13, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 8094, 7000, 25, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 8097, 6763, 25, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 8118, 6765, 255, 0, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 8209, -1, 0, 0, 0, 27, 0, "header" },
	{ "maya_cube_6100_binary", 8230, 24, 255, 0, 0, 0, 0, "num_values64 <= 0xffffffffui32" },
	{ "maya_cube_6100_binary", 8248, -1, 0, 3, 0, 0, 0, "node" },
	{ "maya_cube_6100_binary", 8248, -1, 0, 670, 0, 0, 0, "node" },
	{ "maya_cube_6100_binary", 8252, -1, 0, 0, 0, 40, 0, "name" },
	{ "maya_cube_6100_binary", 8254, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_cube_6100_binary", 8254, -1, 0, 672, 0, 0, 0, "name" },
	{ "maya_cube_6100_binary", 8272, -1, 0, 1737, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 8272, -1, 0, 263, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 8281, -1, 0, 0, 0, 6780, 0, "data" },
	{ "maya_cube_6100_binary", 8475, 6765, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_binary", 8476, 6763, 25, 0, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_cube_6100_binary", 8490, -1, 0, 6, 0, 0, 0, "vals" },
	{ "maya_cube_6100_binary", 8490, -1, 0, 679, 0, 0, 0, "vals" },
	{ "maya_cube_6100_binary", 8498, -1, 0, 0, 0, 87, 0, "data" },
	{ "maya_cube_6100_binary", 8551, 213, 255, 0, 0, 0, 0, "str" },
	{ "maya_cube_6100_binary", 8561, -1, 0, 0, 4, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_cube_6100_binary", 8561, -1, 0, 702, 0, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_cube_6100_binary", 8576, 164, 0, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_cube_6100_binary", 8581, 22, 1, 0, 0, 0, 0, "Bad value type" },
	{ "maya_cube_6100_binary", 8592, 66, 4, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_cube_6100_binary", 8594, 36, 255, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_cube_6100_binary", 8606, 58, 93, 0, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_cube_6100_binary", 8611, 70, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_cube_6100_binary", 8620, -1, 0, 20, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 8620, -1, 0, 724, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 9487, 0, 76, 0, 0, 0, 0, "Expected a 'Name:' token" },
	{ "maya_cube_6100_binary", 9882, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 9882, -1, 0, 0, 2, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 9883, -1, 0, 5, 0, 0, 0, "((ufbx_dom_node**)ufbxi_push_size_copy((&uc->tmp_dom_no..." },
	{ "maya_cube_6100_binary", 9883, -1, 0, 675, 0, 0, 0, "((ufbx_dom_node**)ufbxi_push_size_copy((&uc->tmp_dom_no..." },
	{ "maya_cube_6100_binary", 9898, -1, 0, 6, 0, 0, 0, "result" },
	{ "maya_cube_6100_binary", 9898, -1, 0, 677, 0, 0, 0, "result" },
	{ "maya_cube_6100_binary", 9904, -1, 0, 679, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst->nam..." },
	{ "maya_cube_6100_binary", 9909, -1, 0, 0, 303, 0, 0, "val" },
	{ "maya_cube_6100_binary", 9909, -1, 0, 0, 606, 0, 0, "val" },
	{ "maya_cube_6100_binary", 9938, -1, 0, 689, 0, 0, 0, "val" },
	{ "maya_cube_6100_binary", 9938, -1, 0, 9, 0, 0, 0, "val" },
	{ "maya_cube_6100_binary", 9955, -1, 0, 0, 3, 0, 0, "dst->values.data" },
	{ "maya_cube_6100_binary", 9955, -1, 0, 0, 6, 0, 0, "dst->values.data" },
	{ "maya_cube_6100_binary", 9960, -1, 0, 28, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 9960, -1, 0, 748, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 9965, -1, 0, 0, 25, 0, 0, "dst->children.data" },
	{ "maya_cube_6100_binary", 9965, -1, 0, 0, 50, 0, 0, "dst->children.data" },
	{ "maya_cube_6100_binary", 9975, -1, 0, 0, 116, 0, 0, "children" },
	{ "maya_cube_6100_binary", 9975, -1, 0, 0, 58, 0, 0, "children" },
	{ "maya_cube_6100_binary", 9982, -1, 0, 5, 0, 0, 0, "ufbxi_retain_dom_node(uc, node, &uc->dom_parse_toplevel..." },
	{ "maya_cube_6100_binary", 9982, -1, 0, 675, 0, 0, 0, "ufbxi_retain_dom_node(uc, node, &uc->dom_parse_toplevel..." },
	{ "maya_cube_6100_binary", 9989, -1, 0, 0, 1462, 0, 0, "nodes" },
	{ "maya_cube_6100_binary", 9989, -1, 0, 0, 731, 0, 0, "nodes" },
	{ "maya_cube_6100_binary", 9992, -1, 0, 0, 1464, 0, 0, "dom_root" },
	{ "maya_cube_6100_binary", 9992, -1, 0, 0, 732, 0, 0, "dom_root" },
	{ "maya_cube_7100_ascii", 9725, 8925, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'I')" },
	{ "maya_cube_7100_ascii", 9729, 8929, 11, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_cube_7100_ascii", 9762, 8935, 33, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_cube_7100_binary", 10988, 6091, 0, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_cube_7100_binary", 11187, 797, 0, 0, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_cube_7100_binary", 11299, 3549, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child, ((void *)0))" },
	{ "maya_cube_7100_binary", 11332, 4105, 0, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_cube_7100_binary", 11344, -1, 0, 0, 58, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_cube_7100_binary", 11344, -1, 0, 1329, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_cube_7100_binary", 11347, 4176, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_cube_7100_binary", 11773, -1, 0, 2976, 0, 0, 0, "elem" },
	{ "maya_cube_7100_binary", 11773, -1, 0, 662, 0, 0, 0, "elem" },
	{ "maya_cube_7100_binary", 13683, -1, 0, 2952, 0, 0, 0, "stack" },
	{ "maya_cube_7100_binary", 13683, -1, 0, 653, 0, 0, 0, "stack" },
	{ "maya_cube_7100_binary", 13689, -1, 0, 2963, 0, 0, 0, "entry" },
	{ "maya_cube_7100_binary", 13689, -1, 0, 658, 0, 0, 0, "entry" },
	{ "maya_cube_7100_binary", 13991, 12333, 255, 0, 0, 0, 0, "(info.fbx_id & (0x8000000000000000ULL)) == 0" },
	{ "maya_cube_7100_binary", 14040, 12362, 0, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_cube_7100_binary", 14079, -1, 0, 2952, 0, 0, 0, "ufbxi_read_anim_stack(uc, node, &info)" },
	{ "maya_cube_7100_binary", 14079, -1, 0, 653, 0, 0, 0, "ufbxi_read_anim_stack(uc, node, &info)" },
	{ "maya_cube_7100_binary", 14081, -1, 0, 2976, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_cube_7100_binary", 14081, -1, 0, 662, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_cube_7100_binary", 14824, 59, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_cube_7100_binary", 14825, 3549, 1, 0, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_cube_7100_binary", 14873, 2241, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "maya_cube_7100_binary", 14877, 19077, 75, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Version5)" },
	{ "maya_cube_7100_binary", 3155, 16067, 1, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_cube_7100_binary", 6154, -1, 0, 0, 0, 0, 1434, "ufbxi_report_progress(uc)" },
	{ "maya_cube_7100_binary", 6277, -1, 0, 0, 0, 12392, 0, "uc->read_fn" },
	{ "maya_cube_7100_binary", 6290, -1, 0, 0, 0, 0, 1434, "ufbxi_resume_progress(uc)" },
	{ "maya_cube_7100_binary", 8318, 12382, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_cube_7100_binary", 8372, 16067, 1, 0, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_7100_binary", 8380, 12379, 99, 0, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_cube_7100_binary", 8396, -1, 0, 0, 0, 12392, 0, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_cube_7100_binary", 8453, 12384, 1, 0, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_cube_7100_binary", 8456, 12384, 255, 0, 0, 0, 0, "Bad array encoding" },
	{ "maya_cube_7400_ascii", 8884, -1, 0, 0, 0, 9568, 0, "c != '\\0'" },
	{ "maya_cube_7400_ascii", 9734, -1, 0, 0, 0, 9568, 0, "ufbxi_ascii_skip_until(uc, '}')" },
	{ "maya_cube_7500_binary", 10545, 24, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_cube_7500_binary", 15395, 24, 0, 0, 0, 0, 0, "ufbxi_parse_legacy_toplevel(uc)" },
	{ "maya_cube_7500_binary", 23936, 24, 0, 0, 0, 0, 0, "ufbxi_read_legacy_root(uc)" },
	{ "maya_cube_big_endian_6100_binary", 10368, -1, 0, 3, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 10368, -1, 0, 670, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 7840, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 7840, -1, 0, 670, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 8222, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 8222, -1, 0, 672, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_7100_binary", 7910, -1, 0, 2344, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 7910, -1, 0, 455, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 8461, -1, 0, 2344, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7100_binary", 8461, -1, 0, 455, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7500_binary", 8213, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_7500_binary", 8213, -1, 0, 672, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_obj", 15729, -1, 0, 0, 12, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_big_endian_obj", 15729, -1, 0, 0, 24, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_big_endian_obj", 15735, -1, 0, 0, 13, 0, 0, "uv_set" },
	{ "maya_cube_big_endian_obj", 15735, -1, 0, 0, 26, 0, 0, "uv_set" },
	{ "maya_cube_big_endian_obj", 15805, -1, 0, 61, 0, 0, 0, "mesh" },
	{ "maya_cube_big_endian_obj", 15805, -1, 0, 976, 0, 0, 0, "mesh" },
	{ "maya_cube_big_endian_obj", 15823, -1, 0, 62, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "maya_cube_big_endian_obj", 15823, -1, 0, 978, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "maya_cube_big_endian_obj", 15827, -1, 0, 996, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 15834, -1, 0, 68, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "maya_cube_big_endian_obj", 15834, -1, 0, 997, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "maya_cube_big_endian_obj", 15835, -1, 0, 69, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "maya_cube_big_endian_obj", 15835, -1, 0, 999, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "maya_cube_big_endian_obj", 15849, -1, 0, 0, 2, 0, 0, "groups" },
	{ "maya_cube_big_endian_obj", 15849, -1, 0, 0, 4, 0, 0, "groups" },
	{ "maya_cube_big_endian_obj", 15889, -1, 0, 3, 0, 0, 0, "root" },
	{ "maya_cube_big_endian_obj", 15889, -1, 0, 670, 0, 0, 0, "root" },
	{ "maya_cube_big_endian_obj", 15891, -1, 0, 682, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 15891, -1, 0, 9, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 15966, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_cube_big_endian_obj", 15972, -1, 0, 1068, 0, 0, 0, "new_data" },
	{ "maya_cube_big_endian_obj", 15972, -1, 0, 83, 0, 0, 0, "new_data" },
	{ "maya_cube_big_endian_obj", 16032, -1, 0, 10, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "maya_cube_big_endian_obj", 16032, -1, 0, 684, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "maya_cube_big_endian_obj", 16068, -1, 0, 1068, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "maya_cube_big_endian_obj", 16068, -1, 0, 83, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "maya_cube_big_endian_obj", 16069, -1, 0, 10, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "maya_cube_big_endian_obj", 16069, -1, 0, 684, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "maya_cube_big_endian_obj", 16087, 92, 33, 0, 0, 0, 0, "offset + read_values <= uc->obj.num_tokens" },
	{ "maya_cube_big_endian_obj", 16091, -1, 0, 15, 0, 0, 0, "vals" },
	{ "maya_cube_big_endian_obj", 16091, -1, 0, 707, 0, 0, 0, "vals" },
	{ "maya_cube_big_endian_obj", 16096, 83, 46, 0, 0, 0, 0, "end == str.data + str.length" },
	{ "maya_cube_big_endian_obj", 16145, -1, 0, 1013, 0, 0, 0, "dst" },
	{ "maya_cube_big_endian_obj", 16145, -1, 0, 76, 0, 0, 0, "dst" },
	{ "maya_cube_big_endian_obj", 16187, -1, 0, 61, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 16187, -1, 0, 976, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 16229, -1, 0, 1001, 0, 0, 0, "entry" },
	{ "maya_cube_big_endian_obj", 16229, -1, 0, 70, 0, 0, 0, "entry" },
	{ "maya_cube_big_endian_obj", 16242, -1, 0, 1003, 0, 0, 0, "group" },
	{ "maya_cube_big_endian_obj", 16242, -1, 0, 71, 0, 0, 0, "group" },
	{ "maya_cube_big_endian_obj", 16261, -1, 0, 1005, 0, 0, 0, "face" },
	{ "maya_cube_big_endian_obj", 16261, -1, 0, 72, 0, 0, 0, "face" },
	{ "maya_cube_big_endian_obj", 16270, -1, 0, 1007, 0, 0, 0, "p_face_mat" },
	{ "maya_cube_big_endian_obj", 16270, -1, 0, 73, 0, 0, 0, "p_face_mat" },
	{ "maya_cube_big_endian_obj", 16275, -1, 0, 1009, 0, 0, 0, "p_face_smooth" },
	{ "maya_cube_big_endian_obj", 16275, -1, 0, 74, 0, 0, 0, "p_face_smooth" },
	{ "maya_cube_big_endian_obj", 16281, -1, 0, 1011, 0, 0, 0, "p_face_group" },
	{ "maya_cube_big_endian_obj", 16281, -1, 0, 75, 0, 0, 0, "p_face_group" },
	{ "maya_cube_big_endian_obj", 16288, -1, 0, 1013, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "maya_cube_big_endian_obj", 16288, -1, 0, 76, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "maya_cube_big_endian_obj", 16411, -1, 0, 0, 3, 0, 0, "data" },
	{ "maya_cube_big_endian_obj", 16411, -1, 0, 0, 6, 0, 0, "data" },
	{ "maya_cube_big_endian_obj", 16437, 71, 102, 0, 0, 0, 0, "num_indices == 0 || !required" },
	{ "maya_cube_big_endian_obj", 16449, -1, 0, 0, 18, 0, 0, "dst_indices" },
	{ "maya_cube_big_endian_obj", 16449, -1, 0, 0, 9, 0, 0, "dst_indices" },
	{ "maya_cube_big_endian_obj", 16494, -1, 0, 1070, 0, 0, 0, "meshes" },
	{ "maya_cube_big_endian_obj", 16494, -1, 0, 84, 0, 0, 0, "meshes" },
	{ "maya_cube_big_endian_obj", 16527, -1, 0, 1072, 0, 0, 0, "tmp_indices" },
	{ "maya_cube_big_endian_obj", 16527, -1, 0, 85, 0, 0, 0, "tmp_indices" },
	{ "maya_cube_big_endian_obj", 16551, -1, 0, 0, 3, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, m..." },
	{ "maya_cube_big_endian_obj", 16551, -1, 0, 0, 6, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, m..." },
	{ "maya_cube_big_endian_obj", 16568, -1, 0, 0, 12, 0, 0, "fbx_mesh->faces.data" },
	{ "maya_cube_big_endian_obj", 16568, -1, 0, 0, 6, 0, 0, "fbx_mesh->faces.data" },
	{ "maya_cube_big_endian_obj", 16569, -1, 0, 0, 14, 0, 0, "fbx_mesh->face_material.data" },
	{ "maya_cube_big_endian_obj", 16569, -1, 0, 0, 7, 0, 0, "fbx_mesh->face_material.data" },
	{ "maya_cube_big_endian_obj", 16574, -1, 0, 0, 16, 0, 0, "fbx_mesh->face_smoothing.data" },
	{ "maya_cube_big_endian_obj", 16574, -1, 0, 0, 8, 0, 0, "fbx_mesh->face_smoothing.data" },
	{ "maya_cube_big_endian_obj", 16588, 71, 102, 0, 0, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 16591, -1, 0, 0, 10, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 16591, -1, 0, 0, 20, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 16594, -1, 0, 0, 11, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 16594, -1, 0, 0, 22, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 16636, -1, 0, 0, 12, 0, 0, "ufbxi_finalize_mesh(&uc->result, &uc->error, fbx_mesh)" },
	{ "maya_cube_big_endian_obj", 16636, -1, 0, 0, 24, 0, 0, "ufbxi_finalize_mesh(&uc->result, &uc->error, fbx_mesh)" },
	{ "maya_cube_big_endian_obj", 16641, -1, 0, 0, 14, 0, 0, "fbx_mesh->face_group_parts.data" },
	{ "maya_cube_big_endian_obj", 16641, -1, 0, 0, 28, 0, 0, "fbx_mesh->face_group_parts.data" },
	{ "maya_cube_big_endian_obj", 16676, -1, 0, 10, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "maya_cube_big_endian_obj", 16676, -1, 0, 684, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "maya_cube_big_endian_obj", 16683, 83, 46, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_POSITION, 1..." },
	{ "maya_cube_big_endian_obj", 16690, 111, 9, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_COLOR, 4)" },
	{ "maya_cube_big_endian_obj", 16697, 328, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_UV, 1)" },
	{ "maya_cube_big_endian_obj", 16699, 622, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_NORMAL, 1)" },
	{ "maya_cube_big_endian_obj", 16701, -1, 0, 61, 0, 0, 0, "ufbxi_obj_parse_indices(uc, 1, uc->obj.num_tokens - 1)" },
	{ "maya_cube_big_endian_obj", 16701, -1, 0, 976, 0, 0, 0, "ufbxi_obj_parse_indices(uc, 1, uc->obj.num_tokens - 1)" },
	{ "maya_cube_big_endian_obj", 16725, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "maya_cube_big_endian_obj", 16725, -1, 0, 701, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "maya_cube_big_endian_obj", 16747, -1, 0, 0, 2, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 16747, -1, 0, 0, 4, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 16748, 71, 102, 0, 0, 0, 0, "ufbxi_obj_pop_meshes(uc)" },
	{ "maya_cube_big_endian_obj", 17019, -1, 0, 3, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "maya_cube_big_endian_obj", 17019, -1, 0, 670, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "maya_cube_big_endian_obj", 17020, 71, 102, 0, 0, 0, 0, "ufbxi_obj_parse_file(uc)" },
	{ "maya_cube_big_endian_obj", 17021, -1, 0, 0, 15, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_big_endian_obj", 17021, -1, 0, 1074, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_big_endian_obj", 23943, 71, 102, 0, 0, 0, 0, "ufbxi_obj_load(uc)" },
	{ "maya_display_layers_6100_binary", 14098, -1, 0, 1541, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_display_layers_6100_binary", 14098, -1, 0, 5767, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_display_layers_6100_binary", 21267, -1, 0, 1705, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_display_layers_6100_binary", 21267, -1, 0, 6286, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_human_ik_6100_binary", 13088, -1, 0, 11600, 0, 0, 0, "marker" },
	{ "maya_human_ik_6100_binary", 13088, -1, 0, 40934, 0, 0, 0, "marker" },
	{ "maya_human_ik_6100_binary", 13952, -1, 0, 18104, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 13952, -1, 0, 60764, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 13954, -1, 0, 11600, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 13954, -1, 0, 40934, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_7400_binary", 14030, -1, 0, 10466, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 14030, -1, 0, 2662, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 14032, -1, 0, 1882, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 14032, -1, 0, 7674, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_interpolation_modes_6100_binary", 14449, 16936, 73, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_interpolation_modes_7500_ascii", 9178, -1, 0, 854, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 9558, -1, 0, 0, 0, 291, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_long_keyframes_6100_binary", 14408, 16558, 0, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_long_keyframes_6100_binary", 14416, 16557, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_node_attribute_zoo_6100_ascii", 9655, -1, 0, 5512, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 9683, -1, 0, 5945, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_binary", 12931, -1, 0, 15110, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 12931, -1, 0, 4160, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 12936, 138209, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->basis..." },
	{ "maya_node_attribute_zoo_6100_binary", 12938, 138308, 255, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 12945, 138359, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 12946, 138416, 1, 0, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 12947, 143462, 27, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 12961, -1, 0, 15314, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 12961, -1, 0, 4229, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 12966, 139478, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 12967, 139592, 1, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"ZZ\", &dimens..." },
	{ "maya_node_attribute_zoo_6100_binary", 12968, 139631, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Step, \"II\", &step_u, &ste..." },
	{ "maya_node_attribute_zoo_6100_binary", 12969, 139664, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 12982, 139691, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 12983, 139727, 1, 0, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 12984, 140321, 3, 0, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 12985, 141818, 63, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 12986, 139655, 1, 0, 0, 0, 0, "points->size / 4 == (size_t)dimension_u * (size_t)dimen..." },
	{ "maya_node_attribute_zoo_6100_binary", 13073, -1, 0, 3219, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 13073, -1, 0, 714, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 13928, -1, 0, 3219, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 13928, -1, 0, 714, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 13930, -1, 0, 1757, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 13930, -1, 0, 277, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 13948, -1, 0, 1971, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 13948, -1, 0, 7568, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 13956, -1, 0, 10441, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 13956, -1, 0, 2799, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 14020, -1, 0, 14166, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 14020, -1, 0, 3897, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 14044, 138209, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 14046, 139478, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 14050, -1, 0, 15741, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 14050, -1, 0, 4371, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 14052, -1, 0, 15896, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 14052, -1, 0, 4416, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 14291, -1, 0, 17512, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &src_prop..." },
	{ "maya_node_attribute_zoo_6100_binary", 14294, -1, 0, 17487, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst_prop..." },
	{ "maya_node_attribute_zoo_6100_binary", 19118, -1, 0, 0, 490, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 19118, -1, 0, 0, 980, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 19161, -1, 0, 0, 1016, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 19161, -1, 0, 0, 508, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 20871, -1, 0, 0, 490, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 20871, -1, 0, 0, 980, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 20876, -1, 0, 0, 499, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 20876, -1, 0, 0, 998, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 20877, -1, 0, 0, 1000, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 20877, -1, 0, 0, 500, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 21316, -1, 0, 0, 1016, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_6100_binary", 21316, -1, 0, 0, 508, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_7500_ascii", 9178, -1, 0, 11793, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 9652, -1, 0, 3373, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 9653, -1, 0, 11794, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 9653, -1, 0, 3360, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_binary", 11440, -1, 0, 0, 325, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_node_attribute_zoo_7500_binary", 13315, -1, 0, 1783, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 13315, -1, 0, 6536, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 13320, 61038, 255, 0, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 13321, 61115, 255, 0, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 13322, 61175, 255, 0, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 13323, 61234, 255, 0, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 13324, 61292, 255, 0, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_binary", 13327, 61122, 0, 0, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_binary", 13332, 61242, 0, 0, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_binary", 13333, 61300, 0, 0, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", 13337, -1, 0, 0, 326, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 13337, -1, 0, 0, 652, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 13364, 61431, 0, 0, 0, 0, 0, "refs_left > 0" },
	{ "maya_node_attribute_zoo_7500_binary", 14018, -1, 0, 2977, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 14018, -1, 0, 657, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 14022, -1, 0, 2728, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 14022, -1, 0, 585, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 14024, -1, 0, 2463, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 14024, -1, 0, 491, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 14026, -1, 0, 3174, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 14026, -1, 0, 708, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 14034, -1, 0, 1162, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 14034, -1, 0, 4529, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 14083, -1, 0, 1796, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 14083, -1, 0, 6575, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 14085, 61038, 255, 0, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", 17741, -1, 0, 2170, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 17741, -1, 0, 7727, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 17788, -1, 0, 2172, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 17788, -1, 0, 7731, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 8095, 61146, 109, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 8096, 61333, 103, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 8448, -1, 0, 0, 0, 0, 2942, "ufbxi_resume_progress(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 8452, -1, 0, 0, 0, 0, 2943, "res != -28" },
	{ "maya_notes_6100_ascii", 8860, -1, 0, 1630, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_notes_6100_ascii", 8860, -1, 0, 236, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_notes_6100_ascii", 9059, -1, 0, 1630, 0, 0, 0, "ufbxi_ascii_push_token_string(uc, token, begin, ufbxi_t..." },
	{ "maya_notes_6100_ascii", 9059, -1, 0, 236, 0, 0, 0, "ufbxi_ascii_push_token_string(uc, token, begin, ((size_..." },
	{ "maya_polygon_hole_6100_binary", 12769, 9377, 37, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_static_no_inherit_scale_7700_ascii", 11704, -1, 0, 2833, 0, 0, 0, "scale_node" },
	{ "maya_static_no_inherit_scale_7700_ascii", 11704, -1, 0, 613, 0, 0, 0, "scale_node" },
	{ "maya_static_no_inherit_scale_7700_ascii", 11705, -1, 0, 2842, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_static_no_inherit_scale_7700_ascii", 11711, -1, 0, 2843, 0, 0, 0, "ufbxi_connect_oo(uc, scale_fbx_id, node_fbx_id)" },
	{ "maya_static_no_inherit_scale_7700_ascii", 11711, -1, 0, 616, 0, 0, 0, "ufbxi_connect_oo(uc, scale_fbx_id, node_fbx_id)" },
	{ "maya_static_no_inherit_scale_7700_ascii", 11715, -1, 0, 2845, 0, 0, 0, "extra" },
	{ "maya_static_no_inherit_scale_7700_ascii", 11715, -1, 0, 617, 0, 0, 0, "extra" },
	{ "maya_static_no_inherit_scale_7700_ascii", 11720, -1, 0, 0, 155, 0, 0, "helper_props" },
	{ "maya_static_no_inherit_scale_7700_ascii", 11720, -1, 0, 0, 310, 0, 0, "helper_props" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17102, -1, 0, 606, 0, 0, 0, "elements" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17106, -1, 0, 607, 0, 0, 0, "tmp_connections" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17109, -1, 0, 608, 0, 0, 0, "pre_connections" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17112, -1, 0, 609, 0, 0, 0, "instance_counts" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17115, -1, 0, 610, 0, 0, 0, "modify_not_supported" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17124, -1, 0, 611, 0, 0, 0, "pre_nodes" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17135, -1, 0, 612, 0, 0, 0, "fbx_ids" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17390, -1, 0, 2833, 0, 0, 0, "ufbxi_setup_scale_helper(uc, node, fbx_id)" },
	{ "maya_static_no_inherit_scale_7700_ascii", 17390, -1, 0, 613, 0, 0, 0, "ufbxi_setup_scale_helper(uc, node, fbx_id)" },
	{ "maya_static_no_inherit_scale_7700_ascii", 23958, -1, 0, 606, 0, 0, 0, "ufbxi_pre_finalize_scene(uc)" },
	{ "maya_texture_layers_6100_binary", 13627, -1, 0, 1462, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 13627, -1, 0, 5576, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 13636, -1, 0, 1466, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 13636, -1, 0, 5586, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 14075, -1, 0, 1462, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 14075, -1, 0, 5576, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 18137, -1, 0, 1680, 0, 0, 0, "((ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_texture_layers_6100_binary", 18137, -1, 0, 6259, 0, 0, 0, "((ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_texture_layers_6100_binary", 18144, -1, 0, 0, 268, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 18144, -1, 0, 0, 536, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 19671, -1, 0, 1690, 0, 0, 0, "textures" },
	{ "maya_texture_layers_6100_binary", 19671, -1, 0, 6282, 0, 0, 0, "textures" },
	{ "maya_texture_layers_6100_binary", 19673, -1, 0, 6284, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_texture_layers_6100_binary", 19747, -1, 0, 1687, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 19747, -1, 0, 6276, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 19767, -1, 0, 1690, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &deps, &..." },
	{ "maya_texture_layers_6100_binary", 19767, -1, 0, 6282, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &deps, &..." },
	{ "maya_texture_layers_6100_binary", 19778, -1, 0, 1691, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 19778, -1, 0, 6285, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 19786, -1, 0, 1694, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &files, ..." },
	{ "maya_texture_layers_6100_binary", 19786, -1, 0, 6291, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &files, ..." },
	{ "maya_texture_layers_6100_binary", 19790, -1, 0, 0, 273, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 19790, -1, 0, 0, 546, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 19817, -1, 0, 6270, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 19821, -1, 0, 1685, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 19821, -1, 0, 6271, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 21219, -1, 0, 1680, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_texture_layers_6100_binary", 21219, -1, 0, 6259, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_textured_cube_6100_binary", 21078, -1, 0, 1673, 0, 0, 0, "mat_texs" },
	{ "maya_textured_cube_6100_binary", 21078, -1, 0, 6311, 0, 0, 0, "mat_texs" },
	{ "maya_transform_animation_6100_binary", 14484, 17549, 11, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "motionbuilder_cube_7700_binary", 14028, -1, 0, 1083, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_..." },
	{ "motionbuilder_cube_7700_binary", 14028, -1, 0, 4611, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_..." },
	{ "motionbuilder_sausage_rrss_7700_binary", 17118, -1, 0, 4290, 0, 0, 0, "has_unscaled_children" },
	{ "motionbuilder_sausage_rrss_7700_binary", 17132, -1, 0, 4293, 0, 0, 0, "pre_anim_values" },
	{ "motionbuilder_tangent_linear_7700_ascii", 9107, -1, 0, 1761, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, replacement)" },
	{ "motionbuilder_tangent_linear_7700_ascii", 9107, -1, 0, 6935, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, replacement)" },
	{ "motionbuilder_thumbnail_7700_ascii", 10951, -1, 0, 7647, 0, 0, 0, "dst" },
	{ "motionbuilder_thumbnail_7700_ascii", 10951, -1, 0, 82928, 0, 0, 0, "dst" },
	{ "motionbuilder_thumbnail_7700_ascii", 10969, -1, 0, 0, 1124, 0, 0, "dst_blob->data" },
	{ "motionbuilder_thumbnail_7700_ascii", 10969, -1, 0, 0, 562, 0, 0, "dst_blob->data" },
	{ "motionbuilder_thumbnail_7700_ascii", 11043, -1, 0, 7647, 0, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "motionbuilder_thumbnail_7700_ascii", 11043, -1, 0, 82928, 0, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "motionbuilder_thumbnail_7700_ascii", 9651, -1, 0, 53, 0, 0, 0, "v" },
	{ "motionbuilder_thumbnail_7700_ascii", 9651, -1, 0, 815, 0, 0, 0, "v" },
	{ "motionbuilder_thumbnail_7700_binary", 11117, 2757, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &thumbnail->props)" },
	{ "motionbuilder_thumbnail_7700_binary", 11156, 2757, 0, 0, 0, 0, 0, "ufbxi_read_thumbnail(uc, thumbnail, &uc->scene.metadata..." },
	{ "motionbuilder_thumbnail_7700_binary", 12044, -1, 0, 1261, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_GEO..." },
	{ "motionbuilder_thumbnail_7700_binary", 12044, -1, 0, 5164, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_GEO..." },
	{ "motionbuilder_thumbnail_7700_binary", 12726, -1, 0, 1261, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing.da..." },
	{ "motionbuilder_thumbnail_7700_binary", 12726, -1, 0, 5164, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing.da..." },
	{ "mtl_fuzz_0000", 16814, -1, 0, 0, 5, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &prop->v..." },
	{ "mtl_fuzz_0000", 4683, -1, 0, 0, 5, 0, 0, "p_blob->data" },
	{ "obj_fuzz_0030", 16713, -1, 0, 31, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "obj_fuzz_0030", 16713, -1, 0, 755, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "revit_empty_7400_binary", 11953, -1, 0, 0, 258, 0, 0, "new_index_data" },
	{ "revit_empty_7400_binary", 11953, -1, 0, 0, 516, 0, 0, "new_index_data" },
	{ "revit_empty_7400_binary", 14113, -1, 0, 3962, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadat..." },
	{ "revit_empty_7400_binary", 14113, -1, 0, 904, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadat..." },
	{ "revit_empty_7400_binary", 8039, -1, 0, 0, 301, 0, 0, "d->data" },
	{ "revit_empty_7400_binary", 8039, -1, 0, 0, 602, 0, 0, "d->data" },
	{ "revit_wall_square_obj", 16125, 3058, 102, 0, 0, 0, 0, "index < 0xffffffffffffffffui64 / 10 - 10" },
	{ "revit_wall_square_obj", 16463, 14288, 102, 0, 0, 0, 0, "ix < 0xffffffffui32" },
	{ "synthetic_binary_props_7500_ascii", 11781, -1, 0, 3820, 0, 0, 0, "unknown" },
	{ "synthetic_binary_props_7500_ascii", 11781, -1, 0, 952, 0, 0, 0, "unknown" },
	{ "synthetic_binary_props_7500_ascii", 11788, -1, 0, 3831, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "synthetic_binary_props_7500_ascii", 11790, -1, 0, 3832, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "synthetic_binary_props_7500_ascii", 14115, -1, 0, 3820, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_binary_props_7500_ascii", 14115, -1, 0, 952, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_binary_props_7500_ascii", 9595, -1, 0, 104, 0, 0, 0, "v->data" },
	{ "synthetic_binary_props_7500_ascii", 9595, -1, 0, 839, 0, 0, 0, "v->data" },
	{ "synthetic_bind_to_root_7700_ascii", 17592, -1, 0, 2024, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_BAD_ELEMENT..." },
	{ "synthetic_bind_to_root_7700_ascii", 17592, -1, 0, 7337, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_BAD_ELEMENT..." },
	{ "synthetic_blend_shape_order_7500_ascii", 12117, -1, 0, 3290, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_blend_shape_order_7500_ascii", 12168, -1, 0, 3288, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 12168, -1, 0, 758, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 12176, -1, 0, 3290, 0, 0, 0, "ufbxi_sort_blend_offsets(uc, offsets, num_offsets)" },
	{ "synthetic_broken_filename_7500_ascii", 13603, -1, 0, 3656, 0, 0, 0, "texture" },
	{ "synthetic_broken_filename_7500_ascii", 13603, -1, 0, 838, 0, 0, 0, "texture" },
	{ "synthetic_broken_filename_7500_ascii", 13656, -1, 0, 3537, 0, 0, 0, "video" },
	{ "synthetic_broken_filename_7500_ascii", 13656, -1, 0, 800, 0, 0, 0, "video" },
	{ "synthetic_broken_filename_7500_ascii", 14073, -1, 0, 3656, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 14073, -1, 0, 838, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 14077, -1, 0, 3537, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 14077, -1, 0, 800, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 15587, -1, 0, 3993, 0, 0, 0, "result" },
	{ "synthetic_broken_filename_7500_ascii", 15587, -1, 0, 945, 0, 0, 0, "result" },
	{ "synthetic_broken_filename_7500_ascii", 15607, -1, 0, 0, 260, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 15607, -1, 0, 3995, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 18045, -1, 0, 3989, 0, 0, 0, "tex" },
	{ "synthetic_broken_filename_7500_ascii", 18045, -1, 0, 943, 0, 0, 0, "tex" },
	{ "synthetic_broken_filename_7500_ascii", 18055, -1, 0, 0, 259, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 18055, -1, 0, 0, 518, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 19607, -1, 0, 4007, 0, 0, 0, "entry" },
	{ "synthetic_broken_filename_7500_ascii", 19607, -1, 0, 949, 0, 0, 0, "entry" },
	{ "synthetic_broken_filename_7500_ascii", 19610, -1, 0, 4009, 0, 0, 0, "file" },
	{ "synthetic_broken_filename_7500_ascii", 19610, -1, 0, 950, 0, 0, 0, "file" },
	{ "synthetic_broken_filename_7500_ascii", 19636, -1, 0, 0, 262, 0, 0, "files" },
	{ "synthetic_broken_filename_7500_ascii", 19636, -1, 0, 0, 524, 0, 0, "files" },
	{ "synthetic_broken_filename_7500_ascii", 19713, -1, 0, 4012, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_broken_filename_7500_ascii", 19713, -1, 0, 951, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_broken_filename_7500_ascii", 19717, -1, 0, 4014, 0, 0, 0, "states" },
	{ "synthetic_broken_filename_7500_ascii", 19717, -1, 0, 952, 0, 0, 0, "states" },
	{ "synthetic_broken_filename_7500_ascii", 19802, -1, 0, 0, 263, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 19802, -1, 0, 0, 526, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 20271, -1, 0, 3993, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 20271, -1, 0, 945, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 21055, -1, 0, 3989, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 21055, -1, 0, 943, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 21167, -1, 0, 3991, 0, 0, 0, "content_videos" },
	{ "synthetic_broken_filename_7500_ascii", 21167, -1, 0, 944, 0, 0, 0, "content_videos" },
	{ "synthetic_broken_filename_7500_ascii", 21172, -1, 0, 3993, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 21172, -1, 0, 945, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 21173, -1, 0, 3997, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 21173, -1, 0, 946, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 21214, -1, 0, 4001, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_broken_filename_7500_ascii", 21214, -1, 0, 947, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_broken_filename_7500_ascii", 21215, -1, 0, 4004, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_broken_filename_7500_ascii", 21215, -1, 0, 948, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_broken_filename_7500_ascii", 21233, -1, 0, 4007, 0, 0, 0, "ufbxi_insert_texture_file(uc, texture)" },
	{ "synthetic_broken_filename_7500_ascii", 21233, -1, 0, 949, 0, 0, 0, "ufbxi_insert_texture_file(uc, texture)" },
	{ "synthetic_broken_filename_7500_ascii", 21237, -1, 0, 0, 262, 0, 0, "ufbxi_pop_texture_files(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 21237, -1, 0, 0, 524, 0, 0, "ufbxi_pop_texture_files(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 21319, -1, 0, 4012, 0, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 21319, -1, 0, 951, 0, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_by_vertex_bad_index_7500_ascii", 11946, -1, 0, 2710, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, new_inde..." },
	{ "synthetic_by_vertex_bad_index_7500_ascii", 11946, -1, 0, 579, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, new_inde..." },
	{ "synthetic_by_vertex_overflow_7500_ascii", 11858, -1, 0, 0, 159, 0, 0, "indices" },
	{ "synthetic_by_vertex_overflow_7500_ascii", 11858, -1, 0, 0, 318, 0, 0, "indices" },
	{ "synthetic_by_vertex_overflow_7500_ascii", 12006, -1, 0, 2703, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, mesh->ve..." },
	{ "synthetic_by_vertex_overflow_7500_ascii", 12006, -1, 0, 577, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, mesh->ve..." },
	{ "synthetic_color_suzanne_0_obj", 16692, -1, 0, 16, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_0_obj", 16692, -1, 0, 707, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_1_obj", 16483, -1, 0, 2187, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "synthetic_cube_nan_6100_ascii", 9011, 4866, 45, 0, 0, 0, 0, "token->type == 'F'" },
	{ "synthetic_direct_by_polygon_7700_ascii", 12013, -1, 0, 0, 161, 0, 0, "new_index_data" },
	{ "synthetic_direct_by_polygon_7700_ascii", 12013, -1, 0, 0, 322, 0, 0, "new_index_data" },
	{ "synthetic_duplicate_id_7700_ascii", 17983, -1, 0, 1366, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_C..." },
	{ "synthetic_duplicate_id_7700_ascii", 17983, -1, 0, 5183, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_C..." },
	{ "synthetic_empty_elements_7500_ascii", 17851, 2800, 49, 0, 0, 0, 0, "depth <= num_nodes" },
	{ "synthetic_empty_face_0_obj", 16215, -1, 0, 21, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_EMPTY_FACE_..." },
	{ "synthetic_empty_face_0_obj", 16215, -1, 0, 716, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_EMPTY_FACE_..." },
	{ "synthetic_face_groups_0_obj", 16719, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "synthetic_geometry_transform_inherit_mode_7700_ascii", 11705, -1, 0, 825, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "synthetic_geometry_transform_inherit_mode_7700_ascii", 17121, -1, 0, 818, 0, 0, 0, "has_scale_animation" },
	{ "synthetic_geometry_transform_inherit_mode_7700_ascii", 17406, -1, 0, 3453, 0, 0, 0, "ufbxi_setup_scale_helper(uc, child, child_fbx_id)" },
	{ "synthetic_geometry_transform_inherit_mode_7700_ascii", 17406, -1, 0, 829, 0, 0, 0, "ufbxi_setup_scale_helper(uc, child, child_fbx_id)" },
	{ "synthetic_indexed_by_vertex_7500_ascii", 11934, -1, 0, 0, 159, 0, 0, "new_index_data" },
	{ "synthetic_indexed_by_vertex_7500_ascii", 11934, -1, 0, 0, 318, 0, 0, "new_index_data" },
	{ "synthetic_legacy_nonzero_material_5800_ascii", 15240, -1, 0, 0, 113, 0, 0, "mesh->face_material.data" },
	{ "synthetic_legacy_nonzero_material_5800_ascii", 15240, -1, 0, 0, 226, 0, 0, "mesh->face_material.data" },
	{ "synthetic_legacy_unquoted_child_fail_5800_ascii", 9719, 1, 33, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_missing_cache_fail_7500_ascii", 23344, 1, 33, 0, 0, 0, 0, "open_file_fn()" },
	{ "synthetic_missing_version_6100_ascii", 13700, -1, 0, 13524, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 13700, -1, 0, 3908, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 13724, -1, 0, 13535, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 13724, -1, 0, 3913, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 13734, -1, 0, 13537, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 13734, -1, 0, 3914, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 13950, -1, 0, 1670, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 13950, -1, 0, 253, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 14087, -1, 0, 13524, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 14087, -1, 0, 3908, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 18239, -1, 0, 15717, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_missing_version_6100_ascii", 20519, -1, 0, 0, 253, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 20519, -1, 0, 0, 506, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 20547, -1, 0, 15717, 0, 0, 0, "ufbxi_sort_bone_poses(uc, pose)" },
	{ "synthetic_mixed_attribs_0_obj", 16687, -1, 0, 1020, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_0_obj", 16687, -1, 0, 88, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 16531, -1, 0, 0, 16, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, 0..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 16531, -1, 0, 0, 32, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, 0..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 16534, -1, 0, 0, 19, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 16534, -1, 0, 0, 38, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 16536, -1, 0, 1147, 0, 0, 0, "color_valid" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 16536, -1, 0, 128, 0, 0, 0, "color_valid" },
	{ "synthetic_node_depth_fail_7400_binary", 8201, 23, 233, 0, 0, 0, 0, "depth < 32" },
	{ "synthetic_node_depth_fail_7500_ascii", 9485, 1, 33, 0, 0, 0, 0, "depth < 32" },
	{ "synthetic_obj_zoo_0_obj", 16703, -1, 0, 40, 0, 0, 0, "ufbxi_obj_parse_multi_indices(uc, 1)" },
	{ "synthetic_obj_zoo_0_obj", 16703, -1, 0, 801, 0, 0, 0, "ufbxi_obj_parse_multi_indices(uc, 1)" },
	{ "synthetic_parent_directory_7700_ascii", 20214, -1, 0, 3996, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_parent_directory_7700_ascii", 20254, -1, 0, 0, 263, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 20254, -1, 0, 3997, 0, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 20268, -1, 0, 0, 263, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_parent_directory_7700_ascii", 20268, -1, 0, 3996, 0, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_partial_attrib_0_obj", 16621, -1, 0, 0, 11, 0, 0, "indices" },
	{ "synthetic_partial_attrib_0_obj", 16621, -1, 0, 0, 22, 0, 0, "indices" },
	{ "synthetic_simple_materials_0_mtl", 16908, -1, 0, 14, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17029, -1, 0, 3, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17029, -1, 0, 670, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17030, -1, 0, 0, 1, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17030, -1, 0, 684, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17031, -1, 0, 10, 0, 0, 0, "ufbxi_obj_parse_mtl(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17031, -1, 0, 690, 0, 0, 0, "ufbxi_obj_parse_mtl(uc)" },
	{ "synthetic_simple_materials_0_mtl", 23946, -1, 0, 3, 0, 0, 0, "ufbxi_mtl_load(uc)" },
	{ "synthetic_simple_materials_0_mtl", 23946, -1, 0, 670, 0, 0, 0, "ufbxi_mtl_load(uc)" },
	{ "synthetic_simple_textures_0_mtl", 16912, -1, 0, 112, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 0)" },
	{ "synthetic_simple_textures_0_mtl", 16912, -1, 0, 1140, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 0)" },
	{ "synthetic_string_collision_7500_ascii", 4614, -1, 0, 2205, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_texture_opts_0_mtl", 16855, -1, 0, 20, 0, 0, 0, "ufbxi_obj_parse_prop(uc, tok, start + 1, 0, &start)" },
	{ "synthetic_texture_opts_0_mtl", 16855, -1, 0, 744, 0, 0, 0, "ufbxi_obj_parse_prop(uc, tok, start + 1, 0, &start)" },
	{ "synthetic_texture_opts_0_mtl", 16865, -1, 0, 0, 22, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tex_str,..." },
	{ "synthetic_texture_split_7500_ascii", 9687, 37963, 35, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12052, -1, 0, 2871, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_TRUNCATED_A..." },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12052, -1, 0, 652, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_TRUNCATED_A..." },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12056, -1, 0, 0, 169, 0, 0, "new_data" },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12056, -1, 0, 0, 338, 0, 0, "new_data" },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12719, -1, 0, 2871, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease.data,..." },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12719, -1, 0, 652, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease.data,..." },
	{ "synthetic_unicode_7500_binary", 4258, -1, 0, 0, 3, 0, 0, "desc_copy" },
	{ "synthetic_unicode_7500_binary", 4258, -1, 0, 0, 6, 0, 0, "desc_copy" },
	{ "synthetic_unicode_7500_binary", 4261, -1, 0, 12, 0, 0, 0, "warning" },
	{ "synthetic_unicode_7500_binary", 4261, -1, 0, 700, 0, 0, 0, "warning" },
	{ "synthetic_unicode_7500_binary", 4469, -1, 0, 12, 0, 0, 0, "ufbxi_warnf_imp(pool->warnings, UFBX_WARNING_BAD_UNICOD..." },
	{ "synthetic_unicode_7500_binary", 4469, -1, 0, 700, 0, 0, 0, "ufbxi_warnf_imp(pool->warnings, UFBX_WARNING_BAD_UNICOD..." },
	{ "synthetic_unicode_7500_binary", 4476, -1, 0, 13, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 4476, -1, 0, 702, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 4572, -1, 0, 1137, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_unicode_7500_binary", 4583, -1, 0, 12, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "synthetic_unicode_7500_binary", 4583, -1, 0, 700, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4485, -1, 0, 3088, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4485, -1, 0, 716, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4626, -1, 0, 3086, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4626, -1, 0, 715, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "synthetic_unsafe_cube_7500_binary", 11813, -1, 0, 2712, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_INDEX_CLAMP..." },
	{ "synthetic_unsafe_cube_7500_binary", 11813, -1, 0, 592, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_INDEX_CLAMP..." },
	{ "synthetic_unsafe_cube_7500_binary", 11861, -1, 0, 2712, 0, 0, 0, "ufbxi_fix_index(uc, &indices[i], ix, num_elems)" },
	{ "synthetic_unsafe_cube_7500_binary", 11861, -1, 0, 592, 0, 0, 0, "ufbxi_fix_index(uc, &indices[i], ix, num_elems)" },
	{ "synthetic_unsafe_cube_7500_binary", 11928, -1, 0, 2712, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "synthetic_unsafe_cube_7500_binary", 11928, -1, 0, 592, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "zbrush_d20_6100_binary", 11583, -1, 0, 3545, 0, 0, 0, "conn" },
	{ "zbrush_d20_6100_binary", 11583, -1, 0, 892, 0, 0, 0, "conn" },
	{ "zbrush_d20_6100_binary", 12130, -1, 0, 3549, 0, 0, 0, "shape" },
	{ "zbrush_d20_6100_binary", 12130, -1, 0, 894, 0, 0, 0, "shape" },
	{ "zbrush_d20_6100_binary", 12138, 25242, 2, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "zbrush_d20_6100_binary", 12139, 25217, 0, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "zbrush_d20_6100_binary", 12152, 25290, 2, 0, 0, 0, 0, "normals && normals->size == vertices->size" },
	{ "zbrush_d20_6100_binary", 12198, 25189, 0, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "zbrush_d20_6100_binary", 12202, -1, 0, 3522, 0, 0, 0, "deformer" },
	{ "zbrush_d20_6100_binary", 12202, -1, 0, 883, 0, 0, 0, "deformer" },
	{ "zbrush_d20_6100_binary", 12203, -1, 0, 3533, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "zbrush_d20_6100_binary", 12203, -1, 0, 888, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "zbrush_d20_6100_binary", 12208, -1, 0, 3535, 0, 0, 0, "channel" },
	{ "zbrush_d20_6100_binary", 12208, -1, 0, 889, 0, 0, 0, "channel" },
	{ "zbrush_d20_6100_binary", 12211, -1, 0, 3543, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_6100_binary", 12211, -1, 0, 891, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_6100_binary", 12215, -1, 0, 0, 102, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 12215, -1, 0, 0, 204, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 12227, -1, 0, 3545, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "zbrush_d20_6100_binary", 12227, -1, 0, 892, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "zbrush_d20_6100_binary", 12238, -1, 0, 3547, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &shape_info.fbx_id)" },
	{ "zbrush_d20_6100_binary", 12238, -1, 0, 893, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &shape_info.fbx_id)" },
	{ "zbrush_d20_6100_binary", 12242, 25217, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "zbrush_d20_6100_binary", 12244, -1, 0, 3560, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "zbrush_d20_6100_binary", 12244, -1, 0, 899, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "zbrush_d20_6100_binary", 12245, -1, 0, 3562, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "zbrush_d20_6100_binary", 12245, -1, 0, 900, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "zbrush_d20_6100_binary", 12387, -1, 0, 0, 136, 0, 0, "ids" },
	{ "zbrush_d20_6100_binary", 12387, -1, 0, 0, 68, 0, 0, "ids" },
	{ "zbrush_d20_6100_binary", 12423, -1, 0, 0, 138, 0, 0, "groups" },
	{ "zbrush_d20_6100_binary", 12423, -1, 0, 0, 69, 0, 0, "groups" },
	{ "zbrush_d20_6100_binary", 12435, -1, 0, 0, 140, 0, 0, "parts" },
	{ "zbrush_d20_6100_binary", 12435, -1, 0, 0, 70, 0, 0, "parts" },
	{ "zbrush_d20_6100_binary", 12553, 25189, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "zbrush_d20_6100_binary", 12762, 8305, 32, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "zbrush_d20_6100_binary", 12881, -1, 0, 0, 136, 0, 0, "ufbxi_assign_face_groups(&uc->result, &uc->error, mesh,..." },
	{ "zbrush_d20_6100_binary", 12881, -1, 0, 0, 68, 0, 0, "ufbxi_assign_face_groups(&uc->result, &uc->error, mesh,..." },
	{ "zbrush_d20_6100_binary", 18093, -1, 0, 1419, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "zbrush_d20_6100_binary", 18093, -1, 0, 5352, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "zbrush_d20_6100_binary", 18101, -1, 0, 0, 271, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 18101, -1, 0, 0, 542, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 18114, -1, 0, 1412, 0, 0, 0, "((ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_s..." },
	{ "zbrush_d20_6100_binary", 18114, -1, 0, 5334, 0, 0, 0, "((ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_s..." },
	{ "zbrush_d20_6100_binary", 18121, -1, 0, 0, 260, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 18121, -1, 0, 0, 520, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 18223, -1, 0, 5384, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "zbrush_d20_6100_binary", 18266, -1, 0, 5336, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "zbrush_d20_6100_binary", 20681, -1, 0, 1409, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 20681, -1, 0, 5326, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 20711, -1, 0, 1411, 0, 0, 0, "full_weights" },
	{ "zbrush_d20_6100_binary", 20711, -1, 0, 5332, 0, 0, 0, "full_weights" },
	{ "zbrush_d20_6100_binary", 20716, -1, 0, 1412, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 20716, -1, 0, 5334, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 20727, -1, 0, 5336, 0, 0, 0, "ufbxi_sort_blend_keyframes(uc, channel->keyframes.data,..." },
	{ "zbrush_d20_6100_binary", 20843, -1, 0, 1418, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 20843, -1, 0, 5350, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 20845, -1, 0, 1419, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "zbrush_d20_6100_binary", 20845, -1, 0, 5352, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "zbrush_d20_6100_binary", 21070, -1, 0, 1424, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 21070, -1, 0, 5364, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 21100, -1, 0, 1425, 0, 0, 0, "mat_tex" },
	{ "zbrush_d20_6100_binary", 21100, -1, 0, 5366, 0, 0, 0, "mat_tex" },
	{ "zbrush_d20_6100_binary", 21134, -1, 0, 0, 277, 0, 0, "texs" },
	{ "zbrush_d20_6100_binary", 21134, -1, 0, 0, 554, 0, 0, "texs" },
	{ "zbrush_d20_6100_binary", 21153, -1, 0, 1428, 0, 0, 0, "tex" },
	{ "zbrush_d20_6100_binary", 21153, -1, 0, 5373, 0, 0, 0, "tex" },
	{ "zbrush_d20_6100_binary", 21180, -1, 0, 5384, 0, 0, 0, "ufbxi_sort_videos_by_filename(uc, content_videos, num_c..." },
	{ "zbrush_d20_7500_ascii", 13673, -1, 0, 0, 258, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "zbrush_d20_7500_ascii", 13673, -1, 0, 0, 516, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "zbrush_d20_7500_binary", 13160, -1, 0, 1067, 0, 0, 0, "channel" },
	{ "zbrush_d20_7500_binary", 13160, -1, 0, 4215, 0, 0, 0, "channel" },
	{ "zbrush_d20_7500_binary", 13168, -1, 0, 1072, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_7500_binary", 13168, -1, 0, 4226, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_7500_binary", 14042, 32981, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 14062, -1, 0, 1055, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "zbrush_d20_7500_binary", 14062, -1, 0, 4178, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "zbrush_d20_7500_binary", 14064, -1, 0, 1067, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 14064, -1, 0, 4215, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 14679, -1, 0, 0, 281, 0, 0, "stack->props.props.data" },
	{ "zbrush_d20_7500_binary", 14679, -1, 0, 0, 562, 0, 0, "stack->props.props.data" },
	{ "zbrush_d20_7500_binary", 17947, -1, 0, 1242, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_C..." },
	{ "zbrush_d20_7500_binary", 17947, -1, 0, 4748, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_C..." },
	{ "zbrush_d20_selection_set_6100_binary", 13792, -1, 0, 1314, 0, 0, 0, "set" },
	{ "zbrush_d20_selection_set_6100_binary", 13792, -1, 0, 4871, 0, 0, 0, "set" },
	{ "zbrush_d20_selection_set_6100_binary", 13809, -1, 0, 3823, 0, 0, 0, "sel" },
	{ "zbrush_d20_selection_set_6100_binary", 13809, -1, 0, 977, 0, 0, 0, "sel" },
	{ "zbrush_d20_selection_set_6100_binary", 14094, -1, 0, 1314, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 14094, -1, 0, 4871, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 14101, -1, 0, 3823, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 14101, -1, 0, 977, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 21272, -1, 0, 2237, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_d20_selection_set_6100_binary", 21272, -1, 0, 7963, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_polygroup_mess_0_obj", 12523, -1, 0, 0, 2038, 0, 0, "face_indices" },
	{ "zbrush_polygroup_mess_0_obj", 12523, -1, 0, 0, 4076, 0, 0, "face_indices" },
	{ "zbrush_polygroup_mess_0_obj", 16581, -1, 0, 0, 2034, 0, 0, "fbx_mesh->face_group.data" },
	{ "zbrush_polygroup_mess_0_obj", 16581, -1, 0, 0, 4068, 0, 0, "fbx_mesh->face_group.data" },
	{ "zbrush_polygroup_mess_0_obj", 16645, -1, 0, 0, 2038, 0, 0, "ufbxi_update_face_groups(&uc->result, &uc->error, fbx_m..." },
	{ "zbrush_polygroup_mess_0_obj", 16645, -1, 0, 0, 4076, 0, 0, "ufbxi_update_face_groups(&uc->result, &uc->error, fbx_m..." },
	{ "zbrush_polygroup_mess_0_obj", 16743, -1, 0, 158798, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_UNKNOWN_OBJ..." },
	{ "zbrush_polygroup_mess_0_obj", 16743, -1, 0, 19663, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_UNKNOWN_OBJ..." },
	{ "zbrush_vertex_color_obj", 15748, -1, 0, 0, 12, 0, 0, "color_set" },
	{ "zbrush_vertex_color_obj", 15748, -1, 0, 0, 24, 0, 0, "color_set" },
	{ "zbrush_vertex_color_obj", 16340, -1, 0, 27, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_obj", 16340, -1, 0, 821, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_obj", 16482, -1, 0, 1018, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_obj", 16482, -1, 0, 73, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_obj", 16483, -1, 0, 1020, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "zbrush_vertex_color_obj", 16497, -1, 0, 1018, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_obj", 16497, -1, 0, 73, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_obj", 16556, -1, 0, 0, 0, 880, 0, "min_ix < 0xffffffffffffffffui64" },
	{ "zbrush_vertex_color_obj", 16557, -1, 0, 0, 10, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "zbrush_vertex_color_obj", 16557, -1, 0, 0, 5, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "zbrush_vertex_color_obj", 16559, -1, 0, 1023, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_obj", 16559, -1, 0, 75, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_obj", 16732, -1, 0, 27, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_obj", 16732, -1, 0, 821, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_obj", 16847, -1, 0, 1051, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_obj", 16847, -1, 0, 78, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_obj", 16865, -1, 0, 1058, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tex_str,..." },
	{ "zbrush_vertex_color_obj", 16866, -1, 0, 1059, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &tex_raw..." },
	{ "zbrush_vertex_color_obj", 16870, -1, 0, 1060, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_obj", 16870, -1, 0, 79, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_obj", 16879, -1, 0, 0, 19, 0, 0, "ufbxi_obj_pop_props(uc, &texture->props.props, num_prop..." },
	{ "zbrush_vertex_color_obj", 16879, -1, 0, 0, 38, 0, 0, "ufbxi_obj_pop_props(uc, &texture->props.props, num_prop..." },
	{ "zbrush_vertex_color_obj", 16885, -1, 0, 0, 20, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop, 0)" },
	{ "zbrush_vertex_color_obj", 16885, -1, 0, 1071, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop, 0)" },
	{ "zbrush_vertex_color_obj", 16888, -1, 0, 1073, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_obj", 16888, -1, 0, 84, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_obj", 16910, -1, 0, 1051, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
	{ "zbrush_vertex_color_obj", 16910, -1, 0, 78, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
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
		prog_opts.load_external_files = true;
		if (file_format == UFBX_FILE_FORMAT_UNKNOWN || file_format == UFBX_FILE_FORMAT_OBJ) {
			prog_opts.ignore_missing_external_files = true;
		}
		prog_opts.filename.data = filename;
		prog_opts.filename.length = SIZE_MAX;
		prog_opts.file_format = file_format;
		prog_opts.read_buffer_size = 1;
		prog_opts.temp_allocator.huge_threshold = 1;
		prog_opts.result_allocator.huge_threshold = 1;
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

	// Ignore normals when doing diff to .obj when testing with various
	// `ufbx_load_opts.geometry_transform_handling` values.
	UFBXT_FILE_TEST_FLAG_OPT_HANDLING_IGNORE_NORMALS_IN_DIFF = 0x800,

	// Allow fewer than default progress calls
	UFBXT_FILE_TEST_FLAG_ALLOW_FEWER_PROGRESS_CALLS = 0x1000,

	// Scale FBX vertices by 100 when diffing.
	UFBXT_FILE_TEST_FLAG_DIFF_SCALE_100 = 0x2000,

	// Allow threaded parsing to fail
	UFBXT_FILE_TEST_FLAG_ALLOW_THREAD_ERROR = 0x4000,

} ufbxt_file_test_flags;

const char *ufbxt_file_formats[] = {
	"binary", "ascii", "obj", "mtl",
};

bool ufbxt_parse_format(char *buf, size_t buf_len, const char *name, const char **p_format, uint32_t *p_version)
{
	size_t len = strlen(name);
	const char *p = name + len;
	for (uint32_t i = 0; i < 2; i++) {
		while (p > name && p[-1] != '_') p--;
		if (p > name) p--;
	}
	if (p == name) return false;

	const char *name_end = p;
	p++;

	uint32_t version = 0;
	size_t digits = 0;
	while (p[0] >= '0' && p[0] <= '9') {
		version = version * 10 + (uint32_t)(p[0] - '0');
		digits++;
		p++;
	}
	if (digits == 0) return false;
	if (*p++ != '_') return false;

	bool format_valid = false;
	for (size_t i = 0; i < ufbxt_arraycount(ufbxt_file_formats); i++) {
		if (!strcmp(p, ufbxt_file_formats[i])) {
			format_valid = true;
			break;
		}
	}
	if (!format_valid) return false;

	size_t name_len = name_end - name;
	ufbxt_assert(name_len < buf_len);
	memcpy(buf, name, name_len);
	buf[name_len] = '\0';

	*p_format = p;
	*p_version = version;
	return true;
}

void ufbxt_do_file_test(const char *name, void (*test_fn)(ufbx_scene *s, ufbxt_diff_error *err, ufbx_error *load_error), const char *suffix, ufbx_load_opts user_opts, ufbxt_file_test_flags flags)
{
	const char *req_format = NULL;
	uint32_t req_version = 0;
	char name_buf[256];
	if (ufbxt_parse_format(name_buf, sizeof(name_buf), name, &req_format, &req_version)) {
		name = name_buf;
	}

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
	bool allow_thread_error = (flags & UFBXT_FILE_TEST_FLAG_ALLOW_THREAD_ERROR) != 0;

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
		obj_opts.ignore_missing_external_files = true;

		ufbx_error obj_error;
		obj_scene = ufbx_load_file(buf, &obj_opts, &obj_error);
		if (!obj_scene) {
			ufbxt_log_error(&obj_error);
			ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse .obj file");
		}
		ufbxt_assert(obj_scene->metadata.file_format == UFBX_FILE_FORMAT_OBJ);
		ufbxt_check_scene(obj_scene);

		ufbxt_diff_error err = { 0 };
		ufbxt_diff_to_obj(obj_scene, obj_file, &err, 0);
		if (err.num > 0) {
			double avg = err.sum / (double)err.num;
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

	if ((flags & UFBXT_FILE_TEST_FLAG_DIFF_SCALE_100) != 0) {
		obj_file->fbx_position_scale = 100.0;
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

			if (req_format != NULL) {
				if (strcmp(format, req_format) != 0) continue;
				if (version != req_version) continue;
			}

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
				if ((flags & UFBXT_FILE_TEST_FLAG_ALLOW_FEWER_PROGRESS_CALLS) == 0) {
					ufbxt_assert(progress_ctx.calls >= size / 0x4000 / 2);
				}
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

			#if defined(UFBXT_THREADS)
			{
				ufbx_load_opts thread_opts = load_opts;
				thread_opts.file_format = UFBX_FILE_FORMAT_UNKNOWN;
				thread_opts.retain_dom = true;

				ufbx_os_init_ufbx_thread_pool(&thread_opts.thread_opts.pool, g_thread_pool);

				ufbx_error thread_error;
				ufbx_scene *thread_scene = ufbx_load_file(buf, &thread_opts, &thread_error);
				if (thread_scene) {
					ufbxt_check_scene(thread_scene);
					ufbxt_assert(thread_scene->dom_root);
					ufbxt_assert(thread_scene->metadata.file_format == load_opts.file_format);
				} else if (allow_thread_error) {
					ufbxt_assert(thread_error.type == UFBX_ERROR_THREADED_ASCII_PARSE);
				} else if (!allow_error) {
					ufbxt_log_error(&thread_error);
					ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse threaded file");
				}
				ufbx_free_scene(thread_scene);
			}
			#endif

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
				ufbx_scene *state = ufbx_evaluate_scene(scene, scene->anim, 1.0, NULL, NULL);
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
					ufbx_scene *state = ufbx_evaluate_scene(scene, scene->anim_stacks.data[i]->anim, 1.0, NULL, NULL);
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
					ufbxt_diff_to_obj(scene, obj_file, &err, 0);
					num_failing_diff_checks = ufbxt_end_expect_fail();
				} else {
					ufbxt_diff_to_obj(scene, obj_file, &err, 0);
				}
			}

			if (!skip_opts_checks) {
				{
					ufbx_error opt_error;
					ufbx_load_opts opts = load_opts;
					opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
					ufbx_scene *opt_scene = ufbx_load_memory(data, size, &opts, &opt_error);
					if (opt_scene) {
						ufbxt_check_scene(opt_scene);

						if (scene && obj_file && (!alternative || diff_always) && !expect_diff_fail) {
							uint32_t diff_flags = 0;
							if ((flags & UFBXT_FILE_TEST_FLAG_OPT_HANDLING_IGNORE_NORMALS_IN_DIFF) != 0) {
								diff_flags |= UFBXT_OBJ_DIFF_FLAG_IGNORE_NORMALS;
							}
							ufbxt_diff_to_obj(opt_scene, obj_file, &err, diff_flags);
						}

						ufbx_free_scene(opt_scene);
					} else if (!allow_error) {
						ufbxt_log_error(&opt_error);
						ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file with helper nodes");
					}
				}

				{
					ufbx_error opt_error;
					ufbx_load_opts opts = load_opts;
					opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
					ufbx_scene *opt_scene = ufbx_load_memory(data, size, &opts, &opt_error);
					if (opt_scene) {
						ufbxt_check_scene(opt_scene);

						if (scene && obj_file && (!alternative || diff_always) && !expect_diff_fail) {
							uint32_t diff_flags = 0;
							if ((flags & UFBXT_FILE_TEST_FLAG_OPT_HANDLING_IGNORE_NORMALS_IN_DIFF) != 0) {
								diff_flags |= UFBXT_OBJ_DIFF_FLAG_IGNORE_NORMALS;
							}
							ufbxt_diff_to_obj(opt_scene, obj_file, &err, diff_flags);
						}

						ufbx_free_scene(opt_scene);
					} else if (!allow_error) {
						ufbxt_log_error(&opt_error);
						ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file with modifying geometry");
					}
				}
			}

			test_fn(scene, &err, &error);

			if (err.num > 0) {
				double avg = err.sum / (double)err.num;
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

	if (g_fuzz) {
		#if defined(UFBX_REGRESSION)
			int regression = 1;
		#else
			int regression = 0;
		#endif

		int  threads = 1;
		#if _OPENMP
			threads = omp_get_max_threads();
		#endif

		printf("Fuzzing with %d threads, UFBX_REGRESSION=%d\n", threads, regression);
	}

	#if defined(UFBXT_THREADS)
	{
		ufbx_os_thread_pool_opts pool_opts = { 0 };
		pool_opts.max_threads = 4;
		g_thread_pool = ufbx_os_create_thread_pool(&pool_opts);
		ufbxt_assert(g_thread_pool);
	}
	#endif

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

	#if defined(UFBXT_THREADS)
		ufbx_os_free_thread_pool(g_thread_pool);
	#endif

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

