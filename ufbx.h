#ifndef UFBX_UFBX_H_INLCUDED
#define UFBX_UFBX_H_INLCUDED

// -- Headers

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

// -- Platform

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
	#pragma warning(disable: 4505) // unreferenced local function has been removed
	#define ufbx_inline static __forceinline
#elif defined(__GNUC__)
	#define ufbx_inline static inline __attribute__((always_inline, unused))
#else
	#define ufbx_inline static
#endif

// -- Configuration

typedef double ufbx_real;

#define UFBX_ERROR_STACK_MAX_DEPTH 8

// -- Basic types

typedef struct ufbx_string {
	const char *data;
	size_t length;
} ufbx_string;

typedef struct ufbx_vec2 {
	union {
		struct { ufbx_real x, y; };
		ufbx_real v[2];
	};
} ufbx_vec2;

typedef struct ufbx_vec3 {
	union {
		struct { ufbx_real x, y, z; };
		ufbx_real v[3];
	};
} ufbx_vec3;

typedef struct ufbx_vec4 {
	union {
		struct { ufbx_real x, y, z, w; };
		ufbx_real v[4];
	};
} ufbx_vec4;

typedef enum ufbx_rotation_order {
	UFBX_ROTATION_XYZ,
	UFBX_ROTATION_XZY,
	UFBX_ROTATION_YZX,
	UFBX_ROTATION_YXZ,
	UFBX_ROTATION_ZXY,
	UFBX_ROTATION_ZYX,
} ufbx_rotation_order;

typedef struct ufbx_transform {
	ufbx_vec3 translation;
	ufbx_vec4 rotation;
	ufbx_vec3 scale;
} ufbx_transform;

typedef struct ufbx_matrix {
	union {
		struct {
			ufbx_real m00, m10, m20;
			ufbx_real m01, m11, m21;
			ufbx_real m02, m12, m22;
			ufbx_real m03, m13, m23;
		};
		ufbx_vec3 cols[4];
		ufbx_real v[12];
	};
} ufbx_matrix;

// -- Properties

typedef struct ufbx_prop ufbx_prop;
typedef struct ufbx_props ufbx_props;

typedef enum ufbx_prop_type {
	UFBX_PROP_UNKNOWN,
	UFBX_PROP_BOOLEAN,
	UFBX_PROP_INTEGER,
	UFBX_PROP_NUMBER,
	UFBX_PROP_VECTOR,
	UFBX_PROP_COLOR,
	UFBX_PROP_STRING,
	UFBX_PROP_DATE_TIME,
	UFBX_PROP_TRANSLATION,
	UFBX_PROP_ROTATION,
	UFBX_PROP_SCALING,

	UFBX_NUM_PROP_TYPES,
} ufbx_prop_type;

struct ufbx_prop {
	ufbx_string name;
	uint32_t imp_key;
	ufbx_prop_type type;

	ufbx_string value_str;
	int64_t value_int;
	union {
		ufbx_real value_real_arr[3];
		ufbx_real value_real;
		ufbx_vec2 value_vec2;
		ufbx_vec3 value_vec3;
	};
};

struct ufbx_props {
	ufbx_prop *props;
	size_t num_props;

	ufbx_props *defaults;
};

// -- Scene data

typedef struct ufbx_node ufbx_node;
typedef struct ufbx_model ufbx_model;
typedef struct ufbx_mesh ufbx_mesh;
typedef struct ufbx_light ufbx_light;
typedef struct ufbx_bone ufbx_bone;

typedef struct ufbx_node_ptr_list { ufbx_node **data; size_t size; } ufbx_node_ptr_list;
typedef struct ufbx_model_list { ufbx_model *data; size_t size; } ufbx_model_list;
typedef struct ufbx_mesh_list { ufbx_mesh *data; size_t size; } ufbx_mesh_list;
typedef struct ufbx_light_list { ufbx_light *data; size_t size; } ufbx_light_list;
typedef struct ufbx_bone_list { ufbx_bone *data; size_t size; } ufbx_bone_list;

