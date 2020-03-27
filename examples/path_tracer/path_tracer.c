#define RHMAP_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "rtk.h"
#include "rhmap.h"
#include "stb_image_write.h"
#include "../../ufbx.h"
#include "../../ufbx.c"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// -- Vector operations

static rtk_vec3 v_make(rtk_real x, rtk_real y, rtk_real z)
{
	rtk_vec3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

static rtk_vec3 v_add(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
	v.x = a.x + b.x;
	v.y = a.y + b.y;
	v.z = a.z + b.z;
	return v;
}

static rtk_vec3 v_sub(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	v.z = a.z - b.z;
	return v;
}

static rtk_vec3 v_mul(rtk_vec3 a, rtk_real b) {
	rtk_vec3 v;
	v.x = a.x * b;
	v.y = a.y * b;
	v.z = a.z * b;
	return v;
}

static rtk_vec3 v_mad(rtk_vec3 a, rtk_real b, rtk_vec3 c) {
	rtk_vec3 v;
	v.x = a.x * b + c.x;
	v.y = a.y * b + c.y;
	v.z = a.z * b + c.z;
	return v;
}

static rtk_vec3 v_mul_comp(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
	v.x = a.x * b.x;
	v.y = a.y * b.y;
	v.z = a.z * b.z;
	return v;
}

static rtk_real v_dot(rtk_vec3 a, rtk_vec3 b) {
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

static rtk_vec3 v_cross(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
	v.x = a.y*b.z - a.z*b.y;
	v.y = a.z*b.x - a.x*b.z;
	v.z = a.x*b.y - a.y*b.x;
	return v;
}

static rtk_vec3 v_normalize(rtk_vec3 a) {
	rtk_real rcp_len = 1.0f / sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
	rtk_vec3 v;
	v.x = a.x * rcp_len;
	v.y = a.y * rcp_len;
	v.z = a.z * rcp_len;
	return v;
}

#define pt_alloc(type, n) (type*)calloc((n), sizeof(type))
#define pt_realloc(type, ptr, n) (type*)realloc((ptr), (n) * sizeof(type))

typedef struct {
	rtk_vec3 position;
	rtk_vec3 normal;
	rtk_vec2 uv;
} pt_vertex;

typedef struct {
	rhmap map;
	pt_vertex *vertices;
} pt_vertex_map;

static uint32_t pt_hash_buf(const void *data, size_t size)
{
	uint32_t hash = 0;

	const uint32_t seed = UINT32_C(0x9e3779b9);
	const uint32_t *word = (const uint32_t*)data;
	while (size >= 4) {
		hash = ((hash << 5u | hash >> 27u) ^ *word++) * seed;
		size -= 4;
	}

	const uint8_t *byte = (const uint8_t*)word;
	if (size > 0) {
		uint32_t w = 0;
		while (size > 0) {
			w = w << 8 | *byte++;
			size--;
		}
		hash = ((hash << 5u | hash >> 27u) ^ w) * seed;
	}

	return (uint32_t)hash;
}

static uint32_t pt_vertex_map_insert(pt_vertex_map *map, size_t min_verts, const pt_vertex *vert)
{
	if (map->map.size == map->map.capacity) {
		size_t count, alloc_size;
		rhmap_grow(&map->map, &count, &alloc_size, min_verts, 0.7);
		map->vertices = pt_realloc(pt_vertex, map->vertices, count);
		free(rhmap_rehash(&map->map, count, alloc_size, malloc(alloc_size)));
	}

	rhmap_iter iter = { &map->map, pt_hash_buf(vert, sizeof(pt_vertex)) };
	uint32_t index;
	while (rhmap_find(&iter, &index)) {
		if (!memcmp(&map->vertices[index], vert, sizeof(pt_vertex))) {
			return index;
		}
	}

	index = map->map.size;
	rhmap_insert(&iter, index);
	map->vertices[index] = *vert;

	return index;
}

static rtk_vec2 to_rtk_vec2(ufbx_vec2 v)
{
	rtk_vec2 r = { (rtk_real)v.x, (rtk_real)v.y };
	return r;
}

static rtk_vec3 to_rtk_vec3(ufbx_vec3 v)
{
	rtk_vec3 r = { (rtk_real)v.x, (rtk_real)v.y, (rtk_real)v.z };
	return r;
}

static rtk_matrix to_rtk_matrix(ufbx_matrix m)
{
	rtk_matrix r;
	r.cols[0] = to_rtk_vec3(m.cols[0]);
	r.cols[1] = to_rtk_vec3(m.cols[1]);
	r.cols[2] = to_rtk_vec3(m.cols[2]);
	r.cols[3] = to_rtk_vec3(m.cols[3]);
	return r;
}

static uint32_t pt_insert_vertex(pt_vertex_map *map, const ufbx_mesh *mesh, uint32_t ix)
{
	pt_vertex v = { 0 };
	v.position = to_rtk_vec3(ufbx_get_vertex_vec3(&mesh->vertex_position, ix));
	if (mesh->vertex_normal.data) {
		v.normal = to_rtk_vec3(ufbx_get_vertex_vec3(&mesh->vertex_normal, ix));
	}
	if (mesh->vertex_uv.data) {
		v.uv = to_rtk_vec2(ufbx_get_vertex_vec2(&mesh->vertex_uv, ix));
	}

	return pt_vertex_map_insert(map, mesh->num_vertices, &v);
}

rtk_scene *g_scene;
rtk_vec3 g_camera_pos;
rtk_vec3 g_camera_dir;

int g_width = 1280;
int g_height = 720;

uint8_t *g_image;

static rtk_vec3 pt_trace_ray(rtk_ray *ray)
{
	rtk_hit hit;
	if (rtk_raytrace(g_scene, ray, &hit, RTK_INF)) {
		return v_make(0.7f, 0.7f, 0.7f);
	} else {
		return v_make(0.0f, 0.0f, 0.0f);
	}
}

static void pt_raytrace()
{
	rtk_vec3 up = v_make(0.0f, 1.0f, 0.0f);
	rtk_vec3 right = v_cross(g_camera_dir, up);
	up = v_cross(right, g_camera_dir);

	for (int y = 0; y < g_height; y++)
	for (int x = 0; x < g_width; x++) {
		rtk_vec2 v;
		v.x = (float)(x - g_width/2) / (float)g_height;
		v.y = -(float)(y - g_height/2) / (float)g_height;

		rtk_ray ray;
		ray.origin = g_camera_pos;
		ray.direction = v_add(g_camera_dir, v_add(v_mul(right, v.x), v_mul(up, v.y)));
		ray.min_t = 0.0f;
		rtk_vec3 col = pt_trace_ray(&ray);

		uint8_t *pixel = g_image + (y * g_width + x) * 4;
		pixel[0] = (uint8_t)(col.x * 255.0f);
		pixel[1] = (uint8_t)(col.y * 255.0f);
		pixel[2] = (uint8_t)(col.z * 255.0f);
		pixel[3] = 0xff;
	}
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: path_tracer <input> <output> <opts>\n");
		return 1;
	}

	int num_samples = 1;

	for (int i = 3; i < argc; i++) {

		if (!strcmp(argv[i], "--samples")) {
			if (++i < argc) {
				num_samples = atoi(argv[i]);
			}
		}

		if (!strcmp(argv[i], "--camera")) {
			if (i + 6 < argc) {
				g_camera_pos.x = (rtk_real)atof(argv[++i]);
				g_camera_pos.y = (rtk_real)atof(argv[++i]);
				g_camera_pos.z = (rtk_real)atof(argv[++i]);
				g_camera_dir.x = (rtk_real)atof(argv[++i]);
				g_camera_dir.y = (rtk_real)atof(argv[++i]);
				g_camera_dir.z = (rtk_real)atof(argv[++i]);
				g_camera_dir = v_normalize(g_camera_dir);
			}
		}

	}

	ufbx_error error;
	ufbx_scene *scene = ufbx_load_file(argv[1], NULL, &error);
	if (!scene) {
		for (size_t i = 0; i < error.stack_size; i++) {
			ufbx_error_frame *err = &error.stack[i];
			fprintf(stderr, "Line %u %s(): %s\n", err->source_line, err->function, err->description);
		}
		return 1;
	}

	rtk_mesh *meshes = pt_alloc(rtk_mesh, scene->meshes.size);
	rtk_scene_desc desc = { 0 };
	desc.meshes = meshes;
	desc.num_meshes = scene->meshes.size;

	for (size_t i = 0; i < scene->meshes.size; i++) {
		ufbx_mesh *mesh = &scene->meshes.data[i];
		pt_vertex_map map = { 0 };

		uint32_t *indices = pt_alloc(uint32_t, mesh->num_triangles * 3);
		uint32_t *dst_index = indices;

		for (size_t fi = 0; fi < mesh->num_faces; fi++) {
			ufbx_face face = mesh->faces[fi];
			for (uint32_t i = 1; i + 2 <= face.num_indices; i++) {
				dst_index[0] = pt_insert_vertex(&map, mesh, face.index_begin);
				dst_index[1] = pt_insert_vertex(&map, mesh, face.index_begin + i);
				dst_index[2] = pt_insert_vertex(&map, mesh, face.index_begin + i + 1);
				dst_index += 3;
			}
		}

		rhmap_reset(&map.map);

		rtk_mesh *dst = &meshes[i];
		dst->num_triangles = mesh->num_triangles;
		dst->indices = indices;
		dst->uvs_stride = dst->normals_stride = dst->vertices_stride = sizeof(pt_vertex);
		dst->vertices = &map.vertices[0].position;
		dst->normals = &map.vertices[0].normal;
		dst->uvs = &map.vertices[0].uv;
		dst->transform = to_rtk_matrix(mesh->node.to_root);
	}

	g_scene = rtk_create_scene(&desc);

	ufbx_free_scene(scene);

	g_image = pt_alloc(uint8_t, g_width * g_height * 4);
	pt_raytrace();

	stbi_write_png(argv[2], g_width, g_height, 4, g_image, 0);

	return 0;
}
