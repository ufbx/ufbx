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

#ifndef ufbx_assert
	#include <assert.h>
	#define ufbx_assert(cond) assert(cond)
#endif

// -- Configuration

typedef double ufbx_real;

#define UFBX_ERROR_STACK_MAX_DEPTH 8

// -- Language

#if defined(__cplusplus)
	#define UFBX_LIST_TYPE(p_name, p_type) struct p_name { p_type *data; size_t count; \
		p_type &operator[](size_t index) const { ufbx_assert(index < count); return data[index]; } \
		p_type *begin() const { return data; } \
		p_type *end() const { return data + count; } }
	#define UFBX_LIST_TYPE_DECL(p_name, p_type) struct p_name { p_type *data; size_t count; \
		p_type &operator[](size_t index) const; \
		p_type *begin() const; \
		p_type *end() const; }
	#define UFBX_LIST_TYPE_DEF(p_name, p_type) \
		inline p_type &p_name::operator[](size_t index) const { ufbx_assert(index < count); return data[index]; } \
		inline p_type *p_name::begin() const { return data; } \
		inline p_type *p_name::end() const { return data + count; }
#else
	#define UFBX_LIST_TYPE(p_name, p_type) typedef struct p_name { p_type *data; size_t count; } p_name
	#define UFBX_LIST_TYPE_DECL(p_name, p_type) typedef struct p_name { p_type *data; size_t count; } p_name
	#define UFBX_LIST_TYPE_DEF(p_name, p_type)
#endif

// -- Version

#define UFBX_HEADER_VERSION 1001001 // v1.1.1

// -- Basic types

// Null-terminated string within an FBX file
typedef struct ufbx_string {
	const char *data;
	size_t length;
} ufbx_string;

// 2D vector
typedef struct ufbx_vec2 {
	union {
		struct { ufbx_real x, y; };
		ufbx_real v[2];
	};
} ufbx_vec2;

// 3D vector
typedef struct ufbx_vec3 {
	union {
		struct { ufbx_real x, y, z; };
		ufbx_real v[3];
	};
} ufbx_vec3;

// 4D vector
typedef struct ufbx_vec4 {
	union {
		struct { ufbx_real x, y, z, w; };
		ufbx_real v[4];
	};
} ufbx_vec4;

// Quaternion
typedef struct ufbx_quat {
	union {
		struct { ufbx_real x, y, z, w; };
		ufbx_real v[4];
	};
} ufbx_quat;

// Order in which Euler-angle rotation axes are applied for a transform
// NOTE: The order in the name refers to the order of axes *applied*,
// not the multiplication order: eg. `UFBX_ROTATION_XYZ` is `Z*Y*X`
// [TODO: Figure out what the spheric rotation order is...]
typedef enum ufbx_rotation_order {
	UFBX_ROTATION_XYZ,
	UFBX_ROTATION_XZY,
	UFBX_ROTATION_YZX,
	UFBX_ROTATION_YXZ,
	UFBX_ROTATION_ZXY,
	UFBX_ROTATION_ZYX,
	UFBX_ROTATION_SPHERIC,
} ufbx_rotation_order;

// Explicit translation+rotation+scale transformation.
// NOTE: Rotation is a quaternion, not Euler angles!
typedef struct ufbx_transform {
	ufbx_vec3 translation;
	ufbx_quat rotation;
	ufbx_vec3 scale;
} ufbx_transform;

// 4x3 matrix encoding an affine transformation.
// `cols[0..2]` are the X/Y/Z basis vectors, `cols[3]` is the translation
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

// FBX elements have properties which are arbitrary key/value pairs that can
// have inherited default values or be animated. In most cases you don't need
// to access these unless you need a feature not implemented directly in ufbx.
// NOTE: Prefer using `ufbx_find_prop[_len](...)` to search for a property by
// name as it can find it from the defaults if necessary.

typedef struct ufbx_prop ufbx_prop;
typedef struct ufbx_props ufbx_props;
typedef struct ufbx_element ufbx_element;

// Data type contained within the property. All the data fields are always
// populated regardless of type, so there's no need to switch by type usually
// eg. `prop->value_real` and `prop->value_int` have the same value (well, close)
// if `prop->type == UFBX_PROP_INTEGER`. String values are not converted from/to.
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
	UFBX_PROP_COMPOUND,

	UFBX_NUM_PROP_TYPES,
} ufbx_prop_type;

typedef enum ufbx_prop_flags {
	UFBX_PROP_FLAG_ANIMATABLE = 0x1,
	UFBX_PROP_FLAG_USER_DEFINED = 0x2,
	UFBX_PROP_FLAG_HIDDEN = 0x4,
	UFBX_PROP_FLAG_LOCK_X = 0x10,
	UFBX_PROP_FLAG_LOCK_Y = 0x20,
	UFBX_PROP_FLAG_LOCK_Z = 0x40,
	UFBX_PROP_FLAG_LOCK_W = 0x80,
	UFBX_PROP_FLAG_MUTE_X = 0x100,
	UFBX_PROP_FLAG_MUTE_Y = 0x200,
	UFBX_PROP_FLAG_MUTE_Z = 0x400,
	UFBX_PROP_FLAG_MUTE_W = 0x800,
	UFBX_PROP_FLAG_SYNTHETIC = 0x1000,
	UFBX_PROP_FLAG_ANIMATED = 0x2000,
	UFBX_PROP_FLAG_NOT_FOUND = 0x4000,
} ufbx_prop_flags;

// Single property with name/type/value.
struct ufbx_prop {
	ufbx_string name;
	uint32_t internal_key;
	ufbx_prop_type type;
	ufbx_prop_flags flags;

