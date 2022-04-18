#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr);

#include "../ufbx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#ifndef USE_SETJMP
#if !defined(__wasm__)
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

// -- Thread local

#ifdef _MSC_VER
	#define ufbxt_threadlocal __declspec(thread)
#else
	#define ufbxt_threadlocal __thread
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
	ufbxt_logf("Error: %s", err->description);
	for (size_t i = 0; i < err->stack_size; i++) {
		ufbx_error_frame *f = &err->stack[i];
		ufbxt_logf("Line %u %s: %s", f->source_line, f->function, f->description);
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
	free(ator);
}

char data_root[256];

static uint32_t g_file_version = 0;
static const char *g_file_type = NULL;
static bool g_fuzz = false;
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

void ufbxt_init_allocator(ufbx_allocator_opts *ator)
{
	ator->memory_limit = 0x4000000; // 64MB

	if (g_dedicated_allocs) return;

	ufbxt_allocator *at = (ufbxt_allocator*)malloc(sizeof(ufbxt_allocator));
	ufbxt_assert(at);
	at->offset = 0;
	at->bytes_allocated = 0;

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

	t_jmp_buf = (ufbxt_jmp_buf*)calloc(1, sizeof(ufbxt_jmp_buf));
	int ret = 1;
	if (!ufbxt_setjmp(*t_jmp_buf)) {

		ufbx_load_opts opts = { 0 };
		ufbxt_cancel_ctx cancel_ctx = { 0 };

		opts.load_external_files = true;
		opts.filename.data = filename;
		opts.filename.length = SIZE_MAX;

		ufbxt_init_allocator(&opts.temp_allocator);
		ufbxt_init_allocator(&opts.result_allocator);

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

	} else {
		ret = 0;
	}

	free(t_jmp_buf);
	t_jmp_buf = NULL;

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
// From commit a47a0535
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "blender_279_ball_6100_ascii", 11115, -1, 0, 0, 175, 0, 0, "mat->face_indices" },
	{ "blender_279_ball_6100_ascii", 7297, 18422, 84, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_smoothing, n..." },
	{ "blender_279_ball_6100_ascii", 7304, 18755, 78, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material, n,..." },
	{ "blender_279_default_6100_ascii", 8869, 454, 14, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "blender_279_edge_vertex_7400_binary", 10554, -1, 0, 0, 96, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "blender_279_sausage_6100_ascii", 10049, -1, 0, 10853, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "blender_279_sausage_6100_ascii", 10863, -1, 0, 10849, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_6100_ascii", 10863, -1, 0, 10883, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_6100_ascii", 10906, -1, 0, 0, 340, 0, 0, "skin->vertices.data" },
	{ "blender_279_sausage_6100_ascii", 10910, -1, 0, 0, 341, 0, 0, "skin->weights.data" },
	{ "blender_279_sausage_6100_ascii", 11135, -1, 0, 10852, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "blender_279_sausage_6100_ascii", 11135, -1, 0, 10890, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "blender_279_sausage_6100_ascii", 11138, -1, 0, 10853, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "blender_279_sausage_6100_ascii", 6624, -1, 0, 8773, 0, 0, 0, "entry" },
	{ "blender_279_sausage_6100_ascii", 6624, -1, 0, 8792, 0, 0, 0, "entry" },
	{ "blender_279_sausage_6100_ascii", 7622, -1, 0, 6328, 0, 0, 0, "skin" },
	{ "blender_279_sausage_6100_ascii", 7622, -1, 0, 6337, 0, 0, 0, "skin" },
	{ "blender_279_sausage_6100_ascii", 7652, -1, 0, 6526, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_6100_ascii", 7652, -1, 0, 6535, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_6100_ascii", 8389, -1, 0, 6328, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_6100_ascii", 8389, -1, 0, 6337, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_6100_ascii", 8391, -1, 0, 6526, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "blender_279_sausage_6100_ascii", 8391, -1, 0, 6535, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "blender_279_sausage_6100_ascii", 9564, -1, 0, 10855, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "blender_279_sausage_6100_ascii", 9666, -1, 0, 10855, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "blender_279_sausage_7400_binary", 10049, -1, 0, 4351, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "blender_279_sausage_7400_binary", 11138, -1, 0, 4351, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "blender_279_sausage_7400_binary", 7658, 23076, 0, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "blender_279_sausage_7400_binary", 7667, 23900, 0, 0, 0, 0, 0, "transform->size >= 16" },
	{ "blender_279_sausage_7400_binary", 7668, 24063, 0, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "blender_279_sausage_7400_binary", 8051, 21748, 0, 0, 0, 0, 0, "matrix->size >= 16" },
	{ "blender_279_uv_sets_6100_ascii", 11386, -1, 0, 3800, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 11386, -1, 0, 3809, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 11393, -1, 0, 3801, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 11393, -1, 0, 3810, 0, 0, 0, "mat_texs" },
	{ "blender_279_uv_sets_6100_ascii", 4017, -1, 0, 715, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 4017, -1, 0, 721, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 4021, -1, 0, 716, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 4021, -1, 0, 722, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "blender_279_uv_sets_6100_ascii", 7339, -1, 0, 0, 46, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 7345, -1, 0, 714, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 7345, -1, 0, 720, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 7429, -1, 0, 715, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 7429, -1, 0, 721, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 7432, -1, 0, 717, 0, 0, 0, "extra->texture_arr" },
	{ "blender_279_uv_sets_6100_ascii", 7432, -1, 0, 723, 0, 0, 0, "extra->texture_arr" },
	{ "blender_293_half_skinned_7400_binary", 10057, -1, 0, 0, 138, 0, 0, "list->data" },
	{ "fuzz_0018", 9449, 810, 0, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "fuzz_0272", 6687, -1, 0, 449, 0, 0, 0, "unknown" },
	{ "fuzz_0272", 6687, -1, 0, 452, 0, 0, 0, "unknown" },
	{ "fuzz_0272", 8446, -1, 0, 449, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0272", 8446, -1, 0, 452, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0393", 6843, -1, 0, 0, 99, 0, 0, "index_data" },
	{ "fuzz_0397", 6713, -1, 0, 0, 99, 0, 0, "new_indices" },
	{ "fuzz_0491", 10764, -1, 0, 25, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "fuzz_0491", 10764, -1, 0, 26, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "fuzz_0491", 9508, -1, 0, 25, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 9508, -1, 0, 26, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 9528, -1, 0, 22, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 9528, -1, 0, 23, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 9859, -1, 0, 22, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "fuzz_0491", 9859, -1, 0, 23, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "fuzz_0561", 8385, -1, 0, 450, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "fuzz_0561", 8385, -1, 0, 453, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "max2009_blob_5800_ascii", 5575, -1, 0, 4401, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 5575, -1, 0, 4407, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 5578, -1, 0, 0, 90, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v)" },
	{ "max2009_blob_5800_ascii", 5621, 131240, 45, 0, 0, 0, 0, "Bad array dst type" },
	{ "max2009_blob_5800_ascii", 5920, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "max2009_blob_5800_binary", 4654, -1, 0, 0, 0, 80100, 0, "val" },
	{ "max2009_blob_5800_binary", 9058, -1, 0, 565, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 9058, -1, 0, 571, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 9066, -1, 0, 0, 110, 0, 0, "material->props.props" },
	{ "max2009_blob_5800_binary", 9103, -1, 0, 105, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 9103, -1, 0, 111, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 9110, -1, 0, 0, 26, 0, 0, "light->props.props" },
	{ "max2009_blob_5800_binary", 9118, -1, 0, 304, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 9118, -1, 0, 310, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 9125, -1, 0, 0, 69, 0, 0, "camera->props.props" },
	{ "max2009_blob_5800_binary", 9300, 56700, 78, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material, no..." },
	{ "max2009_blob_5800_binary", 9327, 6207, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 9328, 6229, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 9329, -1, 0, 565, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 9329, -1, 0, 571, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 9330, -1, 0, 567, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 9330, -1, 0, 573, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max2009_blob_5800_binary", 9368, -1, 0, 358, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "max2009_blob_5800_binary", 9368, -1, 0, 364, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "max2009_blob_5800_binary", 9382, -1, 0, 105, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 9382, -1, 0, 111, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 9384, -1, 0, 304, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 9384, -1, 0, 310, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 9451, 113382, 0, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "max7_blend_cube_5000_binary", 7023, -1, 0, 309, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 7023, -1, 0, 315, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 9160, 2350, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "max7_cube_5000_binary", 4656, 1869, 2, 0, 0, 0, 0, "type == 'S' || type == 'R'" },
	{ "max7_cube_5000_binary", 4665, 1888, 255, 0, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d)" },
	{ "max7_cube_5000_binary", 9158, -1, 0, 272, 0, 0, 0, "mesh" },
	{ "max7_cube_5000_binary", 9158, -1, 0, 278, 0, 0, 0, "mesh" },
	{ "max7_cube_5000_binary", 9177, 2383, 23, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "max7_cube_5000_binary", 9206, -1, 0, 0, 23, 0, 0, "mesh->faces" },
	{ "max7_cube_5000_binary", 9227, 2383, 0, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "max7_cube_5000_binary", 9235, -1, 0, 0, 24, 0, 0, "mesh->vertex_first_index" },
	{ "max7_cube_5000_binary", 9283, -1, 0, 0, 25, 0, 0, "set" },
	{ "max7_cube_5000_binary", 9290, 2927, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, uv_info, &set->vert..." },
	{ "max7_cube_5000_binary", 9298, 2856, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MaterialAssignation, \"C\",..." },
	{ "max7_cube_5000_binary", 9359, 324, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"s\", &type_and_name)" },
	{ "max7_cube_5000_binary", 9360, 343, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max7_cube_5000_binary", 9367, -1, 0, 132, 0, 0, 0, "elem_node" },
	{ "max7_cube_5000_binary", 9367, -1, 0, 138, 0, 0, 0, "elem_node" },
	{ "max7_cube_5000_binary", 9375, -1, 0, 133, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 9375, -1, 0, 139, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 9388, 2383, 23, 0, 0, 0, 0, "ufbxi_read_legacy_mesh(uc, node, &attrib_info)" },
	{ "max7_cube_5000_binary", 9397, -1, 0, 274, 0, 0, 0, "entry" },
	{ "max7_cube_5000_binary", 9397, -1, 0, 280, 0, 0, 0, "entry" },
	{ "max7_cube_5000_binary", 9408, -1, 0, 134, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 9408, -1, 0, 140, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 9421, 942, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc..." },
	{ "max7_cube_5000_binary", 9432, -1, 0, 3, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max7_cube_5000_binary", 9432, -1, 0, 4, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max7_cube_5000_binary", 9439, -1, 0, 4, 0, 0, 0, "root" },
	{ "max7_cube_5000_binary", 9439, -1, 0, 5, 0, 0, 0, "root" },
	{ "max7_cube_5000_binary", 9440, -1, 0, 14, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "max7_cube_5000_binary", 9440, -1, 0, 8, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "max7_cube_5000_binary", 9455, 324, 0, 0, 0, 0, 0, "ufbxi_read_legacy_model(uc, node)" },
	{ "max7_cube_5000_binary", 9467, -1, 0, 1209, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 9467, -1, 0, 1216, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 9472, -1, 0, 1212, 0, 0, 0, "stack" },
	{ "max7_cube_5000_binary", 9472, -1, 0, 1219, 0, 0, 0, "stack" },
	{ "max7_cube_5000_binary", 9474, -1, 0, 1214, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "max7_cube_5000_binary", 9474, -1, 0, 1221, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "max7_skin_5000_binary", 9074, -1, 0, 335, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 9074, -1, 0, 342, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 9081, 2420, 136, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "max7_skin_5000_binary", 9090, 4378, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "max7_skin_5000_binary", 9091, 4544, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "max7_skin_5000_binary", 9133, -1, 0, 484, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 9133, -1, 0, 491, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 9145, -1, 0, 0, 36, 0, 0, "bone->props.props" },
	{ "max7_skin_5000_binary", 9334, 2361, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max7_skin_5000_binary", 9335, 2379, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max7_skin_5000_binary", 9336, 2420, 136, 0, 0, 0, 0, "ufbxi_read_legacy_link(uc, child, &fbx_id, name.data)" },
	{ "max7_skin_5000_binary", 9339, -1, 0, 338, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 9339, -1, 0, 345, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 9342, -1, 0, 339, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 9342, -1, 0, 346, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 9343, -1, 0, 341, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 9343, -1, 0, 348, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 9345, -1, 0, 342, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 9345, -1, 0, 349, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_skin_5000_binary", 9386, -1, 0, 484, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max7_skin_5000_binary", 9386, -1, 0, 491, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max_cache_box_7500_binary", 12725, -1, 0, 653, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 12725, -1, 0, 658, 0, 0, 0, "frames" },
	{ "max_cache_box_7500_binary", 12889, -1, 0, 653, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_cache_box_7500_binary", 12889, -1, 0, 658, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "max_curve_line_7500_ascii", 7548, 8302, 43, 0, 0, 0, 0, "points->size % 3 == 0" },
	{ "max_curve_line_7500_binary", 7541, -1, 0, 425, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 7541, -1, 0, 427, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 7546, 13861, 255, 0, 0, 0, 0, "points" },
	{ "max_curve_line_7500_binary", 7547, 13985, 56, 0, 0, 0, 0, "points_index" },
	{ "max_curve_line_7500_binary", 8379, 13861, 255, 0, 0, 0, 0, "ufbxi_read_line(uc, node, &info)" },
	{ "max_selection_sets_6100_binary", 11538, -1, 0, 834, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "max_selection_sets_6100_binary", 11538, -1, 0, 842, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "max_selection_sets_6100_binary", 8122, -1, 0, 537, 0, 0, 0, "set" },
	{ "max_selection_sets_6100_binary", 8122, -1, 0, 545, 0, 0, 0, "set" },
	{ "max_selection_sets_6100_binary", 8139, -1, 0, 410, 0, 0, 0, "sel" },
	{ "max_selection_sets_6100_binary", 8139, -1, 0, 416, 0, 0, 0, "sel" },
	{ "max_selection_sets_6100_binary", 8425, -1, 0, 537, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "max_selection_sets_6100_binary", 8425, -1, 0, 545, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "max_selection_sets_6100_binary", 8432, -1, 0, 410, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "max_selection_sets_6100_binary", 8432, -1, 0, 416, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "max_transformed_skin_6100_binary", 8636, 64699, 7, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_arnold_textures_6100_binary", 11298, -1, 0, 1733, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_arnold_textures_6100_binary", 11298, -1, 0, 1754, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_arnold_textures_6100_binary", 8081, -1, 0, 1498, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 8081, -1, 0, 1518, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 8095, -1, 0, 1500, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 8095, -1, 0, 1520, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 8110, -1, 0, 0, 241, 0, 0, "bindings->prop_bindings.data" },
	{ "maya_arnold_textures_6100_binary", 8420, -1, 0, 1334, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 8420, -1, 0, 1350, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 8422, -1, 0, 1498, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_arnold_textures_6100_binary", 8422, -1, 0, 1518, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_auto_clamp_7100_ascii", 5641, -1, 0, 711, 0, 0, 0, "v" },
	{ "maya_auto_clamp_7100_ascii", 5641, -1, 0, 715, 0, 0, 0, "v" },
	{ "maya_blend_shape_cube_6100_binary", 10070, -1, 0, 676, 0, 0, 0, "(ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_blend_shape_cube_6100_binary", 10070, -1, 0, 684, 0, 0, 0, "(ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_blend_shape_cube_6100_binary", 10077, -1, 0, 0, 121, 0, 0, "list->data" },
	{ "maya_blend_shape_cube_6100_binary", 10971, -1, 0, 674, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "maya_blend_shape_cube_6100_binary", 10971, -1, 0, 682, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "maya_blend_shape_cube_6100_binary", 10996, -1, 0, 675, 0, 0, 0, "full_weights" },
	{ "maya_blend_shape_cube_6100_binary", 10996, -1, 0, 683, 0, 0, 0, "full_weights" },
	{ "maya_blend_shape_cube_6100_binary", 11001, -1, 0, 676, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "maya_blend_shape_cube_6100_binary", 11001, -1, 0, 684, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "maya_blend_shape_cube_6100_binary", 11136, -1, 0, 679, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "maya_blend_shape_cube_6100_binary", 11136, -1, 0, 687, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "maya_blend_shape_cube_6100_binary", 6675, -1, 0, 377, 0, 0, 0, "conn" },
	{ "maya_blend_shape_cube_6100_binary", 6675, -1, 0, 383, 0, 0, 0, "conn" },
	{ "maya_blend_shape_cube_6100_binary", 6932, -1, 0, 378, 0, 0, 0, "shape" },
	{ "maya_blend_shape_cube_6100_binary", 6932, -1, 0, 384, 0, 0, 0, "shape" },
	{ "maya_blend_shape_cube_6100_binary", 6942, 9533, 11, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_blend_shape_cube_6100_binary", 6943, 9493, 3, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "maya_blend_shape_cube_6100_binary", 6993, 9466, 0, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "maya_blend_shape_cube_6100_binary", 6997, -1, 0, 370, 0, 0, 0, "deformer" },
	{ "maya_blend_shape_cube_6100_binary", 6997, -1, 0, 376, 0, 0, 0, "deformer" },
	{ "maya_blend_shape_cube_6100_binary", 6998, -1, 0, 373, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 6998, -1, 0, 379, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 7003, -1, 0, 374, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_6100_binary", 7003, -1, 0, 380, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_6100_binary", 7006, -1, 0, 376, 0, 0, 0, "(ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_we..." },
	{ "maya_blend_shape_cube_6100_binary", 7006, -1, 0, 382, 0, 0, 0, "(ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_we..." },
	{ "maya_blend_shape_cube_6100_binary", 7010, -1, 0, 0, 41, 0, 0, "shape_props" },
	{ "maya_blend_shape_cube_6100_binary", 7021, -1, 0, 377, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "maya_blend_shape_cube_6100_binary", 7021, -1, 0, 383, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "maya_blend_shape_cube_6100_binary", 7038, 9493, 3, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "maya_blend_shape_cube_6100_binary", 7040, -1, 0, 381, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 7040, -1, 0, 387, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 7041, -1, 0, 382, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 7041, -1, 0, 388, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 7054, 9466, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "maya_blend_shape_cube_7100_ascii", 5617, -1, 0, 929, 0, 0, 0, "v" },
	{ "maya_blend_shape_cube_7100_ascii", 5617, -1, 0, 932, 0, 0, 0, "v" },
	{ "maya_blend_shape_cube_7700_binary", 7680, -1, 0, 671, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_7700_binary", 7680, -1, 0, 674, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_7700_binary", 7688, -1, 0, 673, 0, 0, 0, "(ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_we..." },
	{ "maya_blend_shape_cube_7700_binary", 7688, -1, 0, 676, 0, 0, 0, "(ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_we..." },
	{ "maya_blend_shape_cube_7700_binary", 8373, 19502, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "maya_blend_shape_cube_7700_binary", 8393, -1, 0, 654, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "maya_blend_shape_cube_7700_binary", 8393, -1, 0, 657, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "maya_blend_shape_cube_7700_binary", 8395, -1, 0, 671, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "maya_blend_shape_cube_7700_binary", 8395, -1, 0, 674, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "maya_cache_sine_6100_binary", 10990, -1, 0, 1454, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &cache->filename, c..." },
	{ "maya_cache_sine_6100_binary", 10990, -1, 0, 1462, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &cache->filename, c..." },
	{ "maya_cache_sine_6100_binary", 11137, -1, 0, 1457, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 11137, -1, 0, 1465, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 12667, -1, 0, 1578, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->na..." },
	{ "maya_cache_sine_6100_binary", 12667, -1, 0, 1587, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->na..." },
	{ "maya_cache_sine_6100_binary", 12684, -1, 0, 1579, 0, 0, 0, "frame" },
	{ "maya_cache_sine_6100_binary", 12684, -1, 0, 1588, 0, 0, 0, "frame" },
	{ "maya_cache_sine_6100_binary", 12762, -1, 0, 1576, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_cache_sine_6100_binary", 12762, -1, 0, 1585, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_cache_sine_6100_binary", 12784, -1, 0, 1572, 0, 0, 0, "extra" },
	{ "maya_cache_sine_6100_binary", 12784, -1, 0, 1581, 0, 0, 0, "extra" },
	{ "maya_cache_sine_6100_binary", 12786, -1, 0, 0, 182, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra)" },
	{ "maya_cache_sine_6100_binary", 12791, -1, 0, 0, 183, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_cache_sine_6100_binary", 12824, -1, 0, 1575, 0, 0, 0, "cc->channels" },
	{ "maya_cache_sine_6100_binary", 12824, -1, 0, 1583, 0, 0, 0, "cc->channels" },
	{ "maya_cache_sine_6100_binary", 12852, -1, 0, 1576, 0, 0, 0, "ufbxi_cache_sort_tmp_channels(cc, cc->channels, cc->num..." },
	{ "maya_cache_sine_6100_binary", 12852, -1, 0, 1585, 0, 0, 0, "ufbxi_cache_sort_tmp_channels(cc, cc->channels, cc->num..." },
	{ "maya_cache_sine_6100_binary", 12865, -1, 0, 1468, 0, 0, 0, "doc" },
	{ "maya_cache_sine_6100_binary", 12865, -1, 0, 1476, 0, 0, 0, "doc" },
	{ "maya_cache_sine_6100_binary", 12869, -1, 0, 1572, 0, 0, 0, "xml_ok" },
	{ "maya_cache_sine_6100_binary", 12869, -1, 0, 1581, 0, 0, 0, "xml_ok" },
	{ "maya_cache_sine_6100_binary", 12877, -1, 0, 0, 184, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "maya_cache_sine_6100_binary", 12891, -1, 0, 1578, 0, 0, 0, "ufbxi_cache_load_mc(cc)" },
	{ "maya_cache_sine_6100_binary", 12891, -1, 0, 1587, 0, 0, 0, "ufbxi_cache_load_mc(cc)" },
	{ "maya_cache_sine_6100_binary", 12893, -1, 0, 1468, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 12893, -1, 0, 1476, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 12933, -1, 0, 1577, 0, 0, 0, "name_buf" },
	{ "maya_cache_sine_6100_binary", 12933, -1, 0, 1586, 0, 0, 0, "name_buf" },
	{ "maya_cache_sine_6100_binary", 12954, -1, 0, 1578, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename, &found)" },
	{ "maya_cache_sine_6100_binary", 12954, -1, 0, 1587, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename, &found)" },
	{ "maya_cache_sine_6100_binary", 13018, -1, 0, 1640, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_cache_sine_6100_binary", 13018, -1, 0, 1649, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_cache_sine_6100_binary", 13047, -1, 0, 1641, 0, 0, 0, "chan" },
	{ "maya_cache_sine_6100_binary", 13047, -1, 0, 1650, 0, 0, 0, "chan" },
	{ "maya_cache_sine_6100_binary", 13077, -1, 0, 0, 186, 0, 0, "cc->cache.channels.data" },
	{ "maya_cache_sine_6100_binary", 13096, -1, 0, 1467, 0, 0, 0, "filename_data" },
	{ "maya_cache_sine_6100_binary", 13096, -1, 0, 1475, 0, 0, 0, "filename_data" },
	{ "maya_cache_sine_6100_binary", 13103, -1, 0, 1468, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, &found)" },
	{ "maya_cache_sine_6100_binary", 13103, -1, 0, 1476, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, &found)" },
	{ "maya_cache_sine_6100_binary", 13110, -1, 0, 1577, 0, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_cache_sine_6100_binary", 13110, -1, 0, 1586, 0, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_cache_sine_6100_binary", 13115, -1, 0, 0, 185, 0, 0, "cc->cache.frames.data" },
	{ "maya_cache_sine_6100_binary", 13117, -1, 0, 1640, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "maya_cache_sine_6100_binary", 13117, -1, 0, 1649, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "maya_cache_sine_6100_binary", 13118, -1, 0, 1641, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_cache_sine_6100_binary", 13118, -1, 0, 1650, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_cache_sine_6100_binary", 13122, -1, 0, 0, 187, 0, 0, "cc->imp" },
	{ "maya_cache_sine_6100_binary", 13279, -1, 0, 1464, 0, 0, 0, "file" },
	{ "maya_cache_sine_6100_binary", 13279, -1, 0, 1472, 0, 0, 0, "file" },
	{ "maya_cache_sine_6100_binary", 13289, -1, 0, 1466, 0, 0, 0, "files" },
	{ "maya_cache_sine_6100_binary", 13289, -1, 0, 1474, 0, 0, 0, "files" },
	{ "maya_cache_sine_6100_binary", 13297, -1, 0, 1467, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_cache_sine_6100_binary", 13297, -1, 0, 1475, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_cache_sine_6100_binary", 13493, -1, 0, 1464, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_cache_sine_6100_binary", 13493, -1, 0, 1472, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_cache_sine_6100_binary", 3517, -1, 0, 1469, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_cache_sine_6100_binary", 3517, -1, 0, 1477, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_cache_sine_6100_binary", 3552, -1, 0, 1469, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_cache_sine_6100_binary", 3552, -1, 0, 1477, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_cache_sine_6100_binary", 3636, -1, 0, 1507, 0, 0, 0, "ufbxi_xml_push_token_char(xc, c)" },
	{ "maya_cache_sine_6100_binary", 3636, -1, 0, 1515, 0, 0, 0, "ufbxi_xml_push_token_char(xc, c)" },
	{ "maya_cache_sine_6100_binary", 3645, -1, 0, 1478, 0, 0, 0, "dst->data" },
	{ "maya_cache_sine_6100_binary", 3645, -1, 0, 1486, 0, 0, 0, "dst->data" },
	{ "maya_cache_sine_6100_binary", 3657, -1, 0, 1507, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_T..." },
	{ "maya_cache_sine_6100_binary", 3657, -1, 0, 1515, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_T..." },
	{ "maya_cache_sine_6100_binary", 3668, -1, 0, 1475, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3668, -1, 0, 1483, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3673, -1, 0, 1476, 0, 0, 0, "tag->text.data" },
	{ "maya_cache_sine_6100_binary", 3673, -1, 0, 1484, 0, 0, 0, "tag->text.data" },
	{ "maya_cache_sine_6100_binary", 3706, -1, 0, 1469, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_cache_sine_6100_binary", 3706, -1, 0, 1477, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_cache_sine_6100_binary", 3711, -1, 0, 1477, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3711, -1, 0, 1485, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3712, -1, 0, 1478, 0, 0, 0, "ufbxi_xml_read_until(xc, &tag->name, UFBXI_XML_CTYPE_NA..." },
	{ "maya_cache_sine_6100_binary", 3712, -1, 0, 1486, 0, 0, 0, "ufbxi_xml_read_until(xc, &tag->name, UFBXI_XML_CTYPE_NA..." },
	{ "maya_cache_sine_6100_binary", 3728, -1, 0, 1481, 0, 0, 0, "attrib" },
	{ "maya_cache_sine_6100_binary", 3728, -1, 0, 1489, 0, 0, 0, "attrib" },
	{ "maya_cache_sine_6100_binary", 3729, -1, 0, 1482, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->name, UFBXI_XML_CTYPE..." },
	{ "maya_cache_sine_6100_binary", 3729, -1, 0, 1490, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->name, UFBXI_XML_CTYPE..." },
	{ "maya_cache_sine_6100_binary", 3741, -1, 0, 1490, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->value, quote_ctype)" },
	{ "maya_cache_sine_6100_binary", 3741, -1, 0, 1498, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->value, quote_ctype)" },
	{ "maya_cache_sine_6100_binary", 3749, -1, 0, 1485, 0, 0, 0, "tag->attribs" },
	{ "maya_cache_sine_6100_binary", 3749, -1, 0, 1493, 0, 0, 0, "tag->attribs" },
	{ "maya_cache_sine_6100_binary", 3755, -1, 0, 1479, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, tag->name.data)" },
	{ "maya_cache_sine_6100_binary", 3755, -1, 0, 1487, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, tag->name.data)" },
	{ "maya_cache_sine_6100_binary", 3761, -1, 0, 1510, 0, 0, 0, "tag->children" },
	{ "maya_cache_sine_6100_binary", 3761, -1, 0, 1518, 0, 0, 0, "tag->children" },
	{ "maya_cache_sine_6100_binary", 3770, -1, 0, 1468, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3770, -1, 0, 1476, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3776, -1, 0, 1469, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, ((void *)0))" },
	{ "maya_cache_sine_6100_binary", 3776, -1, 0, 1477, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, ((void *)0))" },
	{ "maya_cache_sine_6100_binary", 3782, -1, 0, 1570, 0, 0, 0, "tag->children" },
	{ "maya_cache_sine_6100_binary", 3782, -1, 0, 1578, 0, 0, 0, "tag->children" },
	{ "maya_cache_sine_6100_binary", 3785, -1, 0, 1571, 0, 0, 0, "xc->doc" },
	{ "maya_cache_sine_6100_binary", 3785, -1, 0, 1579, 0, 0, 0, "xc->doc" },
	{ "maya_cache_sine_6100_binary", 8397, -1, 0, 1207, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_cache_sine_6100_binary", 8397, -1, 0, 1215, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_cache_sine_6100_binary", 8442, -1, 0, 1273, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_cache_sine_6100_binary", 8442, -1, 0, 1281, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "maya_character_6100_binary", 8158, -1, 0, 13579, 0, 0, 0, "character" },
	{ "maya_character_6100_binary", 8158, -1, 0, 13687, 0, 0, 0, "character" },
	{ "maya_character_6100_binary", 8435, -1, 0, 13579, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_character_6100_binary", 8435, -1, 0, 13687, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_color_sets_6100_binary", 7223, -1, 0, 0, 52, 0, 0, "mesh->color_sets.data" },
	{ "maya_color_sets_6100_binary", 7276, 7000, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_col..." },
	{ "maya_cone_6100_binary", 7281, 16081, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_cr..." },
	{ "maya_cone_6100_binary", 7284, 15524, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "maya_cone_6100_binary", 7287, 15571, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease, n, u..." },
	{ "maya_constraint_zoo_6100_binary", 10525, -1, 0, 3956, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 10525, -1, 0, 3970, 0, 0, 0, "target" },
	{ "maya_constraint_zoo_6100_binary", 11565, -1, 0, 3956, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 11565, -1, 0, 3970, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 11571, -1, 0, 0, 249, 0, 0, "constraint->targets.data" },
	{ "maya_constraint_zoo_6100_binary", 8184, -1, 0, 3468, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 8184, -1, 0, 3479, 0, 0, 0, "constraint" },
	{ "maya_constraint_zoo_6100_binary", 8437, -1, 0, 3468, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 8437, -1, 0, 3479, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_cube_big_endian_6100_binary", 4517, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 4517, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_6100_binary", 4720, -1, 0, 0, 0, 10701, 0, "val" },
	{ "maya_cube_big_endian_6100_binary", 4800, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 4800, -1, 0, 5, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 5772, -1, 0, 3, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_6100_binary", 5772, -1, 0, 4, 0, 0, 0, "version_word" },
	{ "maya_cube_big_endian_7100_binary", 4584, -1, 0, 452, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 4584, -1, 0, 455, 0, 0, 0, "src" },
	{ "maya_cube_big_endian_7100_binary", 4994, -1, 0, 452, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7100_binary", 4994, -1, 0, 455, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_cube_big_endian_7500_binary", 4791, -1, 0, 4, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_7500_binary", 4791, -1, 0, 5, 0, 0, 0, "header_words" },
	{ "maya_display_layers_6100_binary", 11533, -1, 0, 1674, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_display_layers_6100_binary", 11533, -1, 0, 1684, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "maya_display_layers_6100_binary", 8429, -1, 0, 1525, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_display_layers_6100_binary", 8429, -1, 0, 1534, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "maya_game_sausage_6100_binary", 9847, 48802, 49, 0, 0, 0, 0, "depth <= num_nodes" },
	{ "maya_game_sausage_6100_binary_deform", 8584, 44932, 98, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_interpolation_modes_6100_binary", 8550, 16936, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_interpolation_modes_6100_binary", 8613, 16969, 114, 0, 0, 0, 0, "Unknown slope mode" },
	{ "maya_interpolation_modes_6100_binary", 8617, 16936, 73, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_interpolation_modes_6100_binary", 8643, 16989, 98, 0, 0, 0, 0, "Unknown weight mode" },
	{ "maya_interpolation_modes_6100_binary", 8798, 16706, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "maya_leading_comma_7500_ascii", 10027, -1, 0, 833, 0, 0, 0, "(ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_sta..." },
	{ "maya_leading_comma_7500_ascii", 10027, -1, 0, 836, 0, 0, 0, "(ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_sta..." },
	{ "maya_leading_comma_7500_ascii", 10035, -1, 0, 0, 146, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 10539, -1, 0, 0, 134, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_leading_comma_7500_ascii", 10709, -1, 0, 0, 135, 0, 0, "uc->scene.elements.data" },
	{ "maya_leading_comma_7500_ascii", 10714, -1, 0, 0, 136, 0, 0, "element_data" },
	{ "maya_leading_comma_7500_ascii", 10718, -1, 0, 823, 0, 0, 0, "element_offsets" },
	{ "maya_leading_comma_7500_ascii", 10718, -1, 0, 826, 0, 0, 0, "element_offsets" },
	{ "maya_leading_comma_7500_ascii", 10726, -1, 0, 824, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_leading_comma_7500_ascii", 10726, -1, 0, 827, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_leading_comma_7500_ascii", 10728, -1, 0, 825, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_leading_comma_7500_ascii", 10728, -1, 0, 828, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_leading_comma_7500_ascii", 10734, -1, 0, 829, 0, 0, 0, "typed_offsets" },
	{ "maya_leading_comma_7500_ascii", 10734, -1, 0, 832, 0, 0, 0, "typed_offsets" },
	{ "maya_leading_comma_7500_ascii", 10739, -1, 0, 0, 139, 0, 0, "typed_elems->data" },
	{ "maya_leading_comma_7500_ascii", 10751, -1, 0, 0, 142, 0, 0, "uc->scene.elements_by_name.data" },
	{ "maya_leading_comma_7500_ascii", 10850, -1, 0, 832, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_leading_comma_7500_ascii", 10850, -1, 0, 835, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "maya_leading_comma_7500_ascii", 11021, -1, 0, 0, 144, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_leading_comma_7500_ascii", 11063, -1, 0, 833, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_leading_comma_7500_ascii", 11063, -1, 0, 836, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "maya_leading_comma_7500_ascii", 11168, -1, 0, 834, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_leading_comma_7500_ascii", 11168, -1, 0, 837, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_leading_comma_7500_ascii", 11172, -1, 0, 0, 147, 0, 0, "stack->anim.layers.data" },
	{ "maya_leading_comma_7500_ascii", 11186, -1, 0, 0, 148, 0, 0, "layer_desc" },
	{ "maya_leading_comma_7500_ascii", 11258, -1, 0, 835, 0, 0, 0, "aprop" },
	{ "maya_leading_comma_7500_ascii", 11258, -1, 0, 838, 0, 0, 0, "aprop" },
	{ "maya_leading_comma_7500_ascii", 11262, -1, 0, 0, 149, 0, 0, "layer->anim_props.data" },
	{ "maya_leading_comma_7500_ascii", 11584, -1, 0, 0, 150, 0, 0, "descs" },
	{ "maya_leading_comma_7500_ascii", 1302, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 1337, -1, 0, 86, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 1337, -1, 0, 87, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 13477, -1, 0, 1, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_leading_comma_7500_ascii", 13478, -1, 0, 3, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_leading_comma_7500_ascii", 13478, -1, 0, 4, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_leading_comma_7500_ascii", 13482, 0, 60, 0, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_leading_comma_7500_ascii", 13486, -1, 0, 0, 134, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_leading_comma_7500_ascii", 13487, -1, 0, 823, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_leading_comma_7500_ascii", 13487, -1, 0, 826, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_leading_comma_7500_ascii", 13516, -1, 0, 0, 151, 0, 0, "imp" },
	{ "maya_leading_comma_7500_ascii", 1915, -1, 0, 1, 0, 0, 0, "data" },
	{ "maya_leading_comma_7500_ascii", 2183, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_leading_comma_7500_ascii", 2203, -1, 0, 0, 5, 0, 0, "str" },
	{ "maya_leading_comma_7500_ascii", 3090, -1, 0, 0, 0, 0, 1, "uc->opts.progress_fn(uc->opts.progress_user, &progress)" },
	{ "maya_leading_comma_7500_ascii", 3109, -1, 0, 0, 0, 1, 0, "uc->read_fn" },
	{ "maya_leading_comma_7500_ascii", 3165, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_leading_comma_7500_ascii", 5209, -1, 0, 0, 0, 0, 57, "ufbxi_report_progress(uc)" },
	{ "maya_leading_comma_7500_ascii", 5332, -1, 0, 3, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_leading_comma_7500_ascii", 5332, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_leading_comma_7500_ascii", 5389, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 5389, -1, 0, 4, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 5407, -1, 0, 6, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 5407, -1, 0, 7, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 5434, 288, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_leading_comma_7500_ascii", 5441, 3707, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_leading_comma_7500_ascii", 5450, 292, 0, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_leading_comma_7500_ascii", 5469, 288, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 5481, 2537, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 5487, 168, 0, 0, 0, 0, 0, "depth == 0" },
	{ "maya_leading_comma_7500_ascii", 5495, 0, 60, 0, 0, 0, 0, "Expected a 'Name:' token" },
	{ "maya_leading_comma_7500_ascii", 5497, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_leading_comma_7500_ascii", 5501, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_leading_comma_7500_ascii", 5506, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_leading_comma_7500_ascii", 5506, -1, 0, 5, 0, 0, 0, "node" },
	{ "maya_leading_comma_7500_ascii", 5529, -1, 0, 442, 0, 0, 0, "arr" },
	{ "maya_leading_comma_7500_ascii", 5529, -1, 0, 445, 0, 0, 0, "arr" },
	{ "maya_leading_comma_7500_ascii", 5545, -1, 0, 443, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_leading_comma_7500_ascii", 5545, -1, 0, 446, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_leading_comma_7500_ascii", 5559, 292, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 5590, -1, 0, 0, 5, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &v->s)" },
	{ "maya_leading_comma_7500_ascii", 5614, -1, 0, 676, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5614, -1, 0, 679, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5615, -1, 0, 458, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5615, -1, 0, 461, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5618, -1, 0, 485, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5618, -1, 0, 488, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5642, -1, 0, 444, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5642, -1, 0, 447, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5677, 8927, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'I')" },
	{ "maya_leading_comma_7500_ascii", 5680, 8931, 11, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_leading_comma_7500_ascii", 5700, 8937, 33, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_leading_comma_7500_ascii", 5711, -1, 0, 469, 0, 0, 0, "arr_data" },
	{ "maya_leading_comma_7500_ascii", 5711, -1, 0, 472, 0, 0, 0, "arr_data" },
	{ "maya_leading_comma_7500_ascii", 5724, -1, 0, 8, 0, 0, 0, "node->vals" },
	{ "maya_leading_comma_7500_ascii", 5724, -1, 0, 9, 0, 0, 0, "node->vals" },
	{ "maya_leading_comma_7500_ascii", 5734, 168, 11, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_leading_comma_7500_ascii", 5741, -1, 0, 28, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 5741, -1, 0, 29, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 5758, -1, 0, 0, 0, 1, 0, "header" },
	{ "maya_leading_comma_7500_ascii", 5790, -1, 0, 3, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_leading_comma_7500_ascii", 5790, -1, 0, 4, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_leading_comma_7500_ascii", 5810, 100, 33, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_leading_comma_7500_ascii", 5840, 0, 60, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_leading_comma_7500_ascii", 5854, -1, 0, 5, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 5854, -1, 0, 6, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 5870, 1544, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_leading_comma_7500_ascii", 5878, -1, 0, 131, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 5878, -1, 0, 132, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 5895, 100, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_pars..." },
	{ "maya_leading_comma_7500_ascii", 6179, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_leading_comma_7500_ascii", 6185, -1, 0, 2, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 6185, -1, 0, 3, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 6198, 561, 0, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_leading_comma_7500_ascii", 6201, 587, 0, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_leading_comma_7500_ascii", 6253, -1, 0, 84, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 6253, -1, 0, 85, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 6277, -1, 0, 0, 28, 0, 0, "props->props" },
	{ "maya_leading_comma_7500_ascii", 6280, 561, 0, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_leading_comma_7500_ascii", 6284, -1, 0, 84, 0, 0, 0, "ufbxi_sort_properties(uc, props->props, props->num_prop..." },
	{ "maya_leading_comma_7500_ascii", 6284, -1, 0, 85, 0, 0, 0, "ufbxi_sort_properties(uc, props->props, props->num_prop..." },
	{ "maya_leading_comma_7500_ascii", 6307, 561, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_leading_comma_7500_ascii", 6319, 100, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 6336, 561, 0, 0, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_leading_comma_7500_ascii", 6448, 2615, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 6467, 3021, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object)" },
	{ "maya_leading_comma_7500_ascii", 6474, -1, 0, 164, 0, 0, 0, "tmpl" },
	{ "maya_leading_comma_7500_ascii", 6474, -1, 0, 165, 0, 0, 0, "tmpl" },
	{ "maya_leading_comma_7500_ascii", 6475, 3061, 33, 0, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_leading_comma_7500_ascii", 6481, 3159, 0, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_leading_comma_7500_ascii", 6493, -1, 0, 0, 32, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_leading_comma_7500_ascii", 6496, 3203, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_leading_comma_7500_ascii", 6502, -1, 0, 0, 101, 0, 0, "uc->templates" },
	{ "maya_leading_comma_7500_ascii", 6566, -1, 0, 0, 123, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name)" },
	{ "maya_leading_comma_7500_ascii", 6567, 8892, 0, 0, 0, 0, 0, "ufbxi_check_string(*type)" },
	{ "maya_leading_comma_7500_ascii", 6579, -1, 0, 147, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_o..." },
	{ "maya_leading_comma_7500_ascii", 6579, -1, 0, 148, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_o..." },
	{ "maya_leading_comma_7500_ascii", 6580, -1, 0, 148, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_element_offsets..." },
	{ "maya_leading_comma_7500_ascii", 6580, -1, 0, 149, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_element_offsets..." },
	{ "maya_leading_comma_7500_ascii", 6584, -1, 0, 149, 0, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", 6584, -1, 0, 150, 0, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", 6593, -1, 0, 150, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 6593, -1, 0, 151, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 6638, -1, 0, 759, 0, 0, 0, "elem_node" },
	{ "maya_leading_comma_7500_ascii", 6638, -1, 0, 762, 0, 0, 0, "elem_node" },
	{ "maya_leading_comma_7500_ascii", 6647, -1, 0, 803, 0, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", 6647, -1, 0, 806, 0, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", 6763, 9370, 43, 0, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_leading_comma_7500_ascii", 6774, 9278, 78, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MappingInformationType, \"C..." },
	{ "maya_leading_comma_7500_ascii", 6829, 10556, 67, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_leading_comma_7500_ascii", 6864, 9303, 67, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_leading_comma_7500_ascii", 6874, 10999, 84, 0, 0, 0, 0, "arr" },
	{ "maya_leading_comma_7500_ascii", 7050, -1, 0, 734, 0, 0, 0, "mesh" },
	{ "maya_leading_comma_7500_ascii", 7050, -1, 0, 737, 0, 0, 0, "mesh" },
	{ "maya_leading_comma_7500_ascii", 7072, 8911, 33, 0, 0, 0, 0, "node_vertices" },
	{ "maya_leading_comma_7500_ascii", 7073, 8930, 122, 0, 0, 0, 0, "node_indices" },
	{ "maya_leading_comma_7500_ascii", 7079, 8926, 43, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_leading_comma_7500_ascii", 7106, -1, 0, 0, 115, 0, 0, "edges" },
	{ "maya_leading_comma_7500_ascii", 7145, -1, 0, 0, 116, 0, 0, "mesh->faces" },
	{ "maya_leading_comma_7500_ascii", 7171, 9073, 43, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_leading_comma_7500_ascii", 7181, -1, 0, 0, 117, 0, 0, "mesh->vertex_first_index" },
	{ "maya_leading_comma_7500_ascii", 7217, -1, 0, 736, 0, 0, 0, "bitangents" },
	{ "maya_leading_comma_7500_ascii", 7217, -1, 0, 739, 0, 0, 0, "bitangents" },
	{ "maya_leading_comma_7500_ascii", 7218, -1, 0, 737, 0, 0, 0, "tangents" },
	{ "maya_leading_comma_7500_ascii", 7218, -1, 0, 740, 0, 0, 0, "tangents" },
	{ "maya_leading_comma_7500_ascii", 7222, -1, 0, 0, 118, 0, 0, "mesh->uv_sets.data" },
	{ "maya_leading_comma_7500_ascii", 7232, 9278, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_no..." },
	{ "maya_leading_comma_7500_ascii", 7239, 9692, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.dat..." },
	{ "maya_leading_comma_7500_ascii", 7248, 10114, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.dat..." },
	{ "maya_leading_comma_7500_ascii", 7263, 10531, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_uv...." },
	{ "maya_leading_comma_7500_ascii", 7291, 10925, 78, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "maya_leading_comma_7500_ascii", 7294, 10999, 84, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing, n..." },
	{ "maya_leading_comma_7500_ascii", 7302, 11116, 78, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "maya_leading_comma_7500_ascii", 7307, 11198, 78, 0, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_leading_comma_7500_ascii", 7913, -1, 0, 788, 0, 0, 0, "material" },
	{ "maya_leading_comma_7500_ascii", 7913, -1, 0, 791, 0, 0, 0, "material" },
	{ "maya_leading_comma_7500_ascii", 8300, 1584, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_leading_comma_7500_ascii", 8309, 8861, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_leading_comma_7500_ascii", 8340, 8892, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_leading_comma_7500_ascii", 8343, 11807, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_leading_comma_7500_ascii", 8350, -1, 0, 759, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", 8350, -1, 0, 762, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", 8371, 8911, 33, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", 8402, -1, 0, 788, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", 8402, -1, 0, 791, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", 8410, -1, 0, 803, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "maya_leading_comma_7500_ascii", 8410, -1, 0, 806, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "maya_leading_comma_7500_ascii", 8412, -1, 0, 808, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_leading_comma_7500_ascii", 8412, -1, 0, 811, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_leading_comma_7500_ascii", 8458, 13120, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_leading_comma_7500_ascii", 8504, -1, 0, 813, 0, 0, 0, "conn" },
	{ "maya_leading_comma_7500_ascii", 8504, -1, 0, 816, 0, 0, 0, "conn" },
	{ "maya_leading_comma_7500_ascii", 8864, 0, 60, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_leading_comma_7500_ascii", 8865, 100, 33, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "maya_leading_comma_7500_ascii", 8883, 1525, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_leading_comma_7500_ascii", 8884, 2615, 33, 0, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_leading_comma_7500_ascii", 8899, -1, 0, 147, 0, 0, 0, "root" },
	{ "maya_leading_comma_7500_ascii", 8899, -1, 0, 148, 0, 0, 0, "root" },
	{ "maya_leading_comma_7500_ascii", 8900, -1, 0, 151, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "maya_leading_comma_7500_ascii", 8900, -1, 0, 152, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "maya_leading_comma_7500_ascii", 8911, 2808, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_leading_comma_7500_ascii", 8912, 3021, 33, 0, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_leading_comma_7500_ascii", 8915, 8762, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_leading_comma_7500_ascii", 8919, 0, 0, 0, 0, 0, 0, "uc->top_node" },
	{ "maya_leading_comma_7500_ascii", 8921, 8861, 33, 0, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_leading_comma_7500_ascii", 8924, 13016, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_leading_comma_7500_ascii", 8925, 13120, 33, 0, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_leading_comma_7500_ascii", 8937, 1584, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "maya_leading_comma_7500_ascii", 9630, -1, 0, 824, 0, 0, 0, "tmp_connections" },
	{ "maya_leading_comma_7500_ascii", 9630, -1, 0, 827, 0, 0, 0, "tmp_connections" },
	{ "maya_leading_comma_7500_ascii", 9634, -1, 0, 0, 137, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_leading_comma_7500_ascii", 9664, -1, 0, 0, 138, 0, 0, "uc->scene.connections_dst.data" },
	{ "maya_leading_comma_7500_ascii", 9804, -1, 0, 825, 0, 0, 0, "node_ids" },
	{ "maya_leading_comma_7500_ascii", 9804, -1, 0, 828, 0, 0, 0, "node_ids" },
	{ "maya_leading_comma_7500_ascii", 9807, -1, 0, 826, 0, 0, 0, "node_ptrs" },
	{ "maya_leading_comma_7500_ascii", 9807, -1, 0, 829, 0, 0, 0, "node_ptrs" },
	{ "maya_leading_comma_7500_ascii", 9818, -1, 0, 827, 0, 0, 0, "node_offsets" },
	{ "maya_leading_comma_7500_ascii", 9818, -1, 0, 830, 0, 0, 0, "node_offsets" },
	{ "maya_leading_comma_7500_ascii", 9863, -1, 0, 828, 0, 0, 0, "p_offset" },
	{ "maya_leading_comma_7500_ascii", 9863, -1, 0, 831, 0, 0, 0, "p_offset" },
	{ "maya_leading_comma_7500_ascii", 9930, -1, 0, 834, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_leading_comma_7500_ascii", 9930, -1, 0, 837, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_leading_comma_7500_ascii", 9952, -1, 0, 832, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_leading_comma_7500_ascii", 9952, -1, 0, 835, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_leading_comma_7500_ascii", 9961, -1, 0, 0, 143, 0, 0, "list->data" },
	{ "maya_node_attribute_zoo_6100_ascii", 11207, -1, 0, 6638, 0, 0, 0, "aprop" },
	{ "maya_node_attribute_zoo_6100_ascii", 5670, -1, 0, 6510, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 5670, -1, 0, 6524, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_binary", 10626, -1, 0, 0, 371, 0, 0, "spans" },
	{ "maya_node_attribute_zoo_6100_binary", 10669, -1, 0, 0, 389, 0, 0, "levels" },
	{ "maya_node_attribute_zoo_6100_binary", 10789, -1, 0, 5045, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_node_attribute_zoo_6100_binary", 10789, -1, 0, 5065, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_node_attribute_zoo_6100_binary", 10805, -1, 0, 0, 357, 0, 0, "node->all_attribs.data" },
	{ "maya_node_attribute_zoo_6100_binary", 11155, -1, 0, 0, 371, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &curve->basis)" },
	{ "maya_node_attribute_zoo_6100_binary", 11160, -1, 0, 0, 380, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_u)" },
	{ "maya_node_attribute_zoo_6100_binary", 11161, -1, 0, 0, 381, 0, 0, "ufbxi_finalize_nurbs_basis(uc, &surface->basis_v)" },
	{ "maya_node_attribute_zoo_6100_binary", 11183, -1, 0, 5066, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "maya_node_attribute_zoo_6100_binary", 11183, -1, 0, 5085, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "maya_node_attribute_zoo_6100_binary", 11207, -1, 0, 5067, 0, 0, 0, "aprop" },
	{ "maya_node_attribute_zoo_6100_binary", 11599, -1, 0, 0, 389, 0, 0, "ufbxi_finalize_lod_group(uc, *p_lod)" },
	{ "maya_node_attribute_zoo_6100_binary", 4675, -1, 0, 0, 0, 12405, 0, "val" },
	{ "maya_node_attribute_zoo_6100_binary", 4678, -1, 0, 0, 0, 12158, 0, "val" },
	{ "maya_node_attribute_zoo_6100_binary", 5012, 12130, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_node_attribute_zoo_6100_binary", 6153, -1, 0, 41, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_node_attribute_zoo_6100_binary", 6153, -1, 0, 42, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_node_attribute_zoo_6100_binary", 6606, -1, 0, 4966, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_o..." },
	{ "maya_node_attribute_zoo_6100_binary", 6606, -1, 0, 4985, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_o..." },
	{ "maya_node_attribute_zoo_6100_binary", 6607, -1, 0, 4967, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_element_offsets..." },
	{ "maya_node_attribute_zoo_6100_binary", 6607, -1, 0, 4986, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_element_offsets..." },
	{ "maya_node_attribute_zoo_6100_binary", 6611, -1, 0, 4968, 0, 0, 0, "elem" },
	{ "maya_node_attribute_zoo_6100_binary", 6611, -1, 0, 4989, 0, 0, 0, "elem" },
	{ "maya_node_attribute_zoo_6100_binary", 6639, -1, 0, 1209, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "maya_node_attribute_zoo_6100_binary", 6639, -1, 0, 1217, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "maya_node_attribute_zoo_6100_binary", 6654, -1, 0, 275, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", 6654, -1, 0, 281, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", 6664, -1, 0, 4976, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", 6664, -1, 0, 4996, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", 7470, -1, 0, 4113, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 7470, -1, 0, 4127, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 7475, 138209, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->basis..." },
	{ "maya_node_attribute_zoo_6100_binary", 7477, 138308, 255, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 7484, 138359, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 7485, 138416, 1, 0, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 7486, 143462, 27, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 7500, -1, 0, 4183, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 7500, -1, 0, 4197, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 7505, 139478, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 7506, 139592, 1, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"ZZ\", &dimens..." },
	{ "maya_node_attribute_zoo_6100_binary", 7507, 139631, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Step, \"II\", &step_u, &ste..." },
	{ "maya_node_attribute_zoo_6100_binary", 7508, 139664, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 7521, 139691, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 7522, 139727, 1, 0, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 7523, 140321, 3, 0, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 7524, 141818, 63, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 7525, 139655, 1, 0, 0, 0, 0, "points->size / 4 == (size_t)dimension_u * (size_t)dimen..." },
	{ "maya_node_attribute_zoo_6100_binary", 7610, -1, 0, 704, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 7610, -1, 0, 710, 0, 0, 0, "bone" },
	{ "maya_node_attribute_zoo_6100_binary", 8220, 6671, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_node_attribute_zoo_6100_binary", 8235, -1, 0, 269, 0, 0, 0, "entry" },
	{ "maya_node_attribute_zoo_6100_binary", 8235, -1, 0, 275, 0, 0, 0, "entry" },
	{ "maya_node_attribute_zoo_6100_binary", 8244, -1, 0, 270, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_6100_binary", 8244, -1, 0, 276, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_6100_binary", 8254, -1, 0, 0, 22, 0, 0, "attrib_info.props.props" },
	{ "maya_node_attribute_zoo_6100_binary", 8259, 6754, 255, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_node_attribute_zoo_6100_binary", 8261, -1, 0, 1434, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8261, -1, 0, 1443, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8263, -1, 0, 1203, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8263, -1, 0, 1211, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8265, -1, 0, 704, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 8265, -1, 0, 710, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 8267, -1, 0, 273, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8267, -1, 0, 279, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8280, -1, 0, 2583, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8280, -1, 0, 2592, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8284, -1, 0, 1951, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8284, -1, 0, 1960, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8288, -1, 0, 2770, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8288, -1, 0, 2782, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8294, -1, 0, 275, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8294, -1, 0, 281, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8313, 157532, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 8348, 6671, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, type_st..." },
	{ "maya_node_attribute_zoo_6100_binary", 8355, -1, 0, 3853, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 8355, -1, 0, 3866, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_6100_binary", 8375, 138209, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 8377, 139478, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 8381, -1, 0, 4319, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 8381, -1, 0, 4333, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 8383, -1, 0, 4364, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 8383, -1, 0, 4378, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 8440, -1, 0, 0, 307, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 8526, -1, 0, 4977, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_6100_binary", 8526, -1, 0, 4995, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_6100_binary", 8528, -1, 0, 4979, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "maya_node_attribute_zoo_6100_binary", 8533, 163331, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "maya_node_attribute_zoo_6100_binary", 8536, 163352, 1, 0, 0, 0, 0, "curve->keyframes.data" },
	{ "maya_node_attribute_zoo_6100_binary", 8656, 163388, 86, 0, 0, 0, 0, "Unknown key mode" },
	{ "maya_node_attribute_zoo_6100_binary", 8661, 163349, 3, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_node_attribute_zoo_6100_binary", 8710, 163349, 1, 0, 0, 0, 0, "data == data_end" },
	{ "maya_node_attribute_zoo_6100_binary", 8781, -1, 0, 4972, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8781, -1, 0, 4991, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8782, -1, 0, 4976, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "maya_node_attribute_zoo_6100_binary", 8782, -1, 0, 4996, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "maya_node_attribute_zoo_6100_binary", 8785, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "maya_node_attribute_zoo_6100_binary", 8807, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "maya_node_attribute_zoo_6100_binary", 8819, -1, 0, 4966, 0, 0, 0, "stack" },
	{ "maya_node_attribute_zoo_6100_binary", 8819, -1, 0, 4985, 0, 0, 0, "stack" },
	{ "maya_node_attribute_zoo_6100_binary", 8820, 163019, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"S\", &stack->name)" },
	{ "maya_node_attribute_zoo_6100_binary", 8823, -1, 0, 4969, 0, 0, 0, "layer" },
	{ "maya_node_attribute_zoo_6100_binary", 8823, -1, 0, 4988, 0, 0, 0, "layer" },
	{ "maya_node_attribute_zoo_6100_binary", 8825, -1, 0, 4971, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8825, -1, 0, 4990, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8830, 163046, 255, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_ReferenceTime, \"LL\", &beg..." },
	{ "maya_node_attribute_zoo_6100_binary", 8840, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8850, 162983, 125, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_node_attribute_zoo_6100_binary", 8854, 163019, 0, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_node_attribute_zoo_6100_binary", 8878, -1, 0, 41, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 8878, -1, 0, 42, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 8930, 158678, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_node_attribute_zoo_6100_binary", 8931, 162983, 125, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 8935, 162983, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_node_attribute_zoo_6100_binary", 9939, -1, 0, 0, 385, 0, 0, "list->data" },
	{ "maya_node_attribute_zoo_7500_ascii", 5616, -1, 0, 3264, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 5616, -1, 0, 3268, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 7129, -1, 0, 0, 0, 0, 28459, "index_ix >= 0 && (size_t)index_ix < mesh->num_indices" },
	{ "maya_node_attribute_zoo_7500_binary", 10727, -1, 0, 2083, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 10727, -1, 0, 2093, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 4742, 61146, 109, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 4743, 61333, 103, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 4984, -1, 0, 0, 0, 0, 2909, "res != -28" },
	{ "maya_node_attribute_zoo_7500_binary", 6565, -1, 0, 0, 246, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type)" },
	{ "maya_node_attribute_zoo_7500_binary", 7724, -1, 0, 1727, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 7724, -1, 0, 1735, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 7729, 61038, 255, 0, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 7730, 61115, 255, 0, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 7731, 61175, 255, 0, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 7732, 61234, 255, 0, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 7733, 61292, 255, 0, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_binary", 7736, 61122, 0, 0, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_binary", 7741, 61242, 0, 0, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_binary", 7742, 61300, 0, 0, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", 7746, -1, 0, 0, 247, 0, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 7897, 61431, 0, 0, 0, 0, 0, "refs_left >= 0" },
	{ "maya_node_attribute_zoo_7500_binary", 8353, -1, 0, 649, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 8353, -1, 0, 653, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_7500_binary", 8357, -1, 0, 580, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 8357, -1, 0, 584, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 8359, -1, 0, 488, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 8359, -1, 0, 492, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 8361, -1, 0, 700, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 8361, -1, 0, 704, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_..." },
	{ "maya_node_attribute_zoo_7500_binary", 8365, -1, 0, 1134, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 8365, -1, 0, 1139, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_gro..." },
	{ "maya_node_attribute_zoo_7500_binary", 8414, -1, 0, 1737, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 8414, -1, 0, 1745, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 8416, 61038, 255, 0, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", 9745, -1, 0, 2083, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_7500_binary", 9745, -1, 0, 2093, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_7500_binary", 9770, -1, 0, 2084, 0, 0, 0, "new_prop" },
	{ "maya_node_attribute_zoo_7500_binary", 9770, -1, 0, 2094, 0, 0, 0, "new_prop" },
	{ "maya_node_attribute_zoo_7500_binary", 9784, -1, 0, 2085, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_7500_binary", 9784, -1, 0, 2095, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_7500_binary", 9786, -1, 0, 0, 276, 0, 0, "elem->props.props" },
	{ "maya_resampled_7500_binary", 7770, 24917, 23, 0, 0, 0, 0, "p_ref < p_ref_end" },
	{ "maya_shared_textures_6100_binary", 11373, -1, 0, 2364, 0, 0, 0, "mat_tex" },
	{ "maya_shared_textures_6100_binary", 11373, -1, 0, 2378, 0, 0, 0, "mat_tex" },
	{ "maya_texture_layers_6100_binary", 10093, -1, 0, 1631, 0, 0, 0, "(ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_sta..." },
	{ "maya_texture_layers_6100_binary", 10093, -1, 0, 1644, 0, 0, 0, "(ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_sta..." },
	{ "maya_texture_layers_6100_binary", 10100, -1, 0, 0, 191, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 11516, -1, 0, 1631, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_texture_layers_6100_binary", 11516, -1, 0, 1644, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_texture_layers_6100_binary", 7943, -1, 0, 1439, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 7943, -1, 0, 1452, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 7951, -1, 0, 1441, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 7951, -1, 0, 1454, 0, 0, 0, "extra" },
	{ "maya_texture_layers_6100_binary", 8406, -1, 0, 1439, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 8406, -1, 0, 1452, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_textured_cube_6100_binary", 10580, -1, 0, 1649, 0, 0, 0, "result" },
	{ "maya_textured_cube_6100_binary", 10580, -1, 0, 1662, 0, 0, 0, "result" },
	{ "maya_textured_cube_6100_binary", 10601, -1, 0, 0, 192, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, dst)" },
	{ "maya_textured_cube_6100_binary", 11343, -1, 0, 1631, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "maya_textured_cube_6100_binary", 11343, -1, 0, 1644, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "maya_textured_cube_6100_binary", 11351, -1, 0, 1634, 0, 0, 0, "mat_texs" },
	{ "maya_textured_cube_6100_binary", 11351, -1, 0, 1647, 0, 0, 0, "mat_texs" },
	{ "maya_textured_cube_6100_binary", 11407, -1, 0, 0, 191, 0, 0, "texs" },
	{ "maya_textured_cube_6100_binary", 11426, -1, 0, 1642, 0, 0, 0, "tex" },
	{ "maya_textured_cube_6100_binary", 11426, -1, 0, 1655, 0, 0, 0, "tex" },
	{ "maya_textured_cube_6100_binary", 11466, -1, 0, 1648, 0, 0, 0, "content_videos" },
	{ "maya_textured_cube_6100_binary", 11466, -1, 0, 1661, 0, 0, 0, "content_videos" },
	{ "maya_textured_cube_6100_binary", 11471, -1, 0, 1649, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &video->filename, v..." },
	{ "maya_textured_cube_6100_binary", 11471, -1, 0, 1662, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &video->filename, v..." },
	{ "maya_textured_cube_6100_binary", 11499, -1, 0, 1655, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &texture->filename,..." },
	{ "maya_textured_cube_6100_binary", 11499, -1, 0, 1668, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &texture->filename,..." },
	{ "maya_textured_cube_6100_binary", 7925, -1, 0, 1174, 0, 0, 0, "texture" },
	{ "maya_textured_cube_6100_binary", 7925, -1, 0, 1187, 0, 0, 0, "texture" },
	{ "maya_textured_cube_6100_binary", 7993, -1, 0, 800, 0, 0, 0, "video" },
	{ "maya_textured_cube_6100_binary", 7993, -1, 0, 811, 0, 0, 0, "video" },
	{ "maya_textured_cube_6100_binary", 8404, -1, 0, 1174, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "maya_textured_cube_6100_binary", 8404, -1, 0, 1187, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "maya_textured_cube_6100_binary", 8408, -1, 0, 800, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "maya_textured_cube_6100_binary", 8408, -1, 0, 811, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "maya_textured_cube_7500_ascii", 5448, -1, 0, 787, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_textured_cube_7500_ascii", 5448, -1, 0, 793, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_textured_cube_7500_ascii", 8014, -1, 0, 0, 159, 0, 0, "video->content" },
	{ "maya_textured_cube_7500_binary", 10003, -1, 0, 1104, 0, 0, 0, "tex" },
	{ "maya_textured_cube_7500_binary", 10003, -1, 0, 1111, 0, 0, 0, "tex" },
	{ "maya_textured_cube_7500_binary", 10013, -1, 0, 0, 220, 0, 0, "list->data" },
	{ "maya_textured_cube_7500_binary", 11328, -1, 0, 1104, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "maya_textured_cube_7500_binary", 11328, -1, 0, 1111, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "maya_transform_animation_6100_binary", 8652, 17549, 11, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_uv_set_tangents_6100_binary", 6853, 6895, 0, 0, 0, 0, 0, "ufbxi_check_indices(uc, p_dst_index, mesh->vertex_posit..." },
	{ "maya_zero_end_7400_binary", 1301, 12382, 255, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_zero_end_7400_binary", 1336, 16748, 1, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_zero_end_7400_binary", 2201, 331, 0, 0, 0, 0, 0, "str || length == 0" },
	{ "maya_zero_end_7400_binary", 3244, 36, 255, 0, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_zero_end_7400_binary", 3274, -1, 0, 0, 0, 12392, 0, "uc->read_fn" },
	{ "maya_zero_end_7400_binary", 4734, 16744, 106, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 4741, 12615, 106, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 4744, 12379, 101, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 4765, 12382, 255, 0, 0, 0, 0, "data" },
	{ "maya_zero_end_7400_binary", 4787, -1, 0, 0, 0, 27, 0, "header" },
	{ "maya_zero_end_7400_binary", 4808, 24, 29, 0, 0, 0, 0, "num_values64 <= 0xffffffffui32" },
	{ "maya_zero_end_7400_binary", 4826, -1, 0, 3, 0, 0, 0, "node" },
	{ "maya_zero_end_7400_binary", 4826, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_zero_end_7400_binary", 4830, -1, 0, 0, 0, 40, 0, "name" },
	{ "maya_zero_end_7400_binary", 4832, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_zero_end_7400_binary", 4848, -1, 0, 449, 0, 0, 0, "arr" },
	{ "maya_zero_end_7400_binary", 4848, -1, 0, 452, 0, 0, 0, "arr" },
	{ "maya_zero_end_7400_binary", 4857, -1, 0, 0, 0, 12379, 0, "data" },
	{ "maya_zero_end_7400_binary", 4892, 12382, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_zero_end_7400_binary", 4899, 16748, 1, 0, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_zero_end_7400_binary", 4912, 12379, 99, 0, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_zero_end_7400_binary", 4928, -1, 0, 0, 0, 12392, 0, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_zero_end_7400_binary", 4985, 12384, 1, 0, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_zero_end_7400_binary", 4988, 12384, 255, 0, 0, 0, 0, "Bad array encoding" },
	{ "maya_zero_end_7400_binary", 5013, 12379, 101, 0, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_zero_end_7400_binary", 5022, -1, 0, 6, 0, 0, 0, "vals" },
	{ "maya_zero_end_7400_binary", 5022, -1, 0, 7, 0, 0, 0, "vals" },
	{ "maya_zero_end_7400_binary", 5030, -1, 0, 0, 0, 87, 0, "data" },
	{ "maya_zero_end_7400_binary", 5084, 331, 0, 0, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &vals[i]...." },
	{ "maya_zero_end_7400_binary", 5094, 593, 8, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_zero_end_7400_binary", 5099, 22, 1, 0, 0, 0, 0, "Bad value type" },
	{ "maya_zero_end_7400_binary", 5110, 66, 4, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_zero_end_7400_binary", 5112, 36, 255, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_zero_end_7400_binary", 5124, 58, 93, 0, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_zero_end_7400_binary", 5129, 70, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_zero_end_7400_binary", 5138, -1, 0, 28, 0, 0, 0, "node->children" },
	{ "maya_zero_end_7400_binary", 5138, -1, 0, 29, 0, 0, 0, "node->children" },
	{ "maya_zero_end_7400_binary", 5812, 35, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_zero_end_7400_binary", 5842, 22, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_zero_end_7400_binary", 6568, 12340, 2, 0, 0, 0, 0, "ufbxi_check_string(*name)" },
	{ "maya_zero_end_7400_binary", 6708, 12588, 0, 0, 0, 0, 0, "num_elems > 0 && num_elems < 2147483647i32" },
	{ "maya_zero_end_7400_binary", 6797, 12588, 0, 0, 0, 0, 0, "ufbxi_check_indices(uc, p_dst_index, index_data, 1, num..." },
	{ "maya_zero_end_7400_binary", 7322, 12861, 0, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "maya_zero_end_7400_binary", 8326, 12333, 255, 0, 0, 0, 0, "(info.fbx_id & (0x8000000000000000ULL)) == 0" },
	{ "maya_zero_end_7500_binary", 13480, 24, 0, 0, 0, 0, 0, "ufbxi_read_legacy_root(uc)" },
	{ "maya_zero_end_7500_binary", 5922, 24, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_zero_end_7500_binary", 9444, 24, 0, 0, 0, 0, 0, "ufbxi_parse_legacy_toplevel(uc)" },
	{ "synthetic_blend_shape_order_7500_ascii", 6963, -1, 0, 726, 0, 0, 0, "offsets" },
	{ "synthetic_blend_shape_order_7500_ascii", 6963, -1, 0, 729, 0, 0, 0, "offsets" },
	{ "synthetic_cube_nan_6100_ascii", 5412, 4866, 45, 0, 0, 0, 0, "token->type == 'F'" },
	{ "synthetic_id_collision_7500_ascii", 9564, -1, 0, 83300, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "synthetic_id_collision_7500_ascii", 9666, -1, 0, 83300, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "synthetic_indexed_by_vertex_7500_ascii", 6804, -1, 0, 0, 114, 0, 0, "new_index_data" },
	{ "synthetic_missing_version_6100_ascii", 10819, -1, 0, 0, 197, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 8030, -1, 0, 3865, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 8030, -1, 0, 3874, 0, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", 8054, -1, 0, 3868, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 8054, -1, 0, 3878, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 8064, -1, 0, 3869, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 8064, -1, 0, 3879, 0, 0, 0, "pose->bone_poses.data" },
	{ "synthetic_missing_version_6100_ascii", 8286, -1, 0, 249, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 8286, -1, 0, 255, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 8418, -1, 0, 3865, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 8418, -1, 0, 3874, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "synthetic_missing_version_6100_ascii", 8528, -1, 0, 4466, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "synthetic_missing_version_6100_ascii", 8727, 72756, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "synthetic_missing_version_6100_ascii", 8738, 72840, 102, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "synthetic_string_collision_7500_ascii", 2171, -1, 0, 2243, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "synthetic_string_collision_7500_ascii", 2171, -1, 0, 2274, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
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

			ufbxt_init_allocator(&opts.temp_allocator);
			ufbxt_init_allocator(&opts.result_allocator);

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

void ufbxt_do_file_test(const char *name, void (*test_fn)(ufbx_scene *s, ufbxt_diff_error *err, ufbx_error *load_error), const char *suffix, ufbx_load_opts user_opts, bool alternative, bool allow_error)
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
			load_opts.filename.data = buf;
			load_opts.filename.length = SIZE_MAX;

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

			ufbx_load_opts stream_opts = load_opts;
			ufbxt_init_allocator(&stream_opts.temp_allocator);
			ufbxt_init_allocator(&stream_opts.result_allocator);
			stream_opts.read_buffer_size = 1;
			stream_opts.temp_allocator.huge_threshold = 1;
			stream_opts.result_allocator.huge_threshold = 1;
			stream_opts.filename.data = NULL;
			stream_opts.filename.length = 0;
			stream_opts.progress_cb.fn = &ufbxt_measure_progress;
			stream_opts.progress_cb.user = &stream_progress_ctx;
			stream_opts.progress_interval_hint = 1;
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
					t_jmp_buf = (ufbxt_jmp_buf*)calloc(1, sizeof(ufbxt_jmp_buf));
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
					free(t_jmp_buf);
					t_jmp_buf = NULL;
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
				ufbxt_do_fuzz(scene, streamed_scene, stream_progress_ctx.calls, base_name, data, size, buf);

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
		}
	}

	if (num_opened == 0) {
		ufbxt_assert_fail(__FILE__, __LINE__, "File not found");
	}

	free(obj_file);
}

#define UFBXT_IMPL 1
#define UFBXT_TEST(name) void ufbxt_test_fn_##name(void)
#define UFBXT_FILE_TEST(name) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, user_opts, false, false); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_OPTS(name, get_opts) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, get_opts(), false, false); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_SUFFIX(name, suffix) void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name##_##suffix(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name##_##suffix, #suffix, user_opts, false, false); } \
	void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_SUFFIX_OPTS(name, suffix, get_opts) void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name##_##suffix(void) { \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name##_##suffix, #suffix, get_opts(), false, false); } \
	void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_ALT(name, file) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#file, &ufbxt_test_fn_imp_file_##name, NULL, user_opts, true, false); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_OPTS_ALT(name, file, get_opts) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbxt_do_file_test(#file, &ufbxt_test_fn_imp_file_##name, NULL, get_opts(), false, false); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_ALLOW_ERROR(name) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, user_opts, false, true); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)

#include "all_tests.h"

#undef UFBXT_IMPL
#undef UFBXT_TEST
#undef UFBXT_FILE_TEST
#undef UFBXT_FILE_TEST_OPTS
#undef UFBXT_FILE_TEST_SUFFIX
#undef UFBXT_FILE_TEST_SUFFIX_OPTS
#undef UFBXT_FILE_TEST_ALT
#undef UFBXT_FILE_TEST_OPTS_ALT
#undef UFBXT_FILE_TEST_ALLOW_ERROR
#define UFBXT_IMPL 0
#define UFBXT_TEST(name) { #name, &ufbxt_test_fn_##name },
#define UFBXT_FILE_TEST(name) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS(name, get_opts) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_SUFFIX(name, suffix) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_SUFFIX_OPTS(name, suffix, get_opts) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_ALT(name, file) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS_ALT(name, file, get_opts) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_ALLOW_ERROR(name) { #name, &ufbxt_test_fn_file_##name },
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

		if (!strcmp(argv[i], "--fuzz")) {
			g_fuzz = true;
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

	return num_ok == num_ran ? 0 : 1;
}

