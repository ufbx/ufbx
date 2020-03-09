#ifndef UFBX_UFBX_H_INLCUDED
#define UFBX_UFBX_H_INLCUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define UFBX_MAX_ELEMENTS_PER_MESH 256

#define UFBX_ERROR_DESC_MAX_LENGTH 255
#define UFBX_ERROR_STACK_MAX_DEPTH 8
#define UFBX_ERROR_STACK_NAME_MAX_LENGTH 31

#ifdef __cplusplus
extern "C" {
#endif

// Types

typedef float ufbx_real;
typedef struct ufbx_vec2 ufbx_vec2;
typedef struct ufbx_vec3 ufbx_vec3;
typedef struct ufbx_vec4 ufbx_vec4;

typedef struct ufbx_element ufbx_element;
typedef struct ufbx_mesh_layer ufbx_mesh_layer;
typedef struct ufbx_node ufbx_node;
typedef struct ufbx_model ufbx_model;
typedef struct ufbx_mesh ufbx_mesh;
typedef struct ufbx_template ufbx_template;
typedef struct ufbx_prop ufbx_prop;
typedef struct ufbx_edge ufbx_edge;
typedef struct ufbx_face ufbx_face;
typedef struct ufbx_uv_set ufbx_uv_set;
typedef struct ufbx_color_set ufbx_color_set;

typedef struct ufbx_node_list { ufbx_node **data; size_t size; } ufbx_node_list;
typedef struct ufbx_model_list { ufbx_model **data; size_t size; } ufbx_model_list;
typedef struct ufbx_mesh_list { ufbx_mesh **data; size_t size; } ufbx_mesh_list;
typedef struct ufbx_template_list { ufbx_template *data; size_t size; } ufbx_template_list;
typedef struct ufbx_prop_list { ufbx_prop *data; size_t size; } ufbx_prop_list;
typedef struct ufbx_uv_set_list { ufbx_uv_set *data; size_t size; } ufbx_uv_set_list;
typedef struct ufbx_color_set_list { ufbx_color_set *data; size_t size; } ufbx_color_set_list;

typedef struct ufbx_error {
	uint32_t source_line;
	uint32_t stack_size;
	char desc[UFBX_ERROR_DESC_MAX_LENGTH + 1];
	char stack[UFBX_ERROR_STACK_MAX_DEPTH][UFBX_ERROR_DESC_MAX_LENGTH + 1];
} ufbx_error;

typedef enum ufbx_property_type {
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


struct ufbx_prop {
	ufbx_string name;
	ufbx_prop_type type;

	ufbx_string type_str;
	ufbx_string subtype_str;
	ufbx_string flags;

	ufbx_string value_str;
	int64_t value_int[4];
	union {
		ufbx_real value_real[4];
		ufbx_vec2 value_vec2;
		ufbx_vec3 value_vec3;
		ufbx_vec4 value_vec4;
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
};

typedef struct ufbx_vertex_element {
	void *data;
	int32_t *indices;
} ufbx_vertex_element;

typedef union ufbx_vertex_real {
	ufbx_vertex_element element;
	struct {
		ufbx_real *data;
		int32_t *indices;
	};
} ufbx_vertex_real;

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

typedef struct ufbx_uv_set {
	ufbx_string name;
	ufbx_vertex_vec2 vertex_uv;
} ufbx_uv_set;

typedef struct ufbx_color_set {
	ufbx_string name;
	ufbx_vertex_vec4 vertex_color;
} ufbx_color_set;

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

typedef struct ufbx_load_opts {
	bool allow_nonexistent_indices;
} ufbx_load_opts;

ufbx_scene *ufbx_load_memory(const void *data, size_t size, const ufbx_load_opts *opts, ufbx_error *error);
void ufbx_free_scene(ufbx_scene *scene);

ufbx_node *ufbx_find_node(ufbx_scene *scene, const char *name, ufbx_node_type type);
ufbx_node *ufbx_find_node_str(ufbx_scene *scene, ufbx_string name, ufbx_node_type type);
ufbx_node *ufbx_find_node_any(ufbx_scene *scene, const char *name);
ufbx_node *ufbx_find_node_any_str(ufbx_scene *scene, ufbx_string name);
ufbx_model *ufbx_find_model(ufbx_scene *scene, const char *name);
ufbx_model *ufbx_find_model_str(ufbx_scene *scene, ufbx_string name);
ufbx_mesh *ufbx_find_mesh(ufbx_scene *scene, const char *name);
ufbx_mesh *ufbx_find_mesh_str(ufbx_scene *scene, ufbx_string name);

ufbx_prop *ufbx_get_prop_from_list(const ufbx_prop_list *list, const char *name);
ufbx_prop *ufbx_get_prop_from_list_str(const ufbx_prop_list *list, ufbx_string name);
ufbx_prop *ufbx_get_prop(const ufbx_node *node, const char *name);
ufbx_prop *ufbx_get_prop_str(const ufbx_node *node, ufbx_string name);

const char *ufbx_prop_type_name(ufbx_prop_type type);
const char *ufbx_node_type_name(ufbx_node_type type);

#ifdef __cplusplus
}
#endif

// Range overloads for lists

#ifdef __cplusplus

static inline ufbx_node **begin(const ufbx_node_list &l) { return l.data; }
static inline ufbx_node **end(const ufbx_node_list &l) { return l.data + l.size; }
static inline ufbx_model **begin(const ufbx_model_list &l) { return l.data; }
static inline ufbx_model **end(const ufbx_model_list &l) { return l.data + l.size; }
static inline ufbx_mesh **begin(const ufbx_mesh_list &l) { return l.data; }
static inline ufbx_mesh **end(const ufbx_mesh_list &l) { return l.data + l.size; }
static inline ufbx_template *begin(const ufbx_template_list &l) { return l.data; }
static inline ufbx_template *end(const ufbx_template_list &l) { return l.data + l.size; }
static inline ufbx_prop *begin(const ufbx_prop_list &l) { return l.data; }
static inline ufbx_prop *end(const ufbx_prop_list &l) { return l.data + l.size; }

#endif

#endif
