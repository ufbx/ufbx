#ifndef UFBX_UFBX_H_INLCUDED
#define UFBX_UFBX_H_INLCUDED

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFBX_ERROR_DESC_MAX_LENGTH 255
#define UFBX_ERROR_STACK_MAX_DEPTH 8
#define UFBX_ERROR_STACK_NAME_MAX_LENGTH 31

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
} ufbx_property_type;

typedef struct ufbx_string {
	const char *data;
	size_t length;
} ufbx_string;

typedef struct ufbx_property {
	ufbx_string name;
	ufbx_property_type type;

	ufbx_string type_str;
	ufbx_string subtype_str;
	ufbx_string flags;

	ufbx_string value_str;
	int64_t value_int[4];
	double value_float[4];
} ufbx_property;

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
} ufbx_node_type;

typedef struct ufbx_node ufbx_node;
typedef struct ufbx_model_node ufbx_model_node;
typedef struct ufbx_mesh_node ufbx_mesh_node;

struct ufbx_node {
	ufbx_string name;
	ufbx_string type_str;

	ufbx_property *properties;
	size_t num_properties;

	ufbx_model_node *parent_model;

	ufbx_node *parents;
	size_t num_parents;

	ufbx_node *children;
	size_t num_children;
};

typedef struct ufbx_metadata {
	int ascii;
	uint32_t version;
	ufbx_string creator;
} ufbx_metadata;

typedef struct ufbx_scene {
	ufbx_metadata metadata;

	ufbx_node *templates;
	size_t num_templates;

	ufbx_model_node *root_model;

	ufbx_model_node *models;
	size_t num_models;

	ufbx_mesh_node *meshes;
	size_t num_meshes;
} ufbx_scene;

ufbx_scene *ufbx_load_memory(const void *data, size_t size, ufbx_error *error);
void ufbx_free_scene(ufbx_scene *scene);

#ifdef __cplusplus
}
#endif

#endif