typedef struct ufbx_material ufbx_material;

typedef struct ufbx_vertex_void ufbx_vertex_void;
typedef struct ufbx_vertex_real ufbx_vertex_real;
typedef struct ufbx_vertex_vec2 ufbx_vertex_vec2;
typedef struct ufbx_vertex_vec3 ufbx_vertex_vec3;
typedef struct ufbx_vertex_vec4 ufbx_vertex_vec4;
typedef struct ufbx_uv_set ufbx_uv_set;
typedef struct ufbx_color_set ufbx_color_set;
typedef struct ufbx_edge ufbx_edge;
typedef struct ufbx_face ufbx_face;
typedef struct ufbx_skin ufbx_skin;

typedef struct ufbx_material_list { ufbx_material *data; size_t size; } ufbx_material_list;
typedef struct ufbx_material_ptr_list { ufbx_material **data; size_t size; } ufbx_material_ptr_list;
typedef struct ufbx_uv_set_list { ufbx_uv_set *data; size_t size; } ufbx_uv_set_list;
typedef struct ufbx_color_set_list { ufbx_color_set *data; size_t size; } ufbx_color_set_list;
typedef struct ufbx_skin_list { ufbx_skin *data; size_t size; } ufbx_skin_list;

struct ufbx_vertex_void {
	void *data;
	int32_t *indices;
	size_t num_elements;
};

struct ufbx_vertex_real {
	ufbx_real *data;
	int32_t *indices;
	size_t num_elements;
};

struct ufbx_vertex_vec2 {
	ufbx_vec2 *data;
	int32_t *indices;
	size_t num_elements;
};

struct ufbx_vertex_vec3 {
	ufbx_vec3 *data;
	int32_t *indices;
	size_t num_elements;
};

struct ufbx_vertex_vec4 {
	ufbx_vec4 *data;
	int32_t *indices;
	size_t num_elements;
};

struct ufbx_uv_set {
	ufbx_string name;
	int32_t index;
	ufbx_vertex_vec2 vertex_uv;
	ufbx_vertex_vec3 vertex_binormal;
	ufbx_vertex_vec3 vertex_tangent;
};

struct ufbx_color_set {
	ufbx_string name;
	int32_t index;
	ufbx_vertex_vec4 vertex_color;
};

struct ufbx_edge {
	uint32_t indices[2];
};

struct ufbx_face {
	uint32_t index_begin;
	uint32_t num_indices;
};

struct ufbx_skin {
	ufbx_node *bone;

	ufbx_matrix mesh_to_bind;
	ufbx_matrix bind_to_world;

	size_t num_weights;

	int32_t *indices;
	ufbx_real *weights;
};

struct ufbx_material {
	ufbx_string name;

	ufbx_props props;

	ufbx_vec3 diffuse_color;
	ufbx_vec3 specular_color;
};

typedef enum ufbx_node_type {
	UFBX_NODE_UNKNOWN,
	UFBX_NODE_MODEL,
	UFBX_NODE_MESH,
	UFBX_NODE_LIGHT,
	UFBX_NODE_BONE,
} ufbx_node_type;

typedef enum ufbx_inherit_type {
	UFBX_INHERIT_NO_SHEAR,  // R*r*S*s
	UFBX_INHERIT_NORMAL,    // R*S*r*s
	UFBX_INHERIT_NO_SCALE,  // R*r*s
} ufbx_inherit_type;

struct ufbx_node {
	ufbx_node_type type;
	ufbx_string name;
	ufbx_props props;
	ufbx_node *parent;
	ufbx_inherit_type inherit_type;
	ufbx_transform transform;
	ufbx_transform world_transform;
	ufbx_matrix to_parent;
	ufbx_matrix to_root;
	ufbx_node_ptr_list children;
};