	ufbx_string value_str;
	int64_t value_int;
	union {
		ufbx_real value_real_arr[3];
		ufbx_real value_real;
		ufbx_vec2 value_vec2;
		ufbx_vec3 value_vec3;
	};
};

// List of alphabetically sorted properties with potential defaults.
// For animated objects in as scene from `ufbx_evaluate_scene()` this list
// only has the animated properties, the originals are stored under `defaults`.
struct ufbx_props {
	ufbx_prop *props;
	size_t num_props;
	size_t num_animated;

	ufbx_props *defaults;
};

// -- Elements

// Element is the lowest level representation of the FBX file in ufbx.
// An element contains type, id, name, and properties (see `ufbx_props` above)
// Elements may be connected to each other aribtrarily via `ufbx_connection`

typedef struct ufbx_unknown_element ufbx_unknown_element;
typedef struct ufbx_node ufbx_node;
typedef struct ufbx_model ufbx_model;
typedef struct ufbx_mesh ufbx_mesh;
typedef struct ufbx_light ufbx_light;
typedef struct ufbx_camera ufbx_camera;
typedef struct ufbx_bone ufbx_bone;
typedef struct ufbx_blend_shape ufbx_blend_shape;
typedef struct ufbx_material ufbx_material;
typedef struct ufbx_anim_stack ufbx_anim_stack;
typedef struct ufbx_anim_layer ufbx_anim_layer;
typedef struct ufbx_anim_prop ufbx_anim_prop;
typedef struct ufbx_anim_curve ufbx_anim_curve;

UFBX_LIST_TYPE(ufbx_node_ptr_list, ufbx_node*);
UFBX_LIST_TYPE(ufbx_material_ptr_list, ufbx_material*);
UFBX_LIST_TYPE(ufbx_element_ptr_list, ufbx_element*);
UFBX_LIST_TYPE(ufbx_anim_layer_ptr_list, ufbx_anim_layer*);
UFBX_LIST_TYPE(ufbx_anim_prop_ptr_list, ufbx_anim_prop*);
UFBX_LIST_TYPE(ufbx_anim_curve_ptr_list, ufbx_anim_curve*);

typedef union ufbx_any_node ufbx_any_node;

typedef enum ufbx_element_type {
	UFBX_ELEMENT_UNKNOWN,     // < `ufbx_unknown_element`
	UFBX_ELEMENT_NODE,        // < `ufbx_node`
	UFBX_ELEMENT_MESH,        // < `ufbx_mesh`
	UFBX_ELEMENT_LIGHT,       // < `ufbx_light`
	UFBX_ELEMENT_CAMERA,      // < `ufbx_camera`
	UFBX_ELEMENT_BONE,        // < `ufbx_bone`
	UFBX_ELEMENT_BLEND_SHAPE, // < `ufbx_blend_shape`
	UFBX_ELEMENT_MATERIAL,    // < `ufbx_material`
	UFBX_ELEMENT_ANIM_STACK,  // < `ufbx_anim_stack`
	UFBX_ELEMENT_ANIM_LAYER,  // < `ufbx_anim_layer`
	UFBX_ELEMENT_ANIM_PROP,   // < `ufbx_anim_prop`
	UFBX_ELEMENT_ANIM_CURVE,  // < `ufbx_anim_curve`

	UFBX_NUM_ELEMENT_TYPES,
} ufbx_element_type;

// Connection between two elements source and destination are somewhat
// arbitrary but the destination is often the "container" eg. parent node
// and source is the object conneted to it eg. child node.
typedef struct ufbx_connection {
	ufbx_element *src;
	ufbx_element *dst;
	ufbx_string src_prop;
	ufbx_string dst_prop;
} ufbx_connection;

UFBX_LIST_TYPE(ufbx_connection_list, ufbx_connection);

// Element "base class" found in the head of every element object
// NOTE: The `element_id` value is consistent when loading the
// _same_ file, but re-exporting the file will invalidate them.
struct ufbx_element {
	ufbx_string name;
	ufbx_props props;
	ufbx_element_type type;
	uint32_t id;
	ufbx_connection_list connections_src;
	ufbx_connection_list connections_dst;
};

struct ufbx_unknown_element {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_string type;
	ufbx_string sub_type;
};

UFBX_LIST_TYPE(ufbx_unknown_element_list, ufbx_unknown_element);

// -- Nodes

// Inherit type specifies how hierarchial node transforms are combined.
// UFBX_INHERIT_NORMAL is combined using the "proper" multiplication
// UFBX_INHERIT_NO_SHEAR does component-wise { pos+pos, rot*rot, scale*scale }
// UFBX_INHERIT_NO_SCALE ignores the parent scale { pos+pos, rot*rot, scale }
typedef enum ufbx_inherit_type {
	UFBX_INHERIT_NO_SHEAR, // R*r*S*s
	UFBX_INHERIT_NORMAL,   // R*S*r*s
	UFBX_INHERIT_NO_SCALE, // R*r*s
} ufbx_inherit_type;

// Specialized type of a node, subset of possible element types
typedef enum ufbx_node_type {
	UFBX_NODE_PLAIN  = UFBX_ELEMENT_NODE,    // < Plain node, nothing attached
	UFBX_NODE_MESH   = UFBX_ELEMENT_MESH,    // < Contains `ufbx_mesh`
	UFBX_NODE_LIGHT  = UFBX_ELEMENT_LIGHT,   // < Contains `ufbx_light`
	UFBX_NODE_CAMERA = UFBX_ELEMENT_CAMERA,  // < Contains `ufbx_camera`
	UFBX_NODE_BONE   = UFBX_ELEMENT_BONE,    // < Contains `ufbx_bone`
	UFBX_NODE_MULTI  = UFBX_ELEMENT_UNKNOWN, // < Node contains multiple types, see `ufbx_node.elements`
} ufbx_node_type;

