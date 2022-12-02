#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr);

#include "../ufbx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

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
		if (!(cond)) ufbxt_assert_fail(__FILE__, __LINE__, #cond); \
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

ufbxt_threadlocal ufbxt_jmp_buf *t_jmp_buf;

void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr)
{
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
	if (g_verbose || print_always) {
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
// From commit 2f9c4d3
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "blender_279_ball_0_obj", 13619, -1, 0, 0, 31, 0, 0, "props.data" },
	{ "blender_279_ball_0_obj", 13636, -1, 0, 243, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "blender_279_ball_0_obj", 13782, -1, 0, 1012, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "blender_279_ball_0_obj", 13782, -1, 0, 234, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "blender_279_ball_0_obj", 14037, -1, 0, 132, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "blender_279_ball_0_obj", 14037, -1, 0, 586, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "blender_279_ball_0_obj", 14194, -1, 0, 0, 0, 3099, 0, "uc->obj.num_tokens >= 2" },
	{ "blender_279_ball_0_obj", 14197, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "blender_279_ball_0_obj", 14212, -1, 0, 122, 0, 0, 0, "material" },
	{ "blender_279_ball_0_obj", 14212, -1, 0, 569, 0, 0, 0, "material" },
	{ "blender_279_ball_0_obj", 14219, -1, 0, 124, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "blender_279_ball_0_obj", 14219, -1, 0, 571, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "blender_279_ball_0_obj", 14545, -1, 0, 0, 0, 54, 0, "uc->obj.num_tokens >= 2" },
	{ "blender_279_ball_0_obj", 14548, -1, 0, 12, 0, 0, 0, "lib.data" },
	{ "blender_279_ball_0_obj", 14548, -1, 0, 19, 0, 0, 0, "lib.data" },
	{ "blender_279_ball_0_obj", 14552, -1, 0, 122, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 14552, -1, 0, 569, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "blender_279_ball_0_obj", 14571, -1, 0, 0, 31, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "blender_279_ball_0_obj", 14571, -1, 0, 243, 0, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "blender_279_ball_0_obj", 14586, -1, 0, 1026, 0, 0, 0, "prop" },
	{ "blender_279_ball_0_obj", 14586, -1, 0, 235, 0, 0, 0, "prop" },
	{ "blender_279_ball_0_obj", 14589, -1, 0, 0, 15, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "blender_279_ball_0_obj", 14622, -1, 0, 0, 16, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->va..." },
	{ "blender_279_ball_0_obj", 14709, -1, 0, 1012, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "blender_279_ball_0_obj", 14709, -1, 0, 234, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "blender_279_ball_0_obj", 14716, -1, 0, 0, 31, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 14716, -1, 0, 243, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 14725, -1, 0, 1026, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "blender_279_ball_0_obj", 14725, -1, 0, 235, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "blender_279_ball_0_obj", 14729, -1, 0, 0, 33, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "blender_279_ball_0_obj", 14768, -1, 0, 1011, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "blender_279_ball_0_obj", 14768, -1, 0, 233, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "blender_279_ball_0_obj", 14794, -1, 0, 1012, 0, 0, 0, "ok" },
	{ "blender_279_ball_0_obj", 14794, -1, 0, 234, 0, 0, 0, "ok" },
	{ "blender_279_ball_0_obj", 14805, -1, 0, 1011, 0, 0, 0, "ufbxi_obj_load_mtl(uc)" },
	{ "blender_279_ball_0_obj", 14805, -1, 0, 233, 0, 0, 0, "ufbxi_obj_load_mtl(uc)" },
	{ "blender_279_ball_0_obj", 5376, -1, 0, 1012, 0, 0, 0, "new_buffer" },
	{ "blender_279_ball_0_obj", 5376, -1, 0, 234, 0, 0, 0, "new_buffer" },
	{ "blender_279_ball_7400_binary", 11102, 12516, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_smoothing.da..." },
	{ "blender_279_default_obj", 14235, 481, 48, 0, 0, 0, 0, "min_index < uc->obj.tmp_vertices[attrib].num_items / st..." },
	{ "blender_279_sausage_7400_binary", 11471, -1, 0, 706, 0, 0, 0, "skin" },
	{ "blender_279_sausage_7400_binary", 11471, -1, 0, 732, 0, 0, 0, "skin" },
	{ "blender_279_sausage_7400_binary", 11503, -1, 0, 728, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_7400_binary", 11503, -1, 0, 755, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_7400_binary", 11509, 23076, 0, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "blender_279_sausage_7400_binary", 11520, 23900, 0, 0, 0, 0, 0, "transform->size >= 16" },
	{ "blender_279_sausage_7400_binary", 11521, 24063, 0, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "blender_279_sausage_7400_binary", 11879, 21748, 0, 0, 0, 0, 0, "matrix->size >= 16" },
	{ "blender_279_sausage_7400_binary", 12221, -1, 0, 706, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 12221, -1, 0, 732, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 12223, 23076, 0, 0, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 15568, -1, 0, 4530, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "blender_279_sausage_7400_binary", 17416, -1, 0, 4331, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_7400_binary", 17416, -1, 0, 4528, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_7400_binary", 17459, -1, 0, 0, 382, 0, 0, "skin->vertices.data" },
	{ "blender_279_sausage_7400_binary", 17463, -1, 0, 0, 383, 0, 0, "skin->weights.data" },
	{ "blender_279_sausage_7400_binary", 17518, -1, 0, 4530, 0, 0, 0, "ufbxi_sort_skin_weights(uc, skin)" },
	{ "blender_279_sausage_7400_binary", 17691, -1, 0, 4333, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "blender_279_sausage_7400_binary", 17691, -1, 0, 4531, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "blender_279_unicode_6100_ascii", 12724, 432, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "blender_279_uv_sets_6100_ascii", 11166, -1, 0, 0, 63, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 11172, -1, 0, 726, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 11172, -1, 0, 732, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 11263, -1, 0, 727, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 11263, -1, 0, 735, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 11266, -1, 0, 729, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 11266, -1, 0, 738, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 17976, -1, 0, 3827, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 17976, -1, 0, 3859, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 17983, -1, 0, 3828, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 17983, -1, 0, 3861, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 6380, -1, 0, 727, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 6380, -1, 0, 735, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 6384, -1, 0, 728, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 6384, -1, 0, 736, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "fuzz_0018", 13282, 810, 0, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "fuzz_0070", 4094, -1, 0, 32, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0272", 10244, -1, 0, 0, 126, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "fuzz_0272", 10244, -1, 0, 451, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "fuzz_0393", 10420, -1, 0, 0, 137, 0, 0, "index_data" },
	{ "fuzz_0491", 14880, -1, 0, 26, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 14900, -1, 0, 23, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 15238, -1, 0, 23, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "fuzz_0491", 17315, -1, 0, 26, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "fuzz_0561", 12217, -1, 0, 450, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0561", 12217, -1, 0, 462, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "marvelous_quad_7200_binary", 19551, -1, 0, 0, 272, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "max2009_blob_5800_ascii", 12456, 164150, 114, 0, 0, 0, 0, "Unknown slope mode" },
	{ "max2009_blob_5800_ascii", 12486, 164903, 98, 0, 0, 0, 0, "Unknown weight mode" },
	{ "max2009_blob_5800_ascii", 12499, 164150, 116, 0, 0, 0, 0, "Unknown key mode" },
	{ "max2009_blob_5800_ascii", 8421, -1, 0, 4411, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 8421, -1, 0, 4412, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 8429, -1, 0, 0, 116, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 8491, 131240, 45, 0, 0, 0, 0, "Bad array dst type" },
	{ "max2009_blob_5800_ascii", 8547, -1, 0, 6948, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 8547, -1, 0, 6956, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 9306, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "max2009_blob_5800_binary", 10174, -1, 0, 7, 0, 0, 0, "ufbxi_insert_fbx_id(uc, fbx_id, elem->element_id)" },
	{ "max2009_blob_5800_binary", 10212, -1, 0, 3309, 0, 0, 0, "conn" },
	{ "max2009_blob_5800_binary", 10212, -1, 0, 3310, 0, 0, 0, "conn" },
	{ "max2009_blob_5800_binary", 12369, -1, 0, 3358, 0, 0, 0, "curve" },
	{ "max2009_blob_5800_binary", 12369, -1, 0, 3359, 0, 0, 0, "curve" },
	{ "max2009_blob_5800_binary", 12371, -1, 0, 3360, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max2009_blob_5800_binary", 12371, -1, 0, 3361, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max2009_blob_5800_binary", 12376, 119084, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "max2009_blob_5800_binary", 12379, 119104, 255, 0, 0, 0, 0, "curve->keyframes.data" },
	{ "max2009_blob_5800_binary", 12393, 119110, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max2009_blob_5800_binary", 12479, 119110, 16, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "max2009_blob_5800_binary", 12504, 119102, 3, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max2009_blob_5800_binary", 12553, 119102, 1, 0, 0, 0, 0, "data == data_end" },
	{ "max2009_blob_5800_binary", 12570, 114022, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "max2009_blob_5800_binary", 12581, 119084, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "max2009_blob_5800_binary", 12624, -1, 0, 3303, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 12624, -1, 0, 3305, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 12625, -1, 0, 3309, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max2009_blob_5800_binary", 12625, -1, 0, 3310, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max2009_blob_5800_binary", 12628, 119084, 0, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "max2009_blob_5800_binary", 12641, 114858, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "max2009_blob_5800_binary", 12650, 114022, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "max2009_blob_5800_binary", 12683, 114022, 0, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "max2009_blob_5800_binary", 12937, -1, 0, 567, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 12937, -1, 0, 570, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 12945, -1, 0, 0, 140, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 12986, -1, 0, 104, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 12986, -1, 0, 106, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 12993, -1, 0, 0, 44, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 13001, -1, 0, 304, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 13001, -1, 0, 307, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 13008, -1, 0, 0, 94, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 13041, -1, 0, 564, 0, 0, 0, "mesh" },
	{ "max2009_blob_5800_binary", 13041, -1, 0, 567, 0, 0, 0, "mesh" },
	{ "max2009_blob_5800_binary", 13052, 9030, 37, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "max2009_blob_5800_binary", 13062, -1, 0, 0, 664, 0, 0, "index_data" },
	{ "max2009_blob_5800_binary", 13085, 58502, 255, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "max2009_blob_5800_binary", 13116, -1, 0, 0, 138, 0, 0, "set" },
	{ "max2009_blob_5800_binary", 13120, 65596, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, uv_info, (ufbx_vert..." },
	{ "max2009_blob_5800_binary", 13128, 56645, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MaterialAssignation, \"C\",..." },
	{ "max2009_blob_5800_binary", 13130, 56700, 78, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material.dat..." },
	{ "max2009_blob_5800_binary", 13159, 6207, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 13160, -1, 0, 0, 139, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 13161, -1, 0, 567, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 13161, -1, 0, 570, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 13162, -1, 0, 569, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 13162, -1, 0, 572, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 13193, 818, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 13194, -1, 0, 0, 43, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 13202, -1, 0, 101, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 13202, -1, 0, 103, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 13203, -1, 0, 358, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 13203, -1, 0, 361, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 13206, -1, 0, 102, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13206, -1, 0, 104, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13211, -1, 0, 103, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13211, -1, 0, 105, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13218, -1, 0, 104, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13218, -1, 0, 106, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13220, -1, 0, 304, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13220, -1, 0, 307, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13224, 6207, 0, 0, 0, 0, 0, "ufbxi_read_legacy_mesh(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 13231, -1, 0, 109, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max2009_blob_5800_binary", 13240, -1, 0, 588, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13240, -1, 0, 591, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 13264, -1, 0, 3, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max2009_blob_5800_binary", 13271, -1, 0, 1, 0, 0, 0, "root" },
	{ "max2009_blob_5800_binary", 13271, -1, 0, 4, 0, 0, 0, "root" },
	{ "max2009_blob_5800_binary", 13273, -1, 0, 6, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 13273, -1, 0, 8, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 13284, 113392, 1, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "max2009_blob_5800_binary", 13288, 818, 0, 0, 0, 0, 0, "ufbxi_read_legacy_model(uc, node)" },
	{ "max2009_blob_5800_binary", 13293, -1, 0, 0, 3550, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "max2009_blob_5800_binary", 15148, -1, 0, 3635, 0, 0, 0, "new_prop" },
	{ "max2009_blob_5800_binary", 15148, -1, 0, 3644, 0, 0, 0, "new_prop" },
	{ "max2009_blob_5800_binary", 15165, -1, 0, 0, 355, 0, 0, "elem->props.props.data" },
	{ "max2009_blob_5800_binary", 17235, -1, 0, 0, 411, 0, 0, "mat->face_indices.data" },
	{ "max2009_blob_5800_binary", 17278, -1, 0, 3635, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "max2009_blob_5800_binary", 17278, -1, 0, 3644, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "max2009_blob_5800_binary", 17639, -1, 0, 0, 410, 0, 0, "materials" },
	{ "max2009_blob_5800_binary", 17684, -1, 0, 0, 411, 0, 0, "ufbxi_finalize_mesh_material(&uc->result, &uc->error, m..." },
	{ "max2009_blob_5800_binary", 17748, -1, 0, 3750, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "max2009_blob_5800_binary", 17748, -1, 0, 3761, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "max2009_blob_5800_binary", 17772, -1, 0, 3782, 0, 0, 0, "aprop" },
	{ "max2009_blob_5800_binary", 17772, -1, 0, 3791, 0, 0, 0, "aprop" },
	{ "max2009_blob_5800_binary", 7258, -1, 0, 0, 0, 80100, 0, "val" },
	{ "max2009_blob_5800_binary", 7260, 80062, 17, 0, 0, 0, 0, "type == 'S' || type == 'R'" },
	{ "max2009_blob_5800_binary", 7269, 80082, 1, 0, 0, 0, 0, "d->data" },
	{ "max2009_blob_5800_binary", 7275, -1, 0, 0, 119, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max2009_blob_5800_binary", 9324, -1, 0, 39, 0, 0, 0, "ufbxi_retain_toplevel(uc, &uc->legacy_node)" },
	{ "max2009_blob_5800_binary", 9324, -1, 0, 41, 0, 0, 0, "ufbxi_retain_toplevel(uc, &uc->legacy_node)" },
	{ "max2009_blob_6100_binary", 10187, -1, 0, 1352, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_6100_binary", 10187, -1, 0, 1364, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_6100_binary", 11116, 149477, 3, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material.dat..." },
	{ "max2009_blob_6100_binary", 12082, -1, 0, 371, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 12082, -1, 0, 379, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 12084, -1, 0, 1057, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_6100_binary", 12084, -1, 0, 1068, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "max2009_blob_obj", 13669, -1, 0, 1327, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_obj", 13669, -1, 0, 6707, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_obj", 14027, -1, 0, 0, 2, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "max7_blend_cube_5000_binary", 10639, -1, 0, 310, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 10639, -1, 0, 312, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 13043, 2350, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "max7_cube_5000_binary", 13251, -1, 0, 134, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &uc->legacy_implicit_anim_l..." },
	{ "max7_cube_5000_binary", 13251, -1, 0, 136, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &uc->legacy_implicit_anim_l..." },
	{ "max7_cube_5000_binary", 13253, 942, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc..." },
	{ "max7_cube_5000_binary", 13302, -1, 0, 0, 106, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "max7_cube_5000_binary", 13304, -1, 0, 1211, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 13304, -1, 0, 1212, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 13307, -1, 0, 1214, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &stack_info.fbx_id)" },
	{ "max7_cube_5000_binary", 13307, -1, 0, 1215, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &stack_info.fbx_id)" },
	{ "max7_cube_5000_binary", 13309, -1, 0, 1215, 0, 0, 0, "stack" },
	{ "max7_cube_5000_binary", 13309, -1, 0, 1216, 0, 0, 0, "stack" },
	{ "max7_cube_5000_binary", 13311, -1, 0, 1217, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "max7_cube_5000_binary", 13311, -1, 0, 1218, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "max7_skin_5000_binary", 12955, -1, 0, 337, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 12955, -1, 0, 338, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 12962, 2420, 136, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "max7_skin_5000_binary", 12973, 4378, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "max7_skin_5000_binary", 12974, 4544, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "max7_skin_5000_binary", 13016, -1, 0, 486, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 13016, -1, 0, 488, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 13028, -1, 0, 0, 51, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 13166, 2361, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max7_skin_5000_binary", 13168, 2420, 136, 0, 0, 0, 0, "ufbxi_read_legacy_link(uc, child, &fbx_id, name.data)" },
	{ "max7_skin_5000_binary", 13171, -1, 0, 340, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 13171, -1, 0, 341, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 13174, -1, 0, 341, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 13174, -1, 0, 342, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 13175, -1, 0, 343, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 13175, -1, 0, 344, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 13177, -1, 0, 344, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 13177, -1, 0, 345, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 13222, -1, 0, 486, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max7_skin_5000_binary", 13222, -1, 0, 488, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max_cache_box_7500_binary", 19435, -1, 0, 658, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 19435, -1, 0, 690, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 19605, -1, 0, 658, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_cache_box_7500_binary", 19605, -1, 0, 690, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_cache_box_7500_binary", 19732, -1, 0, 659, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "max_cache_box_7500_binary", 19732, -1, 0, 691, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "max_cache_box_7500_binary", 19761, -1, 0, 660, 0, 0, 0, "chan" },
	{ "max_cache_box_7500_binary", 19761, -1, 0, 693, 0, 0, 0, "chan" },
	{ "max_cache_box_7500_binary", 19791, -1, 0, 0, 220, 0, 0, "cc->cache.channels.data" },
	{ "max_cache_box_7500_binary", 19834, -1, 0, 0, 219, 0, 0, "cc->cache.frames.data" },
	{ "max_cache_box_7500_binary", 19836, -1, 0, 659, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "max_cache_box_7500_binary", 19836, -1, 0, 691, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "max_cache_box_7500_binary", 19837, -1, 0, 660, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "max_cache_box_7500_binary", 19837, -1, 0, 693, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "max_cache_box_7500_binary", 19841, -1, 0, 0, 221, 0, 0, "cc->imp" },
	{ "max_curve_line_7500_ascii", 11382, 8302, 43, 0, 0, 0, 0, "points->size % 3 == 0" },
	{ "max_curve_line_7500_binary", 11375, -1, 0, 425, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 11375, -1, 0, 438, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 11380, 13861, 255, 0, 0, 0, 0, "points" },
	{ "max_curve_line_7500_binary", 11381, 13985, 56, 0, 0, 0, 0, "points_index" },
	{ "max_curve_line_7500_binary", 11403, -1, 0, 0, 140, 0, 0, "line->segments.data" },
	{ "max_curve_line_7500_binary", 12211, 13861, 255, 0, 0, 0, 0, "ufbxi_read_line(uc, node, &info)" },
	{ "max_quote_6100_ascii", 17340, -1, 0, 1387, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 17340, -1, 0, 1415, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_quote_6100_ascii", 17356, -1, 0, 0, 175, 0, 0, "node->all_attribs.data" },
	{ "max_quote_6100_binary", 11106, 8983, 36, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "max_quote_6100_binary", 11109, 9030, 36, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_visibility.d..." },
	{ "max_shadergraph_7700_ascii", 8133, -1, 0, 1093, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, replacement)" },
	{ "max_shadergraph_7700_ascii", 8133, -1, 0, 1129, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, replacement)" },
	{ "max_texture_mapping_6100_binary", 16565, -1, 0, 2703, 0, 0, 0, "copy" },
	{ "max_texture_mapping_6100_binary", 16565, -1, 0, 2803, 0, 0, 0, "copy" },
	{ "max_texture_mapping_6100_binary", 16573, -1, 0, 0, 660, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_texture_mapping_6100_binary", 16625, -1, 0, 2703, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 16625, -1, 0, 2803, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 16712, -1, 0, 0, 659, 0, 0, "shader" },
	{ "max_texture_mapping_6100_binary", 16744, -1, 0, 2703, 0, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_texture_mapping_6100_binary", 16744, -1, 0, 2803, 0, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_texture_mapping_6100_binary", 16756, -1, 0, 0, 677, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "max_texture_mapping_6100_binary", 16799, -1, 0, 2804, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "max_texture_mapping_6100_binary", 16816, -1, 0, 0, 661, 0, 0, "shader->inputs.data" },
	{ "max_texture_mapping_6100_binary", 16996, -1, 0, 2726, 0, 0, 0, "dst" },
	{ "max_texture_mapping_6100_binary", 16996, -1, 0, 2931, 0, 0, 0, "dst" },
	{ "max_texture_mapping_6100_binary", 17075, -1, 0, 2734, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_texture_mapping_6100_binary", 17075, -1, 0, 2948, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "max_texture_mapping_6100_binary", 18075, -1, 0, 2703, 0, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_texture_mapping_6100_binary", 18075, -1, 0, 2803, 0, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_texture_mapping_7700_binary", 16602, -1, 0, 2328, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_texture_mapping_7700_binary", 16602, -1, 0, 2429, 0, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_transformed_skin_6100_binary", 12427, 63310, 98, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_arnold_textures_6100_binary", 11899, -1, 0, 1614, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_arnold_textures_6100_binary", 11909, -1, 0, 1505, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 11909, -1, 0, 1544, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 11923, -1, 0, 1507, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 11923, -1, 0, 1546, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 11938, -1, 0, 0, 343, 0, 0, "bindings->prop_bindings.data" },
	{ "maya_arnold_textures_6100_binary", 11940, -1, 0, 1614, 0, 0, 0, "ufbxi_sort_shader_prop_bindings(uc, bindings->prop_bind..." },
	{ "maya_arnold_textures_6100_binary", 12252, -1, 0, 1341, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 12252, -1, 0, 1375, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 12254, -1, 0, 1505, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 12254, -1, 0, 1544, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 17863, -1, 0, 1744, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_arnold_textures_6100_binary", 17863, -1, 0, 1795, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_auto_clamp_7100_ascii", 8276, -1, 0, 721, 0, 0, 0, "v" },
	{ "maya_auto_clamp_7100_ascii", 8276, -1, 0, 741, 0, 0, 0, "v" },
	{ "maya_cache_sine_6100_binary", 12229, -1, 0, 1211, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_cache_sine_6100_binary", 12229, -1, 0, 1230, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_cache_sine_6100_binary", 12274, -1, 0, 1277, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_cache_sine_6100_binary", 12274, -1, 0, 1300, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_cache_sine_6100_binary", 17547, -1, 0, 1462, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_cache_sine_6100_binary", 17547, -1, 0, 1495, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_cache_sine_6100_binary", 17548, -1, 0, 1463, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->raw..." },
	{ "maya_cache_sine_6100_binary", 17548, -1, 0, 1496, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->raw..." },
	{ "maya_cache_sine_6100_binary", 17693, -1, 0, 1467, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 17693, -1, 0, 1500, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 19502, -1, 0, 0, 247, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_cache_sine_6100_binary", 19507, -1, 0, 0, 250, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_cache_sine_6100_binary", 19552, -1, 0, 0, 251, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_cache_sine_6100_binary", 19581, -1, 0, 1478, 0, 0, 0, "doc" },
	{ "maya_cache_sine_6100_binary", 19581, -1, 0, 1513, 0, 0, 0, "doc" },
	{ "maya_cache_sine_6100_binary", 19585, -1, 0, 0, 247, 0, 0, "xml_ok" },
	{ "maya_cache_sine_6100_binary", 19593, -1, 0, 0, 253, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_cache_sine_6100_binary", 19609, -1, 0, 1478, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 19609, -1, 0, 1513, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 19668, -1, 0, 0, 253, 0, 0, "ufbxi_cache_try_open_file(cc, filename, ((void *)0), &f..." },
	{ "maya_cache_sine_6100_binary", 19814, -1, 0, 1477, 0, 0, 0, "filename_data" },
	{ "maya_cache_sine_6100_binary", 19814, -1, 0, 1512, 0, 0, 0, "filename_data" },
	{ "maya_cache_sine_6100_binary", 19821, -1, 0, 1478, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, ((void *)0..." },
	{ "maya_cache_sine_6100_binary", 19821, -1, 0, 1513, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, ((void *)0..." },
	{ "maya_cache_sine_6100_binary", 19829, -1, 0, 0, 253, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_cache_sine_6100_binary", 20052, -1, 0, 1474, 0, 0, 0, "file" },
	{ "maya_cache_sine_6100_binary", 20052, -1, 0, 1509, 0, 0, 0, "file" },
	{ "maya_cache_sine_6100_binary", 20062, -1, 0, 1476, 0, 0, 0, "files" },
	{ "maya_cache_sine_6100_binary", 20062, -1, 0, 1511, 0, 0, 0, "files" },
	{ "maya_cache_sine_6100_binary", 20070, -1, 0, 1477, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_cache_sine_6100_binary", 20070, -1, 0, 1512, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_cache_sine_6100_binary", 20497, -1, 0, 1474, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_cache_sine_6100_binary", 20497, -1, 0, 1509, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_cache_sine_6100_binary", 5837, -1, 0, 1479, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_cache_sine_6100_binary", 5837, -1, 0, 1514, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_cache_sine_6100_binary", 5872, -1, 0, 1479, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_cache_sine_6100_binary", 5872, -1, 0, 1514, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_cache_sine_6100_binary", 6026, -1, 0, 1479, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_cache_sine_6100_binary", 6026, -1, 0, 1514, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_cache_sine_6100_binary", 6090, -1, 0, 1478, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 6090, -1, 0, 1513, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 6096, -1, 0, 1479, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, ((void *)0))" },
	{ "maya_cache_sine_6100_binary", 6096, -1, 0, 1514, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, ((void *)0))" },
	{ "maya_character_7500_binary", 11986, -1, 0, 6156, 0, 0, 0, "character" },
	{ "maya_character_7500_binary", 11986, -1, 0, 6408, 0, 0, 0, "character" },
	{ "maya_character_7500_binary", 12267, -1, 0, 6156, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_character_7500_binary", 12267, -1, 0, 6408, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_color_sets_6100_binary", 11034, -1, 0, 0, 77, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 11081, 9966, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 11086, 16081, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 11089, 15524, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cone_6100_binary", 11092, 15571, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease.data,..." },
	{ "maya_constraint_zoo_6100_binary", 12012, -1, 0, 3487, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 12012, -1, 0, 3519, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 12269, -1, 0, 3487, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 12269, -1, 0, 3519, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 16411, -1, 0, 3976, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 16411, -1, 0, 4029, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 18166, -1, 0, 3976, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 18166, -1, 0, 4029, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 18172, -1, 0, 0, 315, 0, 0, "constraint->targets.data" },
	{ "maya_cube_6100_ascii", 12120, -1, 0, 572, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_cube_6100_ascii", 7812, -1, 0, 0, 0, 0, 57, "ufbxi_report_progress(uc)" },
	{ "maya_cube_6100_ascii", 7952, -1, 0, 1, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_cube_6100_ascii", 7952, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_cube_6100_ascii", 8029, -1, 0, 1, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 8029, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 8048, -1, 0, 6, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_cube_6100_ascii", 8077, 514, 0, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_cube_6100_ascii", 8084, 4541, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_cube_6100_ascii", 8143, 190, 0, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_cube_6100_ascii", 8163, 190, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 8200, -1, 0, 272, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8200, -1, 0, 277, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8232, 4998, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 8272, -1, 0, 257, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8272, -1, 0, 262, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8295, 4870, 46, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 8307, 514, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_cube_6100_ascii", 8313, 174, 0, 0, 0, 0, 0, "depth == 0" },
	{ "maya_cube_6100_ascii", 8323, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_cube_6100_ascii", 8327, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_cube_6100_ascii", 8332, -1, 0, 3, 0, 0, 0, "node" },
	{ "maya_cube_6100_ascii", 8332, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_cube_6100_ascii", 8356, -1, 0, 254, 0, 0, 0, "arr" },
	{ "maya_cube_6100_ascii", 8356, -1, 0, 259, 0, 0, 0, "arr" },
	{ "maya_cube_6100_ascii", 8373, -1, 0, 255, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_cube_6100_ascii", 8373, -1, 0, 260, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_cube_6100_ascii", 8377, -1, 0, 256, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_cube_6100_ascii", 8377, -1, 0, 261, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_cube_6100_ascii", 8406, 4870, 46, 0, 0, 0, 0, "ufbxi_ascii_read_float_array(uc, (char)arr_type, &num_r..." },
	{ "maya_cube_6100_ascii", 8408, 4998, 45, 0, 0, 0, 0, "ufbxi_ascii_read_int_array(uc, (char)arr_type, &num_rea..." },
	{ "maya_cube_6100_ascii", 8453, -1, 0, 0, 3, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_cube_6100_ascii", 8483, -1, 0, 491, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8483, -1, 0, 496, 0, 0, 0, "v" },
	{ "maya_cube_6100_ascii", 8594, -1, 0, 284, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_ascii", 8594, -1, 0, 289, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_ascii", 8610, -1, 0, 8, 0, 0, 0, "node->vals" },
	{ "maya_cube_6100_ascii", 8610, -1, 0, 9, 0, 0, 0, "node->vals" },
	{ "maya_cube_6100_ascii", 8620, 174, 11, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_cube_6100_ascii", 8627, -1, 0, 20, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_ascii", 8627, -1, 0, 21, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_ascii", 9158, -1, 0, 1, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_cube_6100_ascii", 9158, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_cube_6100_ascii", 9175, 100, 33, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_cube_6100_binary", 10041, -1, 0, 393, 0, 0, 0, "ptr" },
	{ "maya_cube_6100_binary", 10041, -1, 0, 400, 0, 0, 0, "ptr" },
	{ "maya_cube_6100_binary", 10077, -1, 0, 0, 57, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_cube_6100_binary", 10118, -1, 0, 394, 0, 0, 0, "entry" },
	{ "maya_cube_6100_binary", 10186, -1, 0, 407, 0, 0, 0, "elem_node" },
	{ "maya_cube_6100_binary", 10186, -1, 0, 415, 0, 0, 0, "elem_node" },
	{ "maya_cube_6100_binary", 10339, 7448, 71, 0, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_cube_6100_binary", 10355, 7345, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MappingInformationType, \"C..." },
	{ "maya_cube_6100_binary", 10406, 9992, 14, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_cube_6100_binary", 10441, 7377, 14, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_cube_6100_binary", 10451, 10572, 255, 0, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 10493, -1, 0, 412, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 10500, -1, 0, 413, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 10670, -1, 0, 0, 62, 0, 0, "mesh->faces.data" },
	{ "maya_cube_6100_binary", 10696, 6763, 0, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_cube_6100_binary", 10708, -1, 0, 0, 63, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_6100_binary", 10922, -1, 0, 402, 0, 0, 0, "mesh" },
	{ "maya_cube_6100_binary", 10922, -1, 0, 408, 0, 0, 0, "mesh" },
	{ "maya_cube_6100_binary", 10943, 6763, 23, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_cube_6100_binary", 10953, -1, 0, 0, 438, 0, 0, "index_data" },
	{ "maya_cube_6100_binary", 10980, -1, 0, 0, 61, 0, 0, "edges" },
	{ "maya_cube_6100_binary", 11013, 6763, 0, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "maya_cube_6100_binary", 11028, -1, 0, 404, 0, 0, 0, "bitangents" },
	{ "maya_cube_6100_binary", 11028, -1, 0, 410, 0, 0, 0, "bitangents" },
	{ "maya_cube_6100_binary", 11029, -1, 0, 405, 0, 0, 0, "tangents" },
	{ "maya_cube_6100_binary", 11029, -1, 0, 411, 0, 0, 0, "tangents" },
	{ "maya_cube_6100_binary", 11033, -1, 0, 0, 64, 0, 0, "mesh->uv_sets.data" },
	{ "maya_cube_6100_binary", 11043, 7345, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 11049, 8218, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 11057, 9092, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 11069, 9960, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cube_6100_binary", 11096, 10525, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cube_6100_binary", 11099, 10572, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing.da..." },
	{ "maya_cube_6100_binary", 11114, 10799, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cube_6100_binary", 11119, 10846, 255, 0, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_cube_6100_binary", 11149, 7283, 0, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "maya_cube_6100_binary", 11258, -1, 0, 412, 0, 0, 0, "ufbxi_sort_uv_sets(uc, mesh->uv_sets.data, mesh->uv_set..." },
	{ "maya_cube_6100_binary", 11259, -1, 0, 413, 0, 0, 0, "ufbxi_sort_color_sets(uc, mesh->color_sets.data, mesh->..." },
	{ "maya_cube_6100_binary", 11766, -1, 0, 515, 0, 0, 0, "material" },
	{ "maya_cube_6100_binary", 11766, -1, 0, 527, 0, 0, 0, "material" },
	{ "maya_cube_6100_binary", 12039, -1, 0, 393, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "maya_cube_6100_binary", 12039, -1, 0, 400, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "maya_cube_6100_binary", 12045, -1, 0, 0, 59, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_cube_6100_binary", 12058, -1, 0, 394, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_cube_6100_binary", 12065, -1, 0, 395, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_cube_6100_binary", 12065, -1, 0, 401, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_cube_6100_binary", 12075, -1, 0, 0, 60, 0, 0, "attrib_info.props.props.data" },
	{ "maya_cube_6100_binary", 12080, 6763, 23, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_cube_6100_binary", 12120, -1, 0, 406, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_cube_6100_binary", 12126, 15140, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_cube_6100_binary", 12135, 1331, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_cube_6100_binary", 12141, 15140, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_cube_6100_binary", 12168, -1, 0, 0, 57, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_cube_6100_binary", 12171, 1442, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_cube_6100_binary", 12176, 6763, 23, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, type_st..." },
	{ "maya_cube_6100_binary", 12178, -1, 0, 407, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_cube_6100_binary", 12178, -1, 0, 415, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_cube_6100_binary", 12234, -1, 0, 515, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_cube_6100_binary", 12234, -1, 0, 527, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_cube_6100_binary", 12272, -1, 0, 0, 96, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cube_6100_binary", 12272, -1, 0, 471, 0, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cube_6100_binary", 12290, 16292, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_cube_6100_binary", 12347, -1, 0, 563, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 12347, -1, 0, 577, 0, 0, 0, "conn" },
	{ "maya_cube_6100_binary", 12662, -1, 0, 579, 0, 0, 0, "stack" },
	{ "maya_cube_6100_binary", 12662, -1, 0, 595, 0, 0, 0, "stack" },
	{ "maya_cube_6100_binary", 12663, 16506, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"S\", &stack->name)" },
	{ "maya_cube_6100_binary", 12666, -1, 0, 582, 0, 0, 0, "layer" },
	{ "maya_cube_6100_binary", 12666, -1, 0, 598, 0, 0, 0, "layer" },
	{ "maya_cube_6100_binary", 12668, -1, 0, 584, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_cube_6100_binary", 12668, -1, 0, 600, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_cube_6100_binary", 12673, 16533, 65, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_ReferenceTime, \"LL\", &beg..." },
	{ "maya_cube_6100_binary", 12693, 16459, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_cube_6100_binary", 12697, 16506, 0, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_cube_6100_binary", 12719, 0, 76, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_cube_6100_binary", 12720, 35, 1, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "maya_cube_6100_binary", 12733, -1, 0, 41, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_cube_6100_binary", 12754, -1, 0, 40, 0, 0, 0, "root" },
	{ "maya_cube_6100_binary", 12754, -1, 0, 42, 0, 0, 0, "root" },
	{ "maya_cube_6100_binary", 12756, -1, 0, 45, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 12756, -1, 0, 46, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_6100_binary", 12760, 59, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_cube_6100_binary", 12761, 954, 1, 0, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_cube_6100_binary", 12764, 954, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_cube_6100_binary", 12768, 0, 0, 0, 0, 0, 0, "uc->top_node" },
	{ "maya_cube_6100_binary", 12770, 1331, 1, 0, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_cube_6100_binary", 12773, 16288, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_cube_6100_binary", 12774, 16292, 1, 0, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_cube_6100_binary", 12779, 16309, 64, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_cube_6100_binary", 12780, 16459, 1, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_cube_6100_binary", 12784, 16470, 65, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_cube_6100_binary", 15309, -1, 0, 638, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 15309, -1, 0, 659, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_6100_binary", 15318, -1, 0, 0, 155, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 15406, -1, 0, 640, 0, 0, 0, "((ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_cube_6100_binary", 15416, -1, 0, 0, 159, 0, 0, "list->data" },
	{ "maya_cube_6100_binary", 15539, -1, 0, 665, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 17361, -1, 0, 638, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_cube_6100_binary", 17361, -1, 0, 659, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_cube_6100_binary", 17631, -1, 0, 640, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_cube_6100_binary", 17631, -1, 0, 661, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_cube_6100_binary", 17733, -1, 0, 641, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_cube_6100_binary", 17733, -1, 0, 662, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_cube_6100_binary", 17737, -1, 0, 0, 161, 0, 0, "stack->anim.layers.data" },
	{ "maya_cube_6100_binary", 17751, -1, 0, 0, 162, 0, 0, "layer_desc" },
	{ "maya_cube_6100_binary", 17823, -1, 0, 642, 0, 0, 0, "aprop" },
	{ "maya_cube_6100_binary", 17823, -1, 0, 663, 0, 0, 0, "aprop" },
	{ "maya_cube_6100_binary", 17827, -1, 0, 0, 163, 0, 0, "layer->anim_props.data" },
	{ "maya_cube_6100_binary", 18103, -1, 0, 665, 0, 0, 0, "ufbxi_sort_material_textures(uc, material->textures.dat..." },
	{ "maya_cube_6100_binary", 18183, -1, 0, 0, 164, 0, 0, "descs" },
	{ "maya_cube_6100_binary", 20453, -1, 0, 0, 0, 1, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_cube_6100_binary", 20457, 0, 76, 0, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_cube_6100_binary", 20460, -1, 0, 0, 143, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_6100_binary", 2794, 6765, 255, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_cube_6100_binary", 4198, -1, 0, 0, 4, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 5357, -1, 0, 0, 0, 1, 0, "!uc->eof" },
	{ "maya_cube_6100_binary", 5359, 36, 255, 0, 0, 0, 0, "uc->read_fn" },
	{ "maya_cube_6100_binary", 5443, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_cube_6100_binary", 5520, 36, 255, 0, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_cube_6100_binary", 7286, -1, 0, 0, 0, 7040, 0, "val" },
	{ "maya_cube_6100_binary", 7289, -1, 0, 0, 0, 6793, 0, "val" },
	{ "maya_cube_6100_binary", 7326, 10670, 13, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 7327, 7000, 25, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 7330, 6763, 25, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_cube_6100_binary", 7352, 6765, 255, 0, 0, 0, 0, "data" },
	{ "maya_cube_6100_binary", 7374, -1, 0, 0, 0, 27, 0, "header" },
	{ "maya_cube_6100_binary", 7395, 24, 255, 0, 0, 0, 0, "num_values64 <= 0xffffffffui32" },
	{ "maya_cube_6100_binary", 7413, -1, 0, 1, 0, 0, 0, "node" },
	{ "maya_cube_6100_binary", 7413, -1, 0, 3, 0, 0, 0, "node" },
	{ "maya_cube_6100_binary", 7417, -1, 0, 0, 0, 40, 0, "name" },
	{ "maya_cube_6100_binary", 7419, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_cube_6100_binary", 7435, -1, 0, 260, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 7435, -1, 0, 266, 0, 0, 0, "arr" },
	{ "maya_cube_6100_binary", 7444, -1, 0, 0, 0, 6780, 0, "data" },
	{ "maya_cube_6100_binary", 7594, 6765, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_cube_6100_binary", 7595, 6763, 25, 0, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_cube_6100_binary", 7611, -1, 0, 5, 0, 0, 0, "vals" },
	{ "maya_cube_6100_binary", 7611, -1, 0, 6, 0, 0, 0, "vals" },
	{ "maya_cube_6100_binary", 7619, -1, 0, 0, 0, 87, 0, "data" },
	{ "maya_cube_6100_binary", 7672, 213, 255, 0, 0, 0, 0, "str" },
	{ "maya_cube_6100_binary", 7682, -1, 0, 0, 4, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_cube_6100_binary", 7697, 164, 0, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_cube_6100_binary", 7702, 22, 1, 0, 0, 0, 0, "Bad value type" },
	{ "maya_cube_6100_binary", 7713, 66, 4, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_cube_6100_binary", 7715, 36, 255, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_cube_6100_binary", 7727, 58, 93, 0, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_cube_6100_binary", 7732, 70, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_cube_6100_binary", 7741, -1, 0, 19, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 7741, -1, 0, 20, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 8321, 0, 76, 0, 0, 0, 0, "Expected a 'Name:' token" },
	{ "maya_cube_6100_binary", 8664, -1, 0, 0, 2, 0, 0, "dst" },
	{ "maya_cube_6100_binary", 8665, -1, 0, 4, 0, 0, 0, "((ufbx_dom_node**)ufbxi_push_size_copy((&uc->tmp_dom_no..." },
	{ "maya_cube_6100_binary", 8665, -1, 0, 5, 0, 0, 0, "((ufbx_dom_node**)ufbxi_push_size_copy((&uc->tmp_dom_no..." },
	{ "maya_cube_6100_binary", 8680, -1, 0, 6, 0, 0, 0, "result" },
	{ "maya_cube_6100_binary", 8691, -1, 0, 0, 303, 0, 0, "val" },
	{ "maya_cube_6100_binary", 8719, -1, 0, 7, 0, 0, 0, "val" },
	{ "maya_cube_6100_binary", 8719, -1, 0, 9, 0, 0, 0, "val" },
	{ "maya_cube_6100_binary", 8736, -1, 0, 0, 3, 0, 0, "dst->values.data" },
	{ "maya_cube_6100_binary", 8741, -1, 0, 26, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 8741, -1, 0, 28, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 8746, -1, 0, 0, 25, 0, 0, "dst->children.data" },
	{ "maya_cube_6100_binary", 8756, -1, 0, 0, 58, 0, 0, "children" },
	{ "maya_cube_6100_binary", 8763, -1, 0, 4, 0, 0, 0, "ufbxi_retain_dom_node(uc, node, &uc->dom_parse_toplevel..." },
	{ "maya_cube_6100_binary", 8763, -1, 0, 5, 0, 0, 0, "ufbxi_retain_dom_node(uc, node, &uc->dom_parse_toplevel..." },
	{ "maya_cube_6100_binary", 8770, -1, 0, 0, 731, 0, 0, "nodes" },
	{ "maya_cube_6100_binary", 8773, -1, 0, 0, 732, 0, 0, "dom_root" },
	{ "maya_cube_6100_binary", 8788, -1, 0, 7, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 8788, -1, 0, 9, 0, 0, 0, "ufbxi_retain_dom_node(uc, child, ((void *)0))" },
	{ "maya_cube_6100_binary", 9126, -1, 0, 0, 0, 1, 0, "header" },
	{ "maya_cube_6100_binary", 9177, 35, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_cube_6100_binary", 9204, 0, 76, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_cube_6100_binary", 9206, 22, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_cube_6100_binary", 9215, -1, 0, 0, 730, 0, 0, "ufbxi_retain_toplevel(uc, ((void *)0))" },
	{ "maya_cube_6100_binary", 9225, -1, 0, 2, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 9225, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 9229, -1, 0, 4, 0, 0, 0, "ufbxi_retain_toplevel(uc, node)" },
	{ "maya_cube_6100_binary", 9229, -1, 0, 5, 0, 0, 0, "ufbxi_retain_toplevel(uc, node)" },
	{ "maya_cube_6100_binary", 9244, 39, 19, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_cube_6100_binary", 9252, -1, 0, 59, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 9252, -1, 0, 62, 0, 0, 0, "node->children" },
	{ "maya_cube_6100_binary", 9256, -1, 0, 92, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &node->children[i])" },
	{ "maya_cube_6100_binary", 9256, -1, 0, 94, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &node->children[i])" },
	{ "maya_cube_6100_binary", 9275, 35, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_pars..." },
	{ "maya_cube_6100_binary", 9283, -1, 0, 7, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &uc->top_child)" },
	{ "maya_cube_6100_binary", 9283, -1, 0, 9, 0, 0, 0, "ufbxi_retain_toplevel_child(uc, &uc->top_child)" },
	{ "maya_cube_6100_binary", 9547, -1, 0, 41, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_cube_6100_binary", 9663, 1442, 0, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_cube_6100_binary", 9738, -1, 0, 392, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 9738, -1, 0, 398, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_6100_binary", 9780, -1, 0, 0, 58, 0, 0, "props->props.data" },
	{ "maya_cube_6100_binary", 9783, 1442, 0, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_cube_6100_binary", 9786, -1, 0, 392, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_cube_6100_binary", 9786, -1, 0, 398, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_cube_6100_binary", 9794, -1, 0, 0, 96, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_cube_6100_binary", 9794, -1, 0, 471, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_cube_6100_binary", 9806, 35, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_cube_6100_binary", 9954, 954, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object)" },
	{ "maya_cube_6100_binary", 9961, -1, 0, 71, 0, 0, 0, "tmpl" },
	{ "maya_cube_6100_binary", 9961, -1, 0, 76, 0, 0, 0, "tmpl" },
	{ "maya_cube_6100_binary", 9962, 1022, 0, 0, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_cube_6100_binary", 9989, -1, 0, 0, 25, 0, 0, "uc->templates" },
	{ "maya_cube_7100_ascii", 8555, 8925, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'I')" },
	{ "maya_cube_7100_ascii", 8558, 8929, 11, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_cube_7100_ascii", 8583, 8935, 33, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_cube_7100_binary", 10195, -1, 0, 648, 0, 0, 0, "elem" },
	{ "maya_cube_7100_binary", 10195, -1, 0, 668, 0, 0, 0, "elem" },
	{ "maya_cube_7100_binary", 12154, 12333, 255, 0, 0, 0, 0, "(info.fbx_id & (0x8000000000000000ULL)) == 0" },
	{ "maya_cube_7100_binary", 12203, 12362, 0, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_cube_7100_binary", 12242, -1, 0, 648, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "maya_cube_7100_binary", 12242, -1, 0, 668, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "maya_cube_7100_binary", 12244, -1, 0, 653, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_cube_7100_binary", 12244, -1, 0, 673, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_cube_7100_binary", 12738, 59, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_cube_7100_binary", 12739, 3549, 1, 0, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_cube_7100_binary", 12786, 2241, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "maya_cube_7100_binary", 12791, 18890, 74, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ((void *)0))" },
	{ "maya_cube_7100_binary", 15525, -1, 0, 711, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_7100_binary", 17829, -1, 0, 711, 0, 0, 0, "ufbxi_sort_anim_props(uc, layer->anim_props.data, layer..." },
	{ "maya_cube_7100_binary", 2843, 16067, 1, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_cube_7100_binary", 5425, -1, 0, 0, 0, 0, 1434, "ufbxi_report_progress(uc)" },
	{ "maya_cube_7100_binary", 5548, -1, 0, 0, 0, 12392, 0, "uc->read_fn" },
	{ "maya_cube_7100_binary", 5556, -1, 0, 0, 0, 0, 1434, "ufbxi_resume_progress(uc)" },
	{ "maya_cube_7100_binary", 7479, 12382, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_cube_7100_binary", 7486, 16067, 1, 0, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_7100_binary", 7499, 12379, 99, 0, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_cube_7100_binary", 7515, -1, 0, 0, 0, 12392, 0, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_cube_7100_binary", 7572, 12384, 1, 0, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_cube_7100_binary", 7575, 12384, 255, 0, 0, 0, 0, "Bad array encoding" },
	{ "maya_cube_7100_binary", 9666, 6091, 0, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_cube_7100_binary", 9823, 797, 0, 0, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_cube_7100_binary", 9935, 3549, 1, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_cube_7100_binary", 9968, 4105, 0, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_cube_7100_binary", 9980, -1, 0, 0, 58, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_cube_7100_binary", 9983, 4176, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_cube_7400_binary", 15406, -1, 0, 713, 0, 0, 0, "((ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_cube_7500_binary", 13277, 24, 0, 0, 0, 0, 0, "ufbxi_parse_legacy_toplevel(uc)" },
	{ "maya_cube_7500_binary", 20455, 24, 0, 0, 0, 0, 0, "ufbxi_read_legacy_root(uc)" },
	{ "maya_cube_7500_binary", 9308, 24, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_cube_big_endian_6100_binary", 7077, -1, 0, 1, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 7077, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 7387, -1, 0, 3, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 7387, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 9140, -1, 0, 1, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 9140, -1, 0, 3, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_7100_binary", 7146, -1, 0, 452, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 7146, -1, 0, 469, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 7580, -1, 0, 452, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7100_binary", 7580, -1, 0, 469, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7500_binary", 7378, -1, 0, 3, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_7500_binary", 7378, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_obj", 10090, -1, 0, 6, 0, 0, 0, "entry" },
	{ "maya_cube_obj", 10132, -1, 0, 1, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_obj", 10132, -1, 0, 3, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_obj", 10133, -1, 0, 2, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_obj", 10133, -1, 0, 4, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_obj", 10137, -1, 0, 3, 0, 0, 0, "elem" },
	{ "maya_cube_obj", 10137, -1, 0, 5, 0, 0, 0, "elem" },
	{ "maya_cube_obj", 10145, -1, 0, 6, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, elem->element_id)" },
	{ "maya_cube_obj", 10156, -1, 0, 248, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_obj", 10156, -1, 0, 60, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_cube_obj", 10157, -1, 0, 250, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_obj", 10157, -1, 0, 62, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_cube_obj", 10161, -1, 0, 248, 0, 0, 0, "elem" },
	{ "maya_cube_obj", 10161, -1, 0, 60, 0, 0, 0, "elem" },
	{ "maya_cube_obj", 10202, -1, 0, 252, 0, 0, 0, "conn" },
	{ "maya_cube_obj", 10202, -1, 0, 64, 0, 0, 0, "conn" },
	{ "maya_cube_obj", 13348, -1, 0, 0, 14, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_obj", 13357, -1, 0, 0, 15, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_cube_obj", 13566, -1, 0, 0, 12, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_cube_obj", 13583, -1, 0, 0, 13, 0, 0, "uv_set" },
	{ "maya_cube_obj", 13647, -1, 0, 247, 0, 0, 0, "mesh" },
	{ "maya_cube_obj", 13647, -1, 0, 59, 0, 0, 0, "mesh" },
	{ "maya_cube_obj", 13665, -1, 0, 248, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "maya_cube_obj", 13665, -1, 0, 60, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "maya_cube_obj", 13676, -1, 0, 252, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "maya_cube_obj", 13676, -1, 0, 64, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "maya_cube_obj", 13677, -1, 0, 253, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "maya_cube_obj", 13677, -1, 0, 65, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "maya_cube_obj", 13691, -1, 0, 0, 2, 0, 0, "groups" },
	{ "maya_cube_obj", 13731, -1, 0, 1, 0, 0, 0, "root" },
	{ "maya_cube_obj", 13731, -1, 0, 3, 0, 0, 0, "root" },
	{ "maya_cube_obj", 13733, -1, 0, 4, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_obj", 13733, -1, 0, 7, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_cube_obj", 13808, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_cube_obj", 13814, -1, 0, 291, 0, 0, 0, "new_data" },
	{ "maya_cube_obj", 13814, -1, 0, 79, 0, 0, 0, "new_data" },
	{ "maya_cube_obj", 13874, -1, 0, 5, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "maya_cube_obj", 13874, -1, 0, 8, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "maya_cube_obj", 13910, -1, 0, 291, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "maya_cube_obj", 13910, -1, 0, 79, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "maya_cube_obj", 13911, -1, 0, 5, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "maya_cube_obj", 13911, -1, 0, 8, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "maya_cube_obj", 13929, 92, 33, 0, 0, 0, 0, "offset + read_values <= uc->obj.num_tokens" },
	{ "maya_cube_obj", 13932, -1, 0, 13, 0, 0, 0, "vals" },
	{ "maya_cube_obj", 13932, -1, 0, 26, 0, 0, 0, "vals" },
	{ "maya_cube_obj", 13937, 83, 46, 0, 0, 0, 0, "end == str.data + str.length" },
	{ "maya_cube_obj", 13986, -1, 0, 259, 0, 0, 0, "dst" },
	{ "maya_cube_obj", 13986, -1, 0, 72, 0, 0, 0, "dst" },
	{ "maya_cube_obj", 14028, -1, 0, 247, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "maya_cube_obj", 14028, -1, 0, 59, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "maya_cube_obj", 14065, -1, 0, 66, 0, 0, 0, "entry" },
	{ "maya_cube_obj", 14078, -1, 0, 254, 0, 0, 0, "group" },
	{ "maya_cube_obj", 14078, -1, 0, 67, 0, 0, 0, "group" },
	{ "maya_cube_obj", 14097, -1, 0, 255, 0, 0, 0, "face" },
	{ "maya_cube_obj", 14097, -1, 0, 68, 0, 0, 0, "face" },
	{ "maya_cube_obj", 14106, -1, 0, 256, 0, 0, 0, "p_face_mat" },
	{ "maya_cube_obj", 14106, -1, 0, 69, 0, 0, 0, "p_face_mat" },
	{ "maya_cube_obj", 14111, -1, 0, 257, 0, 0, 0, "p_face_smooth" },
	{ "maya_cube_obj", 14111, -1, 0, 70, 0, 0, 0, "p_face_smooth" },
	{ "maya_cube_obj", 14117, -1, 0, 258, 0, 0, 0, "p_face_group" },
	{ "maya_cube_obj", 14117, -1, 0, 71, 0, 0, 0, "p_face_group" },
	{ "maya_cube_obj", 14124, -1, 0, 259, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "maya_cube_obj", 14124, -1, 0, 72, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "maya_cube_obj", 14239, -1, 0, 0, 3, 0, 0, "data" },
	{ "maya_cube_obj", 14265, 71, 102, 0, 0, 0, 0, "num_indices == 0 || !required" },
	{ "maya_cube_obj", 14277, -1, 0, 0, 9, 0, 0, "dst_indices" },
	{ "maya_cube_obj", 14322, -1, 0, 292, 0, 0, 0, "meshes" },
	{ "maya_cube_obj", 14322, -1, 0, 80, 0, 0, 0, "meshes" },
	{ "maya_cube_obj", 14355, -1, 0, 293, 0, 0, 0, "tmp_indices" },
	{ "maya_cube_obj", 14355, -1, 0, 81, 0, 0, 0, "tmp_indices" },
	{ "maya_cube_obj", 14379, -1, 0, 0, 3, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, m..." },
	{ "maya_cube_obj", 14396, -1, 0, 0, 6, 0, 0, "fbx_mesh->faces.data" },
	{ "maya_cube_obj", 14397, -1, 0, 0, 7, 0, 0, "fbx_mesh->face_material.data" },
	{ "maya_cube_obj", 14402, -1, 0, 0, 8, 0, 0, "fbx_mesh->face_smoothing.data" },
	{ "maya_cube_obj", 14416, 71, 102, 0, 0, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_obj", 14419, -1, 0, 0, 10, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_obj", 14422, -1, 0, 0, 11, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "maya_cube_obj", 14464, -1, 0, 0, 12, 0, 0, "ufbxi_finalize_mesh(&uc->result, &uc->error, fbx_mesh)" },
	{ "maya_cube_obj", 14491, -1, 0, 5, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "maya_cube_obj", 14491, -1, 0, 8, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "maya_cube_obj", 14498, 83, 46, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_POSITION, 1..." },
	{ "maya_cube_obj", 14505, 111, 9, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_COLOR, 4)" },
	{ "maya_cube_obj", 14512, 328, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_UV, 1)" },
	{ "maya_cube_obj", 14514, 622, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_NORMAL, 1)" },
	{ "maya_cube_obj", 14516, -1, 0, 247, 0, 0, 0, "ufbxi_obj_parse_indices(uc)" },
	{ "maya_cube_obj", 14516, -1, 0, 59, 0, 0, 0, "ufbxi_obj_parse_indices(uc)" },
	{ "maya_cube_obj", 14536, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "maya_cube_obj", 14556, -1, 0, 0, 2, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "maya_cube_obj", 14557, 71, 102, 0, 0, 0, 0, "ufbxi_obj_pop_meshes(uc)" },
	{ "maya_cube_obj", 14802, -1, 0, 1, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "maya_cube_obj", 14802, -1, 0, 3, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "maya_cube_obj", 14803, 71, 102, 0, 0, 0, 0, "ufbxi_obj_parse_file(uc)" },
	{ "maya_cube_obj", 14804, -1, 0, 0, 14, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_cube_obj", 14880, -1, 0, 306, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_obj", 14900, -1, 0, 302, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_obj", 14936, -1, 0, 296, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_obj", 14936, -1, 0, 84, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_cube_obj", 15002, -1, 0, 295, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_obj", 15002, -1, 0, 83, 0, 0, 0, "tmp_connections" },
	{ "maya_cube_obj", 15006, -1, 0, 0, 18, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_cube_obj", 15041, -1, 0, 0, 19, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_cube_obj", 15043, -1, 0, 296, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "maya_cube_obj", 15043, -1, 0, 84, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "maya_cube_obj", 15044, -1, 0, 298, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_dst.da..." },
	{ "maya_cube_obj", 15183, -1, 0, 299, 0, 0, 0, "node_ids" },
	{ "maya_cube_obj", 15183, -1, 0, 85, 0, 0, 0, "node_ids" },
	{ "maya_cube_obj", 15186, -1, 0, 300, 0, 0, 0, "node_ptrs" },
	{ "maya_cube_obj", 15186, -1, 0, 86, 0, 0, 0, "node_ptrs" },
	{ "maya_cube_obj", 15197, -1, 0, 301, 0, 0, 0, "node_offsets" },
	{ "maya_cube_obj", 15197, -1, 0, 87, 0, 0, 0, "node_offsets" },
	{ "maya_cube_obj", 15238, -1, 0, 302, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "maya_cube_obj", 15242, -1, 0, 303, 0, 0, 0, "p_offset" },
	{ "maya_cube_obj", 15242, -1, 0, 88, 0, 0, 0, "p_offset" },
	{ "maya_cube_obj", 15331, -1, 0, 307, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_obj", 15331, -1, 0, 91, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_cube_obj", 15340, -1, 0, 0, 23, 0, 0, "list->data" },
	{ "maya_cube_obj", 17257, -1, 0, 0, 16, 0, 0, "uc->scene.elements.data" },
	{ "maya_cube_obj", 17262, -1, 0, 0, 17, 0, 0, "element_data" },
	{ "maya_cube_obj", 17266, -1, 0, 294, 0, 0, 0, "element_offsets" },
	{ "maya_cube_obj", 17266, -1, 0, 82, 0, 0, 0, "element_offsets" },
	{ "maya_cube_obj", 17277, -1, 0, 295, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_cube_obj", 17277, -1, 0, 83, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_cube_obj", 17279, -1, 0, 299, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_cube_obj", 17279, -1, 0, 85, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_cube_obj", 17285, -1, 0, 304, 0, 0, 0, "typed_offsets" },
	{ "maya_cube_obj", 17285, -1, 0, 89, 0, 0, 0, "typed_offsets" },
	{ "maya_cube_obj", 17290, -1, 0, 0, 20, 0, 0, "typed_elems->data" },
	{ "maya_cube_obj", 17302, -1, 0, 0, 22, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_cube_obj", 17315, -1, 0, 306, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "maya_cube_obj", 17403, -1, 0, 307, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_cube_obj", 17403, -1, 0, 91, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_cube_obj", 17579, -1, 0, 0, 24, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_cube_obj", 20447, -1, 0, 1, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_cube_obj", 20462, 71, 102, 0, 0, 0, 0, "ufbxi_obj_load(uc)" },
	{ "maya_cube_obj", 20472, -1, 0, 0, 16, 0, 0, "dom_root" },
	{ "maya_cube_obj", 20480, -1, 0, 294, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cube_obj", 20480, -1, 0, 82, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cube_obj", 20519, -1, 0, 0, 26, 0, 0, "imp" },
	{ "maya_cube_obj", 2799, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_obj", 2844, -1, 0, 8, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_obj", 2844, -1, 0, 9, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_obj", 2900, -1, 0, 5, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_cube_obj", 3581, -1, 0, 1, 0, 0, 0, "data" },
	{ "maya_cube_obj", 4242, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_cube_obj", 4263, -1, 0, 0, 1, 0, 0, "str" },
	{ "maya_cube_obj", 5338, -1, 0, 0, 0, 0, 1, "result != UFBX_PROGRESS_CANCEL" },
	{ "maya_cube_obj", 9573, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_cube_obj", 9579, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_display_layers_6100_binary", 12261, -1, 0, 1529, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_display_layers_6100_binary", 12261, -1, 0, 1554, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_display_layers_6100_binary", 18127, -1, 0, 1685, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_display_layers_6100_binary", 18127, -1, 0, 1723, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_human_ik_6100_binary", 11461, -1, 0, 11453, 0, 0, 0, "marker" },
	{ "maya_human_ik_6100_binary", 11461, -1, 0, 11762, 0, 0, 0, "marker" },
	{ "maya_human_ik_6100_binary", 12110, -1, 0, 17519, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 12110, -1, 0, 18044, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 12112, -1, 0, 11453, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_6100_binary", 12112, -1, 0, 11762, 0, 0, 0, "ufbxi_read_marker(uc, node, &attrib_info, sub_type, UFB..." },
	{ "maya_human_ik_7400_binary", 12193, -1, 0, 2544, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 12193, -1, 0, 2680, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 12195, -1, 0, 1799, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 12195, -1, 0, 1902, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_interpolation_modes_6100_binary", 12460, 16936, 73, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_interpolation_modes_7500_ascii", 8204, -1, 0, 845, 0, 0, 0, "v" },
	{ "maya_interpolation_modes_7500_ascii", 8204, -1, 0, 864, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 7975, -1, 0, 0, 0, 9570, 0, "c != '\\0'" },
	{ "maya_leading_comma_7500_ascii", 8392, 291, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 8566, -1, 0, 0, 0, 9570, 0, "ufbxi_ascii_skip_until(uc, '}')" },
	{ "maya_node_attribute_zoo_6100_ascii", 8487, -1, 0, 5477, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 8487, -1, 0, 5510, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 8515, -1, 0, 5903, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 8515, -1, 0, 6002, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_binary", 11304, -1, 0, 4125, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 11304, -1, 0, 4163, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 11309, 138209, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->basis..." },
	{ "maya_node_attribute_zoo_6100_binary", 11311, 138308, 255, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 11318, 138359, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 11319, 138416, 1, 0, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 11320, 143462, 27, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 11334, -1, 0, 4194, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 11334, -1, 0, 4233, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 11339, 139478, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 11340, 139592, 1, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"ZZ\", &dimens..." },
	{ "maya_node_attribute_zoo_6100_binary", 11341, 139631, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Step, \"II\", &step_u, &ste..." },
	{ "maya_node_attribute_zoo_6100_binary", 11342, 139664, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 11355, 139691, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 11356, 139727, 1, 0, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 11357, 140321, 3, 0, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 11358, 141818, 63, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 11359, 139655, 1, 0, 0, 0, 0, "points->size / 4 == (size_t)dimension_u * (size_t)dimen..." },
	{ "maya_node_attribute_zoo_6100_binary", 11446, -1, 0, 707, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 11446, -1, 0, 717, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 12086, -1, 0, 707, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 12086, -1, 0, 717, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 12088, -1, 0, 274, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12088, -1, 0, 280, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12106, -1, 0, 1957, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12106, -1, 0, 1973, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12114, -1, 0, 2778, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12114, -1, 0, 2802, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 12183, -1, 0, 3865, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 12183, -1, 0, 3899, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 12207, 138209, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 12209, 139478, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 12213, -1, 0, 4331, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 12213, -1, 0, 4372, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 12215, -1, 0, 4376, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 12215, -1, 0, 4418, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 16441, -1, 0, 0, 486, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 16484, -1, 0, 0, 505, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 17720, -1, 0, 0, 486, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 17725, -1, 0, 0, 495, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 17726, -1, 0, 0, 496, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 18200, -1, 0, 0, 505, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_7500_ascii", 8484, -1, 0, 3317, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 8484, -1, 0, 3366, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 8485, -1, 0, 3304, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 8485, -1, 0, 3353, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_binary", 10076, -1, 0, 0, 325, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_node_attribute_zoo_7500_binary", 11577, -1, 0, 1727, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 11577, -1, 0, 1782, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 11582, 61038, 255, 0, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 11583, 61115, 255, 0, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 11584, 61175, 255, 0, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 11585, 61234, 255, 0, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 11586, 61292, 255, 0, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_binary", 11589, 61122, 0, 0, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_binary", 11594, 61242, 0, 0, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_binary", 11595, 61300, 0, 0, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", 11599, -1, 0, 0, 326, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 11750, 61431, 0, 0, 0, 0, 0, "refs_left >= 0" },
	{ "maya_node_attribute_zoo_7500_binary", 12181, -1, 0, 649, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 12181, -1, 0, 671, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 12185, -1, 0, 580, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 12185, -1, 0, 600, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 12187, -1, 0, 488, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 12187, -1, 0, 505, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 12189, -1, 0, 700, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 12189, -1, 0, 723, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 12197, -1, 0, 1134, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 12197, -1, 0, 1162, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 12246, -1, 0, 1737, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 12246, -1, 0, 1793, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 12248, 61038, 255, 0, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", 15122, -1, 0, 2083, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 15122, -1, 0, 2155, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 15163, -1, 0, 2085, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 15163, -1, 0, 2157, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_7500_binary", 7328, 61146, 109, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 7329, 61333, 103, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 7567, -1, 0, 0, 0, 0, 2942, "ufbxi_resume_progress(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 7571, -1, 0, 0, 0, 0, 2943, "res != -28" },
	{ "maya_polygon_hole_6100_binary", 11142, 9377, 37, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_polygon_hole_6100_binary", 11144, 9342, 0, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_hole.data, &..." },
	{ "maya_resampled_7500_binary", 11623, 24917, 23, 0, 0, 0, 0, "p_ref < p_ref_end" },
	{ "maya_shaderfx_pbs_material_7700_ascii", 9720, -1, 0, 1341, 0, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "maya_shaderfx_pbs_material_7700_ascii", 9720, -1, 0, 1389, 0, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "maya_texture_layers_6100_binary", 11804, -1, 0, 1444, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 11804, -1, 0, 1474, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 11813, -1, 0, 1446, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 11813, -1, 0, 1476, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 12238, -1, 0, 1444, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 12238, -1, 0, 1474, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 15474, -1, 0, 1651, 0, 0, 0, "((ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_texture_layers_6100_binary", 15474, -1, 0, 1693, 0, 0, 0, "((ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_texture_layers_6100_binary", 15481, -1, 0, 0, 267, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 16929, -1, 0, 1661, 0, 0, 0, "textures" },
	{ "maya_texture_layers_6100_binary", 16929, -1, 0, 1704, 0, 0, 0, "textures" },
	{ "maya_texture_layers_6100_binary", 16931, -1, 0, 1705, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_texture_layers_6100_binary", 17005, -1, 0, 1658, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 17005, -1, 0, 1701, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 17025, -1, 0, 1661, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &deps, &..." },
	{ "maya_texture_layers_6100_binary", 17025, -1, 0, 1704, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &deps, &..." },
	{ "maya_texture_layers_6100_binary", 17036, -1, 0, 1662, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 17036, -1, 0, 1706, 0, 0, 0, "dst" },
	{ "maya_texture_layers_6100_binary", 17044, -1, 0, 1665, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &files, ..." },
	{ "maya_texture_layers_6100_binary", 17044, -1, 0, 1709, 0, 0, 0, "ufbxi_deduplicate_textures(uc, &uc->tmp_parse, &files, ..." },
	{ "maya_texture_layers_6100_binary", 17048, -1, 0, 0, 272, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 17079, -1, 0, 1656, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 17079, -1, 0, 1699, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "maya_texture_layers_6100_binary", 18082, -1, 0, 1651, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_texture_layers_6100_binary", 18082, -1, 0, 1693, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_textured_cube_6100_binary", 17941, -1, 0, 1649, 0, 0, 0, "mat_texs" },
	{ "maya_textured_cube_6100_binary", 17941, -1, 0, 1690, 0, 0, 0, "mat_texs" },
	{ "maya_transform_animation_6100_binary", 12495, 17549, 11, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "mtl_fuzz_0000", 14623, -1, 0, 0, 4, 0, 0, "ufbxi_push_string_place_blob(&uc->string_pool, &prop->v..." },
	{ "mtl_fuzz_0000", 4281, -1, 0, 0, 4, 0, 0, "p_blob->data" },
	{ "obj_fuzz_0030", 14524, -1, 0, 26, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "obj_fuzz_0030", 14524, -1, 0, 47, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "obj_fuzz_0159", 13966, 116, 11, 0, 0, 0, 0, "index < 0xffffffffffffffffui64 / 10 - 10" },
	{ "revit_empty_7400_binary", 10292, -1, 0, 0, 258, 0, 0, "new_indices" },
	{ "revit_empty_7400_binary", 10375, -1, 0, 0, 258, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "revit_empty_7400_binary", 12276, -1, 0, 894, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadat..." },
	{ "revit_empty_7400_binary", 12276, -1, 0, 916, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadat..." },
	{ "revit_empty_7400_binary", 7273, -1, 0, 0, 301, 0, 0, "d->data" },
	{ "synthetic_binary_props_7500_ascii", 10235, -1, 0, 943, 0, 0, 0, "unknown" },
	{ "synthetic_binary_props_7500_ascii", 10235, -1, 0, 965, 0, 0, 0, "unknown" },
	{ "synthetic_binary_props_7500_ascii", 12278, -1, 0, 943, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_binary_props_7500_ascii", 12278, -1, 0, 965, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "synthetic_binary_props_7500_ascii", 8427, -1, 0, 104, 0, 0, 0, "v->data" },
	{ "synthetic_binary_props_7500_ascii", 8427, -1, 0, 107, 0, 0, 0, "v->data" },
	{ "synthetic_blend_shape_order_7500_ascii", 10527, -1, 0, 769, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_blend_shape_order_7500_ascii", 10578, -1, 0, 753, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 10578, -1, 0, 768, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 10586, -1, 0, 769, 0, 0, 0, "ufbxi_sort_blend_offsets(uc, offsets, num_offsets)" },
	{ "synthetic_broken_filename_7500_ascii", 11780, -1, 0, 828, 0, 0, 0, "texture" },
	{ "synthetic_broken_filename_7500_ascii", 11780, -1, 0, 852, 0, 0, 0, "texture" },
	{ "synthetic_broken_filename_7500_ascii", 11833, -1, 0, 794, 0, 0, 0, "video" },
	{ "synthetic_broken_filename_7500_ascii", 11833, -1, 0, 816, 0, 0, 0, "video" },
	{ "synthetic_broken_filename_7500_ascii", 12236, -1, 0, 828, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 12236, -1, 0, 852, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 12240, -1, 0, 794, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 12240, -1, 0, 816, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "synthetic_broken_filename_7500_ascii", 13441, -1, 0, 919, 0, 0, 0, "result" },
	{ "synthetic_broken_filename_7500_ascii", 13441, -1, 0, 951, 0, 0, 0, "result" },
	{ "synthetic_broken_filename_7500_ascii", 13461, -1, 0, 0, 256, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 15382, -1, 0, 917, 0, 0, 0, "tex" },
	{ "synthetic_broken_filename_7500_ascii", 15382, -1, 0, 949, 0, 0, 0, "tex" },
	{ "synthetic_broken_filename_7500_ascii", 15392, -1, 0, 0, 255, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 16971, -1, 0, 923, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_broken_filename_7500_ascii", 16971, -1, 0, 956, 0, 0, 0, "((ufbx_texture**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_broken_filename_7500_ascii", 16975, -1, 0, 924, 0, 0, 0, "states" },
	{ "synthetic_broken_filename_7500_ascii", 16975, -1, 0, 957, 0, 0, 0, "states" },
	{ "synthetic_broken_filename_7500_ascii", 17060, -1, 0, 0, 259, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 17186, -1, 0, 919, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 17186, -1, 0, 951, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 17918, -1, 0, 917, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 17918, -1, 0, 949, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 18030, -1, 0, 918, 0, 0, 0, "content_videos" },
	{ "synthetic_broken_filename_7500_ascii", 18030, -1, 0, 950, 0, 0, 0, "content_videos" },
	{ "synthetic_broken_filename_7500_ascii", 18035, -1, 0, 919, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 18035, -1, 0, 951, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 18036, -1, 0, 920, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 18036, -1, 0, 952, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 18077, -1, 0, 921, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_broken_filename_7500_ascii", 18077, -1, 0, 953, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_broken_filename_7500_ascii", 18078, -1, 0, 922, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_broken_filename_7500_ascii", 18078, -1, 0, 954, 0, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_broken_filename_7500_ascii", 18203, -1, 0, 923, 0, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_broken_filename_7500_ascii", 18203, -1, 0, 956, 0, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_color_suzanne_0_obj", 14507, -1, 0, 14, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_0_obj", 14507, -1, 0, 26, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_1_obj", 14311, -1, 0, 2182, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "synthetic_color_suzanne_1_obj", 14311, -1, 0, 8446, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "synthetic_cube_nan_6100_ascii", 8053, 4866, 45, 0, 0, 0, 0, "token->type == 'F'" },
	{ "synthetic_empty_elements_7500_ascii", 15226, 2800, 49, 0, 0, 0, 0, "depth <= num_nodes" },
	{ "synthetic_face_groups_0_obj", 14530, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "synthetic_indexed_by_vertex_7500_ascii", 10381, -1, 0, 0, 159, 0, 0, "new_index_data" },
	{ "synthetic_missing_version_6100_ascii", 11858, -1, 0, 3880, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 11858, -1, 0, 3901, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 11882, -1, 0, 3883, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 11882, -1, 0, 3904, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 11892, -1, 0, 3884, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 11892, -1, 0, 3905, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 12108, -1, 0, 250, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 12108, -1, 0, 253, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 12250, -1, 0, 3880, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 12250, -1, 0, 3901, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 17372, -1, 0, 0, 252, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_mixed_attribs_0_obj", 14502, -1, 0, 215, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_0_obj", 14502, -1, 0, 78, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14359, -1, 0, 0, 16, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, 0..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14362, -1, 0, 0, 19, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14364, -1, 0, 110, 0, 0, 0, "color_valid" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 14364, -1, 0, 248, 0, 0, 0, "color_valid" },
	{ "synthetic_node_depth_fail_7400_binary", 7366, 23, 233, 0, 0, 0, 0, "depth < 64" },
	{ "synthetic_node_depth_fail_7500_ascii", 8319, 1, 33, 0, 0, 0, 0, "depth < 64" },
	{ "synthetic_parent_directory_7700_ascii", 17129, -1, 0, 958, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_parent_directory_7700_ascii", 17169, -1, 0, 0, 261, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 17183, -1, 0, 0, 261, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_parent_directory_7700_ascii", 17183, -1, 0, 958, 0, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_partial_attrib_0_obj", 14449, -1, 0, 0, 10, 0, 0, "indices" },
	{ "synthetic_simple_materials_0_mtl", 13636, -1, 0, 54, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "synthetic_simple_materials_0_mtl", 14717, -1, 0, 12, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "synthetic_simple_materials_0_mtl", 14717, -1, 0, 16, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "synthetic_simple_materials_0_mtl", 14812, -1, 0, 1, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "synthetic_simple_materials_0_mtl", 14812, -1, 0, 3, 0, 0, 0, "ufbxi_obj_init(uc)" },
	{ "synthetic_simple_materials_0_mtl", 14813, -1, 0, 0, 1, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "synthetic_simple_materials_0_mtl", 14814, -1, 0, 5, 0, 0, 0, "ufbxi_obj_parse_mtl(uc)" },
	{ "synthetic_simple_materials_0_mtl", 14814, -1, 0, 8, 0, 0, 0, "ufbxi_obj_parse_mtl(uc)" },
	{ "synthetic_simple_materials_0_mtl", 20465, -1, 0, 1, 0, 0, 0, "ufbxi_mtl_load(uc)" },
	{ "synthetic_simple_materials_0_mtl", 20465, -1, 0, 3, 0, 0, 0, "ufbxi_mtl_load(uc)" },
	{ "synthetic_simple_textures_0_mtl", 14721, -1, 0, 144, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 0)" },
	{ "synthetic_simple_textures_0_mtl", 14721, -1, 0, 90, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 0)" },
	{ "synthetic_string_collision_7500_ascii", 4212, -1, 0, 2213, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_texture_opts_0_mtl", 14664, -1, 0, 18, 0, 0, 0, "ufbxi_obj_parse_prop(uc, tok, start + 1, 0, &start)" },
	{ "synthetic_texture_opts_0_mtl", 14664, -1, 0, 45, 0, 0, 0, "ufbxi_obj_parse_prop(uc, tok, start + 1, 0, &start)" },
	{ "synthetic_texture_opts_0_mtl", 14674, -1, 0, 0, 22, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tex_str,..." },
	{ "synthetic_texture_split_7500_ascii", 8519, 28571, 35, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_texture_split_7500_binary", 9629, -1, 0, 0, 229, 0, 0, "dst" },
	{ "synthetic_unicode_7500_binary", 4074, -1, 0, 12, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 4170, -1, 0, 1140, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_unicode_7500_binary", 4181, -1, 0, 12, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4083, -1, 0, 710, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_error_identity_6100_binary", 4224, -1, 0, 710, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "zbrush_d20_6100_binary", 10223, -1, 0, 885, 0, 0, 0, "conn" },
	{ "zbrush_d20_6100_binary", 10223, -1, 0, 895, 0, 0, 0, "conn" },
	{ "zbrush_d20_6100_binary", 10540, -1, 0, 887, 0, 0, 0, "shape" },
	{ "zbrush_d20_6100_binary", 10540, -1, 0, 897, 0, 0, 0, "shape" },
	{ "zbrush_d20_6100_binary", 10548, 25242, 2, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "zbrush_d20_6100_binary", 10549, 25217, 0, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "zbrush_d20_6100_binary", 10562, 25290, 2, 0, 0, 0, 0, "normals && normals->size == vertices->size" },
	{ "zbrush_d20_6100_binary", 10608, 25189, 0, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "zbrush_d20_6100_binary", 10612, -1, 0, 878, 0, 0, 0, "deformer" },
	{ "zbrush_d20_6100_binary", 10612, -1, 0, 888, 0, 0, 0, "deformer" },
	{ "zbrush_d20_6100_binary", 10613, -1, 0, 881, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "zbrush_d20_6100_binary", 10613, -1, 0, 891, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "zbrush_d20_6100_binary", 10618, -1, 0, 882, 0, 0, 0, "channel" },
	{ "zbrush_d20_6100_binary", 10618, -1, 0, 892, 0, 0, 0, "channel" },
	{ "zbrush_d20_6100_binary", 10621, -1, 0, 884, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_6100_binary", 10621, -1, 0, 894, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_6100_binary", 10625, -1, 0, 0, 101, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 10637, -1, 0, 885, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "zbrush_d20_6100_binary", 10637, -1, 0, 895, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "zbrush_d20_6100_binary", 10648, -1, 0, 886, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &shape_info.fbx_id)" },
	{ "zbrush_d20_6100_binary", 10648, -1, 0, 896, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &shape_info.fbx_id)" },
	{ "zbrush_d20_6100_binary", 10652, 25217, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "zbrush_d20_6100_binary", 10654, -1, 0, 890, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "zbrush_d20_6100_binary", 10654, -1, 0, 900, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "zbrush_d20_6100_binary", 10655, -1, 0, 891, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "zbrush_d20_6100_binary", 10655, -1, 0, 901, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "zbrush_d20_6100_binary", 10775, -1, 0, 0, 68, 0, 0, "ids" },
	{ "zbrush_d20_6100_binary", 10811, -1, 0, 0, 69, 0, 0, "groups" },
	{ "zbrush_d20_6100_binary", 10926, 25189, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "zbrush_d20_6100_binary", 11135, 8305, 32, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "zbrush_d20_6100_binary", 11137, 8394, 33, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_group.data, ..." },
	{ "zbrush_d20_6100_binary", 11254, -1, 0, 0, 68, 0, 0, "ufbxi_assign_face_groups(&uc->result, &uc->error, mesh,..." },
	{ "zbrush_d20_6100_binary", 14915, -1, 0, 1447, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "zbrush_d20_6100_binary", 15430, -1, 0, 1395, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "zbrush_d20_6100_binary", 15430, -1, 0, 1436, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "zbrush_d20_6100_binary", 15438, -1, 0, 0, 267, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 15451, -1, 0, 1388, 0, 0, 0, "((ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_s..." },
	{ "zbrush_d20_6100_binary", 15451, -1, 0, 1425, 0, 0, 0, "((ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_s..." },
	{ "zbrush_d20_6100_binary", 15458, -1, 0, 0, 258, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 15553, -1, 0, 1452, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "zbrush_d20_6100_binary", 15588, -1, 0, 1426, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "zbrush_d20_6100_binary", 17524, -1, 0, 1385, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 17524, -1, 0, 1422, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 17554, -1, 0, 1387, 0, 0, 0, "full_weights" },
	{ "zbrush_d20_6100_binary", 17554, -1, 0, 1424, 0, 0, 0, "full_weights" },
	{ "zbrush_d20_6100_binary", 17559, -1, 0, 1388, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 17559, -1, 0, 1425, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 17570, -1, 0, 1426, 0, 0, 0, "ufbxi_sort_blend_keyframes(uc, channel->keyframes.data,..." },
	{ "zbrush_d20_6100_binary", 17692, -1, 0, 1394, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 17692, -1, 0, 1435, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 17694, -1, 0, 1395, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "zbrush_d20_6100_binary", 17694, -1, 0, 1436, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "zbrush_d20_6100_binary", 17933, -1, 0, 1400, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 17933, -1, 0, 1443, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 17963, -1, 0, 1401, 0, 0, 0, "mat_tex" },
	{ "zbrush_d20_6100_binary", 17963, -1, 0, 1444, 0, 0, 0, "mat_tex" },
	{ "zbrush_d20_6100_binary", 17984, -1, 0, 1447, 0, 0, 0, "ufbxi_sort_tmp_material_textures(uc, mat_texs, num_mate..." },
	{ "zbrush_d20_6100_binary", 17997, -1, 0, 0, 273, 0, 0, "texs" },
	{ "zbrush_d20_6100_binary", 18016, -1, 0, 1404, 0, 0, 0, "tex" },
	{ "zbrush_d20_6100_binary", 18016, -1, 0, 1448, 0, 0, 0, "tex" },
	{ "zbrush_d20_6100_binary", 18043, -1, 0, 1452, 0, 0, 0, "ufbxi_sort_videos_by_filename(uc, content_videos, num_c..." },
	{ "zbrush_d20_7500_ascii", 11850, -1, 0, 0, 256, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "zbrush_d20_7500_ascii", 8144, -1, 0, 1470, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "zbrush_d20_7500_ascii", 8144, -1, 0, 1496, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "zbrush_d20_7500_ascii", 9647, -1, 0, 0, 256, 0, 0, "dst_blob->data" },
	{ "zbrush_d20_7500_binary", 11533, -1, 0, 1054, 0, 0, 0, "channel" },
	{ "zbrush_d20_7500_binary", 11533, -1, 0, 1082, 0, 0, 0, "channel" },
	{ "zbrush_d20_7500_binary", 11541, -1, 0, 1057, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_7500_binary", 11541, -1, 0, 1085, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "zbrush_d20_7500_binary", 12205, 32981, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 12225, -1, 0, 1042, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "zbrush_d20_7500_binary", 12225, -1, 0, 1070, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "zbrush_d20_7500_binary", 12227, -1, 0, 1054, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "zbrush_d20_7500_binary", 12227, -1, 0, 1082, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 11950, -1, 0, 1271, 0, 0, 0, "set" },
	{ "zbrush_d20_selection_set_6100_binary", 11950, -1, 0, 1312, 0, 0, 0, "set" },
	{ "zbrush_d20_selection_set_6100_binary", 11967, -1, 0, 962, 0, 0, 0, "sel" },
	{ "zbrush_d20_selection_set_6100_binary", 11967, -1, 0, 978, 0, 0, 0, "sel" },
	{ "zbrush_d20_selection_set_6100_binary", 12257, -1, 0, 1271, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 12257, -1, 0, 1312, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 12264, -1, 0, 962, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 12264, -1, 0, 978, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 18132, -1, 0, 2156, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_d20_selection_set_6100_binary", 18132, -1, 0, 2256, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_polygroup_mess_0_obj", 10891, -1, 0, 0, 2036, 0, 0, "face_indices" },
	{ "zbrush_polygroup_mess_0_obj", 14409, -1, 0, 0, 2033, 0, 0, "fbx_mesh->face_group.data" },
	{ "zbrush_polygroup_mess_0_obj", 14467, -1, 0, 0, 2036, 0, 0, "ufbxi_update_face_groups(&uc->result, &uc->error, fbx_m..." },
	{ "zbrush_vertex_color_obj", 13596, -1, 0, 0, 11, 0, 0, "color_set" },
	{ "zbrush_vertex_color_obj", 14168, -1, 0, 127, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_obj", 14168, -1, 0, 25, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_obj", 14310, -1, 0, 246, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_obj", 14310, -1, 0, 68, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_obj", 14325, -1, 0, 246, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_obj", 14325, -1, 0, 68, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_obj", 14384, -1, 0, 0, 0, 880, 0, "min_ix < 0xffffffffffffffffui64" },
	{ "zbrush_vertex_color_obj", 14385, -1, 0, 0, 4, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "zbrush_vertex_color_obj", 14387, -1, 0, 248, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_obj", 14387, -1, 0, 70, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_obj", 14543, -1, 0, 127, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_obj", 14543, -1, 0, 25, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_obj", 14656, -1, 0, 265, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_obj", 14656, -1, 0, 73, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_obj", 14679, -1, 0, 266, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_obj", 14679, -1, 0, 74, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_obj", 14688, -1, 0, 0, 17, 0, 0, "ufbxi_obj_pop_props(uc, &texture->props.props, num_prop..." },
	{ "zbrush_vertex_color_obj", 14694, -1, 0, 0, 18, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop, 0)" },
	{ "zbrush_vertex_color_obj", 14697, -1, 0, 269, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_obj", 14697, -1, 0, 77, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_obj", 14719, -1, 0, 265, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
	{ "zbrush_vertex_color_obj", 14719, -1, 0, 73, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
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
		const char *format = iter->format_ix == 1 ? "ascii" : "binary";
		snprintf(buffer, buffer_size, "%s%s_%u_%s.fbx", iter->root ? iter->root : data_root, iter->path, version, format);

		iter->format_ix++;
		if (iter->format_ix >= 2) {
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

	const ufbx_load_opts *fuzz_opts = NULL;
	if ((flags & UFBXT_FILE_TEST_FLAG_FUZZ_OPTS) != 0) {
		fuzz_opts = &user_opts;
	}

	ufbx_scene *obj_scene = NULL;
	if (obj_file) {
		ufbxt_logf("%s [diff target found]", buf);

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

		{
			ufbxt_diff_error err = { 0 };
			ufbxt_diff_to_obj(obj_scene, obj_file, &err, false);
			if (err.num > 0) {
				ufbx_real avg = err.sum / (ufbx_real)err.num;
				ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", avg, err.max, err.num);
			}
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

	ufbxt_begin_fuzz();

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

			if (scene && obj_file && !alternative) {
				ufbxt_diff_to_obj(scene, obj_file, &err, false);
			}

			test_fn(scene, &err, &error);

			if (err.num > 0) {
				ufbx_real avg = err.sum / (ufbx_real)err.num;
				ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", avg, err.max, err.num);
			}

			if (!alternative || fuzz_always) {
				ufbxt_do_fuzz(base_name, data, size, buf, allow_error, UFBX_FILE_FORMAT_UNKNOWN, fuzz_opts);
			}

			if ((!alternative || fuzz_always) && scene) {
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
#define UFBXT_TEST(name) { #name, &ufbxt_test_fn_##name },
#define UFBXT_FILE_TEST_FLAGS(name, flags) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_PATH_FLAGS(name, path, flags) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS_FLAGS(name, get_opts, flags) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_SUFFIX_FLAGS(name, suffix, flags) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_SUFFIX_OPTS_FLAGS(name, suffix, get_opts, flags) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_ALT_FLAGS(name, file, flags) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS_ALT_FLAGS(name, file, get_opts, flags) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_DEFLATE_TEST(name) { #name, &ufbxt_test_fn_deflate_##name },

ufbxt_test g_tests[] = {
	#include "all_tests.h"
};

int ufbxt_run_test(ufbxt_test *test)
{
	printf("%s: ", test->name);
	fflush(stdout);

	g_error.stack_size = 0;
	g_hint[0] = '\0';

	g_current_test = test;
	if (!ufbxt_setjmp(g_test_jmp)) {
		g_skip_print_ok = false;
		test->func();
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

int main(int argc, char **argv)
{
	uint32_t num_tests = ufbxt_arraycount(g_tests);
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
		if (!strcmp(argv[i], "-f")) {
			if (++i < argc) g_file_version = (uint32_t)atoi(argv[i]);
			if (++i < argc) g_file_type = argv[i];
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
		if (test_filter && strcmp(test->name, test_filter)) {
			continue;
		}

		num_ran++;
		bool print_always = false;
		if (ufbxt_run_test(test)) {
			num_ok++;
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

