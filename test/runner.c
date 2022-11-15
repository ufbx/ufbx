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
// From commit bcdb840
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "blender_279_ball_7400_binary", 10667, 12516, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_smoothing.da..." },
	{ "blender_279_ball_7400_binary", 16664, -1, 0, 0, 181, 0, 0, "mat->face_indices.data" },
	{ "blender_279_ball_7400_binary", 17108, -1, 0, 0, 181, 0, 0, "ufbxi_finalize_mesh_material(&uc->result, &uc->error, m..." },
	{ "blender_279_edge_vertex_6100_ascii", 8254, -1, 0, 424, 0, 0, 0, "v" },
	{ "blender_279_edge_vertex_6100_ascii", 8254, -1, 0, 429, 0, 0, 0, "v" },
	{ "blender_279_sausage_7400_binary", 11032, -1, 0, 706, 0, 0, 0, "skin" },
	{ "blender_279_sausage_7400_binary", 11032, -1, 0, 709, 0, 0, 0, "skin" },
	{ "blender_279_sausage_7400_binary", 11064, -1, 0, 728, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_7400_binary", 11064, -1, 0, 732, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_7400_binary", 11070, 23076, 0, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "blender_279_sausage_7400_binary", 11081, 23900, 0, 0, 0, 0, 0, "transform->size >= 16" },
	{ "blender_279_sausage_7400_binary", 11082, 24063, 0, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "blender_279_sausage_7400_binary", 11138, -1, 0, 856, 0, 0, 0, "curve" },
	{ "blender_279_sausage_7400_binary", 11138, -1, 0, 860, 0, 0, 0, "curve" },
	{ "blender_279_sausage_7400_binary", 11419, -1, 0, 691, 0, 0, 0, "pose" },
	{ "blender_279_sausage_7400_binary", 11419, -1, 0, 694, 0, 0, 0, "pose" },
	{ "blender_279_sausage_7400_binary", 11440, 21748, 0, 0, 0, 0, 0, "matrix->size >= 16" },
	{ "blender_279_sausage_7400_binary", 11443, -1, 0, 693, 0, 0, 0, "tmp_pose" },
	{ "blender_279_sausage_7400_binary", 11443, -1, 0, 696, 0, 0, 0, "tmp_pose" },
	{ "blender_279_sausage_7400_binary", 11453, -1, 0, 698, 0, 0, 0, "pose->bone_poses.data" },
	{ "blender_279_sausage_7400_binary", 11453, -1, 0, 701, 0, 0, 0, "pose->bone_poses.data" },
	{ "blender_279_sausage_7400_binary", 11782, -1, 0, 706, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 11782, -1, 0, 709, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 11784, 23076, 0, 0, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "blender_279_sausage_7400_binary", 11807, -1, 0, 833, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "blender_279_sausage_7400_binary", 11807, -1, 0, 837, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "blender_279_sausage_7400_binary", 11811, 21748, 0, 0, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "blender_279_sausage_7400_binary", 16845, -1, 0, 0, 381, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_7400_binary", 16888, -1, 0, 0, 382, 0, 0, "skin->vertices.data" },
	{ "blender_279_sausage_7400_binary", 16892, -1, 0, 0, 383, 0, 0, "skin->weights.data" },
	{ "blender_279_sausage_7400_binary", 17118, -1, 0, 0, 386, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "blender_279_unicode_6100_ascii", 12285, 432, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "blender_279_uv_sets_6100_ascii", 10731, -1, 0, 0, 63, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 10737, -1, 0, 726, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 10737, -1, 0, 731, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 10824, -1, 0, 727, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 10824, -1, 0, 732, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 10827, -1, 0, 729, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 10827, -1, 0, 734, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 6127, -1, 0, 727, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 6127, -1, 0, 732, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 6131, -1, 0, 728, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 6131, -1, 0, 733, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_293_barbarian_7400_binary", 11094, -1, 0, 1003, 0, 0, 0, "channel" },
	{ "blender_293_barbarian_7400_binary", 11094, -1, 0, 1009, 0, 0, 0, "channel" },
	{ "blender_293_barbarian_7400_binary", 11102, -1, 0, 1005, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "blender_293_barbarian_7400_binary", 11102, -1, 0, 1011, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "blender_293_barbarian_7400_binary", 11786, -1, 0, 991, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "blender_293_barbarian_7400_binary", 11786, -1, 0, 997, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "blender_293_barbarian_7400_binary", 11788, -1, 0, 1003, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "blender_293_barbarian_7400_binary", 11788, -1, 0, 1009, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "fuzz_0000", 11908, -1, 0, 480, 0, 0, 0, "conn" },
	{ "fuzz_0000", 11908, -1, 0, 483, 0, 0, 0, "conn" },
	{ "fuzz_0001", 17190, -1, 0, 714, 0, 0, 0, "aprop" },
	{ "fuzz_0001", 17190, -1, 0, 721, 0, 0, 0, "aprop" },
	{ "fuzz_0001", 17241, -1, 0, 719, 0, 0, 0, "aprop" },
	{ "fuzz_0001", 17241, -1, 0, 726, 0, 0, 0, "aprop" },
	{ "fuzz_0001", 8284, -1, 0, 522, 0, 0, 0, "v" },
	{ "fuzz_0001", 8284, -1, 0, 529, 0, 0, 0, "v" },
	{ "fuzz_0002", 14851, -1, 0, 790, 0, 0, 0, "((ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "fuzz_0002", 14851, -1, 0, 797, 0, 0, 0, "((ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "fuzz_0003", 14567, -1, 0, 719, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "fuzz_0003", 14567, -1, 0, 722, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "fuzz_0003", 14593, -1, 0, 720, 0, 0, 0, "new_prop" },
	{ "fuzz_0003", 14593, -1, 0, 723, 0, 0, 0, "new_prop" },
	{ "fuzz_0003", 14608, -1, 0, 722, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "fuzz_0003", 14608, -1, 0, 725, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "fuzz_0018", 12843, 810, 0, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "fuzz_0070", 3890, -1, 0, 32, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0070", 3890, -1, 0, 33, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0272", 11839, -1, 0, 449, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0272", 11839, -1, 0, 452, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0272", 3879, -1, 0, 451, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0272", 3879, -1, 0, 454, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "fuzz_0272", 4020, -1, 0, 451, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "fuzz_0272", 4020, -1, 0, 454, 0, 0, 0, "ufbxi_sanitize_string(pool, &sanitized, str, length, va..." },
	{ "fuzz_0272", 9964, -1, 0, 449, 0, 0, 0, "unknown" },
	{ "fuzz_0272", 9964, -1, 0, 452, 0, 0, 0, "unknown" },
	{ "fuzz_0272", 9973, -1, 0, 451, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "fuzz_0272", 9973, -1, 0, 454, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &unknown-..." },
	{ "fuzz_0393", 10148, -1, 0, 0, 137, 0, 0, "index_data" },
	{ "fuzz_0491", 14330, -1, 0, 26, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 14330, -1, 0, 27, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 14350, -1, 0, 23, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 14350, -1, 0, 24, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 14683, -1, 0, 23, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "fuzz_0491", 14683, -1, 0, 24, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "fuzz_0491", 16744, -1, 0, 26, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "fuzz_0491", 16744, -1, 0, 27, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "fuzz_0561", 11778, -1, 0, 450, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0561", 11778, -1, 0, 453, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "marvelous_quad_7200_binary", 18938, -1, 0, 0, 272, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "max2009_blob_5800_ascii", 8172, -1, 0, 0, 118, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v, raw)" },
	{ "max2009_blob_5800_ascii", 9035, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "max2009_blob_5800_binary", 12498, -1, 0, 570, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 12498, -1, 0, 575, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 12506, -1, 0, 0, 142, 0, 0, "material->props.props.data" },
	{ "max2009_blob_5800_binary", 12547, -1, 0, 106, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 12547, -1, 0, 111, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 12554, -1, 0, 0, 44, 0, 0, "light->props.props.data" },
	{ "max2009_blob_5800_binary", 12562, -1, 0, 307, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 12562, -1, 0, 312, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 12569, -1, 0, 0, 96, 0, 0, "camera->props.props.data" },
	{ "max2009_blob_5800_binary", 12691, 56700, 78, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material.dat..." },
	{ "max2009_blob_5800_binary", 12720, 6207, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 12721, -1, 0, 0, 141, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 12722, -1, 0, 570, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 12722, -1, 0, 575, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 12723, -1, 0, 572, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 12723, -1, 0, 577, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 12755, -1, 0, 0, 43, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 12764, -1, 0, 361, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 12764, -1, 0, 366, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max2009_blob_5800_binary", 12779, -1, 0, 106, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 12779, -1, 0, 111, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 12781, -1, 0, 307, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 12781, -1, 0, 312, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 12845, 113392, 1, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "max2009_blob_5800_binary", 17063, -1, 0, 0, 412, 0, 0, "materials" },
	{ "max7_blend_cube_5000_binary", 10367, -1, 0, 312, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 10367, -1, 0, 317, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 12229, -1, 0, 499, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "max7_blend_cube_5000_binary", 12604, 2350, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "max7_cube_5000_binary", 12185, -1, 0, 137, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max7_cube_5000_binary", 12185, -1, 0, 142, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "max7_cube_5000_binary", 12186, -1, 0, 141, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max7_cube_5000_binary", 12186, -1, 0, 146, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "max7_cube_5000_binary", 12602, -1, 0, 275, 0, 0, 0, "mesh" },
	{ "max7_cube_5000_binary", 12602, -1, 0, 280, 0, 0, 0, "mesh" },
	{ "max7_cube_5000_binary", 12613, 2383, 23, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "max7_cube_5000_binary", 12646, 2383, 0, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "max7_cube_5000_binary", 12677, -1, 0, 0, 36, 0, 0, "set" },
	{ "max7_cube_5000_binary", 12681, 3130, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, uv_info, (ufbx_vert..." },
	{ "max7_cube_5000_binary", 12689, 2856, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MaterialAssignation, \"C\",..." },
	{ "max7_cube_5000_binary", 12754, 324, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"s\", &type_and_name)" },
	{ "max7_cube_5000_binary", 12763, -1, 0, 132, 0, 0, 0, "elem_node" },
	{ "max7_cube_5000_binary", 12763, -1, 0, 137, 0, 0, 0, "elem_node" },
	{ "max7_cube_5000_binary", 12767, -1, 0, 133, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "max7_cube_5000_binary", 12767, -1, 0, 138, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "max7_cube_5000_binary", 12772, -1, 0, 134, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 12772, -1, 0, 139, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 12785, 2383, 23, 0, 0, 0, 0, "ufbxi_read_legacy_mesh(uc, node, &attrib_info)" },
	{ "max7_cube_5000_binary", 12792, -1, 0, 277, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max7_cube_5000_binary", 12792, -1, 0, 282, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info.fbx_id, attrib_info.fbx_..." },
	{ "max7_cube_5000_binary", 12801, -1, 0, 135, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 12801, -1, 0, 140, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 12812, -1, 0, 136, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &uc->legacy_implicit_anim_l..." },
	{ "max7_cube_5000_binary", 12812, -1, 0, 141, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &uc->legacy_implicit_anim_l..." },
	{ "max7_cube_5000_binary", 12814, 942, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc..." },
	{ "max7_cube_5000_binary", 12825, -1, 0, 3, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max7_cube_5000_binary", 12825, -1, 0, 4, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max7_cube_5000_binary", 12832, -1, 0, 4, 0, 0, 0, "root" },
	{ "max7_cube_5000_binary", 12832, -1, 0, 5, 0, 0, 0, "root" },
	{ "max7_cube_5000_binary", 12834, -1, 0, 13, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max7_cube_5000_binary", 12834, -1, 0, 8, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "max7_cube_5000_binary", 12849, 324, 0, 0, 0, 0, 0, "ufbxi_read_legacy_model(uc, node)" },
	{ "max7_cube_5000_binary", 12863, -1, 0, 0, 108, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &layer_in..." },
	{ "max7_cube_5000_binary", 7022, -1, 0, 0, 26, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d, raw)" },
	{ "max7_cube_5000_binary", 9903, -1, 0, 12, 0, 0, 0, "ufbxi_insert_fbx_id(uc, fbx_id, elem->element_id)" },
	{ "max7_cube_5000_binary", 9903, -1, 0, 7, 0, 0, 0, "ufbxi_insert_fbx_id(uc, fbx_id, elem->element_id)" },
	{ "max7_cube_5000_binary", 9941, -1, 0, 141, 0, 0, 0, "conn" },
	{ "max7_cube_5000_binary", 9941, -1, 0, 146, 0, 0, 0, "conn" },
	{ "max7_skin_5000_binary", 11930, -1, 0, 1279, 0, 0, 0, "curve" },
	{ "max7_skin_5000_binary", 11930, -1, 0, 1287, 0, 0, 0, "curve" },
	{ "max7_skin_5000_binary", 11932, -1, 0, 1281, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max7_skin_5000_binary", 11932, -1, 0, 1288, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "max7_skin_5000_binary", 12223, -1, 0, 1265, 0, 0, 0, "stack" },
	{ "max7_skin_5000_binary", 12223, -1, 0, 1272, 0, 0, 0, "stack" },
	{ "max7_skin_5000_binary", 12227, -1, 0, 1267, 0, 0, 0, "layer" },
	{ "max7_skin_5000_binary", 12227, -1, 0, 1274, 0, 0, 0, "layer" },
	{ "max7_skin_5000_binary", 12229, -1, 0, 1270, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "max7_skin_5000_binary", 12516, -1, 0, 338, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 12516, -1, 0, 344, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 12523, 2420, 136, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "max7_skin_5000_binary", 12534, 4378, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "max7_skin_5000_binary", 12535, 4544, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "max7_skin_5000_binary", 12577, -1, 0, 488, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 12577, -1, 0, 494, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 12589, -1, 0, 0, 51, 0, 0, "bone->props.props.data" },
	{ "max7_skin_5000_binary", 12727, 2361, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max7_skin_5000_binary", 12729, 2420, 136, 0, 0, 0, 0, "ufbxi_read_legacy_link(uc, child, &fbx_id, name.data)" },
	{ "max7_skin_5000_binary", 12732, -1, 0, 341, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 12732, -1, 0, 347, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 12735, -1, 0, 342, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 12735, -1, 0, 348, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 12736, -1, 0, 344, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 12736, -1, 0, 350, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 12738, -1, 0, 345, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 12738, -1, 0, 351, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 12783, -1, 0, 488, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max7_skin_5000_binary", 12783, -1, 0, 494, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max_curve_line_7500_ascii", 10943, 8302, 43, 0, 0, 0, 0, "points->size % 3 == 0" },
	{ "max_curve_line_7500_binary", 10941, 13861, 255, 0, 0, 0, 0, "points" },
	{ "max_curve_line_7500_binary", 10942, 13985, 56, 0, 0, 0, 0, "points_index" },
	{ "max_curve_line_7500_binary", 10964, -1, 0, 0, 140, 0, 0, "line->segments.data" },
	{ "max_curve_line_7500_binary", 11772, 13861, 255, 0, 0, 0, 0, "ufbxi_read_line(uc, node, &info)" },
	{ "max_nurbs_curve_rational_6100_binary", 10865, -1, 0, 283, 0, 0, 0, "nurbs" },
	{ "max_quote_6100_ascii", 16785, -1, 0, 0, 175, 0, 0, "node->all_attribs.data" },
	{ "max_quote_6100_binary", 10671, 8983, 36, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "max_quote_6100_binary", 10674, 9030, 36, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_visibility.d..." },
	{ "max_selection_sets_6100_binary", 11528, -1, 0, 416, 0, 0, 0, "sel" },
	{ "max_selection_sets_6100_binary", 11825, -1, 0, 416, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "max_texture_mapping_6100_binary", 16002, -1, 0, 0, 660, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prefix, ..." },
	{ "max_texture_mapping_6100_binary", 16054, -1, 0, 0, 660, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, name)" },
	{ "max_texture_mapping_6100_binary", 16141, -1, 0, 0, 659, 0, 0, "shader" },
	{ "max_texture_mapping_6100_binary", 16173, -1, 0, 0, 660, 0, 0, "ufbxi_shader_texture_find_prefix(uc, texture, shader)" },
	{ "max_texture_mapping_6100_binary", 16185, -1, 0, 0, 677, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &shader->..." },
	{ "max_texture_mapping_6100_binary", 16245, -1, 0, 0, 661, 0, 0, "shader->inputs.data" },
	{ "max_texture_mapping_6100_binary", 17493, -1, 0, 0, 659, 0, 0, "ufbxi_finalize_shader_texture(uc, texture)" },
	{ "max_texture_mapping_7700_binary", 16031, -1, 0, 0, 733, 0, 0, "ufbxi_push_prop_prefix(uc, &shader->prop_prefix, prop->..." },
	{ "max_transformed_skin_6100_binary", 11988, 63310, 98, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "max_transformed_skin_6100_binary", 12040, 64699, 7, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_anim_light_6100_binary", 11643, -1, 0, 312, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_arnold_textures_6100_binary", 11499, -1, 0, 0, 343, 0, 0, "bindings->prop_bindings.data" },
	{ "maya_arnold_textures_6100_binary", 11815, -1, 0, 0, 343, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 17281, -1, 0, 0, 393, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_blend_shape_cube_6100_binary", 10268, -1, 0, 380, 0, 0, 0, "shape" },
	{ "maya_blend_shape_cube_6100_binary", 10268, -1, 0, 385, 0, 0, 0, "shape" },
	{ "maya_blend_shape_cube_6100_binary", 10340, -1, 0, 371, 0, 0, 0, "deformer" },
	{ "maya_blend_shape_cube_6100_binary", 10340, -1, 0, 376, 0, 0, 0, "deformer" },
	{ "maya_blend_shape_cube_6100_binary", 10341, -1, 0, 374, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 10341, -1, 0, 379, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 10346, -1, 0, 375, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_6100_binary", 10346, -1, 0, 380, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_6100_binary", 10349, -1, 0, 377, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "maya_blend_shape_cube_6100_binary", 10349, -1, 0, 382, 0, 0, 0, "((ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_w..." },
	{ "maya_blend_shape_cube_6100_binary", 10365, -1, 0, 378, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "maya_blend_shape_cube_6100_binary", 10365, -1, 0, 383, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "maya_blend_shape_cube_6100_binary", 10376, -1, 0, 379, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &shape_info.fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 10376, -1, 0, 384, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &shape_info.fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 10382, -1, 0, 383, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 10382, -1, 0, 388, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 10383, -1, 0, 384, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 10383, -1, 0, 389, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 9885, -1, 0, 371, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_blend_shape_cube_6100_binary", 9885, -1, 0, 376, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_blend_shape_cube_6100_binary", 9886, -1, 0, 372, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_blend_shape_cube_6100_binary", 9886, -1, 0, 377, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_blend_shape_cube_6100_binary", 9890, -1, 0, 373, 0, 0, 0, "elem" },
	{ "maya_blend_shape_cube_6100_binary", 9890, -1, 0, 378, 0, 0, 0, "elem" },
	{ "maya_blend_shape_cube_6100_binary", 9952, -1, 0, 378, 0, 0, 0, "conn" },
	{ "maya_blend_shape_cube_6100_binary", 9952, -1, 0, 383, 0, 0, 0, "conn" },
	{ "maya_cache_sine_6100_binary", 16976, -1, 0, 0, 232, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&cache->fil..." },
	{ "maya_cache_sine_6100_binary", 17120, -1, 0, 0, 237, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 18889, -1, 0, 0, 247, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra, 0)" },
	{ "maya_cache_sine_6100_binary", 18894, -1, 0, 0, 250, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_cache_sine_6100_binary", 18939, -1, 0, 0, 251, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &channel-..." },
	{ "maya_cache_sine_6100_binary", 18972, -1, 0, 0, 247, 0, 0, "xml_ok" },
	{ "maya_cache_sine_6100_binary", 18980, -1, 0, 0, 253, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_cache_sine_6100_binary", 18996, -1, 0, 0, 247, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 19055, -1, 0, 0, 253, 0, 0, "ufbxi_cache_try_open_file(cc, filename, &found)" },
	{ "maya_cache_sine_6100_binary", 19178, -1, 0, 0, 255, 0, 0, "cc->cache.channels.data" },
	{ "maya_cache_sine_6100_binary", 19208, -1, 0, 0, 247, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, &found)" },
	{ "maya_cache_sine_6100_binary", 19215, -1, 0, 0, 253, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_cache_sine_6100_binary", 19220, -1, 0, 0, 254, 0, 0, "cc->cache.frames.data" },
	{ "maya_cache_sine_6100_binary", 19223, -1, 0, 0, 255, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_cache_sine_6100_binary", 19227, -1, 0, 0, 256, 0, 0, "cc->imp" },
	{ "maya_cache_sine_6100_binary", 19455, -1, 0, 0, 247, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_cache_sine_6100_binary", 19860, -1, 0, 0, 247, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_color_sets_6100_binary", 10599, -1, 0, 0, 77, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 10646, 9966, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_ascii", 8228, -1, 0, 298, 0, 0, 0, "v" },
	{ "maya_cone_6100_binary", 10651, 16081, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_cone_6100_binary", 10654, 15524, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_cone_6100_binary", 10657, 15571, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease.data,..." },
	{ "maya_constraint_zoo_6100_binary", 17590, -1, 0, 0, 315, 0, 0, "constraint->targets.data" },
	{ "maya_cube_big_endian_6100_binary", 6824, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 6824, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 7134, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 7134, -1, 0, 5, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 8869, -1, 0, 3, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 8869, -1, 0, 4, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_7500_binary", 7125, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_7500_binary", 7125, -1, 0, 5, 0, 0, 0, "header_words" },
	{ "maya_display_layers_6100_binary", 17545, -1, 0, 0, 242, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_human_ik_7400_binary", 11022, -1, 0, 1799, 0, 0, 0, "marker" },
	{ "maya_human_ik_7400_binary", 11022, -1, 0, 1825, 0, 0, 0, "marker" },
	{ "maya_human_ik_7400_binary", 11754, -1, 0, 2544, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 11754, -1, 0, 2576, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 11756, -1, 0, 1799, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_human_ik_7400_binary", 11756, -1, 0, 1825, 0, 0, 0, "ufbxi_read_marker(uc, node, &info, sub_type, UFBX_MARKE..." },
	{ "maya_interpolation_modes_6100_binary", 11954, 16936, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_interpolation_modes_6100_binary", 12021, 16936, 73, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_interpolation_modes_6100_binary", 12131, 16805, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "maya_interpolation_modes_6100_binary", 12202, 16706, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "maya_leading_comma_7500_ascii", 10067, 9370, 43, 0, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_leading_comma_7500_ascii", 10083, 9278, 78, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MappingInformationType, \"C..." },
	{ "maya_leading_comma_7500_ascii", 10134, 10556, 67, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_leading_comma_7500_ascii", 10169, 9303, 67, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_leading_comma_7500_ascii", 10179, 10999, 84, 0, 0, 0, 0, "arr" },
	{ "maya_leading_comma_7500_ascii", 10398, -1, 0, 0, 159, 0, 0, "mesh->faces.data" },
	{ "maya_leading_comma_7500_ascii", 10424, 9073, 43, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_leading_comma_7500_ascii", 10436, -1, 0, 0, 160, 0, 0, "mesh->vertex_first_index.data" },
	{ "maya_leading_comma_7500_ascii", 10508, 8926, 43, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_leading_comma_7500_ascii", 10545, -1, 0, 0, 158, 0, 0, "edges" },
	{ "maya_leading_comma_7500_ascii", 10578, 9073, 43, 0, 0, 0, 0, "ufbxi_process_indices(uc, mesh, index_data)" },
	{ "maya_leading_comma_7500_ascii", 10598, -1, 0, 0, 161, 0, 0, "mesh->uv_sets.data" },
	{ "maya_leading_comma_7500_ascii", 10608, 9278, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_leading_comma_7500_ascii", 10614, 9692, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_leading_comma_7500_ascii", 10622, 10114, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_leading_comma_7500_ascii", 10634, 10531, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, (ufbx_vertex_att..." },
	{ "maya_leading_comma_7500_ascii", 10661, 10925, 78, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_leading_comma_7500_ascii", 10664, 10999, 84, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing.da..." },
	{ "maya_leading_comma_7500_ascii", 10679, 11116, 78, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_leading_comma_7500_ascii", 10684, 11198, 78, 0, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_leading_comma_7500_ascii", 11687, -1, 0, 0, 182, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_leading_comma_7500_ascii", 11696, 8861, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_leading_comma_7500_ascii", 11729, -1, 0, 0, 168, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_leading_comma_7500_ascii", 11732, -1, 0, 0, 169, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_leading_comma_7500_ascii", 11764, 8926, 43, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", 11851, 13120, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_leading_comma_7500_ascii", 12280, 0, 60, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_leading_comma_7500_ascii", 12281, 100, 33, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "maya_leading_comma_7500_ascii", 12299, 1525, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_leading_comma_7500_ascii", 12300, 2615, 33, 0, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_leading_comma_7500_ascii", 12315, -1, 0, 147, 0, 0, 0, "root" },
	{ "maya_leading_comma_7500_ascii", 12315, -1, 0, 148, 0, 0, 0, "root" },
	{ "maya_leading_comma_7500_ascii", 12317, -1, 0, 151, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_leading_comma_7500_ascii", 12317, -1, 0, 152, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "maya_leading_comma_7500_ascii", 12321, 2808, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_leading_comma_7500_ascii", 12322, 3021, 33, 0, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_leading_comma_7500_ascii", 12325, 8762, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_leading_comma_7500_ascii", 12329, 0, 0, 0, 0, 0, 0, "uc->top_node" },
	{ "maya_leading_comma_7500_ascii", 12331, 8861, 33, 0, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_leading_comma_7500_ascii", 12334, 13016, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_leading_comma_7500_ascii", 12335, 13120, 33, 0, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_leading_comma_7500_ascii", 12347, -1, 0, 0, 182, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "maya_leading_comma_7500_ascii", 12909, -1, 0, 0, 183, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_leading_comma_7500_ascii", 12918, -1, 0, 0, 184, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_leading_comma_7500_ascii", 14456, -1, 0, 0, 187, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_leading_comma_7500_ascii", 14486, -1, 0, 0, 188, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_leading_comma_7500_ascii", 14763, -1, 0, 0, 195, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 14785, -1, 0, 0, 196, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 14861, -1, 0, 0, 199, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 16686, -1, 0, 0, 185, 0, 0, "uc->scene.elements.data" },
	{ "maya_leading_comma_7500_ascii", 16691, -1, 0, 0, 186, 0, 0, "element_data" },
	{ "maya_leading_comma_7500_ascii", 16706, -1, 0, 0, 187, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_leading_comma_7500_ascii", 16719, -1, 0, 0, 189, 0, 0, "typed_elems->data" },
	{ "maya_leading_comma_7500_ascii", 16731, -1, 0, 0, 194, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_leading_comma_7500_ascii", 16790, -1, 0, 0, 195, 0, 0, "ufbxi_fetch_dst_elements(uc, &node->materials, &node->e..." },
	{ "maya_leading_comma_7500_ascii", 16832, -1, 0, 0, 196, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_leading_comma_7500_ascii", 17008, -1, 0, 0, 197, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_leading_comma_7500_ascii", 17055, -1, 0, 0, 199, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_leading_comma_7500_ascii", 17151, -1, 0, 0, 200, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_leading_comma_7500_ascii", 17155, -1, 0, 0, 201, 0, 0, "stack->anim.layers.data" },
	{ "maya_leading_comma_7500_ascii", 17169, -1, 0, 0, 202, 0, 0, "layer_desc" },
	{ "maya_leading_comma_7500_ascii", 17245, -1, 0, 0, 203, 0, 0, "layer->anim_props.data" },
	{ "maya_leading_comma_7500_ascii", 17601, -1, 0, 0, 204, 0, 0, "descs" },
	{ "maya_leading_comma_7500_ascii", 19821, -1, 0, 1, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_leading_comma_7500_ascii", 19827, -1, 0, 3, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_leading_comma_7500_ascii", 19827, -1, 0, 4, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_leading_comma_7500_ascii", 19831, 0, 60, 0, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_leading_comma_7500_ascii", 19834, -1, 0, 0, 183, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_leading_comma_7500_ascii", 19843, -1, 0, 0, 185, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_leading_comma_7500_ascii", 19882, -1, 0, 0, 205, 0, 0, "imp" },
	{ "maya_leading_comma_7500_ascii", 2690, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 2728, -1, 0, 86, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 2728, -1, 0, 87, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 3416, -1, 0, 1, 0, 0, 0, "data" },
	{ "maya_leading_comma_7500_ascii", 3994, -1, 0, 0, 10, 0, 0, "dst" },
	{ "maya_leading_comma_7500_ascii", 4038, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_leading_comma_7500_ascii", 4059, -1, 0, 0, 52, 0, 0, "str" },
	{ "maya_leading_comma_7500_ascii", 5130, -1, 0, 0, 0, 0, 1, "result != UFBX_PROGRESS_CANCEL" },
	{ "maya_leading_comma_7500_ascii", 5149, -1, 0, 0, 0, 1, 0, "!uc->eof" },
	{ "maya_leading_comma_7500_ascii", 5235, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_leading_comma_7500_ascii", 7559, -1, 0, 0, 0, 0, 57, "ufbxi_report_progress(uc)" },
	{ "maya_leading_comma_7500_ascii", 7699, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_leading_comma_7500_ascii", 7699, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_leading_comma_7500_ascii", 7722, -1, 0, 0, 0, 9570, 0, "c != '\\0'" },
	{ "maya_leading_comma_7500_ascii", 7776, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 7776, -1, 0, 4, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 7795, -1, 0, 6, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 7795, -1, 0, 7, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 7824, 288, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_leading_comma_7500_ascii", 7831, 3707, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_leading_comma_7500_ascii", 7886, 291, 0, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_leading_comma_7500_ascii", 7906, 288, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 7975, 9088, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 8038, 8943, 46, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 8050, 2537, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 8056, 168, 0, 0, 0, 0, 0, "depth == 0" },
	{ "maya_leading_comma_7500_ascii", 8064, 0, 60, 0, 0, 0, 0, "Expected a 'Name:' token" },
	{ "maya_leading_comma_7500_ascii", 8066, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_leading_comma_7500_ascii", 8070, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_leading_comma_7500_ascii", 8075, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_leading_comma_7500_ascii", 8075, -1, 0, 5, 0, 0, 0, "node" },
	{ "maya_leading_comma_7500_ascii", 8135, 291, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 8149, 8943, 46, 0, 0, 0, 0, "ufbxi_ascii_read_float_array(uc, (char)arr_type, &num_r..." },
	{ "maya_leading_comma_7500_ascii", 8151, 9088, 45, 0, 0, 0, 0, "ufbxi_ascii_read_int_array(uc, (char)arr_type, &num_rea..." },
	{ "maya_leading_comma_7500_ascii", 8196, -1, 0, 0, 10, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &v->s, st..." },
	{ "maya_leading_comma_7500_ascii", 8292, 8927, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'I')" },
	{ "maya_leading_comma_7500_ascii", 8295, 8931, 11, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_leading_comma_7500_ascii", 8303, -1, 0, 0, 0, 9570, 0, "ufbxi_ascii_skip_until(uc, '}')" },
	{ "maya_leading_comma_7500_ascii", 8320, 8937, 33, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_leading_comma_7500_ascii", 8331, -1, 0, 0, 144, 0, 0, "arr_data" },
	{ "maya_leading_comma_7500_ascii", 8347, -1, 0, 8, 0, 0, 0, "node->vals" },
	{ "maya_leading_comma_7500_ascii", 8347, -1, 0, 9, 0, 0, 0, "node->vals" },
	{ "maya_leading_comma_7500_ascii", 8357, 168, 11, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_leading_comma_7500_ascii", 8364, -1, 0, 28, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 8364, -1, 0, 29, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 8855, -1, 0, 0, 0, 1, 0, "header" },
	{ "maya_leading_comma_7500_ascii", 8887, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_leading_comma_7500_ascii", 8887, -1, 0, 4, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_leading_comma_7500_ascii", 8904, 100, 33, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_leading_comma_7500_ascii", 8933, 0, 60, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_leading_comma_7500_ascii", 8954, -1, 0, 5, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 8954, -1, 0, 6, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 8973, 1544, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_leading_comma_7500_ascii", 8981, -1, 0, 131, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 8981, -1, 0, 132, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 9004, 100, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_pars..." },
	{ "maya_leading_comma_7500_ascii", 9302, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_leading_comma_7500_ascii", 9308, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 9308, -1, 0, 3, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 9467, -1, 0, 84, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 9467, -1, 0, 85, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 9509, -1, 0, 0, 42, 0, 0, "props->props.data" },
	{ "maya_leading_comma_7500_ascii", 9515, -1, 0, 84, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_leading_comma_7500_ascii", 9515, -1, 0, 85, 0, 0, 0, "ufbxi_sort_properties(uc, props->props.data, props->pro..." },
	{ "maya_leading_comma_7500_ascii", 9523, -1, 0, 84, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_leading_comma_7500_ascii", 9523, -1, 0, 85, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_leading_comma_7500_ascii", 9535, 100, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 9552, -1, 0, 84, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_leading_comma_7500_ascii", 9552, -1, 0, 85, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_leading_comma_7500_ascii", 9664, 2615, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 9683, 3021, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object)" },
	{ "maya_leading_comma_7500_ascii", 9690, -1, 0, 164, 0, 0, 0, "tmpl" },
	{ "maya_leading_comma_7500_ascii", 9690, -1, 0, 165, 0, 0, 0, "tmpl" },
	{ "maya_leading_comma_7500_ascii", 9691, 3061, 33, 0, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_leading_comma_7500_ascii", 9709, -1, 0, 0, 52, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_leading_comma_7500_ascii", 9712, -1, 0, 283, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_leading_comma_7500_ascii", 9712, -1, 0, 284, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_leading_comma_7500_ascii", 9718, -1, 0, 0, 142, 0, 0, "uc->templates" },
	{ "maya_leading_comma_7500_ascii", 9806, -1, 0, 0, 168, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name, 0)" },
	{ "maya_leading_comma_7500_ascii", 9819, -1, 0, 150, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 9819, -1, 0, 151, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 9861, -1, 0, 147, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_leading_comma_7500_ascii", 9861, -1, 0, 148, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_..." },
	{ "maya_leading_comma_7500_ascii", 9862, -1, 0, 148, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_leading_comma_7500_ascii", 9862, -1, 0, 149, 0, 0, 0, "((size_t*)ufbxi_push_size_copy((&uc->tmp_element_offset..." },
	{ "maya_leading_comma_7500_ascii", 9866, -1, 0, 149, 0, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", 9866, -1, 0, 150, 0, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", 9874, -1, 0, 150, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, elem->element_id)" },
	{ "maya_leading_comma_7500_ascii", 9874, -1, 0, 151, 0, 0, 0, "ufbxi_insert_fbx_id(uc, info->fbx_id, elem->element_id)" },
	{ "maya_lod_group_6100_binary", 11675, -1, 0, 278, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_lod_group_6100_binary", 11675, -1, 0, 284, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_lod_group_7500_ascii", 11758, -1, 0, 486, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_lod_group_7500_ascii", 11758, -1, 0, 490, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_6100_ascii", 7943, -1, 0, 460, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 7943, -1, 0, 465, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 8015, -1, 0, 445, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 8015, -1, 0, 450, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 8099, -1, 0, 442, 0, 0, 0, "arr" },
	{ "maya_node_attribute_zoo_6100_ascii", 8099, -1, 0, 447, 0, 0, 0, "arr" },
	{ "maya_node_attribute_zoo_6100_ascii", 8116, -1, 0, 443, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_node_attribute_zoo_6100_ascii", 8116, -1, 0, 448, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, 8, 1)" },
	{ "maya_node_attribute_zoo_6100_ascii", 8120, -1, 0, 444, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_node_attribute_zoo_6100_ascii", 8120, -1, 0, 449, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_node_attribute_zoo_6100_binary", 10487, -1, 0, 532, 0, 0, 0, "mesh" },
	{ "maya_node_attribute_zoo_6100_binary", 10487, -1, 0, 537, 0, 0, 0, "mesh" },
	{ "maya_node_attribute_zoo_6100_binary", 10870, 138209, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->basis..." },
	{ "maya_node_attribute_zoo_6100_binary", 10872, 138308, 255, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 10879, 138359, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 10880, 138416, 1, 0, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 10881, 143462, 27, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 10900, 139478, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 10901, 139592, 1, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"ZZ\", &dimens..." },
	{ "maya_node_attribute_zoo_6100_binary", 10902, 139631, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Step, \"II\", &step_u, &ste..." },
	{ "maya_node_attribute_zoo_6100_binary", 10903, 139664, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 10916, 139691, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 10917, 139727, 1, 0, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 10918, 140321, 3, 0, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 10919, 141818, 63, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 10920, 139655, 1, 0, 0, 0, 0, "points->size / 4 == (size_t)dimension_u * (size_t)dimen..." },
	{ "maya_node_attribute_zoo_6100_binary", 11007, -1, 0, 707, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 11007, -1, 0, 712, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 11600, -1, 0, 269, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 11600, -1, 0, 274, 0, 0, 0, "ufbxi_push_synthetic_id(uc, &attrib_info.fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 11606, -1, 0, 0, 39, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_node_attribute_zoo_6100_binary", 11619, -1, 0, 270, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_node_attribute_zoo_6100_binary", 11619, -1, 0, 275, 0, 0, 0, "ufbxi_insert_fbx_attr(uc, info->fbx_id, attrib_info.fbx..." },
	{ "maya_node_attribute_zoo_6100_binary", 11626, -1, 0, 271, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_6100_binary", 11626, -1, 0, 276, 0, 0, 0, "((ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), siz..." },
	{ "maya_node_attribute_zoo_6100_binary", 11636, -1, 0, 0, 40, 0, 0, "attrib_info.props.props.data" },
	{ "maya_node_attribute_zoo_6100_binary", 11641, 12128, 23, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_node_attribute_zoo_6100_binary", 11647, -1, 0, 707, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 11647, -1, 0, 712, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 11649, -1, 0, 274, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 11649, -1, 0, 279, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 11681, -1, 0, 276, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 11681, -1, 0, 281, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 11702, 157559, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 11737, 12128, 23, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, type_st..." },
	{ "maya_node_attribute_zoo_6100_binary", 11739, -1, 0, 277, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 11739, -1, 0, 282, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 11768, 138209, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 11770, 139478, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 11833, -1, 0, 0, 392, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 11937, 163331, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "maya_node_attribute_zoo_6100_binary", 11940, 163352, 1, 0, 0, 0, 0, "curve->keyframes.data" },
	{ "maya_node_attribute_zoo_6100_binary", 12060, 163388, 86, 0, 0, 0, 0, "Unknown key mode" },
	{ "maya_node_attribute_zoo_6100_binary", 12065, 163349, 3, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_node_attribute_zoo_6100_binary", 12114, 163349, 1, 0, 0, 0, 0, "data == data_end" },
	{ "maya_node_attribute_zoo_6100_binary", 12189, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "maya_node_attribute_zoo_6100_binary", 12211, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "maya_node_attribute_zoo_6100_binary", 12224, 163019, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"S\", &stack->name)" },
	{ "maya_node_attribute_zoo_6100_binary", 12234, 163046, 255, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_ReferenceTime, \"LL\", &beg..." },
	{ "maya_node_attribute_zoo_6100_binary", 12244, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 12254, 162983, 125, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_node_attribute_zoo_6100_binary", 12258, 163019, 0, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 12294, -1, 0, 41, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 12294, -1, 0, 42, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 12340, 158678, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_node_attribute_zoo_6100_binary", 12341, 162983, 125, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 12345, 162983, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_node_attribute_zoo_6100_binary", 15872, -1, 0, 0, 490, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 15915, -1, 0, 0, 509, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 17138, -1, 0, 0, 490, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 17143, -1, 0, 0, 499, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 17144, -1, 0, 0, 500, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 17166, -1, 0, 0, 505, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "maya_node_attribute_zoo_6100_binary", 17618, -1, 0, 0, 509, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_6100_binary", 7033, -1, 0, 0, 0, 12405, 0, "val" },
	{ "maya_node_attribute_zoo_6100_binary", 7036, -1, 0, 0, 0, 12158, 0, "val" },
	{ "maya_node_attribute_zoo_6100_binary", 7182, -1, 0, 448, 0, 0, 0, "arr" },
	{ "maya_node_attribute_zoo_6100_binary", 7182, -1, 0, 453, 0, 0, 0, "arr" },
	{ "maya_node_attribute_zoo_6100_binary", 7341, 12130, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_node_attribute_zoo_6100_binary", 9276, -1, 0, 41, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_node_attribute_zoo_6100_binary", 9276, -1, 0, 42, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_node_attribute_zoo_6100_binary", 9770, -1, 0, 269, 0, 0, 0, "ptr" },
	{ "maya_node_attribute_zoo_6100_binary", 9770, -1, 0, 274, 0, 0, 0, "ptr" },
	{ "maya_node_attribute_zoo_6100_binary", 9847, -1, 0, 270, 0, 0, 0, "entry" },
	{ "maya_node_attribute_zoo_6100_binary", 9847, -1, 0, 275, 0, 0, 0, "entry" },
	{ "maya_node_attribute_zoo_6100_binary", 9915, -1, 0, 277, 0, 0, 0, "elem_node" },
	{ "maya_node_attribute_zoo_6100_binary", 9915, -1, 0, 282, 0, 0, 0, "elem_node" },
	{ "maya_node_attribute_zoo_6100_binary", 9924, -1, 0, 274, 0, 0, 0, "elem" },
	{ "maya_node_attribute_zoo_6100_binary", 9924, -1, 0, 279, 0, 0, 0, "elem" },
	{ "maya_node_attribute_zoo_6100_binary", 9931, -1, 0, 276, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", 9931, -1, 0, 281, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_7500_ascii", 10865, -1, 0, 903, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_7500_ascii", 11742, -1, 0, 729, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_ascii", 11742, -1, 0, 732, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_ascii", 11744, -1, 0, 715, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_7500_ascii", 11744, -1, 0, 718, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_7500_ascii", 11746, -1, 0, 660, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_ascii", 11746, -1, 0, 663, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_ascii", 11750, -1, 0, 780, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_ascii", 11750, -1, 0, 783, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_ascii", 8228, -1, 0, 891, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_binary", 11143, 61038, 255, 0, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 11144, 61115, 255, 0, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 11145, 61175, 255, 0, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 11146, 61234, 255, 0, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 11147, 61292, 255, 0, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_binary", 11150, 61122, 0, 0, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_binary", 11155, 61242, 0, 0, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_binary", 11156, 61300, 0, 0, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", 11160, -1, 0, 0, 328, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 11311, 61431, 0, 0, 0, 0, 0, "refs_left >= 0" },
	{ "maya_node_attribute_zoo_7500_binary", 11748, -1, 0, 488, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 11748, -1, 0, 492, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 11809, 61038, 255, 0, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", 14610, -1, 0, 0, 359, 0, 0, "elem->props.props.data" },
	{ "maya_node_attribute_zoo_7500_binary", 16707, -1, 0, 0, 359, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 7075, 61146, 109, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 7076, 61333, 103, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 7314, -1, 0, 0, 0, 0, 2942, "ufbxi_resume_progress(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 7318, -1, 0, 0, 0, 0, 2943, "res != -28" },
	{ "maya_node_attribute_zoo_7500_binary", 9805, -1, 0, 0, 327, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type, 0)" },
	{ "maya_polygon_hole_6100_binary", 10707, 9377, 37, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "maya_polygon_hole_6100_binary", 10709, 9342, 0, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_hole.data, &..." },
	{ "maya_resampled_7500_binary", 11184, 24917, 23, 0, 0, 0, 0, "p_ref < p_ref_end" },
	{ "maya_scale_no_inherit_6100_ascii", 12017, 19165, 114, 0, 0, 0, 0, "Unknown slope mode" },
	{ "maya_scale_no_inherit_6100_ascii", 12047, 19171, 111, 0, 0, 0, 0, "Unknown weight mode" },
	{ "maya_shaderfx_pbs_material_7700_ascii", 9449, -1, 0, 0, 321, 0, 0, "ufbxi_read_embedded_blob(uc, &prop->value_blob, binary)" },
	{ "maya_slime_7500_binary", 10936, -1, 0, 854, 0, 0, 0, "line" },
	{ "maya_slime_7500_binary", 10936, -1, 0, 859, 0, 0, 0, "line" },
	{ "maya_texture_layers_6100_binary", 14926, -1, 0, 0, 267, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 16477, -1, 0, 0, 272, 0, 0, "texture->file_textures.data" },
	{ "maya_texture_layers_6100_binary", 17500, -1, 0, 0, 267, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_transform_animation_6100_binary", 12056, 17549, 11, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_zero_end_7400_binary", 10714, 12861, 0, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "maya_zero_end_7400_binary", 11715, 12333, 255, 0, 0, 0, 0, "(info.fbx_id & (0x8000000000000000ULL)) == 0" },
	{ "maya_zero_end_7400_binary", 2689, 12382, 255, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_zero_end_7400_binary", 2727, 16748, 1, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_zero_end_7400_binary", 5151, 36, 255, 0, 0, 0, 0, "uc->read_fn" },
	{ "maya_zero_end_7400_binary", 5217, -1, 0, 0, 0, 0, 1434, "ufbxi_report_progress(uc)" },
	{ "maya_zero_end_7400_binary", 5312, 36, 255, 0, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_zero_end_7400_binary", 5340, -1, 0, 0, 0, 12392, 0, "uc->read_fn" },
	{ "maya_zero_end_7400_binary", 5348, -1, 0, 0, 0, 0, 1434, "ufbxi_resume_progress(uc)" },
	{ "maya_zero_end_7400_binary", 7073, 16744, 106, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 7074, 12615, 106, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 7077, 12379, 101, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 7099, 12382, 255, 0, 0, 0, 0, "data" },
	{ "maya_zero_end_7400_binary", 7121, -1, 0, 0, 0, 27, 0, "header" },
	{ "maya_zero_end_7400_binary", 7142, 24, 29, 0, 0, 0, 0, "num_values64 <= 0xffffffffui32" },
	{ "maya_zero_end_7400_binary", 7160, -1, 0, 3, 0, 0, 0, "node" },
	{ "maya_zero_end_7400_binary", 7160, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_zero_end_7400_binary", 7164, -1, 0, 0, 0, 40, 0, "name" },
	{ "maya_zero_end_7400_binary", 7166, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_zero_end_7400_binary", 7191, -1, 0, 0, 0, 12379, 0, "data" },
	{ "maya_zero_end_7400_binary", 7226, 12382, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_zero_end_7400_binary", 7233, 16748, 1, 0, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_zero_end_7400_binary", 7246, 12379, 99, 0, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_zero_end_7400_binary", 7262, -1, 0, 0, 0, 12392, 0, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_zero_end_7400_binary", 7319, 12384, 1, 0, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_zero_end_7400_binary", 7322, 12384, 255, 0, 0, 0, 0, "Bad array encoding" },
	{ "maya_zero_end_7400_binary", 7342, 12379, 101, 0, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_zero_end_7400_binary", 7358, -1, 0, 6, 0, 0, 0, "vals" },
	{ "maya_zero_end_7400_binary", 7358, -1, 0, 7, 0, 0, 0, "vals" },
	{ "maya_zero_end_7400_binary", 7366, -1, 0, 0, 0, 87, 0, "data" },
	{ "maya_zero_end_7400_binary", 7419, 331, 0, 0, 0, 0, 0, "str" },
	{ "maya_zero_end_7400_binary", 7429, -1, 0, 0, 11, 0, 0, "ufbxi_push_sanitized_string(&uc->string_pool, &vals[i]...." },
	{ "maya_zero_end_7400_binary", 7444, 593, 8, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_zero_end_7400_binary", 7449, 22, 1, 0, 0, 0, 0, "Bad value type" },
	{ "maya_zero_end_7400_binary", 7460, 66, 4, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_zero_end_7400_binary", 7462, 36, 255, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_zero_end_7400_binary", 7474, 58, 93, 0, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_zero_end_7400_binary", 7479, 70, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_zero_end_7400_binary", 7488, -1, 0, 28, 0, 0, 0, "node->children" },
	{ "maya_zero_end_7400_binary", 7488, -1, 0, 29, 0, 0, 0, "node->children" },
	{ "maya_zero_end_7400_binary", 8906, 35, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_zero_end_7400_binary", 8935, 22, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_zero_end_7400_binary", 9392, 797, 0, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_zero_end_7400_binary", 9395, 6091, 0, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_zero_end_7400_binary", 9512, 797, 0, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_zero_end_7400_binary", 9697, 4105, 0, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_zero_end_7500_binary", 12838, 24, 0, 0, 0, 0, 0, "ufbxi_parse_legacy_toplevel(uc)" },
	{ "maya_zero_end_7500_binary", 19829, 24, 0, 0, 0, 0, 0, "ufbxi_read_legacy_root(uc)" },
	{ "maya_zero_end_7500_binary", 9037, 24, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "obj_fuzz_0006", 13771, 0, 29, 0, 0, 0, 0, "num_indices == 0 || !required" },
	{ "obj_fuzz_0012", 14031, -1, 0, 13, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "obj_fuzz_0012", 14031, -1, 0, 14, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "obj_fuzz_0023", 13710, -1, 0, 28, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, mesh->fbx_node_id)" },
	{ "obj_fuzz_0023", 13710, -1, 0, 29, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, mesh->fbx_node_id)" },
	{ "obj_fuzz_0028", 13564, -1, 0, 56, 0, 0, 0, "((uint32_t*)ufbxi_push_size_zero((&uc->obj.tmp_face_gro..." },
	{ "obj_fuzz_0028", 13564, -1, 0, 57, 0, 0, 0, "((uint32_t*)ufbxi_push_size_zero((&uc->obj.tmp_face_gro..." },
	{ "obj_fuzz_0030", 14025, -1, 0, 23, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "obj_fuzz_0030", 14025, -1, 0, 24, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_face_smooth..." },
	{ "obj_fuzz_0052", 13677, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &name, 0)" },
	{ "obj_fuzz_0159", 13451, 116, 11, 0, 0, 0, 0, "index < 0xffffffffffffffffui64 / 10 - 10" },
	{ "revit_empty_7400_binary", 10020, -1, 0, 0, 262, 0, 0, "new_indices" },
	{ "revit_empty_7400_binary", 10103, -1, 0, 0, 262, 0, 0, "ufbxi_check_indices(uc, &attrib->indices.data, index_da..." },
	{ "revit_empty_7400_binary", 10681, 21004, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material.dat..." },
	{ "revit_empty_7400_binary", 7007, 25199, 2, 0, 0, 0, 0, "type == 'S' || type == 'R'" },
	{ "revit_empty_7400_binary", 7016, 25220, 255, 0, 0, 0, 0, "d->data" },
	{ "revit_empty_7400_binary", 7020, -1, 0, 0, 305, 0, 0, "d->data" },
	{ "synthetic_binary_props_7500_ascii", 8164, -1, 0, 59, 0, 0, 0, "v" },
	{ "synthetic_binary_props_7500_ascii", 8164, -1, 0, 60, 0, 0, 0, "v" },
	{ "synthetic_binary_props_7500_ascii", 8170, -1, 0, 104, 0, 0, 0, "v->data" },
	{ "synthetic_binary_props_7500_ascii", 8170, -1, 0, 105, 0, 0, 0, "v->data" },
	{ "synthetic_broken_filename_7500_ascii", 13022, -1, 0, 0, 256, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &dst, raw..." },
	{ "synthetic_broken_filename_7500_ascii", 14837, -1, 0, 0, 255, 0, 0, "list->data" },
	{ "synthetic_broken_filename_7500_ascii", 16489, -1, 0, 0, 259, 0, 0, "texture->file_textures.data" },
	{ "synthetic_broken_filename_7500_ascii", 16615, -1, 0, 0, 256, 0, 0, "ufbxi_resolve_relative_filename(uc, filename, relative_..." },
	{ "synthetic_broken_filename_7500_ascii", 17336, -1, 0, 0, 255, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "synthetic_broken_filename_7500_ascii", 17453, -1, 0, 0, 256, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->fil..." },
	{ "synthetic_broken_filename_7500_ascii", 17454, -1, 0, 0, 257, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&video->raw..." },
	{ "synthetic_broken_filename_7500_ascii", 17621, -1, 0, 0, 259, 0, 0, "ufbxi_fetch_file_textures(uc)" },
	{ "synthetic_color_suzanne_0_obj", 13591, -1, 0, 2122, 0, 0, 0, "p_face_smooth" },
	{ "synthetic_color_suzanne_0_obj", 13591, -1, 0, 2123, 0, 0, 0, "p_face_smooth" },
	{ "synthetic_color_suzanne_0_obj", 13908, -1, 0, 0, 6, 0, 0, "fbx_mesh->face_smoothing.data" },
	{ "synthetic_color_suzanne_0_obj", 13924, -1, 0, 0, 9, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "synthetic_color_suzanne_0_obj", 14008, -1, 0, 14, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_0_obj", 14008, -1, 0, 15, 0, 0, 0, "valid" },
	{ "synthetic_color_suzanne_0_obj", 14015, 41935, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_NORMAL, 1)" },
	{ "synthetic_color_suzanne_1_obj", 13192, -1, 0, 0, 3, 0, 0, "ufbxi_obj_pop_props(uc, &uc->obj.mesh->fbx_mesh->props...." },
	{ "synthetic_color_suzanne_1_obj", 13532, -1, 0, 1629, 0, 0, 0, "entry" },
	{ "synthetic_color_suzanne_1_obj", 13532, -1, 0, 1630, 0, 0, 0, "entry" },
	{ "synthetic_color_suzanne_1_obj", 13545, -1, 0, 1630, 0, 0, 0, "prop" },
	{ "synthetic_color_suzanne_1_obj", 13545, -1, 0, 1631, 0, 0, 0, "prop" },
	{ "synthetic_color_suzanne_1_obj", 13553, -1, 0, 0, 2, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "synthetic_color_suzanne_1_obj", 13597, -1, 0, 1633, 0, 0, 0, "p_face_group" },
	{ "synthetic_color_suzanne_1_obj", 13597, -1, 0, 1634, 0, 0, 0, "p_face_group" },
	{ "synthetic_color_suzanne_1_obj", 13914, -1, 0, 0, 9, 0, 0, "fbx_mesh->face_group.data" },
	{ "synthetic_color_suzanne_1_obj", 14037, -1, 0, 0, 1, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->obj...." },
	{ "synthetic_color_suzanne_1_obj", 14054, -1, 0, 0, 3, 0, 0, "ufbxi_obj_flush_mesh(uc)" },
	{ "synthetic_cube_nan_6100_ascii", 7800, 4866, 45, 0, 0, 0, 0, "token->type == 'F'" },
	{ "synthetic_empty_elements_7500_ascii", 14671, 2800, 49, 0, 0, 0, 0, "depth <= num_nodes" },
	{ "synthetic_empty_elements_7500_ascii", 16708, 2800, 49, 0, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "synthetic_id_collision_7500_ascii", 10593, -1, 0, 745, 0, 0, 0, "bitangents" },
	{ "synthetic_id_collision_7500_ascii", 10593, -1, 0, 748, 0, 0, 0, "bitangents" },
	{ "synthetic_id_collision_7500_ascii", 10594, -1, 0, 746, 0, 0, 0, "tangents" },
	{ "synthetic_id_collision_7500_ascii", 10594, -1, 0, 749, 0, 0, 0, "tangents" },
	{ "synthetic_id_collision_7500_ascii", 11327, -1, 0, 797, 0, 0, 0, "material" },
	{ "synthetic_id_collision_7500_ascii", 11327, -1, 0, 800, 0, 0, 0, "material" },
	{ "synthetic_id_collision_7500_ascii", 11795, -1, 0, 797, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "synthetic_id_collision_7500_ascii", 11795, -1, 0, 800, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "synthetic_id_collision_7500_ascii", 11803, -1, 0, 812, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "synthetic_id_collision_7500_ascii", 11803, -1, 0, 815, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "synthetic_id_collision_7500_ascii", 11805, -1, 0, 817, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "synthetic_id_collision_7500_ascii", 11805, -1, 0, 820, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "synthetic_id_collision_7500_ascii", 8224, -1, 0, 685, 0, 0, 0, "v" },
	{ "synthetic_id_collision_7500_ascii", 8224, -1, 0, 688, 0, 0, 0, "v" },
	{ "synthetic_id_collision_7500_ascii", 9916, -1, 0, 832, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "synthetic_id_collision_7500_ascii", 9916, -1, 0, 835, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "synthetic_indexed_by_vertex_7500_ascii", 10109, -1, 0, 0, 159, 0, 0, "new_index_data" },
	{ "synthetic_missing_version_6100_ascii", 11643, -1, 0, 866, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 11645, -1, 0, 633, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 11645, -1, 0, 638, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 11669, -1, 0, 250, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 11669, -1, 0, 255, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 12142, 72840, 102, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "synthetic_missing_version_6100_ascii", 16801, -1, 0, 0, 255, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_mixed_attribs_0_obj", 13175, -1, 0, 68, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "synthetic_mixed_attribs_0_obj", 13175, -1, 0, 69, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "synthetic_mixed_attribs_0_obj", 13817, -1, 0, 73, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "synthetic_mixed_attribs_0_obj", 13817, -1, 0, 74, 0, 0, 0, "((_Bool*)ufbxi_push_size_zero((&uc->obj.tmp_color_valid..." },
	{ "synthetic_mixed_attribs_0_obj", 14003, -1, 0, 72, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_0_obj", 14003, -1, 0, 73, 0, 0, 0, "ufbxi_obj_pad_colors(uc, num_vertices - 1)" },
	{ "synthetic_mixed_attribs_0_obj", 14386, -1, 0, 155, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_mixed_attribs_0_obj", 14386, -1, 0, 156, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_mixed_attribs_0_obj", 14488, -1, 0, 155, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "synthetic_mixed_attribs_0_obj", 14488, -1, 0, 156, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 13865, -1, 0, 0, 8, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, 0..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 13868, -1, 0, 0, 11, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "synthetic_mixed_attribs_reuse_0_obj", 13870, -1, 0, 100, 0, 0, 0, "color_valid" },
	{ "synthetic_mixed_attribs_reuse_0_obj", 13870, -1, 0, 99, 0, 0, 0, "color_valid" },
	{ "synthetic_parent_directory_7700_ascii", 16598, -1, 0, 0, 261, 0, 0, "dst" },
	{ "synthetic_parent_directory_7700_ascii", 16612, -1, 0, 0, 261, 0, 0, "ufbxi_absolute_to_relative_path(uc, relative_filename, ..." },
	{ "synthetic_parent_directory_7700_ascii", 17495, -1, 0, 0, 263, 0, 0, "ufbxi_resolve_filenames(uc, (ufbxi_strblob*)&texture->f..." },
	{ "synthetic_partial_attrib_0_obj", 13951, -1, 0, 0, 9, 0, 0, "indices" },
	{ "synthetic_partial_attrib_0_obj", 14776, -1, 0, 61, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_partial_attrib_0_obj", 14776, -1, 0, 62, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "synthetic_string_collision_7500_ascii", 4008, -1, 0, 2221, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_string_collision_7500_ascii", 4008, -1, 0, 2252, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_texture_split_7500_ascii", 11394, -1, 0, 851, 0, 0, 0, "video" },
	{ "synthetic_texture_split_7500_ascii", 11394, -1, 0, 857, 0, 0, 0, "video" },
	{ "synthetic_texture_split_7500_ascii", 7887, -1, 0, 929, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "synthetic_texture_split_7500_ascii", 7887, -1, 0, 935, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "synthetic_texture_split_7500_ascii", 8232, 14287, 45, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_texture_split_7500_ascii", 8258, 28571, 35, 0, 0, 0, 0, "Bad array dst type" },
	{ "synthetic_texture_split_7500_binary", 7005, -1, 0, 0, 0, 26628, 0, "val" },
	{ "synthetic_texture_split_7500_binary", 9358, -1, 0, 0, 229, 0, 0, "dst" },
	{ "synthetic_unicode_7500_binary", 3870, -1, 0, 12, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 3870, -1, 0, 13, 0, 0, 0, "ufbxi_grow_array_size((pool->map.ator), sizeof(**(&pool..." },
	{ "synthetic_unicode_7500_binary", 3966, -1, 0, 1144, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_unicode_7500_binary", 3966, -1, 0, 1336, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_unicode_7500_binary", 3977, -1, 0, 12, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "synthetic_unicode_7500_binary", 3977, -1, 0, 13, 0, 0, 0, "ufbxi_sanitize_string(pool, sanitized, str, length, val..." },
	{ "zbrush_d20_6100_binary", 10276, 25242, 2, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "zbrush_d20_6100_binary", 10277, 25217, 0, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "zbrush_d20_6100_binary", 10290, 25290, 2, 0, 0, 0, 0, "normals && normals->size == vertices->size" },
	{ "zbrush_d20_6100_binary", 10336, 25189, 0, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "zbrush_d20_6100_binary", 10353, -1, 0, 0, 99, 0, 0, "shape_props" },
	{ "zbrush_d20_6100_binary", 10380, 25217, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "zbrush_d20_6100_binary", 10491, 25189, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "zbrush_d20_6100_binary", 10700, 8305, 32, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"c\",..." },
	{ "zbrush_d20_6100_binary", 10702, 8394, 33, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_group.data, ..." },
	{ "zbrush_d20_6100_binary", 14883, -1, 0, 0, 263, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 14903, -1, 0, 0, 254, 0, 0, "list->data" },
	{ "zbrush_d20_6100_binary", 16953, -1, 0, 0, 253, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "zbrush_d20_6100_binary", 16988, -1, 0, 0, 254, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "zbrush_d20_6100_binary", 17119, -1, 0, 0, 262, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "zbrush_d20_6100_binary", 17121, -1, 0, 0, 263, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "zbrush_d20_6100_binary", 17351, -1, 0, 0, 268, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "zbrush_d20_6100_binary", 17415, -1, 0, 0, 269, 0, 0, "texs" },
	{ "zbrush_d20_7500_ascii", 11411, -1, 0, 0, 252, 0, 0, "ufbxi_read_embedded_blob(uc, &video->content, content_n..." },
	{ "zbrush_d20_7500_ascii", 11801, -1, 0, 0, 252, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "zbrush_d20_7500_ascii", 9376, -1, 0, 0, 252, 0, 0, "dst_blob->data" },
	{ "zbrush_d20_7500_binary", 11766, 32981, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "zbrush_d20_selection_set_6100_binary", 17550, -1, 0, 0, 405, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "zbrush_vertex_color_0_obj", 13002, -1, 0, 67, 0, 0, 0, "result" },
	{ "zbrush_vertex_color_0_obj", 13002, -1, 0, 68, 0, 0, 0, "result" },
	{ "zbrush_vertex_color_0_obj", 13072, -1, 0, 0, 8, 0, 0, "mesh->vertex_first_index.data" },
	{ "zbrush_vertex_color_0_obj", 13089, -1, 0, 0, 9, 0, 0, "uv_set" },
	{ "zbrush_vertex_color_0_obj", 13102, -1, 0, 0, 10, 0, 0, "color_set" },
	{ "zbrush_vertex_color_0_obj", 13125, -1, 0, 0, 20, 0, 0, "props.data" },
	{ "zbrush_vertex_color_0_obj", 13142, -1, 0, 78, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "zbrush_vertex_color_0_obj", 13142, -1, 0, 79, 0, 0, 0, "ufbxi_sort_properties(uc, props.data, props.count)" },
	{ "zbrush_vertex_color_0_obj", 13153, -1, 0, 45, 0, 0, 0, "mesh" },
	{ "zbrush_vertex_color_0_obj", 13153, -1, 0, 46, 0, 0, 0, "mesh" },
	{ "zbrush_vertex_color_0_obj", 13171, -1, 0, 46, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "zbrush_vertex_color_0_obj", 13171, -1, 0, 47, 0, 0, 0, "mesh->fbx_node && mesh->fbx_mesh" },
	{ "zbrush_vertex_color_0_obj", 13181, -1, 0, 50, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "zbrush_vertex_color_0_obj", 13181, -1, 0, 51, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_mesh_id, mesh->fbx_node_..." },
	{ "zbrush_vertex_color_0_obj", 13182, -1, 0, 51, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "zbrush_vertex_color_0_obj", 13182, -1, 0, 52, 0, 0, 0, "ufbxi_connect_oo(uc, mesh->fbx_node_id, 0)" },
	{ "zbrush_vertex_color_0_obj", 13266, -1, 0, 68, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "zbrush_vertex_color_0_obj", 13266, -1, 0, 69, 0, 0, 0, "ufbxi_refill(uc, new_cap, 0)" },
	{ "zbrush_vertex_color_0_obj", 13292, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "zbrush_vertex_color_0_obj", 13298, -1, 0, 62, 0, 0, 0, "new_data" },
	{ "zbrush_vertex_color_0_obj", 13298, -1, 0, 63, 0, 0, 0, "new_data" },
	{ "zbrush_vertex_color_0_obj", 13358, -1, 0, 8, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "zbrush_vertex_color_0_obj", 13358, -1, 0, 9, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "zbrush_vertex_color_0_obj", 13394, -1, 0, 62, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "zbrush_vertex_color_0_obj", 13394, -1, 0, 63, 0, 0, 0, "ufbxi_obj_read_line(uc)" },
	{ "zbrush_vertex_color_0_obj", 13395, -1, 0, 8, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "zbrush_vertex_color_0_obj", 13395, -1, 0, 9, 0, 0, 0, "ufbxi_obj_tokenize(uc)" },
	{ "zbrush_vertex_color_0_obj", 13413, 316, 33, 0, 0, 0, 0, "offset + read_values <= uc->obj.num_tokens" },
	{ "zbrush_vertex_color_0_obj", 13416, -1, 0, 16, 0, 0, 0, "vals" },
	{ "zbrush_vertex_color_0_obj", 13416, -1, 0, 17, 0, 0, 0, "vals" },
	{ "zbrush_vertex_color_0_obj", 13422, 315, 47, 0, 0, 0, 0, "end == str.data + str.length" },
	{ "zbrush_vertex_color_0_obj", 13471, -1, 0, 55, 0, 0, 0, "dst" },
	{ "zbrush_vertex_color_0_obj", 13471, -1, 0, 56, 0, 0, 0, "dst" },
	{ "zbrush_vertex_color_0_obj", 13512, -1, 0, 45, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "zbrush_vertex_color_0_obj", 13512, -1, 0, 46, 0, 0, 0, "ufbxi_obj_push_mesh(uc)" },
	{ "zbrush_vertex_color_0_obj", 13519, -1, 0, 52, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "zbrush_vertex_color_0_obj", 13519, -1, 0, 53, 0, 0, 0, "ufbxi_connect_oo(uc, uc->obj.usemtl_fbx_id, mesh->fbx_n..." },
	{ "zbrush_vertex_color_0_obj", 13577, -1, 0, 53, 0, 0, 0, "face" },
	{ "zbrush_vertex_color_0_obj", 13577, -1, 0, 54, 0, 0, 0, "face" },
	{ "zbrush_vertex_color_0_obj", 13586, -1, 0, 54, 0, 0, 0, "p_face_mat" },
	{ "zbrush_vertex_color_0_obj", 13586, -1, 0, 55, 0, 0, 0, "p_face_mat" },
	{ "zbrush_vertex_color_0_obj", 13604, -1, 0, 55, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "zbrush_vertex_color_0_obj", 13604, -1, 0, 56, 0, 0, 0, "ufbxi_obj_parse_index(uc, &tok, attrib)" },
	{ "zbrush_vertex_color_0_obj", 13648, -1, 0, 25, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_0_obj", 13648, -1, 0, 26, 0, 0, 0, "p_rgba && p_valid" },
	{ "zbrush_vertex_color_0_obj", 13674, -1, 0, 0, 0, 300, 0, "uc->obj.num_tokens >= 2" },
	{ "zbrush_vertex_color_0_obj", 13693, -1, 0, 13, 0, 0, 0, "material" },
	{ "zbrush_vertex_color_0_obj", 13693, -1, 0, 14, 0, 0, 0, "material" },
	{ "zbrush_vertex_color_0_obj", 13700, -1, 0, 15, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "zbrush_vertex_color_0_obj", 13700, -1, 0, 16, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->o..." },
	{ "zbrush_vertex_color_0_obj", 13741, 884, 33, 0, 0, 0, 0, "min_index < uc->obj.tmp_vertices[attrib].num_items / st..." },
	{ "zbrush_vertex_color_0_obj", 13745, -1, 0, 0, 1, 0, 0, "data" },
	{ "zbrush_vertex_color_0_obj", 13783, -1, 0, 0, 6, 0, 0, "dst_indices" },
	{ "zbrush_vertex_color_0_obj", 13816, -1, 0, 64, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_0_obj", 13816, -1, 0, 65, 0, 0, 0, "((ufbx_real*)ufbxi_push_size_zero((&uc->obj.tmp_vertice..." },
	{ "zbrush_vertex_color_0_obj", 13828, -1, 0, 63, 0, 0, 0, "meshes" },
	{ "zbrush_vertex_color_0_obj", 13828, -1, 0, 64, 0, 0, 0, "meshes" },
	{ "zbrush_vertex_color_0_obj", 13831, -1, 0, 64, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_0_obj", 13831, -1, 0, 65, 0, 0, 0, "ufbxi_obj_pad_colors(uc, uc->obj.vertex_count[UFBXI_OBJ..." },
	{ "zbrush_vertex_color_0_obj", 13861, -1, 0, 65, 0, 0, 0, "tmp_indices" },
	{ "zbrush_vertex_color_0_obj", 13861, -1, 0, 66, 0, 0, 0, "tmp_indices" },
	{ "zbrush_vertex_color_0_obj", 13885, 884, 33, 0, 0, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[attrib], attrib, m..." },
	{ "zbrush_vertex_color_0_obj", 13890, -1, 0, 0, 0, 880, 0, "min_ix < 0xffffffffffffffffui64" },
	{ "zbrush_vertex_color_0_obj", 13891, -1, 0, 0, 3, 0, 0, "ufbxi_obj_pop_vertices(uc, &vertices[UFBXI_OBJ_ATTRIB_C..." },
	{ "zbrush_vertex_color_0_obj", 13893, -1, 0, 66, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_0_obj", 13893, -1, 0, 67, 0, 0, 0, "color_valid" },
	{ "zbrush_vertex_color_0_obj", 13902, -1, 0, 0, 4, 0, 0, "fbx_mesh->faces.data" },
	{ "zbrush_vertex_color_0_obj", 13903, -1, 0, 0, 5, 0, 0, "fbx_mesh->face_material.data" },
	{ "zbrush_vertex_color_0_obj", 13918, -1, 0, 0, 6, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "zbrush_vertex_color_0_obj", 13921, -1, 0, 0, 7, 0, 0, "ufbxi_obj_setup_attrib(uc, mesh, tmp_indices, (ufbx_ver..." },
	{ "zbrush_vertex_color_0_obj", 13966, -1, 0, 0, 8, 0, 0, "ufbxi_finalize_mesh(&uc->result, &uc->error, fbx_mesh)" },
	{ "zbrush_vertex_color_0_obj", 13986, -1, 0, 3, 0, 0, 0, "root" },
	{ "zbrush_vertex_color_0_obj", 13986, -1, 0, 4, 0, 0, 0, "root" },
	{ "zbrush_vertex_color_0_obj", 13988, -1, 0, 7, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "zbrush_vertex_color_0_obj", 13988, -1, 0, 8, 0, 0, 0, "((uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), s..." },
	{ "zbrush_vertex_color_0_obj", 13992, -1, 0, 8, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "zbrush_vertex_color_0_obj", 13992, -1, 0, 9, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "zbrush_vertex_color_0_obj", 13999, 315, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_POSITION, 1..." },
	{ "zbrush_vertex_color_0_obj", 14006, 338, 9, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_COLOR, 4)" },
	{ "zbrush_vertex_color_0_obj", 14013, 741, 47, 0, 0, 0, 0, "ufbxi_obj_parse_vertex(uc, UFBXI_OBJ_ATTRIB_UV, 1)" },
	{ "zbrush_vertex_color_0_obj", 14017, -1, 0, 45, 0, 0, 0, "ufbxi_obj_parse_indices(uc)" },
	{ "zbrush_vertex_color_0_obj", 14017, -1, 0, 46, 0, 0, 0, "ufbxi_obj_parse_indices(uc)" },
	{ "zbrush_vertex_color_0_obj", 14041, -1, 0, 25, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_0_obj", 14041, -1, 0, 26, 0, 0, 0, "ufbxi_obj_parse_comment(uc)" },
	{ "zbrush_vertex_color_0_obj", 14043, -1, 0, 0, 0, 268, 0, "uc->obj.num_tokens >= 2" },
	{ "zbrush_vertex_color_0_obj", 14046, -1, 0, 12, 0, 0, 0, "lib.data" },
	{ "zbrush_vertex_color_0_obj", 14046, -1, 0, 13, 0, 0, 0, "lib.data" },
	{ "zbrush_vertex_color_0_obj", 14050, -1, 0, 13, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "zbrush_vertex_color_0_obj", 14050, -1, 0, 14, 0, 0, 0, "ufbxi_obj_parse_material(uc)" },
	{ "zbrush_vertex_color_0_obj", 14055, 884, 33, 0, 0, 0, 0, "ufbxi_obj_pop_meshes(uc)" },
	{ "zbrush_vertex_color_0_obj", 14069, -1, 0, 78, 0, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "zbrush_vertex_color_0_obj", 14069, -1, 0, 79, 0, 0, 0, "ufbxi_obj_pop_props(uc, &material->props.props, num_pro..." },
	{ "zbrush_vertex_color_0_obj", 14084, -1, 0, 69, 0, 0, 0, "prop" },
	{ "zbrush_vertex_color_0_obj", 14084, -1, 0, 70, 0, 0, 0, "prop" },
	{ "zbrush_vertex_color_0_obj", 14087, -1, 0, 0, 14, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->na..." },
	{ "zbrush_vertex_color_0_obj", 14114, -1, 0, 0, 15, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop->va..." },
	{ "zbrush_vertex_color_0_obj", 14143, -1, 0, 72, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_0_obj", 14143, -1, 0, 73, 0, 0, 0, "ufbxi_obj_parse_prop(uc, ufbxi_str_c(\"obj|args\"), 1, ..." },
	{ "zbrush_vertex_color_0_obj", 14165, -1, 0, 73, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_0_obj", 14165, -1, 0, 74, 0, 0, 0, "texture" },
	{ "zbrush_vertex_color_0_obj", 14174, -1, 0, 0, 20, 0, 0, "ufbxi_obj_pop_props(uc, &texture->props.props, num_prop..." },
	{ "zbrush_vertex_color_0_obj", 14180, -1, 0, 0, 21, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop, 0)" },
	{ "zbrush_vertex_color_0_obj", 14183, -1, 0, 76, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_0_obj", 14183, -1, 0, 77, 0, 0, 0, "ufbxi_connect_op(uc, fbx_id, uc->obj.usemtl_fbx_id, pro..." },
	{ "zbrush_vertex_color_0_obj", 14195, -1, 0, 68, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "zbrush_vertex_color_0_obj", 14195, -1, 0, 69, 0, 0, 0, "ufbxi_obj_tokenize_line(uc)" },
	{ "zbrush_vertex_color_0_obj", 14205, -1, 0, 72, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
	{ "zbrush_vertex_color_0_obj", 14205, -1, 0, 73, 0, 0, 0, "ufbxi_obj_parse_mtl_map(uc, 4)" },
	{ "zbrush_vertex_color_0_obj", 14209, -1, 0, 69, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "zbrush_vertex_color_0_obj", 14209, -1, 0, 70, 0, 0, 0, "ufbxi_obj_parse_prop(uc, uc->obj.tokens[0], 1, 1, ((voi..." },
	{ "zbrush_vertex_color_0_obj", 14213, -1, 0, 78, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "zbrush_vertex_color_0_obj", 14213, -1, 0, 79, 0, 0, 0, "ufbxi_obj_flush_material(uc)" },
	{ "zbrush_vertex_color_0_obj", 14252, -1, 0, 67, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "zbrush_vertex_color_0_obj", 14252, -1, 0, 68, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, (ufbxi_strblob*)&ds..." },
	{ "zbrush_vertex_color_0_obj", 14267, -1, 0, 68, 0, 0, 0, "ok" },
	{ "zbrush_vertex_color_0_obj", 14267, -1, 0, 69, 0, 0, 0, "ok" },
	{ "zbrush_vertex_color_0_obj", 14276, 315, 47, 0, 0, 0, 0, "ufbxi_obj_parse_file(uc)" },
	{ "zbrush_vertex_color_0_obj", 14277, -1, 0, 0, 11, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "zbrush_vertex_color_0_obj", 14278, -1, 0, 67, 0, 0, 0, "ufbxi_obj_load_mtl(uc)" },
	{ "zbrush_vertex_color_0_obj", 14278, -1, 0, 68, 0, 0, 0, "ufbxi_obj_load_mtl(uc)" },
	{ "zbrush_vertex_color_0_obj", 14452, -1, 0, 80, 0, 0, 0, "tmp_connections" },
	{ "zbrush_vertex_color_0_obj", 14452, -1, 0, 81, 0, 0, 0, "tmp_connections" },
	{ "zbrush_vertex_color_0_obj", 14628, -1, 0, 81, 0, 0, 0, "node_ids" },
	{ "zbrush_vertex_color_0_obj", 14628, -1, 0, 82, 0, 0, 0, "node_ids" },
	{ "zbrush_vertex_color_0_obj", 14631, -1, 0, 82, 0, 0, 0, "node_ptrs" },
	{ "zbrush_vertex_color_0_obj", 14631, -1, 0, 83, 0, 0, 0, "node_ptrs" },
	{ "zbrush_vertex_color_0_obj", 14642, -1, 0, 83, 0, 0, 0, "node_offsets" },
	{ "zbrush_vertex_color_0_obj", 14642, -1, 0, 84, 0, 0, 0, "node_offsets" },
	{ "zbrush_vertex_color_0_obj", 14687, -1, 0, 84, 0, 0, 0, "p_offset" },
	{ "zbrush_vertex_color_0_obj", 14687, -1, 0, 85, 0, 0, 0, "p_offset" },
	{ "zbrush_vertex_color_0_obj", 14754, -1, 0, 89, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "zbrush_vertex_color_0_obj", 14754, -1, 0, 90, 0, 0, 0, "((ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack),..." },
	{ "zbrush_vertex_color_0_obj", 16695, -1, 0, 79, 0, 0, 0, "element_offsets" },
	{ "zbrush_vertex_color_0_obj", 16695, -1, 0, 80, 0, 0, 0, "element_offsets" },
	{ "zbrush_vertex_color_0_obj", 16714, -1, 0, 85, 0, 0, 0, "typed_offsets" },
	{ "zbrush_vertex_color_0_obj", 16714, -1, 0, 86, 0, 0, 0, "typed_offsets" },
	{ "zbrush_vertex_color_0_obj", 19836, 315, 47, 0, 0, 0, 0, "ufbxi_obj_load(uc)" },
	{ "zbrush_vertex_color_0_obj", 5168, -1, 0, 68, 0, 0, 0, "new_buffer" },
	{ "zbrush_vertex_color_0_obj", 5168, -1, 0, 69, 0, 0, 0, "new_buffer" },
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

void ufbxt_do_fuzz(const char *base_name, void *data, size_t size, const char *filename, bool allow_error, ufbx_file_format file_format)
{
	size_t temp_allocs = 1000;
	size_t result_allocs = 500;
	size_t progress_calls = 100;

	{
		ufbxt_progress_ctx progress_ctx = { 0 };

		bool temp_freed = false, result_freed = false;

		ufbx_load_opts prog_opts = { 0 };
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

			if (!ufbxt_test_fuzz(filename, data, size, step, -1, (size_t)i, 0, 0, 0)) fail_step = step;
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

			if (!ufbxt_test_fuzz(filename, data, size, step, -1, 0, (size_t)i, 0, 0)) fail_step = step;
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

				if (!ufbxt_test_fuzz(filename, data, size, step, -1, 0, 0, (size_t)i, 0)) fail_step = step;
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
		if (ufbx_open_file(NULL, &stream, buffer, SIZE_MAX)) {
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
		if (!alternative) {
			ufbxt_do_fuzz(base_name, data, size, buf, allow_error, UFBX_FILE_FORMAT_UNKNOWN);
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
			if (g_fuzz && !g_fuzz_no_buffer && g_fuzz_step == SIZE_MAX && !alternative) {
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

			if (!alternative) {
				ufbxt_do_fuzz(base_name, data, size, buf, allow_error, UFBX_FILE_FORMAT_UNKNOWN);
			}

			if (!alternative && scene) {
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