UFBX_LIST_TYPE_DECL(ufbx_node_list, ufbx_node);

// Nodes form the scene transformation hierarchy and can contain attached
// elements such as meshes or lights. In normal cases a single `ufbx_node`
// contains only a single attached element, so using `type/mesh/...` is safe.
struct ufbx_node {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Node hierarchy
	ufbx_node *parent;
	ufbx_node_list children;
	uint32_t node_depth;

	// Attached element type and typed pointers. Nonexistent elements are `NULL`
	// so checking `type` is not necessary if acccessing eg. `node->mesh`.
	ufbx_node_type node_type;
	ufbx_mesh *mesh;
	ufbx_light *light;
	ufbx_camera *camera;
	ufbx_bone *bone;

	// All the elements in the node, necessary if `type == UFBX_NODE_MULTI`.
	ufbx_element_ptr_list elements;

	// Local transform in parent, geometry transform is a non-inherited
	// transform applied only to attachments like meshes
	ufbx_inherit_type inherit_type;
	ufbx_transform local_transform;
	ufbx_transform geometry_transform;

	// Raw Euler angles in degrees for those who want them
	ufbx_rotation_order rotation_order;
	ufbx_vec3 euler_rotation;

	// Transform to the global "world" space, may be incorrect if the node
	// uses `UFBX_INHERIT_NORMAL`, prefer using the `node_to_world` matrix.
	ufbx_transform world_transform;

	// Matrices derived from the transformations, for transforming geometry
	// prefer using `geometry_to_world` as that supports geometric transforms.
	ufbx_matrix node_to_parent;
	ufbx_matrix node_to_world;
	ufbx_matrix geometry_to_node;
	ufbx_matrix geometry_to_world;
};

UFBX_LIST_TYPE_DEF(ufbx_node_list, ufbx_node)

// Vertex attribute: All attributes are stored in a consistent indexed format
// regardless of how it's actually stored in the file.
// `data` is a contiguous array of `num_elements` attribute values
// `indices` map each mesh index into a value in the `data` array
typedef struct ufbx_vertex_void {
	void *data;
	int32_t *indices;
	size_t num_elements;
} ufbx_vertex_void;

// 1D vertex attribute, see `ufbx_vertex_void` for information
typedef struct ufbx_vertex_real {
	ufbx_real *data;
	int32_t *indices;
	size_t num_elements;
} ufbx_vertex_real;

// 2D vertex attribute, see `ufbx_vertex_void` for information
typedef struct ufbx_vertex_vec2 {
	ufbx_vec2 *data;
	int32_t *indices;
	size_t num_elements;
} ufbx_vertex_vec2;

// 3D vertex attribute, see `ufbx_vertex_void` for information
typedef struct ufbx_vertex_vec3 {
	ufbx_vec3 *data;
	int32_t *indices;
	size_t num_elements;
} ufbx_vertex_vec3;

// 4D vertex attribute, see `ufbx_vertex_void` for information
typedef struct ufbx_vertex_vec4 {
	ufbx_vec4 *data;
	int32_t *indices;
	size_t num_elements;
} ufbx_vertex_vec4;

// Vertex UV set/layer
typedef struct ufbx_uv_set {
	ufbx_string name;
	int32_t index;

	// Vertex attributes, see `ufbx_mesh` attributes for more information
	ufbx_vertex_vec2 vertex_uv;        // < UV / texture coordinates
	ufbx_vertex_vec3 vertex_tangent;   // < Tangent vector in UV.x direction
	ufbx_vertex_vec3 vertex_bitangent; // < Tangent vector in UV.y direction
} ufbx_uv_set;

// Vertex color set/layer
typedef struct ufbx_color_set {
	ufbx_string name;
	int32_t index;

	// Vertex attributes, see `ufbx_mesh` attributes for more information
	ufbx_vertex_vec4 vertex_color; // < Per-vertex RGBA color
} ufbx_color_set;

UFBX_LIST_TYPE(ufbx_uv_set_list, ufbx_uv_set);
UFBX_LIST_TYPE(ufbx_color_set_list, ufbx_color_set);

// Edge between two _indices_ in a mesh
typedef struct ufbx_edge {
	uint32_t indices[2];
} ufbx_edge;

// Polygonal face with arbitrary number vertices, a single face contains a 
// contiguous range of mesh indices, eg. `{5,3}` would have indices 5, 6, 7
//
// NOTE: `num_indices` maybe less than 3 in which case the face is invalid!
// [TODO #23: should probably remove the bad faces at load time]
typedef struct ufbx_face {
	uint32_t index_begin;
	uint32_t num_indices;
} ufbx_face;

// Polygonal mesh geometry.
struct ufbx_mesh {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Number of "logical" vertices that would be treated as a single point,
	// one vertex may be split to multiple indices for split attributes, eg. UVs
	size_t num_vertices;  // < Number of logical "vertex" points
	size_t num_indices;   // < Number of combiend vertex/attribute tuples
	size_t num_triangles; // < Number of triangles if triangulated

	// Faces and optional per-face extra data
	size_t num_faces;
	ufbx_face *faces;       // < Face index range
	bool *face_smoothing;   // < Should the face have soft normals
	int32_t *face_material; // < Indices to `ufbx_mesh.materials`

