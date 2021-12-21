#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr);

#define ufbxt_arraycount(arr) (sizeof(arr) / sizeof(*(arr)))

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

// -- Vector helpers

static ufbx_real ufbxt_dot2(ufbx_vec2 a, ufbx_vec2 b)
{
	return a.x*b.x + a.y*b.y;
}

static ufbx_real ufbxt_dot3(ufbx_vec3 a, ufbx_vec3 b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

static ufbx_vec2 ufbxt_add2(ufbx_vec2 a, ufbx_vec2 b)
{
	ufbx_vec2 v;
	v.x = a.x + b.x;
	v.y = a.y + b.y;
	return v;
}

static ufbx_vec3 ufbxt_add3(ufbx_vec3 a, ufbx_vec3 b)
{
	ufbx_vec3 v;
	v.x = a.x + b.x;
	v.y = a.y + b.y;
	v.z = a.z + b.z;
	return v;
}

static ufbx_vec2 ufbxt_sub2(ufbx_vec2 a, ufbx_vec2 b)
{
	ufbx_vec2 v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	return v;
}

static ufbx_vec3 ufbxt_sub3(ufbx_vec3 a, ufbx_vec3 b)
{
	ufbx_vec3 v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	v.z = a.z - b.z;
	return v;
}

static ufbx_vec2 ufbxt_mul2(ufbx_vec2 a, ufbx_real b)
{
	ufbx_vec2 v;
	v.x = a.x * b;
	v.y = a.y * b;
	return v;
}

static ufbx_vec3 ufbxt_mul3(ufbx_vec3 a, ufbx_real b)
{
	ufbx_vec3 v;
	v.x = a.x * b;
	v.y = a.y * b;
	v.z = a.z * b;
	return v;
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

#include "check_scene.h"

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

static void *ufbxt_read_file(const char *name, size_t *p_size)
{
	FILE *file = fopen(name, "rb");
	if (!file) return NULL;

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *data = malloc(size + 1);
	ufbxt_assert(data != NULL);
	size_t num_read = fread(data, 1, size, file);
	fclose(file);

	data[size] = '\0';

	if (num_read != size) {
		ufbxt_assert_fail(__FILE__, __LINE__, "Failed to load file");
	}

	*p_size = size;
	return data;
}

typedef struct {
	char name[64];

	size_t num_faces;
	size_t num_indices;

	ufbx_face *faces;

	ufbx_vertex_vec3 vertex_position;
	ufbx_vertex_vec3 vertex_normal;
	ufbx_vertex_vec2 vertex_uv;
} ufbxt_obj_mesh;

typedef struct {

	ufbxt_obj_mesh *meshes;
	size_t num_meshes;

	bool bad_normals;
	bool bad_order;
	bool bad_uvs;
	ufbx_real tolerance;

} ufbxt_obj_file;

static ufbxt_obj_file *ufbxt_load_obj(void *obj_data, size_t obj_size)
{
	size_t num_positions = 0;
	size_t num_normals = 0;
	size_t num_uvs = 0;
	size_t num_faces = 0;
	size_t num_meshes = 0;

	char *line = (char*)obj_data;
	for (;;) {
		char *end = strpbrk(line, "\r\n");
		char prev = '\0';
		if (end) {
			prev = *end;
			*end = '\0';
		}

		if (!strncmp(line, "v ", 2)) num_positions++;
		else if (!strncmp(line, "vt ", 3)) num_uvs++;
		else if (!strncmp(line, "vn ", 3)) num_normals++;
		else if (!strncmp(line, "f ", 2)) num_faces++;
		else if (!strncmp(line, "g default", 7)) { /* ignore default group */ }
		else if (!strncmp(line, "g ", 2)) num_meshes++;

		if (end) {
			*end = prev;
			line = end + 1;
		} else {
			break;
		}
	}

	size_t alloc_size = 0;
	alloc_size += sizeof(ufbxt_obj_file);
	alloc_size += num_positions * sizeof(ufbx_vec3);
	alloc_size += num_normals * sizeof(ufbx_vec3);
	alloc_size += num_uvs * sizeof(ufbx_vec2);
	alloc_size += num_faces * sizeof(ufbx_face);
	alloc_size += num_faces * 3 * 4 * sizeof(int32_t);
	alloc_size += num_meshes * sizeof(ufbxt_obj_mesh);

	void *data = malloc(alloc_size);
	ufbxt_assert(data);

	ufbxt_obj_file *obj = (ufbxt_obj_file*)data;
	ufbx_vec3 *positions = (ufbx_vec3*)(obj + 1);
	ufbx_vec3 *normals = (ufbx_vec3*)(positions + num_positions);
	ufbx_vec2 *uvs = (ufbx_vec2*)(normals + num_normals);
	ufbx_face *faces = (ufbx_face*)(uvs + num_uvs);
	int32_t *position_indices = (int32_t*)(faces + num_faces);
	int32_t *normal_indices = (int32_t*)(position_indices + num_faces * 4);
	int32_t *uv_indices = (int32_t*)(normal_indices + num_faces * 4);
	ufbxt_obj_mesh *meshes = (ufbxt_obj_mesh*)(uv_indices + num_faces * 4);
	void *data_end = meshes + num_meshes;
	ufbxt_assert((char*)data_end - (char*)data == alloc_size);

	memset(obj, 0, sizeof(ufbxt_obj_file));

	ufbx_vec3 *dp = positions;
	ufbx_vec3 *dn = normals;
	ufbx_vec2 *du = uvs;
	ufbxt_obj_mesh *mesh = NULL;

	int32_t *dpi = position_indices;
	int32_t *dni = normal_indices;
	int32_t *dui = uv_indices;

	ufbx_face *df = faces;

	obj->meshes = meshes;
	obj->num_meshes = num_meshes;
	obj->tolerance = 0.001f;

	line = (char*)obj_data;
	for (;;) {
		char *line_end = strpbrk(line, "\r\n");
		char prev = '\0';
		if (line_end) {
			prev = *line_end;
			*line_end = '\0';
		}

		if (!strncmp(line, "v ", 2)) {
			ufbxt_assert(sscanf(line, "v %lf %lf %lf", &dp->x, &dp->y, &dp->z) == 3);
			dp++;
		} else if (!strncmp(line, "vt ", 3)) {
			ufbxt_assert(sscanf(line, "vt %lf %lf", &du->x, &du->y) == 2);
			du++;
		} else if (!strncmp(line, "vn ", 3)) {
			ufbxt_assert(sscanf(line, "vn %lf %lf %lf", &dn->x, &dn->y, &dn->z) == 3);
			dn++;
		} else if (!strncmp(line, "f ", 2)) {
			ufbxt_assert(mesh);

			df->index_begin = (uint32_t)mesh->num_indices;
			df->num_indices = 0;

			char *begin = line + 2;
			do {
				char *end = strchr(begin, ' ');
				if (end) *end++ = '\0';

				if (begin[strspn(begin, " \t\r\n")] == '\0') {
					begin = end;
					continue;
				}

				int pi = 0, ui = 0, ni = 0;
				if (sscanf(begin, "%d/%d/%d", &pi, &ui, &ni) == 3) {
				} else if (sscanf(begin, "%d//%d", &pi, &ni) == 2) {
				} else if (sscanf(begin, "%d/%d", &pi, &ui) == 2) {
				} else {
					ufbxt_assert(0 && "Failed to parse face indices");
				}

				*dpi++ = pi - 1;
				*dni++ = ni - 1;
				*dui++ = ui - 1;
				mesh->num_indices++;
				df->num_indices++;

				begin = end;
			} while (begin);

			mesh->num_faces++;
			df++;
		} else if (!strncmp(line, "g default", 7)) {
			/* ignore default group */
		} else if (!strncmp(line, "g ", 2)) {
			mesh = mesh ? mesh + 1 : meshes;
			memset(mesh, 0, sizeof(ufbxt_obj_mesh));

			// HACK: Truncate name at '_' to separate Blender
			// model and mesh names
			size_t len = strcspn(line + 2, "_");

			ufbxt_assert(len < sizeof(mesh->name));
			memcpy(mesh->name, line + 2, len);
			mesh->faces = df;
			mesh->vertex_position.data = positions;
			mesh->vertex_normal.data = normals;
			mesh->vertex_uv.data = uvs;
			mesh->vertex_position.indices = dpi;
			mesh->vertex_normal.indices = dni;
			mesh->vertex_uv.indices = dui;
		}

		if (line[0] == '#') {
			line += 1;
			while (line < line_end && (line[0] == ' ' || line[0] == '\t')) {
				line++;
			}
			while (line_end > line && (line_end[-1] == ' ' || line_end[-1] == '\t')) {
				*--line_end = '\0';
			}
			if (!strcmp(line, "ufbx:bad_normals")) {
				obj->bad_normals = true;
			}
			if (!strcmp(line, "ufbx:bad_order")) {
				obj->bad_order = true;
			}
			if (!strcmp(line, "ufbx:bad_uvs")) {
				obj->bad_uvs = true;
			}
			double tolerance = 0.0;
			if (sscanf(line, "ufbx:tolerance=%lf", &tolerance) == 1) {
				obj->tolerance = (ufbx_real)tolerance;
			}
		}

		if (line_end) {
			*line_end = prev;
			line = line_end + 1;
		} else {
			break;
		}
	}

	return obj;
}

static void ufbxt_debug_dump_obj_mesh(const char *file, ufbx_node *node, ufbx_mesh *mesh)
{
	FILE *f = fopen(file, "wb");
	ufbxt_assert(f);

	for (size_t i = 0; i < mesh->vertex_position.num_values; i++) {
		ufbx_vec3 v = mesh->vertex_position.data[i];
		v = ufbx_transform_position(&node->geometry_to_world, v);
		fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
	}

	for (size_t i = 0; i < mesh->vertex_uv.num_values; i++) {
		ufbx_vec2 v = mesh->vertex_uv.data[i];
		fprintf(f, "vt %f %f\n", v.x, v.y);
	}

	for (size_t fi = 0; fi < mesh->num_faces; fi++) {
		ufbx_face face = mesh->faces[fi];
		fprintf(f, "f");
		for (size_t ci = 0; ci < face.num_indices; ci++) {
			int32_t vi = mesh->vertex_position.indices[face.index_begin + ci];
			int32_t ti = mesh->vertex_uv.indices[face.index_begin + ci];
			fprintf(f, " %d/%d", vi + 1, ti + 1);
		}
		fprintf(f, "\n");
	}

	fclose(f);
}

static void ufbxt_debug_dump_obj_scene(const char *file, ufbx_scene *scene)
{
	FILE *f = fopen(file, "wb");
	ufbxt_assert(f);

	for (size_t mi = 0; mi < scene->meshes.count; mi++) {
		ufbx_mesh *mesh = scene->meshes.data[mi];
		for (size_t ni = 0; ni < mesh->instances.count; ni++) {
			ufbx_node *node = mesh->instances.data[ni];

			for (size_t i = 0; i < mesh->vertex_position.num_values; i++) {
				ufbx_vec3 v = mesh->skinned_position.data[i];
				if (mesh->skinned_is_local) {
					v = ufbx_transform_position(&node->geometry_to_world, v);
				}
				fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
			}

			for (size_t i = 0; i < mesh->vertex_uv.num_values; i++) {
				ufbx_vec2 v = mesh->vertex_uv.data[i];
				fprintf(f, "vt %f %f\n", v.x, v.y);
			}

			ufbx_matrix mat = ufbx_matrix_for_normals(&node->geometry_to_world);
			for (size_t i = 0; i < mesh->skinned_normal.num_values; i++) {
				ufbx_vec3 v = mesh->skinned_normal.data[i];
				if (mesh->skinned_is_local) {
					v = ufbx_transform_direction(&mat, v);
				}
				fprintf(f, "vn %f %f %f\n", v.x, v.y, v.z);
			}

			fprintf(f, "\n");
		}
	}

	int32_t v_off = 0, t_off = 0, n_off = 0;
	for (size_t mi = 0; mi < scene->meshes.count; mi++) {
		ufbx_mesh *mesh = scene->meshes.data[mi];
		for (size_t ni = 0; ni < mesh->instances.count; ni++) {
			ufbx_node *node = mesh->instances.data[ni];
			fprintf(f, "g %s\n", node->name.data);

			for (size_t fi = 0; fi < mesh->num_faces; fi++) {
				ufbx_face face = mesh->faces[fi];
				fprintf(f, "f");
				for (size_t ci = 0; ci < face.num_indices; ci++) {
					int32_t vi = v_off + mesh->skinned_position.indices[face.index_begin + ci];
					int32_t ni = n_off + mesh->skinned_normal.indices[face.index_begin + ci];
					if (mesh->vertex_uv.indices) {
						int32_t ti = t_off + mesh->vertex_uv.indices[face.index_begin + ci];
						fprintf(f, " %d/%d/%d", vi + 1, ti + 1, ni + 1);
					} else {
						fprintf(f, " %d//%d", vi + 1, ni + 1);
					}
				}
				fprintf(f, "\n");
			}

			fprintf(f, "\n");

			v_off += (int32_t)mesh->skinned_position.num_values;
			t_off += (int32_t)mesh->vertex_uv.num_values;
			n_off += (int32_t)mesh->skinned_normal.num_values;
		}
	}

	fclose(f);
}

typedef struct {
	size_t num;
	ufbx_real sum;
	ufbx_real max;
} ufbxt_diff_error;

static void ufbxt_assert_close_real(ufbxt_diff_error *p_err, ufbx_real a, ufbx_real b)
{
	ufbx_real err = fabs(a - b);
	ufbxt_assert(err < 0.001);
	p_err->num++;
	p_err->sum += err;
	if (err > p_err->max) p_err->max = err;
}

static void ufbxt_assert_close_vec2(ufbxt_diff_error *p_err, ufbx_vec2 a, ufbx_vec2 b)
{
	ufbxt_assert_close_real(p_err, a.x, b.x);
	ufbxt_assert_close_real(p_err, a.y, b.y);
}

static void ufbxt_assert_close_vec3(ufbxt_diff_error *p_err, ufbx_vec3 a, ufbx_vec3 b)
{
	ufbxt_assert_close_real(p_err, a.x, b.x);
	ufbxt_assert_close_real(p_err, a.y, b.y);
	ufbxt_assert_close_real(p_err, a.z, b.z);
}

static void ufbxt_assert_close_vec4(ufbxt_diff_error *p_err, ufbx_vec4 a, ufbx_vec4 b)
{
	ufbxt_assert_close_real(p_err, a.x, b.x);
	ufbxt_assert_close_real(p_err, a.y, b.y);
	ufbxt_assert_close_real(p_err, a.z, b.z);
	ufbxt_assert_close_real(p_err, a.w, b.w);
}

typedef struct {
	ufbx_vec3 pos;
	ufbx_vec2 uv;
} ufbxt_match_vertex;

static int ufbxt_cmp_sub_vertex(const void *va, const void *vb)
{
	const ufbxt_match_vertex *a = (const ufbxt_match_vertex*)va, *b = (const ufbxt_match_vertex*)vb;
	if (a->pos.x != b->pos.x) return a->pos.x < b->pos.x ? -1 : +1;
	if (a->pos.y != b->pos.y) return a->pos.y < b->pos.y ? -1 : +1;
	if (a->pos.z != b->pos.z) return a->pos.z < b->pos.z ? -1 : +1;
	if (a->uv.x != b->uv.x) return a->uv.x < b->uv.x ? -1 : +1;
	if (a->uv.y != b->uv.y) return a->uv.y < b->uv.y ? -1 : +1;
	return 0;
}

static void ufbxt_match_obj_mesh(ufbx_node *fbx_node, ufbx_mesh *fbx_mesh, ufbxt_obj_mesh *obj_mesh, ufbxt_diff_error *p_err, ufbx_real tolerance)
{
	ufbxt_assert(fbx_mesh->num_faces == obj_mesh->num_faces);
	ufbxt_assert(fbx_mesh->num_indices == obj_mesh->num_indices);

	// Check that all vertices exist, anything more doesn't really make sense
	ufbxt_match_vertex *obj_verts = (ufbxt_match_vertex*)calloc(obj_mesh->num_indices, sizeof(ufbxt_match_vertex));
	ufbxt_match_vertex *fbx_verts = (ufbxt_match_vertex*)calloc(fbx_mesh->num_indices, sizeof(ufbxt_match_vertex));
	ufbxt_assert(obj_verts && fbx_verts);

	for (size_t i = 0; i < obj_mesh->num_indices; i++) {
		obj_verts[i].pos = ufbx_get_vertex_vec3(&obj_mesh->vertex_position, i);
		if (obj_mesh->vertex_uv.data) {
			obj_verts[i].uv = ufbx_get_vertex_vec2(&obj_mesh->vertex_uv, i);
		}
	}
	for (size_t i = 0; i < fbx_mesh->num_indices; i++) {
		ufbx_vec3 fp = ufbx_get_vertex_vec3(&fbx_mesh->skinned_position, i);
		if (fbx_mesh->skinned_is_local) {
			fp = ufbx_transform_position(&fbx_node->geometry_to_world, fp);
		}
		fbx_verts[i].pos = fp;
		if (obj_mesh->vertex_uv.data) {
			ufbxt_assert(fbx_mesh->vertex_uv.data);
			fbx_verts[i].uv = ufbx_get_vertex_vec2(&fbx_mesh->vertex_uv, i);
		}
	}

	qsort(obj_verts, obj_mesh->num_indices, sizeof(ufbxt_match_vertex), &ufbxt_cmp_sub_vertex);
	qsort(fbx_verts, fbx_mesh->num_indices, sizeof(ufbxt_match_vertex), &ufbxt_cmp_sub_vertex);

	for (int32_t i = (int32_t)fbx_mesh->num_indices - 1; i >= 0; i--) {
		ufbxt_match_vertex v = fbx_verts[i];

		bool found = false;
		for (int32_t j = i; j >= 0 && obj_verts[j].pos.x >= v.pos.x - tolerance; j--) {
			ufbx_real dx = obj_verts[j].pos.x - v.pos.x;
			ufbx_real dy = obj_verts[j].pos.y - v.pos.y;
			ufbx_real dz = obj_verts[j].pos.z - v.pos.z;
			ufbx_real du = obj_verts[j].uv.x - v.uv.x;
			ufbx_real dv = obj_verts[j].uv.y - v.uv.y;
			ufbxt_assert(dx <= tolerance);
			ufbx_real err = (ufbx_real)sqrt(dx*dx + dy*dy + dz*dz + du*du + dv*dv);
			if (err < tolerance) {
				if (err > p_err->max) p_err->max = err;
				p_err->sum += err;
				p_err->num++;

				obj_verts[j] = obj_verts[i];
				found = true;
				break;
			}
		}

		ufbxt_assert(found);
	}

	free(obj_verts);
	free(fbx_verts);

}

static void ufbxt_diff_to_obj(ufbx_scene *scene, ufbxt_obj_file *obj, ufbxt_diff_error *p_err, bool check_deformed_normals)
{
	// ufbxt_debug_dump_obj_scene("test.obj", scene);

	for (size_t mesh_i = 0; mesh_i < obj->num_meshes; mesh_i++) {
		ufbxt_obj_mesh *obj_mesh = &obj->meshes[mesh_i];
		if (obj_mesh->num_indices == 0) continue;

		ufbx_node *node = ufbx_find_node(scene, obj_mesh->name);
		ufbxt_assert(node);
		ufbx_mesh *mesh = node->mesh;
		ufbxt_assert(mesh);

		ufbx_matrix *mat = &node->geometry_to_world;
		ufbx_matrix norm_mat = ufbx_get_compatible_matrix_for_normals(node);

		if (mesh->subdivision_display_mode == UFBX_SUBDIVISION_DISPLAY_SMOOTH || mesh->subdivision_display_mode == UFBX_SUBDIVISION_DISPLAY_HULL_AND_SMOOTH) {
			ufbx_mesh *sub_mesh = ufbx_subdivide_mesh(mesh, mesh->subdivision_preview_levels, NULL, NULL);

			ufbxt_check_mesh(scene, mesh);
			ufbxt_match_obj_mesh(node, sub_mesh, obj_mesh, p_err, obj->tolerance);
			ufbx_free_mesh(sub_mesh);

			continue;
		}

		ufbxt_assert(obj_mesh->num_faces == mesh->num_faces);
		ufbxt_assert(obj_mesh->num_indices == mesh->num_indices);

		bool check_normals = true;
		if (obj->bad_normals) check_normals = false;
		if (!check_deformed_normals && mesh->all_deformers.count > 0) check_normals = false;

		if (obj->bad_order) {
			ufbxt_match_obj_mesh(node, mesh, obj_mesh, p_err, obj->tolerance);
		} else {
			// Assume that the indices are in the same order!
			for (size_t face_ix = 0; face_ix < mesh->num_faces; face_ix++) {
				ufbx_face obj_face = obj_mesh->faces[face_ix];
				ufbx_face face = mesh->faces[face_ix];
				ufbxt_assert(obj_face.index_begin == face.index_begin);
				ufbxt_assert(obj_face.num_indices == face.num_indices);

				for (size_t ix = face.index_begin; ix < face.index_begin + face.num_indices; ix++) {
					ufbx_vec3 op = ufbx_get_vertex_vec3(&obj_mesh->vertex_position, ix);
					ufbx_vec3 fp = ufbx_get_vertex_vec3(&mesh->skinned_position, ix);
					ufbx_vec3 on = ufbx_get_vertex_vec3(&obj_mesh->vertex_normal, ix);
					ufbx_vec3 fn = ufbx_get_vertex_vec3(&mesh->skinned_normal, ix);

					if (mesh->skinned_is_local) {
						fp = ufbx_transform_position(mat, fp);
						fn = ufbx_transform_direction(&norm_mat, fn);

						ufbx_real fn_len = (ufbx_real)sqrt(fn.x*fn.x + fn.y*fn.y + fn.z*fn.z);
						fn.x /= fn_len;
						fn.y /= fn_len;
						fn.z /= fn_len;
					}

					ufbxt_assert_close_vec3(p_err, op, fp);

					if (check_normals) {
						ufbxt_assert_close_vec3(p_err, on, fn);
					}

					if (obj_mesh->vertex_uv.data && !obj->bad_uvs) {
						ufbxt_assert(mesh->vertex_uv.data);
						ufbx_vec2 ou = ufbx_get_vertex_vec2(&obj_mesh->vertex_uv, ix);
						ufbx_vec2 fu = ufbx_get_vertex_vec2(&mesh->vertex_uv, ix);
						ufbxt_assert_close_vec2(p_err, ou, fu);
					}
				}
			}
		}
	}
}

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

void ufbxt_init_allocator(ufbx_allocator *ator)
{
	ator->memory_limit = 0x4000000; // 64MB

	if (g_dedicated_allocs) return;

	ufbxt_allocator *at = (ufbxt_allocator*)malloc(sizeof(ufbxt_allocator));
	ufbxt_assert(at);
	at->offset = 0;
	at->bytes_allocated = 0;

	ator->user = at;
	ator->alloc_fn = &ufbxt_alloc;
	ator->free_fn = &ufbxt_free;
	ator->free_allocator_fn = &ufbxt_free_allocator;
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

bool ufbxt_cancel_progress(void *user, const ufbx_progress *progress)
{
	ufbxt_cancel_ctx *ctx = (ufbxt_cancel_ctx*)user;
	return --ctx->calls_left > 0;
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
			opts.progress_fn = &ufbxt_cancel_progress;
			opts.progress_user = &cancel_ctx;
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
						check->description = frame.description;
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
// From commit 1297996
// TODO: Non-regression version
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "maya_zero_end_7400_binary", 1261, 12382, 255, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_leading_comma_7500_ascii", 1262, -1, 0, 1, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_zero_end_7400_binary", 1296, 16748, 1, 0, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_leading_comma_7500_ascii", 1297, -1, 0, 87, 0, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", 1889, -1, 0, 1, 0, 0, 0, "data" },
	{ "synthetic_string_collision_7500_ascii", 2145, -1, 0, 2278, 0, 0, 0, "ufbxi_map_grow_size((&pool->map), sizeof(ufbx_string), ..." },
	{ "maya_leading_comma_7500_ascii", 2157, -1, 0, 0, 1, 0, 0, "dst" },
	{ "maya_zero_end_7400_binary", 2175, 331, 0, 0, 0, 0, 0, "str || length == 0" },
	{ "maya_leading_comma_7500_ascii", 2177, -1, 0, 0, 5, 0, 0, "str" },
	{ "maya_leading_comma_7500_ascii", 3060, -1, 0, 0, 0, 0, 1, "uc->opts.progress_fn(uc->opts.progress_user, &progress)" },
	{ "maya_leading_comma_7500_ascii", 3078, -1, 0, 0, 0, 1, 0, "uc->read_fn" },
	{ "maya_leading_comma_7500_ascii", 3134, -1, 0, 0, 0, 0, 1, "ufbxi_report_progress(uc)" },
	{ "maya_zero_end_7400_binary", 3213, 36, 255, 0, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_zero_end_7400_binary", 3243, -1, 0, 0, 0, 12392, 0, "uc->read_fn" },
	{ "maya_cache_sine_6100_binary", 3503, -1, 0, 1476, 0, 0, 0, "ufbxi_grow_array_size((xc->ator), sizeof(**(&xc->tok)),..." },
	{ "maya_cache_sine_6100_binary", 3538, -1, 0, 1476, 0, 0, 0, "ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & w..." },
	{ "maya_cache_sine_6100_binary", 3622, -1, 0, 1514, 0, 0, 0, "ufbxi_xml_push_token_char(xc, c)" },
	{ "maya_cache_sine_6100_binary", 3631, -1, 0, 1485, 0, 0, 0, "dst->data" },
	{ "maya_cache_sine_6100_binary", 3643, -1, 0, 1514, 0, 0, 0, "ufbxi_xml_read_until(xc, ((void *)0), UFBXI_XML_CTYPE_T..." },
	{ "maya_cache_sine_6100_binary", 3654, -1, 0, 1482, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3659, -1, 0, 1483, 0, 0, 0, "tag->text.data" },
	{ "maya_cache_sine_6100_binary", 3692, -1, 0, 1476, 0, 0, 0, "ufbxi_xml_skip_until_string(xc, ((void *)0), \"?>\")" },
	{ "maya_cache_sine_6100_binary", 3697, -1, 0, 1484, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3698, -1, 0, 1485, 0, 0, 0, "ufbxi_xml_read_until(xc, &tag->name, UFBXI_XML_CTYPE_NA..." },
	{ "maya_cache_sine_6100_binary", 3714, -1, 0, 1488, 0, 0, 0, "attrib" },
	{ "maya_cache_sine_6100_binary", 3715, -1, 0, 1489, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->name, UFBXI_XML_CTYPE..." },
	{ "maya_cache_sine_6100_binary", 3727, -1, 0, 1497, 0, 0, 0, "ufbxi_xml_read_until(xc, &attrib->value, quote_ctype)" },
	{ "maya_cache_sine_6100_binary", 3735, -1, 0, 1492, 0, 0, 0, "tag->attribs" },
	{ "maya_cache_sine_6100_binary", 3741, -1, 0, 1486, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, tag->name.data)" },
	{ "maya_cache_sine_6100_binary", 3747, -1, 0, 1517, 0, 0, 0, "tag->children" },
	{ "maya_cache_sine_6100_binary", 3756, -1, 0, 1475, 0, 0, 0, "tag" },
	{ "maya_cache_sine_6100_binary", 3762, -1, 0, 1476, 0, 0, 0, "ufbxi_xml_parse_tag(xc, &closing, ((void *)0))" },
	{ "maya_cache_sine_6100_binary", 3768, -1, 0, 1577, 0, 0, 0, "tag->children" },
	{ "maya_cache_sine_6100_binary", 3771, -1, 0, 1578, 0, 0, 0, "xc->doc" },
	{ "blender_279_uv_sets_6100_ascii", 4003, -1, 0, 720, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 4007, -1, 0, 721, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->e..." },
	{ "maya_cube_big_endian_6100_binary", 4503, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->s..." },
	{ "maya_cube_big_endian_7100_binary", 4570, -1, 0, 455, 0, 0, 0, "src" },
	{ "max2009_blob_5800_binary", 4640, -1, 0, 0, 0, 80100, 0, "val" },
	{ "max7_cube_5000_binary", 4642, 1869, 2, 0, 0, 0, 0, "type == 'S' || type == 'R'" },
	{ "max7_cube_5000_binary", 4651, 1888, 255, 0, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, d)" },
	{ "maya_node_attribute_zoo_6100_binary", 4661, -1, 0, 0, 0, 12405, 0, "val" },
	{ "maya_node_attribute_zoo_6100_binary", 4664, -1, 0, 0, 0, 12158, 0, "val" },
	{ "maya_cube_big_endian_6100_binary", 4706, -1, 0, 0, 0, 10701, 0, "val" },
	{ "maya_zero_end_7400_binary", 4720, 16744, 106, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 4727, 12615, 106, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 4728, 61146, 109, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 4729, 61333, 103, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 4730, 12379, 101, 0, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 4751, 12382, 255, 0, 0, 0, 0, "data" },
	{ "maya_zero_end_7400_binary", 4773, -1, 0, 0, 0, 27, 0, "header" },
	{ "maya_cube_big_endian_7500_binary", 4777, -1, 0, 5, 0, 0, 0, "header_words" },
	{ "maya_cube_big_endian_6100_binary", 4786, -1, 0, 5, 0, 0, 0, "header_words" },
	{ "maya_zero_end_7400_binary", 4794, 24, 29, 0, 0, 0, 0, "num_values64 <= 0xffffffffui32" },
	{ "maya_zero_end_7400_binary", 4812, -1, 0, 4, 0, 0, 0, "node" },
	{ "maya_zero_end_7400_binary", 4816, -1, 0, 0, 0, 40, 0, "name" },
	{ "maya_zero_end_7400_binary", 4818, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_zero_end_7400_binary", 4834, -1, 0, 452, 0, 0, 0, "arr" },
	{ "maya_zero_end_7400_binary", 4843, -1, 0, 0, 0, 12379, 0, "data" },
	{ "maya_zero_end_7400_binary", 4878, 12382, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_zero_end_7400_binary", 4885, 16748, 1, 0, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_zero_end_7400_binary", 4898, 12379, 99, 0, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_zero_end_7400_binary", 4914, -1, 0, 0, 0, 12392, 0, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_node_attribute_zoo_7500_binary", 4970, -1, 0, 0, 0, 0, 2909, "res != -28" },
	{ "maya_zero_end_7400_binary", 4971, 12384, 1, 0, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_zero_end_7400_binary", 4974, 12384, 255, 0, 0, 0, 0, "Bad array encoding" },
	{ "maya_cube_big_endian_7100_binary", 4980, -1, 0, 455, 0, 0, 0, "ufbxi_binary_convert_array(uc, src_type, dst_type, deco..." },
	{ "maya_node_attribute_zoo_6100_binary", 4998, 12130, 255, 0, 0, 0, 0, "arr_data" },
	{ "maya_zero_end_7400_binary", 4999, 12379, 101, 0, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_zero_end_7400_binary", 5008, -1, 0, 7, 0, 0, 0, "vals" },
	{ "maya_zero_end_7400_binary", 5016, -1, 0, 0, 0, 87, 0, "data" },
	{ "maya_zero_end_7400_binary", 5070, 331, 0, 0, 0, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &vals[i]...." },
	{ "maya_zero_end_7400_binary", 5080, 593, 8, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_zero_end_7400_binary", 5085, 22, 1, 0, 0, 0, 0, "Bad value type" },
	{ "maya_zero_end_7400_binary", 5096, 66, 4, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_zero_end_7400_binary", 5098, 36, 255, 0, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_zero_end_7400_binary", 5110, 58, 93, 0, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_zero_end_7400_binary", 5115, 70, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_zero_end_7400_binary", 5124, -1, 0, 29, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 5195, -1, 0, 0, 0, 0, 57, "ufbxi_report_progress(uc)" },
	{ "maya_leading_comma_7500_ascii", 5318, -1, 0, 4, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&token..." },
	{ "maya_leading_comma_7500_ascii", 5375, -1, 0, 4, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 5393, -1, 0, 7, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "synthetic_cube_nan_6100_ascii", 5398, 4866, 45, 0, 0, 0, 0, "token->type == 'F'" },
	{ "maya_leading_comma_7500_ascii", 5420, 288, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_leading_comma_7500_ascii", 5427, 3707, 45, 0, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_textured_cube_7500_ascii", 5434, -1, 0, 793, 0, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 5436, 292, 0, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_leading_comma_7500_ascii", 5455, 288, 45, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 5467, 2537, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 5473, 168, 0, 0, 0, 0, 0, "depth == 0" },
	{ "maya_leading_comma_7500_ascii", 5481, 0, 60, 0, 0, 0, 0, "Expected a 'Name:' token" },
	{ "maya_leading_comma_7500_ascii", 5483, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_leading_comma_7500_ascii", 5487, -1, 0, 0, 1, 0, 0, "name" },
	{ "maya_leading_comma_7500_ascii", 5492, -1, 0, 5, 0, 0, 0, "node" },
	{ "maya_leading_comma_7500_ascii", 5515, -1, 0, 445, 0, 0, 0, "arr" },
	{ "maya_leading_comma_7500_ascii", 5531, -1, 0, 446, 0, 0, 0, "ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4)" },
	{ "maya_leading_comma_7500_ascii", 5545, 292, 0, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "max2009_blob_5800_ascii", 5561, -1, 0, 4406, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 5564, -1, 0, 0, 90, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, v)" },
	{ "maya_leading_comma_7500_ascii", 5576, -1, 0, 0, 5, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &v->s)" },
	{ "maya_leading_comma_7500_ascii", 5600, -1, 0, 679, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5601, -1, 0, 461, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", 5602, -1, 0, 3268, 0, 0, 0, "v" },
	{ "maya_blend_shape_cube_7100_ascii", 5603, -1, 0, 932, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5604, -1, 0, 488, 0, 0, 0, "v" },
	{ "max2009_blob_5800_ascii", 5607, 131240, 45, 0, 0, 0, 0, "Bad array dst type" },
	{ "maya_auto_clamp_7100_ascii", 5627, -1, 0, 715, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5628, -1, 0, 447, 0, 0, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", 5656, -1, 0, 6523, 0, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 5663, 8927, 0, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'I')" },
	{ "maya_leading_comma_7500_ascii", 5666, 8931, 11, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, 'N')" },
	{ "maya_leading_comma_7500_ascii", 5685, 8937, 33, 0, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_leading_comma_7500_ascii", 5696, -1, 0, 472, 0, 0, 0, "arr_data" },
	{ "maya_leading_comma_7500_ascii", 5709, -1, 0, 9, 0, 0, 0, "node->vals" },
	{ "maya_leading_comma_7500_ascii", 5719, 168, 11, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_leading_comma_7500_ascii", 5726, -1, 0, 29, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 5743, -1, 0, 0, 0, 1, 0, "header" },
	{ "maya_cube_big_endian_6100_binary", 5757, -1, 0, 4, 0, 0, 0, "version_word" },
	{ "maya_leading_comma_7500_ascii", 5775, -1, 0, 4, 0, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_leading_comma_7500_ascii", 5795, 100, 33, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_zero_end_7400_binary", 5797, 35, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, 1)" },
	{ "maya_leading_comma_7500_ascii", 5825, 0, 60, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_zero_end_7400_binary", 5827, 22, 1, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_leading_comma_7500_ascii", 5839, -1, 0, 6, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 5855, 1544, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_leading_comma_7500_ascii", 5863, -1, 0, 132, 0, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 5880, 100, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_pars..." },
	{ "max2009_blob_5800_ascii", 5905, 12, 0, 0, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_zero_end_7500_binary", 5907, 24, 0, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_node_attribute_zoo_6100_binary", 6138, -1, 0, 42, 0, 0, 0, "ufbxi_map_grow_size((&uc->node_prop_set), sizeof(const ..." },
	{ "maya_leading_comma_7500_ascii", 6164, -1, 0, 1, 0, 0, 0, "ufbxi_map_grow_size((&uc->prop_type_map), sizeof(ufbxi_..." },
	{ "maya_leading_comma_7500_ascii", 6170, -1, 0, 3, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 6183, 561, 0, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_leading_comma_7500_ascii", 6186, 587, 0, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_leading_comma_7500_ascii", 6238, -1, 0, 85, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 6262, -1, 0, 0, 28, 0, 0, "props->props" },
	{ "maya_leading_comma_7500_ascii", 6265, 561, 0, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_leading_comma_7500_ascii", 6269, -1, 0, 85, 0, 0, 0, "ufbxi_sort_properties(uc, props->props, props->num_prop..." },
	{ "maya_leading_comma_7500_ascii", 6292, 561, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.metadata.sce..." },
	{ "maya_leading_comma_7500_ascii", 6304, 100, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 6321, 561, 0, 0, 0, 0, 0, "ufbxi_read_scene_info(uc, child)" },
	{ "maya_leading_comma_7500_ascii", 6436, 2615, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 6455, 3021, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object)" },
	{ "maya_leading_comma_7500_ascii", 6462, -1, 0, 165, 0, 0, 0, "tmpl" },
	{ "maya_leading_comma_7500_ascii", 6463, 3061, 33, 0, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_leading_comma_7500_ascii", 6469, 3159, 0, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_leading_comma_7500_ascii", 6475, -1, 0, 0, 32, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &tmpl->su..." },
	{ "maya_leading_comma_7500_ascii", 6478, 3203, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_leading_comma_7500_ascii", 6484, -1, 0, 0, 101, 0, 0, "uc->templates" },
	{ "maya_node_attribute_zoo_7500_binary", 6547, -1, 0, 0, 247, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, type)" },
	{ "maya_leading_comma_7500_ascii", 6548, -1, 0, 0, 123, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, name)" },
	{ "maya_leading_comma_7500_ascii", 6549, 8892, 0, 0, 0, 0, 0, "ufbxi_check_string(*type)" },
	{ "maya_zero_end_7400_binary", 6550, 12340, 2, 0, 0, 0, 0, "ufbxi_check_string(*name)" },
	{ "maya_leading_comma_7500_ascii", 6561, -1, 0, 148, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_o..." },
	{ "maya_leading_comma_7500_ascii", 6562, -1, 0, 149, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_element_offsets..." },
	{ "maya_leading_comma_7500_ascii", 6566, -1, 0, 150, 0, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", 6575, -1, 0, 151, 0, 0, 0, "entry" },
	{ "maya_node_attribute_zoo_6100_binary", 6588, -1, 0, 4989, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_typed_element_o..." },
	{ "maya_node_attribute_zoo_6100_binary", 6589, -1, 0, 4984, 0, 0, 0, "(size_t*)ufbxi_push_size_copy((&uc->tmp_element_offsets..." },
	{ "maya_node_attribute_zoo_6100_binary", 6593, -1, 0, 4987, 0, 0, 0, "elem" },
	{ "blender_279_sausage_6100_ascii", 6606, -1, 0, 9862, 0, 0, 0, "entry" },
	{ "maya_leading_comma_7500_ascii", 6620, -1, 0, 762, 0, 0, 0, "elem_node" },
	{ "maya_node_attribute_zoo_6100_binary", 6621, -1, 0, 1216, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "maya_leading_comma_7500_ascii", 6629, -1, 0, 806, 0, 0, 0, "elem" },
	{ "maya_node_attribute_zoo_6100_binary", 6636, -1, 0, 280, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", 6646, -1, 0, 4993, 0, 0, 0, "conn" },
	{ "maya_blend_shape_cube_6100_binary", 6657, -1, 0, 382, 0, 0, 0, "conn" },
	{ "fuzz_0272", 6669, -1, 0, 452, 0, 0, 0, "unknown" },
	{ "maya_zero_end_7400_binary", 6690, 12588, 0, 0, 0, 0, 0, "num_elems > 0 && num_elems < 2147483647i32" },
	{ "fuzz_0397", 6695, -1, 0, 0, 99, 0, 0, "new_indices" },
	{ "maya_leading_comma_7500_ascii", 6745, 9370, 43, 0, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_leading_comma_7500_ascii", 6756, 9278, 78, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MappingInformationType, \"C..." },
	{ "maya_zero_end_7400_binary", 6779, 12588, 0, 0, 0, 0, 0, "ufbxi_check_indices(uc, p_dst_index, index_data, 1, num..." },
	{ "synthetic_indexed_by_vertex_7500_ascii", 6786, -1, 0, 0, 114, 0, 0, "new_index_data" },
	{ "maya_leading_comma_7500_ascii", 6811, 10556, 67, 0, 0, 0, 0, "Invalid mapping" },
	{ "fuzz_0393", 6825, -1, 0, 0, 99, 0, 0, "index_data" },
	{ "maya_uv_set_tangents_6100_binary", 6835, 6895, 0, 0, 0, 0, 0, "ufbxi_check_indices(uc, p_dst_index, mesh->vertex_posit..." },
	{ "maya_leading_comma_7500_ascii", 6846, 9303, 67, 0, 0, 0, 0, "Invalid mapping" },
	{ "maya_leading_comma_7500_ascii", 6856, 10999, 84, 0, 0, 0, 0, "arr" },
	{ "maya_blend_shape_cube_6100_binary", 6914, -1, 0, 383, 0, 0, 0, "shape" },
	{ "maya_blend_shape_cube_6100_binary", 6924, 9533, 11, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_blend_shape_cube_6100_binary", 6925, 9493, 3, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "synthetic_blend_shape_order_7500_ascii", 6945, -1, 0, 729, 0, 0, 0, "offsets" },
	{ "maya_blend_shape_cube_6100_binary", 6975, 9466, 0, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "maya_blend_shape_cube_6100_binary", 6979, -1, 0, 375, 0, 0, 0, "deformer" },
	{ "maya_blend_shape_cube_6100_binary", 6980, -1, 0, 378, 0, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 6985, -1, 0, 379, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_6100_binary", 6988, -1, 0, 381, 0, 0, 0, "(ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_we..." },
	{ "maya_blend_shape_cube_6100_binary", 6992, -1, 0, 0, 41, 0, 0, "shape_props" },
	{ "maya_blend_shape_cube_6100_binary", 7003, -1, 0, 382, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "max7_blend_cube_5000_binary", 7005, -1, 0, 314, 0, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "maya_blend_shape_cube_6100_binary", 7020, 9493, 3, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "maya_blend_shape_cube_6100_binary", 7022, -1, 0, 386, 0, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 7023, -1, 0, 387, 0, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "maya_leading_comma_7500_ascii", 7037, -1, 0, 737, 0, 0, 0, "mesh" },
	{ "maya_blend_shape_cube_6100_binary", 7041, 9466, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "maya_leading_comma_7500_ascii", 7060, 8926, 43, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_leading_comma_7500_ascii", 7086, -1, 0, 0, 115, 0, 0, "edges" },
	{ "maya_node_attribute_zoo_7500_ascii", 7109, -1, 0, 0, 0, 0, 28459, "index_ix >= 0 && (size_t)index_ix < mesh->num_indices" },
	{ "maya_leading_comma_7500_ascii", 7125, -1, 0, 0, 116, 0, 0, "mesh->faces" },
	{ "maya_leading_comma_7500_ascii", 7151, 9073, 43, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_leading_comma_7500_ascii", 7161, -1, 0, 0, 117, 0, 0, "mesh->vertex_first_index" },
	{ "maya_leading_comma_7500_ascii", 7197, -1, 0, 739, 0, 0, 0, "bitangents" },
	{ "maya_leading_comma_7500_ascii", 7198, -1, 0, 740, 0, 0, 0, "tangents" },
	{ "maya_leading_comma_7500_ascii", 7202, -1, 0, 0, 118, 0, 0, "mesh->uv_sets.data" },
	{ "maya_color_sets_6100_binary", 7203, -1, 0, 0, 52, 0, 0, "mesh->color_sets.data" },
	{ "maya_leading_comma_7500_ascii", 7212, 9278, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_no..." },
	{ "maya_leading_comma_7500_ascii", 7219, 9692, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.dat..." },
	{ "maya_leading_comma_7500_ascii", 7228, 10114, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.dat..." },
	{ "maya_leading_comma_7500_ascii", 7243, 10531, 78, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_uv...." },
	{ "maya_color_sets_6100_binary", 7256, 7000, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_col..." },
	{ "maya_cone_6100_binary", 7261, 16081, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_cr..." },
	{ "maya_cone_6100_binary", 7264, 15524, 255, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "maya_cone_6100_binary", 7267, 15571, 255, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease, n, u..." },
	{ "maya_leading_comma_7500_ascii", 7271, 10925, 78, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "maya_leading_comma_7500_ascii", 7274, 10999, 84, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing, n..." },
	{ "blender_279_ball_6100_ascii", 7277, 18422, 84, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_smoothing, n..." },
	{ "maya_leading_comma_7500_ascii", 7282, 11116, 78, 0, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "blender_279_ball_6100_ascii", 7284, 18755, 78, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material, n,..." },
	{ "maya_leading_comma_7500_ascii", 7287, 11198, 78, 0, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_zero_end_7400_binary", 7302, 12861, 0, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "blender_279_uv_sets_6100_ascii", 7319, -1, 0, 0, 46, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &prop_nam..." },
	{ "blender_279_uv_sets_6100_ascii", 7325, -1, 0, 719, 0, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", 7409, -1, 0, 720, 0, 0, 0, "extra" },
	{ "blender_279_uv_sets_6100_ascii", 7412, -1, 0, 722, 0, 0, 0, "extra->texture_arr" },
	{ "maya_node_attribute_zoo_6100_binary", 7450, -1, 0, 4126, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 7453, 138209, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->topol..." },
	{ "maya_node_attribute_zoo_6100_binary", 7454, 138308, 255, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Dimension, \"I\", &nurbs->t..." },
	{ "maya_node_attribute_zoo_6100_binary", 7455, 138332, 3, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 7461, 138359, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 7462, 138416, 1, 0, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 7463, 143462, 27, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 7477, -1, 0, 4197, 0, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 7480, 139478, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 7481, 139592, 1, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"II\", &nurbs-..." },
	{ "maya_node_attribute_zoo_6100_binary", 7482, 139631, 3, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 7490, 139691, 3, 0, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 7491, 139727, 1, 0, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 7492, 140321, 3, 0, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 7493, 141818, 63, 0, 0, 0, 0, "points->size % 4 == 0" },
	{ "max_curve_line_7500_binary", 7509, -1, 0, 427, 0, 0, 0, "line" },
	{ "max_curve_line_7500_binary", 7514, 13861, 255, 0, 0, 0, 0, "points" },
	{ "max_curve_line_7500_binary", 7515, 13985, 56, 0, 0, 0, 0, "points_index" },
	{ "max_curve_line_7500_ascii", 7516, 8302, 43, 0, 0, 0, 0, "points->size % 3 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", 7578, -1, 0, 709, 0, 0, 0, "bone" },
	{ "blender_279_sausage_6100_ascii", 7590, -1, 0, 6336, 0, 0, 0, "skin" },
	{ "blender_279_sausage_6100_ascii", 7620, -1, 0, 6534, 0, 0, 0, "cluster" },
	{ "blender_279_sausage_7400_binary", 7626, 23076, 0, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "blender_279_sausage_7400_binary", 7635, 23900, 0, 0, 0, 0, 0, "transform->size >= 16" },
	{ "blender_279_sausage_7400_binary", 7636, 24063, 0, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "maya_blend_shape_cube_7700_binary", 7648, -1, 0, 674, 0, 0, 0, "channel" },
	{ "maya_blend_shape_cube_7700_binary", 7656, -1, 0, 676, 0, 0, 0, "(ufbx_real_list*)ufbxi_push_size_copy((&uc->tmp_full_we..." },
	{ "maya_node_attribute_zoo_7500_binary", 7692, -1, 0, 1735, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 7697, 61038, 255, 0, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 7698, 61115, 255, 0, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 7699, 61175, 255, 0, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 7700, 61234, 255, 0, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 7701, 61292, 255, 0, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_binary", 7704, 61122, 0, 0, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_binary", 7709, 61242, 0, 0, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_binary", 7710, 61300, 0, 0, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", 7714, -1, 0, 0, 248, 0, 0, "keys" },
	{ "maya_resampled_7500_binary", 7738, 24917, 23, 0, 0, 0, 0, "p_ref < p_ref_end" },
	{ "maya_node_attribute_zoo_7500_binary", 7865, 61431, 0, 0, 0, 0, 0, "refs_left >= 0" },
	{ "maya_leading_comma_7500_ascii", 7881, -1, 0, 791, 0, 0, 0, "material" },
	{ "maya_textured_cube_6100_binary", 7893, -1, 0, 1186, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 7911, -1, 0, 1451, 0, 0, 0, "texture" },
	{ "maya_texture_layers_6100_binary", 7919, -1, 0, 1453, 0, 0, 0, "extra" },
	{ "maya_textured_cube_6100_binary", 7961, -1, 0, 810, 0, 0, 0, "video" },
	{ "maya_textured_cube_7500_ascii", 7982, -1, 0, 0, 159, 0, 0, "video->content" },
	{ "synthetic_missing_version_6100_ascii", 7998, -1, 0, 3873, 0, 0, 0, "pose" },
	{ "blender_279_sausage_7400_binary", 8019, 21748, 0, 0, 0, 0, 0, "matrix->size >= 16" },
	{ "synthetic_missing_version_6100_ascii", 8022, -1, 0, 3877, 0, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", 8032, -1, 0, 3878, 0, 0, 0, "pose->bone_poses.data" },
	{ "maya_arnold_textures_6100_binary", 8049, -1, 0, 1517, 0, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", 8063, -1, 0, 1519, 0, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", 8078, -1, 0, 0, 241, 0, 0, "bindings->prop_bindings.data" },
	{ "max_selection_sets_6100_binary", 8090, -1, 0, 544, 0, 0, 0, "set" },
	{ "max_selection_sets_6100_binary", 8107, -1, 0, 415, 0, 0, 0, "sel" },
	{ "maya_character_6100_ascii", 8126, -1, 0, 13683, 0, 0, 0, "character" },
	{ "maya_constraint_zoo_6100_binary", 8152, -1, 0, 3477, 0, 0, 0, "constraint" },
	{ "maya_node_attribute_zoo_6100_binary", 8188, 6671, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &attrib_ty..." },
	{ "maya_node_attribute_zoo_6100_binary", 8203, -1, 0, 274, 0, 0, 0, "entry" },
	{ "maya_node_attribute_zoo_6100_binary", 8212, -1, 0, 275, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_6100_binary", 8222, -1, 0, 0, 22, 0, 0, "attrib_info.props.props" },
	{ "maya_node_attribute_zoo_6100_binary", 8227, 12128, 23, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_node_attribute_zoo_6100_binary", 8229, -1, 0, 1442, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8231, -1, 0, 1210, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8233, -1, 0, 709, 0, 0, 0, "ufbxi_read_bone(uc, node, &attrib_info, sub_type)" },
	{ "maya_node_attribute_zoo_6100_binary", 8235, -1, 0, 278, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8248, -1, 0, 2591, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8252, -1, 0, 1959, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", 8254, -1, 0, 254, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8256, -1, 0, 2781, 0, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", 8262, -1, 0, 280, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_leading_comma_7500_ascii", 8268, 1584, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_leading_comma_7500_ascii", 8277, 8861, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_node_attribute_zoo_6100_binary", 8281, 157532, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_zero_end_7400_binary", 8294, 12333, 255, 0, 0, 0, 0, "(info.fbx_id & (0x8000000000000000ULL)) == 0" },
	{ "maya_leading_comma_7500_ascii", 8308, 8892, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_leading_comma_7500_ascii", 8311, 11807, 0, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_node_attribute_zoo_6100_binary", 8316, 6671, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, type_st..." },
	{ "maya_leading_comma_7500_ascii", 8318, -1, 0, 762, 0, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", 8321, -1, 0, 653, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_6100_binary", 8323, -1, 0, 3865, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_7500_binary", 8325, -1, 0, 584, 0, 0, 0, "ufbxi_read_bone(uc, node, &info, sub_type)" },
	{ "maya_node_attribute_zoo_7500_binary", 8327, -1, 0, 492, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", 8329, -1, 0, 704, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_..." },
	{ "maya_node_attribute_zoo_7500_binary", 8333, -1, 0, 1139, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_..." },
	{ "maya_leading_comma_7500_ascii", 8339, 8926, 43, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_blend_shape_cube_7700_binary", 8341, 19502, 0, 0, 0, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 8343, 138209, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 8345, 139478, 3, 0, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "max_curve_line_7500_binary", 8347, 13861, 255, 0, 0, 0, 0, "ufbxi_read_line(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 8349, -1, 0, 4333, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", 8351, -1, 0, 4378, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "fuzz_0561", 8353, -1, 0, 453, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "blender_279_sausage_6100_ascii", 8357, -1, 0, 6336, 0, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_6100_ascii", 8359, -1, 0, 6534, 0, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "maya_blend_shape_cube_7700_binary", 8361, -1, 0, 657, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "maya_blend_shape_cube_7700_binary", 8363, -1, 0, 674, 0, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "maya_cache_sine_6100_binary", 8365, -1, 0, 1214, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_d..." },
	{ "maya_leading_comma_7500_ascii", 8370, -1, 0, 791, 0, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_textured_cube_6100_binary", 8372, -1, 0, 1186, 0, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "maya_texture_layers_6100_binary", 8374, -1, 0, 1451, 0, 0, 0, "ufbxi_read_layered_texture(uc, node, &info)" },
	{ "maya_textured_cube_6100_binary", 8376, -1, 0, 810, 0, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", 8378, -1, 0, 806, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "maya_leading_comma_7500_ascii", 8380, -1, 0, 811, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_node_attribute_zoo_7500_binary", 8382, -1, 0, 1745, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 8384, 61038, 255, 0, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "synthetic_missing_version_6100_ascii", 8386, -1, 0, 3873, 0, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "maya_arnold_textures_6100_binary", 8388, -1, 0, 1349, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", 8390, -1, 0, 1517, 0, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "max_selection_sets_6100_binary", 8393, -1, 0, 544, 0, 0, 0, "ufbxi_read_selection_set(uc, node, &info)" },
	{ "maya_display_layers_6100_binary", 8397, -1, 0, 1533, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_display..." },
	{ "max_selection_sets_6100_binary", 8400, -1, 0, 415, 0, 0, 0, "ufbxi_read_selection_node(uc, node, &info)" },
	{ "maya_character_6100_ascii", 8403, -1, 0, 13683, 0, 0, 0, "ufbxi_read_character(uc, node, &info)" },
	{ "maya_constraint_zoo_6100_binary", 8405, -1, 0, 3477, 0, 0, 0, "ufbxi_read_constraint(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 8408, -1, 0, 0, 309, 0, 0, "ufbxi_read_scene_info(uc, node)" },
	{ "maya_cache_sine_6100_binary", 8410, -1, 0, 1280, 0, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_f..." },
	{ "fuzz_0272", 8414, -1, 0, 452, 0, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "maya_leading_comma_7500_ascii", 8426, 13120, 33, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_leading_comma_7500_ascii", 8472, -1, 0, 816, 0, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", 8494, -1, 0, 4994, 0, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_6100_binary", 8496, -1, 0, 4996, 0, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "maya_node_attribute_zoo_6100_binary", 8501, 163331, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "maya_node_attribute_zoo_6100_binary", 8504, 163352, 1, 0, 0, 0, 0, "curve->keyframes.data" },
	{ "maya_interpolation_modes_6100_binary", 8518, 16936, 0, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_game_sausage_6100_binary_deform", 8552, 44932, 98, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_interpolation_modes_6100_binary", 8581, 16969, 114, 0, 0, 0, 0, "Unknown slope mode" },
	{ "maya_interpolation_modes_6100_binary", 8585, 16936, 73, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "max_transformed_skin_6100_binary", 8604, 64699, 7, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_interpolation_modes_6100_binary", 8611, 16989, 98, 0, 0, 0, 0, "Unknown weight mode" },
	{ "maya_transform_animation_6100_binary", 8620, 17549, 11, 0, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_node_attribute_zoo_6100_binary", 8624, 163388, 86, 0, 0, 0, 0, "Unknown key mode" },
	{ "maya_node_attribute_zoo_6100_binary", 8629, 163349, 3, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_node_attribute_zoo_6100_binary", 8678, 163349, 1, 0, 0, 0, 0, "data == data_end" },
	{ "synthetic_missing_version_6100_ascii", 8695, 72756, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "synthetic_missing_version_6100_ascii", 8706, 72840, 102, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "maya_node_attribute_zoo_6100_binary", 8749, -1, 0, 4989, 0, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8750, -1, 0, 4993, 0, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "maya_node_attribute_zoo_6100_binary", 8753, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "maya_interpolation_modes_6100_binary", 8766, 16706, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "maya_node_attribute_zoo_6100_binary", 8775, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "maya_node_attribute_zoo_6100_binary", 8787, -1, 0, 4984, 0, 0, 0, "stack" },
	{ "maya_node_attribute_zoo_6100_binary", 8788, 163019, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"S\", &stack->name)" },
	{ "maya_node_attribute_zoo_6100_binary", 8791, -1, 0, 4987, 0, 0, 0, "layer" },
	{ "synthetic_duplicate_prop_6100_ascii", 8793, -1, 0, 608, 0, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8798, 163046, 255, 0, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_ReferenceTime, \"LL\", &beg..." },
	{ "maya_node_attribute_zoo_6100_binary", 8808, 163331, 0, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 8818, 162983, 125, 0, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_node_attribute_zoo_6100_binary", 8822, 163019, 0, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_leading_comma_7500_ascii", 8832, 0, 60, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_leading_comma_7500_ascii", 8833, 100, 33, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "blender_279_default_6100_ascii", 8837, 454, 14, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "maya_node_attribute_zoo_6100_binary", 8846, -1, 0, 42, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_leading_comma_7500_ascii", 8851, 1525, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_leading_comma_7500_ascii", 8852, 2615, 33, 0, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_leading_comma_7500_ascii", 8867, -1, 0, 148, 0, 0, 0, "root" },
	{ "maya_leading_comma_7500_ascii", 8868, -1, 0, 152, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "maya_leading_comma_7500_ascii", 8879, 2808, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_leading_comma_7500_ascii", 8880, 3021, 33, 0, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_leading_comma_7500_ascii", 8883, 8762, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_leading_comma_7500_ascii", 8887, 0, 0, 0, 0, 0, 0, "uc->top_node" },
	{ "maya_leading_comma_7500_ascii", 8889, 8861, 33, 0, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_leading_comma_7500_ascii", 8892, 13016, 11, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_leading_comma_7500_ascii", 8893, 13120, 33, 0, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 8898, 158678, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_node_attribute_zoo_6100_binary", 8899, 162983, 125, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 8903, 162983, 255, 0, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_leading_comma_7500_ascii", 8905, 1584, 0, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "max2009_blob_5800_binary", 9026, -1, 0, 570, 0, 0, 0, "material" },
	{ "max2009_blob_5800_binary", 9034, -1, 0, 0, 110, 0, 0, "material->props.props" },
	{ "max7_skin_5000_binary", 9042, -1, 0, 341, 0, 0, 0, "cluster" },
	{ "max7_skin_5000_binary", 9049, 2420, 136, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "max7_skin_5000_binary", 9058, 4378, 15, 0, 0, 0, 0, "transform->size >= 16" },
	{ "max7_skin_5000_binary", 9059, 4544, 15, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "max2009_blob_5800_binary", 9071, -1, 0, 110, 0, 0, 0, "light" },
	{ "max2009_blob_5800_binary", 9078, -1, 0, 0, 26, 0, 0, "light->props.props" },
	{ "max2009_blob_5800_binary", 9086, -1, 0, 309, 0, 0, 0, "camera" },
	{ "max2009_blob_5800_binary", 9093, -1, 0, 0, 69, 0, 0, "camera->props.props" },
	{ "max7_skin_5000_binary", 9101, -1, 0, 490, 0, 0, 0, "bone" },
	{ "max7_skin_5000_binary", 9113, -1, 0, 0, 36, 0, 0, "bone->props.props" },
	{ "max7_cube_5000_binary", 9126, -1, 0, 277, 0, 0, 0, "mesh" },
	{ "max7_blend_cube_5000_binary", 9128, 2350, 0, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "max7_cube_5000_binary", 9145, 2383, 23, 0, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "max7_cube_5000_binary", 9174, -1, 0, 0, 23, 0, 0, "mesh->faces" },
	{ "max7_cube_5000_binary", 9195, 2383, 0, 0, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "max7_cube_5000_binary", 9203, -1, 0, 0, 24, 0, 0, "mesh->vertex_first_index" },
	{ "max7_cube_5000_binary", 9251, -1, 0, 0, 25, 0, 0, "set" },
	{ "max7_cube_5000_binary", 9258, 2927, 0, 0, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, uv_info, &set->vert..." },
	{ "max7_cube_5000_binary", 9266, 2856, 0, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MaterialAssignation, \"C\",..." },
	{ "max2009_blob_5800_binary", 9268, 56700, 78, 0, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material, no..." },
	{ "max2009_blob_5800_binary", 9295, 6207, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max2009_blob_5800_binary", 9296, 6229, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max2009_blob_5800_binary", 9297, -1, 0, 570, 0, 0, 0, "ufbxi_read_legacy_material(uc, child, &fbx_id, name.dat..." },
	{ "max2009_blob_5800_binary", 9298, -1, 0, 572, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 9302, 2361, 0, 0, 0, 0, 0, "ufbxi_get_val1(child, \"s\", &type_and_name)" },
	{ "max7_skin_5000_binary", 9303, 2379, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max7_skin_5000_binary", 9304, 2420, 136, 0, 0, 0, 0, "ufbxi_read_legacy_link(uc, child, &fbx_id, name.data)" },
	{ "max7_skin_5000_binary", 9307, -1, 0, 344, 0, 0, 0, "ufbxi_connect_oo(uc, node_fbx_id, fbx_id)" },
	{ "max7_skin_5000_binary", 9310, -1, 0, 345, 0, 0, 0, "skin" },
	{ "max7_skin_5000_binary", 9311, -1, 0, 347, 0, 0, 0, "ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id)" },
	{ "max7_skin_5000_binary", 9313, -1, 0, 348, 0, 0, 0, "ufbxi_connect_oo(uc, fbx_id, skin_fbx_id)" },
	{ "max7_cube_5000_binary", 9327, 324, 0, 0, 0, 0, 0, "ufbxi_get_val1(node, \"s\", &type_and_name)" },
	{ "max7_cube_5000_binary", 9328, 343, 0, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type, &na..." },
	{ "max7_cube_5000_binary", 9335, -1, 0, 137, 0, 0, 0, "elem_node" },
	{ "max2009_blob_5800_binary", 9336, -1, 0, 363, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "max7_cube_5000_binary", 9343, -1, 0, 138, 0, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id)" },
	{ "max2009_blob_5800_binary", 9350, -1, 0, 110, 0, 0, 0, "ufbxi_read_legacy_light(uc, node, &attrib_info)" },
	{ "max2009_blob_5800_binary", 9352, -1, 0, 309, 0, 0, 0, "ufbxi_read_legacy_camera(uc, node, &attrib_info)" },
	{ "max7_skin_5000_binary", 9354, -1, 0, 490, 0, 0, 0, "ufbxi_read_legacy_limb_node(uc, node, &attrib_info)" },
	{ "max7_cube_5000_binary", 9356, 2383, 23, 0, 0, 0, 0, "ufbxi_read_legacy_mesh(uc, node, &attrib_info)" },
	{ "max7_cube_5000_binary", 9365, -1, 0, 279, 0, 0, 0, "entry" },
	{ "max7_cube_5000_binary", 9376, -1, 0, 139, 0, 0, 0, "ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id)" },
	{ "max7_cube_5000_binary", 9389, 942, 0, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc..." },
	{ "max7_cube_5000_binary", 9400, -1, 0, 4, 0, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "max7_cube_5000_binary", 9407, -1, 0, 5, 0, 0, 0, "root" },
	{ "max7_cube_5000_binary", 9408, -1, 0, 13, 0, 0, 0, "(uint32_t*)ufbxi_push_size_copy((&uc->tmp_node_ids), si..." },
	{ "maya_zero_end_7500_binary", 9412, 24, 0, 0, 0, 0, 0, "ufbxi_parse_legacy_toplevel(uc)" },
	{ "fuzz_0018", 9417, 810, 0, 0, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "max2009_blob_5800_binary", 9419, 113382, 0, 0, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "max7_cube_5000_binary", 9423, 324, 0, 0, 0, 0, 0, "ufbxi_read_legacy_model(uc, node)" },
	{ "max7_cube_5000_binary", 9435, -1, 0, 1215, 0, 0, 0, "layer" },
	{ "max7_cube_5000_binary", 9440, -1, 0, 1218, 0, 0, 0, "stack" },
	{ "max7_cube_normals_5000_binary", 9442, -1, 0, 1220, 0, 0, 0, "ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_..." },
	{ "fuzz_0491", 9476, -1, 0, 26, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "fuzz_0491", 9496, -1, 0, 23, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "blender_279_sausage_6100_ascii", 9532, -1, 0, 10854, 0, 0, 0, "ufbxi_grow_array_size((&uc->ator_tmp), sizeof(**(&uc->t..." },
	{ "maya_leading_comma_7500_ascii", 9598, -1, 0, 827, 0, 0, 0, "tmp_connections" },
	{ "maya_leading_comma_7500_ascii", 9602, -1, 0, 0, 137, 0, 0, "uc->scene.connections_src.data" },
	{ "maya_leading_comma_7500_ascii", 9632, -1, 0, 0, 138, 0, 0, "uc->scene.connections_dst.data" },
	{ "blender_279_sausage_6100_ascii", 9634, -1, 0, 10854, 0, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "maya_node_attribute_zoo_7500_binary", 9713, -1, 0, 2093, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_7500_binary", 9738, -1, 0, 2094, 0, 0, 0, "new_prop" },
	{ "maya_node_attribute_zoo_7500_binary", 9752, -1, 0, 2095, 0, 0, 0, "(ufbx_prop*)ufbxi_push_size_copy((&uc->tmp_stack), size..." },
	{ "maya_node_attribute_zoo_7500_binary", 9754, -1, 0, 0, 277, 0, 0, "elem->props.props" },
	{ "maya_leading_comma_7500_ascii", 9772, -1, 0, 828, 0, 0, 0, "node_ids" },
	{ "maya_leading_comma_7500_ascii", 9775, -1, 0, 829, 0, 0, 0, "node_ptrs" },
	{ "maya_leading_comma_7500_ascii", 9786, -1, 0, 830, 0, 0, 0, "node_offsets" },
	{ "maya_game_sausage_6100_binary", 9815, 48802, 49, 0, 0, 0, 0, "depth <= num_nodes" },
	{ "fuzz_0491", 9827, -1, 0, 23, 0, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "maya_leading_comma_7500_ascii", 9831, -1, 0, 831, 0, 0, 0, "p_offset" },
	{ "maya_leading_comma_7500_ascii", 9898, -1, 0, 837, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_node_attribute_zoo_6100_binary", 9907, -1, 0, 0, 375, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 9920, -1, 0, 835, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_leading_comma_7500_ascii", 9929, -1, 0, 0, 143, 0, 0, "list->data" },
	{ "maya_textured_cube_7500_binary", 9971, -1, 0, 1111, 0, 0, 0, "tex" },
	{ "maya_textured_cube_7500_binary", 9981, -1, 0, 0, 220, 0, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", 9995, -1, 0, 836, 0, 0, 0, "(ufbx_mesh_material*)ufbxi_push_size_copy((&uc->tmp_sta..." },
	{ "maya_leading_comma_7500_ascii", 10003, -1, 0, 0, 146, 0, 0, "list->data" },
	{ "blender_279_sausage_7400_binary", 10017, -1, 0, 4351, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "blender_293_half_skinned_7400_binary", 10025, -1, 0, 0, 138, 0, 0, "list->data" },
	{ "maya_blend_shape_cube_6100_binary", 10038, -1, 0, 683, 0, 0, 0, "(ufbx_blend_keyframe*)ufbxi_push_size_copy((&uc->tmp_st..." },
	{ "maya_blend_shape_cube_6100_binary", 10045, -1, 0, 0, 121, 0, 0, "list->data" },
	{ "maya_texture_layers_6100_binary", 10061, -1, 0, 1643, 0, 0, 0, "(ufbx_texture_layer*)ufbxi_push_size_copy((&uc->tmp_sta..." },
	{ "maya_texture_layers_6100_binary", 10068, -1, 0, 0, 191, 0, 0, "list->data" },
	{ "maya_constraint_zoo_6100_binary", 10493, -1, 0, 3968, 0, 0, 0, "target" },
	{ "maya_leading_comma_7500_ascii", 10507, -1, 0, 0, 134, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "blender_279_edge_vertex_7400_binary", 10522, -1, 0, 0, 96, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, &uc->scen..." },
	{ "maya_textured_cube_6100_binary", 10548, -1, 0, 1661, 0, 0, 0, "result" },
	{ "maya_textured_cube_6100_binary", 10569, -1, 0, 0, 192, 0, 0, "ufbxi_push_string_place_str(&uc->string_pool, dst)" },
	{ "maya_leading_comma_7500_ascii", 10581, -1, 0, 0, 135, 0, 0, "uc->scene.elements.data" },
	{ "maya_leading_comma_7500_ascii", 10586, -1, 0, 0, 136, 0, 0, "element_data" },
	{ "maya_leading_comma_7500_ascii", 10590, -1, 0, 826, 0, 0, 0, "element_offsets" },
	{ "maya_leading_comma_7500_ascii", 10598, -1, 0, 827, 0, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", 10599, -1, 0, 2093, 0, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "maya_leading_comma_7500_ascii", 10600, -1, 0, 828, 0, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_leading_comma_7500_ascii", 10606, -1, 0, 832, 0, 0, 0, "typed_offsets" },
	{ "maya_leading_comma_7500_ascii", 10611, -1, 0, 0, 139, 0, 0, "typed_elems->data" },
	{ "maya_leading_comma_7500_ascii", 10623, -1, 0, 0, 142, 0, 0, "uc->scene.elements_by_name.data" },
	{ "fuzz_0491", 10636, -1, 0, 26, 0, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "maya_node_attribute_zoo_6100_binary", 10661, -1, 0, 5065, 0, 0, 0, "(ufbx_element**)ufbxi_push_size_copy((&uc->tmp_stack), ..." },
	{ "maya_node_attribute_zoo_6100_binary", 10677, -1, 0, 0, 361, 0, 0, "node->all_attribs.data" },
	{ "synthetic_missing_version_6100_ascii", 10691, -1, 0, 0, 197, 0, 0, "pose->bone_poses.data" },
	{ "maya_leading_comma_7500_ascii", 10722, -1, 0, 835, 0, 0, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, 0,..." },
	{ "blender_279_sausage_6100_ascii", 10735, -1, 0, 10884, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_6100_ascii", 10778, -1, 0, 0, 340, 0, 0, "skin->vertices.data" },
	{ "blender_279_sausage_6100_ascii", 10782, -1, 0, 0, 341, 0, 0, "skin->weights.data" },
	{ "maya_blend_shape_cube_6100_binary", 10843, -1, 0, 681, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "maya_cache_sine_6100_binary", 10862, -1, 0, 1461, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &cache->filename, c..." },
	{ "maya_blend_shape_cube_6100_binary", 10868, -1, 0, 682, 0, 0, 0, "full_weights" },
	{ "maya_blend_shape_cube_6100_binary", 10873, -1, 0, 683, 0, 0, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "maya_leading_comma_7500_ascii", 10893, -1, 0, 0, 144, 0, 0, "zero_indices && consecutive_indices" },
	{ "maya_leading_comma_7500_ascii", 10935, -1, 0, 836, 0, 0, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "blender_279_ball_6100_ascii", 10987, -1, 0, 0, 175, 0, 0, "mat->face_indices" },
	{ "blender_279_sausage_7400_binary", 11007, -1, 0, 4350, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &me..." },
	{ "maya_blend_shape_cube_6100_binary", 11008, -1, 0, 686, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &m..." },
	{ "maya_cache_sine_6100_binary", 11009, -1, 0, 1464, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &m..." },
	{ "blender_279_sausage_7400_binary", 11010, -1, 0, 4351, 0, 0, 0, "ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->..." },
	{ "maya_leading_comma_7500_ascii", 11027, -1, 0, 837, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_leading_comma_7500_ascii", 11031, -1, 0, 0, 147, 0, 0, "stack->anim.layers.data" },
	{ "maya_node_attribute_zoo_6100_binary", 11042, -1, 0, 5084, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "maya_leading_comma_7500_ascii", 11045, -1, 0, 0, 148, 0, 0, "layer_desc" },
	{ "maya_node_attribute_zoo_6100_binary", 11066, -1, 0, 5086, 0, 0, 0, "aprop" },
	{ "maya_leading_comma_7500_ascii", 11117, -1, 0, 838, 0, 0, 0, "aprop" },
	{ "maya_leading_comma_7500_ascii", 11121, -1, 0, 0, 149, 0, 0, "layer->anim_props.data" },
	{ "maya_arnold_textures_6100_binary", 11157, -1, 0, 1753, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader..." },
	{ "maya_textured_cube_7500_binary", 11187, -1, 0, 1111, 0, 0, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "maya_textured_cube_6100_binary", 11202, -1, 0, 1643, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "maya_textured_cube_6100_binary", 11210, -1, 0, 1647, 0, 0, 0, "mat_texs" },
	{ "maya_shared_textures_6100_binary", 11232, -1, 0, 2377, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 11245, -1, 0, 3808, 0, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", 11252, -1, 0, 3810, 0, 0, 0, "mat_texs" },
	{ "maya_textured_cube_6100_binary", 11266, -1, 0, 0, 191, 0, 0, "texs" },
	{ "maya_textured_cube_6100_binary", 11285, -1, 0, 1654, 0, 0, 0, "tex" },
	{ "maya_textured_cube_6100_binary", 11325, -1, 0, 1660, 0, 0, 0, "content_videos" },
	{ "maya_textured_cube_6100_binary", 11330, -1, 0, 1661, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &video->filename, v..." },
	{ "maya_textured_cube_6100_binary", 11358, -1, 0, 1667, 0, 0, 0, "ufbxi_resolve_relative_filename(uc, &texture->filename,..." },
	{ "maya_texture_layers_6100_binary", 11375, -1, 0, 1643, 0, 0, 0, "ufbxi_fetch_texture_layers(uc, &texture->layers, &textu..." },
	{ "maya_display_layers_6100_binary", 11392, -1, 0, 1683, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->ele..." },
	{ "max_selection_sets_6100_binary", 11397, -1, 0, 841, 0, 0, 0, "ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element..." },
	{ "maya_constraint_zoo_6100_binary", 11424, -1, 0, 3968, 0, 0, 0, "ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)c..." },
	{ "maya_constraint_zoo_6100_binary", 11430, -1, 0, 0, 249, 0, 0, "constraint->targets.data" },
	{ "maya_leading_comma_7500_ascii", 11443, -1, 0, 0, 150, 0, 0, "descs" },
	{ "maya_cache_sine_6100_binary", 12523, -1, 0, 1586, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->na..." },
	{ "maya_cache_sine_6100_binary", 12540, -1, 0, 1587, 0, 0, 0, "frame" },
	{ "max_cache_box_7500_binary", 12581, -1, 0, 658, 0, 0, 0, "frames" },
	{ "maya_cache_sine_6100_binary", 12618, -1, 0, 1584, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_cache_sine_6100_binary", 12640, -1, 0, 1579, 0, 0, 0, "extra" },
	{ "maya_cache_sine_6100_binary", 12642, -1, 0, 0, 182, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, extra)" },
	{ "maya_cache_sine_6100_binary", 12647, -1, 0, 0, 183, 0, 0, "cc->cache.extra_info.data" },
	{ "maya_cache_sine_6100_binary", 12680, -1, 0, 1582, 0, 0, 0, "cc->channels" },
	{ "maya_cache_sine_6100_binary", 12708, -1, 0, 1584, 0, 0, 0, "ufbxi_cache_sort_tmp_channels(cc, cc->channels, cc->num..." },
	{ "maya_cache_sine_6100_binary", 12721, -1, 0, 1475, 0, 0, 0, "doc" },
	{ "maya_cache_sine_6100_binary", 12725, -1, 0, 1579, 0, 0, 0, "xml_ok" },
	{ "maya_cache_sine_6100_binary", 12733, -1, 0, 0, 184, 0, 0, "ufbxi_push_string_place_str(&cc->string_pool, &cc->stre..." },
	{ "max_cache_box_7500_binary", 12745, -1, 0, 658, 0, 0, 0, "ufbxi_cache_load_pc2(cc)" },
	{ "maya_cache_sine_6100_binary", 12747, -1, 0, 1586, 0, 0, 0, "ufbxi_cache_load_mc(cc)" },
	{ "maya_cache_sine_6100_binary", 12749, -1, 0, 1475, 0, 0, 0, "ufbxi_cache_load_xml(cc)" },
	{ "maya_cache_sine_6100_binary", 12789, -1, 0, 1585, 0, 0, 0, "name_buf" },
	{ "maya_cache_sine_6100_binary", 12810, -1, 0, 1586, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename, &found)" },
	{ "maya_cache_sine_6100_binary", 12874, -1, 0, 1648, 0, 0, 0, "ufbxi_grow_array_size((cc->ator_tmp), sizeof(**(&cc->tm..." },
	{ "maya_cache_sine_6100_binary", 12903, -1, 0, 1649, 0, 0, 0, "chan" },
	{ "maya_cache_sine_6100_binary", 12933, -1, 0, 0, 186, 0, 0, "cc->cache.channels.data" },
	{ "maya_cache_sine_6100_binary", 12952, -1, 0, 1474, 0, 0, 0, "filename_data" },
	{ "maya_cache_sine_6100_binary", 12959, -1, 0, 1475, 0, 0, 0, "ufbxi_cache_try_open_file(cc, filename_copy, &found)" },
	{ "maya_cache_sine_6100_binary", 12966, -1, 0, 1585, 0, 0, 0, "ufbxi_cache_load_frame_files(cc)" },
	{ "maya_cache_sine_6100_binary", 12971, -1, 0, 0, 185, 0, 0, "cc->cache.frames.data" },
	{ "maya_cache_sine_6100_binary", 12973, -1, 0, 1648, 0, 0, 0, "ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->..." },
	{ "maya_cache_sine_6100_binary", 12974, -1, 0, 1649, 0, 0, 0, "ufbxi_cache_setup_channels(cc)" },
	{ "maya_cache_sine_6100_binary", 12978, -1, 0, 0, 187, 0, 0, "cc->imp" },
	{ "maya_cache_sine_6100_binary", 13135, -1, 0, 1471, 0, 0, 0, "file" },
	{ "maya_cache_sine_6100_binary", 13145, -1, 0, 1473, 0, 0, 0, "files" },
	{ "maya_cache_sine_6100_binary", 13153, -1, 0, 1474, 0, 0, 0, "ufbxi_load_external_cache(uc, file)" },
	{ "maya_leading_comma_7500_ascii", 13333, -1, 0, 1, 0, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_leading_comma_7500_ascii", 13334, -1, 0, 4, 0, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_zero_end_7500_binary", 13336, 24, 0, 0, 0, 0, 0, "ufbxi_read_legacy_root(uc)" },
	{ "maya_leading_comma_7500_ascii", 13338, 0, 60, 0, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_leading_comma_7500_ascii", 13342, -1, 0, 0, 134, 0, 0, "ufbxi_init_file_paths(uc)" },
	{ "maya_leading_comma_7500_ascii", 13343, -1, 0, 826, 0, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_cache_sine_6100_binary", 13349, -1, 0, 1471, 0, 0, 0, "ufbxi_load_external_files(uc)" },
	{ "maya_leading_comma_7500_ascii", 13372, -1, 0, 0, 151, 0, 0, "imp" },
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
				opts.progress_fn = &ufbxt_cancel_progress;
				opts.progress_user = &cancel_ctx;
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

typedef struct {
	uint64_t calls;
} ufbxt_progress_ctx;

bool ufbxt_measure_progress(void *user, const ufbx_progress *progress)
{
	ufbxt_progress_ctx *ctx = (ufbxt_progress_ctx*)user;
	ctx->calls++;
	return true;
}

void ufbxt_do_file_test(const char *name, void (*test_fn)(ufbx_scene *s, ufbxt_diff_error *err, ufbx_error *load_error), const char *suffix, ufbx_load_opts user_opts, bool alternative, bool allow_error)
{
	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s.obj", data_root, name);
	size_t obj_size = 0;
	void *obj_data = ufbxt_read_file(buf, &obj_size);
	ufbxt_obj_file *obj_file = obj_data ? ufbxt_load_obj(obj_data, obj_size) : NULL;
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
			memory_opts.progress_fn = &ufbxt_measure_progress;
			memory_opts.progress_user = &progress_ctx;

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
			stream_opts.progress_fn = &ufbxt_measure_progress;
			stream_opts.progress_user = &stream_progress_ctx;
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

			if (scene && obj_file) {
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
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, get_opts, false, false); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err)
#define UFBXT_FILE_TEST_SUFFIX(name, suffix) void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name##_##suffix(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name##_##suffix, #suffix, user_opts, false, false); } \
	void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_SUFFIX_OPTS(name, suffix, get_opts) void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name##_##suffix(void) { \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name##_##suffix, #suffix, get_opts, false, false); } \
	void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error)
#define UFBXT_FILE_TEST_ALT(name, file) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err, ufbx_error *load_error); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#file, &ufbxt_test_fn_imp_file_##name, NULL, user_opts, true, false); } \
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
#undef UFBXT_FILE_TEST_ALLOW_ERROR
#define UFBXT_IMPL 0
#define UFBXT_TEST(name) { #name, &ufbxt_test_fn_##name },
#define UFBXT_FILE_TEST(name) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS(name, get_opts) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_SUFFIX(name, suffix) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_SUFFIX_OPTS(name, suffix, get_opts) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_ALT(name, file) { #name, &ufbxt_test_fn_file_##name },
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