struct ufbx_model {
	ufbx_node node;
};

struct ufbx_mesh {
	ufbx_node node;

	size_t num_vertices;
	size_t num_indices;
	size_t num_triangles;
	size_t num_faces;
	size_t num_bad_faces;
	size_t num_edges;

	ufbx_face *faces;
	ufbx_edge *edges;

	ufbx_vertex_vec3 vertex_position;
	ufbx_vertex_vec3 vertex_normal;
	ufbx_vertex_vec3 vertex_binormal;
	ufbx_vertex_vec3 vertex_tangent;
	ufbx_vertex_vec2 vertex_uv;
	ufbx_vertex_vec4 vertex_color;
	ufbx_vertex_real vertex_crease;

	bool *edge_smoothing;
	ufbx_real *edge_crease;

	bool *face_smoothing;
	int32_t *face_material;

	ufbx_uv_set_list uv_sets;
	ufbx_color_set_list color_sets;
	ufbx_material_ptr_list materials;
	ufbx_skin_list skins;
};

struct ufbx_light {
	ufbx_node node;

	ufbx_vec3 color;
	ufbx_real intensity;
};

struct ufbx_bone {
	ufbx_node node;

	ufbx_real length;
};

// -- Animations

typedef struct ufbx_anim_stack ufbx_anim_stack;
typedef struct ufbx_anim_layer ufbx_anim_layer;
typedef struct ufbx_anim_prop ufbx_anim_prop;
typedef struct ufbx_anim_curve ufbx_anim_curve;
typedef struct ufbx_keyframe ufbx_keyframe;
typedef struct ufbx_tangent ufbx_tangent;

typedef struct ufbx_anim_stack_list { ufbx_anim_stack *data; size_t size; } ufbx_anim_stack_list;
typedef struct ufbx_anim_layer_list { ufbx_anim_layer *data; size_t size; } ufbx_anim_layer_list;
typedef struct ufbx_anim_layer_ptr_list { ufbx_anim_layer **data; size_t size; } ufbx_anim_layer_ptr_list;
typedef struct ufbx_anim_prop_list { ufbx_anim_prop *data; size_t size; } ufbx_anim_prop_list;
typedef struct ufbx_anim_curve_list { ufbx_anim_curve *data; size_t size; } ufbx_anim_curve_list;
typedef struct ufbx_keyframe_list { ufbx_keyframe *data; size_t size; } ufbx_keyframe_list;

struct ufbx_tangent {
	float dx;
	float dy;
};

typedef enum ufbx_interpolation {
	UFBX_INTERPOLATION_CONSTANT_PREV,
	UFBX_INTERPOLATION_CONSTANT_NEXT,
	UFBX_INTERPOLATION_LINEAR,
	UFBX_INTERPOLATION_CUBIC,
} ufbx_interpolation;

typedef enum ufbx_anim_target {
	UFBX_ANIM_UNKNOWN,
	UFBX_ANIM_ANIM_LAYER,
	UFBX_ANIM_MODEL,
	UFBX_ANIM_MESH,
	UFBX_ANIM_LIGHT,
	UFBX_ANIM_MATERIAL,
	UFBX_ANIM_BONE,
	UFBX_ANIM_INVALID,
} ufbx_anim_target;

struct ufbx_anim_stack {
	ufbx_string name;
	ufbx_props props;

	double time_begin;
	double time_end;

	ufbx_anim_layer_ptr_list layers;
};

struct ufbx_anim_layer {
	ufbx_string name;
	ufbx_props layer_props;

	ufbx_anim_prop_list props;
	ufbx_real weight;
	bool compose_rotation;
	bool compose_scale;
};

struct ufbx_anim_curve {
	ufbx_real default_value;
	uint32_t index;
	ufbx_anim_prop *prop;
	ufbx_keyframe_list keyframes;
};