	// Edges and optional per-edge extra data
	size_t num_edges;
	ufbx_edge *edges;       // < Edge index range
	bool *edge_smoothing;   // < Should the edge have soft normals
	ufbx_real *edge_crease; // < Crease value for subdivision surfaces

	// Vertex attributes: Every attribute is stored in a consistent indexed
	// format so you can access all attributes for a given index using
	// `vertex_ATTRIB.data[vertex_attrib.indices[index]]` or via the equivalent
	// helper function `ufbx_get_vertex_TYPE(&vertex_ATTRIB, index)`.
	//
	// NOTE: Not all meshes have all attributes, in that case `data == NULL`!
	//
	// NOTE: UV/tangent/bitangent and color are the from first sets,
	// use `uv_sets/color_sets` to access the other layers.
	ufbx_vertex_vec3 vertex_position;  // < Vertex positions
	ufbx_vertex_vec3 vertex_normal;    // < Normal vectors
	ufbx_vertex_vec2 vertex_uv;        // < UV / texture coordinates
	ufbx_vertex_vec3 vertex_tangent;   // < Tangent vector in UV.x direction
	ufbx_vertex_vec3 vertex_bitangent; // < Tangent vector in UV.y direction
	ufbx_vertex_vec4 vertex_color;     // < Per-vertex RGBA color
	ufbx_vertex_real vertex_crease;    // < Crease value for subdivision surfaces

	// Multiple named UV/color sets
	// NOTE: The first set contains the same data as `vertex_uv/color`!
	ufbx_uv_set_list uv_sets;
	ufbx_color_set_list color_sets;

	// List of materials used by the mesh, indexed by `ufbx_mesh.face_material`
	ufbx_material_ptr_list materials;

	// TODO
#if 0
	ufbx_skin_list skins;
	ufbx_blend_channel_ptr_list blend_channels;
#endif
};

UFBX_LIST_TYPE(ufbx_mesh_list, ufbx_mesh);

struct ufbx_light {
	// Element "base class" header
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };
};

UFBX_LIST_TYPE(ufbx_light_list, ufbx_light);

struct ufbx_camera {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };
};

UFBX_LIST_TYPE(ufbx_camera_list, ufbx_camera);

struct ufbx_bone {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };
};

UFBX_LIST_TYPE(ufbx_bone_list, ufbx_bone);

struct ufbx_blend_shape {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Vertex offsets to apply over the base mesh
	// NOTE: `indices` refers to VERTEX indices not mesh indices, so it ranges
	// from `0 .. mesh->num_vertices` and refers to `mesh->vertex_position.data`
	// NOTE: The blend shape indices are _not_ sanitized and may point outside
	// the mesh vertex buffers!
	size_t num_offsets;
	ufbx_vec3 *position_offsets;
	int32_t *indices;
};

UFBX_LIST_TYPE(ufbx_blend_shape_list, ufbx_blend_shape);

struct ufbx_material {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };
};

UFBX_LIST_TYPE(ufbx_material_list, ufbx_material);

struct ufbx_anim_stack {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	double time_begin;
	double time_end;

	ufbx_anim_layer_ptr_list layers;
};

UFBX_LIST_TYPE(ufbx_anim_stack_list, ufbx_anim_stack);

typedef struct ufbx_element_anim_prop {
	ufbx_element *element;
	uint32_t element_id;
	uint32_t internal_key;
	ufbx_string prop_name;
	ufbx_anim_prop *anim_prop;
} ufbx_element_anim_prop;

UFBX_LIST_TYPE(ufbx_element_anim_prop_list, ufbx_element_anim_prop);

struct ufbx_anim_layer {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_real weight;
	bool weight_is_animated;
	bool blended;
	bool additive;
	bool compose_rotation;
	bool compose_scale;

	ufbx_anim_prop_ptr_list anim_props;
	ufbx_element_anim_prop_list element_props; // < Sorted by `element_id,prop_name`
};

UFBX_LIST_TYPE(ufbx_anim_layer_list, ufbx_anim_layer);

struct ufbx_anim_prop {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_vec3 default_value;
	ufbx_anim_curve *curves[3];
};

UFBX_LIST_TYPE(ufbx_anim_prop_list, ufbx_anim_prop);

// Animation curve segment interpolation mode between two keyframes
typedef enum ufbx_interpolation {
	UFBX_INTERPOLATION_CONSTANT_PREV, // < Hold previous key value
	UFBX_INTERPOLATION_CONSTANT_NEXT, // < Hold next key value
	UFBX_INTERPOLATION_LINEAR,        // < Linear interpolation between two keys
	UFBX_INTERPOLATION_CUBIC,         // < Cubic interpolation, see `ufbx_tangent`
} ufbx_interpolation;

// Tangent vector at a keyframe, may be split into left/right
typedef struct ufbx_tangent {
	ufbx_real dx; // < Derivative in the time axis
	ufbx_real dy; // < Derivative in the (curve specific) value axis
} ufbx_tangent;

// Single real `value` at a specified `time`, interpolation between two keyframes
// is determined by the `interpolation` field of the _previous_ key.
// If `interpolation == UFBX_INTERPOLATION_CUBIC` the span is evaluated as a
// cubic bezier curve through the following points:
//
//   (prev->time, prev->value)
//   (prev->time + prev->dx, prev->value + prev->dy)
//   (next->time - prev->dx, next->value - prev->dy)
//   (next->time, next->value)
//
// PROTIP: Use `ufbx_evaluate_curve(ufbx_anim_curve *curve, double time)` rather
// than trying to manually handle all the interpolation modes.
typedef struct ufbx_keyframe {
	double time;
	ufbx_real value;
	ufbx_interpolation interpolation;
	ufbx_tangent left;
	ufbx_tangent right;
} ufbx_keyframe;

