#ifndef UFBX_UFBX_H_INLCUDED
#define UFBX_UFBX_H_INLCUDED

#include <stdint.h>
#include <stddef.h>

#define ufbx_define_list_type(name, type) \
	typedef struct name { type* data; size_t size; } name; \
	static inline type *begin(const type &l) { return l.data; } \
	static inline type *end(const type &l) { return l.data + l.size; } \

#define UFBX_ERROR_DESC_MAX_LENGTH 255
#define UFBX_ERROR_STACK_MAX_DEPTH 8
#define UFBX_ERROR_STACK_NAME_MAX_LENGTH 31

// Types

typedef struct ufbx_node ufbx_node;
typedef struct ufbx_model ufbx_model;
typedef struct ufbx_mesh ufbx_mesh;
typedef struct ufbx_template ufbx_template;
typedef struct ufbx_prop ufbx_prop;

typedef struct ufbx_node_list { ufbx_node **data; size_t size; } ufbx_node_list;
typedef struct ufbx_model_list { ufbx_model **data; size_t size; } ufbx_model_list;
typedef struct ufbx_mesh_list { ufbx_mesh **data; size_t size; } ufbx_mesh_list;
typedef struct ufbx_template_list { ufbx_template *data; size_t size; } ufbx_template_list;
typedef struct ufbx_prop_list { ufbx_prop *data; size_t size; } ufbx_prop_list;

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

struct ufbx_prop {
	ufbx_string name;
	ufbx_prop_type type;

	ufbx_string type_str;
	ufbx_string subtype_str;
	ufbx_string flags;

	ufbx_string value_str;
	int64_t value_int[4];
	double value_float[4];
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

	ufbx_node *parents;
	size_t num_parents;

	ufbx_node *children;
	size_t num_children;
};

struct ufbx_model {
	ufbx_node node;
};

struct ufbx_mesh {
	ufbx_node node;
};

typedef struct ufbx_metadata {
	int ascii;
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

#ifdef __cplusplus
extern "C" {
#endif

ufbx_scene *ufbx_load_memory(const void *data, size_t size, ufbx_error *error);
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

static inline void begin(const ufbx_node_list &l) { return l.data; }
static inline void end(const ufbx_node_list &l) { return l.data + l.size; }
static inline void begin(const ufbx_model_list &l) { return l.data; }
static inline void end(const ufbx_model_list &l) { return l.data + l.size; }
static inline void begin(const ufbx_mesh_list &l) { return l.data; }
static inline void end(const ufbx_mesh_list &l) { return l.data + l.size; }
static inline void begin(const ufbx_template_list &l) { return l.data; }
static inline void end(const ufbx_template_list &l) { return l.data + l.size; }
static inline void begin(const ufbx_prop_list &l) { return l.data; }
static inline void end(const ufbx_prop_list &l) { return l.data + l.size; }

#endif

#endif
