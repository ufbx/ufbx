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
	#define ufbx_inline static __attribute__((always_inline, unused))
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
} ufbx_rotation_order;

typedef struct ufbx_transform {
	ufbx_vec3 translation;
	ufbx_rotation_order rotation_order;
	ufbx_vec3 rotation_euler;
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
		ufbx_real value_real[3];
		ufbx_vec2 value_vec2;
		ufbx_vec3 value_vec3;
	};
};

struct ufbx_props {
	ufbx_prop *defaults;

	ufbx_prop *props;
	size_t num_props;
};

// -- Static data

typedef struct ufbx_geometry ufbx_geometry;
typedef struct ufbx_material ufbx_material;

typedef union ufbx_vertex_vec2 ufbx_vertex_vec2;
typedef union ufbx_vertex_vec3 ufbx_vertex_vec3;
typedef union ufbx_vertex_vec4 ufbx_vertex_vec4;
typedef struct ufbx_uv_set ufbx_uv_set;
typedef struct ufbx_color_set ufbx_color_set;
typedef struct ufbx_edge ufbx_edge;
typedef struct ufbx_face ufbx_face;

typedef struct ufbx_material_list { ufbx_material **data; size_t size; } ufbx_material_list;
typedef struct ufbx_uv_set_list { ufbx_uv_set *data; size_t size; } ufbx_uv_set_list;
typedef struct ufbx_color_set_list { ufbx_color_set *data; size_t size; } ufbx_color_set_list;

union ufbx_vertex_vec2 {
	struct {
		ufbx_vec2 *data;
		int32_t *indices;
	};
};

union ufbx_vertex_vec3 {
	struct {
		ufbx_vec3 *data;
		int32_t *indices;
	};
};

union ufbx_vertex_vec4 {
	struct {
		ufbx_vec4 *data;
		int32_t *indices;
	};
};

struct ufbx_uv_set {
	ufbx_string name;
	ufbx_vertex_vec2 vertex_uv;
};

struct ufbx_color_set {
	ufbx_string name;
	ufbx_vertex_vec4 vertex_color;
};

struct ufbx_edge {
	int32_t indices[2];
};

struct ufbx_face {
	uint32_t index_begin;
	uint32_t num_indices;
};

struct ufbx_geometry {
	ufbx_props props;

	size_t num_vertices;
	size_t num_indices;
	size_t num_faces;
	size_t num_edges;

	ufbx_face *faces;
	ufbx_edge *edges;

	ufbx_vertex_vec3 vertex_position;
	ufbx_vertex_vec3 vertex_normal;
	ufbx_vertex_vec3 vertex_binormal;
	ufbx_vertex_vec3 vertex_tangent;
	ufbx_vertex_vec4 vertex_color;
	ufbx_vertex_vec2 vertex_uv;

	bool *edge_smoothing;
	ufbx_real *edge_crease;

	bool *face_smoothing;
	int32_t *face_material;

	ufbx_uv_set_list uv_sets;
	ufbx_color_set_list color_sets;
};

struct ufbx_material {
	ufbx_props props;

	ufbx_vec3 color;
};

// -- Scene graph

typedef struct ufbx_node ufbx_node;
typedef struct ufbx_model ufbx_model;
typedef struct ufbx_mesh ufbx_mesh;
typedef struct ufbx_light ufbx_light;

typedef struct ufbx_node_list { ufbx_node **data; size_t size; } ufbx_node_list;
typedef struct ufbx_model_list { ufbx_model *data; size_t size; } ufbx_model_list;
typedef struct ufbx_mesh_list { ufbx_mesh *data; size_t size; } ufbx_mesh_list;
typedef struct ufbx_light_list { ufbx_light *data; size_t size; } ufbx_light_list;

typedef enum ufbx_node_type {
	UFBX_NODE_UNKNOWN,
	UFBX_NODE_MODEL,
	UFBX_NODE_MESH,
	UFBX_NODE_LIGHT,
} ufbx_node_type;

struct ufbx_node {
	ufbx_node_type type;
	ufbx_string name;
	ufbx_props props;
	ufbx_node *parent;
	ufbx_transform transform;
	ufbx_node_list children;
};

struct ufbx_model {
	ufbx_node node;
};

struct ufbx_mesh {
	ufbx_node node;

	ufbx_geometry *geometry;
	ufbx_material_list materials;
};

struct ufbx_light {
	ufbx_node node;

	ufbx_vec3 color;
	ufbx_real intensity;
};

// -- Animations