struct ufbx_anim_prop {
	ufbx_string name;
	uint32_t imp_key;
	ufbx_anim_layer *layer;
	ufbx_anim_target target;
	uint32_t index;
	ufbx_anim_curve curves[3];
};

struct ufbx_keyframe {
	double time;
	ufbx_real value;
	ufbx_interpolation interpolation;
	ufbx_tangent left;
	ufbx_tangent right;
};

// -- Scene

typedef struct ufbx_scene ufbx_scene;

typedef struct ufbx_metadata {
	bool ascii;
	uint32_t version;
	ufbx_string creator;

	size_t result_memory_used;
	size_t temp_memory_used;
	size_t result_allocs;
	size_t temp_allocs;

	size_t num_total_child_refs;
	size_t num_total_material_refs;
	size_t num_total_skins;
	size_t num_skinned_positions;
	size_t num_skinned_indices;
	size_t max_skinned_positions;
	size_t max_skinned_indices;

	double ktime_to_sec;
} ufbx_metadata;

struct ufbx_scene {
	ufbx_metadata metadata;

	ufbx_model *root;

	ufbx_node_ptr_list nodes;
	ufbx_model_list models;
	ufbx_mesh_list meshes;
	ufbx_light_list lights;
	ufbx_bone_list bones;

	ufbx_material_list materials;

	ufbx_anim_stack_list anim_stacks;
	ufbx_anim_layer_list anim_layers;
	ufbx_anim_prop_list anim_props;
	ufbx_anim_curve_list anim_curves;
};

// -- Loading

typedef void *ufbx_alloc_fn(void *user, size_t size);
typedef void *ufbx_realloc_fn(void *user, void *old_ptr, size_t old_size, size_t new_size);
typedef void ufbx_free_fn(void *user, void *ptr, size_t size);
typedef size_t ufbx_read_fn(void *user, void *data, size_t size);

typedef struct ufbx_error_frame {
	uint32_t source_line;
	const char *function;
	const char *description;
} ufbx_error_frame;

typedef struct ufbx_error {
	uint32_t stack_size;
	ufbx_error_frame stack[UFBX_ERROR_STACK_MAX_DEPTH];
} ufbx_error;

typedef struct ufbx_allocator {
	ufbx_alloc_fn *alloc_fn;
	ufbx_realloc_fn *realloc_fn;
	ufbx_free_fn *free_fn;
	void *user;
} ufbx_allocator;

typedef struct ufbx_load_opts {
	ufbx_allocator temp_allocator;
	ufbx_allocator result_allocator;

	// Preferences
	bool ignore_geometry;
	bool ignore_animation;

	// Limits
	size_t max_temp_memory;
	size_t max_result_memory;
	size_t max_temp_allocs;
	size_t max_result_allocs;
	size_t temp_huge_size;
	size_t result_huge_size;
	size_t max_ascii_token_length;
	size_t read_buffer_size;
	size_t max_properties;

	uint32_t max_string_length;
	uint32_t max_strings;
	uint32_t max_node_depth;
	uint32_t max_node_values;
	uint32_t max_node_children;
	uint32_t max_array_size;
	uint32_t max_child_depth;

	bool allow_nonexistent_indices;
} ufbx_load_opts;

typedef struct ufbx_evaluate_opts {
	ufbx_scene *reuse_scene;

	ufbx_allocator allocator;

	bool evaluate_skinned_vertices;

	const ufbx_anim_layer *layer;
} ufbx_evaluate_opts;

// -- Inflate

typedef struct ufbx_inflate_input ufbx_inflate_input;
typedef struct ufbx_inflate_retain ufbx_inflate_retain;

struct ufbx_inflate_input {
	size_t total_size;

	const void *data;
	size_t data_size;

	void *buffer;
	size_t buffer_size;

	ufbx_read_fn *read_fn;
	void *read_user;
};

struct ufbx_inflate_retain {
	bool initialized;
	uint64_t data[512];
};

// -- API

