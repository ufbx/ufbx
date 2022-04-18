#ifndef UFBXT_TESTING_UTILS_INCLUDED
#define UFBXT_TESTING_UTILS_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#define ufbxt_arraycount(arr) (sizeof(arr) / sizeof(*(arr)))

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

static ufbx_vec3 ufbxt_cross3(ufbx_vec3 a, ufbx_vec3 b)
{
	ufbx_vec3 v = { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
	return v;
}

static ufbx_vec3 ufbxt_normalize(ufbx_vec3 a) {
	ufbx_real len = (ufbx_real)sqrt(ufbxt_dot3(a, a));
	if (len != 0.0) {
		return ufbxt_mul3(a, (ufbx_real)1.0 / len);
	} else {
		ufbx_vec3 zero = { (ufbx_real)0 };
		return zero;
	}
}

// -- obj load and diff

typedef struct {
	const char **groups;
	size_t num_groups;

	size_t num_faces;
	size_t num_indices;

	ufbx_face *faces;

	ufbx_vertex_vec3 vertex_position;
	ufbx_vertex_vec3 vertex_normal;
	ufbx_vertex_vec2 vertex_uv;

} ufbxt_obj_mesh;

typedef enum {
	UFBXT_OBJ_EXPORTER_UNKNOWN,
	UFBXT_OBJ_EXPORTER_BLENDER,
} ufbxt_obj_exporter;

typedef struct {

	ufbxt_obj_mesh *meshes;
	size_t num_meshes;

	bool bad_normals;
	bool bad_order;
	bool bad_uvs;
	ufbx_real tolerance;
	int32_t animation_frame;

	bool normalize_units;

	ufbxt_obj_exporter exporter;

} ufbxt_obj_file;

typedef struct {

	const char *name_end_chars;

} ufbxt_load_obj_opts;

static int ufbxt_cmp_obj_mesh(const void *va, const void *vb)
{
	const ufbxt_obj_mesh *a = (const ufbxt_obj_mesh*)va, *b = (const ufbxt_obj_mesh*)vb;
	if (a->num_groups < b->num_groups) return -1;
	if (a->num_groups > b->num_groups) return +1;
	return 0;
}

static ufbxt_obj_file *ufbxt_load_obj(void *obj_data, size_t obj_size, const ufbxt_load_obj_opts *opts)
{
	ufbxt_load_obj_opts zero_opts;
	if (!opts) {
		memset(&zero_opts, 0, sizeof(zero_opts));
		opts = &zero_opts;
	}

	size_t num_positions = 0;
	size_t num_normals = 0;
	size_t num_uvs = 0;
	size_t num_faces = 0;
	size_t num_meshes = 0;
	size_t num_indices = 0;
	size_t num_groups = 0;
	size_t total_name_length = 0;

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
		else if (!strncmp(line, "f ", 2)) {
			num_faces++;
			bool prev_space = false;
			for (char *c = line; *c; c++) {
				bool space = *c == ' ' || *c == '\t';
				if (space && !prev_space) num_indices++;
				prev_space = space;
			}
		}
		else if (!strncmp(line, "g default", 7)) { /* ignore default group */ }
		else if (!strncmp(line, "g ", 2)) {
			bool prev_space = false;
			num_groups++;
			for (char *c = line; *c; c++) {
				bool space = *c == ' ' || *c == '\t';
				if (space && !prev_space) num_groups++;
				if (!space) total_name_length++;
				prev_space = space;
			}
			num_meshes++;
		}

		if (end) {
			*end = prev;
			line = end + 1;
		} else {
			break;
		}
	}

	total_name_length += num_groups;

	size_t alloc_size = 0;
	alloc_size += sizeof(ufbxt_obj_file);
	alloc_size += num_meshes * sizeof(ufbxt_obj_mesh);
	alloc_size += num_groups * sizeof(const char*);
	alloc_size += num_positions * sizeof(ufbx_vec3);
	alloc_size += num_normals * sizeof(ufbx_vec3);
	alloc_size += num_uvs * sizeof(ufbx_vec2);
	alloc_size += num_faces * sizeof(ufbx_face);
	alloc_size += num_indices * 3 * sizeof(int32_t);
	alloc_size += total_name_length * sizeof(char);

	void *data = malloc(alloc_size);
	ufbxt_assert(data);

	ufbxt_obj_file *obj = (ufbxt_obj_file*)data;
	const char **group_ptrs = (const char**)(obj + 1);
	ufbxt_obj_mesh *meshes = (ufbxt_obj_mesh*)(group_ptrs + num_groups);
	ufbx_vec3 *positions = (ufbx_vec3*)(meshes + num_meshes);
	ufbx_vec3 *normals = (ufbx_vec3*)(positions + num_positions);
	ufbx_vec2 *uvs = (ufbx_vec2*)(normals + num_normals);
	ufbx_face *faces = (ufbx_face*)(uvs + num_uvs);
	int32_t *position_indices = (int32_t*)(faces + num_faces);
	int32_t *normal_indices = (int32_t*)(position_indices + num_indices);
	int32_t *uv_indices = (int32_t*)(normal_indices + num_indices);
	char *name_data = (char*)(uv_indices + num_indices);
	void *data_end = name_data + total_name_length;
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
	obj->normalize_units = false;
	obj->animation_frame = -1;
	obj->exporter = UFBXT_OBJ_EXPORTER_UNKNOWN;

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

				mesh->vertex_position.indices.count++;
				mesh->vertex_normal.indices.count++;
				mesh->vertex_uv.indices.count++;

				mesh->vertex_position.values.count = (size_t)(dp - positions);
				mesh->vertex_normal.values.count = (size_t)(dn - normals);
				mesh->vertex_uv.values.count = (size_t)(du - uvs);

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

			mesh->groups = group_ptrs;

			const char *c = line + 2;
			for (;;) {
				while (*c == ' ' || *c == '\t' || *c == '\r') {
					c++;
				}

				if (*c == '\n' || *c == '\0') break;

				const char *group_begin = c;
				char *group_copy = name_data;
				char *dst = group_copy;

				while (*c != ' ' && *c != '\t' && *c != '\r' && *c != '\n' && *c != '\0') {
					if (!strncmp(c, "FBXASC", 6)) {
						c += 6;
						char num[4] = { 0 };
						if (*c != '\0') num[0] = *c++;
						if (*c != '\0') num[1] = *c++;
						if (*c != '\0') num[2] = *c++;
						*dst++ = (char)atoi(num);
					} else {
						*dst++ = *c++;
					}
				}
				*dst++ = '\0';
				name_data = dst;

				*group_ptrs++ = group_copy;
			}

			mesh->num_groups = group_ptrs - mesh->groups;

			mesh->faces = df;
			mesh->vertex_position.values.data = positions;
			mesh->vertex_normal.values.data = normals;
			mesh->vertex_uv.values.data = uvs;
			mesh->vertex_position.indices.data = dpi;
			mesh->vertex_normal.indices.data = dni;
			mesh->vertex_uv.indices.data = dui;
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
			if (!strcmp(line, "www.blender.org")) {
				obj->exporter = UFBXT_OBJ_EXPORTER_BLENDER;
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
			int frame = 0;
			if (sscanf(line, "ufbx:frame=%d", &frame) == 1) {
				obj->animation_frame = (int32_t)frame;
			}
		}

		if (line_end) {
			*line_end = prev;
			line = line_end + 1;
		} else {
			break;
		}
	}

	qsort(obj->meshes, obj->num_meshes, sizeof(ufbxt_obj_mesh), ufbxt_cmp_obj_mesh);

	return obj;
}

