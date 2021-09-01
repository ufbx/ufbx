#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr);

#define ufbxt_arraycount(arr) (sizeof(arr) / sizeof(*(arr)))

#include "../ufbx.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <math.h>

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
jmp_buf g_test_jmp;
int g_verbose;

char g_log_buf[16*1024];
uint32_t g_log_pos;

char g_hint[8*1024];

bool g_skip_print_ok = false;

typedef struct {
	char *test_name;
	uint8_t patch_value;
	uint32_t patch_offset;
	uint32_t temp_limit;
	uint32_t result_limit;
	uint32_t truncate_length;
	const char *description;
} ufbxt_check_line;

static ufbxt_check_line g_checks[16384];

ufbxt_threadlocal jmp_buf *t_jmp_buf;

void ufbxt_assert_fail(const char *file, uint32_t line, const char *expr)
{
	if (t_jmp_buf) {
		longjmp(*t_jmp_buf, 1);
	}

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
				ufbx_vec3 v = mesh->vertex_position.data[i];
				v = ufbx_transform_position(&node->geometry_to_world, v);
				fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
			}

			for (size_t i = 0; i < mesh->vertex_uv.num_values; i++) {
				ufbx_vec2 v = mesh->vertex_uv.data[i];
				fprintf(f, "vt %f %f\n", v.x, v.y);
			}

			for (size_t i = 0; i < mesh->vertex_normal.num_values; i++) {
				ufbx_vec3 v = mesh->vertex_normal.data[i];
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
					int32_t vi = v_off + mesh->vertex_position.indices[face.index_begin + ci];
					int32_t ni = n_off + mesh->vertex_normal.indices[face.index_begin + ci];
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

			v_off += (int32_t)mesh->vertex_position.num_values;
			t_off += (int32_t)mesh->vertex_uv.num_values;
			n_off += (int32_t)mesh->vertex_normal.num_values;
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

static void ufbxt_match_obj_mesh(ufbx_node *fbx_node, ufbx_mesh *fbx_mesh, ufbxt_obj_mesh *obj_mesh, ufbxt_diff_error *p_err)
{
	ufbxt_assert(fbx_mesh->num_faces == obj_mesh->num_faces);
	ufbxt_assert(fbx_mesh->num_indices == obj_mesh->num_indices);

	// Check that all vertices exist, anything more doesn't really make sense
	ufbxt_match_vertex *obj_verts = (ufbxt_match_vertex*)calloc(obj_mesh->num_indices, sizeof(ufbxt_match_vertex));
	ufbxt_match_vertex *sub_verts = (ufbxt_match_vertex*)calloc(fbx_mesh->num_indices, sizeof(ufbxt_match_vertex));
	ufbxt_assert(obj_verts && sub_verts);

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
		sub_verts[i].pos = fp;
		if (obj_mesh->vertex_uv.data) {
			ufbxt_assert(fbx_mesh->vertex_uv.data);
			sub_verts[i].uv = ufbx_get_vertex_vec2(&fbx_mesh->vertex_uv, i);
		}
	}

	qsort(obj_verts, obj_mesh->num_indices, sizeof(ufbxt_match_vertex), &ufbxt_cmp_sub_vertex);
	qsort(sub_verts, fbx_mesh->num_indices, sizeof(ufbxt_match_vertex), &ufbxt_cmp_sub_vertex);

	for (int32_t i = (int32_t)fbx_mesh->num_indices - 1; i >= 0; i--) {
		ufbxt_match_vertex v = sub_verts[i];

		bool found = false;
		for (int32_t j = i; j >= 0 && obj_verts[j].pos.x >= v.pos.x - 0.002f; j--) {
			ufbx_real dx = obj_verts[j].pos.x - v.pos.x;
			ufbx_real dy = obj_verts[j].pos.y - v.pos.y;
			ufbx_real dz = obj_verts[j].pos.z - v.pos.z;
			ufbx_real du = obj_verts[j].uv.x - v.uv.x;
			ufbx_real dv = obj_verts[j].uv.y - v.uv.y;
			ufbxt_assert(dx <= 0.002f);
			ufbx_real err = (ufbx_real)sqrt(dx*dx + dy*dy + dz*dz + du*du + dv*dv);
			if (err < 0.001f) {
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
	free(sub_verts);

}

static void ufbxt_diff_to_obj(ufbx_scene *scene, ufbxt_obj_file *obj, ufbxt_diff_error *p_err, bool check_deformed_normals)
{
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
			ufbxt_match_obj_mesh(node, sub_mesh, obj_mesh, p_err);
			ufbx_free_mesh(sub_mesh);

			continue;
		}

		ufbxt_assert(obj_mesh->num_faces == mesh->num_faces);
		ufbxt_assert(obj_mesh->num_indices == mesh->num_indices);

		bool check_normals = true;
		if (obj->bad_normals) check_normals = false;
		if (!check_deformed_normals && mesh->all_deformers.count > 0) check_normals = false;

		if (obj->bad_order) {
			ufbxt_match_obj_mesh(node, mesh, obj_mesh, p_err);
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

					if (obj_mesh->vertex_uv.data) {
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

int ufbxt_test_fuzz(void *data, size_t size, size_t step, int offset, size_t temp_limit, size_t result_limit, size_t truncate_length)
{
	if (g_fuzz_step < SIZE_MAX && step != g_fuzz_step) return 1;

	t_jmp_buf = (jmp_buf*)calloc(1, sizeof(jmp_buf));
	int ret = 1;
	if (!setjmp(*t_jmp_buf)) {

		ufbx_load_opts opts = { 0 };

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
				if (check->test_name && strcmp(g_fuzz_test_name, check->test_name)) continue;
				if ((uint32_t)offset > check->patch_offset - 1) continue;

				#pragma omp critical(check)
				{
					bool ok = (uint32_t)offset <= check->patch_offset - 1;
					if (check->test_name && strcmp(g_fuzz_test_name, check->test_name)) ok = false;

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
						check->temp_limit = (uint32_t)temp_limit;
						check->result_limit = (uint32_t)result_limit;
						check->truncate_length = (uint32_t)truncate_length;
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
	int32_t patch_offset;
	uint8_t patch_value;
	uint32_t temp_limit;
	uint32_t result_limit;
	uint32_t truncate_length;
	const char *description;
} ufbxt_fuzz_check;

// Generated by running `runner --fuzz`
static const ufbxt_fuzz_check g_fuzz_checks[] = {
	{ "maya_zero_end_7400_binary", 12382, 255, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 32, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_zero_end_7400_binary", 16748, 1, 0, 0, 0, "total <= ator->max_size - ator->current_size" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 257, 0, 0, "ator->num_allocs < ator->max_allocs" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 1, 0, 0, "data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 64, 0, "dst" },
	{ "maya_zero_end_7400_binary", 331, 0, 0, 0, 0, "str || length == 0" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 64, 0, "str" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 0, 1, "uc->read_fn" },
	{ "maya_zero_end_7400_binary", 36, 255, 0, 0, 0, "ufbxi_read_bytes(uc, (size_t)to_skip)" },
	{ "maya_zero_end_7400_binary", -1, 0, 0, 0, 12481, "uc->read_fn" },
	{ "maya_cube_6100_binary", -1, 0, 0, 0, 10721, "val" },
	{ "maya_zero_end_7400_binary", 16744, 255, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 12615, 255, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 61146, 255, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_node_attribute_zoo_7500_binary", 61333, 255, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 12379, 255, 0, 0, 0, "Bad multivalue array type" },
	{ "maya_zero_end_7400_binary", 12382, 255, 0, 0, 0, "data" },
	{ "maya_zero_end_7400_binary", -1, 0, 0, 0, 129, "header" },
	{ "maya_zero_end_7400_binary", 24, 255, 0, 0, 0, "num_values64 <= UINT32_MAX" },
	{ "maya_zero_end_7400_binary", -1, 0, 192, 0, 0, "node" },
	{ "maya_zero_end_7400_binary", -1, 0, 0, 0, 49, "name" },
	{ "maya_zero_end_7400_binary", -1, 0, 0, 144, 0, "name" },
	{ "maya_zero_end_7400_binary", -1, 0, 448, 0, 0, "arr" },
	{ "maya_zero_end_7400_binary", -1, 0, 0, 0, 12379, "data" },
	{ "maya_zero_end_7400_binary", 12382, 255, 0, 0, 0, "arr_data" },
	{ "maya_zero_end_7400_binary", 16748, 1, 0, 0, 0, "ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_..." },
	{ "maya_zero_end_7400_binary", 12379, 99, 0, 0, 0, "encoded_size == decoded_data_size" },
	{ "maya_zero_end_7400_binary", -1, 0, 0, 0, 12481, "ufbxi_read_to(uc, decoded_data, encoded_size)" },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 1068, 0, 0, "ufbxi_grow_array(&uc->ator_tmp, &uc->read_buffer, &uc->..." },
	{ "maya_zero_end_7400_binary", 12384, 1, 0, 0, 0, "res == (ptrdiff_t)decoded_data_size" },
	{ "maya_zero_end_7400_binary", 12384, 255, 0, 0, 0, "Bad array encoding" },
	{ "maya_node_attribute_zoo_6100_binary", 12130, 255, 0, 0, 0, "arr_data" },
	{ "maya_zero_end_7400_binary", 12379, 255, 0, 0, 0, "ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_d..." },
	{ "maya_zero_end_7400_binary", -1, 0, 272, 0, 0, "vals" },
	{ "maya_zero_end_7400_binary", -1, 0, 0, 0, 369, "data" },
	{ "maya_zero_end_7400_binary", 331, 0, 0, 0, 0, "ufbxi_push_string_place_str(uc, &vals[i].s)" },
	{ "maya_zero_end_7400_binary", 593, 8, 0, 0, 0, "ufbxi_skip_bytes(uc, encoded_size)" },
	{ "maya_zero_end_7400_binary", 31, 255, 0, 0, 0, "Bad value type" },
	{ "maya_zero_end_7400_binary", 66, 0, 0, 0, 0, "offset <= values_end_offset" },
	{ "maya_zero_end_7400_binary", 36, 255, 0, 0, 0, "ufbxi_skip_bytes(uc, values_end_offset - offset)" },
	{ "maya_zero_end_7400_binary", 58, 255, 0, 0, 0, "current_offset == end_offset || end_offset == 0" },
	{ "maya_zero_end_7400_binary", 70, 0, 0, 0, 0, "ufbxi_binary_parse_node(uc, depth + 1, parse_state, &en..." },
	{ "maya_zero_end_7400_binary", -1, 0, 160, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 2, 0, 0, "ufbxi_grow_array(&uc->ator_tmp, &token->str_data, &toke..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 2, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 5, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 288, 43, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_leading_comma_7500_ascii", 3707, 43, 0, 0, 0, "end == token->str_data + token->str_len - 1" },
	{ "maya_slime_7500_ascii", -1, 0, 2800, 0, 0, "ufbxi_ascii_push_token_char(uc, token, c)" },
	{ "maya_leading_comma_7500_ascii", 292, 0, 0, 0, 0, "c != '\\0'" },
	{ "maya_leading_comma_7500_ascii", 288, 43, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 2537, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", 168, 0, 0, 0, 0, "depth == 0" },
	{ "maya_leading_comma_7500_ascii", 0, 255, 0, 0, 0, "ufbxi_ascii_accept(uc, UFBXI_ASCII_NAME)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 33, 0, "name" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 33, 0, 0, "node" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 416, 0, 0, "arr" },
	{ "maya_leading_comma_7500_ascii", 292, 0, 0, 0, 0, "ufbxi_ascii_next_token(uc, &ua->token)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 96, 0, "ufbxi_push_string_place_str(uc, &v->s)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 285, 0, "v" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 417, 0, 0, "v" },
	{ "maya_node_attribute_zoo_7500_ascii", -1, 0, 1649, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 160, 0, "v" },
	{ "maya_auto_clamp_7100_ascii", -1, 0, 594, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 272, 0, "v" },
	{ "maya_node_attribute_zoo_6100_ascii", -1, 0, 4865, 0, 0, "v" },
	{ "maya_leading_comma_7500_ascii", 8927, 255, 0, 0, 0, "ufbxi_ascii_accept(uc, UFBXI_ASCII_INT)" },
	{ "maya_leading_comma_7500_ascii", 8931, 255, 0, 0, 0, "ufbxi_ascii_accept(uc, UFBXI_ASCII_NAME)" },
	{ "maya_leading_comma_7500_ascii", 8937, 255, 0, 0, 0, "ufbxi_ascii_accept(uc, '}')" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 420, 0, 0, "arr_data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 32, 0, 0, "node->vals" },
	{ "maya_leading_comma_7500_ascii", 168, 255, 0, 0, 0, "ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 130, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 0, 1, "header" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 2, 0, 0, "ufbxi_ascii_next_token(uc, &uc->ascii.token)" },
	{ "maya_leading_comma_7500_ascii", 100, 255, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, true)" },
	{ "maya_zero_end_7400_binary", 35, 255, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, state, p_end, buf, true)" },
	{ "maya_leading_comma_7500_ascii", 0, 255, 0, 0, 0, "ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &..." },
	{ "maya_zero_end_7400_binary", 24, 255, 0, 0, 0, "ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, ..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 136, 0, 0, "ufbxi_grow_array(&uc->ator_tmp, &uc->top_nodes, &uc->to..." },
	{ "maya_leading_comma_7500_ascii", 1544, 255, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &en..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 119, 0, 0, "node->children" },
	{ "maya_leading_comma_7500_ascii", 100, 255, 0, 0, 0, "ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_pars..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 29, 0, 0, "ufbxi_map_grow(&uc->node_prop_set, const char*, ufbxi_a..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 1, 0, 0, "ufbxi_map_grow(&uc->prop_type_map, ufbxi_prop_type_name..." },
	{ "maya_leading_comma_7500_ascii", 100, 255, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 2615, 255, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &child)" },
	{ "maya_leading_comma_7500_ascii", 1584, 0, 0, 0, 0, "ufbxi_get_val2(node, \"SC\", &prop->name, (char**)&type..." },
	{ "maya_leading_comma_7500_ascii", 1601, 0, 0, 0, 0, "ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 257, 0, 0, "ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 65, 0, "props->props" },
	{ "maya_leading_comma_7500_ascii", 1584, 0, 0, 0, 0, "ufbxi_read_property(uc, &node->children[i], &props->pro..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 257, 0, 0, "ufbxi_sort_properties(uc, props->props, props->num_prop..." },
	{ "maya_leading_comma_7500_ascii", 3021, 255, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &object)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 256, 0, 0, "tmpl" },
	{ "maya_leading_comma_7500_ascii", 3061, 255, 0, 0, 0, "ufbxi_get_val1(object, \"C\", (char**)&tmpl->type)" },
	{ "maya_leading_comma_7500_ascii", 3159, 0, 0, 0, 0, "ufbxi_get_val1(props, \"S\", &tmpl->sub_type)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 64, 0, "ufbxi_push_string_place_str(uc, &tmpl->sub_type)" },
	{ "maya_leading_comma_7500_ascii", 3203, 0, 0, 0, 0, "ufbxi_read_properties(uc, props, &tmpl->props)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 119, 0, "uc->templates" },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 0, 286, 0, "ufbxi_push_string_place_str(uc, type)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 296, 0, "ufbxi_push_string_place_str(uc, name)" },
	{ "maya_leading_comma_7500_ascii", 8892, 0, 0, 0, 0, "ufbxi_check_string(*type)" },
	{ "maya_zero_end_7400_binary", 17803, 0, 0, 0, 0, "ufbxi_check_string(*name)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 131, 0, 0, "ufbxi_push_copy(&uc->tmp_typed_element_offsets[type], s..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 132, 0, 0, "ufbxi_push_copy(&uc->tmp_element_offsets, size_t, 1, &u..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 133, 0, 0, "elem" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 134, 0, 0, "ufbxi_map_grow(&uc->fbx_id_map, ufbxi_fbx_id_entry, 64)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4816, 0, 0, "ufbxi_push_copy(&uc->tmp_typed_element_offsets[type], s..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4821, 0, 0, "ufbxi_push_copy(&uc->tmp_element_offsets, size_t, 1, &u..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4817, 0, 0, "elem" },
	{ "blender_279_sausage_6100_ascii", -1, 0, 5712, 0, 0, "ufbxi_map_grow(&uc->fbx_id_map, ufbxi_fbx_id_entry, 64)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 574, 0, 0, "elem_node" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 1168, 0, 0, "ufbxi_push_copy(&uc->tmp_node_ids, uint32_t, 1, &elem_n..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 614, 0, 0, "elem" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 242, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4819, 0, 0, "conn" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 356, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4595, 0, 0, "unknown" },
	{ "fuzz_0397", -1, 0, 0, 115, 0, "new_indices" },
	{ "maya_leading_comma_7500_ascii", 9361, 77, 0, 0, 0, "data" },
	{ "maya_leading_comma_7500_ascii", 9370, 43, 0, 0, 0, "data->size % num_components == 0" },
	{ "maya_leading_comma_7500_ascii", 9278, 76, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_MappingInformationType, \"C..." },
	{ "fuzz_0397", -1, 0, 0, 115, 0, "ufbxi_check_indices(uc, p_dst_index, index_data, true, ..." },
	{ "synthetic_indexed_by_vertex_7500_ascii", -1, 0, 0, 182, 0, "new_index_data" },
	{ "maya_leading_comma_7500_ascii", 10556, 255, 0, 0, 0, "Invalid mapping" },
	{ "fuzz_0393", -1, 0, 0, 114, 0, "index_data" },
	{ "maya_leading_comma_7500_ascii", 9303, 255, 0, 0, 0, "Invalid mapping" },
	{ "maya_leading_comma_7500_ascii", 10999, 82, 0, 0, 0, "arr" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 358, 0, 0, "shape" },
	{ "maya_blend_shape_cube_6100_binary", 9533, 11, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_blend_shape_cube_6100_binary", 9493, 0, 0, 0, 0, "indices->size == vertices->size / 3" },
	{ "synthetic_blend_shape_order_7500_ascii", -1, 0, 514, 0, 0, "offsets" },
	{ "maya_blend_shape_cube_6100_binary", 9466, 0, 0, 0, 0, "ufbxi_get_val1(n, \"S\", &name)" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 340, 0, 0, "deformer" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 343, 0, 0, "ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 354, 0, 0, "channel" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 355, 0, 0, "ufbxi_push_copy(&uc->tmp_full_weights, ufbx_real_list, ..." },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 0, 48, 0, "shape_props" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 356, 0, 0, "ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name..." },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 357, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", 9493, 0, 0, 0, 0, "ufbxi_read_shape(uc, n, &shape_info)" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 352, 0, 0, "ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id)" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 353, 0, 0, "ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 551, 0, 0, "mesh" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 553, 0, 0, "tex_list" },
	{ "maya_blend_shape_cube_6100_binary", 9466, 0, 0, 0, 0, "ufbxi_read_synthetic_blend_shapes(uc, node, info)" },
	{ "maya_leading_comma_7500_ascii", 8926, 43, 0, 0, 0, "vertices->size % 3 == 0" },
	{ "maya_leading_comma_7500_ascii", 9073, 43, 0, 0, 0, "index_data[mesh->num_indices - 1] < 0" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 288, 0, "edges" },
	{ "maya_leading_comma_7500_ascii", 9157, 122, 0, 0, 0, "index_ix >= 0 && (size_t)index_ix < mesh->num_indices" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 289, 0, "mesh->faces" },
	{ "maya_leading_comma_7500_ascii", 9107, 56, 0, 0, 0, "(size_t)ix < mesh->num_vertices" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 290, 0, "mesh->vertex_first_index" },
	{ "maya_uv_sets_6100_binary", -1, 0, 501, 0, 0, "bitangents" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 554, 0, 0, "tangents" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 291, 0, "mesh->uv_sets.data" },
	{ "maya_color_sets_6100_binary", -1, 0, 0, 60, 0, "mesh->color_sets.data" },
	{ "maya_leading_comma_7500_ascii", 9278, 76, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_no..." },
	{ "maya_leading_comma_7500_ascii", 9692, 76, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.dat..." },
	{ "maya_leading_comma_7500_ascii", 10114, 76, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.dat..." },
	{ "maya_leading_comma_7500_ascii", 10531, 76, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_uv...." },
	{ "maya_color_sets_6100_binary", 9909, 255, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_col..." },
	{ "maya_cone_6100_binary", 16031, 255, 0, 0, 0, "ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_cr..." },
	{ "maya_cone_6100_binary", 15524, 255, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "maya_cone_6100_binary", 15571, 255, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_crease, n, u..." },
	{ "maya_leading_comma_7500_ascii", 10925, 76, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "maya_leading_comma_7500_ascii", 10999, 82, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->edge_smoothing, n..." },
	{ "blender_279_ball_6100_ascii", 18422, 82, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_smoothing, n..." },
	{ "maya_leading_comma_7500_ascii", 11116, 76, 0, 0, 0, "ufbxi_find_val1(n, ufbxi_MappingInformationType, \"C\",..." },
	{ "blender_279_ball_6100_ascii", 18755, 76, 0, 0, 0, "ufbxi_read_truncated_array(uc, &mesh->face_material, n,..." },
	{ "maya_leading_comma_7500_ascii", 11198, 76, 0, 0, 0, "arr && arr->size >= 1" },
	{ "maya_zero_end_7400_binary", 12861, 0, 0, 0, 0, "!memchr(n->name, '\\0', n->name_len)" },
	{ "maya_textured_cube_6100_binary", -1, 0, 0, 64, 0, "ufbxi_push_string_place_str(uc, &prop_name)" },
	{ "blender_279_uv_sets_6100_ascii", -1, 0, 551, 0, 0, "tex" },
	{ "blender_279_uv_sets_6100_ascii", -1, 0, 552, 0, 0, "tex_list->data" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4039, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 138208, 255, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Order, \"I\", &nurbs->topol..." },
	{ "maya_node_attribute_zoo_6100_binary", 138308, 255, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Dimension, \"I\", &nurbs->t..." },
	{ "maya_node_attribute_zoo_6100_binary", 138331, 255, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_Form, \"C\", (char**)&form)" },
	{ "maya_node_attribute_zoo_6100_binary", 138358, 255, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 138414, 255, 0, 0, 0, "knot" },
	{ "maya_node_attribute_zoo_6100_binary", 138417, 31, 0, 0, 0, "points->size % 4 == 0" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4150, 0, 0, "nurbs" },
	{ "maya_node_attribute_zoo_6100_binary", 139477, 255, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, \"II\", ..." },
	{ "maya_node_attribute_zoo_6100_binary", 139590, 255, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Dimensions, \"II\", &nurbs-..." },
	{ "maya_node_attribute_zoo_6100_binary", 139630, 255, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_Form, \"CC\", (char**)&form..." },
	{ "maya_node_attribute_zoo_6100_binary", 139690, 255, 0, 0, 0, "points" },
	{ "maya_node_attribute_zoo_6100_binary", 139725, 255, 0, 0, 0, "knot_u" },
	{ "maya_node_attribute_zoo_6100_binary", 140320, 255, 0, 0, 0, "knot_v" },
	{ "maya_node_attribute_zoo_6100_binary", 139728, 63, 0, 0, 0, "points->size % 4 == 0" },
	{ "blender_279_sausage_6100_ascii", -1, 0, 3808, 0, 0, "skin" },
	{ "blender_279_sausage_6100_ascii", -1, 0, 3938, 0, 0, "cluster" },
	{ "maya_game_sausage_6100_binary", 45318, 0, 0, 0, 0, "indices->size == weights->size" },
	{ "maya_game_sausage_6100_binary", 45470, 0, 0, 0, 0, "transform->size >= 16" },
	{ "maya_game_sausage_6100_binary", 45636, 0, 0, 0, 0, "transform_link->size >= 16" },
	{ "maya_blend_shape_cube_7700_binary", -1, 0, 634, 0, 0, "channel" },
	{ "maya_blend_shape_cube_7700_binary", -1, 0, 636, 0, 0, "ufbxi_push_copy(&uc->tmp_full_weights, ufbx_real_list, ..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 1607, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_7500_binary", 61038, 255, 0, 0, 0, "times = ufbxi_find_array(node, ufbxi_KeyTime, 'l')" },
	{ "maya_node_attribute_zoo_7500_binary", 61115, 255, 0, 0, 0, "values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r..." },
	{ "maya_node_attribute_zoo_7500_binary", 61175, 255, 0, 0, 0, "attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags,..." },
	{ "maya_node_attribute_zoo_7500_binary", 61234, 255, 0, 0, 0, "attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, ..." },
	{ "maya_node_attribute_zoo_7500_binary", 61292, 255, 0, 0, 0, "refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i..." },
	{ "maya_node_attribute_zoo_7500_ascii", 70359, 43, 0, 0, 0, "times->size == values->size" },
	{ "maya_node_attribute_zoo_7500_ascii", 70603, 43, 0, 0, 0, "attr_flags->size == refs->size" },
	{ "maya_node_attribute_zoo_7500_ascii", 70729, 43, 0, 0, 0, "attrs->size == refs->size * 4u" },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 0, 287, 0, "keys" },
	{ "maya_node_attribute_zoo_7500_binary", 61431, 0, 0, 0, 0, "refs_left >= 0" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 601, 0, 0, "material" },
	{ "maya_slime_7500_binary", -1, 0, 1511, 0, 0, "texture" },
	{ "maya_slime_7500_binary", -1, 0, 1481, 0, 0, "video" },
	{ "maya_slime_7500_ascii", -1, 0, 0, 16520, 0, "video->content" },
	{ "synthetic_missing_version_6100_ascii", -1, 0, 3751, 0, 0, "pose" },
	{ "synthetic_missing_version_6100_ascii", -1, 0, 3754, 0, 0, "tmp_pose" },
	{ "synthetic_missing_version_6100_ascii", -1, 0, 3755, 0, 0, "pose->bone_poses.data" },
	{ "maya_arnold_textures_6100_binary", -1, 0, 1434, 0, 0, "bindings" },
	{ "maya_arnold_textures_6100_binary", -1, 0, 1456, 0, 0, "bind" },
	{ "maya_arnold_textures_6100_binary", -1, 0, 0, 286, 0, "bindings->prop_bindings.data" },
	{ "maya_node_attribute_zoo_6100_binary", 6671, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 236, 0, 0, "ufbxi_map_grow(&uc->fbx_attr_map, ufbxi_fbx_attr_entry,..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 237, 0, 0, "ufbxi_push_copy(&uc->tmp_stack, ufbx_prop, 1, &ps[src])" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 0, 48, 0, "attrib_info.props.props" },
	{ "maya_node_attribute_zoo_6100_binary", 12128, 0, 0, 0, 0, "ufbxi_read_mesh(uc, node, &attrib_info)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 1390, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 1162, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 666, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 240, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 2528, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 1904, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "synthetic_missing_version_6100_ascii", -1, 0, 225, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 2712, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "blender_279_sausage_6100_ascii", -1, 0, 398, 0, 0, "ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 242, 0, 0, "ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id)" },
	{ "maya_leading_comma_7500_ascii", 1584, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &uc->scene.settings.pro..." },
	{ "maya_leading_comma_7500_ascii", 8861, 255, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_node_attribute_zoo_6100_binary", 157509, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, node)" },
	{ "maya_leading_comma_7500_ascii", 8892, 0, 0, 0, 0, "ufbxi_split_type_and_name(uc, type_and_name, &type_str,..." },
	{ "maya_leading_comma_7500_ascii", 11807, 0, 0, 0, 0, "ufbxi_read_properties(uc, node, &info.props)" },
	{ "maya_node_attribute_zoo_6100_binary", 6671, 0, 0, 0, 0, "ufbxi_read_synthetic_attribute(uc, node, &info, sub_typ..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 574, 0, 0, "ufbxi_read_model(uc, node, &info)" },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 604, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_light),..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 3783, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera)..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 539, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_bone), ..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 450, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty),..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 653, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 1056, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_..." },
	{ "maya_leading_comma_7500_ascii", 8926, 43, 0, 0, 0, "ufbxi_read_mesh(uc, node, &info)" },
	{ "maya_blend_shape_cube_7700_binary", -1, 0, 529, 0, 0, "ufbxi_read_shape(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 138208, 255, 0, 0, 0, "ufbxi_read_nurbs_curve(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", 139477, 255, 0, 0, 0, "ufbxi_read_nurbs_surface(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4237, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4280, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_t..." },
	{ "maya_slime_7500_binary", -1, 0, 803, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "blender_279_sausage_6100_ascii", -1, 0, 3808, 0, 0, "ufbxi_read_skin(uc, node, &info)" },
	{ "blender_279_sausage_6100_ascii", -1, 0, 3938, 0, 0, "ufbxi_read_skin_cluster(uc, node, &info)" },
	{ "maya_blend_shape_cube_7700_binary", -1, 0, 605, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_d..." },
	{ "maya_blend_shape_cube_7700_binary", -1, 0, 634, 0, 0, "ufbxi_read_blend_channel(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 601, 0, 0, "ufbxi_read_material(uc, node, &info)" },
	{ "maya_slime_7500_binary", -1, 0, 1511, 0, 0, "ufbxi_read_texture(uc, node, &info)" },
	{ "maya_slime_7500_binary", -1, 0, 1481, 0, 0, "ufbxi_read_video(uc, node, &info)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 614, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_st..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 618, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_la..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 1664, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_va..." },
	{ "maya_node_attribute_zoo_7500_binary", 61038, 255, 0, 0, 0, "ufbxi_read_animation_curve(uc, node, &info)" },
	{ "synthetic_missing_version_6100_ascii", -1, 0, 3751, 0, 0, "ufbxi_read_pose(uc, node, &info, sub_type)" },
	{ "maya_arnold_textures_6100_binary", -1, 0, 1273, 0, 0, "ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader)..." },
	{ "maya_arnold_textures_6100_binary", -1, 0, 1456, 0, 0, "ufbxi_read_binding_table(uc, node, &info)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4595, 0, 0, "ufbxi_read_unknown(uc, node, &info, type_str, sub_type_..." },
	{ "maya_leading_comma_7500_ascii", 13120, 255, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 625, 0, 0, "conn" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4820, 0, 0, "curve" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4823, 0, 0, "ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve-..." },
	{ "maya_node_attribute_zoo_6100_binary", 163331, 0, 0, 0, 0, "ufbxi_find_val1(node, ufbxi_KeyCount, \"Z\", &num_keys)" },
	{ "maya_node_attribute_zoo_6100_binary", 163351, 255, 0, 0, 0, "curve->keyframes.data" },
	{ "maya_node_attribute_zoo_6100_binary", 163357, 0, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_game_sausage_6100_binary_deform", 44932, 98, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_node_attribute_zoo_6100_binary", 163390, 255, 0, 0, 0, "Unknown slope mode" },
	{ "maya_node_attribute_zoo_6100_binary", 163357, 13, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_node_attribute_zoo_6100_binary", 163410, 255, 0, 0, 0, "Unknown weight mode" },
	{ "maya_transform_animation_6100_binary", 17549, 11, 0, 0, 0, "data_end - data >= 1" },
	{ "maya_node_attribute_zoo_6100_binary", 163388, 255, 0, 0, 0, "Unknown key mode" },
	{ "maya_node_attribute_zoo_6100_binary", 163349, 255, 0, 0, 0, "data_end - data >= 2" },
	{ "maya_node_attribute_zoo_6100_binary", 163349, 0, 0, 0, 0, "data == data_end" },
	{ "synthetic_missing_version_6100_ascii", 72756, 0, 0, 0, 0, "ufbxi_get_val1(child, \"C\", (char**)&old_name)" },
	{ "synthetic_missing_version_6100_ascii", 72839, 74, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4816, 0, 0, "ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4819, 0, 0, "ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name)" },
	{ "maya_node_attribute_zoo_6100_binary", 163331, 0, 0, 0, 0, "ufbxi_read_take_anim_channel(uc, channel_nodes[i], valu..." },
	{ "maya_node_attribute_zoo_6100_binary", 163171, 0, 0, 0, 0, "ufbxi_get_val1(node, \"c\", (char**)&type_and_name)" },
	{ "maya_node_attribute_zoo_6100_binary", 163331, 0, 0, 0, 0, "ufbxi_read_take_prop_channel(uc, child, target_fbx_id, ..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4810, 0, 0, "stack" },
	{ "maya_node_attribute_zoo_6100_binary", 163019, 0, 0, 0, 0, "ufbxi_get_val1(node, \"S\", &stack->name)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4812, 0, 0, "layer" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4815, 0, 0, "ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 163046, 255, 0, 0, 0, "ufbxi_find_val2(node, ufbxi_ReferenceTime, \"LL\", &beg..." },
	{ "maya_node_attribute_zoo_6100_binary", 163171, 0, 0, 0, 0, "ufbxi_read_take_object(uc, child, layer_fbx_id)" },
	{ "maya_node_attribute_zoo_6100_binary", 162972, 255, 0, 0, 0, "ufbxi_parse_toplevel_child(uc, &node)" },
	{ "maya_node_attribute_zoo_6100_binary", 163019, 0, 0, 0, 0, "ufbxi_read_take(uc, node)" },
	{ "maya_leading_comma_7500_ascii", 0, 255, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension)" },
	{ "maya_leading_comma_7500_ascii", 100, 255, 0, 0, 0, "ufbxi_read_header_extension(uc)" },
	{ "blender_279_default_6100_ascii", 454, 255, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Creator)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 29, 0, 0, "ufbxi_init_node_prop_names(uc)" },
	{ "maya_leading_comma_7500_ascii", 1525, 255, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Documents)" },
	{ "maya_leading_comma_7500_ascii", 2615, 255, 0, 0, 0, "ufbxi_read_document(uc)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 131, 0, 0, "root" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 135, 0, 0, "ufbxi_push_copy(&uc->tmp_node_ids, uint32_t, 1, &root->..." },
	{ "maya_leading_comma_7500_ascii", 2808, 255, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Definitions)" },
	{ "maya_leading_comma_7500_ascii", 3021, 255, 0, 0, 0, "ufbxi_read_definitions(uc)" },
	{ "maya_leading_comma_7500_ascii", 8762, 255, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Objects)" },
	{ "maya_leading_comma_7500_ascii", 8861, 255, 0, 0, 0, "ufbxi_read_objects(uc)" },
	{ "maya_leading_comma_7500_ascii", 13016, 255, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Connections)" },
	{ "maya_leading_comma_7500_ascii", 13120, 255, 0, 0, 0, "ufbxi_read_connections(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 158678, 255, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_Takes)" },
	{ "maya_node_attribute_zoo_6100_binary", 162972, 255, 0, 0, 0, "ufbxi_read_takes(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", 162983, 255, 0, 0, 0, "ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings)" },
	{ "maya_leading_comma_7500_ascii", 1584, 0, 0, 0, 0, "ufbxi_read_global_settings(uc, uc->top_node)" },
	{ "fuzz_0112", -1, 0, 116, 0, 0, "ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_..." },
	{ "fuzz_0112", -1, 0, 115, 0, 0, "ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_..." },
	{ "blender_279_sausage_6100_ascii", -1, 0, 7643, 0, 0, "ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 629, 0, 0, "tmp_connections" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 310, 0, "uc->scene.connections_src.data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 311, 0, "uc->scene.connections_dst.data" },
	{ "blender_279_sausage_6100_ascii", -1, 0, 7643, 0, 0, "ufbxi_sort_connections(uc, uc->scene.connections_src.da..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 1857, 0, 0, "ufbxi_push_copy(&uc->tmp_stack, ufbx_prop, (size_t)(pro..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 1858, 0, 0, "new_prop" },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 1859, 0, 0, "ufbxi_push_copy(&uc->tmp_stack, ufbx_prop, (size_t)(pro..." },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 0, 315, 0, "elem->props.props" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4864, 0, 0, "node_ids" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4865, 0, 0, "node_ptrs" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4866, 0, 0, "node_offsets" },
	{ "maya_game_sausage_6100_binary", 48802, 49, 0, 0, 0, "depth <= num_nodes" },
	{ "fuzz_0112", -1, 0, 115, 0, 0, "ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4867, 0, 0, "p_offset" },
	{ "maya_game_sausage_6100_binary_wiggle", -1, 0, 2039, 0, 0, "ufbxi_push_copy(&uc->tmp_stack, ufbx_element*, 1, &conn..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 320, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 316, 0, "list->data" },
	{ "maya_textured_cube_7500_binary", -1, 0, 1010, 0, 0, "tex" },
	{ "maya_slime_7500_binary", -1, 0, 0, 469, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 319, 0, "list->data" },
	{ "maya_blend_inbetween_7500_binary", -1, 0, 784, 0, 0, "ufbxi_push_copy(&uc->tmp_stack, ufbx_blend_keyframe, 1,..." },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 0, 140, 0, "list->data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 308, 0, "uc->scene.elements.data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 309, 0, "element_data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 628, 0, 0, "element_offsets" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 629, 0, 0, "ufbxi_resolve_connections(uc)" },
	{ "maya_node_attribute_zoo_7500_binary", -1, 0, 1857, 0, 0, "ufbxi_add_connections_to_elements(uc)" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 4864, 0, 0, "ufbxi_linearize_nodes(uc)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 630, 0, 0, "typed_offsets" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 312, 0, "typed_elems->data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 315, 0, "uc->scene.elements_by_name.data" },
	{ "fuzz_0112", -1, 0, 116, 0, 0, "ufbxi_sort_name_elements(uc, uc->scene.elements_by_name..." },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 0, 416, 0, "node->all_attribs.data" },
	{ "synthetic_missing_version_6100_ascii", -1, 0, 0, 274, 0, "pose->bone_poses.data" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 316, 0, "ufbxi_fetch_src_elements(uc, &elem->instances, elem, fa..." },
	{ "blender_279_sausage_6100_ascii", -1, 0, 0, 3302, 0, "ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->el..." },
	{ "blender_279_sausage_6100_ascii", -1, 0, 0, 3303, 0, "skin->vertices.data" },
	{ "blender_279_sausage_6100_ascii", -1, 0, 0, 3304, 0, "skin->weights.data" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 0, 139, 0, "ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->..." },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 621, 0, 0, "full_weights" },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 0, 140, 0, "ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &c..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 317, 0, "zero_indices && consecutive_indices" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 319, 0, "ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh-..." },
	{ "blender_279_ball_6100_ascii", -1, 0, 0, 709, 0, "mat->faces" },
	{ "blender_279_sausage_6100_ascii", -1, 0, 0, 3308, 0, "ufbxi_fetch_dst_elements(uc, &mesh->skins, &mesh->eleme..." },
	{ "maya_blend_shape_cube_6100_binary", -1, 0, 0, 145, 0, "ufbxi_fetch_dst_elements(uc, &mesh->blend_shapes, &mesh..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 320, 0, "ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->el..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 321, 0, "stack->anim.layers.data" },
	{ "maya_node_attribute_zoo_6100_binary", -1, 0, 0, 432, 0, "ufbxi_fetch_dst_elements(uc, &layer->anim_values, &laye..." },
	{ "synthetic_missing_version_6100_ascii", -1, 0, 4454, 0, 0, "aprop" },
	{ "synthetic_missing_version_6100_ascii", -1, 0, 4461, 0, 0, "aprop" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 322, 0, "layer->anim_props.data" },
	{ "maya_slime_7500_binary", -1, 0, 0, 469, 0, "ufbxi_fetch_textures(uc, &material->textures, &material..." },
	{ "maya_leading_comma_7500_ascii", -1, 0, 631, 0, 0, "mesh_textures" },
	{ "maya_textured_cube_6100_binary", -1, 0, 0, 229, 0, "ufbxi_fetch_dst_elements(uc, &textures, &mesh->element,..." },
	{ "maya_textured_cube_6100_binary", -1, 0, 1536, 0, 0, "mat_texs" },
	{ "maya_shared_textures_6100_binary", -1, 0, 2243, 0, 0, "mat_tex" },
	{ "maya_textured_cube_6100_binary", -1, 0, 1539, 0, 0, "mat_tex" },
	{ "blender_279_uv_sets_6100_ascii", -1, 0, 3560, 0, 0, "mat_texs" },
	{ "maya_textured_cube_6100_binary", -1, 0, 0, 230, 0, "texs" },
	{ "maya_textured_cube_6100_binary", -1, 0, 1541, 0, 0, "tex" },
	{ "maya_slime_7500_binary", -1, 0, 3677, 0, 0, "content_videos" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 1, 0, 0, "ufbxi_load_maps(uc)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 2, 0, 0, "ufbxi_begin_parse(uc)" },
	{ "maya_leading_comma_7500_ascii", 0, 255, 0, 0, 0, "ufbxi_read_root(uc)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 628, 0, 0, "ufbxi_finalize_scene(uc)" },
	{ "maya_leading_comma_7500_ascii", -1, 0, 0, 323, 0, "imp" },
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

void ufbxt_do_fuzz(ufbx_scene *scene, ufbx_scene *streamed_scene, const char *base_name, void *data, size_t size)
{
	if (g_fuzz) {
		size_t step = 0;
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

			if (!ufbxt_test_fuzz(data, size, step, -1, (size_t)i, 0, 0)) fail_step = step;
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

			if (!ufbxt_test_fuzz(data, size, step, -1, 0, (size_t)i, 0)) fail_step = step;
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

				if (!ufbxt_test_fuzz(data, size, step, -1, 0, 0, (size_t)i)) fail_step = step;
			}

			fprintf(stderr, "\rFuzzing truncate %s: %d/%d\n", base_name, (int)size, (int)size);
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
						if (!ufbxt_test_fuzz(data_u8, size, step + v, i, 0, 0, 0)) fail_step = step + v;
					}
				} else {
					data_u8[i] = original + 1;
					if (!ufbxt_test_fuzz(data_u8, size, step + 1, i, 0, 0, 0)) fail_step = step + 1;

					data_u8[i] = original - 1;
					if (!ufbxt_test_fuzz(data_u8, size, step + 2, i, 0, 0, 0)) fail_step = step + 2;

					if (original != 0) {
						data_u8[i] = 0;
						if (!ufbxt_test_fuzz(data_u8, size, step + 3, i, 0, 0, 0)) fail_step = step + 3;
					}

					if (original != 0xff) {
						data_u8[i] = 0xff;
						if (!ufbxt_test_fuzz(data_u8, size, step + 4, i, 0, 0, 0)) fail_step = step + 4;
					}
				}


				data_u8[i] = original;
			}

			fprintf(stderr, "\rFuzzing patch %s: %d/%d\n", base_name, (int)size, (int)size);

			for (size_t i = 0; i < ufbxt_arraycount(data_copy); i++) {
				free(data_copy[i]);
			}

		}

		ufbxt_hintf("Fuzz failed on step: %zu", step);
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

const uint32_t ufbxt_file_versions[] = { 5000, 5800, 6100, 7100, 7400, 7500, 7700 };

void ufbxt_do_file_test(const char *name, void (*test_fn)(ufbx_scene *s, ufbxt_diff_error *err), const char *suffix, ufbx_load_opts user_opts)
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

			uint64_t load_begin = cputime_cpu_tick();
			ufbx_scene *scene = ufbx_load_memory(data, size, &load_opts, &error);
			uint64_t load_end = cputime_cpu_tick();

			if (!scene) {
				ufbxt_log_error(&error);
				ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse file");
			} else {
				ufbxt_check_scene(scene);
			}

			ufbx_load_opts stream_opts = load_opts;
			ufbxt_init_allocator(&stream_opts.temp_allocator);
			ufbxt_init_allocator(&stream_opts.result_allocator);
			stream_opts.read_buffer_size = 1;
			stream_opts.temp_allocator.huge_threshold = 1;
			stream_opts.result_allocator.huge_threshold = 1;
			ufbx_scene *streamed_scene = ufbx_load_file(buf, &stream_opts, &error);
			if (streamed_scene) {
				ufbxt_check_scene(streamed_scene);
			} else {
				ufbxt_log_error(&error);
				ufbxt_assert_fail(__FILE__, __LINE__, "Failed to parse streamed file");
			}

			// Try a couple of read buffer sizes
			if (g_fuzz && !g_fuzz_no_buffer) {
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
					t_jmp_buf = (jmp_buf*)calloc(1, sizeof(jmp_buf));
					if (!setjmp(*t_jmp_buf)) {
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

				if (fail_sz >= 0) {
					size_t error_size = 256;
					char *error = (char*)malloc(error_size);
					ufbxt_assert(error);
					snprintf(error, error_size, "Failed to parse with: read_buffer_size = %d", fail_sz);
					printf("%s: %s\n", base_name, error);
					ufbxt_assert_fail(__FILE__, __LINE__, error);
				} else {
					fprintf(stderr, "\rFuzzing read buffer size %s: %d/%d\n", base_name, (int)size, (int)size);
				}

			} else {
			}

			// Ignore geometry, animations, and both

			{
				ufbx_load_opts opts = load_opts;
				opts.ignore_geometry = true;
				ufbx_scene *ignore_scene = ufbx_load_memory(data, size, &opts, NULL);
				ufbxt_check_scene(scene);
				ufbx_free_scene(ignore_scene);
			}

			{
				ufbx_load_opts opts = load_opts;
				opts.ignore_animation = true;
				ufbx_scene *ignore_scene = ufbx_load_memory(data, size, &opts, NULL);
				ufbxt_check_scene(ignore_scene);
				ufbx_free_scene(ignore_scene);
			}

			{
				ufbx_load_opts opts = load_opts;
				opts.ignore_geometry = true;
				opts.ignore_animation = true;
				ufbx_scene *ignore_scene = ufbx_load_memory(data, size, &opts, NULL);
				ufbxt_check_scene(ignore_scene);
				ufbx_free_scene(ignore_scene);
			}

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

			// Evaluate all the default animation and all stacks

			{
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

			for (size_t i = 1; i < scene->anim_stacks.count; i++) {
				ufbx_scene *state = ufbx_evaluate_scene(scene, scene->anim_stacks.data[i]->anim, 1.0, NULL, NULL);
				ufbxt_assert(state);
				ufbxt_check_scene(state);
				ufbx_free_scene(state);
			}

			ufbxt_diff_error err = { 0 };

			if (obj_file) {
				ufbxt_diff_to_obj(scene, obj_file, &err, false);
			}

			test_fn(scene, &err);

			if (err.num > 0) {
				ufbx_real avg = err.sum / (ufbx_real)err.num;
				ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", avg, err.max, err.num);
			}

			ufbxt_do_fuzz(scene, streamed_scene, base_name, data, size);

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
#define UFBXT_FILE_TEST(name) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, user_opts); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err)
#define UFBXT_FILE_TEST_OPTS(name, get_opts) void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err); \
	void ufbxt_test_fn_file_##name(void) { \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name, NULL, get_opts); } \
	void ufbxt_test_fn_imp_file_##name(ufbx_scene *scene, ufbxt_diff_error *err)
#define UFBXT_FILE_TEST_SUFFIX(name, suffix) void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err); \
	void ufbxt_test_fn_file_##name##_##suffix(void) { \
	ufbx_load_opts user_opts = { 0 }; \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name##_##suffix, #suffix, user_opts); } \
	void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err)
#define UFBXT_FILE_TEST_SUFFIX_OPTS(name, suffix, get_opts) void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err); \
	void ufbxt_test_fn_file_##name##_##suffix(void) { \
	ufbxt_do_file_test(#name, &ufbxt_test_fn_imp_file_##name##_##suffix, #suffix, get_opts); } \
	void ufbxt_test_fn_imp_file_##name##_##suffix(ufbx_scene *scene, ufbxt_diff_error *err)

#include "all_tests.h"

#undef UFBXT_IMPL
#undef UFBXT_TEST
#undef UFBXT_FILE_TEST
#undef UFBXT_FILE_TEST_OPTS
#undef UFBXT_FILE_TEST_SUFFIX
#undef UFBXT_FILE_TEST_SUFFIX_OPTS
#define UFBXT_IMPL 0
#define UFBXT_TEST(name) { #name, &ufbxt_test_fn_##name },
#define UFBXT_FILE_TEST(name) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_OPTS(name, get_opts) { #name, &ufbxt_test_fn_file_##name },
#define UFBXT_FILE_TEST_SUFFIX(name, suffix) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
#define UFBXT_FILE_TEST_SUFFIX_OPTS(name, suffix, get_opts) { #name "_" #suffix, &ufbxt_test_fn_file_##name##_##suffix },
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
	if (!setjmp(g_test_jmp)) {
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
			if (check->patch_offset == 0) continue;

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

			printf("\t{ \"%s\", %d, %u, %u, %u, %u, \"%s\" },\n", check->test_name,
				patch_offset, (uint32_t)check->patch_value, (uint32_t)check->temp_limit, (uint32_t)check->result_limit, (uint32_t)check->truncate_length, safe_desc);

			free(check->test_name);
		}
		printf("};\n");
	}

	return num_ok == num_ran ? 0 : 1;
}