#ifdef __cplusplus
extern "C" {
#endif

extern const ufbx_string ufbx_empty_string;
extern const ufbx_matrix ufbx_identity_matrix;
extern const ufbx_transform ufbx_identity_transform;

ufbx_scene *ufbx_load_memory(const void *data, size_t size, const ufbx_load_opts *opts, ufbx_error *error);
ufbx_scene *ufbx_load_file(const char *filename, const ufbx_load_opts *opts, ufbx_error *error);
ufbx_scene *ufbx_load_stdio(void *file, const ufbx_load_opts *opts, ufbx_error *error);
void ufbx_free_scene(ufbx_scene *scene);

ufbx_node *ufbx_find_node_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_mesh *ufbx_find_mesh_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_material *ufbx_find_material_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_light *ufbx_find_light_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_anim_stack *ufbx_find_anim_stack_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_anim_layer *ufbx_find_anim_layer_len(const ufbx_scene *scene, const char *name, size_t name_len);

ufbx_prop *ufbx_find_prop_len(const ufbx_props *props, const char *name, size_t name_len);

ufbx_anim_prop *ufbx_find_node_anim_prop_begin(const ufbx_scene *scene, const ufbx_anim_layer *layer, const ufbx_node *node);

ufbx_face *ufbx_find_face(const ufbx_mesh *mesh, size_t index);

ufbx_matrix ufbx_get_transform_matrix(const ufbx_transform *transform);

void ufbx_matrix_mul(ufbx_matrix *dst, const ufbx_matrix *l, const ufbx_matrix *r);
ufbx_vec3 ufbx_transform_position(const ufbx_matrix *m, ufbx_vec3 v);
ufbx_vec3 ufbx_transform_direction(const ufbx_matrix *m, ufbx_vec3 v);
ufbx_matrix ufbx_get_normal_matrix(const ufbx_matrix *m);
ufbx_matrix ufbx_get_inverse_matrix(const ufbx_matrix *m);

ufbx_real ufbx_evaluate_curve(const ufbx_anim_curve *curve, double time);

ufbx_transform ufbx_evaluate_transform(const ufbx_scene *scene, const ufbx_node *node, const ufbx_anim_stack *stack, double time);

ufbx_scene *ufbx_evaluate_scene(const ufbx_scene *scene, const ufbx_evaluate_opts *opts, double time);

ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain);

ufbx_vec3 ufbx_rotate_vector(ufbx_vec4 q, ufbx_vec3 v);

bool ufbx_triangulate(uint32_t *indices, size_t num_indices, ufbx_mesh *mesh, ufbx_face face);

// Utility

ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain);

// -- Inline API

ufbx_inline ufbx_node *ufbx_find_node(const ufbx_scene *scene, const char *name) {
	return ufbx_find_node_len(scene, name, strlen(name));
}

ufbx_inline ufbx_mesh *ufbx_find_mesh(const ufbx_scene *scene, const char *name) {
	return ufbx_find_mesh_len(scene, name, strlen(name));
}

ufbx_inline ufbx_material *ufbx_find_material(const ufbx_scene *scene, const char *name) {
	return ufbx_find_material_len(scene, name, strlen(name));
}

ufbx_inline ufbx_light *ufbx_find_light(const ufbx_scene *scene, const char *name) {
	return ufbx_find_light_len(scene, name, strlen(name));
}

ufbx_inline ufbx_anim_stack *ufbx_find_anim_stack(const ufbx_scene *scene, const char *name) {
	return ufbx_find_anim_stack_len(scene, name, strlen(name));
}

ufbx_inline ufbx_anim_layer *ufbx_find_anim_layer(const ufbx_scene *scene, const char *name) {
	return ufbx_find_anim_layer_len(scene, name, strlen(name));
}

ufbx_inline ufbx_prop *ufbx_find_prop(const ufbx_props *props, const char *name) {
	return ufbx_find_prop_len(props, name, strlen(name));
}

