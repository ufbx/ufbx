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
#include <float.h>

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
// From commit c13c876
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "blender_272_cube_7400_binary", 22373, -1, 0, 0, 150, 0, 0, "ufbxi_push_anim(uc, &uc->scene.anim, ((void *)0), 0)" },
	{ "blender_272_cube_7400_binary", 22373, -1, 0, 0, 300, 0, 0, "ufbxi_push_anim(uc, &uc->scene.anim, ((void *)0), 0)" },
	{ "blender_279_ball_0_obj", 16664, -1, 0, 0, 32, 0, 0, "props.data" },
	{ "blender_279_ball_0_obj", 16664, -1, 0, 0, 64, 0, 0, "props.data" },
	{ "blender_279_ball_0_obj", 16681, -1, 0, 2299, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "blender_279_ball_0_obj", 16681, -1, 0, 249, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "blender_279_ball_0_obj", 16823, -1, 0, 2204, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "blender_279_ball_0_obj", 16823, -1, 0, 240, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "blender_279_ball_0_obj", 17079, -1, 0, 136, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "blender_279_ball_0_obj", 17079, -1, 0, 1424, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "blender_279_ball_0_obj", 17249, -1, 0, 0, 0, 3099, 0, "uc->obj.num_tokens >= 2" },
	{ "blender_279_ball_0_obj", 17252, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "blender_279_ball_0_obj", 17252, -1, 0, 1379, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "blender_279_ball_0_obj", 17255, -1, 0, 1381, 0, 0, 0, "fbx_id" },
	{ "blender_279_ball_0_obj", 17268, -1, 0, 124, 0, 0, 0, "material" },
	{ "blender_279_ball_0_obj", 17268, -1, 0, 1383, 0, 0, 0, "material" },
	{ "blender_279_ball_0_obj", 17275, -1, 0, 126, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "blender_279_ball_0_obj", 17275, -1, 0, 1391, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "blender_279_ball_0_obj", 17603, -1, 0, 721, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "blender_279_ball_0_obj", 17618, -1, 0, 0, 0, 54, 0, "uc->obj.num_tokens >= 2" },
	{ "blender_279_ball_0_obj", 17621, -1, 0, 14, 0, 0, 0, "lib.data" },
	{ "blender_279_ball_0_obj", 17621, -1, 0, 717, 0, 0, 0, "lib.data" },
	{ "blender_279_ball_0_obj", 17625, -1, 0, 124, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 17625, -1, 0, 1379, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 17648, -1, 0, 2299, 0, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "blender_279_ball_0_obj", 17648, -1, 0, 249, 0, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "blender_279_ball_0_obj", 17663, -1, 0, 2219, 0, 0, 0, "prop" },
	{ "blender_279_ball_0_obj", 17663, -1, 0, 241, 0, 0, 0, "prop" },
	{ "blender_279_ball_0_obj", 17666, -1, 0, 0, 16, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "blender_279_ball_0_obj", 17666, -1, 0, 2221, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "blender_279_ball_0_obj", 17699, -1, 0, 0, 17, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->va..." },
	{ "blender_279_ball_0_obj", 17699, -1, 0, 2223, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->va..." },
	{ "blender_279_ball_0_obj", 17700, -1, 0, 2225, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &prop->v..." },
	{ "blender_279_ball_0_obj", 17786, -1, 0, 2204, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "blender_279_ball_0_obj", 17786, -1, 0, 240, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "blender_279_ball_0_obj", 17793, -1, 0, 2299, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 17793, -1, 0, 249, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 17794, -1, 0, 2216, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 17802, -1, 0, 2219, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "blender_279_ball_0_obj", 17802, -1, 0, 241, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "blender_279_ball_0_obj", 17806, -1, 0, 0, 34, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 17806, -1, 0, 2369, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 17847, -1, 0, 2202, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_EXT..." },
	{ "blender_279_ball_0_obj", 17847, -1, 0, 239, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_EXT..." },
	{ "blender_279_ball_0_obj", 17853, -1, 0, 2200, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "blender_279_ball_0_obj", 17853, -1, 0, 239, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "blender_279_ball_0_obj", 17895, -1, 0, 2204, 0, 0, 0, "ok" },
	{ "blender_279_ball_0_obj", 17895, -1, 0, 240, 0, 0, 0, "ok" },
	{ "blender_279_ball_0_obj", 17898, 55, 99, 0, 0, 0, 0, "ufbxi_obj_load_mtl()" },
	{ "blender_279_ball_0_obj", 17909, 55, 99, 0, 0, 0, 0, "ufbxi_obj_load_mtl(uc)" },
	{ "blender_279_ball_0_obj", 6704, -1, 0, 2204, 0, 0, 0, "new_buffer" },
	{ "blender_279_ball_0_obj", 6704, -1, 0, 240, 0, 0, 0, "new_buffer" },
	{ "blender_279_ball_7400_binary", 25005, -1, 0, 1, 0, 0, 0, "ufbxi_fixup_opts_string(uc, &uc->opts.obj_mtl_path, 1)" },
	{ "blender_279_default_obj", 17291, 481, 48, 0, 0, 0, 0, "min_index < uc->obj.tmp_vertices[attrib].num_items / st..." },
	{ "blender_279_unicode_6100_ascii", 15734, 432, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "blender_279_uv_sets_6100_ascii", 13635, -1, 0, 0, 61, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 13635, -1, 0, 3183, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 13641, -1, 0, 3185, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 13641, -1, 0, 729, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 13732, -1, 0, 3189, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 13732, -1, 0, 730, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 13735, -1, 0, 3193, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 13735, -1, 0, 732, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 18414, -1, 0, 13162, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "blender_279_uv_sets_6100_ascii", 22190, -1, 0, 13158, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 22190, -1, 0, 3859, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 22197, -1, 0, 13160, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 22197, -1, 0, 3860, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 22198, -1, 0, 13162, 0, 0, 0, "ufbxi_sort_tmp_material_textures(uc, mat_texs, num_mate..." },
	{ "blender_279_uv_sets_6100_ascii", 7850, -1, 0, 3189, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 7850, -1, 0, 730, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 7857, -1, 0, 3191, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 7857, -1, 0, 731, 0, 0, 0, "extra" },
	{ "blender_282_suzanne_and_transform_obj", 17069, -1, 0, 0, 2, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "blender_282_suzanne_and_transform_obj", 17069, -1, 0, 0, 4, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "blender_293_instancing_obj", 16714, -1, 0, 8042, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "blender_293_instancing_obj", 17858, -1, 0, 112316, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_EXT..." },
	{ "blender_293_instancing_obj", 17858, -1, 0, 17228, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_EXT..." },
	{ "blender_293x_nonmanifold_subsurf_obj", 17181, -1, 0, 1167, 0, 0, 0, "ufbxi_obj_parse_indices(uc, begin, window)" },
	{ "blender_293x_nonmanifold_subsurf_obj", 17181, -1, 0, 93, 0, 0, 0, "ufbxi_obj_parse_indices(uc, begin, window)" },
	{ "blender_293x_nonmanifold_subsurf_obj", 17352, -1, 0, 1187, 0, 0, 0, "ufbxi_fix_index(uc, &dst_indices[i], (uint32_t)ix, num_..." },
	{ "blender_293x_nonmanifold_subsurf_obj", 17352, -1, 0, 99, 0, 0, 0, "ufbxi_fix_index(uc, &dst_indices[i], (uint32_t)ix, num_..." },
	{ "blender_293x_nonmanifold_subsurf_obj", 17589, -1, 0, 1167, 0, 0, 0, "ufbxi_obj_parse_multi_indices(uc, 2)" },
	{ "blender_293x_nonmanifold_subsurf_obj", 17589, -1, 0, 93, 0, 0, 0, "ufbxi_obj_parse_multi_indices(uc, 2)" },
	{ "fuzz_0018", 16330, 810, 0, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "fuzz_0070", 5079, -1, 0, 33, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0070", 5079, -1, 0, 774, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0393", 12814, -1, 0, 0, 136, 0, 0, "index_data" },
	{ "fuzz_0393", 12814, -1, 0, 0, 272, 0, 0, "index_data" },
	{ "fuzz_0397", 12643, -1, 0, 0, 137, 0, 0, "new_indices" },
	{ "fuzz_0397", 12643, -1, 0, 0, 274, 0, 0, "new_indices" },
	{ "marvelous_quad_7200_binary", 24125, -1, 0, 0, 272, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "max2009_blob_5800_ascii", 10371, -1, 0, 15614, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 10371, -1, 0, 4418, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 10382, -1, 0, 0, 113, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 10382, -1, 0, 15616, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 10444, 131240, 45, 0, 0, 0, 0, "Bad array dst type" },
	{ "max2009_blob_5800_ascii", 10516, -1, 0, 23251, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 10516, -1, 0, 6977, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 11347, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "max2009_blob_5800_ascii", 15365, 164150, 114, 0, 0, 0, 0, "Unknown slope mode" },
	{ "max2009_blob_5800_ascii", 15395, 164585, 45, 0, 0, 0, 0, "Unknown weight mode" },
	{ "max2009_blob_5800_ascii", 15408, 164150, 116, 0, 0, 0, 0, "Unknown key mode" },
	{ "max2009_blob_5800_binary", 11365, -1, 0, 43, 0, 0, 0, "ufbxi_retain_toplevel(uc, &uc->legacy_node)" },
	{ "max2009_blob_5800_binary", 11365, -1, 0, 996, 0, 0, 0, "ufbxi_retain_toplevel(uc, &uc->legacy_node)" },
	{ "max2009_blob_5800_binary", 12356, -1, 0, 9, 0, 0, 0, "ufbxi_insert_fbx_id(uc, *p_fbx_id, element_id)" },
	{ "max2009_blob_5800_binary", 12377, -1, 0, 3342, 0, 0, 0, "conn" },
	{ "max2009_blob_5800_binary", 12377, -1, 0, 9925, 0, 0, 0, "conn" },
	{ "max2009_blob_5800_binary", 15268, -1, 0, 10083, 0, 0, 0, "curve" },
	{ "max2009_blob_5800_binary", 15268, -1, 0, 3403, 0, 0, 0, "curve" },
	{ "max2009_blob_5800_binary", 15270, -1, 0, 10091, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max2009_blob_5800_binary", 15270, -1, 0, 3405, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max2009_blob_5800_binary", 15278, 119084, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "max2009_blob_5800_binary", 15281, 119104, 255, 0, 0, 0, 0, "curve->keyframes.data" },
	{ "max2009_blob_5800_binary", 15295, 119110, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max2009_blob_5800_binary", 15388, 119110, 16, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "max2009_blob_5800_binary", 15413, 119102, 3, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max2009_blob_5800_binary", 15462, 119102, 1, 0, 0, 0, 0, "data == data_end" },
	{ "max2009_blob_5800_binary", 15483, 114022, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "max2009_blob_5800_binary", 15494, 119084, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "max2009_blob_5800_binary", 15537, -1, 0, 3336, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 15537, -1, 0, 9912, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 15538, -1, 0, 3342, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max2009_blob_5800_binary", 15538, -1, 0, 9925, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max2009_blob_5800_binary", 15541, 119084, 0, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "max2009_blob_5800_binary", 15554, 114858, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "max2009_blob_5800_binary", 15564, 114022, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "max2009_blob_5800_binary", 15627, 114022, 0, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 15689, -1, 0, 3729, 0, 0, 0, "ufbxi_sort_properties(uc, new_props, new_count)" },
	{ "max2009_blob_5800_binary", 15959, -1, 0, 2559, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 15959, -1, 0, 577, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 15967, -1, 0, 0, 137, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 15967, -1, 0, 0, 274, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 16008, -1, 0, 107, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 16008, -1, 0, 1208, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 16015, -1, 0, 0, 41, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 16015, -1, 0, 0, 82, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 16023, -1, 0, 1799, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 16023, -1, 0, 310, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 16030, -1, 0, 0, 182, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 16030, -1, 0, 0, 91, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 16063, -1, 0, 2545, 0, 0, 0, "mesh" },
	{ "max2009_blob_5800_binary", 16063, -1, 0, 572, 0, 0, 0, "mesh" },
	{ "max2009_blob_5800_binary", 16074, 9030, 37, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "max2009_blob_5800_binary", 16084, -1, 0, 0, 1322, 0, 0, "index_data" },
	{ "max2009_blob_5800_binary", 16084, -1, 0, 0, 661, 0, 0, "index_data" },
	{ "max2009_blob_5800_binary", 16107, 58502, 255, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "max2009_blob_5800_binary", 16138, -1, 0, 0, 135, 0, 0, "set" },
	{ "max2009_blob_5800_binary", 16138, -1, 0, 0, 270, 0, 0, "set" },
	{ "max2009_blob_5800_binary", 16142, 84142, 21, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, uv_info, (ufbx_vert..." },
	{ "max2009_blob_5800_binary", 16152, 56645, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MaterialAssignation, \"C\",..." },
	{ "max2009_blob_5800_binary", 16183, 6207, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 16184, -1, 0, 0, 136, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 16184, -1, 0, 2556, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 16185, -1, 0, 2559, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 16185, -1, 0, 577, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 16186, -1, 0, 2567, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 16186, -1, 0, 579, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 16235, 818, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 16236, -1, 0, 0, 40, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 16236, -1, 0, 1193, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 16240, -1, 0, 1196, 0, 0, 0, "info.fbx_id" },
	{ "max2009_blob_5800_binary", 16245, -1, 0, 105, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 16245, -1, 0, 1198, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 16246, -1, 0, 1205, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 16246, -1, 0, 366, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 16254, -1, 0, 106, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 16254, -1, 0, 1206, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 16261, -1, 0, 107, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 16261, -1, 0, 1208, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 16263, -1, 0, 1799, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 16263, -1, 0, 310, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 16267, 6207, 0, 0, 0, 0, 0, "ufbxi_read_legacy_mesh(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 16274, -1, 0, 112, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max2009_blob_5800_binary", 16274, -1, 0, 1219, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max2009_blob_5800_binary", 16283, -1, 0, 2654, 0, 0, 0, "child_fbx_id" },
	{ "max2009_blob_5800_binary", 16284, -1, 0, 2655, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 16284, -1, 0, 604, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 16308, -1, 0, 3, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max2009_blob_5800_binary", 16308, -1, 0, 689, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max2009_blob_5800_binary", 16315, -1, 0, 4, 0, 0, 0, "root" },
	{ "max2009_blob_5800_binary", 16315, -1, 0, 889, 0, 0, 0, "root" },
	{ "max2009_blob_5800_binary", 16317, -1, 0, 10, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 16317, -1, 0, 901, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 16334, 114022, 0, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "max2009_blob_5800_binary", 16336, 818, 0, 0, 0, 0, 0, "ufbxi_read_legacy_model(uc, node)" },
	{ "max2009_blob_5800_binary", 16338, -1, 0, 10957, 0, 0, 0, "ufbxi_read_legacy_settings(uc, node)" },
	{ "max2009_blob_5800_binary", 16338, -1, 0, 3729, 0, 0, 0, "ufbxi_read_legacy_settings(uc, node)" },
	{ "max2009_blob_5800_binary", 16343, -1, 0, 0, 3549, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "max2009_blob_5800_binary", 16343, -1, 0, 0, 7098, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "max2009_blob_5800_binary", 18019, -1, 0, 10983, 0, 0, 0, "pre_anim_values" },
	{ "max2009_blob_5800_binary", 18435, -1, 0, 3733, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max2009_blob_5800_binary", 18558, -1, 0, 3733, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "max2009_blob_5800_binary", 18669, -1, 0, 10996, 0, 0, 0, "new_prop" },
	{ "max2009_blob_5800_binary", 18669, -1, 0, 3734, 0, 0, 0, "new_prop" },
	{ "max2009_blob_5800_binary", 18686, -1, 0, 0, 354, 0, 0, "elem->props.props.data" },
	{ "max2009_blob_5800_binary", 18686, -1, 0, 0, 708, 0, 0, "elem->props.props.data" },
	{ "max2009_blob_5800_binary", 21381, -1, 0, 0, 411, 0, 0, "part->face_indices.data" },
	{ "max2009_blob_5800_binary", 21381, -1, 0, 0, 822, 0, 0, "part->face_indices.data" },
	{ "max2009_blob_5800_binary", 21396, -1, 0, 0, 415, 0, 0, "mesh->material_part_usage_order.data" },
	{ "max2009_blob_5800_binary", 21396, -1, 0, 0, 830, 0, 0, "mesh->material_part_usage_order.data" },
	{ "max2009_blob_5800_binary", 21466, -1, 0, 10996, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "max2009_blob_5800_binary", 21466, -1, 0, 3734, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "max2009_blob_5800_binary", 21859, -1, 0, 0, 409, 0, 0, "materials" },
	{ "max2009_blob_5800_binary", 21859, -1, 0, 0, 818, 0, 0, "materials" },
	{ "max2009_blob_5800_binary", 21903, -1, 0, 0, 411, 0, 0, "ufbxi_finalize_mesh_material(&uc->result, &uc->error, m..." },
	{ "max2009_blob_5800_binary", 21903, -1, 0, 0, 822, 0, 0, "ufbxi_finalize_mesh_material(&uc->result, &uc->error, m..." },
	{ "max2009_blob_5800_binary", 21956, -1, 0, 11236, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "max2009_blob_5800_binary", 21956, -1, 0, 3846, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "max2009_blob_5800_binary", 21974, -1, 0, 11331, 0, 0, 0, "aprop" },
	{ "max2009_blob_5800_binary", 21974, -1, 0, 3878, 0, 0, 0, "aprop" },
	{ "max2009_blob_5800_binary", 8764, -1, 0, 0, 0, 80100, 0, "val" },
	{ "max2009_blob_5800_binary", 8766, 80062, 17, 0, 0, 0, 0, "type == 'S' || type == 'R'" },
	{ "max2009_blob_5800_binary", 8775, 80082, 1, 0, 0, 0, 0, "d->data" },
	{ "max2009_blob_5800_binary", 8781, -1, 0, 0, 116, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max2009_blob_5800_binary", 8781, -1, 0, 2497, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max2009_blob_6100_binary", 12551, -1, 0, 1359, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_6100_binary", 14834, -1, 0, 2141, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 14834, -1, 0, 373, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 14836, -1, 0, 1061, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 14836, -1, 0, 4535, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_cube_texture_5800_binary", 16221, 710, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"S\", &video_info.name)" },
	{ "max2009_cube_texture_5800_binary", 16225, -1, 0, 1083, 0, 0, 0, "ufbxi_read_video(uc, child, &video_info)" },
	{ "max2009_cube_texture_5800_binary", 16225, -1, 0, 71, 0, 0, 0, "ufbxi_read_video(uc, child, &video_info)" },
	{ "max2009_cube_texture_5800_binary", 16332, 710, 0, 0, 0, 0, 0, "ufbxi_read_legacy_media(uc, node)" },
	{ "max7_blend_cube_5000_binary", 13062, -1, 0, 1755, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 13062, -1, 0, 319, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 15505, -1, 0, 1851, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "max7_blend_cube_5000_binary", 16065, 2350, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "max7_skin_5000_binary", 15977, -1, 0, 1814, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 15977, -1, 0, 343, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 15984, 2420, 136, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "max7_skin_5000_binary", 15995, 4378, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "max7_skin_5000_binary", 15996, 4544, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "max7_skin_5000_binary", 16038, -1, 0, 2243, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 16038, -1, 0, 502, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 16050, -1, 0, 0, 49, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 16050, -1, 0, 0, 98, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 16190, 2361, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max7_skin_5000_binary", 16191, -1, 0, 1813, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max7_skin_5000_binary", 16192, 2420, 136, 0, 0, 0, 0, "ufbxi_read_legacy_link(uc, child, &fbx_id, name.data)" },
	{ "max7_skin_5000_binary", 16195, -1, 0, 1850, 0, 0, 0, "node_fbx_id" },
	{ "max7_skin_5000_binary", 16196, -1, 0, 1825, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 16196, -1, 0, 348, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 16199, -1, 0, 1827, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 16199, -1, 0, 349, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 16200, -1, 0, 1835, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 16200, -1, 0, 351, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 16202, -1, 0, 1837, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 16202, -1, 0, 352, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 16265, -1, 0, 2243, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max7_skin_5000_binary", 16265, -1, 0, 502, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max_cache_box_7500_binary", 24009, -1, 0, 3053, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 24009, -1, 0, 685, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 24179, -1, 0, 3053, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_cache_box_7500_binary", 24179, -1, 0, 685, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_curve_line_7500_ascii", 13851, 8302, 43, 0, 0, 0, 0, "points->size % 3 == 0" },
	{ "max_curve_line_7500_binary", 13844, -1, 0, 2244, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 13844, -1, 0, 428, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 13849, 13861, 255, 0, 0, 0, 0, "points" },
	{ "max_curve_line_7500_binary", 13850, 13985, 56, 0, 0, 0, 0, "points_index" },
	{ "max_curve_line_7500_binary", 13872, -1, 0, 0, 139, 0, 0, "line->segments.data" },
	{ "max_curve_line_7500_binary", 13872, -1, 0, 0, 278, 0, 0, "line->segments.data" },
	{ "max_curve_line_7500_binary", 14959, 13861, 255, 0, 0, 0, 0, "ufbxi_read_line(uc, node, &info)" },
	{ "max_geometry_transform_6100_binary", 12466, -1, 0, 3290, 0, 0, 0, "geo_node" },
	{ "max_geometry_transform_6100_binary", 12466, -1, 0, 731, 0, 0, 0, "geo_node" },
	{ "max_geometry_transform_6100_binary", 12467, -1, 0, 3299, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max_geometry_transform_6100_binary", 12467, -1, 0, 740, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max_geometry_transform_6100_binary", 12471, -1, 0, 0, 131, 0, 0, "props" },
	{ "max_geometry_transform_6100_binary", 12471, -1, 0, 0, 262, 0, 0, "props" },
	{ "max_geometry_transform_6100_binary", 12482, -1, 0, 3300, 0, 0, 0, "ufbxi_connect_oo(uc, geo_fbx_id, node_fbx_id)" },
	{ "max_geometry_transform_6100_binary", 12482, -1, 0, 734, 0, 0, 0, "ufbxi_connect_oo(uc, geo_fbx_id, node_fbx_id)" },
	{ "max_geometry_transform_6100_binary", 12486, -1, 0, 3302, 0, 0, 0, "extra" },
	{ "max_geometry_transform_6100_binary", 12486, -1, 0, 735, 0, 0, 0, "extra" },
	{ "max_geometry_transform_6100_binary", 18259, -1, 0, 3290, 0, 0, 0, "ufbxi_setup_geometry_transform_helper(uc, node, fbx_id)" },
	{ "max_geometry_transform_6100_binary", 18259, -1, 0, 731, 0, 0, 0, "ufbxi_setup_geometry_transform_helper(uc, node, fbx_id)" },
	{ "max_geometry_transform_6100_binary", 25006, -1, 0, 1, 0, 0, 0, "ufbxi_fixup_opts_string(uc, &uc->opts.geometry_transfor..." },
	{ "max_geometry_transform_types_obj", 18399, -1, 0, 29, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_geometry_transform_types_obj", 18762, -1, 0, 29, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "max_openpbr_material_7700_ascii", 20194, -1, 0, 10565, 0, 0, 0, "copy" },
	{ "max_openpbr_material_7700_ascii", 20194, -1, 0, 2760, 0, 0, 0, "copy" },
	{ "max_openpbr_material_7700_ascii", 20202, -1, 0, 0, 454, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_openpbr_material_7700_ascii", 20202, -1, 0, 10567, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_openpbr_material_7700_ascii", 20231, -1, 0, 10565, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_openpbr_material_7700_ascii", 20231, -1, 0, 2760, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_openpbr_material_7700_ascii", 20342, -1, 0, 0, 453, 0, 0, "shader" },
	{ "max_openpbr_material_7700_ascii", 20342, -1, 0, 0, 906, 0, 0, "shader" },
	{ "max_openpbr_material_7700_ascii", 20374, -1, 0, 10565, 0, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_openpbr_material_7700_ascii", 20374, -1, 0, 2760, 0, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_openpbr_material_7700_ascii", 20429, -1, 0, 10569, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_openpbr_material_7700_ascii", 20446, -1, 0, 0, 455, 0, 0, "shader->inputs.data" },
	{ "max_openpbr_material_7700_ascii", 20446, -1, 0, 0, 910, 0, 0, "shader->inputs.data" },
	{ "max_openpbr_material_7700_ascii", 20624, -1, 0, 10987, 0, 0, 0, "textures" },
	{ "max_openpbr_material_7700_ascii", 20624, -1, 0, 2828, 0, 0, 0, "textures" },
	{ "max_openpbr_material_7700_ascii", 20626, -1, 0, 10989, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_openpbr_material_7700_ascii", 20691, -1, 0, 10985, 0, 0, 0, "dst" },
	{ "max_openpbr_material_7700_ascii", 20691, -1, 0, 2827, 0, 0, 0, "dst" },
	{ "max_openpbr_material_7700_ascii", 20711, -1, 0, 10992, 0, 0, 0, "dst" },
	{ "max_openpbr_material_7700_ascii", 20711, -1, 0, 2854, 0, 0, 0, "dst" },
	{ "max_openpbr_material_7700_ascii", 20720, -1, 0, 10987, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &deps, &..." },
	{ "max_openpbr_material_7700_ascii", 20720, -1, 0, 2828, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &deps, &..." },
	{ "max_openpbr_material_7700_ascii", 20770, -1, 0, 10984, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_openpbr_material_7700_ascii", 20770, -1, 0, 2853, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_openpbr_material_7700_ascii", 20781, -1, 0, 10991, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_openpbr_material_7700_ascii", 22257, -1, 0, 10565, 0, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_openpbr_material_7700_ascii", 22257, -1, 0, 2760, 0, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_quote_6100_ascii", 12264, -1, 0, 1136, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_O..." },
	{ "max_quote_6100_ascii", 12264, -1, 0, 4556, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_O..." },
	{ "max_quote_6100_ascii", 21546, -1, 0, 1395, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 21546, -1, 0, 5498, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 21548, -1, 0, 5500, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 21562, -1, 0, 0, 176, 0, 0, "node->all_attribs.data" },
	{ "max_quote_6100_ascii", 21562, -1, 0, 0, 352, 0, 0, "node->all_attribs.data" },
	{ "max_texture_mapping_6100_binary", 20254, -1, 0, 10881, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 20254, -1, 0, 2735, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 20386, -1, 0, 0, 677, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "max_texture_mapping_6100_binary", 20386, -1, 0, 10985, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "maya_anim_no_inherit_scale_7700_ascii", 24928, -1, 0, 1, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, str, 0)" },
	{ "maya_anim_no_inherit_scale_7700_ascii", 25007, -1, 0, 1, 0, 0, 0, "ufbxi_fixup_opts_string(uc, &uc->opts.scale_helper_name..." },
	{ "maya_audio_7500_ascii", 14740, -1, 0, 0, 220, 0, 0, "ufbxi_read_embedded_blob(uc, &audio->content, content_n..." },
	{ "maya_audio_7500_ascii", 14740, -1, 0, 0, 440, 0, 0, "ufbxi_read_embedded_blob(uc, &audio->content, content_n..." },
	{ "maya_audio_7500_binary", 14733, -1, 0, 3492, 0, 0, 0, "audio" },
	{ "maya_audio_7500_binary", 14733, -1, 0, 796, 0, 0, 0, "audio" },
	{ "maya_audio_7500_binary", 15026, -1, 0, 3420, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_audio_l..." },
	{ "maya_audio_7500_binary", 15026, -1, 0, 778, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_audio_l..." },
	{ "maya_audio_7500_binary", 15028, -1, 0, 3492, 0, 0, 0, "ufbxi_read_audio_clip(uc, node, &info)" },
	{ "maya_audio_7500_binary", 15028, -1, 0, 796, 0, 0, 0, "ufbxi_read_audio_clip(uc, node, &info)" },
	{ "maya_audio_7500_binary", 21290, -1, 0, 3737, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&clip->file..." },
	{ "maya_audio_7500_binary", 21290, -1, 0, 880, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&clip->file..." },
	{ "maya_audio_7500_binary", 21291, -1, 0, 3741, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&clip->raw_..." },
	{ "maya_audio_7500_binary", 21291, -1, 0, 881, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&clip->raw_..." },
	{ "maya_audio_7500_binary", 21292, -1, 0, 3744, 0, 0, 0, "ufbxi_push_file_content(uc, &clip->absolute_filename, &..." },
	{ "maya_audio_7500_binary", 21292, -1, 0, 882, 0, 0, 0, "ufbxi_push_file_content(uc, &clip->absolute_filename, &..." },
	{ "maya_audio_7500_binary", 22362, -1, 0, 3750, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->clips, &layer->ele..." },
	{ "maya_audio_7500_binary", 22362, -1, 0, 884, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->clips, &layer->ele..." },
	{ "maya_axes_anim_7700_ascii", 14977, -1, 0, 17982, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_axes_anim_7700_ascii", 14977, -1, 0, 5444, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_axes_anim_7700_ascii", 15022, -1, 0, 18178, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_axes_anim_7700_ascii", 15022, -1, 0, 5503, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_axes_anim_7700_ascii", 21763, -1, 0, 26886, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_axes_anim_7700_ascii", 21763, -1, 0, 8477, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_axes_anim_7700_ascii", 21764, -1, 0, 26890, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->raw..." },
	{ "maya_axes_anim_7700_ascii", 21764, -1, 0, 8478, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->raw..." },
	{ "maya_axes_anim_7700_ascii", 21909, -1, 0, 26906, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_axes_anim_7700_ascii", 21909, -1, 0, 8485, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_axes_anim_7700_ascii", 23951, -1, 0, 27994, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->na..." },
	{ "maya_axes_anim_7700_ascii", 23951, -1, 0, 8682, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->na..." },
	{ "maya_axes_anim_7700_ascii", 23955, -1, 0, 27996, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->chan..." },
	{ "maya_axes_anim_7700_ascii", 23968, -1, 0, 27997, 0, 0, 0, "frame" },
	{ "maya_axes_anim_7700_ascii", 23968, -1, 0, 8683, 0, 0, 0, "frame" },
	{ "maya_axes_anim_7700_ascii", 24053, -1, 0, 27988, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_axes_anim_7700_ascii", 24053, -1, 0, 8680, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_axes_anim_7700_ascii", 24074, -1, 0, 27968, 0, 0, 0, "extra" },
	{ "maya_axes_anim_7700_ascii", 24074, -1, 0, 8676, 0, 0, 0, "extra" },
	{ "maya_axes_anim_7700_ascii", 24076, -1, 0, 0, 544, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_axes_anim_7700_ascii", 24076, -1, 0, 27970, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_axes_anim_7700_ascii", 24081, -1, 0, 0, 1094, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_axes_anim_7700_ascii", 24081, -1, 0, 0, 547, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_axes_anim_7700_ascii", 24114, -1, 0, 27980, 0, 0, 0, "cc->channels" },
	{ "maya_axes_anim_7700_ascii", 24114, -1, 0, 8679, 0, 0, 0, "cc->channels" },
	{ "maya_axes_anim_7700_ascii", 24125, -1, 0, 27982, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_axes_anim_7700_ascii", 24126, -1, 0, 0, 548, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_axes_anim_7700_ascii", 24126, -1, 0, 27983, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_axes_anim_7700_ascii", 24142, -1, 0, 27988, 0, 0, 0, "ufbxi_cache_sort_tmp_channels(cc, cc->channels, cc->num..." },
	{ "maya_axes_anim_7700_ascii", 24142, -1, 0, 8680, 0, 0, 0, "ufbxi_cache_sort_tmp_channels(cc, cc->channels, cc->num..." },
	{ "maya_axes_anim_7700_ascii", 24155, -1, 0, 27043, 0, 0, 0, "doc" },
	{ "maya_axes_anim_7700_ascii", 24155, -1, 0, 8542, 0, 0, 0, "doc" },
	{ "maya_axes_anim_7700_ascii", 24159, -1, 0, 27968, 0, 0, 0, "xml_ok" },
	{ "maya_axes_anim_7700_ascii", 24159, -1, 0, 8676, 0, 0, 0, "xml_ok" },
	{ "maya_axes_anim_7700_ascii", 24167, -1, 0, 0, 550, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_axes_anim_7700_ascii", 24167, -1, 0, 27042, 0, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_axes_anim_7700_ascii", 24181, -1, 0, 27994, 0, 0, 0, "ufbxi_cache_load_mc(cc)" },
	{ "maya_axes_anim_7700_ascii", 24181, -1, 0, 8682, 0, 0, 0, "ufbxi_cache_load_mc(cc)" },
	{ "maya_axes_anim_7700_ascii", 24183, -1, 0, 27043, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_axes_anim_7700_ascii", 24183, -1, 0, 8542, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_axes_anim_7700_ascii", 24221, -1, 0, 27990, 0, 0, 0, "name_buf" },
	{ "maya_axes_anim_7700_ascii", 24221, -1, 0, 8681, 0, 0, 0, "name_buf" },
	{ "maya_axes_anim_7700_ascii", 24242, -1, 0, 27992, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename, ((void *)0), &f..." },
	{ "maya_axes_anim_7700_ascii", 24242, -1, 0, 8682, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename, ((void *)0), &f..." },
	{ "maya_axes_anim_7700_ascii", 24306, -1, 0, 28164, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_axes_anim_7700_ascii", 24306, -1, 0, 8726, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_axes_anim_7700_ascii", 24335, -1, 0, 28166, 0, 0, 0, "chan" },
	{ "maya_axes_anim_7700_ascii", 24335, -1, 0, 8727, 0, 0, 0, "chan" },
	{ "maya_axes_anim_7700_ascii", 24380, -1, 0, 0, 1104, 0, 0, "cc->cache.channels.data" },
	{ "maya_axes_anim_7700_ascii", 24380, -1, 0, 0, 552, 0, 0, "cc->cache.channels.data" },
	{ "maya_axes_anim_7700_ascii", 24400, -1, 0, 27040, 0, 0, 0, "filename_data" },
	{ "maya_axes_anim_7700_ascii", 24400, -1, 0, 8541, 0, 0, 0, "filename_data" },
	{ "maya_axes_anim_7700_ascii", 24407, -1, 0, 27042, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, ((void *)0..." },
	{ "maya_axes_anim_7700_ascii", 24407, -1, 0, 8542, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, ((void *)0..." },
	{ "maya_axes_anim_7700_ascii", 24415, -1, 0, 27990, 0, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_axes_anim_7700_ascii", 24415, -1, 0, 8681, 0, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_axes_anim_7700_ascii", 24420, -1, 0, 0, 1102, 0, 0, "cc->cache.frames.data" },
	{ "maya_axes_anim_7700_ascii", 24420, -1, 0, 0, 551, 0, 0, "cc->cache.frames.data" },
	{ "maya_axes_anim_7700_ascii", 24422, -1, 0, 28164, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "maya_axes_anim_7700_ascii", 24422, -1, 0, 8726, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "maya_axes_anim_7700_ascii", 24423, -1, 0, 28166, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_axes_anim_7700_ascii", 24423, -1, 0, 8727, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_axes_anim_7700_ascii", 24427, -1, 0, 0, 1106, 0, 0, "cc->imp" },
	{ "maya_axes_anim_7700_ascii", 24427, -1, 0, 0, 553, 0, 0, "cc->imp" },
	{ "maya_axes_anim_7700_ascii", 24637, -1, 0, 27036, 0, 0, 0, "file" },
	{ "maya_axes_anim_7700_ascii", 24637, -1, 0, 8539, 0, 0, 0, "file" },
	{ "maya_axes_anim_7700_ascii", 24647, -1, 0, 27038, 0, 0, 0, "files" },
	{ "maya_axes_anim_7700_ascii", 24647, -1, 0, 8540, 0, 0, 0, "files" },
	{ "maya_axes_anim_7700_ascii", 24655, -1, 0, 27040, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_axes_anim_7700_ascii", 24655, -1, 0, 8541, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_axes_anim_7700_ascii", 25106, -1, 0, 27036, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_axes_anim_7700_ascii", 25106, -1, 0, 8539, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_axes_anim_7700_ascii", 7296, -1, 0, 27045, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_axes_anim_7700_ascii", 7296, -1, 0, 8543, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_axes_anim_7700_ascii", 7331, -1, 0, 27045, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_axes_anim_7700_ascii", 7331, -1, 0, 8543, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_axes_anim_7700_ascii", 7343, -1, 0, 27068, 0, 0, 0, "ufbxi_xml_push_token_char(xc, '\\0')" },
	{ "maya_axes_anim_7700_ascii", 7415, -1, 0, 27069, 0, 0, 0, "ufbxi_xml_push_token_char(xc, c)" },
	{ "maya_axes_anim_7700_ascii", 7415, -1, 0, 8592, 0, 0, 0, "ufbxi_xml_push_token_char(xc, c)" },
	{ "maya_axes_anim_7700_ascii", 7420, -1, 0, 27071, 0, 0, 0, "ufbxi_xml_push_token_char(xc, '\\0')" },
	{ "maya_axes_anim_7700_ascii", 7424, -1, 0, 27098, 0, 0, 0, "dst->data" },
	{ "maya_axes_anim_7700_ascii", 7424, -1, 0, 8552, 0, 0, 0, "dst->data" },
	{ "maya_axes_anim_7700_ascii", 7441, -1, 0, 27069, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_T..." },
	{ "maya_axes_anim_7700_ascii", 7441, -1, 0, 8592, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_T..." },
	{ "maya_axes_anim_7700_ascii", 7452, -1, 0, 27072, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 7452, -1, 0, 8549, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 7457, -1, 0, 27074, 0, 0, 0, "tag->text.data" },
	{ "maya_axes_anim_7700_ascii", 7457, -1, 0, 8550, 0, 0, 0, "tag->text.data" },
	{ "maya_axes_anim_7700_ascii", 7464, -1, 0, 27369, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_N..." },
	{ "maya_axes_anim_7700_ascii", 7490, -1, 0, 27045, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_axes_anim_7700_ascii", 7490, -1, 0, 8543, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_axes_anim_7700_ascii", 7495, -1, 0, 27076, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 7495, -1, 0, 8551, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 7496, -1, 0, 27078, 0, 0, 0, "ufbxi_xml_read_until(xc, &tag->name, UFBXI_XML_CTYPE_NA..." },
	{ "maya_axes_anim_7700_ascii", 7496, -1, 0, 8552, 0, 0, 0, "ufbxi_xml_read_until(xc, &tag->name, UFBXI_XML_CTYPE_NA..." },
	{ "maya_axes_anim_7700_ascii", 7512, -1, 0, 27123, 0, 0, 0, "attrib" },
	{ "maya_axes_anim_7700_ascii", 7512, -1, 0, 8557, 0, 0, 0, "attrib" },
	{ "maya_axes_anim_7700_ascii", 7513, -1, 0, 27125, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->name, UFBXI_XML_CTYPE..." },
	{ "maya_axes_anim_7700_ascii", 7513, -1, 0, 8558, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->name, UFBXI_XML_CTYPE..." },
	{ "maya_axes_anim_7700_ascii", 7525, -1, 0, 27132, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->value, quote_ctype)" },
	{ "maya_axes_anim_7700_ascii", 7525, -1, 0, 8559, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->value, quote_ctype)" },
	{ "maya_axes_anim_7700_ascii", 7533, -1, 0, 27159, 0, 0, 0, "tag->attribs" },
	{ "maya_axes_anim_7700_ascii", 7533, -1, 0, 8563, 0, 0, 0, "tag->attribs" },
	{ "maya_axes_anim_7700_ascii", 7539, -1, 0, 27100, 0, 0, 0, "ufbxi_xml_parse_tag(xc, depth + 1, &closing, tag->name...." },
	{ "maya_axes_anim_7700_ascii", 7539, -1, 0, 8553, 0, 0, 0, "ufbxi_xml_parse_tag(xc, depth + 1, &closing, tag->name...." },
	{ "maya_axes_anim_7700_ascii", 7545, -1, 0, 27375, 0, 0, 0, "tag->children" },
	{ "maya_axes_anim_7700_ascii", 7545, -1, 0, 8595, 0, 0, 0, "tag->children" },
	{ "maya_axes_anim_7700_ascii", 7554, -1, 0, 27043, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 7554, -1, 0, 8542, 0, 0, 0, "tag" },
	{ "maya_axes_anim_7700_ascii", 7560, -1, 0, 27045, 0, 0, 0, "ufbxi_xml_parse_tag(xc, 0, &closing, ((void *)0))" },
	{ "maya_axes_anim_7700_ascii", 7560, -1, 0, 8543, 0, 0, 0, "ufbxi_xml_parse_tag(xc, 0, &closing, ((void *)0))" },
	{ "maya_axes_anim_7700_ascii", 7566, -1, 0, 27964, 0, 0, 0, "tag->children" },
	{ "maya_axes_anim_7700_ascii", 7566, -1, 0, 8674, 0, 0, 0, "tag->children" },
	{ "maya_axes_anim_7700_ascii", 7569, -1, 0, 27966, 0, 0, 0, "xc->doc" },
	{ "maya_axes_anim_7700_ascii", 7569, -1, 0, 8675, 0, 0, 0, "xc->doc" },
	{ "maya_character_6100_binary", 14723, -1, 0, 13724, 0, 0, 0, "character" },
	{ "maya_character_6100_binary", 15015, -1, 0, 13724, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_character_7500_binary", 14723, -1, 0, 22046, 0, 0, 0, "character" },
	{ "maya_character_7500_binary", 15015, -1, 0, 22046, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_color_sets_6100_binary", 13495, -1, 0, 0, 152, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 13495, -1, 0, 0, 76, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 13542, 10076, 95, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_color_sets_6100_binary", 20871, -1, 0, 0, 180, 0, 0, "indices->data" },
	{ "maya_color_sets_6100_binary", 20871, -1, 0, 0, 360, 0, 0, "indices->data" },
	{ "maya_color_sets_6100_binary", 20896, -1, 0, 0, 180, 0, 0, "ufbxi_flip_attrib_winding(uc, mesh, &mesh->vertex_norma..." },
	{ "maya_color_sets_6100_binary", 20896, -1, 0, 0, 360, 0, 0, "ufbxi_flip_attrib_winding(uc, mesh, &mesh->vertex_norma..." },
	{ "maya_color_sets_6100_binary", 20924, -1, 0, 3286, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_color_sets_6100_binary", 21018, -1, 0, 0, 180, 0, 0, "ufbxi_flip_winding(uc, mesh)" },
	{ "maya_color_sets_6100_binary", 21018, -1, 0, 3286, 0, 0, 0, "ufbxi_flip_winding(uc, mesh)" },
	{ "maya_color_sets_6100_binary", 25095, -1, 0, 0, 180, 0, 0, "ufbxi_modify_geometry(uc)" },
	{ "maya_color_sets_6100_binary", 25095, -1, 0, 3286, 0, 0, 0, "ufbxi_modify_geometry(uc)" },
	{ "maya_constraint_zoo_6100_binary", 14764, -1, 0, 13095, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 14764, -1, 0, 3514, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 15017, -1, 0, 13095, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 15017, -1, 0, 3514, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 20038, -1, 0, 14854, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 20038, -1, 0, 4014, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 22351, -1, 0, 14854, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 22351, -1, 0, 4014, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 22357, -1, 0, 0, 315, 0, 0, "constraint->targets.data" },
	{ "maya_constraint_zoo_6100_binary", 22357, -1, 0, 0, 630, 0, 0, "constraint->targets.data" },
	{ "maya_cube_6100_ascii", 10157, -1, 0, 1734, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 10157, -1, 0, 260, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 10179, 4870, 46, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 10254, 514, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 10260, 174, 0, 0, 0, 0, 0, "depth == 0" },
	{ "maya_cube_6100_ascii", 10270, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_cube_6100_ascii", 10274, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_cube_6100_ascii", 10274, -1, 0, 691, 0, 0, 0, "name" },
	{ "maya_cube_6100_ascii", 10279, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_cube_6100_ascii", 10279, -1, 0, 692, 0, 0, 0, "node" },
	{ "maya_cube_6100_ascii", 10304, -1, 0, 1728, 0, 0, 0, "arr" },
	{ "maya_cube_6100_ascii", 10304, -1, 0, 257, 0, 0, 0, "arr" },
	{ "maya_cube_6100_ascii", 10321, -1, 0, 1730, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_cube_6100_ascii", 10321, -1, 0, 258, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_cube_6100_ascii", 10325, -1, 0, 1732, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_cube_6100_ascii", 10325, -1, 0, 259, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_cube_6100_ascii", 10356, 4870, 46, 0, 0, 0, 0, "ufbxi_ascii_read_float_array(uc, (char)arr_type, &num_r..." },
	{ "maya_cube_6100_ascii", 10358, 4998, 45, 0, 0, 0, 0, "ufbxi_ascii_read_int_array(uc, (char)arr_type, &num_rea..." },
	{ "maya_cube_6100_ascii", 10406, -1, 0, 0, 2, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_cube_6100_ascii", 10406, -1, 0, 716, 0, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_cube_6100_ascii", 10435, -1, 0, 2432, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 10435, -1, 0, 494, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 10437, -1, 0, 1806, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 10440, -1, 0, 1982, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 10468, -1, 0, 1769, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 10588, -1, 0, 1829, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_ascii", 10588, -1, 0, 287, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_ascii", 10629, -1, 0, 701, 0, 0, 0, "node->vals" },
	{ "maya_cube_6100_ascii", 10629, -1, 0, 8, 0, 0, 0, "node->vals" },
	{ "maya_cube_6100_ascii", 10639, 174, 11, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_cube_6100_ascii", 10646, -1, 0, 20, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_ascii", 10646, -1, 0, 740, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_ascii", 11190, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_cube_6100_ascii", 11190, -1, 0, 689, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_cube_6100_ascii", 11207, 100, 33, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_cube_6100_ascii", 9448, -1, 0, 0, 0, 0, 57, "ufbxi_report_progress(uc)" },
	{ "maya_cube_6100_ascii", 9588, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_cube_6100_ascii", 9588, -1, 0, 689, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_cube_6100_ascii", 9728, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 9728, -1, 0, 689, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 9747, -1, 0, 6, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 9747, -1, 0, 696, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 9765, 514, 0, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_cube_6100_ascii", 9770, 4541, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_cube_6100_ascii", 9845, 12038, 35, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_cube_6100_ascii", 9865, 1138, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 9902, -1, 0, 1777, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9902, -1, 0, 275, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 9921, 4998, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_binary", 10268, 0, 76, 0, 0, 0, 0, "Expected a 'Name:' token" },
	{ "maya_cube_6100_binary", 10686, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 10687, -1, 0, 5, 0, 0, 0, "((ufbx_dom_node**)ufbxi_push_size_copy((&uc->tmp_dom_no..." },
	{ "maya_cube_6100_binary", 10687, -1, 0, 694, 0, 0, 0, "((ufbx_dom_node**)ufbxi_push_size_copy((&uc->tmp_dom_no..." },
	{ "maya_cube_6100_binary", 10702, -1, 0, 6, 0, 0, 0, "result" },
	{ "maya_cube_6100_binary", 10702, -1, 0, 696, 0, 0, 0, "result" },
	{ "maya_cube_6100_binary", 10708, -1, 0, 698, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst->nam..." },
	{ "maya_cube_6100_binary", 10713, -1, 0, 0, 302, 0, 0, "val" },
	{ "maya_cube_6100_binary", 10713, -1, 0, 0, 604, 0, 0, "val" },
	{ "maya_cube_6100_binary", 10742, -1, 0, 707, 0, 0, 0, "val" },
	{ "maya_cube_6100_binary", 10742, -1, 0, 9, 0, 0, 0, "val" },
	{ "maya_cube_6100_binary", 10759, -1, 0, 0, 2, 0, 0, "dst->values.data" },
	{ "maya_cube_6100_binary", 10759, -1, 0, 0, 4, 0, 0, "dst->values.data" },
	{ "maya_cube_6100_binary", 10764, -1, 0, 28, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 10764, -1, 0, 766, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 10769, -1, 0, 0, 24, 0, 0, "dst->children.data" },
	{ "maya_cube_6100_binary", 10769, -1, 0, 0, 48, 0, 0, "dst->children.data" },
	{ "maya_cube_6100_binary", 10779, -1, 0, 0, 114, 0, 0, "children" },
	{ "maya_cube_6100_binary", 10779, -1, 0, 0, 57, 0, 0, "children" },
	{ "maya_cube_6100_binary", 10786, -1, 0, 5, 0, 0, 0, "ufbxi_retain_dom_node(uc, node, &uc->dom_parse_toplevel..." },
	{ "maya_cube_6100_binary", 10786, -1, 0, 694, 0, 0, 0, "ufbxi_retain_dom_node(uc, node, &uc->dom_parse_toplevel..." },
	{ "maya_cube_6100_binary", 10793, -1, 0, 0, 1460, 0, 0, "nodes" },
	{ "maya_cube_6100_binary", 10793, -1, 0, 0, 730, 0, 0, "nodes" },
	{ "maya_cube_6100_binary", 10796, -1, 0, 0, 1462, 0, 0, "dom_root" },
	{ "maya_cube_6100_binary", 10796, -1, 0, 0, 731, 0, 0, "dom_root" },
	{ "maya_cube_6100_binary", 10811, -1, 0, 704, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 10811, -1, 0, 9, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 11158, -1, 0, 0, 0, 1, 0, "header" },
	{ "maya_cube_6100_binary", 11197, 0, 76, 0, 0, 0, 0, "uc->version > 0" },
	{ "maya_cube_6100_binary", 11209, 35, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_cube_6100_binary", 11236, 0, 76, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_cube_6100_binary", 11238, 22, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_cube_6100_binary", 11247, -1, 0, 0, 1458, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "maya_cube_6100_binary", 11247, -1, 0, 0, 729, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "maya_cube_6100_binary", 11257, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 11257, -1, 0, 692, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 11261, -1, 0, 5, 0, 0, 0, "ufbxi_retain_toplevel(uc, node)" },
	{ "maya_cube_6100_binary", 11261, -1, 0, 694, 0, 0, 0, "ufbxi_retain_toplevel(uc, node)" },
	{ "maya_cube_6100_binary", 11276, 39, 19, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_cube_6100_binary", 11284, -1, 0, 1061, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 11284, -1, 0, 61, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 11288, -1, 0, 1178, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &node->children[i])" },
	{ "maya_cube_6100_binary", 11288, -1, 0, 94, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &node->children[i])" },
	{ "maya_cube_6100_binary", 11309, 35, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, tmp_buf ? tmp..." },
	{ "maya_cube_6100_binary", 11324, -1, 0, 704, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, dst)" },
	{ "maya_cube_6100_binary", 11324, -1, 0, 9, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, dst)" },
	{ "maya_cube_6100_binary", 11387, -1, 0, 1, 0, 0, 0, "ufbxi_push_string_imp(&uc->string_pool, str->data, str-..." },
	{ "maya_cube_6100_binary", 11674, -1, 0, 41, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_cube_6100_binary", 11674, -1, 0, 805, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_cube_6100_binary", 11678, -1, 0, 807, 0, 0, 0, "pooled" },
	{ "maya_cube_6100_binary", 11681, -1, 0, 809, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 11700, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_cube_6100_binary", 11700, -1, 0, 599, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_cube_6100_binary", 11703, -1, 0, 601, 0, 0, 0, "pooled" },
	{ "maya_cube_6100_binary", 11706, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 11706, -1, 0, 603, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 11753, 1442, 0, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_cube_6100_binary", 11832, -1, 0, 2113, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 11832, -1, 0, 395, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 11874, -1, 0, 0, 114, 0, 0, "props->props.data" },
	{ "maya_cube_6100_binary", 11874, -1, 0, 0, 57, 0, 0, "props->props.data" },
	{ "maya_cube_6100_binary", 11877, 1442, 0, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_cube_6100_binary", 11880, -1, 0, 2113, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_cube_6100_binary", 11880, -1, 0, 395, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_cube_6100_binary", 11923, -1, 0, 0, 95, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_cube_6100_binary", 11923, -1, 0, 2370, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_cube_6100_binary", 11941, 35, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child, ((void *)0))" },
	{ "maya_cube_6100_binary", 12100, 954, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object, ((void *)0))" },
	{ "maya_cube_6100_binary", 12107, -1, 0, 1095, 0, 0, 0, "tmpl" },
	{ "maya_cube_6100_binary", 12107, -1, 0, 73, 0, 0, 0, "tmpl" },
	{ "maya_cube_6100_binary", 12108, 1022, 0, 0, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_cube_6100_binary", 12135, -1, 0, 0, 24, 0, 0, "uc->templates" },
	{ "maya_cube_6100_binary", 12135, -1, 0, 0, 48, 0, 0, "uc->templates" },
	{ "maya_cube_6100_binary", 12187, -1, 0, 1007, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 12246, -1, 0, 2110, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_cube_6100_binary", 12247, -1, 0, 0, 56, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_cube_6100_binary", 12247, -1, 0, 2111, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_cube_6100_binary", 12259, -1, 0, 1019, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 12259, -1, 0, 47, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 12289, -1, 0, 2119, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 12289, -1, 0, 396, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 12304, -1, 0, 1009, 0, 0, 0, "((size_t*)ufbxi_push_size_copy_fast((&uc->tmp_typed_ele..." },
	{ "maya_cube_6100_binary", 12304, -1, 0, 42, 0, 0, 0, "((size_t*)ufbxi_push_size_copy_fast((&uc->tmp_typed_ele..." },
	{ "maya_cube_6100_binary", 12305, -1, 0, 1011, 0, 0, 0, "((size_t*)ufbxi_push_size_copy_fast((&uc->tmp_element_o..." },
	{ "maya_cube_6100_binary", 12305, -1, 0, 43, 0, 0, 0, "((size_t*)ufbxi_push_size_copy_fast((&uc->tmp_element_o..." },
	{ "maya_cube_6100_binary", 12306, -1, 0, 1013, 0, 0, 0, "((uint64_t*)ufbxi_push_size_copy_fast((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 12306, -1, 0, 44, 0, 0, 0, "((uint64_t*)ufbxi_push_size_copy_fast((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 12310, -1, 0, 1015, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 12310, -1, 0, 45, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 12322, -1, 0, 1017, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy_fast((&uc->tmp_el..." },
	{ "maya_cube_6100_binary", 12322, -1, 0, 46, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy_fast((&uc->tmp_el..." },
	{ "maya_cube_6100_binary", 12324, -1, 0, 1019, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 12324, -1, 0, 47, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 12336, -1, 0, 2798, 0, 0, 0, "((size_t*)ufbxi_push_size_copy_fast((&uc->tmp_typed_ele..." },
	{ "maya_cube_6100_binary", 12336, -1, 0, 583, 0, 0, 0, "((size_t*)ufbxi_push_size_copy_fast((&uc->tmp_typed_ele..." },
	{ "maya_cube_6100_binary", 12337, -1, 0, 2800, 0, 0, 0, "((size_t*)ufbxi_push_size_copy_fast((&uc->tmp_element_o..." },
	{ "maya_cube_6100_binary", 12337, -1, 0, 584, 0, 0, 0, "((size_t*)ufbxi_push_size_copy_fast((&uc->tmp_element_o..." },
	{ "maya_cube_6100_binary", 12341, -1, 0, 2802, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 12341, -1, 0, 585, 0, 0, 0, "elem" },
	{ "maya_cube_6100_binary", 12351, -1, 0, 2804, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy_fast((&uc->tmp_el..." },
	{ "maya_cube_6100_binary", 12351, -1, 0, 586, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy_fast((&uc->tmp_el..." },
	{ "maya_cube_6100_binary", 12355, -1, 0, 2806, 0, 0, 0, "((uint64_t*)ufbxi_push_size_copy_fast((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 12355, -1, 0, 587, 0, 0, 0, "((uint64_t*)ufbxi_push_size_copy_fast((&uc->tmp_element..." },
	{ "maya_cube_6100_binary", 12356, -1, 0, 2808, 0, 0, 0, "ufbxi_insert_fbx_id(uc, *p_fbx_id, element_id)" },
	{ "maya_cube_6100_binary", 12367, -1, 0, 2147, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 12367, -1, 0, 407, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 12550, -1, 0, 2149, 0, 0, 0, "elem_node" },
	{ "maya_cube_6100_binary", 12550, -1, 0, 408, 0, 0, 0, "elem_node" },
	{ "maya_cube_6100_binary", 12551, -1, 0, 2159, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 12698, 7295, 255, 0, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 12699, 7448, 71, 0, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_cube_6100_binary", 12923, -1, 0, 2145, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 12930, -1, 0, 2146, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 13093, -1, 0, 0, 122, 0, 0, "mesh->faces.data" },
	{ "maya_cube_6100_binary", 13093, -1, 0, 0, 61, 0, 0, "mesh->faces.data" },
	{ "maya_cube_6100_binary", 13119, 6763, 0, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_cube_6100_binary", 13133, -1, 0, 0, 124, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_6100_binary", 13133, -1, 0, 0, 62, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_6100_binary", 13382, -1, 0, 2133, 0, 0, 0, "mesh" },
	{ "maya_cube_6100_binary", 13382, -1, 0, 403, 0, 0, 0, "mesh" },
	{ "maya_cube_6100_binary", 13404, 6763, 23, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_cube_6100_binary", 13414, -1, 0, 0, 437, 0, 0, "index_data" },
	{ "maya_cube_6100_binary", 13414, -1, 0, 0, 874, 0, 0, "index_data" },
	{ "maya_cube_6100_binary", 13432, 7000, 23, 0, 0, 0, 0, "Non-negated last index" },
	{ "maya_cube_6100_binary", 13441, -1, 0, 0, 120, 0, 0, "edges" },
	{ "maya_cube_6100_binary", 13441, -1, 0, 0, 60, 0, 0, "edges" },
	{ "maya_cube_6100_binary", 13451, 7000, 0, 0, 0, 0, 0, "Edge index out of bounds" },
	{ "maya_cube_6100_binary", 13474, 6763, 0, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "maya_cube_6100_binary", 13489, -1, 0, 2141, 0, 0, 0, "bitangents" },
	{ "maya_cube_6100_binary", 13489, -1, 0, 405, 0, 0, 0, "bitangents" },
	{ "maya_cube_6100_binary", 13490, -1, 0, 2143, 0, 0, 0, "tangents" },
	{ "maya_cube_6100_binary", 13490, -1, 0, 406, 0, 0, 0, "tangents" },
	{ "maya_cube_6100_binary", 13494, -1, 0, 0, 126, 0, 0, "mesh->uv_sets.data" },
	{ "maya_cube_6100_binary", 13494, -1, 0, 0, 63, 0, 0, "mesh->uv_sets.data" },
	{ "maya_cube_6100_binary", 13504, 7295, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 13510, 8164, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 13518, 9038, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 13530, 9906, 255, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 13586, 10846, 255, 0, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_cube_6100_binary", 13618, 7283, 0, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "maya_cube_6100_binary", 13662, 9960, 0, 0, 0, 0, 0, "mesh->uv_sets.count == num_uv" },
	{ "maya_cube_6100_binary", 13664, 8218, 0, 0, 0, 0, 0, "num_bitangents_read == num_bitangents" },
	{ "maya_cube_6100_binary", 13665, 9092, 0, 0, 0, 0, 0, "num_tangents_read == num_tangents" },
	{ "maya_cube_6100_binary", 13727, -1, 0, 2145, 0, 0, 0, "ufbxi_sort_uv_sets(uc, mesh->uv_sets.data, mesh->uv_set..." },
	{ "maya_cube_6100_binary", 13728, -1, 0, 2146, 0, 0, 0, "ufbxi_sort_color_sets(uc, mesh->color_sets.data, mesh->..." },
	{ "maya_cube_6100_binary", 14482, -1, 0, 2581, 0, 0, 0, "material" },
	{ "maya_cube_6100_binary", 14482, -1, 0, 518, 0, 0, 0, "material" },
	{ "maya_cube_6100_binary", 14797, -1, 0, 0, 58, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_cube_6100_binary", 14797, -1, 0, 2115, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_cube_6100_binary", 14801, -1, 0, 2118, 0, 0, 0, "attrib_id" },
	{ "maya_cube_6100_binary", 14810, -1, 0, 2119, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_cube_6100_binary", 14810, -1, 0, 396, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_cube_6100_binary", 14818, -1, 0, 2121, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_cube_6100_binary", 14818, -1, 0, 397, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_cube_6100_binary", 14828, -1, 0, 0, 118, 0, 0, "attrib_info.props.props.data" },
	{ "maya_cube_6100_binary", 14828, -1, 0, 0, 59, 0, 0, "attrib_info.props.props.data" },
	{ "maya_cube_6100_binary", 14832, 6763, 23, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_cube_6100_binary", 14872, -1, 0, 2147, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_cube_6100_binary", 14872, -1, 0, 407, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_cube_6100_binary", 14878, 15140, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_cube_6100_binary", 14888, 15140, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_cube_6100_binary", 14905, -1, 0, 2109, 0, 0, 0, "info.fbx_id" },
	{ "maya_cube_6100_binary", 14916, -1, 0, 0, 56, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_cube_6100_binary", 14916, -1, 0, 2110, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_cube_6100_binary", 14919, 1442, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_cube_6100_binary", 14924, 6763, 23, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, type_st..." },
	{ "maya_cube_6100_binary", 14926, -1, 0, 2149, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_cube_6100_binary", 14926, -1, 0, 408, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_cube_6100_binary", 14982, -1, 0, 2581, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_cube_6100_binary", 14982, -1, 0, 518, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_cube_6100_binary", 15020, -1, 0, 0, 95, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cube_6100_binary", 15020, -1, 0, 2370, 0, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cube_6100_binary", 15041, -1, 0, 1146, 0, 0, 0, "uc->p_element_id" },
	{ "maya_cube_6100_binary", 15041, -1, 0, 93, 0, 0, 0, "uc->p_element_id" },
	{ "maya_cube_6100_binary", 15046, 1331, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node, ((void *)0))" },
	{ "maya_cube_6100_binary", 15049, 1442, 0, 0, 0, 0, 0, "ufbxi_read_object(uc, node)" },
	{ "maya_cube_6100_binary", 15176, 16292, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node, ((void *)0))" },
	{ "maya_cube_6100_binary", 15237, -1, 0, 2749, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 15237, -1, 0, 567, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 15587, 16506, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"C\", (char**)&name)" },
	{ "maya_cube_6100_binary", 15611, -1, 0, 2798, 0, 0, 0, "stack" },
	{ "maya_cube_6100_binary", 15611, -1, 0, 583, 0, 0, 0, "stack" },
	{ "maya_cube_6100_binary", 15615, -1, 0, 0, 122, 0, 0, "stack->props.props.data" },
	{ "maya_cube_6100_binary", 15615, -1, 0, 0, 244, 0, 0, "stack->props.props.data" },
	{ "maya_cube_6100_binary", 15618, -1, 0, 2809, 0, 0, 0, "layer" },
	{ "maya_cube_6100_binary", 15618, -1, 0, 588, 0, 0, 0, "layer" },
	{ "maya_cube_6100_binary", 15620, -1, 0, 2817, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_cube_6100_binary", 15620, -1, 0, 590, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_cube_6100_binary", 15637, 16459, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node, ((void *)0))" },
	{ "maya_cube_6100_binary", 15641, 16506, 0, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_cube_6100_binary", 15682, -1, 0, 0, 142, 0, 0, "new_props" },
	{ "maya_cube_6100_binary", 15682, -1, 0, 0, 284, 0, 0, "new_props" },
	{ "maya_cube_6100_binary", 15689, -1, 0, 2946, 0, 0, 0, "ufbxi_sort_properties(uc, new_props, new_count)" },
	{ "maya_cube_6100_binary", 15729, 0, 76, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_cube_6100_binary", 15730, 35, 1, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "maya_cube_6100_binary", 15743, -1, 0, 41, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_cube_6100_binary", 15743, -1, 0, 805, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_cube_6100_binary", 15757, -1, 0, 1005, 0, 0, 0, "root_name" },
	{ "maya_cube_6100_binary", 15759, -1, 0, 1007, 0, 0, 0, "uc->root_id" },
	{ "maya_cube_6100_binary", 15767, -1, 0, 1009, 0, 0, 0, "root" },
	{ "maya_cube_6100_binary", 15767, -1, 0, 42, 0, 0, 0, "root" },
	{ "maya_cube_6100_binary", 15769, -1, 0, 1021, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 15769, -1, 0, 48, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 15773, 59, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_cube_6100_binary", 15774, 954, 1, 0, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_cube_6100_binary", 15777, 954, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_cube_6100_binary", 15781, 0, 0, 0, 0, 0, 0, "uc->top_node" },
	{ "maya_cube_6100_binary", 15786, 1331, 1, 0, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_cube_6100_binary", 15790, 16288, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_cube_6100_binary", 15791, 16292, 1, 0, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_cube_6100_binary", 15794, 16309, 64, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_cube_6100_binary", 15795, 16459, 1, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_cube_6100_binary", 15798, 16470, 65, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_cube_6100_binary", 15808, -1, 0, 0, 142, 0, 0, "ufbxi_read_legacy_settings(uc, settings)" },
	{ "maya_cube_6100_binary", 15808, -1, 0, 2946, 0, 0, 0, "ufbxi_read_legacy_settings(uc, settings)" },
	{ "maya_cube_6100_binary", 16398, -1, 0, 0, 143, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 16398, -1, 0, 2947, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 16399, -1, 0, 2949, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &uc->sce..." },
	{ "maya_cube_6100_binary", 16407, -1, 0, 0, 144, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 16407, -1, 0, 2950, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_6100_binary", 16408, -1, 0, 2952, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &uc->sce..." },
	{ "maya_cube_6100_binary", 17989, -1, 0, 2953, 0, 0, 0, "elements" },
	{ "maya_cube_6100_binary", 17993, -1, 0, 2955, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 17996, -1, 0, 2957, 0, 0, 0, "pre_connections" },
	{ "maya_cube_6100_binary", 17999, -1, 0, 2959, 0, 0, 0, "instance_counts" },
	{ "maya_cube_6100_binary", 18002, -1, 0, 2961, 0, 0, 0, "modify_not_supported" },
	{ "maya_cube_6100_binary", 18005, -1, 0, 2962, 0, 0, 0, "has_unscaled_children" },
	{ "maya_cube_6100_binary", 18008, -1, 0, 2963, 0, 0, 0, "has_scale_animation" },
	{ "maya_cube_6100_binary", 18011, -1, 0, 2965, 0, 0, 0, "pre_nodes" },
	{ "maya_cube_6100_binary", 18015, -1, 0, 2967, 0, 0, 0, "pre_meshes" },
	{ "maya_cube_6100_binary", 18022, -1, 0, 2968, 0, 0, 0, "fbx_ids" },
	{ "maya_cube_6100_binary", 18371, -1, 0, 2998, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 18399, -1, 0, 2984, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 18435, -1, 0, 2976, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 18455, -1, 0, 2974, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 18455, -1, 0, 635, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_6100_binary", 18459, -1, 0, 0, 147, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_cube_6100_binary", 18459, -1, 0, 0, 294, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_cube_6100_binary", 18556, -1, 0, 0, 148, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_cube_6100_binary", 18556, -1, 0, 0, 296, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_cube_6100_binary", 18558, -1, 0, 2976, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "maya_cube_6100_binary", 18559, -1, 0, 2977, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_dst.da..." },
	{ "maya_cube_6100_binary", 18704, -1, 0, 2978, 0, 0, 0, "node_ids" },
	{ "maya_cube_6100_binary", 18704, -1, 0, 636, 0, 0, 0, "node_ids" },
	{ "maya_cube_6100_binary", 18707, -1, 0, 2980, 0, 0, 0, "node_ptrs" },
	{ "maya_cube_6100_binary", 18707, -1, 0, 637, 0, 0, 0, "node_ptrs" },
	{ "maya_cube_6100_binary", 18718, -1, 0, 2982, 0, 0, 0, "node_offsets" },
	{ "maya_cube_6100_binary", 18718, -1, 0, 638, 0, 0, 0, "node_offsets" },
	{ "maya_cube_6100_binary", 18762, -1, 0, 2984, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "maya_cube_6100_binary", 18766, -1, 0, 2985, 0, 0, 0, "p_offset" },
	{ "maya_cube_6100_binary", 18766, -1, 0, 639, 0, 0, 0, "p_offset" },
	{ "maya_cube_6100_binary", 18849, -1, 0, 2999, 0, 0, 0, "p_elem" },
	{ "maya_cube_6100_binary", 18849, -1, 0, 645, 0, 0, 0, "p_elem" },
	{ "maya_cube_6100_binary", 18859, -1, 0, 0, 155, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 18859, -1, 0, 0, 310, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 18887, -1, 0, 3001, 0, 0, 0, "p_elem" },
	{ "maya_cube_6100_binary", 18887, -1, 0, 646, 0, 0, 0, "p_elem" },
	{ "maya_cube_6100_binary", 18897, -1, 0, 0, 156, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 18897, -1, 0, 0, 312, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 18969, -1, 0, 3003, 0, 0, 0, "((ufbx_material**)ufbxi_push_size_copy((&uc->tmp_stack)..." },
	{ "maya_cube_6100_binary", 18969, -1, 0, 647, 0, 0, 0, "((ufbx_material**)ufbxi_push_size_copy((&uc->tmp_stack)..." },
	{ "maya_cube_6100_binary", 18979, -1, 0, 0, 159, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 18979, -1, 0, 0, 318, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 19088, -1, 0, 3009, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 19102, -1, 0, 3011, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 21244, -1, 0, 3010, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 21298, -1, 0, 3010, 0, 0, 0, "ufbxi_sort_file_contents(uc, uc->file_content, uc->num_..." },
	{ "maya_cube_6100_binary", 21415, -1, 0, 0, 161, 0, 0, "anim" },
	{ "maya_cube_6100_binary", 21415, -1, 0, 0, 322, 0, 0, "anim" },
	{ "maya_cube_6100_binary", 21430, -1, 0, 0, 145, 0, 0, "uc->scene.elements.data" },
	{ "maya_cube_6100_binary", 21430, -1, 0, 0, 290, 0, 0, "uc->scene.elements.data" },
	{ "maya_cube_6100_binary", 21434, -1, 0, 0, 146, 0, 0, "element_data" },
	{ "maya_cube_6100_binary", 21434, -1, 0, 0, 292, 0, 0, "element_data" },
	{ "maya_cube_6100_binary", 21438, -1, 0, 2970, 0, 0, 0, "element_offsets" },
	{ "maya_cube_6100_binary", 21438, -1, 0, 633, 0, 0, 0, "element_offsets" },
	{ "maya_cube_6100_binary", 21459, -1, 0, 2972, 0, 0, 0, "uc->tmp_element_flag" },
	{ "maya_cube_6100_binary", 21459, -1, 0, 634, 0, 0, 0, "uc->tmp_element_flag" },
	{ "maya_cube_6100_binary", 21465, -1, 0, 2974, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_cube_6100_binary", 21465, -1, 0, 635, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_cube_6100_binary", 21467, -1, 0, 2978, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_cube_6100_binary", 21467, -1, 0, 636, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_cube_6100_binary", 21473, -1, 0, 2988, 0, 0, 0, "typed_offsets" },
	{ "maya_cube_6100_binary", 21473, -1, 0, 640, 0, 0, 0, "typed_offsets" },
	{ "maya_cube_6100_binary", 21478, -1, 0, 0, 149, 0, 0, "typed_elems->data" },
	{ "maya_cube_6100_binary", 21478, -1, 0, 0, 298, 0, 0, "typed_elems->data" },
	{ "maya_cube_6100_binary", 21490, -1, 0, 0, 154, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_cube_6100_binary", 21490, -1, 0, 0, 308, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_cube_6100_binary", 21503, -1, 0, 2998, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "maya_cube_6100_binary", 21567, -1, 0, 2999, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_cube_6100_binary", 21567, -1, 0, 645, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_cube_6100_binary", 21615, -1, 0, 3001, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_cube_6100_binary", 21615, -1, 0, 646, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_cube_6100_binary", 21799, -1, 0, 0, 157, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_cube_6100_binary", 21799, -1, 0, 0, 314, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_cube_6100_binary", 21851, -1, 0, 3003, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_cube_6100_binary", 21851, -1, 0, 647, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_cube_6100_binary", 21875, -1, 0, 0, 160, 0, 0, "mesh->material_parts.data" },
	{ "maya_cube_6100_binary", 21875, -1, 0, 0, 320, 0, 0, "mesh->material_parts.data" },
	{ "maya_cube_6100_binary", 21949, -1, 0, 3005, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_cube_6100_binary", 21949, -1, 0, 648, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_cube_6100_binary", 21951, -1, 0, 0, 161, 0, 0, "ufbxi_push_anim(uc, &stack->anim, stack->layers.data, s..." },
	{ "maya_cube_6100_binary", 21951, -1, 0, 0, 322, 0, 0, "ufbxi_push_anim(uc, &stack->anim, stack->layers.data, s..." },
	{ "maya_cube_6100_binary", 21958, -1, 0, 0, 162, 0, 0, "ufbxi_push_anim(uc, &layer->anim, p_layer, 1)" },
	{ "maya_cube_6100_binary", 21958, -1, 0, 0, 324, 0, 0, "ufbxi_push_anim(uc, &layer->anim, p_layer, 1)" },
	{ "maya_cube_6100_binary", 22025, -1, 0, 3007, 0, 0, 0, "aprop" },
	{ "maya_cube_6100_binary", 22025, -1, 0, 649, 0, 0, 0, "aprop" },
	{ "maya_cube_6100_binary", 22029, -1, 0, 0, 163, 0, 0, "layer->anim_props.data" },
	{ "maya_cube_6100_binary", 22029, -1, 0, 0, 326, 0, 0, "layer->anim_props.data" },
	{ "maya_cube_6100_binary", 22031, -1, 0, 3009, 0, 0, 0, "ufbxi_sort_anim_props(uc, layer->anim_props.data, layer..." },
	{ "maya_cube_6100_binary", 22239, -1, 0, 3010, 0, 0, 0, "ufbxi_resolve_file_content(uc)" },
	{ "maya_cube_6100_binary", 22288, -1, 0, 3011, 0, 0, 0, "ufbxi_sort_material_textures(uc, material->textures.dat..." },
	{ "maya_cube_6100_binary", 24941, -1, 0, 3012, 0, 0, 0, "element_ids" },
	{ "maya_cube_6100_binary", 24941, -1, 0, 650, 0, 0, 0, "element_ids" },
	{ "maya_cube_6100_binary", 25036, -1, 0, 1, 0, 0, 0, "ufbxi_load_strings(uc)" },
	{ "maya_cube_6100_binary", 25037, -1, 0, 1, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_cube_6100_binary", 25037, -1, 0, 599, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_cube_6100_binary", 25043, 0, 76, 0, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_cube_6100_binary", 25047, 0, 76, 0, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_cube_6100_binary", 25053, -1, 0, 0, 143, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_6100_binary", 25053, -1, 0, 2947, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_6100_binary", 25070, -1, 0, 2953, 0, 0, 0, "ufbxi_pre_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 25075, -1, 0, 2970, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 25075, -1, 0, 633, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cube_6100_binary", 25119, -1, 0, 3012, 0, 0, 0, "ufbxi_resolve_warning_elements(uc)" },
	{ "maya_cube_6100_binary", 25119, -1, 0, 650, 0, 0, 0, "ufbxi_resolve_warning_elements(uc)" },
	{ "maya_cube_6100_binary", 25132, -1, 0, 0, 164, 0, 0, "imp" },
	{ "maya_cube_6100_binary", 25132, -1, 0, 0, 328, 0, 0, "imp" },
	{ "maya_cube_6100_binary", 3648, 6765, 255, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_cube_6100_binary", 3653, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3698, -1, 0, 1032, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3698, -1, 0, 51, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 3754, -1, 0, 692, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 4016, -1, 0, 689, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 4068, -1, 0, 1009, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 4505, -1, 0, 1, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 4563, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_6100_binary", 5155, -1, 0, 720, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "maya_cube_6100_binary", 5180, -1, 0, 721, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 5183, -1, 0, 0, 3, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 5183, -1, 0, 0, 6, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 5197, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "maya_cube_6100_binary", 5223, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 5227, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 5248, -1, 0, 0, 56, 0, 0, "str" },
	{ "maya_cube_6100_binary", 5248, -1, 0, 698, 0, 0, 0, "str" },
	{ "maya_cube_6100_binary", 5266, -1, 0, 2949, 0, 0, 0, "p_blob->data" },
	{ "maya_cube_6100_binary", 6665, -1, 0, 0, 0, 0, 1, "result != UFBX_PROGRESS_CANCEL" },
	{ "maya_cube_6100_binary", 6684, -1, 0, 0, 0, 1, 0, "!uc->eof" },
	{ "maya_cube_6100_binary", 6686, -1, 0, 0, 0, 40, 0, "uc->read_fn || uc->data_size > 0" },
	{ "maya_cube_6100_binary", 6687, 36, 255, 0, 0, 0, 0, "uc->read_fn" },
	{ "maya_cube_6100_binary", 6778, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_cube_6100_binary", 6855, 36, 255, 0, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_cube_6100_binary", 8792, -1, 0, 0, 0, 7040, 0, "val" },
	{ "maya_cube_6100_binary", 8795, -1, 0, 0, 0, 6793, 0, "val" },
	{ "maya_cube_6100_binary", 8833, 10670, 13, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 8834, 7000, 25, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 8837, 6763, 25, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 8858, 6765, 255, 0, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 8949, -1, 0, 0, 0, 27, 0, "header" },
	{ "maya_cube_6100_binary", 8970, 24, 255, 0, 0, 0, 0, "num_values64 <= 0xffffffffui32" },
	{ "maya_cube_6100_binary", 8988, -1, 0, 3, 0, 0, 0, "node" },
	{ "maya_cube_6100_binary", 8988, -1, 0, 689, 0, 0, 0, "node" },
	{ "maya_cube_6100_binary", 8992, -1, 0, 0, 0, 40, 0, "name" },
	{ "maya_cube_6100_binary", 8994, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_cube_6100_binary", 8994, -1, 0, 691, 0, 0, 0, "name" },
	{ "maya_cube_6100_binary", 9012, -1, 0, 1756, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 9012, -1, 0, 263, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 9021, -1, 0, 0, 0, 6780, 0, "data" },
	{ "maya_cube_6100_binary", 9216, 6765, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_binary", 9217, 6763, 25, 0, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_cube_6100_binary", 9231, -1, 0, 6, 0, 0, 0, "vals" },
	{ "maya_cube_6100_binary", 9231, -1, 0, 697, 0, 0, 0, "vals" },
	{ "maya_cube_6100_binary", 9239, -1, 0, 0, 0, 87, 0, "data" },
	{ "maya_cube_6100_binary", 9292, 213, 255, 0, 0, 0, 0, "str" },
	{ "maya_cube_6100_binary", 9302, -1, 0, 0, 3, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_cube_6100_binary", 9302, -1, 0, 720, 0, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_cube_6100_binary", 9317, 164, 0, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_cube_6100_binary", 9322, 22, 1, 0, 0, 0, 0, "Bad value type" },
	{ "maya_cube_6100_binary", 9333, 66, 4, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_cube_6100_binary", 9335, 36, 255, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_cube_6100_binary", 9347, 58, 93, 0, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_cube_6100_binary", 9352, 70, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_cube_6100_binary", 9361, -1, 0, 20, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 9361, -1, 0, 742, 0, 0, 0, "node->children" },
	{ "maya_cube_7100_ascii", 10526, 8925, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'I')" },
	{ "maya_cube_7100_ascii", 10530, 8929, 11, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_cube_7100_ascii", 10563, 8935, 33, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_cube_7100_binary", 11756, 6091, 0, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_cube_7100_binary", 11968, 797, 0, 0, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_cube_7100_binary", 12081, 3549, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child, ((void *)0))" },
	{ "maya_cube_7100_binary", 12114, 4105, 0, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_cube_7100_binary", 12126, -1, 0, 0, 57, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_cube_7100_binary", 12126, -1, 0, 1347, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_cube_7100_binary", 12129, 4176, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_cube_7100_binary", 12578, -1, 0, 2994, 0, 0, 0, "elem" },
	{ "maya_cube_7100_binary", 12578, -1, 0, 662, 0, 0, 0, "elem" },
	{ "maya_cube_7100_binary", 14576, -1, 0, 2970, 0, 0, 0, "stack" },
	{ "maya_cube_7100_binary", 14576, -1, 0, 653, 0, 0, 0, "stack" },
	{ "maya_cube_7100_binary", 14582, -1, 0, 2981, 0, 0, 0, "entry" },
	{ "maya_cube_7100_binary", 14582, -1, 0, 658, 0, 0, 0, "entry" },
	{ "maya_cube_7100_binary", 14951, 12362, 0, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_cube_7100_binary", 14990, -1, 0, 2970, 0, 0, 0, "ufbxi_read_anim_stack(uc, node, &info)" },
	{ "maya_cube_7100_binary", 14990, -1, 0, 653, 0, 0, 0, "ufbxi_read_anim_stack(uc, node, &info)" },
	{ "maya_cube_7100_binary", 14992, -1, 0, 2994, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_cube_7100_binary", 14992, -1, 0, 662, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_cube_7100_binary", 15750, 59, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_cube_7100_binary", 15751, 3549, 1, 0, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_cube_7100_binary", 15800, 2241, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "maya_cube_7100_binary", 15804, 19077, 75, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Version5)" },
	{ "maya_cube_7100_binary", 3697, 16067, 1, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_cube_7100_binary", 6760, -1, 0, 0, 0, 0, 1434, "ufbxi_report_progress(uc)" },
	{ "maya_cube_7100_binary", 6883, -1, 0, 0, 0, 12392, 0, "uc->read_fn" },
	{ "maya_cube_7100_binary", 6896, -1, 0, 0, 0, 0, 1434, "ufbxi_resume_progress(uc)" },
	{ "maya_cube_7100_binary", 9058, 12382, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_cube_7100_binary", 9113, 16067, 1, 0, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_7100_binary", 9121, 12379, 99, 0, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_cube_7100_binary", 9137, -1, 0, 0, 0, 12392, 0, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_cube_7100_binary", 9194, 12384, 1, 0, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_cube_7100_binary", 9197, 12384, 255, 0, 0, 0, 0, "Bad array encoding" },
	{ "maya_cube_7400_ascii", 10535, -1, 0, 0, 0, 9568, 0, "ufbxi_ascii_skip_until(uc, '}')" },
	{ "maya_cube_7400_ascii", 9625, -1, 0, 0, 0, 9568, 0, "c != '\\0'" },
	{ "maya_cube_7500_binary", 11349, 24, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_cube_7500_binary", 16325, 24, 0, 0, 0, 0, 0, "ufbxi_parse_legacy_toplevel(uc)" },
	{ "maya_cube_7500_binary", 25045, 24, 0, 0, 0, 0, 0, "ufbxi_read_legacy_root(uc)" },
	{ "maya_cube_big_endian_6100_binary", 11172, -1, 0, 3, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 11172, -1, 0, 689, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 8586, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 8586, -1, 0, 689, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 8962, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 8962, -1, 0, 691, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_7100_binary", 8650, -1, 0, 2362, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 8650, -1, 0, 455, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 9202, -1, 0, 2362, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7100_binary", 9202, -1, 0, 455, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7500_binary", 8953, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_7500_binary", 8953, -1, 0, 691, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_obj", 16616, -1, 0, 0, 12, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_big_endian_obj", 16616, -1, 0, 0, 24, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_big_endian_obj", 16622, -1, 0, 0, 13, 0, 0, "uv_set" },
	{ "maya_cube_big_endian_obj", 16622, -1, 0, 0, 26, 0, 0, "uv_set" },
	{ "maya_cube_big_endian_obj", 16692, -1, 0, 61, 0, 0, 0, "mesh" },
	{ "maya_cube_big_endian_obj", 16692, -1, 0, 995, 0, 0, 0, "mesh" },
	{ "maya_cube_big_endian_obj", 16710, -1, 0, 62, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "maya_cube_big_endian_obj", 16710, -1, 0, 997, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "maya_cube_big_endian_obj", 16714, -1, 0, 1015, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 16721, -1, 0, 1016, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "maya_cube_big_endian_obj", 16721, -1, 0, 68, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "maya_cube_big_endian_obj", 16722, -1, 0, 1018, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "maya_cube_big_endian_obj", 16722, -1, 0, 69, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "maya_cube_big_endian_obj", 16736, -1, 0, 0, 2, 0, 0, "groups" },
	{ "maya_cube_big_endian_obj", 16736, -1, 0, 0, 4, 0, 0, "groups" },
	{ "maya_cube_big_endian_obj", 16776, -1, 0, 3, 0, 0, 0, "root" },
	{ "maya_cube_big_endian_obj", 16776, -1, 0, 689, 0, 0, 0, "root" },
	{ "maya_cube_big_endian_obj", 16778, -1, 0, 701, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 16778, -1, 0, 9, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_big_endian_obj", 16849, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_cube_big_endian_obj", 16855, -1, 0, 1087, 0, 0, 0, "new_data" },
	{ "maya_cube_big_endian_obj", 16855, -1, 0, 83, 0, 0, 0, "new_data" },
	{ "maya_cube_big_endian_obj", 16915, -1, 0, 10, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "maya_cube_big_endian_obj", 16915, -1, 0, 703, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "maya_cube_big_endian_obj", 16951, -1, 0, 1087, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "maya_cube_big_endian_obj", 16951, -1, 0, 83, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "maya_cube_big_endian_obj", 16952, -1, 0, 10, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "maya_cube_big_endian_obj", 16952, -1, 0, 703, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "maya_cube_big_endian_obj", 16970, 92, 33, 0, 0, 0, 0, "offset + read_values <= uc->obj.num_tokens" },
	{ "maya_cube_big_endian_obj", 16974, -1, 0, 15, 0, 0, 0, "vals" },
	{ "maya_cube_big_endian_obj", 16974, -1, 0, 726, 0, 0, 0, "vals" },
	{ "maya_cube_big_endian_obj", 16979, 83, 46, 0, 0, 0, 0, "end == str.data + str.length" },
	{ "maya_cube_big_endian_obj", 17028, -1, 0, 1032, 0, 0, 0, "dst" },
	{ "maya_cube_big_endian_obj", 17028, -1, 0, 76, 0, 0, 0, "dst" },
	{ "maya_cube_big_endian_obj", 17070, -1, 0, 61, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 17070, -1, 0, 995, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 17112, -1, 0, 1020, 0, 0, 0, "entry" },
	{ "maya_cube_big_endian_obj", 17112, -1, 0, 70, 0, 0, 0, "entry" },
	{ "maya_cube_big_endian_obj", 17125, -1, 0, 1022, 0, 0, 0, "group" },
	{ "maya_cube_big_endian_obj", 17125, -1, 0, 71, 0, 0, 0, "group" },
	{ "maya_cube_big_endian_obj", 17144, -1, 0, 1024, 0, 0, 0, "face" },
	{ "maya_cube_big_endian_obj", 17144, -1, 0, 72, 0, 0, 0, "face" },
	{ "maya_cube_big_endian_obj", 17153, -1, 0, 1026, 0, 0, 0, "p_face_mat" },
	{ "maya_cube_big_endian_obj", 17153, -1, 0, 73, 0, 0, 0, "p_face_mat" },
	{ "maya_cube_big_endian_obj", 17158, -1, 0, 1028, 0, 0, 0, "p_face_smooth" },
	{ "maya_cube_big_endian_obj", 17158, -1, 0, 74, 0, 0, 0, "p_face_smooth" },
	{ "maya_cube_big_endian_obj", 17164, -1, 0, 1030, 0, 0, 0, "p_face_group" },
	{ "maya_cube_big_endian_obj", 17164, -1, 0, 75, 0, 0, 0, "p_face_group" },
	{ "maya_cube_big_endian_obj", 17171, -1, 0, 1032, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "maya_cube_big_endian_obj", 17171, -1, 0, 76, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "maya_cube_big_endian_obj", 17295, -1, 0, 0, 3, 0, 0, "data" },
	{ "maya_cube_big_endian_obj", 17295, -1, 0, 0, 6, 0, 0, "data" },
	{ "maya_cube_big_endian_obj", 17321, 71, 102, 0, 0, 0, 0, "num_indices == 0 || !required" },
	{ "maya_cube_big_endian_obj", 17333, -1, 0, 0, 18, 0, 0, "dst_indices" },
	{ "maya_cube_big_endian_obj", 17333, -1, 0, 0, 9, 0, 0, "dst_indices" },
	{ "maya_cube_big_endian_obj", 17378, -1, 0, 1089, 0, 0, 0, "meshes" },
	{ "maya_cube_big_endian_obj", 17378, -1, 0, 84, 0, 0, 0, "meshes" },
	{ "maya_cube_big_endian_obj", 17411, -1, 0, 1091, 0, 0, 0, "tmp_indices" },
	{ "maya_cube_big_endian_obj", 17411, -1, 0, 85, 0, 0, 0, "tmp_indices" },
	{ "maya_cube_big_endian_obj", 17435, -1, 0, 0, 3, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, m..." },
	{ "maya_cube_big_endian_obj", 17435, -1, 0, 0, 6, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, m..." },
	{ "maya_cube_big_endian_obj", 17452, -1, 0, 0, 12, 0, 0, "fbx_mesh->faces.data" },
	{ "maya_cube_big_endian_obj", 17452, -1, 0, 0, 6, 0, 0, "fbx_mesh->faces.data" },
	{ "maya_cube_big_endian_obj", 17453, -1, 0, 0, 14, 0, 0, "fbx_mesh->face_material.data" },
	{ "maya_cube_big_endian_obj", 17453, -1, 0, 0, 7, 0, 0, "fbx_mesh->face_material.data" },
	{ "maya_cube_big_endian_obj", 17458, -1, 0, 0, 16, 0, 0, "fbx_mesh->face_smoothing.data" },
	{ "maya_cube_big_endian_obj", 17458, -1, 0, 0, 8, 0, 0, "fbx_mesh->face_smoothing.data" },
	{ "maya_cube_big_endian_obj", 17472, 71, 102, 0, 0, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 17475, -1, 0, 0, 10, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 17475, -1, 0, 0, 20, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 17478, -1, 0, 0, 11, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 17478, -1, 0, 0, 22, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_big_endian_obj", 17520, -1, 0, 0, 12, 0, 0, "ufbxi_finalize_mesh(&uc->result, &uc->error, fbx_mesh)" },
	{ "maya_cube_big_endian_obj", 17520, -1, 0, 0, 24, 0, 0, "ufbxi_finalize_mesh(&uc->result, &uc->error, fbx_mesh)" },
	{ "maya_cube_big_endian_obj", 17525, -1, 0, 0, 14, 0, 0, "fbx_mesh->face_group_parts.data" },
	{ "maya_cube_big_endian_obj", 17525, -1, 0, 0, 28, 0, 0, "fbx_mesh->face_group_parts.data" },
	{ "maya_cube_big_endian_obj", 17560, -1, 0, 10, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "maya_cube_big_endian_obj", 17560, -1, 0, 703, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "maya_cube_big_endian_obj", 17567, 83, 46, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_POSITION, 1..." },
	{ "maya_cube_big_endian_obj", 17574, 111, 9, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_COLOR, 4)" },
	{ "maya_cube_big_endian_obj", 17581, 328, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_UV, 1)" },
	{ "maya_cube_big_endian_obj", 17583, 622, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_NORMAL, 1)" },
	{ "maya_cube_big_endian_obj", 17585, -1, 0, 61, 0, 0, 0, "ufbxi_obj_parse_indices(uc, 1, uc->obj.num_tokens - 1)" },
	{ "maya_cube_big_endian_obj", 17585, -1, 0, 995, 0, 0, 0, "ufbxi_obj_parse_indices(uc, 1, uc->obj.num_tokens - 1)" },
	{ "maya_cube_big_endian_obj", 17609, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "maya_cube_big_endian_obj", 17609, -1, 0, 720, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "maya_cube_big_endian_obj", 17633, -1, 0, 0, 2, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 17633, -1, 0, 0, 4, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "maya_cube_big_endian_obj", 17634, 71, 102, 0, 0, 0, 0, "ufbxi_obj_pop_meshes(uc)" },
	{ "maya_cube_big_endian_obj", 17906, -1, 0, 3, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "maya_cube_big_endian_obj", 17906, -1, 0, 689, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "maya_cube_big_endian_obj", 17907, 71, 102, 0, 0, 0, 0, "ufbxi_obj_parse_file(uc)" },
	{ "maya_cube_big_endian_obj", 17908, -1, 0, 0, 15, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_big_endian_obj", 17908, -1, 0, 1093, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_big_endian_obj", 25055, 71, 102, 0, 0, 0, 0, "ufbxi_obj_load(uc)" },
	{ "maya_equal_pivot_7700_ascii", 18210, -1, 0, 0, 196, 0, 0, "new_props" },
	{ "maya_equal_pivot_7700_ascii", 18210, -1, 0, 0, 392, 0, 0, "new_props" },
	{ "maya_equal_pivot_7700_ascii", 18225, -1, 0, 3707, 0, 0, 0, "ufbxi_sort_properties(uc, node->props.props.data, node-..." },
	{ "maya_extrapolate_cube_7700_ascii", 9906, -1, 0, 1113, 0, 0, 0, "v" },
	{ "maya_game_sausage_6100_binary", 13940, -1, 0, 5304, 0, 0, 0, "skin" },
	{ "maya_game_sausage_6100_binary", 13972, -1, 0, 5375, 0, 0, 0, "cluster" },
	{ "maya_game_sausage_6100_binary", 13978, 45318, 7, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "maya_game_sausage_6100_binary", 13989, 45470, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "maya_game_sausage_6100_binary", 13990, 45636, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "maya_game_sausage_6100_binary", 14616, 42301, 15, 0, 0, 0, 0, "matrix->size >= 16" },
	{ "maya_game_sausage_6100_binary", 14969, -1, 0, 5304, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "maya_game_sausage_6100_binary", 14971, 45318, 7, 0, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "maya_game_sausage_6100_binary", 18751, 23, 213, 0, 0, 0, 0, "depth <= uc->opts.node_depth_limit" },
	{ "maya_game_sausage_6100_binary", 18993, -1, 0, 1543, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_game_sausage_6100_binary", 18993, -1, 0, 6023, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_game_sausage_6100_binary", 19001, -1, 0, 0, 225, 0, 0, "list->data" },
	{ "maya_game_sausage_6100_binary", 19001, -1, 0, 0, 450, 0, 0, "list->data" },
	{ "maya_game_sausage_6100_binary", 19132, -1, 0, 6018, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_game_sausage_6100_binary", 21628, -1, 0, 1539, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "maya_game_sausage_6100_binary", 21628, -1, 0, 6013, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "maya_game_sausage_6100_binary", 21673, -1, 0, 0, 218, 0, 0, "skin->vertices.data" },
	{ "maya_game_sausage_6100_binary", 21673, -1, 0, 0, 436, 0, 0, "skin->vertices.data" },
	{ "maya_game_sausage_6100_binary", 21677, -1, 0, 0, 219, 0, 0, "skin->weights.data" },
	{ "maya_game_sausage_6100_binary", 21677, -1, 0, 0, 438, 0, 0, "skin->weights.data" },
	{ "maya_game_sausage_6100_binary", 21734, -1, 0, 6018, 0, 0, 0, "ufbxi_sort_skin_weights(uc, skin)" },
	{ "maya_game_sausage_6100_binary", 21907, -1, 0, 1542, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "maya_game_sausage_6100_binary", 21907, -1, 0, 6021, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "maya_game_sausage_6100_binary", 21910, -1, 0, 1543, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "maya_game_sausage_6100_binary", 21910, -1, 0, 6023, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "maya_game_sausage_7500_binary", 13940, -1, 0, 861, 0, 0, 0, "skin" },
	{ "maya_game_sausage_7500_binary", 13972, -1, 0, 880, 0, 0, 0, "cluster" },
	{ "maya_game_sausage_7500_binary", 14969, -1, 0, 861, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "maya_human_ik_6100_binary", 13930, -1, 0, 11735, 0, 0, 0, "marker" },
	{ "maya_human_ik_6100_binary", 13930, -1, 0, 40954, 0, 0, 0, "marker" },
	{ "maya_human_ik_6100_binary", 14862, -1, 0, 17214, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 14862, -1, 0, 60674, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 14864, -1, 0, 11735, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 14864, -1, 0, 40954, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_7400_binary", 14941, -1, 0, 10484, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 14941, -1, 0, 2662, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 14943, -1, 0, 1882, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 14943, -1, 0, 7692, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 5155, -1, 0, 9899, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "maya_leading_comma_7500_ascii", 10340, -1, 0, 0, 0, 291, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_long_keyframes_6100_binary", 15328, 16558, 0, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_long_keyframes_6100_binary", 15336, 16557, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_material_chart_6100_binary", 15009, -1, 0, 26209, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_material_chart_6100_binary", 15009, -1, 0, 7599, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_material_chart_6100_binary", 22312, -1, 0, 27355, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_material_chart_6100_binary", 22312, -1, 0, 7971, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_node_attribute_zoo_6100_ascii", 10440, -1, 0, 5493, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 10468, -1, 0, 5926, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 15369, 142406, 45, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_node_attribute_zoo_6100_binary", 13773, -1, 0, 15121, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 13773, -1, 0, 4141, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 13778, 138209, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->basis..." },
	{ "maya_node_attribute_zoo_6100_binary", 13780, 138308, 255, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 13787, 138359, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 13788, 138416, 1, 0, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 13789, 143462, 27, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 13803, -1, 0, 15326, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 13803, -1, 0, 4210, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 13808, 139478, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 13809, 139592, 1, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"ZZ\", &dimens..." },
	{ "maya_node_attribute_zoo_6100_binary", 13810, 139631, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Step, \"II\", &step_u, &ste..." },
	{ "maya_node_attribute_zoo_6100_binary", 13811, 139664, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 13824, 139691, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 13825, 139727, 1, 0, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 13826, 140321, 3, 0, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 13827, 141818, 63, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 13828, 139655, 1, 0, 0, 0, 0, "points->size / 4 == (size_t)dimension_u * (size_t)dimen..." },
	{ "maya_node_attribute_zoo_6100_binary", 13915, -1, 0, 3237, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 13915, -1, 0, 710, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 14838, -1, 0, 3237, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 14838, -1, 0, 710, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 14840, -1, 0, 1777, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 14840, -1, 0, 276, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 14858, -1, 0, 1963, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 14858, -1, 0, 7584, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 14866, -1, 0, 10453, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 14866, -1, 0, 2786, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 14931, -1, 0, 14175, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 14931, -1, 0, 3878, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 14955, 138209, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 14957, 139478, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 14961, -1, 0, 15756, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 14961, -1, 0, 4352, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 14963, -1, 0, 15912, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 14963, -1, 0, 4397, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 15204, -1, 0, 17537, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &src_prop..." },
	{ "maya_node_attribute_zoo_6100_binary", 15207, -1, 0, 17512, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst_prop..." },
	{ "maya_node_attribute_zoo_6100_binary", 20070, -1, 0, 0, 489, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 20070, -1, 0, 0, 978, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 20113, -1, 0, 0, 1014, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 20113, -1, 0, 0, 507, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 21936, -1, 0, 0, 489, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 21936, -1, 0, 0, 978, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 21941, -1, 0, 0, 498, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 21941, -1, 0, 0, 996, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 21942, -1, 0, 0, 499, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 21942, -1, 0, 0, 998, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 22366, -1, 0, 0, 1014, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_6100_binary", 22366, -1, 0, 0, 507, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_7500_ascii", 10437, -1, 0, 3373, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 10438, -1, 0, 11812, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 10438, -1, 0, 3360, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 9906, -1, 0, 11811, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_binary", 12246, -1, 0, 0, 324, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_node_attribute_zoo_7500_binary", 14205, -1, 0, 1783, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 14205, -1, 0, 6554, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 14213, 61038, 255, 0, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 14214, 61115, 255, 0, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 14215, 61175, 255, 0, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 14216, 61234, 255, 0, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 14217, 61292, 255, 0, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_binary", 14220, 61122, 0, 0, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_binary", 14225, 61242, 0, 0, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_binary", 14226, 61300, 0, 0, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", 14230, -1, 0, 0, 325, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 14230, -1, 0, 0, 650, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 14257, 61431, 0, 0, 0, 0, 0, "refs_left > 0" },
	{ "maya_node_attribute_zoo_7500_binary", 14929, -1, 0, 2995, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 14929, -1, 0, 657, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 14933, -1, 0, 2746, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 14933, -1, 0, 585, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 14935, -1, 0, 2481, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 14935, -1, 0, 491, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 14937, -1, 0, 3192, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 14937, -1, 0, 708, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 14945, -1, 0, 1162, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 14945, -1, 0, 4547, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 14994, -1, 0, 1796, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 14994, -1, 0, 6593, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 14996, 61038, 255, 0, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", 18637, -1, 0, 2170, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 18637, -1, 0, 7745, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 18684, -1, 0, 2172, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 18684, -1, 0, 7749, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 8835, 61146, 109, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 8836, 61333, 103, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 9189, -1, 0, 0, 0, 0, 2942, "ufbxi_resume_progress(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 9193, -1, 0, 0, 0, 0, 2943, "res != -28" },
	{ "maya_notes_6100_ascii", 9601, -1, 0, 1650, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_notes_6100_ascii", 9601, -1, 0, 236, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_notes_6100_ascii", 9787, -1, 0, 1650, 0, 0, 0, "ufbxi_ascii_push_token_string(uc, token, begin, ufbxi_t..." },
	{ "maya_notes_6100_ascii", 9787, -1, 0, 236, 0, 0, 0, "ufbxi_ascii_push_token_string(uc, token, begin, ((size_..." },
	{ "maya_scale_no_inherit_6100_ascii", 12509, -1, 0, 1121, 0, 0, 0, "scale_node" },
	{ "maya_scale_no_inherit_6100_ascii", 12509, -1, 0, 4580, 0, 0, 0, "scale_node" },
	{ "maya_scale_no_inherit_6100_ascii", 12510, -1, 0, 4589, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_scale_no_inherit_6100_ascii", 12516, -1, 0, 1124, 0, 0, 0, "ufbxi_connect_oo(uc, scale_fbx_id, node_fbx_id)" },
	{ "maya_scale_no_inherit_6100_ascii", 12516, -1, 0, 4590, 0, 0, 0, "ufbxi_connect_oo(uc, scale_fbx_id, node_fbx_id)" },
	{ "maya_scale_no_inherit_6100_ascii", 12520, -1, 0, 1125, 0, 0, 0, "extra" },
	{ "maya_scale_no_inherit_6100_ascii", 12520, -1, 0, 4592, 0, 0, 0, "extra" },
	{ "maya_scale_no_inherit_6100_ascii", 12525, -1, 0, 0, 120, 0, 0, "helper_props" },
	{ "maya_scale_no_inherit_6100_ascii", 12525, -1, 0, 0, 240, 0, 0, "helper_props" },
	{ "maya_scale_no_inherit_6100_ascii", 17989, -1, 0, 1113, 0, 0, 0, "elements" },
	{ "maya_scale_no_inherit_6100_ascii", 17993, -1, 0, 1114, 0, 0, 0, "tmp_connections" },
	{ "maya_scale_no_inherit_6100_ascii", 17996, -1, 0, 1115, 0, 0, 0, "pre_connections" },
	{ "maya_scale_no_inherit_6100_ascii", 17999, -1, 0, 1116, 0, 0, 0, "instance_counts" },
	{ "maya_scale_no_inherit_6100_ascii", 18002, -1, 0, 1117, 0, 0, 0, "modify_not_supported" },
	{ "maya_scale_no_inherit_6100_ascii", 18011, -1, 0, 1118, 0, 0, 0, "pre_nodes" },
	{ "maya_scale_no_inherit_6100_ascii", 18019, -1, 0, 1119, 0, 0, 0, "pre_anim_values" },
	{ "maya_scale_no_inherit_6100_ascii", 18022, -1, 0, 1120, 0, 0, 0, "fbx_ids" },
	{ "maya_scale_no_inherit_6100_ascii", 18280, -1, 0, 1121, 0, 0, 0, "ufbxi_setup_scale_helper(uc, node, fbx_id)" },
	{ "maya_scale_no_inherit_6100_ascii", 18280, -1, 0, 4580, 0, 0, 0, "ufbxi_setup_scale_helper(uc, node, fbx_id)" },
	{ "maya_scale_no_inherit_6100_ascii", 25070, -1, 0, 1113, 0, 0, 0, "ufbxi_pre_finalize_scene(uc)" },
	{ "maya_slime_7500_ascii", 10337, -1, 0, 0, 0, 0, 540984, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_slime_7500_ascii", 9700, -1, 0, 0, 0, 540884, 0, "ufbxi_ascii_skip_until(uc, '\"')" },
	{ "maya_texture_layers_6100_binary", 14520, -1, 0, 1460, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 14520, -1, 0, 5607, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 14529, -1, 0, 1464, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 14529, -1, 0, 5617, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 14986, -1, 0, 1460, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 14986, -1, 0, 5607, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 19037, -1, 0, 1677, 0, 0, 0, "((ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_texture_layers_6100_binary", 19037, -1, 0, 6289, 0, 0, 0, "((ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_texture_layers_6100_binary", 19044, -1, 0, 0, 267, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 19044, -1, 0, 0, 534, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 20700, -1, 0, 1684, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 20700, -1, 0, 6306, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 20731, -1, 0, 1688, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 20731, -1, 0, 6315, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 20739, -1, 0, 1691, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &files, ..." },
	{ "maya_texture_layers_6100_binary", 20739, -1, 0, 6321, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &files, ..." },
	{ "maya_texture_layers_6100_binary", 20743, -1, 0, 0, 272, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 20743, -1, 0, 0, 544, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 20774, -1, 0, 1682, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 20774, -1, 0, 6301, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 22264, -1, 0, 1677, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_texture_layers_6100_binary", 22264, -1, 0, 6289, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_textured_cube_6100_binary", 22155, -1, 0, 1671, 0, 0, 0, "mat_texs" },
	{ "maya_textured_cube_6100_binary", 22155, -1, 0, 6343, 0, 0, 0, "mat_texs" },
	{ "motionbuilder_cube_7700_binary", 14939, -1, 0, 1083, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_..." },
	{ "motionbuilder_cube_7700_binary", 14939, -1, 0, 4627, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_..." },
	{ "motionbuilder_sausage_rrss_7700_binary", 18005, -1, 0, 4290, 0, 0, 0, "has_unscaled_children" },
	{ "motionbuilder_tangent_linear_7700_ascii", 9835, -1, 0, 1761, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, replacement)" },
	{ "motionbuilder_tangent_linear_7700_ascii", 9835, -1, 0, 6951, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, replacement)" },
	{ "motionbuilder_thumbnail_7700_ascii", 10436, -1, 0, 53, 0, 0, 0, "v" },
	{ "motionbuilder_thumbnail_7700_ascii", 10436, -1, 0, 831, 0, 0, 0, "v" },
	{ "motionbuilder_thumbnail_7700_ascii", 11734, -1, 0, 0, 1118, 0, 0, "dst" },
	{ "motionbuilder_thumbnail_7700_ascii", 11734, -1, 0, 0, 559, 0, 0, "dst" },
	{ "motionbuilder_thumbnail_7700_ascii", 11814, -1, 0, 0, 1118, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "motionbuilder_thumbnail_7700_ascii", 11814, -1, 0, 0, 559, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "motionbuilder_thumbnail_7700_binary", 11888, 2757, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &thumbnail->props)" },
	{ "motionbuilder_thumbnail_7700_binary", 11927, 2757, 0, 0, 0, 0, 0, "ufbxi_read_thumbnail(uc, thumbnail, &uc->scene.metadata..." },
	{ "motionbuilder_thumbnail_7700_binary", 12877, -1, 0, 1261, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_GEO..." },
	{ "motionbuilder_thumbnail_7700_binary", 12877, -1, 0, 5180, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_GEO..." },
	{ "motionbuilder_thumbnail_7700_binary", 13562, -1, 0, 1261, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing.da..." },
	{ "motionbuilder_thumbnail_7700_binary", 13562, -1, 0, 5180, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing.da..." },
	{ "mtl_fuzz_0000", 17700, -1, 0, 0, 5, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &prop->v..." },
	{ "mtl_fuzz_0000", 5266, -1, 0, 0, 5, 0, 0, "p_blob->data" },
	{ "obj_fuzz_0030", 17597, -1, 0, 31, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "obj_fuzz_0030", 17597, -1, 0, 774, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "revit_empty_7400_binary", 12769, -1, 0, 0, 257, 0, 0, "new_index_data" },
	{ "revit_empty_7400_binary", 12769, -1, 0, 0, 514, 0, 0, "new_index_data" },
	{ "revit_empty_7400_binary", 15024, -1, 0, 3980, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadat..." },
	{ "revit_empty_7400_binary", 15024, -1, 0, 904, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadat..." },
	{ "revit_empty_7400_binary", 8779, -1, 0, 0, 300, 0, 0, "d->data" },
	{ "revit_empty_7400_binary", 8779, -1, 0, 0, 600, 0, 0, "d->data" },
	{ "revit_wall_square_7700_binary", 12779, 24503, 0, 0, 0, 0, 0, "ufbxi_fix_index(uc, &index, index, num_elems)" },
	{ "revit_wall_square_7700_binary", 12841, 24213, 96, 0, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, new_inde..." },
	{ "revit_wall_square_obj", 17008, 3058, 102, 0, 0, 0, 0, "index < 0xffffffffffffffffui64 / 10 - 10" },
	{ "revit_wall_square_obj", 17347, 14288, 102, 0, 0, 0, 0, "ix < 0xffffffffui32" },
	{ "synthetic_base64_parse_7700_ascii", 14566, -1, 0, 0, 138, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "synthetic_base64_parse_7700_ascii", 14566, -1, 0, 0, 69, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "synthetic_binary_props_7500_ascii", 10189, -1, 0, 60, 0, 0, 0, "table" },
	{ "synthetic_binary_props_7500_ascii", 10189, -1, 0, 858, 0, 0, 0, "table" },
	{ "synthetic_binary_props_7500_ascii", 10205, -1, 0, 60, 0, 0, 0, "ufbxi_setup_base64(uc)" },
	{ "synthetic_binary_props_7500_ascii", 10205, -1, 0, 858, 0, 0, 0, "ufbxi_setup_base64(uc)" },
	{ "synthetic_binary_props_7500_ascii", 10238, -1, 0, 61, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_BAD_BASE64_..." },
	{ "synthetic_binary_props_7500_ascii", 10238, -1, 0, 860, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_BAD_BASE64_..." },
	{ "synthetic_binary_props_7500_ascii", 10376, -1, 0, 105, 0, 0, 0, "v->data" },
	{ "synthetic_binary_props_7500_ascii", 10376, -1, 0, 857, 0, 0, 0, "v->data" },
	{ "synthetic_binary_props_7500_ascii", 10377, -1, 0, 60, 0, 0, 0, "ufbxi_decode_base64(uc, v, tok->str_data, tok->str_len,..." },
	{ "synthetic_binary_props_7500_ascii", 10377, -1, 0, 858, 0, 0, 0, "ufbxi_decode_base64(uc, v, tok->str_data, tok->str_len,..." },
	{ "synthetic_binary_props_7500_ascii", 12586, -1, 0, 3826, 0, 0, 0, "unknown" },
	{ "synthetic_binary_props_7500_ascii", 12586, -1, 0, 946, 0, 0, 0, "unknown" },
	{ "synthetic_binary_props_7500_ascii", 12593, -1, 0, 3837, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "synthetic_binary_props_7500_ascii", 12595, -1, 0, 3838, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "synthetic_binary_props_7500_ascii", 15030, -1, 0, 3826, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_binary_props_7500_ascii", 15030, -1, 0, 946, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_binary_props_7500_ascii", 25118, -1, 0, 0, 228, 0, 0, "ufbxi_pop_warnings(&uc->warnings, &uc->scene.metadata.w..." },
	{ "synthetic_binary_props_7500_ascii", 25118, -1, 0, 0, 456, 0, 0, "ufbxi_pop_warnings(&uc->warnings, &uc->scene.metadata.w..." },
	{ "synthetic_binary_props_7500_ascii", 4841, -1, 0, 0, 21, 0, 0, "desc_copy" },
	{ "synthetic_binary_props_7500_ascii", 4841, -1, 0, 0, 42, 0, 0, "desc_copy" },
	{ "synthetic_binary_props_7500_ascii", 4844, -1, 0, 61, 0, 0, 0, "warning" },
	{ "synthetic_binary_props_7500_ascii", 4844, -1, 0, 860, 0, 0, 0, "warning" },
	{ "synthetic_binary_props_7500_ascii", 4870, -1, 0, 0, 228, 0, 0, "warnings->data" },
	{ "synthetic_binary_props_7500_ascii", 4870, -1, 0, 0, 456, 0, 0, "warnings->data" },
	{ "synthetic_bind_to_root_7700_ascii", 18488, -1, 0, 2024, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_BAD_ELEMENT..." },
	{ "synthetic_bind_to_root_7700_ascii", 18488, -1, 0, 7353, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_BAD_ELEMENT..." },
	{ "synthetic_blend_shape_order_7500_ascii", 12950, -1, 0, 3308, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_blend_shape_order_7500_ascii", 13001, -1, 0, 3306, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 13001, -1, 0, 758, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 13009, -1, 0, 3308, 0, 0, 0, "ufbxi_sort_blend_offsets(uc, offsets, num_offsets)" },
	{ "synthetic_broken_filename_7500_ascii", 14496, -1, 0, 3674, 0, 0, 0, "texture" },
	{ "synthetic_broken_filename_7500_ascii", 14496, -1, 0, 838, 0, 0, 0, "texture" },
	{ "synthetic_broken_filename_7500_ascii", 14549, -1, 0, 3555, 0, 0, 0, "video" },
	{ "synthetic_broken_filename_7500_ascii", 14549, -1, 0, 800, 0, 0, 0, "video" },
	{ "synthetic_broken_filename_7500_ascii", 14984, -1, 0, 3674, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 14984, -1, 0, 838, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 14988, -1, 0, 3555, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 14988, -1, 0, 800, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 16506, -1, 0, 4009, 0, 0, 0, "result" },
	{ "synthetic_broken_filename_7500_ascii", 16506, -1, 0, 944, 0, 0, 0, "result" },
	{ "synthetic_broken_filename_7500_ascii", 16526, -1, 0, 0, 259, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 16526, -1, 0, 4011, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 18945, -1, 0, 4007, 0, 0, 0, "tex" },
	{ "synthetic_broken_filename_7500_ascii", 18945, -1, 0, 943, 0, 0, 0, "tex" },
	{ "synthetic_broken_filename_7500_ascii", 18955, -1, 0, 0, 258, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 18955, -1, 0, 0, 516, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 20560, -1, 0, 4024, 0, 0, 0, "entry" },
	{ "synthetic_broken_filename_7500_ascii", 20560, -1, 0, 948, 0, 0, 0, "entry" },
	{ "synthetic_broken_filename_7500_ascii", 20563, -1, 0, 4026, 0, 0, 0, "file" },
	{ "synthetic_broken_filename_7500_ascii", 20563, -1, 0, 949, 0, 0, 0, "file" },
	{ "synthetic_broken_filename_7500_ascii", 20589, -1, 0, 0, 261, 0, 0, "files" },
	{ "synthetic_broken_filename_7500_ascii", 20589, -1, 0, 0, 522, 0, 0, "files" },
	{ "synthetic_broken_filename_7500_ascii", 20666, -1, 0, 4029, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_broken_filename_7500_ascii", 20666, -1, 0, 950, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_broken_filename_7500_ascii", 20670, -1, 0, 4031, 0, 0, 0, "states" },
	{ "synthetic_broken_filename_7500_ascii", 20670, -1, 0, 951, 0, 0, 0, "states" },
	{ "synthetic_broken_filename_7500_ascii", 20755, -1, 0, 0, 262, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 20755, -1, 0, 0, 524, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 21230, -1, 0, 4009, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 21230, -1, 0, 944, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 21279, -1, 0, 4009, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 21279, -1, 0, 944, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 21280, -1, 0, 4013, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 21280, -1, 0, 945, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 22132, -1, 0, 4007, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 22132, -1, 0, 943, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 22239, -1, 0, 944, 0, 0, 0, "ufbxi_resolve_file_content(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 22259, -1, 0, 4018, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_broken_filename_7500_ascii", 22259, -1, 0, 946, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_broken_filename_7500_ascii", 22260, -1, 0, 4021, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_broken_filename_7500_ascii", 22260, -1, 0, 947, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_broken_filename_7500_ascii", 22278, -1, 0, 4024, 0, 0, 0, "ufbxi_insert_texture_file(uc, texture)" },
	{ "synthetic_broken_filename_7500_ascii", 22278, -1, 0, 948, 0, 0, 0, "ufbxi_insert_texture_file(uc, texture)" },
	{ "synthetic_broken_filename_7500_ascii", 22282, -1, 0, 0, 261, 0, 0, "ufbxi_pop_texture_files(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 22282, -1, 0, 0, 522, 0, 0, "ufbxi_pop_texture_files(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 22369, -1, 0, 4029, 0, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 22369, -1, 0, 950, 0, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_by_vertex_bad_index_7500_ascii", 12762, -1, 0, 2728, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, new_inde..." },
	{ "synthetic_by_vertex_bad_index_7500_ascii", 12762, -1, 0, 579, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, new_inde..." },
	{ "synthetic_by_vertex_overflow_7500_ascii", 12663, -1, 0, 0, 158, 0, 0, "indices" },
	{ "synthetic_by_vertex_overflow_7500_ascii", 12663, -1, 0, 0, 316, 0, 0, "indices" },
	{ "synthetic_by_vertex_overflow_7500_ascii", 12824, -1, 0, 2721, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, mesh->ve..." },
	{ "synthetic_by_vertex_overflow_7500_ascii", 12824, -1, 0, 577, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, mesh->ve..." },
	{ "synthetic_color_suzanne_0_obj", 17576, -1, 0, 16, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_0_obj", 17576, -1, 0, 726, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_1_obj", 17367, -1, 0, 2186, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "synthetic_comment_cube_7500_ascii", 10542, -1, 0, 2331, 0, 0, 0, "ufbxi_ascii_store_array(uc, tmp_buf)" },
	{ "synthetic_comment_cube_7500_ascii", 10542, -1, 0, 492, 0, 0, 0, "ufbxi_ascii_store_array(uc, tmp_buf)" },
	{ "synthetic_comment_cube_7500_ascii", 10604, -1, 0, 2335, 0, 0, 0, "spans" },
	{ "synthetic_comment_cube_7500_ascii", 10604, -1, 0, 494, 0, 0, 0, "spans" },
	{ "synthetic_comment_cube_7500_ascii", 10618, -1, 0, 2337, 0, 0, 0, "task->data" },
	{ "synthetic_comment_cube_7500_ascii", 10618, -1, 0, 495, 0, 0, 0, "task->data" },
	{ "synthetic_comment_cube_7500_ascii", 9647, -1, 0, 0, 0, 8935, 0, "c != '\\0'" },
	{ "synthetic_comment_cube_7500_ascii", 9647, -1, 0, 0, 0, 9574, 0, "c != '\\0'" },
	{ "synthetic_comment_cube_7500_ascii", 9662, -1, 0, 2331, 0, 0, 0, "span" },
	{ "synthetic_comment_cube_7500_ascii", 9662, -1, 0, 492, 0, 0, 0, "span" },
	{ "synthetic_direct_by_polygon_7700_ascii", 12831, -1, 0, 0, 158, 0, 0, "new_index_data" },
	{ "synthetic_direct_by_polygon_7700_ascii", 12831, -1, 0, 0, 316, 0, 0, "new_index_data" },
	{ "synthetic_duplicate_id_7700_ascii", 18881, -1, 0, 1366, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_C..." },
	{ "synthetic_duplicate_id_7700_ascii", 18881, -1, 0, 5199, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_C..." },
	{ "synthetic_empty_elements_7500_ascii", 18747, 2800, 49, 0, 0, 0, 0, "depth <= num_nodes" },
	{ "synthetic_empty_face_0_obj", 17098, -1, 0, 21, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_EMPTY_FACE_..." },
	{ "synthetic_empty_face_0_obj", 17098, -1, 0, 735, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_EMPTY_FACE_..." },
	{ "synthetic_face_groups_0_obj", 17603, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "synthetic_filename_mtl_0_obj", 17868, -1, 0, 1126, 0, 0, 0, "copy" },
	{ "synthetic_filename_mtl_0_obj", 17868, -1, 0, 93, 0, 0, 0, "copy" },
	{ "synthetic_filename_mtl_0_obj", 17874, -1, 0, 1128, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_IMPLICIT_MT..." },
	{ "synthetic_filename_mtl_0_obj", 17874, -1, 0, 94, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_IMPLICIT_MT..." },
	{ "synthetic_geometry_transform_inherit_mode_7700_ascii", 12510, -1, 0, 825, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "synthetic_geometry_transform_inherit_mode_7700_ascii", 18008, -1, 0, 818, 0, 0, 0, "has_scale_animation" },
	{ "synthetic_geometry_transform_inherit_mode_7700_ascii", 18296, -1, 0, 3469, 0, 0, 0, "ufbxi_setup_scale_helper(uc, child, child_fbx_id)" },
	{ "synthetic_geometry_transform_inherit_mode_7700_ascii", 18296, -1, 0, 829, 0, 0, 0, "ufbxi_setup_scale_helper(uc, child, child_fbx_id)" },
	{ "synthetic_indexed_by_vertex_7500_ascii", 12750, -1, 0, 0, 158, 0, 0, "new_index_data" },
	{ "synthetic_indexed_by_vertex_7500_ascii", 12750, -1, 0, 0, 316, 0, 0, "new_index_data" },
	{ "synthetic_inf_nan_7700_ascii", 10525, 9199, 42, 0, 0, 0, 0, "!in_ascii_array" },
	{ "synthetic_integer_holes_7700_binary", 11317, -1, 0, 2731, 0, 0, 0, "dst" },
	{ "synthetic_integer_holes_7700_binary", 11317, -1, 0, 592, 0, 0, 0, "dst" },
	{ "synthetic_integer_holes_7700_binary", 11387, -1, 0, 1, 0, 0, 0, "ufbxi_push_string_imp(&uc->string_pool, str->data, str-..." },
	{ "synthetic_integer_holes_7700_binary", 13611, 20615, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "synthetic_integer_holes_7700_binary", 15077, 15404, 1, 0, 0, 0, 0, "ufbxi_thread_pool_wait_group(&uc->thread_pool)" },
	{ "synthetic_integer_holes_7700_binary", 15085, -1, 0, 2962, 0, 0, 0, "uc->p_element_id" },
	{ "synthetic_integer_holes_7700_binary", 15085, -1, 0, 660, 0, 0, 0, "uc->p_element_id" },
	{ "synthetic_integer_holes_7700_binary", 15089, 15374, 0, 0, 0, 0, 0, "ufbxi_read_object(uc, *p_node)" },
	{ "synthetic_integer_holes_7700_binary", 15130, 15284, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node, tmp_buf)" },
	{ "synthetic_integer_holes_7700_binary", 15135, -1, 0, 2733, 0, 0, 0, "((ufbxi_node**)ufbxi_push_size_copy((&uc->tmp_stack), s..." },
	{ "synthetic_integer_holes_7700_binary", 15135, -1, 0, 593, 0, 0, 0, "((ufbxi_node**)ufbxi_push_size_copy((&uc->tmp_stack), s..." },
	{ "synthetic_integer_holes_7700_binary", 15147, -1, 0, 2960, 0, 0, 0, "batch->nodes" },
	{ "synthetic_integer_holes_7700_binary", 15147, -1, 0, 659, 0, 0, 0, "batch->nodes" },
	{ "synthetic_integer_holes_7700_binary", 15784, 15284, 1, 0, 0, 0, 0, "ufbxi_read_objects_threaded(uc)" },
	{ "synthetic_integer_holes_7700_binary", 25036, -1, 0, 1, 0, 0, 0, "ufbxi_load_strings(uc)" },
	{ "synthetic_integer_holes_7700_binary", 6033, 15404, 1, 0, 0, 0, 0, "Task failed" },
	{ "synthetic_integer_holes_7700_binary", 6040, 15404, 1, 0, 0, 0, 0, "ufbxi_thread_pool_wait_imp(pool, pool->group, 1)" },
	{ "synthetic_integer_holes_7700_binary", 9072, -1, 0, 2459, 0, 0, 0, "t" },
	{ "synthetic_legacy_nonzero_material_5800_ascii", 15556, -1, 0, 7404, 0, 0, 0, "target_fbx_id" },
	{ "synthetic_legacy_nonzero_material_5800_ascii", 16167, -1, 0, 0, 111, 0, 0, "mesh->face_material.data" },
	{ "synthetic_legacy_nonzero_material_5800_ascii", 16167, -1, 0, 0, 222, 0, 0, "mesh->face_material.data" },
	{ "synthetic_legacy_unquoted_child_fail_5800_ascii", 10520, 1, 33, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_missing_cache_fail_7500_ascii", 24410, 1, 33, 0, 0, 0, 0, "open_file_fn()" },
	{ "synthetic_missing_mapping_7500_ascii", 12682, -1, 0, 3196, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_POL..." },
	{ "synthetic_missing_mapping_7500_ascii", 12682, -1, 0, 742, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_MISSING_POL..." },
	{ "synthetic_missing_mapping_7500_ascii", 12799, -1, 0, 3202, 0, 0, 0, "ufbxi_warn_polygon_mapping(uc, data_name, mapping)" },
	{ "synthetic_missing_mapping_7500_ascii", 12799, -1, 0, 745, 0, 0, 0, "ufbxi_warn_polygon_mapping(uc, data_name, mapping)" },
	{ "synthetic_missing_mapping_7500_ascii", 12852, -1, 0, 3196, 0, 0, 0, "ufbxi_warn_polygon_mapping(uc, data_name, mapping)" },
	{ "synthetic_missing_mapping_7500_ascii", 12852, -1, 0, 742, 0, 0, 0, "ufbxi_warn_polygon_mapping(uc, data_name, mapping)" },
	{ "synthetic_missing_mapping_7500_ascii", 13567, -1, 0, 3204, 0, 0, 0, "ufbxi_warn_polygon_mapping(uc, ufbxi_Smoothing, mapping..." },
	{ "synthetic_missing_mapping_7500_ascii", 13567, -1, 0, 746, 0, 0, 0, "ufbxi_warn_polygon_mapping(uc, ufbxi_Smoothing, mapping..." },
	{ "synthetic_missing_mapping_7500_ascii", 13599, -1, 0, 3206, 0, 0, 0, "ufbxi_warn_polygon_mapping(uc, ufbxi_Materials, mapping..." },
	{ "synthetic_missing_mapping_7500_ascii", 13599, -1, 0, 747, 0, 0, 0, "ufbxi_warn_polygon_mapping(uc, ufbxi_Materials, mapping..." },
	{ "synthetic_missing_normals_7400_ascii", 20154, -1, 0, 3488, 0, 0, 0, "topo" },
	{ "synthetic_missing_normals_7400_ascii", 20154, -1, 0, 814, 0, 0, 0, "topo" },
	{ "synthetic_missing_normals_7400_ascii", 20157, -1, 0, 0, 199, 0, 0, "normal_indices" },
	{ "synthetic_missing_normals_7400_ascii", 20157, -1, 0, 0, 398, 0, 0, "normal_indices" },
	{ "synthetic_missing_normals_7400_ascii", 20167, -1, 0, 0, 200, 0, 0, "normal_data" },
	{ "synthetic_missing_normals_7400_ascii", 20167, -1, 0, 0, 400, 0, 0, "normal_data" },
	{ "synthetic_missing_normals_7400_ascii", 21834, -1, 0, 3488, 0, 0, 0, "ufbxi_generate_normals(uc, mesh)" },
	{ "synthetic_missing_normals_7400_ascii", 21834, -1, 0, 814, 0, 0, 0, "ufbxi_generate_normals(uc, mesh)" },
	{ "synthetic_missing_version_6100_ascii", 14593, -1, 0, 13534, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 14593, -1, 0, 3897, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 14619, -1, 0, 13545, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 14619, -1, 0, 3902, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 14629, -1, 0, 13547, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 14629, -1, 0, 3903, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 14860, -1, 0, 1688, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 14860, -1, 0, 252, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 14998, -1, 0, 13534, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 14998, -1, 0, 3897, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 19125, -1, 0, 15727, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_missing_version_6100_ascii", 21578, -1, 0, 0, 251, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 21578, -1, 0, 0, 502, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 21606, -1, 0, 15727, 0, 0, 0, "ufbxi_sort_bone_poses(uc, pose)" },
	{ "synthetic_mixed_attribs_0_obj", 17571, -1, 0, 1039, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_0_obj", 17571, -1, 0, 88, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 17415, -1, 0, 0, 16, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, 0..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 17415, -1, 0, 0, 32, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, 0..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 17418, -1, 0, 0, 19, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 17418, -1, 0, 0, 38, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 17420, -1, 0, 1166, 0, 0, 0, "color_valid" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 17420, -1, 0, 128, 0, 0, 0, "color_valid" },
	{ "synthetic_negative_fbx_id_7700_binary", 12187, -1, 0, 820, 0, 0, 0, "entry" },
	{ "synthetic_negative_fbx_id_7700_binary", 12210, -1, 0, 3367, 0, 0, 0, "fbx_id" },
	{ "synthetic_negative_fbx_id_7700_binary", 12210, -1, 0, 820, 0, 0, 0, "fbx_id" },
	{ "synthetic_negative_fbx_id_7700_binary", 14901, -1, 0, 3367, 0, 0, 0, "ufbxi_validate_fbx_id(uc, &info.fbx_id)" },
	{ "synthetic_negative_fbx_id_7700_binary", 14901, -1, 0, 820, 0, 0, 0, "ufbxi_validate_fbx_id(uc, &info.fbx_id)" },
	{ "synthetic_node_depth_fail_7400_binary", 8941, 23, 233, 0, 0, 0, 0, "depth < 32" },
	{ "synthetic_node_depth_fail_7500_ascii", 10266, 1, 33, 0, 0, 0, 0, "depth < 32" },
	{ "synthetic_obj_zoo_0_obj", 17587, -1, 0, 40, 0, 0, 0, "ufbxi_obj_parse_multi_indices(uc, 1)" },
	{ "synthetic_obj_zoo_0_obj", 17587, -1, 0, 820, 0, 0, 0, "ufbxi_obj_parse_multi_indices(uc, 1)" },
	{ "synthetic_obj_zoo_0_obj", 17629, -1, 0, 45, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_UNKNOWN_OBJ..." },
	{ "synthetic_obj_zoo_0_obj", 17629, -1, 0, 845, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_UNKNOWN_OBJ..." },
	{ "synthetic_parent_directory_7700_ascii", 21173, -1, 0, 4010, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_parent_directory_7700_ascii", 21213, -1, 0, 0, 260, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 21213, -1, 0, 4011, 0, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 21227, -1, 0, 0, 260, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_parent_directory_7700_ascii", 21227, -1, 0, 4010, 0, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_partial_attrib_0_obj", 17505, -1, 0, 0, 10, 0, 0, "indices" },
	{ "synthetic_partial_attrib_0_obj", 17505, -1, 0, 0, 20, 0, 0, "indices" },
	{ "synthetic_partial_material_0_obj", 18371, -1, 0, 83, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_partial_material_0_obj", 21503, -1, 0, 83, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "synthetic_recursive_transform_6100_ascii", 15404, 13116, 45, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "synthetic_simple_materials_0_mtl", 17794, -1, 0, 14, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17916, -1, 0, 3, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17916, -1, 0, 689, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17917, -1, 0, 0, 1, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17917, -1, 0, 703, 0, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17918, -1, 0, 10, 0, 0, 0, "ufbxi_obj_parse_mtl(uc)" },
	{ "synthetic_simple_materials_0_mtl", 17918, -1, 0, 709, 0, 0, 0, "ufbxi_obj_parse_mtl(uc)" },
	{ "synthetic_simple_materials_0_mtl", 25058, -1, 0, 3, 0, 0, 0, "ufbxi_mtl_load(uc)" },
	{ "synthetic_simple_materials_0_mtl", 25058, -1, 0, 689, 0, 0, 0, "ufbxi_mtl_load(uc)" },
	{ "synthetic_simple_textures_0_mtl", 17798, -1, 0, 112, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 0)" },
	{ "synthetic_simple_textures_0_mtl", 17798, -1, 0, 1163, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 0)" },
	{ "synthetic_string_collision_7500_ascii", 5197, -1, 0, 2189, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_tcdefinition_127_6100_ascii", 14636, -1, 0, 3623, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_tcdefinition_127_6100_ascii", 14646, -1, 0, 3531, 0, 0, 0, "bindings" },
	{ "synthetic_tcdefinition_127_6100_ascii", 14646, -1, 0, 740, 0, 0, 0, "bindings" },
	{ "synthetic_tcdefinition_127_6100_ascii", 14660, -1, 0, 3539, 0, 0, 0, "bind" },
	{ "synthetic_tcdefinition_127_6100_ascii", 14660, -1, 0, 742, 0, 0, 0, "bind" },
	{ "synthetic_tcdefinition_127_6100_ascii", 14675, -1, 0, 0, 205, 0, 0, "bindings->prop_bindings.data" },
	{ "synthetic_tcdefinition_127_6100_ascii", 14675, -1, 0, 0, 410, 0, 0, "bindings->prop_bindings.data" },
	{ "synthetic_tcdefinition_127_6100_ascii", 14677, -1, 0, 3623, 0, 0, 0, "ufbxi_sort_shader_prop_bindings(uc, bindings->prop_bind..." },
	{ "synthetic_tcdefinition_127_6100_ascii", 15000, -1, 0, 3005, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "synthetic_tcdefinition_127_6100_ascii", 15000, -1, 0, 627, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "synthetic_tcdefinition_127_6100_ascii", 15002, -1, 0, 3531, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "synthetic_tcdefinition_127_6100_ascii", 15002, -1, 0, 740, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "synthetic_tcdefinition_127_6100_ascii", 22073, -1, 0, 1048, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "synthetic_tcdefinition_127_6100_ascii", 22073, -1, 0, 4400, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "synthetic_tcdefinition_127_7100_ascii", 10161, -1, 0, 4182, 0, 0, 0, "v" },
	{ "synthetic_tcdefinition_127_7100_ascii", 10161, -1, 0, 976, 0, 0, 0, "v" },
	{ "synthetic_tcdefinition_127_7100_ascii", 10439, -1, 0, 4191, 0, 0, 0, "v" },
	{ "synthetic_texture_opts_0_mtl", 17741, -1, 0, 20, 0, 0, 0, "ufbxi_obj_parse_prop(uc, tok, start + 1, 0, &start)" },
	{ "synthetic_texture_opts_0_mtl", 17741, -1, 0, 765, 0, 0, 0, "ufbxi_obj_parse_prop(uc, tok, start + 1, 0, &start)" },
	{ "synthetic_texture_opts_0_mtl", 17751, -1, 0, 0, 22, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tex_str,..." },
	{ "synthetic_texture_split_7500_ascii", 10472, 14622, 35, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12885, -1, 0, 2887, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_TRUNCATED_A..." },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12885, -1, 0, 652, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_TRUNCATED_A..." },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12889, -1, 0, 0, 166, 0, 0, "new_data" },
	{ "synthetic_truncated_crease_partial_7700_ascii", 12889, -1, 0, 0, 332, 0, 0, "new_data" },
	{ "synthetic_truncated_crease_partial_7700_ascii", 13553, -1, 0, 2887, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease.data,..." },
	{ "synthetic_truncated_crease_partial_7700_ascii", 13553, -1, 0, 652, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease.data,..." },
	{ "synthetic_unicode_7500_binary", 5052, -1, 0, 12, 0, 0, 0, "ufbxi_warnf_imp(pool->warnings, UFBX_WARNING_BAD_UNICOD..." },
	{ "synthetic_unicode_7500_binary", 5052, -1, 0, 719, 0, 0, 0, "ufbxi_warnf_imp(pool->warnings, UFBX_WARNING_BAD_UNICOD..." },
	{ "synthetic_unicode_7500_binary", 5059, -1, 0, 13, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 5059, -1, 0, 721, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 5166, -1, 0, 12, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "synthetic_unicode_7500_binary", 5166, -1, 0, 719, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "synthetic_unicode_error_identity_6100_binary", 5068, -1, 0, 3107, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_error_identity_6100_binary", 5068, -1, 0, 714, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_error_identity_6100_binary", 5209, -1, 0, 3105, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "synthetic_unicode_error_identity_6100_binary", 5209, -1, 0, 713, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "synthetic_unsafe_cube_7500_binary", 12594, 15198, 0, 0, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "synthetic_unsafe_cube_7500_binary", 12595, 15161, 255, 0, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "synthetic_unsafe_cube_7500_binary", 12618, -1, 0, 2730, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_INDEX_CLAMP..." },
	{ "synthetic_unsafe_cube_7500_binary", 12618, -1, 0, 592, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_INDEX_CLAMP..." },
	{ "synthetic_unsafe_cube_7500_binary", 12625, 23, 77, 0, 0, 0, 0, "UFBX_INDEX_ERROR_HANDLING_ABORT_LOADING" },
	{ "synthetic_unsafe_cube_7500_binary", 12666, 23, 77, 0, 0, 0, 0, "ufbxi_fix_index(uc, &indices[i], ix, num_elems)" },
	{ "synthetic_unsafe_cube_7500_binary", 12744, 23, 77, 0, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "synthetic_unsafe_cube_7500_binary", 12818, 19389, 77, 0, 0, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "synthetic_unsafe_cube_7500_binary", 14965, 15198, 0, 0, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_unsafe_cube_7500_binary", 5051, 23, 77, 0, 0, 0, 0, "pool->error_handling != UFBX_UNICODE_ERROR_HANDLING_ABO..." },
	{ "synthetic_unsupported_cube_2000_binary", 16297, 942, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc..." },
	{ "synthetic_unsupported_cube_2000_binary", 16352, -1, 0, 0, 105, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "synthetic_unsupported_cube_2000_binary", 16352, -1, 0, 4167, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "synthetic_unsupported_cube_2000_binary", 16354, -1, 0, 1222, 0, 0, 0, "layer" },
	{ "synthetic_unsupported_cube_2000_binary", 16354, -1, 0, 4169, 0, 0, 0, "layer" },
	{ "synthetic_unsupported_cube_2000_binary", 16359, -1, 0, 1227, 0, 0, 0, "stack" },
	{ "synthetic_unsupported_cube_2000_binary", 16359, -1, 0, 4180, 0, 0, 0, "stack" },
	{ "synthetic_unsupported_cube_2000_binary", 16361, -1, 0, 1229, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "synthetic_unsupported_cube_2000_binary", 16361, -1, 0, 4188, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "synthetic_unsupported_cube_2000_binary", 25050, -1, 0, 1230, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_UNSUPPORTED..." },
	{ "synthetic_unsupported_cube_2000_binary", 25050, -1, 0, 4190, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_UNSUPPORTED..." },
	{ "zbrush_d20_6100_binary", 12388, -1, 0, 3565, 0, 0, 0, "conn" },
	{ "zbrush_d20_6100_binary", 12388, -1, 0, 890, 0, 0, 0, "conn" },
	{ "zbrush_d20_6100_binary", 12963, -1, 0, 3567, 0, 0, 0, "shape" },
	{ "zbrush_d20_6100_binary", 12963, -1, 0, 891, 0, 0, 0, "shape" },
	{ "zbrush_d20_6100_binary", 12971, 25242, 2, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "zbrush_d20_6100_binary", 12972, 25217, 0, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "zbrush_d20_6100_binary", 12985, 25290, 2, 0, 0, 0, 0, "normals && normals->size == vertices->size" },
	{ "zbrush_d20_6100_binary", 13031, 25189, 0, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "zbrush_d20_6100_binary", 13035, -1, 0, 3542, 0, 0, 0, "deformer" },
	{ "zbrush_d20_6100_binary", 13035, -1, 0, 881, 0, 0, 0, "deformer" },
	{ "zbrush_d20_6100_binary", 13036, -1, 0, 3553, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "zbrush_d20_6100_binary", 13036, -1, 0, 886, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "zbrush_d20_6100_binary", 13041, -1, 0, 3555, 0, 0, 0, "channel" },
	{ "zbrush_d20_6100_binary", 13041, -1, 0, 887, 0, 0, 0, "channel" },
	{ "zbrush_d20_6100_binary", 13044, -1, 0, 3563, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_6100_binary", 13044, -1, 0, 889, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_6100_binary", 13048, -1, 0, 0, 101, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 13048, -1, 0, 0, 202, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 13060, -1, 0, 3565, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "zbrush_d20_6100_binary", 13060, -1, 0, 890, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "zbrush_d20_6100_binary", 13075, 25217, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "zbrush_d20_6100_binary", 13077, -1, 0, 3578, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "zbrush_d20_6100_binary", 13077, -1, 0, 896, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "zbrush_d20_6100_binary", 13078, -1, 0, 3580, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "zbrush_d20_6100_binary", 13078, -1, 0, 897, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "zbrush_d20_6100_binary", 13220, -1, 0, 0, 134, 0, 0, "ids" },
	{ "zbrush_d20_6100_binary", 13220, -1, 0, 0, 67, 0, 0, "ids" },
	{ "zbrush_d20_6100_binary", 13256, -1, 0, 0, 136, 0, 0, "groups" },
	{ "zbrush_d20_6100_binary", 13256, -1, 0, 0, 68, 0, 0, "groups" },
	{ "zbrush_d20_6100_binary", 13268, -1, 0, 0, 138, 0, 0, "parts" },
	{ "zbrush_d20_6100_binary", 13268, -1, 0, 0, 69, 0, 0, "parts" },
	{ "zbrush_d20_6100_binary", 13386, 25189, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "zbrush_d20_6100_binary", 13604, 8305, 32, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "zbrush_d20_6100_binary", 13723, -1, 0, 0, 134, 0, 0, "ufbxi_assign_face_groups(&uc->result, &uc->error, mesh,..." },
	{ "zbrush_d20_6100_binary", 13723, -1, 0, 0, 67, 0, 0, "ufbxi_assign_face_groups(&uc->result, &uc->error, mesh,..." },
	{ "zbrush_d20_6100_binary", 19014, -1, 0, 1406, 0, 0, 0, "((ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_s..." },
	{ "zbrush_d20_6100_binary", 19014, -1, 0, 5351, 0, 0, 0, "((ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_s..." },
	{ "zbrush_d20_6100_binary", 19021, -1, 0, 0, 259, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 19021, -1, 0, 0, 518, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 19152, -1, 0, 5353, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "zbrush_d20_6100_binary", 21253, -1, 0, 1425, 0, 0, 0, "content" },
	{ "zbrush_d20_6100_binary", 21253, -1, 0, 5399, 0, 0, 0, "content" },
	{ "zbrush_d20_6100_binary", 21281, -1, 0, 1425, 0, 0, 0, "ufbxi_push_file_content(uc, &video->absolute_filename, ..." },
	{ "zbrush_d20_6100_binary", 21281, -1, 0, 5399, 0, 0, 0, "ufbxi_push_file_content(uc, &video->absolute_filename, ..." },
	{ "zbrush_d20_6100_binary", 21297, -1, 0, 1426, 0, 0, 0, "uc->file_content" },
	{ "zbrush_d20_6100_binary", 21297, -1, 0, 5401, 0, 0, 0, "uc->file_content" },
	{ "zbrush_d20_6100_binary", 21740, -1, 0, 1403, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 21740, -1, 0, 5343, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 21770, -1, 0, 1405, 0, 0, 0, "full_weights" },
	{ "zbrush_d20_6100_binary", 21770, -1, 0, 5349, 0, 0, 0, "full_weights" },
	{ "zbrush_d20_6100_binary", 21775, -1, 0, 1406, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 21775, -1, 0, 5351, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 21786, -1, 0, 5353, 0, 0, 0, "ufbxi_sort_blend_keyframes(uc, channel->keyframes.data,..." },
	{ "zbrush_d20_6100_binary", 21908, -1, 0, 1412, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 21908, -1, 0, 5367, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 22147, -1, 0, 1418, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 22147, -1, 0, 5381, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 22177, -1, 0, 1419, 0, 0, 0, "mat_tex" },
	{ "zbrush_d20_6100_binary", 22177, -1, 0, 5383, 0, 0, 0, "mat_tex" },
	{ "zbrush_d20_6100_binary", 22211, -1, 0, 0, 276, 0, 0, "texs" },
	{ "zbrush_d20_6100_binary", 22211, -1, 0, 0, 552, 0, 0, "texs" },
	{ "zbrush_d20_6100_binary", 22230, -1, 0, 1422, 0, 0, 0, "tex" },
	{ "zbrush_d20_6100_binary", 22230, -1, 0, 5390, 0, 0, 0, "tex" },
	{ "zbrush_d20_7500_binary", 14002, -1, 0, 1067, 0, 0, 0, "channel" },
	{ "zbrush_d20_7500_binary", 14002, -1, 0, 4233, 0, 0, 0, "channel" },
	{ "zbrush_d20_7500_binary", 14010, -1, 0, 1072, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_7500_binary", 14010, -1, 0, 4244, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_7500_binary", 14018, -1, 0, 0, 257, 0, 0, "shape_props" },
	{ "zbrush_d20_7500_binary", 14018, -1, 0, 0, 514, 0, 0, "shape_props" },
	{ "zbrush_d20_7500_binary", 14953, 32981, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 14973, -1, 0, 1055, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "zbrush_d20_7500_binary", 14973, -1, 0, 4196, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "zbrush_d20_7500_binary", 14975, -1, 0, 1067, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 14975, -1, 0, 4233, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 15600, -1, 0, 0, 282, 0, 0, "stack->props.props.data" },
	{ "zbrush_d20_7500_binary", 15600, -1, 0, 0, 564, 0, 0, "stack->props.props.data" },
	{ "zbrush_d20_7500_binary", 18843, -1, 0, 1242, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_C..." },
	{ "zbrush_d20_7500_binary", 18843, -1, 0, 4766, 0, 0, 0, "ufbxi_warnf_imp(&uc->warnings, UFBX_WARNING_DUPLICATE_C..." },
	{ "zbrush_d20_selection_set_6100_binary", 14687, -1, 0, 1308, 0, 0, 0, "set" },
	{ "zbrush_d20_selection_set_6100_binary", 14687, -1, 0, 4907, 0, 0, 0, "set" },
	{ "zbrush_d20_selection_set_6100_binary", 14704, -1, 0, 3837, 0, 0, 0, "sel" },
	{ "zbrush_d20_selection_set_6100_binary", 14704, -1, 0, 971, 0, 0, 0, "sel" },
	{ "zbrush_d20_selection_set_6100_binary", 15005, -1, 0, 1308, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 15005, -1, 0, 4907, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 15012, -1, 0, 3837, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 15012, -1, 0, 971, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 22317, -1, 0, 2232, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_d20_selection_set_6100_binary", 22317, -1, 0, 8026, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_polygroup_mess_0_obj", 13356, -1, 0, 0, 2037, 0, 0, "face_indices" },
	{ "zbrush_polygroup_mess_0_obj", 13356, -1, 0, 0, 4074, 0, 0, "face_indices" },
	{ "zbrush_polygroup_mess_0_obj", 17465, -1, 0, 0, 2033, 0, 0, "fbx_mesh->face_group.data" },
	{ "zbrush_polygroup_mess_0_obj", 17465, -1, 0, 0, 4066, 0, 0, "fbx_mesh->face_group.data" },
	{ "zbrush_polygroup_mess_0_obj", 17529, -1, 0, 0, 2037, 0, 0, "ufbxi_update_face_groups(&uc->result, &uc->error, fbx_m..." },
	{ "zbrush_polygroup_mess_0_obj", 17529, -1, 0, 0, 4074, 0, 0, "ufbxi_update_face_groups(&uc->result, &uc->error, fbx_m..." },
	{ "zbrush_vertex_color_obj", 16635, -1, 0, 0, 11, 0, 0, "color_set" },
	{ "zbrush_vertex_color_obj", 16635, -1, 0, 0, 22, 0, 0, "color_set" },
	{ "zbrush_vertex_color_obj", 17223, -1, 0, 27, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_obj", 17223, -1, 0, 842, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_obj", 17366, -1, 0, 1037, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_obj", 17366, -1, 0, 72, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_obj", 17367, -1, 0, 1039, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "zbrush_vertex_color_obj", 17381, -1, 0, 1037, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_obj", 17381, -1, 0, 72, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_obj", 17440, -1, 0, 0, 0, 880, 0, "min_ix < 0xffffffffffffffffui64" },
	{ "zbrush_vertex_color_obj", 17441, -1, 0, 0, 4, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "zbrush_vertex_color_obj", 17441, -1, 0, 0, 8, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "zbrush_vertex_color_obj", 17443, -1, 0, 1042, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_obj", 17443, -1, 0, 74, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_obj", 17616, -1, 0, 27, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_obj", 17616, -1, 0, 842, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_obj", 17733, -1, 0, 1070, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_obj", 17733, -1, 0, 77, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_obj", 17751, -1, 0, 1077, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tex_str,..." },
	{ "zbrush_vertex_color_obj", 17752, -1, 0, 1078, 0, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &tex_raw..." },
	{ "zbrush_vertex_color_obj", 17756, -1, 0, 1079, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_obj", 17756, -1, 0, 78, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_obj", 17765, -1, 0, 0, 18, 0, 0, "ufbxi_obj_pop_props(uc, &texture->props.props, num_prop..." },
	{ "zbrush_vertex_color_obj", 17765, -1, 0, 0, 36, 0, 0, "ufbxi_obj_pop_props(uc, &texture->props.props, num_prop..." },
	{ "zbrush_vertex_color_obj", 17771, -1, 0, 0, 19, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop, 0)" },
	{ "zbrush_vertex_color_obj", 17771, -1, 0, 1090, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop, 0)" },
	{ "zbrush_vertex_color_obj", 17774, -1, 0, 1092, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_obj", 17774, -1, 0, 83, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_obj", 17796, -1, 0, 1070, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
	{ "zbrush_vertex_color_obj", 17796, -1, 0, 77, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
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

const uint32_t ufbxt_file_versions[] = { 0, 1, 2, 3, 2000, 3000, 5000, 5800, 6100, 7100, 7200, 7300, 7400, 7500, 7700, 8000 };

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
		if (ufbx_open_file(&stream, buffer, SIZE_MAX, NULL, NULL)) {
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

	// Allow warnings
	UFBXT_FILE_TEST_FLAG_ALLOW_WARNINGS = 0x8000,

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
	// bool fuzz_always = (flags & UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS) != 0;
	bool fuzz_always = true; // Always fuzz always
	bool diff_always = (flags & UFBXT_FILE_TEST_FLAG_DIFF_ALWAYS) != 0;
	bool allow_thread_error = (flags & UFBXT_FILE_TEST_FLAG_ALLOW_THREAD_ERROR) != 0;

	const ufbx_load_opts *fuzz_opts = NULL;
	if ((flags & UFBXT_FILE_TEST_FLAG_FUZZ_OPTS) != 0 || alternative) {
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
				if ((flags & UFBXT_FILE_TEST_FLAG_ALLOW_WARNINGS) == 0) {
					for (size_t i = 0; i < scene->metadata.warnings.count; i++) {
						ufbx_warning warning = scene->metadata.warnings.data[i];

						if ((flags & UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE) != 0) {
							if (warning.type == UFBX_WARNING_BAD_UNICODE) {
								continue;
							}
						}

						ufbxt_hintf("Unexpected warning: %s", warning.description.data);
						ufbxt_assert(false);
					}
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
#define UFBXT_FILE_TEST_ALT_SUFFIX_FLAGS(name, file, suffix, flags) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#file, &ufbxt_test_fn_imp_file_##name, #suffix, user_opts, flags | UFBXT_FILE_TEST_FLAG_ALTERNATIVE); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
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
#define UFBXT_FILE_TEST_ALT_SUFFIX(name, file, suffix) UFBXT_FILE_TEST_ALT_SUFFIX_FLAGS(name, file, suffix, 0)
#define UFBXT_FILE_TEST_ALT(name, file) UFBXT_FILE_TEST_ALT_FLAGS(name, file, 0)
#define UFBXT_FILE_TEST_OPTS_ALT(name, file, get_opts) UFBXT_FILE_TEST_OPTS_ALT_FLAGS(name, file, get_opts, 0)

#include "all_tests.h"

#undef UFBXT_IMPL
#undef UFBXT_TEST
#undef UFBXT_FILE_TEST_FLAGS
#undef UFBXT_FILE_TEST_PATH_FLAGS
#undef UFBXT_FILE_TEST_OPTS_FLAGS
#undef UFBXT_FILE_TEST_SUFFIX_FLAGS
#undef UFBXT_FILE_TEST_ALT_SUFFIX_FLAGS
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
#define UFBXT_FILE_TEST_ALT_SUFFIX_FLAGS(name, file, suffix, flags) { UFBXT_TEST_GROUP, #name, &ufbxt_test_fn_file_##name },
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

