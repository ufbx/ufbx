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
	if (!g_verbose) return;

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

void ufbxt_log_flush()
{
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

	bool *freed_ptr;

	union {
		uint64_t align;
		char data[1024 * 1024];
	} local;
} ufbxt_allocator;

static void *ufbxt_alloc(void *user, size_t size)
{
	ufbxt_allocator *ator = (ufbxt_allocator*)user;
	ator->bytes_allocated += size;
	if (size < 1024 && sizeof(ator->local.data) - ator->offset >= size) {
		void *ptr = ator->local.data + ator->offset;
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
	if ((uintptr_t)ptr >= (uintptr_t)ator->local.data
		&& (uintptr_t)ptr < (uintptr_t)(ator->local.data + sizeof(ator->local.data))) {
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
static size_t g_fuzz_step = SIZE_MAX;

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

int ufbxt_test_fuzz(const char *filename, void *data, size_t size, size_t step, int offset, size_t temp_limit, size_t result_limit, size_t truncate_length, size_t cancel_step)
{
	if (g_fuzz_step < SIZE_MAX && step != g_fuzz_step) return 1;

	#if UFBXT_HAS_THREADLOCAL
		t_jmp_buf = (ufbxt_jmp_buf*)calloc(1, sizeof(ufbxt_jmp_buf));
	#endif

	int ret = 1;
	if (!ufbxt_setjmp(*t_jmp_buf)) {

		ufbx_load_opts opts = { 0 };
		ufbxt_cancel_ctx cancel_ctx = { 0 };

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
// From commit 8559a7b
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "blender_279_ball_6100_ascii", 13802, -1, 0, 0, 237, 0, 0, "mat->face_indices.data" },
	{ "blender_279_ball_6100_ascii", 8976, 18422, 84, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_smoothing.da..." },
	{ "blender_279_sausage_6100_ascii", 13538, -1, 0, 0, 414, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_6100_ascii", 13581, -1, 0, 0, 415, 0, 0, "skin->vertices.data" },
	{ "blender_279_sausage_6100_ascii", 13585, -1, 0, 0, 416, 0, 0, "skin->weights.data" },
	{ "blender_279_sausage_6100_ascii", 13823, -1, 0, 0, 420, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "blender_279_sausage_7400_binary", 10096, -1, 0, 710, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 10098, 23076, 0, 0, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 10121, -1, 0, 838, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "blender_279_sausage_7400_binary", 10125, 21748, 0, 0, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "blender_279_sausage_7400_binary", 9339, -1, 0, 710, 0, 0, 0, "skin" },
	{ "blender_279_sausage_7400_binary", 9371, -1, 0, 733, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_7400_binary", 9377, 23076, 0, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "blender_279_sausage_7400_binary", 9388, 23900, 0, 0, 0, 0, 0, "transform->size >= 16" },
	{ "blender_279_sausage_7400_binary", 9389, 24063, 0, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "blender_279_sausage_7400_binary", 9445, -1, 0, 861, 0, 0, 0, "curve" },
	{ "blender_279_sausage_7400_binary", 9726, -1, 0, 695, 0, 0, 0, "pose" },
	{ "blender_279_sausage_7400_binary", 9747, 21748, 0, 0, 0, 0, 0, "matrix->size >= 16" },
	{ "blender_279_sausage_7400_binary", 9750, -1, 0, 697, 0, 0, 0, "tmp_pose" },
	{ "blender_279_sausage_7400_binary", 9760, -1, 0, 702, 0, 0, 0, "pose->bone_poses.data" },
	{ "blender_279_unicode_6100_ascii", 10599, 432, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "blender_279_uv_sets_6100_ascii", 5027, -1, 0, 721, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 5031, -1, 0, 722, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 9040, -1, 0, 0, 63, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 9046, -1, 0, 720, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 9133, -1, 0, 721, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 9136, -1, 0, 723, 0, 0, 0, "extra->texture_arr" },
	{ "blender_293_barbarian_7400_binary", 10100, -1, 0, 998, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "blender_293_barbarian_7400_binary", 10102, -1, 0, 1010, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "blender_293_barbarian_7400_binary", 9401, -1, 0, 1010, 0, 0, 0, "channel" },
	{ "blender_293_barbarian_7400_binary", 9409, -1, 0, 1012, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "fuzz_0000", 10222, -1, 0, 484, 0, 0, 0, "conn" },
	{ "fuzz_0000", 11662, -1, 0, 496, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "fuzz_0001", 13895, -1, 0, 720, 0, 0, 0, "aprop" },
	{ "fuzz_0001", 13946, -1, 0, 725, 0, 0, 0, "aprop" },
	{ "fuzz_0001", 7011, -1, 0, 528, 0, 0, 0, "v" },
	{ "fuzz_0002", 11737, -1, 0, 789, 0, 0, 0, "((ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "fuzz_0003", 11453, -1, 0, 723, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "fuzz_0003", 11479, -1, 0, 724, 0, 0, 0, "new_prop" },
	{ "fuzz_0003", 11494, -1, 0, 726, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "fuzz_0018", 11153, 810, 0, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "fuzz_0070", 2937, -1, 0, 34, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0272", 10153, -1, 0, 453, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0272", 2926, -1, 0, 455, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0272", 3067, -1, 0, 455, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "fuzz_0272", 8294, -1, 0, 453, 0, 0, 0, "unknown" },
	{ "fuzz_0272", 8303, -1, 0, 455, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "fuzz_0393", 8454, -1, 0, 0, 137, 0, 0, "index_data" },
	{ "fuzz_0491", 11216, -1, 0, 28, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 11236, -1, 0, 25, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 11569, -1, 0, 25, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "fuzz_0491", 13439, -1, 0, 28, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "fuzz_0561", 10092, -1, 0, 454, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "max2009_blob_5800_ascii", 6900, -1, 0, 0, 118, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 7439, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "max2009_blob_5800_binary", 10805, -1, 0, 571, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 10813, -1, 0, 0, 142, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 10854, -1, 0, 111, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 10861, -1, 0, 0, 44, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 10869, -1, 0, 311, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 10876, -1, 0, 0, 96, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 10997, 56700, 78, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material.dat..." },
	{ "max2009_blob_5800_binary", 11026, 6207, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 11027, -1, 0, 0, 141, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 11028, -1, 0, 571, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 11029, -1, 0, 573, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 11061, -1, 0, 0, 43, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 11070, -1, 0, 364, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 11085, -1, 0, 111, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 11087, -1, 0, 311, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 11155, 113392, 1, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "max7_blend_cube_5000_binary", 10543, -1, 0, 495, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "max7_blend_cube_5000_binary", 10911, 2350, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "max7_blend_cube_5000_binary", 8673, -1, 0, 315, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_cube_5000_binary", 10499, -1, 0, 141, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max7_cube_5000_binary", 10500, -1, 0, 145, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max7_cube_5000_binary", 10909, -1, 0, 278, 0, 0, 0, "mesh" },
	{ "max7_cube_5000_binary", 10920, 2383, 23, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "max7_cube_5000_binary", 10952, 2383, 0, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "max7_cube_5000_binary", 10983, -1, 0, 0, 36, 0, 0, "set" },
	{ "max7_cube_5000_binary", 10987, 3130, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, uv_info, (ufbx_vert..." },
	{ "max7_cube_5000_binary", 10995, 2856, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MaterialAssignation, \"C\",..." },
	{ "max7_cube_5000_binary", 11060, 324, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"s\", &type_and_name)" },
	{ "max7_cube_5000_binary", 11069, -1, 0, 138, 0, 0, 0, "elem_node" },
	{ "max7_cube_5000_binary", 11078, -1, 0, 139, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 11091, 2383, 23, 0, 0, 0, 0, "ufbxi_read_legacy_mesh(uc, node, &attrib_info)" },
	{ "max7_cube_5000_binary", 11100, -1, 0, 280, 0, 0, 0, "entry" },
	{ "max7_cube_5000_binary", 11111, -1, 0, 140, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 11124, 942, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc..." },
	{ "max7_cube_5000_binary", 11135, -1, 0, 4, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max7_cube_5000_binary", 11142, -1, 0, 6, 0, 0, 0, "root" },
	{ "max7_cube_5000_binary", 11144, -1, 0, 14, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max7_cube_5000_binary", 11159, 324, 0, 0, 0, 0, 0, "ufbxi_read_legacy_model(uc, node)" },
	{ "max7_cube_5000_binary", 11173, -1, 0, 0, 108, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "max7_cube_5000_binary", 5918, -1, 0, 0, 26, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max7_cube_5000_binary", 8231, -1, 0, 13, 0, 0, 0, "entry" },
	{ "max7_cube_5000_binary", 8271, -1, 0, 145, 0, 0, 0, "conn" },
	{ "max7_skin_5000_binary", 10244, -1, 0, 1280, 0, 0, 0, "curve" },
	{ "max7_skin_5000_binary", 10246, -1, 0, 1282, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max7_skin_5000_binary", 10537, -1, 0, 1267, 0, 0, 0, "stack" },
	{ "max7_skin_5000_binary", 10541, -1, 0, 1269, 0, 0, 0, "layer" },
	{ "max7_skin_5000_binary", 10823, -1, 0, 342, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 10830, 2420, 136, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "max7_skin_5000_binary", 10841, 4378, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "max7_skin_5000_binary", 10842, 4544, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "max7_skin_5000_binary", 10884, -1, 0, 491, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 10896, -1, 0, 0, 51, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 11033, 2361, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max7_skin_5000_binary", 11035, 2420, 136, 0, 0, 0, 0, "ufbxi_read_legacy_link(uc, child, &fbx_id, name.data)" },
	{ "max7_skin_5000_binary", 11038, -1, 0, 345, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 11041, -1, 0, 346, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 11042, -1, 0, 348, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 11044, -1, 0, 349, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 11089, -1, 0, 491, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max_curve_line_7500_ascii", 9252, 8302, 43, 0, 0, 0, 0, "points->size % 3 == 0" },
	{ "max_curve_line_7500_binary", 10086, 13861, 255, 0, 0, 0, 0, "ufbxi_read_line(uc, node, &info)" },
	{ "max_curve_line_7500_binary", 9250, 13861, 255, 0, 0, 0, 0, "points" },
	{ "max_curve_line_7500_binary", 9251, 13985, 56, 0, 0, 0, 0, "points_index" },
	{ "max_curve_line_7500_binary", 9271, -1, 0, 0, 140, 0, 0, "line->segments.data" },
	{ "max_quote_6100_ascii", 13480, -1, 0, 0, 174, 0, 0, "node->all_attribs.data" },
	{ "max_quote_6100_binary", 8980, 8983, 36, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "max_quote_6100_binary", 8983, 9030, 36, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_visibility.d..." },
	{ "max_selection_sets_6100_binary", 10139, -1, 0, 416, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "max_selection_sets_6100_binary", 9835, -1, 0, 416, 0, 0, 0, "sel" },
	{ "max_texture_mapping_6100_binary", 12771, -1, 0, 0, 659, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_texture_mapping_6100_binary", 12823, -1, 0, 0, 659, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 12909, -1, 0, 0, 658, 0, 0, "shader" },
	{ "max_texture_mapping_6100_binary", 12941, -1, 0, 0, 659, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_texture_mapping_6100_binary", 12953, -1, 0, 0, 676, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "max_texture_mapping_6100_binary", 13013, -1, 0, 0, 660, 0, 0, "shader->inputs.data" },
	{ "max_texture_mapping_6100_binary", 14186, -1, 0, 0, 658, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_texture_mapping_7700_binary", 12800, -1, 0, 0, 732, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_transformed_skin_6100_binary", 10302, 63310, 98, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max_transformed_skin_6100_binary", 10354, 64699, 7, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_arnold_textures_6100_binary", 10129, -1, 0, 0, 343, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 13986, -1, 0, 0, 392, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_arnold_textures_6100_binary", 9806, -1, 0, 0, 343, 0, 0, "bindings->prop_bindings.data" },
	{ "maya_blend_shape_cube_6100_binary", 8212, -1, 0, 376, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_blend_shape_cube_6100_binary", 8213, -1, 0, 377, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_blend_shape_cube_6100_binary", 8217, -1, 0, 378, 0, 0, 0, "elem" },
	{ "maya_blend_shape_cube_6100_binary", 8282, -1, 0, 383, 0, 0, 0, "conn" },
	{ "maya_blend_shape_cube_6100_binary", 8574, -1, 0, 384, 0, 0, 0, "shape" },
	{ "maya_blend_shape_cube_6100_binary", 8646, -1, 0, 376, 0, 0, 0, "deformer" },
	{ "maya_blend_shape_cube_6100_binary", 8647, -1, 0, 379, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 8652, -1, 0, 380, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_6100_binary", 8655, -1, 0, 382, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "maya_blend_shape_cube_6100_binary", 8671, -1, 0, 383, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "maya_blend_shape_cube_6100_binary", 8691, -1, 0, 387, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 8692, -1, 0, 388, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "maya_cache_sine_6100_binary", 13669, -1, 0, 0, 230, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_cache_sine_6100_binary", 13825, -1, 0, 0, 235, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 15574, -1, 0, 0, 245, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_cache_sine_6100_binary", 15579, -1, 0, 0, 248, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_cache_sine_6100_binary", 15624, -1, 0, 0, 249, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_cache_sine_6100_binary", 15657, -1, 0, 0, 245, 0, 0, "xml_ok" },
	{ "maya_cache_sine_6100_binary", 15665, -1, 0, 0, 251, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_cache_sine_6100_binary", 15681, -1, 0, 0, 245, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 15740, -1, 0, 0, 251, 0, 0, "ufbxi_cache_try_open_file(cc, filename, &found)" },
	{ "maya_cache_sine_6100_binary", 15877, -1, 0, 0, 253, 0, 0, "cc->cache.channels.data" },
	{ "maya_cache_sine_6100_binary", 15907, -1, 0, 0, 245, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, &found)" },
	{ "maya_cache_sine_6100_binary", 15914, -1, 0, 0, 251, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_cache_sine_6100_binary", 15919, -1, 0, 0, 252, 0, 0, "cc->cache.frames.data" },
	{ "maya_cache_sine_6100_binary", 15922, -1, 0, 0, 253, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_cache_sine_6100_binary", 15926, -1, 0, 0, 254, 0, 0, "cc->imp" },
	{ "maya_cache_sine_6100_binary", 16150, -1, 0, 0, 245, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_cache_sine_6100_binary", 16523, -1, 0, 0, 245, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_color_sets_6100_binary", 8908, -1, 0, 0, 77, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 8955, 9966, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 8960, 16081, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 8963, 15524, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cone_6100_binary", 8966, 15571, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease.data,..." },
	{ "maya_constraint_zoo_6100_binary", 14276, -1, 0, 0, 315, 0, 0, "constraint->targets.data" },
	{ "maya_cube_big_endian_6100_binary", 5720, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 6030, -1, 0, 6, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 7274, -1, 0, 4, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 8902, -1, 0, 411, 0, 0, 0, "bitangents" },
	{ "maya_cube_big_endian_6100_binary", 8903, -1, 0, 412, 0, 0, 0, "tangents" },
	{ "maya_cube_big_endian_7500_binary", 6021, -1, 0, 6, 0, 0, 0, "header_words" },
	{ "maya_display_layers_6100_binary", 14238, -1, 0, 0, 238, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_human_ik_7400_binary", 10068, -1, 0, 2577, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 10070, -1, 0, 1826, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 9329, -1, 0, 1826, 0, 0, 0, "marker" },
	{ "maya_interpolation_modes_6100_binary", 10268, 16936, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_interpolation_modes_6100_binary", 10335, 16936, 73, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_interpolation_modes_6100_binary", 10445, 16805, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "maya_interpolation_modes_6100_binary", 10516, 16706, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "maya_leading_comma_7500_ascii", 10001, -1, 0, 0, 182, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_leading_comma_7500_ascii", 10010, 8861, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_leading_comma_7500_ascii", 10043, -1, 0, 0, 168, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_leading_comma_7500_ascii", 10046, -1, 0, 0, 169, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_leading_comma_7500_ascii", 10078, 8926, 43, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", 10165, 13120, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_leading_comma_7500_ascii", 10594, 0, 60, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_leading_comma_7500_ascii", 10595, 100, 33, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "maya_leading_comma_7500_ascii", 10613, 1525, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_leading_comma_7500_ascii", 10614, 2615, 33, 0, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_leading_comma_7500_ascii", 10629, -1, 0, 149, 0, 0, 0, "root" },
	{ "maya_leading_comma_7500_ascii", 10631, -1, 0, 153, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_leading_comma_7500_ascii", 10635, 2808, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_leading_comma_7500_ascii", 10636, 3021, 33, 0, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_leading_comma_7500_ascii", 10639, 8762, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_leading_comma_7500_ascii", 10643, 0, 0, 0, 0, 0, 0, "uc->top_node" },
	{ "maya_leading_comma_7500_ascii", 10645, 8861, 33, 0, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_leading_comma_7500_ascii", 10648, 13016, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_leading_comma_7500_ascii", 10649, 13120, 33, 0, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_leading_comma_7500_ascii", 10661, -1, 0, 0, 182, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "maya_leading_comma_7500_ascii", 11342, -1, 0, 0, 187, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_leading_comma_7500_ascii", 11372, -1, 0, 0, 188, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_leading_comma_7500_ascii", 11649, -1, 0, 0, 199, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 11671, -1, 0, 0, 195, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 11745, -1, 0, 0, 198, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 12501, -1, 0, 0, 183, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_leading_comma_7500_ascii", 12510, -1, 0, 0, 184, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_leading_comma_7500_ascii", 13381, -1, 0, 0, 185, 0, 0, "uc->scene.elements.data" },
	{ "maya_leading_comma_7500_ascii", 13386, -1, 0, 0, 186, 0, 0, "element_data" },
	{ "maya_leading_comma_7500_ascii", 13401, -1, 0, 0, 187, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_leading_comma_7500_ascii", 13414, -1, 0, 0, 189, 0, 0, "typed_elems->data" },
	{ "maya_leading_comma_7500_ascii", 13426, -1, 0, 0, 194, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_leading_comma_7500_ascii", 13525, -1, 0, 0, 195, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_leading_comma_7500_ascii", 13701, -1, 0, 0, 196, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_leading_comma_7500_ascii", 13748, -1, 0, 0, 198, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_leading_comma_7500_ascii", 13856, -1, 0, 0, 199, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_leading_comma_7500_ascii", 13860, -1, 0, 0, 200, 0, 0, "stack->anim.layers.data" },
	{ "maya_leading_comma_7500_ascii", 13874, -1, 0, 0, 201, 0, 0, "layer_desc" },
	{ "maya_leading_comma_7500_ascii", 13950, -1, 0, 0, 202, 0, 0, "layer->anim_props.data" },
	{ "maya_leading_comma_7500_ascii", 14287, -1, 0, 0, 203, 0, 0, "descs" },
	{ "maya_leading_comma_7500_ascii", 16496, -1, 0, 1, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_leading_comma_7500_ascii", 16497, -1, 0, 4, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_leading_comma_7500_ascii", 16501, 0, 60, 0, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_leading_comma_7500_ascii", 16505, -1, 0, 0, 183, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_leading_comma_7500_ascii", 16506, -1, 0, 0, 185, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_leading_comma_7500_ascii", 16545, -1, 0, 0, 204, 0, 0, "imp" },
	{ "maya_leading_comma_7500_ascii", 1762, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 1798, -1, 0, 88, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 2469, -1, 0, 1, 0, 0, 0, "data" },
	{ "maya_leading_comma_7500_ascii", 3041, -1, 0, 0, 10, 0, 0, "dst" },
	{ "maya_leading_comma_7500_ascii", 3085, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_leading_comma_7500_ascii", 3106, -1, 0, 0, 52, 0, 0, "str" },
	{ "maya_leading_comma_7500_ascii", 4055, -1, 0, 0, 0, 0, 1, "result != UFBX_PROGRESS_CANCEL" },
	{ "maya_leading_comma_7500_ascii", 4074, -1, 0, 0, 0, 1, 0, "uc->read_fn" },
	{ "maya_leading_comma_7500_ascii", 4130, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_leading_comma_7500_ascii", 6456, -1, 0, 0, 0, 0, 57, "ufbxi_report_progress(uc)" },
	{ "maya_leading_comma_7500_ascii", 6580, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_leading_comma_7500_ascii", 6603, -1, 0, 0, 0, 9570, 0, "c != '\\0'" },
	{ "maya_leading_comma_7500_ascii", 6657, -1, 0, 4, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 6675, -1, 0, 8, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 6702, 288, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_leading_comma_7500_ascii", 6709, 3707, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_leading_comma_7500_ascii", 6764, 291, 0, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_leading_comma_7500_ascii", 6784, 288, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 6796, 2537, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 6802, 168, 0, 0, 0, 0, 0, "depth == 0" },
	{ "maya_leading_comma_7500_ascii", 6810, 0, 60, 0, 0, 0, 0, "Expected a 'Name:' token" },
	{ "maya_leading_comma_7500_ascii", 6812, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_leading_comma_7500_ascii", 6816, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_leading_comma_7500_ascii", 6821, -1, 0, 6, 0, 0, 0, "node" },
	{ "maya_leading_comma_7500_ascii", 6875, 291, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 6924, -1, 0, 0, 10, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_leading_comma_7500_ascii", 7019, 8927, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'I')" },
	{ "maya_leading_comma_7500_ascii", 7022, 8931, 11, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_leading_comma_7500_ascii", 7030, -1, 0, 0, 0, 9570, 0, "ufbxi_ascii_skip_until(uc, '}')" },
	{ "maya_leading_comma_7500_ascii", 7047, 8937, 33, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_leading_comma_7500_ascii", 7058, -1, 0, 0, 144, 0, 0, "arr_data" },
	{ "maya_leading_comma_7500_ascii", 7071, -1, 0, 10, 0, 0, 0, "node->vals" },
	{ "maya_leading_comma_7500_ascii", 7081, 168, 11, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_leading_comma_7500_ascii", 7088, -1, 0, 30, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 7260, -1, 0, 0, 0, 1, 0, "header" },
	{ "maya_leading_comma_7500_ascii", 7292, -1, 0, 4, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_leading_comma_7500_ascii", 7312, 100, 33, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_leading_comma_7500_ascii", 7341, 0, 60, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_leading_comma_7500_ascii", 7358, -1, 0, 7, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 7377, 1544, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_leading_comma_7500_ascii", 7385, -1, 0, 133, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 7408, 100, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_pars..." },
	{ "maya_leading_comma_7500_ascii", 7705, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_leading_comma_7500_ascii", 7711, -1, 0, 3, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 7860, -1, 0, 86, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 7884, -1, 0, 0, 42, 0, 0, "props->props.data" },
	{ "maya_leading_comma_7500_ascii", 7891, -1, 0, 86, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_leading_comma_7500_ascii", 7914, -1, 0, 86, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_leading_comma_7500_ascii", 7926, 100, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 7943, -1, 0, 86, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_leading_comma_7500_ascii", 8055, 2615, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 8074, 3021, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object)" },
	{ "maya_leading_comma_7500_ascii", 8081, -1, 0, 166, 0, 0, 0, "tmpl" },
	{ "maya_leading_comma_7500_ascii", 8082, 3061, 33, 0, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_leading_comma_7500_ascii", 8100, -1, 0, 0, 52, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_leading_comma_7500_ascii", 8103, -1, 0, 285, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_leading_comma_7500_ascii", 8109, -1, 0, 0, 142, 0, 0, "uc->templates" },
	{ "maya_leading_comma_7500_ascii", 8173, -1, 0, 0, 168, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_leading_comma_7500_ascii", 8184, -1, 0, 149, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_leading_comma_7500_ascii", 8185, -1, 0, 150, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_leading_comma_7500_ascii", 8189, -1, 0, 151, 0, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", 8199, -1, 0, 152, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 8375, 9370, 43, 0, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_leading_comma_7500_ascii", 8389, 9278, 78, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MappingInformationType, \"C..." },
	{ "maya_leading_comma_7500_ascii", 8440, 10556, 67, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_leading_comma_7500_ascii", 8475, 9303, 67, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_leading_comma_7500_ascii", 8485, 10999, 84, 0, 0, 0, 0, "arr" },
	{ "maya_leading_comma_7500_ascii", 8707, -1, 0, 0, 159, 0, 0, "mesh->faces.data" },
	{ "maya_leading_comma_7500_ascii", 8733, 9073, 43, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_leading_comma_7500_ascii", 8745, -1, 0, 0, 160, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_leading_comma_7500_ascii", 8818, 8926, 43, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_leading_comma_7500_ascii", 8854, -1, 0, 0, 158, 0, 0, "edges" },
	{ "maya_leading_comma_7500_ascii", 8887, 9073, 43, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "maya_leading_comma_7500_ascii", 8907, -1, 0, 0, 161, 0, 0, "mesh->uv_sets.data" },
	{ "maya_leading_comma_7500_ascii", 8917, 9278, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_leading_comma_7500_ascii", 8923, 9692, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_leading_comma_7500_ascii", 8931, 10114, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_leading_comma_7500_ascii", 8943, 10531, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_leading_comma_7500_ascii", 8970, 10925, 78, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_leading_comma_7500_ascii", 8973, 10999, 84, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing.da..." },
	{ "maya_leading_comma_7500_ascii", 8988, 11116, 78, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_leading_comma_7500_ascii", 8993, 11198, 78, 0, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_lod_group_6100_binary", 9989, -1, 0, 284, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_lod_group_7500_ascii", 10072, -1, 0, 491, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_6100_ascii", 6845, -1, 0, 447, 0, 0, 0, "arr" },
	{ "maya_node_attribute_zoo_6100_ascii", 6861, -1, 0, 448, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_node_attribute_zoo_6100_ascii", 6952, -1, 0, 463, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 6955, -1, 0, 490, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 6981, -1, 0, 449, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_binary", 10016, 157559, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 10051, 12128, 23, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, type_st..." },
	{ "maya_node_attribute_zoo_6100_binary", 10053, -1, 0, 282, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 10082, 138209, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 10084, 139478, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 10147, -1, 0, 0, 392, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 10251, 163331, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "maya_node_attribute_zoo_6100_binary", 10254, 163352, 1, 0, 0, 0, 0, "curve->keyframes.data" },
	{ "maya_node_attribute_zoo_6100_binary", 10374, 163388, 86, 0, 0, 0, 0, "Unknown key mode" },
	{ "maya_node_attribute_zoo_6100_binary", 10379, 163349, 3, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_node_attribute_zoo_6100_binary", 10428, 163349, 1, 0, 0, 0, 0, "data == data_end" },
	{ "maya_node_attribute_zoo_6100_binary", 10503, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "maya_node_attribute_zoo_6100_binary", 10525, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "maya_node_attribute_zoo_6100_binary", 10538, 163019, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"S\", &stack->name)" },
	{ "maya_node_attribute_zoo_6100_binary", 10548, 163046, 255, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_ReferenceTime, \"LL\", &beg..." },
	{ "maya_node_attribute_zoo_6100_binary", 10558, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 10568, 162983, 125, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_node_attribute_zoo_6100_binary", 10572, 163019, 0, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 10608, -1, 0, 43, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 10654, 158678, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_node_attribute_zoo_6100_binary", 10655, 162983, 125, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 10659, 162983, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_node_attribute_zoo_6100_binary", 12641, -1, 0, 0, 485, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 12684, -1, 0, 0, 504, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 13843, -1, 0, 0, 485, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 13848, -1, 0, 0, 494, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 13849, -1, 0, 0, 495, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 13871, -1, 0, 0, 500, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "maya_node_attribute_zoo_6100_binary", 14304, -1, 0, 0, 504, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_6100_binary", 5929, -1, 0, 0, 0, 12405, 0, "val" },
	{ "maya_node_attribute_zoo_6100_binary", 5932, -1, 0, 0, 0, 12158, 0, "val" },
	{ "maya_node_attribute_zoo_6100_binary", 6078, -1, 0, 453, 0, 0, 0, "arr" },
	{ "maya_node_attribute_zoo_6100_binary", 6238, 12130, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_node_attribute_zoo_6100_binary", 7679, -1, 0, 43, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_node_attribute_zoo_6100_binary", 8245, -1, 0, 282, 0, 0, 0, "elem_node" },
	{ "maya_node_attribute_zoo_6100_binary", 8254, -1, 0, 279, 0, 0, 0, "elem" },
	{ "maya_node_attribute_zoo_6100_binary", 8261, -1, 0, 281, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", 8797, -1, 0, 536, 0, 0, 0, "mesh" },
	{ "maya_node_attribute_zoo_6100_binary", 9179, 138209, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->basis..." },
	{ "maya_node_attribute_zoo_6100_binary", 9181, 138308, 255, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 9188, 138359, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 9189, 138416, 1, 0, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 9190, 143462, 27, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 9209, 139478, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 9210, 139592, 1, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"ZZ\", &dimens..." },
	{ "maya_node_attribute_zoo_6100_binary", 9211, 139631, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Step, \"II\", &step_u, &ste..." },
	{ "maya_node_attribute_zoo_6100_binary", 9212, 139664, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 9225, 139691, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 9226, 139727, 1, 0, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 9227, 140321, 3, 0, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 9228, 141818, 63, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 9229, 139655, 1, 0, 0, 0, 0, "points->size / 4 == (size_t)dimension_u * (size_t)dimen..." },
	{ "maya_node_attribute_zoo_6100_binary", 9314, -1, 0, 710, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 9916, -1, 0, 0, 39, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_node_attribute_zoo_6100_binary", 9931, -1, 0, 275, 0, 0, 0, "entry" },
	{ "maya_node_attribute_zoo_6100_binary", 9940, -1, 0, 276, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_6100_binary", 9950, -1, 0, 0, 40, 0, 0, "attrib_info.props.props.data" },
	{ "maya_node_attribute_zoo_6100_binary", 9955, 12128, 23, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_node_attribute_zoo_6100_binary", 9961, -1, 0, 710, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 9963, -1, 0, 279, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 9995, -1, 0, 281, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_node_attribute_zoo_7500_ascii", 10056, -1, 0, 727, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_ascii", 10058, -1, 0, 713, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_7500_ascii", 10060, -1, 0, 658, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_ascii", 10064, -1, 0, 778, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_ascii", 8877, -1, 0, 0, 0, 0, 28459, "index_ix >= 0 && (size_t)index_ix < mesh->num_indices" },
	{ "maya_node_attribute_zoo_7500_ascii", 9174, -1, 0, 897, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_7500_binary", 10062, -1, 0, 493, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 10123, 61038, 255, 0, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", 11496, -1, 0, 0, 359, 0, 0, "elem->props.props.data" },
	{ "maya_node_attribute_zoo_7500_binary", 13402, -1, 0, 0, 359, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 5971, 61146, 109, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 5972, 61333, 103, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 6215, -1, 0, 0, 0, 0, 2909, "res != -28" },
	{ "maya_node_attribute_zoo_7500_binary", 8172, -1, 0, 0, 327, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_node_attribute_zoo_7500_binary", 9450, 61038, 255, 0, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 9451, 61115, 255, 0, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 9452, 61175, 255, 0, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 9453, 61234, 255, 0, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 9454, 61292, 255, 0, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_binary", 9457, 61122, 0, 0, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_binary", 9462, 61242, 0, 0, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_binary", 9463, 61300, 0, 0, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", 9467, -1, 0, 0, 328, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 9618, 61431, 0, 0, 0, 0, 0, "refs_left >= 0" },
	{ "maya_polygon_hole_6100_binary", 9016, 9377, 37, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_polygon_hole_6100_binary", 9018, 9342, 0, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_hole.data, &..." },
	{ "maya_resampled_7500_binary", 9491, 24917, 23, 0, 0, 0, 0, "p_ref < p_ref_end" },
	{ "maya_scale_no_inherit_6100_ascii", 10331, 19165, 114, 0, 0, 0, 0, "Unknown slope mode" },
	{ "maya_scale_no_inherit_6100_ascii", 10361, 19171, 111, 0, 0, 0, 0, "Unknown weight mode" },
	{ "maya_shaderfx_pbs_material_7700_ascii", 7845, -1, 0, 0, 321, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "maya_slime_7500_binary", 9245, -1, 0, 860, 0, 0, 0, "line" },
	{ "maya_texture_layers_6100_binary", 11810, -1, 0, 0, 266, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 13238, -1, 0, 0, 271, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 14193, -1, 0, 0, 266, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_transform_animation_6100_binary", 10370, 17549, 11, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_zero_end_7400_binary", 10029, 12333, 255, 0, 0, 0, 0, "(info.fbx_id & (0x8000000000000000ULL)) == 0" },
	{ "maya_zero_end_7400_binary", 1761, 12382, 255, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_zero_end_7400_binary", 1797, 16748, 1, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_zero_end_7400_binary", 4209, 36, 255, 0, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_zero_end_7400_binary", 4239, -1, 0, 0, 0, 12392, 0, "uc->read_fn" },
	{ "maya_zero_end_7400_binary", 5969, 16744, 106, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 5970, 12615, 106, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 5973, 12379, 101, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 5995, 12382, 255, 0, 0, 0, 0, "data" },
	{ "maya_zero_end_7400_binary", 6017, -1, 0, 0, 0, 27, 0, "header" },
	{ "maya_zero_end_7400_binary", 6038, 24, 29, 0, 0, 0, 0, "num_values64 <= 0xffffffffui32" },
	{ "maya_zero_end_7400_binary", 6056, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_zero_end_7400_binary", 6060, -1, 0, 0, 0, 40, 0, "name" },
	{ "maya_zero_end_7400_binary", 6062, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_zero_end_7400_binary", 6087, -1, 0, 0, 0, 12379, 0, "data" },
	{ "maya_zero_end_7400_binary", 6122, 12382, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_zero_end_7400_binary", 6129, 16748, 1, 0, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_zero_end_7400_binary", 6142, 12379, 99, 0, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_zero_end_7400_binary", 6158, -1, 0, 0, 0, 12392, 0, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_zero_end_7400_binary", 6216, 12384, 1, 0, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_zero_end_7400_binary", 6219, 12384, 255, 0, 0, 0, 0, "Bad array encoding" },
	{ "maya_zero_end_7400_binary", 6239, 12379, 101, 0, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_zero_end_7400_binary", 6255, -1, 0, 8, 0, 0, 0, "vals" },
	{ "maya_zero_end_7400_binary", 6263, -1, 0, 0, 0, 87, 0, "data" },
	{ "maya_zero_end_7400_binary", 6316, 331, 0, 0, 0, 0, 0, "str" },
	{ "maya_zero_end_7400_binary", 6326, -1, 0, 0, 11, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_zero_end_7400_binary", 6341, 593, 8, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_zero_end_7400_binary", 6346, 22, 1, 0, 0, 0, 0, "Bad value type" },
	{ "maya_zero_end_7400_binary", 6357, 66, 4, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_zero_end_7400_binary", 6359, 36, 255, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_zero_end_7400_binary", 6371, 58, 93, 0, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_zero_end_7400_binary", 6376, 70, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_zero_end_7400_binary", 6385, -1, 0, 30, 0, 0, 0, "node->children" },
	{ "maya_zero_end_7400_binary", 7314, 35, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_zero_end_7400_binary", 7343, 22, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_zero_end_7400_binary", 7795, 797, 0, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_zero_end_7400_binary", 7798, 6091, 0, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_zero_end_7400_binary", 7887, 797, 0, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_zero_end_7400_binary", 8088, 4105, 0, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_zero_end_7400_binary", 9023, 12861, 0, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "maya_zero_end_7500_binary", 11148, 24, 0, 0, 0, 0, 0, "ufbxi_parse_legacy_toplevel(uc)" },
	{ "maya_zero_end_7500_binary", 16499, 24, 0, 0, 0, 0, 0, "ufbxi_read_legacy_root(uc)" },
	{ "maya_zero_end_7500_binary", 7441, 24, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "revit_empty_7400_binary", 5903, 25199, 2, 0, 0, 0, 0, "type == 'S' || type == 'R'" },
	{ "revit_empty_7400_binary", 5912, 25220, 255, 0, 0, 0, 0, "d->data" },
	{ "revit_empty_7400_binary", 5916, -1, 0, 0, 305, 0, 0, "d->data" },
	{ "revit_empty_7400_binary", 8326, -1, 0, 0, 262, 0, 0, "new_indices" },
	{ "revit_empty_7400_binary", 8409, -1, 0, 0, 262, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "revit_empty_7400_binary", 8990, 21004, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material.dat..." },
	{ "synthetic_binary_props_7500_ascii", 6892, -1, 0, 60, 0, 0, 0, "v" },
	{ "synthetic_binary_props_7500_ascii", 6898, -1, 0, 104, 0, 0, 0, "v->data" },
	{ "synthetic_broken_filename_7500_ascii", 11723, -1, 0, 0, 254, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 12614, -1, 0, 0, 255, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 13251, -1, 0, 0, 258, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 13370, -1, 0, 0, 255, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 14029, -1, 0, 0, 254, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 14146, -1, 0, 0, 255, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 14147, -1, 0, 0, 256, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 14307, -1, 0, 0, 258, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_cube_nan_6100_ascii", 6680, 4866, 45, 0, 0, 0, 0, "token->type == 'F'" },
	{ "synthetic_empty_elements_7500_ascii", 11557, 2800, 49, 0, 0, 0, 0, "depth <= num_nodes" },
	{ "synthetic_id_collision_7500_ascii", 6951, -1, 0, 680, 0, 0, 0, "v" },
	{ "synthetic_id_collision_7500_ascii", 8246, -1, 0, 827, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "synthetic_indexed_by_vertex_7500_ascii", 8415, -1, 0, 0, 159, 0, 0, "new_index_data" },
	{ "synthetic_missing_version_6100_ascii", 10456, 72840, 102, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "synthetic_missing_version_6100_ascii", 13494, -1, 0, 0, 254, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 9957, -1, 0, 864, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 9959, -1, 0, 637, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 9983, -1, 0, 255, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_parent_directory_7700_ascii", 13353, -1, 0, 0, 260, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 13367, -1, 0, 0, 260, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_parent_directory_7700_ascii", 14188, -1, 0, 0, 262, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_parent_directory_7700_ascii", 14189, -1, 0, 0, 263, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->r..." },
	{ "synthetic_string_collision_7500_ascii", 10109, -1, 0, 140066, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "synthetic_string_collision_7500_ascii", 10117, -1, 0, 140081, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "synthetic_string_collision_7500_ascii", 10119, -1, 0, 140087, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "synthetic_string_collision_7500_ascii", 11338, -1, 0, 140102, 0, 0, 0, "tmp_connections" },
	{ "synthetic_string_collision_7500_ascii", 11528, -1, 0, 140105, 0, 0, 0, "node_offsets" },
	{ "synthetic_string_collision_7500_ascii", 11640, -1, 0, 140114, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_string_collision_7500_ascii", 13403, -1, 0, 140105, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "synthetic_string_collision_7500_ascii", 13409, -1, 0, 140108, 0, 0, 0, "typed_offsets" },
	{ "synthetic_string_collision_7500_ascii", 3055, -1, 0, 2253, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_string_collision_7500_ascii", 9634, -1, 0, 140066, 0, 0, 0, "material" },
	{ "synthetic_texture_split_7500_ascii", 6765, -1, 0, 927, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "synthetic_texture_split_7500_ascii", 6959, 14287, 45, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_texture_split_7500_ascii", 6985, 28571, 35, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_texture_split_7500_ascii", 9701, -1, 0, 851, 0, 0, 0, "video" },
	{ "synthetic_texture_split_7500_binary", 5901, -1, 0, 0, 0, 26628, 0, "val" },
	{ "synthetic_texture_split_7500_binary", 7761, -1, 0, 0, 229, 0, 0, "dst" },
	{ "synthetic_unicode_7500_binary", 11514, -1, 0, 14372, 0, 0, 0, "node_ids" },
	{ "synthetic_unicode_7500_binary", 11517, -1, 0, 14373, 0, 0, 0, "node_ptrs" },
	{ "synthetic_unicode_7500_binary", 11573, -1, 0, 14375, 0, 0, 0, "p_offset" },
	{ "synthetic_unicode_7500_binary", 13390, -1, 0, 14371, 0, 0, 0, "element_offsets" },
	{ "synthetic_unicode_7500_binary", 2917, -1, 0, 14, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 3013, -1, 0, 1337, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_unicode_7500_binary", 3024, -1, 0, 14, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "zbrush_d20_6100_binary", 11767, -1, 0, 0, 261, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 11787, -1, 0, 0, 252, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 13646, -1, 0, 0, 251, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 13681, -1, 0, 0, 252, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 13824, -1, 0, 0, 260, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 13826, -1, 0, 0, 261, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "zbrush_d20_6100_binary", 14044, -1, 0, 0, 266, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 14108, -1, 0, 0, 267, 0, 0, "texs" },
	{ "zbrush_d20_6100_binary", 8582, 25242, 2, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "zbrush_d20_6100_binary", 8583, 25217, 0, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "zbrush_d20_6100_binary", 8596, 25290, 2, 0, 0, 0, 0, "normals && normals->size == vertices->size" },
	{ "zbrush_d20_6100_binary", 8642, 25189, 0, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "zbrush_d20_6100_binary", 8659, -1, 0, 0, 99, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 8689, 25217, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "zbrush_d20_6100_binary", 8801, 25189, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "zbrush_d20_6100_binary", 9009, 8305, 32, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "zbrush_d20_6100_binary", 9011, 8394, 33, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_group.data, ..." },
	{ "zbrush_d20_7500_ascii", 10115, -1, 0, 0, 252, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "zbrush_d20_7500_ascii", 7779, -1, 0, 0, 252, 0, 0, "dst_blob->data" },
	{ "zbrush_d20_7500_ascii", 9718, -1, 0, 0, 252, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "zbrush_d20_7500_binary", 10080, 32981, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 14243, -1, 0, 0, 403, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
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

void ufbxt_do_fuzz(ufbx_scene *scene, ufbx_scene *streamed_scene, size_t progress_calls, const char *base_name, void *data, size_t size, const char *filename)
{
	if (g_fuzz) {
		size_t fail_step = 0;
		int i;

		g_fuzz_test_name = base_name;

		size_t temp_allocs = 1000;
		size_t result_allocs = 500;
		if (streamed_scene) {
			temp_allocs = streamed_scene->metadata.temp_allocs + 10;
			result_allocs = streamed_scene->metadata.result_allocs + 10;
		}

		#pragma omp parallel for schedule(static, 16)
		for (i = 0; i < (int)temp_allocs; i++) {
			if (ufbxt_fuzz_should_skip(i)) continue;
			if (omp_get_thread_num() == 0) {
				if (i % 16 == 0) {
					fprintf(stderr, "\rFuzzing temp limit %s: %d/%d", base_name, i, (int)temp_allocs);
					fflush(stderr);
				}
			}

			size_t step = 10000000 + (size_t)i;

			if (!ufbxt_test_fuzz(filename, data, size, step, -1, (size_t)i, 0, 0, 0)) fail_step = step;
		}

		fprintf(stderr, "\rFuzzing temp limit %s: %d/%d\n", base_name, (int)temp_allocs, (int)temp_allocs);

		#pragma omp parallel for schedule(static, 16)
		for (i = 0; i < (int)result_allocs; i++) {
			if (ufbxt_fuzz_should_skip(i)) continue;
			if (omp_get_thread_num() == 0) {
				if (i % 16 == 0) {
					fprintf(stderr, "\rFuzzing result limit %s: %d/%d", base_name, i, (int)result_allocs);
					fflush(stderr);
				}
			}

			size_t step = 20000000 + (size_t)i;

			if (!ufbxt_test_fuzz(filename, data, size, step, -1, 0, (size_t)i, 0, 0)) fail_step = step;
		}

		fprintf(stderr, "\rFuzzing result limit %s: %d/%d\n", base_name, (int)result_allocs, (int)result_allocs);

		if (!g_fuzz_no_truncate) {
			#pragma omp parallel for schedule(static, 16)
			for (i = 1; i < (int)size; i++) {
				if (ufbxt_fuzz_should_skip(i)) continue;
				if (omp_get_thread_num() == 0) {
					if (i % 16 == 0) {
						fprintf(stderr, "\rFuzzing truncate %s: %d/%d", base_name, i, (int)size);
						fflush(stderr);
					}
				}

				size_t step = 30000000 + (size_t)i;

				if (!ufbxt_test_fuzz(filename, data, size, step, -1, 0, 0, (size_t)i, 0)) fail_step = step;
			}

			fprintf(stderr, "\rFuzzing truncate %s: %d/%d\n", base_name, (int)size, (int)size);
		}

		if (!g_fuzz_no_cancel) {
			#pragma omp parallel for schedule(static, 16)
			for (i = 0; i < (int)progress_calls; i++) {
				if (ufbxt_fuzz_should_skip(i)) continue;
				if (omp_get_thread_num() == 0) {
					if (i % 16 == 0) {
						fprintf(stderr, "\rFuzzing cancel %s: %d/%d", base_name, i, (int)size);
						fflush(stderr);
					}
				}

				size_t step = 40000000 + (size_t)i;

				if (!ufbxt_test_fuzz(filename, data, size, step, -1, 0, 0, 0, (size_t)i+1)) fail_step = step;
			}

			fprintf(stderr, "\rFuzzing cancel %s: %d/%d\n", base_name, (int)size, (int)size);
		}

		if (!g_fuzz_no_patch) {

			uint8_t *data_copy[256] = { 0 };

			int patch_start = g_patch_start - omp_get_num_threads() * 16;
			if (patch_start < 0) {
				patch_start = 0;
			}

			#pragma omp parallel for schedule(static, 16)
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
						if (!ufbxt_test_fuzz(filename, data_u8, size, step + v, i, 0, 0, 0, 0)) fail_step = step + v;
					}
				} else {
					data_u8[i] = original + 1;
					if (!ufbxt_test_fuzz(filename, data_u8, size, step + 1, i, 0, 0, 0, 0)) fail_step = step + 1;

					data_u8[i] = original - 1;
					if (!ufbxt_test_fuzz(filename, data_u8, size, step + 2, i, 0, 0, 0, 0)) fail_step = step + 2;

					if (original != 0) {
						data_u8[i] = 0;
						if (!ufbxt_test_fuzz(filename, data_u8, size, step + 3, i, 0, 0, 0, 0)) fail_step = step + 3;
					}

					if (original != 0xff) {
						data_u8[i] = 0xff;
						if (!ufbxt_test_fuzz(filename, data_u8, size, step + 4, i, 0, 0, 0, 0)) fail_step = step + 4;
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

const uint32_t ufbxt_file_versions[] = { 3000, 5000, 5800, 6100, 7100, 7400, 7500, 7700 };

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
		if (ufbx_open_file(NULL, &stream, buffer, SIZE_MAX)) {
			if (stream.close_fn) {
				stream.close_fn(stream.user);
			}
			iter->num_found++;
			return true;
		}
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

typedef enum ufbxt_file_test_flags {
	// Alternative test for a given file, does not execute fuzz tests again.
	UFBXT_FILE_TEST_FLAG_ALTERNATIVE = 0x1,

	// Allow scene loading to fail.
	// Calls test function with `scene == NULL && load_error != NULL` on failure.
	UFBXT_FILE_TEST_FLAG_ALLOW_ERROR = 0x2,

	// Allow invalid Unicode in the file.
	UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE = 0x4,
} ufbxt_file_test_flags;

void ufbxt_do_file_test(const char *name, void (*test_fn)(ufbx_scene *s, ufbxt_diff_error *err, ufbx_error *load_error), const char *suffix, ufbx_load_opts user_opts, ufbxt_file_test_flags flags)
{
	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s.obj", data_root, name);
	size_t obj_size = 0;
	void *obj_data = ufbxt_read_file(buf, &obj_size);
	ufbxt_obj_file *obj_file = obj_data ? ufbxt_load_obj(obj_data, obj_size, NULL) : NULL;
	free(obj_data);

	if (obj_file) {
		ufbxt_logf("%s [diff target found]", buf);
	}

	char base_name[512];

	ufbxt_begin_fuzz();

	uint32_t num_opened = 0;

	bool allow_error = (flags & UFBXT_FILE_TEST_FLAG_ALLOW_ERROR) != 0;
	bool alternative = (flags & UFBXT_FILE_TEST_FLAG_ALTERNATIVE) != 0;

	for (uint32_t vi = 0; vi < ufbxt_arraycount(ufbxt_file_versions); vi++) {
		for (uint32_t fi = 0; fi < 2; fi++) {
			uint32_t version = ufbxt_file_versions[vi];
			const char *format = fi == 1 ? "ascii" : "binary";
			if (suffix) {
				snprintf(buf, sizeof(buf), "%s%s_%u_%s_%s.fbx", data_root, name, version, format, suffix);
				snprintf(base_name, sizeof(base_name), "%s_%u_%s_%s", name, version, format, suffix);
			} else {
				snprintf(buf, sizeof(buf), "%s%s_%u_%s.fbx", data_root, name, version, format);
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
			} else if (!allow_error) {
				ufbxt_log_error(&error);
				ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse streamed file");
			}

			// Try a couple of read buffer sizes
			if (g_fuzz && !g_fuzz_no_buffer && g_fuzz_step == SIZE_MAX) {
				ufbxt_begin_fuzz();

				int fail_sz = -1;

				int buf_sz = 0;
				#pragma omp parallel for schedule(static, 16)
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

			if (scene) {
				ufbxt_logf(".. Loaded in %.2fms: File %.1fkB, temp %.1fkB (%zu allocs), result %.1fkB (%zu allocs)",
					cputime_cpu_delta_to_sec(NULL, load_end - load_begin) * 1e3,
					(double)size * 1e-3,
					(double)scene->metadata.temp_memory_used * 1e-3,
					scene->metadata.temp_allocs,
					(double)scene->metadata.result_memory_used * 1e-3,
					scene->metadata.result_allocs
				);

				ufbxt_assert(scene->metadata.ascii == ((fi == 1) ? 1 : 0));
				ufbxt_assert(scene->metadata.version == version);

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

			if (!alternative && scene) {
				ufbxt_do_fuzz(scene, streamed_scene, (size_t)stream_progress_ctx.calls, base_name, data, size, buf);

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

	free(obj_file);
}

#define UFBXT_IMPL 1
#define UFBXT_TEST(name) void ufbxt_test_fn_##name(void)
#define UFBXT_FILE_TEST_FLAGS(name, flags) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, user_opts, flags); } \
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

#define UFBXT_FILE_TEST(name) UFBXT_FILE_TEST_FLAGS(name, 0)
#define UFBXT_FILE_TEST_OPTS(name, get_opts) UFBXT_FILE_TEST_OPTS_FLAGS(name, get_opts, 0)
#define UFBXT_FILE_TEST_SUFFIX(name, suffix) UFBXT_FILE_TEST_SUFFIX_FLAGS(name, suffix, 0)
#define UFBXT_FILE_TEST_SUFFIX_OPTS(name, suffix, get_opts) UFBXT_FILE_TEST_SUFFIX_OPTS_FLAGS(name, suffix, get_opts, 0)
#define UFBXT_FILE_TEST_ALT(name, file) UFBXT_FILE_TEST_ALT_FLAGS(name, file, 0)
#define UFBXT_FILE_TEST_OPTS_ALT(name, file, get_opts) UFBXT_FILE_TEST_OPTS_ALT_FLAGS(name, file, get_opts, 0)

#include "all_tests.h"

#undef UFBXT_IMPL
#undef UFBXT_TEST
#undef UFBXT_FILE_TEST_FLAGS
#undef UFBXT_FILE_TEST_OPTS_FLAGS
#undef UFBXT_FILE_TEST_SUFFIX_FLAGS
#undef UFBXT_FILE_TEST_SUFFIX_OPTS_FLAGS
#undef UFBXT_FILE_TEST_ALT_FLAGS
#undef UFBXT_FILE_TEST_OPTS_ALT_FLAGS
#define UFBXT_IMPL 0
#define UFBXT_TEST(name) { #name, &ufbxt_test_fn_##name },
#define UFBXT_FILE_TEST_FLAGS(name, flags) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS_FLAGS(name, get_opts, flags) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_SUFFIX_FLAGS(name, suffix, flags) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_SUFFIX_OPTS_FLAGS(name, suffix, get_opts, flags) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_ALT_FLAGS(name, file, flags) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS_ALT_FLAGS(name, file, get_opts, flags) { #name, &ufbxt_test_fn_file_##name },
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

		if (!strcmp(argv[i], "--threads")) {
			#if _OPENMP
			if (++i < argc) omp_set_num_threads(atoi(argv[i]));
			#endif
		}

		if (!strcmp(argv[i], "--fuzz-step")) {
			if (++i < argc) g_fuzz_step = (size_t)atoi(argv[i]);
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