ufbx_inline ufbx_real ufbx_get_vertex_real(const ufbx_vertex_real *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec2 ufbx_get_vertex_vec2(const ufbx_vertex_vec2 *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec3 ufbx_get_vertex_vec3(const ufbx_vertex_vec3 *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec4 ufbx_get_vertex_vec4(const ufbx_vertex_vec4 *v, size_t index) { return v->data[v->indices[index]]; }

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

ufbx_inline ufbx_material *begin(const ufbx_material_list &l) { return l.data; }
ufbx_inline ufbx_material *end(const ufbx_material_list &l) { return l.data + l.size; }
ufbx_inline ufbx_material **begin(const ufbx_material_ptr_list &l) { return l.data; }
ufbx_inline ufbx_material **end(const ufbx_material_ptr_list &l) { return l.data + l.size; }
ufbx_inline ufbx_uv_set *begin(const ufbx_uv_set_list &l) { return l.data; }
ufbx_inline ufbx_uv_set *end(const ufbx_uv_set_list &l) { return l.data + l.size; }
ufbx_inline ufbx_color_set *begin(const ufbx_color_set_list &l) { return l.data; }
ufbx_inline ufbx_color_set *end(const ufbx_color_set_list &l) { return l.data + l.size; }
ufbx_inline ufbx_node **begin(const ufbx_node_ptr_list &l) { return l.data; }
ufbx_inline ufbx_node **end(const ufbx_node_ptr_list &l) { return l.data + l.size; }
ufbx_inline ufbx_model *begin(const ufbx_model_list &l) { return l.data; }
ufbx_inline ufbx_model *end(const ufbx_model_list &l) { return l.data + l.size; }
ufbx_inline ufbx_mesh *begin(const ufbx_mesh_list &l) { return l.data; }
ufbx_inline ufbx_mesh *end(const ufbx_mesh_list &l) { return l.data + l.size; }
ufbx_inline ufbx_light *begin(const ufbx_light_list &l) { return l.data; }
ufbx_inline ufbx_light *end(const ufbx_light_list &l) { return l.data + l.size; }
ufbx_inline ufbx_bone *begin(const ufbx_bone_list &l) { return l.data; }
ufbx_inline ufbx_bone *end(const ufbx_bone_list &l) { return l.data + l.size; }
ufbx_inline ufbx_anim_stack *begin(const ufbx_anim_stack_list &l) { return l.data; }
ufbx_inline ufbx_anim_stack *end(const ufbx_anim_stack_list &l) { return l.data + l.size; }
ufbx_inline ufbx_anim_layer *begin(const ufbx_anim_layer_list &l) { return l.data; }
ufbx_inline ufbx_anim_layer *end(const ufbx_anim_layer_list &l) { return l.data + l.size; }
ufbx_inline ufbx_anim_layer **begin(const ufbx_anim_layer_ptr_list &l) { return l.data; }
ufbx_inline ufbx_anim_layer **end(const ufbx_anim_layer_ptr_list &l) { return l.data + l.size; }
ufbx_inline ufbx_anim_prop *begin(const ufbx_anim_prop_list &l) { return l.data; }
ufbx_inline ufbx_anim_prop *end(const ufbx_anim_prop_list &l) { return l.data + l.size; }
ufbx_inline ufbx_anim_curve *begin(const ufbx_anim_curve_list &l) { return l.data; }
ufbx_inline ufbx_anim_curve *end(const ufbx_anim_curve_list &l) { return l.data + l.size; }
ufbx_inline ufbx_keyframe *begin(const ufbx_keyframe_list &l) { return l.data; }
ufbx_inline ufbx_keyframe *end(const ufbx_keyframe_list &l) { return l.data + l.size; }
ufbx_inline ufbx_skin *begin(const ufbx_skin_list& l) { return l.data; }
ufbx_inline ufbx_skin *end(const ufbx_skin_list& l) { return l.data + l.size; }

#endif

#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#endif
