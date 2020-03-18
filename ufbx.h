#ifndef UFBX_UFBX_H_INLCUDED
#define UFBX_UFBX_H_INLCUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

#define UFBX_MAX_ELEMENTS_PER_MESH 256

#define UFBX_ERROR_STACK_MAX_DEPTH 8

#ifdef __cplusplus
extern "C" {
#endif

// Types

typedef double ufbx_real;
typedef struct ufbx_vec2 ufbx_vec2;
typedef struct ufbx_vec3 ufbx_vec3;
typedef struct ufbx_vec4 ufbx_vec4;
typedef struct ufbx_transform ufbx_transform;
typedef struct ufbx_edge ufbx_edge;
typedef struct ufbx_face ufbx_face;

typedef struct ufbx_node ufbx_node;
typedef struct ufbx_model ufbx_model;
typedef struct ufbx_mesh ufbx_mesh;
typedef struct ufbx_template ufbx_template;
typedef struct ufbx_prop ufbx_prop;
typedef struct ufbx_uv_set ufbx_uv_set;
typedef struct ufbx_color_set ufbx_color_set;

typedef struct ufbx_node_list { ufbx_node **data; size_t size; } ufbx_node_list;
typedef struct ufbx_model_list { ufbx_model **data; size_t size; } ufbx_model_list;
typedef struct ufbx_mesh_list { ufbx_mesh **data; size_t size; } ufbx_mesh_list;
typedef struct ufbx_template_list { ufbx_template *data; size_t size; } ufbx_template_list;
typedef struct ufbx_prop_list { ufbx_prop *data; size_t size; } ufbx_prop_list;
typedef struct ufbx_uv_set_list { ufbx_uv_set *data; size_t size; } ufbx_uv_set_list;
typedef struct ufbx_color_set_list { ufbx_color_set *data; size_t size; } ufbx_color_set_list;

typedef struct ufbx_error_frame {
	uint32_t source_line;
	const char *function;
	const char *description;
} ufbx_error_frame;

typedef struct ufbx_error {
	uint32_t stack_size;
	ufbx_error_frame stack[UFBX_ERROR_STACK_MAX_DEPTH];
} ufbx_error;

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

typedef struct ufbx_string {
	const char *data;
	size_t length;
} ufbx_string;
extern const ufbx_string ufbx_empty_string;

struct ufbx_vec2 {
	union {
		struct { ufbx_real x, y; };
		ufbx_real v[2];
	};
};

struct ufbx_vec3 {
	union {
		struct { ufbx_real x, y, z; };
		ufbx_real v[3];
	};
};

struct ufbx_vec4 {
	union {
		struct { ufbx_real x, y, z, w; };
		ufbx_real v[4];
	};
};

struct ufbx_transform {
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
};

struct ufbx_prop {
	ufbx_string name;
	uint32_t imp_key;
	ufbx_prop_type type;

	ufbx_string type_str;
	ufbx_string subtype_str;
	ufbx_string flags;

	ufbx_string value_str;
	int64_t value_int;
	union {
		ufbx_real value_real[3];
		ufbx_vec2 value_vec2;
		ufbx_vec3 value_vec3;
	};
};

typedef enum ufbx_node_type {
	UFBX_NODE_UNKNOWN,
	UFBX_NODE_MODEL,
	UFBX_NODE_MESH,
	UFBX_NODE_MATERIAL,
	UFBX_NODE_TEXTURE,
	UFBX_NODE_BONE,
	UFBX_NODE_SKIN,
	UFBX_NODE_ANIMATION,
	UFBX_NODE_ANIMATION_CURVE,
	UFBX_NODE_ANIMATION_LAYER,
	UFBX_NODE_ATTRIBUTE,

	UFBX_NUM_NODE_TYPES,
} ufbx_node_type;

struct ufbx_template {
	ufbx_string name;
	ufbx_string type_str;
	ufbx_prop_list props;
};

struct ufbx_node {
	ufbx_node_type type;

	uint64_t id;
	ufbx_string name;
	ufbx_string type_str;
	ufbx_string sub_type_str;

	ufbx_template *prop_template;
	ufbx_prop_list props;

	ufbx_model *parent_model;

	ufbx_node_list parents;
	ufbx_node_list children;
};

struct ufbx_model {
	ufbx_node node;

	ufbx_transform self_to_root;
	ufbx_transform self_to_parent;
	ufbx_transform geometry_to_self;

	ufbx_model_list models;
	ufbx_mesh_list meshes;
};

typedef struct ufbx_vertex_element {
	void *data;
	int32_t *indices;
} ufbx_vertex_element;

typedef union ufbx_vertex_vec2 {
	ufbx_vertex_element element;
	struct {
		ufbx_vec2 *data;
		int32_t *indices;
	};
} ufbx_vertex_vec2;

typedef union ufbx_vertex_vec3 {
	ufbx_vertex_element element;
	struct {
		ufbx_vec3 *data;
		int32_t *indices;
	};
} ufbx_vertex_vec3;

typedef union ufbx_vertex_vec4 {
	ufbx_vertex_element element;
	struct {
		ufbx_vec4 *data;
		int32_t *indices;
	};
} ufbx_vertex_vec4;

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

struct ufbx_mesh {
	ufbx_node node;

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

typedef struct ufbx_metadata {
	bool ascii;
	uint32_t version;
	ufbx_string creator;
} ufbx_metadata;

typedef struct ufbx_scene {
	ufbx_metadata metadata;

	ufbx_model *root_model;

	ufbx_node_list nodes;
	ufbx_model_list models;
	ufbx_mesh_list meshes;
	ufbx_template_list templates;
} ufbx_scene;

typedef void *ufbx_alloc_fn(void *user, size_t size);
typedef void *ufbx_realloc_fn(void *user, void *old_ptr, size_t old_size, size_t new_size);
typedef void ufbx_free_fn(void *user, void *ptr, size_t size);

typedef struct ufbx_allocator {
	ufbx_alloc_fn *alloc_fn;
	ufbx_realloc_fn *realloc_fn;
	ufbx_free_fn *free_fn;
	void *user;
} ufbx_allocator;

typedef struct ufbx_load_opts {
	ufbx_allocator temp_allocator;
	ufbx_allocator result_allocator;

	size_t max_temp_memory;
	size_t max_result_memory;

	bool allow_nonexistent_indices;
} ufbx_load_opts;

typedef size_t ufbx_read_fn(void *user, void *data, size_t size);

ufbx_scene *ufbx_load_memory(const void *data, size_t size, const ufbx_load_opts *opts, ufbx_error *error);
ufbx_scene *ufbx_load_file(const char *filename, const ufbx_load_opts *opts, ufbx_error *error);
void ufbx_free_scene(ufbx_scene *scene);

ufbx_prop *ufbx_get_prop_from_list(const ufbx_prop_list *list, const char *name);
ufbx_prop *ufbx_get_prop_from_list_len(const ufbx_prop_list *list, const char *name, size_t len);
ufbx_prop *ufbx_get_prop(const ufbx_node *node, const char *name);
ufbx_prop *ufbx_get_prop_len(const ufbx_node *node, const char *name, size_t len);

void ufbx_transform_mul(ufbx_transform *dst, const ufbx_transform *l, const ufbx_transform *r);
ufbx_vec3 ufbx_transform_position(const ufbx_transform *t, ufbx_vec3 v);
ufbx_vec3 ufbx_transform_direction(const ufbx_transform *t, ufbx_vec3 v);
ufbx_vec3 ufbx_transform_normal(const ufbx_transform *t, ufbx_vec3 v);

ufbx_inline ufbx_vec2 ufbx_get_vertex_vec2(const ufbx_vertex_vec2 *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec3 ufbx_get_vertex_vec3(const ufbx_vertex_vec3 *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec4 ufbx_get_vertex_vec4(const ufbx_vertex_vec4 *v, size_t index) { return v->data[v->indices[index]]; }

// Utility

typedef struct ufbx_inflate_input {
	size_t total_size;

	const void *data;
	size_t data_size;

	void *buffer;
	size_t buffer_size;

	ufbx_read_fn *read_fn;
	void *read_user;
} ufbx_inflate_input;

typedef struct ufbx_inflate_retain {
	bool initialized;
	uint64_t data[512];
} ufbx_inflate_retain;

ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain);

#ifdef __cplusplus
}
#endif

// Range overloads for lists

#ifdef __cplusplus

ufbx_inline ufbx_node **begin(const ufbx_node_list &l) { return l.data; }
ufbx_inline ufbx_node **end(const ufbx_node_list &l) { return l.data + l.size; }
ufbx_inline ufbx_model **begin(const ufbx_model_list &l) { return l.data; }
ufbx_inline ufbx_model **end(const ufbx_model_list &l) { return l.data + l.size; }
ufbx_inline ufbx_mesh **begin(const ufbx_mesh_list &l) { return l.data; }
ufbx_inline ufbx_mesh **end(const ufbx_mesh_list &l) { return l.data + l.size; }
ufbx_inline ufbx_template *begin(const ufbx_template_list &l) { return l.data; }
ufbx_inline ufbx_template *end(const ufbx_template_list &l) { return l.data + l.size; }
ufbx_inline ufbx_prop *begin(const ufbx_prop_list &l) { return l.data; }
ufbx_inline ufbx_prop *end(const ufbx_prop_list &l) { return l.data + l.size; }

#endif

#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#endif