UFBX_LIST_TYPE(ufbx_keyframe_list, ufbx_keyframe);

struct ufbx_anim_curve {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_keyframe_list keyframes;
};

UFBX_LIST_TYPE(ufbx_anim_curve_list, ufbx_anim_curve);

// -- Nodes bound with data

typedef struct ufbx_mesh_node { ufbx_node *node; ufbx_mesh *mesh; } ufbx_mesh_node;
typedef struct ufbx_light_node { ufbx_node *node; ufbx_light *light; } ufbx_light_node;
typedef struct ufbx_camera_node { ufbx_node *node; ufbx_camera *camera; } ufbx_camera_node;
typedef struct ufbx_bone_node { ufbx_node *node; ufbx_bone *bone; } ufbx_bone_node;

UFBX_LIST_TYPE(ufbx_mesh_node_list, ufbx_mesh_node);
UFBX_LIST_TYPE(ufbx_light_node_list, ufbx_light_node);
UFBX_LIST_TYPE(ufbx_camera_node_list, ufbx_camera_node);
UFBX_LIST_TYPE(ufbx_bone_node_list, ufbx_bone_node);

// -- Scene

// Scene is the root object loaded by ufbx that everything is accessed from.

typedef struct ufbx_scene ufbx_scene;

// Miscellaneous data related to the loaded file
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
	size_t num_total_blend_channel_refs;
	size_t num_total_skins;
	size_t num_skinned_positions;
	size_t num_skinned_indices;
	size_t max_skinned_positions;
	size_t max_skinned_indices;
	size_t max_skinned_blended_positions;
	size_t max_skinned_blended_indices;

	double ktime_to_sec;
} ufbx_metadata;

struct ufbx_scene {
	ufbx_metadata metadata;

	// Node instances in the scene
	ufbx_node *root_node;
	ufbx_node_list nodes;
	ufbx_mesh_node_list mesh_nodes;
	ufbx_light_node_list light_nodes;
	ufbx_camera_node_list camera_nodes;
	ufbx_bone_node_list bone_nodes;

	// Default animation layers
	ufbx_anim_layer_ptr_list default_anim;

	// Elements that were parsed but not supported by ufbx natively
	// NOTE: Some information like data arrays might be missing
	ufbx_unknown_element_list unknown_elements;

	// These elements are just "types" of meshes/lights/cameras, use
	// the `TYPE_nodes` lists to get ones bound to instanced nodes!
	ufbx_mesh_list meshes;
	ufbx_light_list lights;
	ufbx_camera_list cameras;
	ufbx_bone_list bones;
	ufbx_blend_shape_list blend_shapes;
	ufbx_material_list materials;
	ufbx_anim_stack_list anim_stacks;
	ufbx_anim_layer_list anim_layers;
	ufbx_anim_prop_list anim_props;
	ufbx_anim_curve_list anim_curves;

	// All elements and connections in the whole file
	ufbx_element_ptr_list elements;       // < Sorted by `name,element_type`
	ufbx_connection_list connections_src; // < Sorted by `src->element_id,src_prop`
	ufbx_connection_list connections_dst; // < Sorted by `dst->element_id,dst_prop`
};

// -- Memory callbacks

// You can optionally provide an allocator to ufbx, the default is to use the
// CRT malloc/realloc/free

// Allocate `size` bytes, must be at least 8 byte aligned
typedef void *ufbx_alloc_fn(void *user, size_t size);

// Reallocate `old_ptr` from `old_size` to `new_size`
// NOTE: If omit `alloc_fn` and `free_fn` they will be translated to:
//   `alloc(size)` -> `realloc_fn(user, NULL, 0, size)`
//   `free_fn(ptr, size)` ->  `realloc_fn(user, ptr, size, 0)`
typedef void *ufbx_realloc_fn(void *user, void *old_ptr, size_t old_size, size_t new_size);

// Free pointer `ptr` (of `size` bytes) returned by `alloc_fn` or `realloc_fn`
typedef void ufbx_free_fn(void *user, void *ptr, size_t size);

// Allocator callbacks and user context
// NOTE: The allocator will be stored to the loaded scene and will be called
// again from `ufbx_free_scene()` so make sure `user` outlives that!
typedef struct ufbx_allocator {
	ufbx_alloc_fn *alloc_fn;
	ufbx_realloc_fn *realloc_fn;
	ufbx_free_fn *free_fn;
	void *user;
} ufbx_allocator;

// -- IO callbacks

// Try to read up to `size` bytes to `data`, return the amount of read bytes.
// Return `SIZE_MAX` to indicate an IO error.
typedef size_t ufbx_read_fn(void *user, void *data, size_t size);

// Detailed error stack frame
typedef struct ufbx_error_frame {
	uint32_t source_line;
	const char *function;
	const char *description;
} ufbx_error_frame;

// Error description with detailed stack trace
// HINT: You can use `ufbx_format_error()` for formatting the error
typedef struct ufbx_error {
	const char *description;
	uint32_t stack_size;
	ufbx_error_frame stack[UFBX_ERROR_STACK_MAX_DEPTH];
} ufbx_error;

// -- Inflate

typedef struct ufbx_inflate_input ufbx_inflate_input;
typedef struct ufbx_inflate_retain ufbx_inflate_retain;

// Source data/stream to decompress with `ufbx_inflate()`
struct ufbx_inflate_input {
	// Total size of the data in bytes
	size_t total_size;

	// (optional) Initial or complete data chunk
	const void *data;
	size_t data_size;