static void ufbxt_debug_dump_obj_mesh(const char *file, ufbx_node *node, ufbx_mesh *mesh)
{
	FILE *f = fopen(file, "wb");
	ufbxt_assert(f);

	fprintf(f, "s 1\n");

	for (size_t i = 0; i < mesh->vertex_position.values.count; i++) {
		ufbx_vec3 v = mesh->vertex_position.values.data[i];
		v = ufbx_transform_position(&node->geometry_to_world, v);
		fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
	}
	for (size_t i = 0; i < mesh->vertex_uv.values.count; i++) {
		ufbx_vec2 v = mesh->vertex_uv.values.data[i];
		fprintf(f, "vt %f %f\n", v.x, v.y);
	}

	ufbx_matrix mat = ufbx_matrix_for_normals(&node->geometry_to_world);
	for (size_t i = 0; i < mesh->vertex_normal.values.count; i++) {
		ufbx_vec3 v = mesh->vertex_normal.values.data[i];
		v = ufbx_transform_direction(&mat, v);
		fprintf(f, "vn %f %f %f\n", v.x, v.y, v.z);
	}


	for (size_t fi = 0; fi < mesh->num_faces; fi++) {
		ufbx_face face = mesh->faces.data[fi];
		fprintf(f, "f");
		for (size_t ci = 0; ci < face.num_indices; ci++) {
			int32_t vi = mesh->vertex_position.indices.data[face.index_begin + ci];
			int32_t ti = mesh->vertex_uv.indices.data[face.index_begin + ci];
			int32_t ni = mesh->vertex_normal.indices.data[face.index_begin + ci];
			fprintf(f, " %d/%d/%d", vi + 1, ti + 1, ni + 1);
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

			for (size_t i = 0; i < mesh->vertex_position.values.count; i++) {
				ufbx_vec3 v = mesh->skinned_position.values.data[i];
				if (mesh->skinned_is_local) {
					v = ufbx_transform_position(&node->geometry_to_world, v);
				}
				fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
			}

			for (size_t i = 0; i < mesh->vertex_uv.values.count; i++) {
				ufbx_vec2 v = mesh->vertex_uv.values.data[i];
				fprintf(f, "vt %f %f\n", v.x, v.y);
			}

			ufbx_matrix mat = ufbx_matrix_for_normals(&node->geometry_to_world);
			for (size_t i = 0; i < mesh->skinned_normal.values.count; i++) {
				ufbx_vec3 v = mesh->skinned_normal.values.data[i];
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
				ufbx_face face = mesh->faces.data[fi];
				fprintf(f, "f");
				for (size_t ci = 0; ci < face.num_indices; ci++) {
					int32_t vi = v_off + mesh->skinned_position.indices.data[face.index_begin + ci];
					int32_t ni = n_off + mesh->skinned_normal.indices.data[face.index_begin + ci];
					if (mesh->vertex_uv.exists) {
						int32_t ti = t_off + mesh->vertex_uv.indices.data[face.index_begin + ci];
						fprintf(f, " %d/%d/%d", vi + 1, ti + 1, ni + 1);
					} else {
						fprintf(f, " %d//%d", vi + 1, ni + 1);
					}
				}
				fprintf(f, "\n");
			}

			fprintf(f, "\n");

			v_off += (int32_t)mesh->skinned_position.values.count;
			t_off += (int32_t)mesh->vertex_uv.values.count;
			n_off += (int32_t)mesh->skinned_normal.values.count;
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

static void ufbxt_assert_close_quat(ufbxt_diff_error *p_err, ufbx_quat a, ufbx_quat b)
{
	ufbxt_assert_close_real(p_err, a.x, b.x);
	ufbxt_assert_close_real(p_err, a.y, b.y);
	ufbxt_assert_close_real(p_err, a.z, b.z);
	ufbxt_assert_close_real(p_err, a.w, b.w);
}

static void ufbxt_assert_close_matrix(ufbxt_diff_error *p_err, ufbx_matrix a, ufbx_matrix b)
{
	ufbxt_assert_close_vec3(p_err, a.cols[0], b.cols[0]);
	ufbxt_assert_close_vec3(p_err, a.cols[1], b.cols[1]);
	ufbxt_assert_close_vec3(p_err, a.cols[2], b.cols[2]);
	ufbxt_assert_close_vec3(p_err, a.cols[3], b.cols[3]);
}

typedef struct {
	ufbx_vec3 pos;
	ufbx_vec3 normal;
	ufbx_vec2 uv;
} ufbxt_match_vertex;

static int ufbxt_cmp_sub_vertex(const void *va, const void *vb)
{
	const ufbxt_match_vertex *a = (const ufbxt_match_vertex*)va, *b = (const ufbxt_match_vertex*)vb;
	if (a->pos.x != b->pos.x) return a->pos.x < b->pos.x ? -1 : +1;
	if (a->pos.y != b->pos.y) return a->pos.y < b->pos.y ? -1 : +1;
	if (a->pos.z != b->pos.z) return a->pos.z < b->pos.z ? -1 : +1;
	if (a->normal.x != b->normal.x) return a->normal.x < b->normal.x ? -1 : +1;
	if (a->normal.y != b->normal.y) return a->normal.y < b->normal.y ? -1 : +1;
	if (a->normal.z != b->normal.z) return a->normal.z < b->normal.z ? -1 : +1;
	if (a->uv.x != b->uv.x) return a->uv.x < b->uv.x ? -1 : +1;
	if (a->uv.y != b->uv.y) return a->uv.y < b->uv.y ? -1 : +1;
	return 0;
}

static void ufbxt_match_obj_mesh(ufbxt_obj_file *obj, ufbx_node *fbx_node, ufbx_mesh *fbx_mesh, ufbxt_obj_mesh *obj_mesh, ufbxt_diff_error *p_err)
{
	ufbx_real tolerance = obj->tolerance;

	ufbxt_assert(fbx_mesh->num_faces == obj_mesh->num_faces);
	ufbxt_assert(fbx_mesh->num_indices == obj_mesh->num_indices);

	// Check that all vertices exist, anything more doesn't really make sense
	ufbxt_match_vertex *obj_verts = (ufbxt_match_vertex*)calloc(obj_mesh->num_indices, sizeof(ufbxt_match_vertex));
	ufbxt_match_vertex *fbx_verts = (ufbxt_match_vertex*)calloc(fbx_mesh->num_indices, sizeof(ufbxt_match_vertex));
	ufbxt_assert(obj_verts && fbx_verts);

	ufbx_matrix norm_mat = ufbx_get_compatible_matrix_for_normals(fbx_node);

	for (size_t i = 0; i < obj_mesh->num_indices; i++) {
		obj_verts[i].pos = ufbx_get_vertex_vec3(&obj_mesh->vertex_position, i);
		obj_verts[i].normal = ufbx_get_vertex_vec3(&obj_mesh->vertex_normal, i);
		if (obj_mesh->vertex_uv.exists) {
			obj_verts[i].uv = ufbx_get_vertex_vec2(&obj_mesh->vertex_uv, i);
		}
	}
	for (size_t i = 0; i < fbx_mesh->num_indices; i++) {
		ufbx_vec3 fp = ufbx_get_vertex_vec3(&fbx_mesh->skinned_position, i);
		ufbx_vec3 fn = ufbx_get_vertex_vec3(&fbx_mesh->skinned_normal, i);
		if (fbx_mesh->skinned_is_local) {
			fp = ufbx_transform_position(&fbx_node->geometry_to_world, fp);
			fn = ufbx_transform_direction(&norm_mat, fn);
		}
		fbx_verts[i].pos = fp;
		fbx_verts[i].normal = fn;
		if (obj_mesh->vertex_uv.exists) {
			ufbxt_assert(fbx_mesh->vertex_uv.exists);
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
			ufbx_real dnx = obj_verts[j].normal.x - v.normal.x;
			ufbx_real dny = obj_verts[j].normal.y - v.normal.y;
			ufbx_real dnz = obj_verts[j].normal.z - v.normal.z;
			ufbx_real du = obj_verts[j].uv.x - v.uv.x;
			ufbx_real dv = obj_verts[j].uv.y - v.uv.y;

			if (obj->bad_normals) {
				dnx = 0.0f;
				dny = 0.0f;
				dnz = 0.0f;
			}

			if (obj->bad_uvs) {
				du = 0.0f;
				dv = 0.0f;
			}

			ufbxt_assert(dx <= tolerance);
			ufbx_real err = (ufbx_real)sqrt(dx*dx + dy*dy + dz*dz + dnx*dnx + dny*dny + dnz*dnz + du*du + dv*dv);
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
	ufbx_node **used_nodes = (ufbx_node**)malloc(obj->num_meshes * sizeof(ufbx_node*));
	ufbxt_assert(used_nodes);

	size_t num_used_nodes = 0;

	for (size_t mesh_i = 0; mesh_i < obj->num_meshes; mesh_i++) {
		ufbxt_obj_mesh *obj_mesh = &obj->meshes[mesh_i];
		if (obj_mesh->num_indices == 0) continue;

		ufbx_node *node = NULL;

		for (size_t group_i = 0; group_i < obj_mesh->num_groups; group_i++) {
			const char *name = obj_mesh->groups[group_i];

			node = ufbx_find_node(scene, name);
			if (!node && obj->exporter == UFBXT_OBJ_EXPORTER_BLENDER) {
				// Blender concatenates _Material to names
				size_t name_len = strcspn(name, "_");
				node = ufbx_find_node_len(scene, name, name_len);
			}

			if (node && node->mesh) {
				bool seen = false;
				for (size_t i = 0; i < num_used_nodes; i++) {
					if (used_nodes[i] == node) {
						seen = true;
						break;
					}
				}
				if (!seen) break;
			}
		}

		ufbxt_assert(node);
		ufbx_mesh *mesh = node->mesh;

		used_nodes[num_used_nodes++] = node;

		if (!mesh && node->attrib_type == UFBX_ELEMENT_NURBS_SURFACE) {
			ufbx_nurbs_surface *surface = (ufbx_nurbs_surface*)node->attrib;
			ufbx_tessellate_opts opts = { 0 };
			opts.span_subdivision_u = surface->span_subdivision_u;
			opts.span_subdivision_v = surface->span_subdivision_v;
			ufbx_mesh *tess_mesh = ufbx_tessellate_nurbs_surface(surface, &opts, NULL);
			ufbxt_assert(tess_mesh);

			// ufbxt_debug_dump_obj_mesh("test.obj", node, tess_mesh);

			ufbxt_check_mesh(scene, tess_mesh);
			ufbxt_match_obj_mesh(obj, node, tess_mesh, obj_mesh, p_err);
			ufbx_free_mesh(tess_mesh);

			continue;
		}

		ufbxt_assert(mesh);

		ufbx_matrix *mat = &node->geometry_to_world;
		ufbx_matrix norm_mat = ufbx_get_compatible_matrix_for_normals(node);

		if (mesh->subdivision_display_mode == UFBX_SUBDIVISION_DISPLAY_SMOOTH || mesh->subdivision_display_mode == UFBX_SUBDIVISION_DISPLAY_HULL_AND_SMOOTH) {
			ufbx_mesh *sub_mesh = ufbx_subdivide_mesh(mesh, mesh->subdivision_preview_levels, NULL, NULL);
			ufbxt_assert(sub_mesh);

			ufbxt_check_mesh(scene, sub_mesh);
			ufbxt_match_obj_mesh(obj, node, sub_mesh, obj_mesh, p_err);
			ufbx_free_mesh(sub_mesh);

			continue;
		}

		ufbxt_assert(obj_mesh->num_faces == mesh->num_faces);
		ufbxt_assert(obj_mesh->num_indices == mesh->num_indices);

		bool check_normals = true;
		if (obj->bad_normals) check_normals = false;
		if (!check_deformed_normals && mesh->all_deformers.count > 0) check_normals = false;

		if (obj->bad_order) {
			ufbxt_match_obj_mesh(obj, node, mesh, obj_mesh, p_err);
		} else {
			// Assume that the indices are in the same order!
			for (size_t face_ix = 0; face_ix < mesh->num_faces; face_ix++) {
				ufbx_face obj_face = obj_mesh->faces[face_ix];
				ufbx_face face = mesh->faces.data[face_ix];
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

					if (obj->normalize_units) {
						fp.x *= scene->settings.unit_meters;
						fp.y *= scene->settings.unit_meters;
						fp.z *= scene->settings.unit_meters;
						op.x *= scene->settings.unit_meters;
						op.y *= scene->settings.unit_meters;
						op.z *= scene->settings.unit_meters;
					}

					ufbxt_assert_close_vec3(p_err, op, fp);

					if (check_normals) {
						ufbx_real on2 = on.x*on.x + on.y*on.y + on.z*on.z;
						if (on2 > 0.01f) {
							ufbxt_assert_close_vec3(p_err, on, fn);
						}
					}

					if (obj_mesh->vertex_uv.exists && !obj->bad_uvs) {
						ufbxt_assert(mesh->vertex_uv.exists);
						ufbx_vec2 ou = ufbx_get_vertex_vec2(&obj_mesh->vertex_uv, ix);
						ufbx_vec2 fu = ufbx_get_vertex_vec2(&mesh->vertex_uv, ix);
						ufbxt_assert_close_vec2(p_err, ou, fu);
					}
				}
			}
		}
	}

	free(used_nodes);
}

// -- IO

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

#endif
