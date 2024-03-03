#pragma once

#include <stdint.h>
#include <string.h>

typedef double ufbxw_real;

typedef struct ufbxw_vec3 {
	union {
		struct { ufbxw_real x, y, z; };
		ufbxw_real v[3];
	};
} ufbxw_vec3;

typedef struct ufbxw_quat {
	union {
		struct { ufbxw_real x, y, z, w; };
		ufbxw_real v[4];
	};
} ufbxw_quat;

typedef struct ufbxw_transform {
	ufbxw_vec3 translation;
	ufbxw_quat rotation;
	ufbxw_vec3 scale;
} ufbxw_transform;

typedef struct ufbxw_element {
	uint64_t id;
};

typedef struct ufbxw_node {
	union {
		ufbxw_element element;
		uint64_t id;
	};
} ufbxw_node;

typedef struct ufbxw_mesh {
	union {
		ufbxw_element element;
		uint64_t id;
	};
} ufbxw_mesh;

ufbxw_node ufbcxw_as_node(ufbxw_element e);
ufbxw_mesh ufbcxw_as_mesh(ufbxw_element e);

typedef struct ufbxw_scene ufbxw_scene;

typedef enum ufbxw_element_type {
	UFBXW_ELEMENT_UNKNOWN,
	UFBXW_ELEMENT_NODE,
	UFBXW_ELEMENT_MESH,
} ufbxw_element_type;

typedef enum ufbxw_rotation_order {
	UFBXW_ROTATION_ORDER_XYZ,
	UFBXW_ROTATION_ORDER_XZY,
	UFBXW_ROTATION_ORDER_YZX,
	UFBXW_ROTATION_ORDER_YXZ,
	UFBXW_ROTATION_ORDER_ZXY,
	UFBXW_ROTATION_ORDER_ZYX,
	UFBXW_ROTATION_ORDER_SPHERIC,
} ufbxw_rotation_order;

// -- API

ufbxw_scene *ufbxw_create_scene();

ufbxw_element ufbxw_add_element_len(ufbxw_scene *scene, ufbxw_element_type type, const char *name, size_t name_len);
ufbxw_element ufbxw_add_element(ufbxw_scene *scene, ufbxw_element_type type, const char *name) {
	return ufbxw_add_element_len(scene, type, name, name ? strlen(name) : 0);
}

void ufbxw_element_set_name(ufbxw_scene *scene, ufbxw_element element, const char *name);

// -- Nodes

inline ufbxw_node ufbxw_add_node_len(ufbxw_scene *scene, const char *name, size_t name_len) {
	return ufbcxw_as_node(ufbxw_add_element_len(scene, UFBXW_ELEMENT_NODE, name, name_len));
}
inline ufbxw_node ufbxw_add_node(ufbxw_scene *scene, const char *name) {
	return ufbxw_add_node_len(scene, name, name ? strlen(name) : 0);
}

void ufbxw_node_set_position(ufbxw_scene *scene, ufbxw_node node, ufbxw_vec3 position);
void ufbxw_node_set_rotation(ufbxw_scene *scene, ufbxw_node node, ufbxw_quat rotation);
void ufbxw_node_set_rotation_explicit(ufbxw_scene *scene, ufbxw_node node, ufbxw_quat rotation, ufbxw_rotation_order order);
void ufbxw_node_set_rotation_euler(ufbxw_scene *scene, ufbxw_node node, ufbxw_vec3 euler, ufbxw_rotation_order order);
void ufbxw_node_set_scale(ufbxw_scene *scene, ufbxw_node node, ufbxw_vec3 scale);

void ufbxw_node_set_transform(ufbxw_scene *scene, ufbxw_node node, ufbxw_transform scale);

void ufbxw_node_set_parent(ufbxw_scene *scene, ufbxw_node node, ufbxw_node parent);

// -- Meshes

inline ufbxw_mesh ufbxw_add_mesh_len(ufbxw_scene *scene, const char *name, size_t name_len) {
	return ufbcxw_as_mesh(ufbxw_add_element_len(scene, UFBXW_ELEMENT_MESH, name, name_len));
}
inline ufbxw_mesh ufbxw_add_mesh(ufbxw_scene *scene, const char *name) {
	return ufbxw_add_mesh_len(scene, name, name ? strlen(name) : 0);
}

typedef enum ufbxw_mesh_attribute {
	UFBXW_MESH_ATTRIBUTE_POSITION,
	UFBXW_MESH_ATTRIBUTE_NORMAL,
	UFBXW_MESH_ATTRIBUTE_UV,
} ufbxw_mesh_attribute;

void ufbxw_mesh_add_geometry(ufbxw_scene *scene, ufbxw_mesh mesh, ufbxw_mesh_attribute attrib, size_t index, size_t num_values, size_t num_indices);