	// (optional) Temporary buffer, defaults to 256b stack buffer
	void *buffer;
	size_t buffer_size;

	// (optional) Streaming read function, concatenated after `data`
	ufbx_read_fn *read_fn;
	void *read_user;
};

// Persistent data between `ufbx_inflate()` calls
// NOTE: You must set `initialized` to `false`, but `data` may be uninitialized
struct ufbx_inflate_retain {
	bool initialized;
	uint64_t data[512];
};

// -- Main API

typedef struct ufbx_load_opts {
	ufbx_allocator temp_allocator;   // < Allocator used during loading
	ufbx_allocator result_allocator; // < Allocator used for the final scene

	// Preferences
	bool ignore_geometry;   // < Do not load geometry datsa (vertices, indices, etc)
	bool ignore_animation;  // < Do not load animation curves
	bool evaluate_skinning; // < Evaluate skinning (see ufbx_mesh.skinned_vertices)

	// Allow indices in `ufbx_vertex_TYPE` arrays that area larger than the data
	// array. Enabling this makes `ufbx_get_vertex_TYPE()` unsafe as they don't
	// do bounds checking.
	bool allow_out_of_bounds_vertex_indices;

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

} ufbx_load_opts;

// -- API

#ifdef __cplusplus
extern "C" {
#endif

extern const ufbx_string ufbx_empty_string;
extern const ufbx_matrix ufbx_identity_matrix;
extern const ufbx_transform ufbx_identity_transform;
extern const ufbx_vec2 ufbx_zero_vec2;
extern const ufbx_vec3 ufbx_zero_vec3;
extern const ufbx_vec4 ufbx_zero_vec4;
extern const ufbx_quat ufbx_identity_quat;
extern const uint32_t ufbx_source_version;

// Load a scene from a `size` byte memory buffer at `data`
ufbx_scene *ufbx_load_memory(
	const void *data, size_t size,
	const ufbx_load_opts *opts, ufbx_error *error);

// Load a scene by opening a file named `filename`
ufbx_scene *ufbx_load_file(
	const char *filename,
	const ufbx_load_opts *opts, ufbx_error *error);

// Load a scene by reading from an `FILE *file` stream
// NOTE: Uses a void pointer to not include <stdio.h>
ufbx_scene *ufbx_load_stdio(
	void *file,
	const ufbx_load_opts *opts, ufbx_error *error);

// Load a scene from a user-specified stream with an optional prefix
ufbx_scene *ufbx_load_stream(
	const void *prefix, size_t prefix_size,
	ufbx_read_fn *read_fn, void *read_user,
	const ufbx_load_opts *opts, ufbx_error *error);

// Free a previously loaded or evaluated scene
void ufbx_free_scene(ufbx_scene *scene);

// Query

ufbx_prop *ufbx_find_prop_len(const ufbx_props *props, const char *name, size_t name_len);

// Utility

ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain);

// Animation evaluation

ufbx_real ufbx_evaluate_curve(const ufbx_anim_curve *curve, double time, ufbx_real default_value);

ufbx_real ufbx_evaluate_anim_prop_real(const ufbx_anim_prop *anim_prop, double time);
ufbx_vec2 ufbx_evaluate_anim_prop_vec2(const ufbx_anim_prop *anim_prop, double time);
ufbx_vec3 ufbx_evaluate_anim_prop_vec3(const ufbx_anim_prop *anim_prop, double time);

ufbx_prop ufbx_evaluate_prop_len(ufbx_anim_layer_ptr_list layers, ufbx_element *element, const char *name, size_t name_len, double time);
ufbx_props ufbx_evaluate_props(ufbx_anim_layer_ptr_list layers, ufbx_element *element, double time, ufbx_prop *buffer, size_t buffer_size);

// Math

ufbx_quat ufbx_mul_quat(ufbx_quat a, ufbx_quat b);
ufbx_quat ufbx_slerp(ufbx_quat a, ufbx_quat b, ufbx_real t);
ufbx_vec3 ufbx_rotate_vector(ufbx_quat q, ufbx_vec3 v);
ufbx_quat ufbx_euler_to_quat(ufbx_vec3 v, ufbx_rotation_order order);
ufbx_vec3 ufbx_quat_to_euler(ufbx_quat q, ufbx_rotation_order order);

// -- Inline API

