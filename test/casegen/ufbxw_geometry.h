#ifndef UFBXW_GEOMETRY_H_INCLUDED
#define UFBXW_GEOMETRY_H_INCLUDED

// Geometry processing utilities.
//
// In one C or C++ translation unit, do the following:
//
//     #include "path/to/ufbx_write.h"
//
//     #define UFBXW_GEOMETRY_IMPLEMENTATION
//     #include "path/to/ufbxw_geometry.h"

#if !defined(UFBXW_VERSION)
	#error "Please include ufbx_write.h before implementing ufbxw_geometry.h"
#endif

#if !defined(ufbxw_geometry_abi)
	#if defined(UFBXW_GEOMETRY_STATIC)
		#define ufbxw_geometry_abi static
	#else
		#define ufbxw_geometry_abi
	#endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

ufbxw_geometry_abi void ufbxw_generate_flat_normals(ufbxw_scene *scene, ufbxw_mesh mesh);

#if defined(__cplusplus)
}
#endif

#endif

#ifdef UFBXW_GEOMETRY_IMPLEMENTATION
#ifndef UFBXW_GEOMETRY_H_IMPLEMENTED
#define UFBXW_GEOMETRY_H_IMPLEMENTED

#if !defined(UFBXW_VERSION)
	#error "Please include ufbx_write.h before implementing ufbxw_geometry.h"
#endif

#include <math.h>

#if defined(__cplusplus)
extern "C" {
#endif

static bool ufbxwi_geometry_validate_topology(ufbxw_const_vec3_list vertices, ufbxw_const_int_list indices, ufbxw_const_int_list face_offsets)
{
	if (face_offsets.count == 0) return false;
	if (face_offsets.data[0] != 0) return false;
	if ((size_t)face_offsets.data[face_offsets.count - 1] != indices.count) return false;

	for (size_t face_ix = 0; face_ix + 1 < face_offsets.count; face_ix++) {
		int32_t begin = face_offsets.data[face_ix];
		int32_t end = face_offsets.data[face_ix + 1];
		if (begin < 0 || end < begin || (size_t)end > indices.count) return false;
	}

	for (size_t index_ix = 0; index_ix < indices.count; index_ix++) {
		int32_t vertex_ix = indices.data[index_ix];
		if (vertex_ix < 0 || (size_t)vertex_ix >= vertices.count) return false;
	}

	return true;
}

ufbxw_geometry_abi void ufbxw_generate_flat_normals(ufbxw_scene *scene, ufbxw_mesh mesh)
{
	ufbxw_const_vec3_list vertices = ufbxw_view_vec3_buffer(scene, ufbxw_mesh_get_vertices(scene, mesh));
	ufbxw_const_int_list indices = ufbxw_view_int_buffer(scene, ufbxw_mesh_get_vertex_indices(scene, mesh));
	ufbxw_const_int_list face_offsets = ufbxw_view_int_buffer(scene, ufbxw_mesh_get_face_offsets(scene, mesh));
	if (!vertices.data || !indices.data || !face_offsets.data) return;
	if (!ufbxwi_geometry_validate_topology(vertices, indices, face_offsets)) return;

	size_t face_count = face_offsets.count - 1;
	ufbxw_vec3_buffer normal_buffer = ufbxw_create_vec3_buffer(scene, face_count);
	ufbxw_vec3_list normals = ufbxw_edit_vec3_buffer(scene, normal_buffer);
	if (!normals.data) return;

	for (size_t face_ix = 0; face_ix < face_count; face_ix++) {
		int32_t begin = face_offsets.data[face_ix];
		int32_t end = face_offsets.data[face_ix + 1];
		ufbxw_vec3 normal = { 0 };

		for (int32_t index_ix = begin; index_ix < end; index_ix++) {
			int32_t next_ix = index_ix + 1 < end ? index_ix + 1 : begin;
			ufbxw_vec3 a = vertices.data[indices.data[index_ix]];
			ufbxw_vec3 b = vertices.data[indices.data[next_ix]];
			normal.x += (a.y - b.y) * (a.z + b.z);
			normal.y += (a.z - b.z) * (a.x + b.x);
			normal.z += (a.x - b.x) * (a.y + b.y);
		}

		ufbxw_real length_sq = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
		if (length_sq > 0.0f) {
			ufbxw_real inv_length = (ufbxw_real)(1.0 / sqrt((double)length_sq));
			normal.x *= inv_length;
			normal.y *= inv_length;
			normal.z *= inv_length;
		}

		normals.data[face_ix] = normal;
	}

	ufbxw_mesh_set_normals(scene, mesh, normal_buffer, UFBXW_ATTRIBUTE_MAPPING_POLYGON);
}

#if defined(__cplusplus)
}
#endif

#endif
#endif