typedef struct ufbx_anim_layer ufbx_anim_layer;
typedef struct ufbx_anim_prop ufbx_anim_prop;
typedef struct ufbx_anim_curve ufbx_anim_curve;
typedef struct ufbx_keyframe ufbx_keyframe;
typedef struct ufbx_tangent ufbx_tangent;
typedef struct ufbx_anim_state ufbx_anim_state;

typedef struct ufbx_anim_layer_list { ufbx_anim_layer *data; size_t size; } ufbx_anim_layer_list;
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

struct ufbx_anim_layer {
	ufbx_string name;
	ufbx_anim_prop_list props;
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
	ufbx_node *node;
	ufbx_anim_curve curves[3];
};

struct ufbx_keyframe {
	double time;
	ufbx_real value;
	ufbx_interpolation interpolation;
	ufbx_tangent left;
	ufbx_tangent right;
};

struct ufbx_anim_state {
	const ufbx_anim_layer *layers;
	size_t num_layers;
	ufbx_real time;
};

// -- Scene

typedef struct ufbx_scene ufbx_scene;

typedef struct ufbx_metadata {
	bool ascii;
	uint32_t version;
	ufbx_string creator;
} ufbx_metadata;

struct ufbx_scene {
	ufbx_metadata metadata;

	ufbx_model *root;

	ufbx_node_list nodes;
	ufbx_model_list models;
	ufbx_mesh_list meshes;
	ufbx_light_list lights;
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

	// Limits
	size_t max_temp_memory;
	size_t max_result_memory;
	size_t max_ascii_token_length;
	size_t read_buffer_size;
	size_t max_properties;

	uint32_t max_string_length;
	uint32_t max_strings;
	uint32_t max_node_depth;
	uint32_t max_node_values;
	uint32_t max_node_children;
	uint32_t max_array_size;

	bool allow_nonexistent_indices;
} ufbx_load_opts;

typedef struct ufbx_evaluate_opts {
	ufbx_scene *reuse_scene;
	ufbx_allocator allocator;
	size_t max_memory;
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

ufbx_scene *ufbx_load_memory(const void *data, size_t size, const ufbx_load_opts *opts, ufbx_error *error);
ufbx_scene *ufbx_load_file(const char *filename, const ufbx_load_opts *opts, ufbx_error *error);
void ufbx_free_scene(ufbx_scene *scene);

ufbx_mesh *ufbx_find_mesh_len(const ufbx_scene *scene, const char *name, size_t name_len);

ufbx_prop *ufbx_find_prop_len(const ufbx_props *props, const char *name, size_t name_len);

ufbx_vec4 ufbx_get_rotation_quaternion(ufbx_rotation_order order, ufbx_vec3 euler);
ufbx_matrix ufbx_get_transform_matrix(const ufbx_transform *transform);

ufbx_real ufbx_evaluate_curve(const ufbx_anim_curve *curve, double time);
ufbx_real ufbx_evaluate_prop_real_len(const ufbx_anim_state *state, const ufbx_node *node, const char *prop, size_t prop_len);
ufbx_vec3 ufbx_evaluate_prop_vec3_len(const ufbx_anim_state *state, const ufbx_node *node, const char *prop, size_t prop_len);

ufbx_model ufbx_evaluate_model(const ufbx_anim_state *state, const ufbx_model *model);
ufbx_light ufbx_evaluate_light(const ufbx_anim_state *state, const ufbx_light *light);
ufbx_scene *ufbx_evaluate_scene(const ufbx_anim_state *state, const ufbx_scene *scene, const ufbx_evaluate_opts *opts);

ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain);

// Utility

ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain);

// -- Inline API

ufbx_inline ufbx_mesh *ufbx_find_mesh(const ufbx_scene *scene, const char *name) {
	return ufbx_find_mesh_len(scene, name, strlen(name));
}

ufbx_inline ufbx_prop *ufbx_find_prop(const ufbx_props *props, const char *name) {
	return ufbx_find_prop_len(props, name, strlen(name));
}

ufbx_inline ufbx_real ufbx_evaluate_prop_real(const ufbx_anim_state *state, const ufbx_node *node, const char *prop) {
	return ufbx_evaluate_prop_real_len(state, node, prop, strlen(prop));
}

ufbx_inline ufbx_vec3 ufbx_evaluate_prop_vec3(const ufbx_anim_state *state, const ufbx_node *node, const char *prop) {
	return ufbx_evaluate_prop_vec3_len(state, node, prop, strlen(prop));
}

ufbx_inline ufbx_vec4 ufbx_get_transform_quaternion(const ufbx_transform *transform) {
	return ufbx_get_rotation_quaternion(transform->rotation_order, transform->rotation_euler);
}

#ifdef __cplusplus
}
#endif

#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#endif