ufbx_inline ufbx_real ufbx_get_vertex_real(const ufbx_vertex_real *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec2 ufbx_get_vertex_vec2(const ufbx_vertex_vec2 *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec3 ufbx_get_vertex_vec3(const ufbx_vertex_vec3 *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec4 ufbx_get_vertex_vec4(const ufbx_vertex_vec4 *v, size_t index) { return v->data[v->indices[index]]; }

#endif

#ifdef __cplusplus
}
#endif


#if 0

// -- Scene data

typedef struct ufbx_element ufbx_element;
typedef struct ufbx_node ufbx_node;
typedef struct ufbx_model ufbx_model;
typedef struct ufbx_mesh ufbx_mesh;
typedef struct ufbx_light ufbx_light;
typedef struct ufbx_camera ufbx_camera;
typedef struct ufbx_bone ufbx_bone;
typedef union ufbx_any_element ufbx_any_element;

typedef struct ufbx_node_ptr_list { ufbx_node **data; size_t size; } ufbx_node_ptr_list;
typedef struct ufbx_model_list { ufbx_model *data; size_t size; } ufbx_model_list;
typedef struct ufbx_mesh_list { ufbx_mesh *data; size_t size; } ufbx_mesh_list;
typedef struct ufbx_light_list { ufbx_light *data; size_t size; } ufbx_light_list;
typedef struct ufbx_camera_list { ufbx_camera *data; size_t size; } ufbx_camera_list;
typedef struct ufbx_bone_list { ufbx_bone *data; size_t size; } ufbx_bone_list;

typedef struct ufbx_material ufbx_material;
typedef struct ufbx_blend_shape ufbx_blend_shape;
typedef struct ufbx_blend_keyframe ufbx_blend_keyframe;
typedef struct ufbx_blend_channel ufbx_blend_channel;

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
typedef struct ufbx_blend_shape_list { ufbx_blend_shape *data; size_t size; } ufbx_blend_shape_list;
typedef struct ufbx_blend_keyframe_list { ufbx_blend_keyframe *data; size_t size; } ufbx_blend_keyframe_list;
typedef struct ufbx_blend_channel_list { ufbx_blend_channel *data; size_t size; } ufbx_blend_channel_list;
typedef struct ufbx_blend_channel_ptr_list { ufbx_blend_channel **data; size_t size; } ufbx_blend_channel_ptr_list;

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

struct ufbx_blend_shape {
	ufbx_string name;

	ufbx_props props;

	size_t num_offsets;
	ufbx_vec3 *position_offsets;
	int32_t *indices;
};

struct ufbx_blend_keyframe {
	ufbx_blend_shape *shape;
	ufbx_real target_weight;
	ufbx_real effective_weight;
};

struct ufbx_blend_channel {
	ufbx_string name;
	ufbx_props props;

	ufbx_real weight;

	ufbx_blend_keyframe_list keyframes;
};

typedef enum ufbx_node_type {
	UFBX_NODE_UNKNOWN,
	UFBX_NODE_MODEL,
	UFBX_NODE_MESH,
	UFBX_NODE_LIGHT,
	UFBX_NODE_CAMERA,
	UFBX_NODE_BONE,
} ufbx_node_type;

typedef enum ufbx_aspect_mode {
	UFBX_ASPECT_MODE_WINDOW_SIZE,
	UFBX_ASPECT_MODE_FIXED_RATIO,
	UFBX_ASPECT_MODE_FIXED_RESOLUTION,
	UFBX_ASPECT_MODE_FIXED_WIDTH,
	UFBX_ASPECT_MODE_FIXED_HEIGHT,
} ufbx_aspect_mode;

typedef enum ufbx_aperture_mode {
	UFBX_APERTURE_MODE_HORIZONTAL_AND_VERTICAL,
	UFBX_APERTURE_MODE_HORIZONTAL,
	UFBX_APERTURE_MODE_VERTICAL,
	UFBX_APERTURE_MODE_FOCAL_LENGTH,
} ufbx_aperture_mode;

typedef enum ufbx_aperture_format {
	UFBX_APERTURE_FORMAT_CUSTOM,
	UFBX_APERTURE_FORMAT_16MM_THEATRICAL,
	UFBX_APERTURE_FORMAT_SUPER_16MM,
	UFBX_APERTURE_FORMAT_35MM_ACADEMY,
	UFBX_APERTURE_FORMAT_35MM_TV_PROJECTION,
	UFBX_APERTURE_FORMAT_35MM_FULL_APERTURE,
	UFBX_APERTURE_FORMAT_35MM_185_PROJECTION,
	UFBX_APERTURE_FORMAT_35MM_ANAMORPHIC,
	UFBX_APERTURE_FORMAT_70MM_PROJECTION,
	UFBX_APERTURE_FORMAT_VISTAVISION,
	UFBX_APERTURE_FORMAT_DYNAVISION,
	UFBX_APERTURE_FORMAT_IMAX,
} ufbx_aperture_format;

typedef enum ufbx_gate_fit {
	UFBX_GATE_FIT_NONE,
	UFBX_GATE_FIT_VERTICAL,
	UFBX_GATE_FIT_HORIZONTAL,
	UFBX_GATE_FIT_FILL,
	UFBX_GATE_FIT_OVERSCAN,
	UFBX_GATE_FIT_STRETCH,
} ufbx_gate_fit;

struct ufbx_element {
	ufbx_element_type element_type;
	ufbx_string name;
	ufbx_props props;
};

struct ufbx_node {
	union {
		ufbx_element element;
		struct {
			ufbx_element_type element_type;
			ufbx_string name;
			ufbx_props props;
		};
	};

	ufbx_node_type type;
	ufbx_node *parent;
	ufbx_inherit_type inherit_type;
	ufbx_transform transform;
	ufbx_transform world_transform;
	ufbx_matrix to_parent;
	ufbx_matrix to_root;
	ufbx_node_ptr_list children;
};

struct ufbx_model {
	union {
		ufbx_element element;
		struct {
			ufbx_element_type element_type;
			ufbx_string name;
			ufbx_props props;
		};
	};

};

struct ufbx_mesh {
	union {
		ufbx_element element;
		struct {
			ufbx_element_type element_type;
			ufbx_string name;
			ufbx_props props;
		};
	};

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
	ufbx_blend_channel_ptr_list blend_channels;
};

struct ufbx_light {
	union {
		ufbx_element element;
		struct {
			ufbx_element_type element_type;
			ufbx_string name;
			ufbx_props props;
		};
	};

	ufbx_vec3 color;
	ufbx_real intensity;
};

struct ufbx_camera {
	union {
		ufbx_element element;
		struct {
			ufbx_element_type element_type;
			ufbx_string name;
			ufbx_props props;
		};
	};

	bool resolution_is_pixels;
	ufbx_vec2 resolution;
	ufbx_vec2 field_of_view_deg;
	ufbx_vec2 field_of_view_tan;

	ufbx_aspect_mode aspect_mode;
	ufbx_aperture_format aperture_format;
	ufbx_aperture_mode aperture_mode;
	ufbx_gate_fit gate_fit;
	ufbx_real focal_length_mm;
	ufbx_vec2 film_size_inch;
	ufbx_vec2 aperture_size_inch;
	ufbx_real squeeze_ratio;
};

struct ufbx_bone {
	union {
		ufbx_element element;
		struct {
			ufbx_element_type element_type;
			ufbx_string name;
			ufbx_props props;
		};
	};

	ufbx_real length;
};

union ufbx_any_element {
	ufbx_element_type element_type;
	ufbx_element element;
	ufbx_node node;
	ufbx_model model;
	ufbx_mesh mesh;
	ufbx_light light;
	ufbx_bone bone;
};

struct ufbx_light_node {
	ufbx_node *node;
	ufbx_light *light;
};

struct ufbx_camera_node {
	ufbx_node *node;
	ufbx_camera *camera;
};

struct ufbx_mesh_node {
	ufbx_node *node;
	ufbx_mesh *mesh;
};

struct ufbx_any_node {
	ufbx_node *node;
	ufbx_any_element *element;
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
	UFBX_ANIM_CAMERA,
	UFBX_ANIM_MATERIAL,
	UFBX_ANIM_BONE,
	UFBX_ANIM_BLEND_CHANNEL,
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
	uint32_t internal_key;
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
	size_t num_total_blend_channel_refs;
	size_t num_total_skins;
	size_t num_skinned_positions;
	size_t num_skinned_indices;
	size_t max_skinned_positions;
	size_t max_skinned_indices;
	size_t max_skinned_blended_positions;
	size_t max_skinned_blended_indices;

	double ktime_to_sec;
} ufbx_metadata;

struct ufbx_scene {
	ufbx_metadata metadata;

	ufbx_model *root;

	ufbx_node_ptr_list nodes;
	ufbx_model_list models;
	ufbx_mesh_list meshes;
	ufbx_light_list lights;
	ufbx_camera_list cameras;
	ufbx_bone_list bones;
	ufbx_blend_shape_list blend_shapes;
	ufbx_blend_channel_list blend_channels;

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
	const char *description;
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
	bool evaluate_skinning;

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
	bool allow_out_of_bounds_indices;
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
extern const uint32_t ufbx_source_version;

ufbx_scene *ufbx_load_memory(const void *data, size_t size, const ufbx_load_opts *opts, ufbx_error *error);
ufbx_scene *ufbx_load_file(const char *filename, const ufbx_load_opts *opts, ufbx_error *error);
ufbx_scene *ufbx_load_stdio(void *file, const ufbx_load_opts *opts, ufbx_error *error);
void ufbx_free_scene(ufbx_scene *scene);

size_t ufbx_format_error(char *dst, size_t dst_size, const ufbx_error *error);

ufbx_node *ufbx_find_node_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_mesh *ufbx_find_mesh_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_material *ufbx_find_material_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_light *ufbx_find_light_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_camera *ufbx_find_camera_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_bone *ufbx_find_bone_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_blend_channel *ufbx_find_blend_channel_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_anim_stack *ufbx_find_anim_stack_len(const ufbx_scene *scene, const char *name, size_t name_len);
ufbx_anim_layer *ufbx_find_anim_layer_len(const ufbx_scene *scene, const char *name, size_t name_len);

ufbx_prop *ufbx_find_prop_len(const ufbx_props *props, const char *name, size_t name_len);

ufbx_anim_prop *ufbx_find_anim_prop_len(const ufbx_anim_prop *props, const char *name, size_t name_len);
size_t ufbx_anim_prop_count(const ufbx_anim_prop *props);

ufbx_anim_prop *ufbx_find_anim_prop_begin(const ufbx_scene *scene, const ufbx_anim_layer *layer, ufbx_anim_target target, uint32_t index);
ufbx_anim_prop *ufbx_find_node_anim_prop_begin(const ufbx_scene *scene, const ufbx_anim_layer *layer, const ufbx_node *node);
ufbx_anim_prop *ufbx_find_blend_channel_anim_prop_begin(const ufbx_scene *scene, const ufbx_anim_layer *layer, const ufbx_blend_channel *channel);

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

size_t ufbx_triangulate(uint32_t *indices, size_t num_indices, ufbx_mesh *mesh, ufbx_face face);

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

ufbx_inline ufbx_camera *ufbx_find_camera(const ufbx_scene *scene, const char *name) {
	return ufbx_find_camera_len(scene, name, strlen(name));
}

ufbx_inline ufbx_bone *ufbx_find_bone(const ufbx_scene *scene, const char *name) {
	return ufbx_find_bone_len(scene, name, strlen(name));
}

ufbx_inline ufbx_blend_channel *ufbx_find_blend_channel(const ufbx_scene *scene, const char *name) {
	return ufbx_find_blend_channel_len(scene, name, strlen(name));
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

ufbx_inline ufbx_anim_prop *ufbx_find_anim_prop(const ufbx_anim_prop *props, const char *name) {
	return ufbx_find_anim_prop_len(props, name, strlen(name));
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
ufbx_inline ufbx_camera *begin(const ufbx_camera_list &l) { return l.data; }
ufbx_inline ufbx_camera *end(const ufbx_camera_list &l) { return l.data + l.size; }
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
ufbx_inline ufbx_blend_shape *begin(const ufbx_blend_shape_list& l) { return l.data; }
ufbx_inline ufbx_blend_shape *end(const ufbx_blend_shape_list& l) { return l.data + l.size; }

#endif

#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#endif
