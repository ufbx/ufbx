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
#else
	#define UFBX_LIST_TYPE(p_name, p_type) typedef struct p_name { p_type *data; size_t count; } p_name
#endif

// -- Version

#define ufbx_pack_version(major, minor, patch) ((uint32_t)(major)*1000000u + (uint32_t)(minor)*1000u + (uint32_t)(patch))
#define ufbx_version_major(version) ((uint32_t)(version)/1000000u%1000u)
#define ufbx_version_minor(version) ((uint32_t)(version)/1000u%1000u)
#define ufbx_version_patch(version) ((uint32_t)(version)%1000u)

#define UFBX_HEADER_VERSION ufbx_pack_version(0, 1, 1)

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

UFBX_LIST_TYPE(ufbx_real_list, ufbx_real);
UFBX_LIST_TYPE(ufbx_vec4_list, ufbx_vec4);

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
	UFBX_PROP_DISTANCE,
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
	UFBX_PROP_FLAG_CONNECTED = 0x8000,
	UFBX_PROP_FLAG_NO_VALUE = 0x10000,
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

typedef struct ufbx_element ufbx_element;

// Unknown
typedef struct ufbx_unknown ufbx_unknown;

// Nodes
typedef struct ufbx_node ufbx_node;

// Node attributes (common)
typedef struct ufbx_mesh ufbx_mesh;
typedef struct ufbx_light ufbx_light;
typedef struct ufbx_camera ufbx_camera;
typedef struct ufbx_bone ufbx_bone;
typedef struct ufbx_empty ufbx_empty;

// Node attributes (curves/surfaces)
typedef struct ufbx_line_curve ufbx_line_curve;
typedef struct ufbx_nurbs_curve ufbx_nurbs_curve;
typedef struct ufbx_patch_surface ufbx_patch_surface;
typedef struct ufbx_nurbs_surface ufbx_nurbs_surface;
typedef struct ufbx_nurbs_trim_surface ufbx_nurbs_trim_surface;
typedef struct ufbx_nurbs_trim_boundary ufbx_nurbs_trim_boundary;

// Node attributes (advanced)
typedef struct ufbx_procedural_geometry ufbx_procedural_geometry;
typedef struct ufbx_camera_stereo ufbx_camera_stereo;
typedef struct ufbx_camera_switcher ufbx_camera_switcher;
typedef struct ufbx_lod_group ufbx_lod_group;

// Deformers
typedef struct ufbx_skin_deformer ufbx_skin_deformer;
typedef struct ufbx_skin_cluster ufbx_skin_cluster;
typedef struct ufbx_blend_deformer ufbx_blend_deformer;
typedef struct ufbx_blend_channel ufbx_blend_channel;
typedef struct ufbx_blend_shape ufbx_blend_shape;
typedef struct ufbx_cache_deformer ufbx_cache_deformer;

// Materials
typedef struct ufbx_material ufbx_material;
typedef struct ufbx_texture ufbx_texture;
typedef struct ufbx_video ufbx_video;
typedef struct ufbx_shader ufbx_shader;
typedef struct ufbx_shader_binding ufbx_shader_binding;

// Animation
typedef struct ufbx_anim_stack ufbx_anim_stack;
typedef struct ufbx_anim_layer ufbx_anim_layer;
typedef struct ufbx_anim_value ufbx_anim_value;
typedef struct ufbx_anim_curve ufbx_anim_curve;

// Miscellaneous
typedef struct ufbx_pose ufbx_pose;

UFBX_LIST_TYPE(ufbx_element_list, ufbx_element*);
UFBX_LIST_TYPE(ufbx_unknown_list, ufbx_unknown*);
UFBX_LIST_TYPE(ufbx_node_list, ufbx_node*);
UFBX_LIST_TYPE(ufbx_mesh_list, ufbx_mesh*);
UFBX_LIST_TYPE(ufbx_light_list, ufbx_light*);
UFBX_LIST_TYPE(ufbx_camera_list, ufbx_camera*);
UFBX_LIST_TYPE(ufbx_bone_list, ufbx_bone*);
UFBX_LIST_TYPE(ufbx_empty_list, ufbx_empty*);
UFBX_LIST_TYPE(ufbx_line_curve_list, ufbx_line_curve*);
UFBX_LIST_TYPE(ufbx_nurbs_curve_list, ufbx_nurbs_curve*);
UFBX_LIST_TYPE(ufbx_patch_surface_list, ufbx_patch_surface*);
UFBX_LIST_TYPE(ufbx_nurbs_surface_list, ufbx_nurbs_surface*);
UFBX_LIST_TYPE(ufbx_nurbs_trim_surface_list, ufbx_nurbs_trim_surface*);
UFBX_LIST_TYPE(ufbx_nurbs_trim_boundary_list, ufbx_nurbs_trim_boundary*);
UFBX_LIST_TYPE(ufbx_procedural_geometry_list, ufbx_procedural_geometry*);
UFBX_LIST_TYPE(ufbx_camera_stereo_list, ufbx_camera_stereo*);
UFBX_LIST_TYPE(ufbx_camera_switcher_list, ufbx_camera_switcher*);
UFBX_LIST_TYPE(ufbx_lod_group_list, ufbx_lod_group*);
UFBX_LIST_TYPE(ufbx_skin_deformer_list, ufbx_skin_deformer*);
UFBX_LIST_TYPE(ufbx_skin_cluster_list, ufbx_skin_cluster*);
UFBX_LIST_TYPE(ufbx_blend_deformer_list, ufbx_blend_deformer*);
UFBX_LIST_TYPE(ufbx_blend_channel_list, ufbx_blend_channel*);
UFBX_LIST_TYPE(ufbx_blend_shape_list, ufbx_blend_shape*);
UFBX_LIST_TYPE(ufbx_cache_deformer_list, ufbx_cache_deformer*);
UFBX_LIST_TYPE(ufbx_material_list, ufbx_material*);
UFBX_LIST_TYPE(ufbx_texture_list, ufbx_texture*);
UFBX_LIST_TYPE(ufbx_video_list, ufbx_video*);
UFBX_LIST_TYPE(ufbx_shader_list, ufbx_shader*);
UFBX_LIST_TYPE(ufbx_shader_binding_list, ufbx_shader_binding*);
UFBX_LIST_TYPE(ufbx_anim_stack_list, ufbx_anim_stack*);
UFBX_LIST_TYPE(ufbx_anim_layer_list, ufbx_anim_layer*);
UFBX_LIST_TYPE(ufbx_anim_value_list, ufbx_anim_value*);
UFBX_LIST_TYPE(ufbx_anim_curve_list, ufbx_anim_curve*);
UFBX_LIST_TYPE(ufbx_pose_list, ufbx_pose*);

typedef enum ufbx_element_type {
	UFBX_ELEMENT_UNKNOWN,             // < `ufbx_unknown`
	UFBX_ELEMENT_NODE,                // < `ufbx_node`
	UFBX_ELEMENT_MESH,                // < `ufbx_mesh`
	UFBX_ELEMENT_LIGHT,               // < `ufbx_light`
	UFBX_ELEMENT_CAMERA,              // < `ufbx_camera`
	UFBX_ELEMENT_BONE,                // < `ufbx_bone`
	UFBX_ELEMENT_EMPTY,               // < `ufbx_empty`
	UFBX_ELEMENT_LINE_CURVE,          // < `ufbx_line_curve`
	UFBX_ELEMENT_NURBS_CURVE,         // < `ufbx_nurbs_curve`
	UFBX_ELEMENT_PATCH_SURFACE,       // < `ufbx_patch_surface`
	UFBX_ELEMENT_NURBS_SURFACE,       // < `ufbx_nurbs_surface`
	UFBX_ELEMENT_NURBS_TRIM_SURFACE,  // < `ufbx_nurbs_trim_surface`
	UFBX_ELEMENT_NURBS_TRIM_BOUNDARY, // < `ufbx_nurbs_trim_boundary`
	UFBX_ELEMENT_PROCEDURAL_GEOMETRY, // < `ufbx_procedural_geometry`
	UFBX_ELEMENT_CAMERA_STEREO,       // < `ufbx_camera_stereo`
	UFBX_ELEMENT_CAMERA_SWITCHER,     // < `ufbx_camera_switcher`
	UFBX_ELEMENT_LOD_GROUP,           // < `ufbx_lod_group`
	UFBX_ELEMENT_SKIN_DEFORMER,       // < `ufbx_skin_deformer`
	UFBX_ELEMENT_SKIN_CLUSTER,        // < `ufbx_skin_cluster`
	UFBX_ELEMENT_BLEND_DEFORMER,      // < `ufbx_blend_deformer`
	UFBX_ELEMENT_BLEND_CHANNEL,       // < `ufbx_blend_channel`
	UFBX_ELEMENT_BLEND_SHAPE,         // < `ufbx_blend_shape`
	UFBX_ELEMENT_CACHE_DEFORMER,      // < `ufbx_cache_deformer`
	UFBX_ELEMENT_MATERIAL,            // < `ufbx_material`
	UFBX_ELEMENT_TEXTURE,             // < `ufbx_texture`
	UFBX_ELEMENT_VIDEO,               // < `ufbx_video`
	UFBX_ELEMENT_SHADER,              // < `ufbx_shader`
	UFBX_ELEMENT_SHADER_BINDING,      // < `ufbx_shader_binding`
	UFBX_ELEMENT_ANIM_STACK,          // < `ufbx_anim_stack`
	UFBX_ELEMENT_ANIM_LAYER,          // < `ufbx_anim_layer`
	UFBX_ELEMENT_ANIM_VALUE,          // < `ufbx_anim_value`
	UFBX_ELEMENT_ANIM_CURVE,          // < `ufbx_anim_curve`
	UFBX_ELEMENT_POSE,                // < `ufbx_pose`

	UFBX_NUM_ELEMENT_TYPES,
	UFBX_ELEMENT_TYPE_FIRST_ATTRIB = UFBX_ELEMENT_MESH,
	UFBX_ELEMENT_TYPE_LAST_ATTRIB = UFBX_ELEMENT_LOD_GROUP,
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
	ufbx_node_list instances;
	ufbx_element_type type;
	uint32_t id;
	uint32_t typed_id;
	ufbx_connection_list connections_src;
	ufbx_connection_list connections_dst;
};

// -- Unknown

struct ufbx_unknown {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// FBX format specific type information
	ufbx_string type;
	ufbx_string sub_type;
};

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

// Nodes form the scene transformation hierarchy and can contain attached
// elements such as meshes or lights. In normal cases a single `ufbx_node`
// contains only a single attached element, so using `type/mesh/...` is safe.
struct ufbx_node {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Node hierarchy
	ufbx_node *parent;
	ufbx_node_list children;
	uint32_t node_depth;

	// Attached element type and typed pointers. Nonexistent attributes are `NULL`
	// so checking `type` is not necessary if acccessing eg. `node->mesh`.
	ufbx_element_type attrib_type;
	ufbx_element *attrib;
	ufbx_mesh *mesh;
	ufbx_light *light;
	ufbx_camera *camera;
	ufbx_bone *bone;
	ufbx_element_list all_attribs;

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

// Vertex attribute: All attributes are stored in a consistent indexed format
// regardless of how it's actually stored in the file.
// `data` is a contiguous array of `num_values` attribute values.
// `indices` maps each mesh index into a value in the `data` array.
//
// If `unique_per_vertex` is set then the attribute is guaranteed to have a
// single defined value per vertex accessible via:
//   `attrib.data[attrib.indices[mesh->vertex_first_index[vertex_ix]]`
// NOTE: The attribute may be unique per vertex also if `unique_per_vertex`
// is `false` depending on how it's stored in the FBX file.
typedef struct ufbx_vertex_attrib {
	ufbx_real *data;
	int32_t *indices;
	size_t num_values;
	size_t value_reals;
	bool unique_per_vertex;
} ufbx_vertex_attrib;

// 1D vertex attribute, see `ufbx_vertex_attrib` for information
typedef struct ufbx_vertex_real {
	ufbx_real *data;
	int32_t *indices;
	size_t num_values;
	size_t value_reals;
	bool unique_per_vertex;
} ufbx_vertex_real;

// 2D vertex attribute, see `ufbx_vertex_attrib` for information
typedef struct ufbx_vertex_vec2 {
	ufbx_vec2 *data;
	int32_t *indices;
	size_t num_values;
	size_t value_reals;
	bool unique_per_vertex;
} ufbx_vertex_vec2;

// 3D vertex attribute, see `ufbx_vertex_attrib` for information
typedef struct ufbx_vertex_vec3 {
	ufbx_vec3 *data;
	int32_t *indices;
	size_t num_values;
	size_t value_reals;
	bool unique_per_vertex;
} ufbx_vertex_vec3;

// 4D vertex attribute, see `ufbx_vertex_attrib` for information
typedef struct ufbx_vertex_vec4 {
	ufbx_vec4 *data;
	int32_t *indices;
	size_t num_values;
	size_t value_reals;
	bool unique_per_vertex;
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

typedef struct ufbx_mesh_material {
	ufbx_material *material;

	size_t num_faces;
	size_t num_triangles;
	int32_t *faces;
} ufbx_mesh_material;

UFBX_LIST_TYPE(ufbx_mesh_material_list, ufbx_mesh_material);

typedef enum ufbx_subdivision_display_mode {
	UFBX_SUBDIVISION_DISPLAY_DISABLED,
	UFBX_SUBDIVISION_DISPLAY_HULL,
	UFBX_SUBDIVISION_DISPLAY_HULL_AND_SMOOTH,
	UFBX_SUBDIVISION_DISPLAY_SMOOTH,
} ufbx_subdivision_display_mode;

// TODO: Relate these to OpenSubdiv modes
typedef enum ufbx_subdivision_boundary {
	UFBX_SUBDIVISION_BOUNDARY_DEFAULT,
	UFBX_SUBDIVISION_BOUNDARY_LEGACY,
	// OpenSubdiv: VTX_BOUNDARY_EDGE_ONLY/FVAR_LINEAR_NONE
	UFBX_SUBDIVISION_BOUNDARY_SHARP_NONE,
	// OpenSubdiv: VTX_BOUNDARY_EDGE_AND_CORNER/FVAR_LINEAR_CORNERS_ONLY
	UFBX_SUBDIVISION_BOUNDARY_SHARP_CORNERS,
	// OpenSubdiv: FVAR_LINEAR_BOUNDARIES
	UFBX_SUBDIVISION_BOUNDARY_SHARP_BOUNDARY,
	// OpenSubdiv: FVAR_LINEAR_ALL
	UFBX_SUBDIVISION_BOUNDARY_SHARP_INTERIOR,
} ufbx_subdivision_boundary;

// Polygonal mesh geometry.
//
// Example mesh with two triangles (x, z) and a quad (y).
// The faces have a constant UV coordinate x/y/z.
// The vertices have _per vertex_ normals that point up/down.
// 
//     ^   ^     ^
//     A---B-----C
//     |x /     /|
//     | /  y  / |
//     |/     / z|
//     D-----E---F
//     v     v   v
//
// Attributes may have multiple values within a single vertex, for example a
// UV seam vertex has two UV coordinates. Thus polygons are defined using
// an index that counts each corner of each face polygon. If an attribute is
// defined (even per-vertex) it will always have a valid `indices` array.
//
//   {0,3}    {3,4}    {7,3}   faces ({ index_begin, num_indices })
//   0 1 2   3 4 5 6   7 8 9   index
//
//   0 1 3   1 2 4 3   2 4 5   vertex_indices[index]
//   A B D   B C E D   C E F   vertices[vertex_indices[index]]
//
//   0 0 1   0 0 1 1   0 1 1   vertex_normal.indices[index]
//   ^ ^ v   ^ ^ v v   ^ v v   vertex_normal.data[vertex_normal.indices[index]]
//
//   0 0 0   1 1 1 1   2 2 2   vertex_uv.indices[index]
//   x x x   y y y y   z z z   vertex_uv.data[vertex_uv.indices[index]]
//
// Vertex position can also be accessed uniformly through an accessor:
//   0 1 3   1 2 4 3   2 4 5   vertex_position.indices[index]
//   A B D   B C E D   C E F   vertex_position.data[vertex_position.indices[index]]
//
// Some geometry data is specified per logical vertex. Vertex positions are
// the only attribute that is guaranteed to be defined _uniquely_ per vertex.
// Vertex attributes _may_ be defined per vertex if `unique_per_vertex == true`.
// You can access the per-vertex values by first finding the first index that
// refers to the given vertex.
//
//   0 1 2 3 4 5  vertex
//   A B C D E F  vertices[vertex]
//
//   0 1 4 2 5 9  vertex_first_index[vertex]
//   0 0 0 1 1 1  vertex_normal.indices[vertex_first_index[vertex]]
//   ^ ^ ^ v v v  vertex_normal.data[vertex_normal.indices[vertex_first_index[vertex]]]
//
struct ufbx_mesh {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };

	// Number of "logical" vertices that would be treated as a single point,
	// one vertex may be split to multiple indices for split attributes, eg. UVs
	size_t num_vertices;  // < Number of logical "vertex" points
	size_t num_indices;   // < Number of combiend vertex/attribute tuples
	size_t num_triangles; // < Number of triangles

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

	// Logical vertices and positions, alternatively you can use
	// `vertex_position` for consistent interface with other attributes.
	int32_t *vertex_indices;
	ufbx_vec3 *vertices;

	// First index referring to a given vertex, `-1` if the vertex is unused.
	int32_t *vertex_first_index;

	// Vertex attributes, see the comment over the struct.
	//
	// NOTE: Not all meshes have all attributes, in that case `indices/data == NULL`!
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

	// List of materials used by the mesh
	ufbx_mesh_material_list materials;

	// Skinned vertex positions, for efficiency the skinned positions are the
	// same as the static ones for non-skinned meshes and `skinned_is_local`
	// is set to true meaning you need to transform them manually using
	// `ufbx_transform_position(&node->geometry_to_world, skinned_pos)`!
	bool skinned_is_local;
	ufbx_vertex_vec3 skinned_position;
	ufbx_vertex_vec3 skinned_normal;

	// Deformers
	ufbx_skin_deformer_list skins;
	ufbx_blend_deformer_list blend_shapes;
	ufbx_cache_deformer_list geometry_caches;
	ufbx_element_list all_deformers;

	// Subdivision
	int32_t subdivision_preview_levels;
	int32_t subdivision_render_levels;
	ufbx_subdivision_display_mode subdivision_display_mode;
	ufbx_subdivision_boundary subdivision_boundary;
	ufbx_subdivision_boundary subdivision_uv_boundary;
	bool subdivision_evaluated;
};

// The kind of light source
typedef enum ufbx_light_type {
	// Single point at local origin, at `node->world_transform.position`
	UFBX_LIGHT_POINT,
	// Infinite directional light pointing locally towards `light->local_direction`
	// For global: `ufbx_transform_direction(&node->node_to_world, light->local_direction)`
	UFBX_LIGHT_DIRECTIONAL,
	// Cone shaped light towards `light->local_direction`, between `light->inner/outer_angle`.
	// For global: `ufbx_transform_direction(&node->node_to_world, light->local_direction)`
	UFBX_LIGHT_SPOT,
	// Area light, shape specified by `light->area_shape`
	// TODO: Units?
	UFBX_LIGHT_AREA,
	// Volumetric light source
	// TODO: How does this work
	UFBX_LIGHT_VOLUME,
} ufbx_light_type;

// How fast does the light intensity decay at a distance
typedef enum ufbx_light_decay {
	UFBX_LIGHT_DECAY_NONE,      // < 1 (no decay)
	UFBX_LIGHT_DECAY_LINEAR,    // < 1 / d
	UFBX_LIGHT_DECAY_QUADRATIC, // < 1 / d^2 (physically accurate)
	UFBX_LIGHT_DECAY_CUBIC,     // < 1 / d^3
} ufbx_light_decay;

typedef enum ufbx_light_area_shape {
	UFBX_LIGHT_AREA_RECTANGLE,
	UFBX_LIGHT_AREA_SPHERE,
} ufbx_light_area_shape;

// Light source attached to a `ufbx_node`
struct ufbx_light {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };

	// Color and intensity of the light, usually you want to use `color * intensity` 
	// NOTE: `intensity` is 0.01x of the property `"Intensity"` as that matches
	// matches values in DCC programs before exporting.
	ufbx_vec3 color;
	ufbx_real intensity;

	// Direction the light is aimed at in node's local space, usually -Y
	ufbx_vec3 local_direction;

	// Type of the light and shape parameters
	ufbx_light_type type;
	ufbx_light_decay decay;
	ufbx_light_area_shape area_shape;
	ufbx_real inner_angle;
	ufbx_real outer_angle;

	bool cast_light;
	bool cast_shadows;
};

// Method of specifying the rendering resolution from properties
// NOTE: Handled internally by ufbx, ignore unless you interpret `ufbx_props` directly!
typedef enum ufbx_aspect_mode {
	// No defined resolution
	UFBX_ASPECT_MODE_WINDOW_SIZE,
	// `"AspectWidth"` and `"AspectHeight"` are relative to each other
	UFBX_ASPECT_MODE_FIXED_RATIO,
	// `"AspectWidth"` and `"AspectHeight"` are both pixels
	UFBX_ASPECT_MODE_FIXED_RESOLUTION,
	// `"AspectWidth"` is pixels, `"AspectHeight"` is relative to width
	UFBX_ASPECT_MODE_FIXED_WIDTH,
	// < `"AspectHeight"` is pixels, `"AspectWidth"` is relative to height
	UFBX_ASPECT_MODE_FIXED_HEIGHT,
} ufbx_aspect_mode;

// Method of specifying the field of view from properties
// NOTE: Handled internally by ufbx, ignore unless you interpret `ufbx_props` directly!
typedef enum ufbx_aperture_mode {
	// Use separate `"FieldOfViewX"` and `"FieldOfViewY"` as horizontal/vertical FOV angles
	UFBX_APERTURE_MODE_HORIZONTAL_AND_VERTICAL,
	// Use `"FieldOfView"` as horizontal FOV angle, derive vertical angle via aspect ratio
	UFBX_APERTURE_MODE_HORIZONTAL,
	// Use `"FieldOfView"` as vertical FOV angle, derive horizontal angle via aspect ratio
	UFBX_APERTURE_MODE_VERTICAL,
	// Compute the field of view from the render gate size and focal length
	UFBX_APERTURE_MODE_FOCAL_LENGTH,
} ufbx_aperture_mode;

// Method of specifying the render gate size from properties
// NOTE: Handled internally by ufbx, ignore unless you interpret `ufbx_props` directly!
typedef enum ufbx_gate_fit {
	// Use the film/aperture size directly as the render gate
	UFBX_GATE_FIT_NONE,
	// Fit the render gate to the height of the film, derive width from aspect ratio
	UFBX_GATE_FIT_VERTICAL,
	// Fit the render gate to the width of the film, derive height from aspect ratio
	UFBX_GATE_FIT_HORIZONTAL,
	// Fit the render gate so that it is fully contained within the film gate
	UFBX_GATE_FIT_FILL,
	// Fit the render gate so that it fully contains the film gate
	UFBX_GATE_FIT_OVERSCAN,
	// Stretch the render gate to match the film gate
	// TODO: Does this differ from `UFBX_GATE_FIT_NONE`?
	UFBX_GATE_FIT_STRETCH,
} ufbx_gate_fit;

// Camera film/aperture size defaults
// NOTE: Handled internally by ufbx, ignore unless you interpret `ufbx_props` directly!
typedef enum ufbx_aperture_format {
	UFBX_APERTURE_FORMAT_CUSTOM,              // < Use `"FilmWidth"` and `"FilmHeight"`
	UFBX_APERTURE_FORMAT_16MM_THEATRICAL,     // < 0.404 x 0.295 inches
	UFBX_APERTURE_FORMAT_SUPER_16MM,          // < 0.493 x 0.292 inches
	UFBX_APERTURE_FORMAT_35MM_ACADEMY,        // < 0.864 x 0.630 inches
	UFBX_APERTURE_FORMAT_35MM_TV_PROJECTION,  // < 0.816 x 0.612 inches
	UFBX_APERTURE_FORMAT_35MM_FULL_APERTURE,  // < 0.980 x 0.735 inches
	UFBX_APERTURE_FORMAT_35MM_185_PROJECTION, // < 0.825 x 0.446 inches
	UFBX_APERTURE_FORMAT_35MM_ANAMORPHIC,     // < 0.864 x 0.732 inches (squeeze ratio: 2)
	UFBX_APERTURE_FORMAT_70MM_PROJECTION,     // < 2.066 x 0.906 inches
	UFBX_APERTURE_FORMAT_VISTAVISION,         // < 1.485 x 0.991 inches
	UFBX_APERTURE_FORMAT_DYNAVISION,          // < 2.080 x 1.480 inches
	UFBX_APERTURE_FORMAT_IMAX,                // < 2.772 x 2.072 inches
} ufbx_aperture_format;

// Camera attached to a `ufbx_node`
struct ufbx_camera {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };

	// If set to `true`, `resolution` reprensents actual pixel values, otherwise
	// it's only useful for its aspect ratio.
	bool resolution_is_pixels;

	// Render resolution, either in pixels or arbitrary units, depending on above
	ufbx_vec2 resolution;

	// Horizontal/vertical field of view in degrees
	ufbx_vec2 field_of_view_deg;

	// Component-wise `tan(field_of_view_deg)`, also represents the size of the
	// proection frustum slice at distance of 1.
	ufbx_vec2 field_of_view_tan;

	// Advanced properties used to compute the above
	ufbx_aspect_mode aspect_mode;
	ufbx_aperture_mode aperture_mode;
	ufbx_gate_fit gate_fit;
	ufbx_aperture_format aperture_format;
	ufbx_real focal_length_mm;     // < Focal length in millimeters
	ufbx_vec2 film_size_inch;      // < Film size in inches
	ufbx_vec2 aperture_size_inch;  // < Aperture/film gate size in inches
	ufbx_real squeeze_ratio;       // < Anamoprhic stretch ratio
};

// Bone attached to a `ufbx_node`, provides the logical length of the bone
// but most interesting information is directly in `ufbx_node`.
struct ufbx_bone {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };

	// Visual radius of the bone
	ufbx_real radius;

	// Length of the bone relative to the distance between two nodes
	ufbx_real relative_length;
};

// Empty/NULL/locator connected to a node, actual details in `ufbx_node`
struct ufbx_empty {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };
};

// -- Node attributes (curves/surfaces)

typedef enum ufbx_nurbs_edges {
	UFBX_NURBS_PERIODIC,
	UFBX_NURBS_CLOSED,
	UFBX_NURBS_OPEN,
} ufbx_nurbs_edges;

typedef struct ufbx_nurbs_topology {
	uint32_t dimension;
	uint32_t order;
	ufbx_nurbs_edges edges;
	ufbx_real_list knot_vector;
} ufbx_nurbs_topology;

struct ufbx_line_curve {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };
};

struct ufbx_nurbs_curve {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };

	ufbx_nurbs_topology topology;
	ufbx_vec4_list control_points;
};

struct ufbx_patch_surface {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };
};

struct ufbx_nurbs_surface {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };

	ufbx_nurbs_topology topology[2];
	ufbx_vec4_list control_points;

	// TODO: Materials
};

struct ufbx_nurbs_trim_surface {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };
};

struct ufbx_nurbs_trim_boundary {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };
};

// -- Node attributes (advanced)

struct ufbx_procedural_geometry {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };
};

struct ufbx_camera_stereo {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };

	ufbx_camera *left;
	ufbx_camera *right;
};

struct ufbx_camera_switcher {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };
};

struct ufbx_lod_group {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; ufbx_node_list instances; }; };
};

// -- Deformers

// Method to evaluate the skinning on a per-vertex level
typedef enum ufbx_skinning_method {
	// Linear blend skinning: Blend transformation matrices by vertex weights
	UFBX_SKINNING_LINEAR,
	// One vertex should have only one bone attached
	UFBX_SKINNING_RIGID,
	// Convert the transformations to dual quaternions and blend in that space
	UFBX_SKINNING_DUAL_QUATERNION,
	// Blend between `UFBX_SKINNING_LINEAR` and `UFBX_SKINNING_BLENDED_DQ_LINEAR`
	// The blend weight can be found either per-vertex in `ufbx_skin_vertex.dq_weight`
	// or in `ufbx_skin_deformer.dq_vertices/dq_weights` (indexed by vertex).
	UFBX_SKINNING_BLENDED_DQ_LINEAR,
} ufbx_skinning_method;

// Skin weight information for a single mesh vertex
typedef struct ufbx_skin_vertex {

	// Each vertex is influenced by weights from `ufbx_skin_deformer.weights[]`
	// The weights are sorted by decreasing weight so you can take the first N
	// weights to get a cheaper approximation of the vertex.
	// NOTE: The weights are not guaranteed to be normalized!
	uint32_t weight_begin; // < Index to start from in the `weights[]` array
	uint32_t num_weights; // < Number of weights influencing the vertex

	// Blend weight between Linear Blend Skinning (0.0) and Dual Quaternion (1.0)
	ufbx_real dq_weight;

} ufbx_skin_vertex;

UFBX_LIST_TYPE(ufbx_skin_vertex_list, ufbx_skin_vertex);

// Single per-vertex per-cluster weight, see `ufbx_skin_vertex`
typedef struct ufbx_skin_weight {
	uint32_t cluster_index; // < Index into `ufbx_skin_deformer.clusters[]`
	ufbx_real weight;       // < Amount this bone influence the vertex
} ufbx_skin_weight;

UFBX_LIST_TYPE(ufbx_skin_weight_list, ufbx_skin_weight);

// Skin deformer specifies a binding between a logical set of bones (a skeleton)
// and a mesh. Each bone is represented by a `ufbx_skin_cluster` that contains
// the binding matrix and a `ufbx_node *bone` that has the current transformation.
struct ufbx_skin_deformer {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_skinning_method skinning_method;

	// Clusters (bones) in the skin
	ufbx_skin_cluster_list clusters;

	// Per-vertex weight information
	ufbx_skin_vertex_list vertices;
	ufbx_skin_weight_list weights;

	// Largest amount of weights a single vertex can have
	size_t max_weights_per_vertex;

	// Blend weights between Linear Blend Skinning (0.0) and Dual Quaternion (1.0)
	// Should be used if `skinning_method == UFBX_SKINNING_BLENDED_DQ_LINEAR`
	size_t num_dq_weights;
	int32_t *dq_vertices;
	ufbx_real *dq_weights;
};

// Cluster of vertices bound to a single bone.
struct ufbx_skin_cluster {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// The bone node the cluster is attached to (may be `NULL`!)
	// TODO: Strict mode where `bone` cannot be `NULL`!
	ufbx_node *bone;

	// Binding matrix from local mesh vertices to the bone
	ufbx_matrix geometry_to_bone; 

	// Matrix that specifies the rest/bind pose transform of the node,
	// not generally needed for skinning, use `geometry_to_bone` instead.
	ufbx_matrix bind_to_world;

	// Precomputed matrix/transform that accounts for the current bbone transform
	// ie. `ufbx_matrix_mul(&cluster->bone->node_to_world, &cluster->geometry_to_bone)`
	ufbx_matrix geometry_to_world;
	ufbx_transform geometry_to_world_transform;

	// Raw weights indexed by each _vertex_ of a mesh (not index!)
	// HINT: It may be simpler to use `ufbx_skin_deformer.vertices[]/weights[]` instead!
	size_t num_weights; // < Number of vertices in the cluster
	int32_t *vertices;  // < Vertex indices in `ufbx_mesh.vertices[]`
	ufbx_real *weights; // < Per-vertex weight values
};

// Blend shape deformer can contain multiple channels (think of sliders between morphs)
// that may optionally have in-between keyframes.
struct ufbx_blend_deformer {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_blend_channel_list channels;
};

// Blend shape associated with a target weight in a series of morphs
typedef struct ufbx_blend_keyframe {
	// May be NULL! TODO: Strict mode...
	ufbx_blend_shape *shape;

	// Weight value at which to apply the keyframe at full strength
	ufbx_real target_weight;

	// The weight the shape should be currently applied with
	ufbx_real effective_weight;
} ufbx_blend_keyframe;

UFBX_LIST_TYPE(ufbx_blend_keyframe_list, ufbx_blend_keyframe);

// Blend channel consists of multiple morph-key targets that are interpolated.
// In simple cases there will be only one keyframe that is the target shape.
struct ufbx_blend_channel {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Current weight of the channel
	ufbx_real weight;

	ufbx_blend_keyframe_list keyframes;
};

// Blend shape target containing the actual vertex offsets
struct ufbx_blend_shape {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Vertex offsets to apply over the base mesh
	size_t num_offsets;          // < Number of vertex offsets in the following arrays
	int32_t *offset_vertices;    // < Indices to `ufbx_mesh.vertices[]`
	ufbx_vec3 *position_offsets; // < Always specified per-vertex offsets
	ufbx_vec3 *normal_offsets;   // < `NULL` if not specified
};

struct ufbx_cache_deformer {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };
};

// -- Materials

// Material property, either specified with a constant value or a mapped texture
typedef struct ufbx_material_map {
	// Constant value, defined if `has_value == true`, for scalar values use `value.x`
	bool has_value;
	ufbx_vec3 value;
	int64_t value_int;

	// Texture if connected, otherwise `NULL`
	ufbx_texture *texture;
} ufbx_material_map;

// Texture attached to an FBX property
typedef struct ufbx_material_texture {
	ufbx_string material_prop; // < Name of the property in `ufbx_material.props`
	ufbx_string shader_prop;   // < Shader-specific property mapping name
	ufbx_texture *texture;     // < The attached texture
} ufbx_material_texture;

UFBX_LIST_TYPE(ufbx_material_texture_list, ufbx_material_texture);

// Shading model type
typedef enum ufbx_shader_type {
	// Unknown shading model
	UFBX_SHADER_UNKNOWN,
	// FBX builtin diffuse material
	UFBX_SHADER_FBX_LAMBERT,
	// FBX builtin diffuse+specular material
	UFBX_SHADER_FBX_PHONG,
	// Open Shading Language standard surface
	// https://github.com/Autodesk/standard-surface
	UFBX_SHADER_OSL_STANDARD,
	// Arnold standard surface
	// https://docs.arnoldrenderer.com/display/A5AFMUG/Standard+Surface
	UFBX_SHADER_ARNOLD,
	// Variation of the FBX phong shader that can recover PBR properties like
	// `metallic` or `roughness` from the FBX non-physical values.
	UFBX_SHADER_BLENDER_PHONG,

	UFBX_NUM_SHADER_TYPES,
} ufbx_shader_type;

// FBX builtin material properties, matches maps in `ufbx_material_fbx_maps`
typedef enum ufbx_material_fbx_map {
	UFBX_MATERIAL_FBX_DIFFUSE_FACTOR,
	UFBX_MATERIAL_FBX_DIFFUSE_COLOR,
	UFBX_MATERIAL_FBX_SPECULAR_FACTOR,
	UFBX_MATERIAL_FBX_SPECULAR_COLOR,
	UFBX_MATERIAL_FBX_SPECULAR_EXPONENT,
	UFBX_MATERIAL_FBX_REFLECTION_FACTOR,
	UFBX_MATERIAL_FBX_REFLECTION_COLOR,
	UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR,
	UFBX_MATERIAL_FBX_TRANSPARENCY_COLOR,
	UFBX_MATERIAL_FBX_EMISSION_FACTOR,
	UFBX_MATERIAL_FBX_EMISSION_COLOR,
	UFBX_MATERIAL_FBX_AMBIENT_FACTOR,
	UFBX_MATERIAL_FBX_AMBIENT_COLOR,
	UFBX_MATERIAL_FBX_NORMAL_MAP,
	UFBX_MATERIAL_FBX_BUMP,
	UFBX_MATERIAL_FBX_BUMP_FACTOR,
	UFBX_MATERIAL_FBX_DISPLACEMENT_FACTOR,
	UFBX_MATERIAL_FBX_DISPLACEMENT,
	UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT_FACTOR,
	UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT,
	UFBX_NUM_MATERIAL_FBX_MAPS,
} ufbx_material_fbx_map;

// Known PBR material properties, matches maps in `ufbx_material_pbr_maps`
typedef enum ufbx_material_pbr_map {
	UFBX_MATERIAL_PBR_BASE_FACTOR,
	UFBX_MATERIAL_PBR_BASE_COLOR,
	UFBX_MATERIAL_PBR_ROUGHNESS,
	UFBX_MATERIAL_PBR_METALLIC,
	UFBX_MATERIAL_PBR_DIFFUSE_ROUGHNESS,
	UFBX_MATERIAL_PBR_SPECULAR_FACTOR,
	UFBX_MATERIAL_PBR_SPECULAR_COLOR,
	UFBX_MATERIAL_PBR_SPECULAR_IOR,
	UFBX_MATERIAL_PBR_SPECULAR_ANISOTROPY,
	UFBX_MATERIAL_PBR_SPECULAR_ROTATION,
	UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR,
	UFBX_MATERIAL_PBR_TRANSMISSION_COLOR,
	UFBX_MATERIAL_PBR_TRANSMISSION_DEPTH,
	UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER,
	UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER_ANISOTROPY,
	UFBX_MATERIAL_PBR_TRANSMISSION_DISPERSION,
	UFBX_MATERIAL_PBR_TRANSMISSION_ROUGHNESS,
	UFBX_MATERIAL_PBR_TRANSMISSION_PRIORITY,
	UFBX_MATERIAL_PBR_TRANSMISSION_ENABLE_IN_AOV,
	UFBX_MATERIAL_PBR_SUBSURFACE_FACTOR,
	UFBX_MATERIAL_PBR_SUBSURFACE_COLOR,
	UFBX_MATERIAL_PBR_SUBSURFACE_RADIUS,
	UFBX_MATERIAL_PBR_SUBSURFACE_SCALE,
	UFBX_MATERIAL_PBR_SUBSURFACE_ANISOTROPY,
	UFBX_MATERIAL_PBR_SUBSURFACE_TYPE,
	UFBX_MATERIAL_PBR_SHEEN_FACTOR,
	UFBX_MATERIAL_PBR_SHEEN_COLOR,
	UFBX_MATERIAL_PBR_SHEEN_ROUGHNESS,
	UFBX_MATERIAL_PBR_COAT_FACTOR,
	UFBX_MATERIAL_PBR_COAT_COLOR,
	UFBX_MATERIAL_PBR_COAT_ROUGHNESS,
	UFBX_MATERIAL_PBR_COAT_IOR,
	UFBX_MATERIAL_PBR_COAT_ANISOTROPY,
	UFBX_MATERIAL_PBR_COAT_ROTATION,
	UFBX_MATERIAL_PBR_COAT_NORMAL,
	UFBX_MATERIAL_PBR_THIN_FILM_THICKNESS,
	UFBX_MATERIAL_PBR_THIN_FILM_IOR,
	UFBX_MATERIAL_PBR_EMISSION_FACTOR,
	UFBX_MATERIAL_PBR_EMISSION_COLOR,
	UFBX_MATERIAL_PBR_OPACITY,
	UFBX_MATERIAL_PBR_INDIRECT_DIFFUSE,
	UFBX_MATERIAL_PBR_INDIRECT_SPECULAR,
	UFBX_MATERIAL_PBR_NORMAL_MAP,
	UFBX_MATERIAL_PBR_TANGENT_MAP,
	UFBX_MATERIAL_PBR_MATTE_ENABLED,
	UFBX_MATERIAL_PBR_MATTE_FACTOR,
	UFBX_MATERIAL_PBR_MATTE_COLOR,
	UFBX_MATERIAL_PBR_THIN_WALLED,
	UFBX_MATERIAL_PBR_CAUSTICS,
	UFBX_MATERIAL_PBR_EXIT_TO_BACKGROUND,
	UFBX_MATERIAL_PBR_INTERNAL_REFLECTIONS,
	UFBX_NUM_MATERIAL_PBR_MAPS,
} ufbx_material_pbr_map;

typedef struct ufbx_material_fbx_maps {
	union {
		ufbx_material_map maps[UFBX_NUM_MATERIAL_FBX_MAPS];
		struct {
			ufbx_material_map diffuse_factor;
			ufbx_material_map diffuse_color;
			ufbx_material_map specular_factor;
			ufbx_material_map specular_color;
			ufbx_material_map specular_exponent;
			ufbx_material_map reflection_factor;
			ufbx_material_map reflection_color;
			ufbx_material_map transparency_factor;
			ufbx_material_map transparency_color;
			ufbx_material_map emission_factor;
			ufbx_material_map emission_color;
			ufbx_material_map ambient_factor;
			ufbx_material_map ambient_color;
			ufbx_material_map normal_map;
			ufbx_material_map bump;
			ufbx_material_map bump_factor;
			ufbx_material_map displacement_factor;
			ufbx_material_map displacement;
			ufbx_material_map vector_displacement_factor;
			ufbx_material_map vector_displacement;
		};
	};
} ufbx_material_fbx_maps;

typedef struct ufbx_material_pbr_maps {
	union {
		ufbx_material_map maps[UFBX_NUM_MATERIAL_PBR_MAPS];
		struct {
			ufbx_material_map base_factor;
			ufbx_material_map base_color;
			ufbx_material_map roughness;
			ufbx_material_map metallic;
			ufbx_material_map diffuse_roughness;
			ufbx_material_map specular_factor;
			ufbx_material_map specular_color;
			ufbx_material_map specular_ior;
			ufbx_material_map specular_anisotropy;
			ufbx_material_map specular_rotation;
			ufbx_material_map transmission_factor;
			ufbx_material_map transmission_color;
			ufbx_material_map transmission_depth;
			ufbx_material_map transmission_scatter;
			ufbx_material_map transmission_scatter_anisotropy;
			ufbx_material_map transmission_dispersion;
			ufbx_material_map transmission_roughness;
			ufbx_material_map transmission_priority;
			ufbx_material_map transmission_enable_in_aov;
			ufbx_material_map subsurface_factor;
			ufbx_material_map subsurface_color;
			ufbx_material_map subsurface_radius;
			ufbx_material_map subsurface_scale;
			ufbx_material_map subsurface_anisotropy;
			ufbx_material_map subsurface_type;
			ufbx_material_map sheen_factor;
			ufbx_material_map sheen_color;
			ufbx_material_map sheen_roughness;
			ufbx_material_map coat_factor;
			ufbx_material_map coat_color;
			ufbx_material_map coat_roughness;
			ufbx_material_map coat_ior;
			ufbx_material_map coat_anisotropy;
			ufbx_material_map coat_rotation;
			ufbx_material_map coat_normal;
			ufbx_material_map thin_film_thickness;
			ufbx_material_map thin_film_ior;
			ufbx_material_map emission_factor;
			ufbx_material_map emission_color;
			ufbx_material_map opacity;
			ufbx_material_map indirect_diffuse;
			ufbx_material_map indirect_specular;
			ufbx_material_map normal_map;
			ufbx_material_map tangent_map;
			ufbx_material_map matte_enabled;
			ufbx_material_map matte_factor;
			ufbx_material_map matte_color;
			ufbx_material_map thin_walled;
			ufbx_material_map caustics;
			ufbx_material_map exit_to_background;
			ufbx_material_map internal_reflections;
		};
	};
} ufbx_material_pbr_maps;

// Surface material properties such as color, roughness, etc. Each property may
// be optionally bound to an `ufbx_texture`.
struct ufbx_material {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// FBX builtin properties
	// NOTE: These may be empty if the material is using a custom shader
	ufbx_material_fbx_maps fbx;

	// PBR material properties, defined for all shading models but may be
	// somewhat approximate if `shader == NULL`.
	ufbx_material_pbr_maps pbr;

	// Shading information
	ufbx_shader_type shader_type;   // < Always defined
	ufbx_shader *shader;            // < Optional extended shader information
	ufbx_string shading_model_name; // < Often one of `{ "lambert", "phong", "unknown" }`

	// All textures attached to the material, if you want specific maps if might be
	// more convenient to use eg. `fbx.diffuse_color.texture` or `pbr.base_color.texture`
	ufbx_material_texture_list textures; // < Sorted by `material_prop`
};

// Texture that controls material appearance
struct ufbx_texture {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Paths to the resource
	ufbx_string filename;
	ufbx_string relative_filename;

	// Optional embedded content blob, eg. raw .png format data
	const void *content;
	size_t content_size;

	// Optional video texture
	ufbx_video *video;
};

// TODO: Video textures
struct ufbx_video {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Paths to the resource
	ufbx_string filename;
	ufbx_string relative_filename;

	// Optional embedded content blob
	const void *content;
	size_t content_size;
};

// Shader specifies a shading model and contains `ufbx_shader_binding` elements
// that define how to interpret FBX properties in the shader.
struct ufbx_shader {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	// Known shading model
	ufbx_shader_type type;

	// TODO: Expose actual properties here

	// Bindings from FBX properties to the shader
	// HINT: `ufbx_find_shader_prop()` translates shader properties to FBX properties
	ufbx_shader_binding_list bindings;
};

// Binding from a material property to shader implementation
typedef struct ufbx_shader_prop_binding {
	ufbx_string shader_prop;   // < Property name used by the shader implementation
	ufbx_string material_prop; // < Property name inside `ufbx_material.props`
} ufbx_shader_prop_binding;

UFBX_LIST_TYPE(ufbx_shader_prop_binding_list, ufbx_shader_prop_binding);

// Shader binding table
struct ufbx_shader_binding {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_shader_prop_binding_list prop_bindings; // < Sorted by `shader_prop`
};

// -- Animation

typedef struct ufbx_anim_layer_desc {
	ufbx_anim_layer *layer;
	ufbx_real weight;
} ufbx_anim_layer_desc;

UFBX_LIST_TYPE(ufbx_anim_layer_desc_list, ufbx_anim_layer_desc);

typedef struct ufbx_anim {
	ufbx_anim_layer_desc_list layers;
	bool ignore_connections;
} ufbx_anim;

struct ufbx_anim_stack {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	double time_begin;
	double time_end;

	ufbx_anim_layer_list layers;
	ufbx_anim anim;
};

typedef struct ufbx_anim_prop {
	ufbx_element *element;
	uint32_t internal_key;
	ufbx_string prop_name;
	ufbx_anim_value *anim_value;
} ufbx_anim_prop;

UFBX_LIST_TYPE(ufbx_anim_prop_list, ufbx_anim_prop);

struct ufbx_anim_layer {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_real weight;
	bool weight_is_animated;
	bool blended;
	bool additive;
	bool compose_rotation;
	bool compose_scale;

	ufbx_anim_value_list anim_values;
	ufbx_anim_prop_list anim_props; // < Sorted by `element,prop_name`
};

struct ufbx_anim_value {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	ufbx_vec3 default_value;
	ufbx_anim_curve *curves[3];
};

// Animation curve segment interpolation mode between two keyframes
typedef enum ufbx_interpolation {
	UFBX_INTERPOLATION_CONSTANT_PREV, // < Hold previous key value
	UFBX_INTERPOLATION_CONSTANT_NEXT, // < Hold next key value
	UFBX_INTERPOLATION_LINEAR,        // < Linear interpolation between two keys
	UFBX_INTERPOLATION_CUBIC,         // < Cubic interpolation, see `ufbx_tangent`
} ufbx_interpolation;

// Tangent vector at a keyframe, may be split into left/right
typedef struct ufbx_tangent {
	float dx; // < Derivative in the time axis
	float dy; // < Derivative in the (curve specific) value axis
} ufbx_tangent;

// Single real `value` at a specified `time`, interpolation between two keyframes
// is determined by the `interpolation` field of the _previous_ key.
// If `interpolation == UFBX_INTERPOLATION_CUBIC` the span is evaluated as a
// cubic bezier curve through the following points:
//
//   (prev->time, prev->value)
//   (prev->time + prev->dx, prev->value + prev->dy)
//   (next->time - next->dx, next->value - next->dy)
//   (next->time, next->value)
//
// HINT: Use `ufbx_evaluate_curve(ufbx_anim_curve *curve, double time)` rather
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

// -- Miscellaneous

typedef struct ufbx_bone_pose {
	ufbx_node *bone;
	ufbx_matrix bone_to_world;
} ufbx_bone_pose;

UFBX_LIST_TYPE(ufbx_bone_pose_list, ufbx_bone_pose);

struct ufbx_pose {
	union { ufbx_element element; struct { ufbx_string name; ufbx_props props; }; };

	bool bind_pose;
	ufbx_bone_pose_list bone_poses;
};

typedef struct ufbx_name_element {
	ufbx_string name;
	ufbx_element_type type;
	uint32_t internal_key;
	ufbx_element *element;
} ufbx_name_element;

UFBX_LIST_TYPE(ufbx_name_element_list, ufbx_name_element);

// -- Scene

// Scene is the root object loaded by ufbx that everything is accessed from.

typedef struct ufbx_scene ufbx_scene;

typedef enum ufbx_exporter {
	UFBX_EXPORTER_UNKNOWN,
	UFBX_EXPORTER_FBX_SDK,
	UFBX_EXPORTER_BLENDER_BINARY,
	UFBX_EXPORTER_BLENDER_ASCII,
} ufbx_exporter;

// Miscellaneous data related to the loaded file
typedef struct ufbx_metadata {
	bool ascii;
	uint32_t version;
	ufbx_string creator;

	ufbx_exporter exporter;
	uint32_t exporter_version;

	bool geometry_ignored;
	bool animation_ignored;
	bool embedded_ignored;

	size_t result_memory_used;
	size_t temp_memory_used;
	size_t result_allocs;
	size_t temp_allocs;

	size_t element_buffer_size;

	ufbx_real bone_prop_size_unit;
	bool bone_prop_limb_length_relative;
	double ktime_to_sec;
} ufbx_metadata;

typedef enum ufbx_coordinate_axis {
	UFBX_COORDINATE_AXIS_POSITIVE_X,
	UFBX_COORDINATE_AXIS_NEGATIVE_X,
	UFBX_COORDINATE_AXIS_POSITIVE_Y,
	UFBX_COORDINATE_AXIS_NEGATIVE_Y,
	UFBX_COORDINATE_AXIS_POSITIVE_Z,
	UFBX_COORDINATE_AXIS_NEGATIVE_Z,
	UFBX_COORDINATE_AXIS_UNKNOWN,
} ufbx_coordinate_axis;

typedef enum ufbx_time_mode {
	UFBX_TIME_MODE_DEFAULT,
	UFBX_TIME_MODE_120_FPS,
	UFBX_TIME_MODE_100_FPS,
	UFBX_TIME_MODE_60_FPS,
	UFBX_TIME_MODE_50_FPS,
	UFBX_TIME_MODE_48_FPS,
	UFBX_TIME_MODE_30_FPS,
	UFBX_TIME_MODE_30_FPS_DROP,
	UFBX_TIME_MODE_NTSC_DROP_FRAME,
	UFBX_TIME_MODE_NTSC_FULL_FRAME,
	UFBX_TIME_MODE_PAL,
	UFBX_TIME_MODE_24_FPS,
	UFBX_TIME_MODE_1000_FPS,
	UFBX_TIME_MODE_FILM_FULL_FRAME,
	UFBX_TIME_MODE_CUSTOM,
	UFBX_TIME_MODE_96_FPS,
	UFBX_TIME_MODE_72_FPS,
	UFBX_TIME_MODE_59_94_FPS,
} ufbx_time_mode;

typedef enum ufbx_time_protocol {
	UFBX_TIME_PROTOCOL_SMPTE,
	UFBX_TIME_PROTOCOL_FRAME_COUNT,
	UFBX_TIME_PROTOCOL_DEFAULT,
} ufbx_time_protocol;

typedef enum ufbx_snap_mode {
	UFBX_SNAP_MODE_NONE,
	UFBX_SNAP_MODE_SNAP,
	UFBX_SNAP_MODE_PLAY,
	UFBX_SNAP_MODE_SNAP_AND_PLAY,
} ufbx_snap_mode;

// Global settings: Axes and time/unit scales
typedef struct ufbx_scene_settings {
	ufbx_props props;

	ufbx_coordinate_axis axis_right;
	ufbx_coordinate_axis axis_up;
	ufbx_coordinate_axis axis_front;

	ufbx_real unit_scale_factor;
	ufbx_real frames_per_second;

	ufbx_vec3 ambient_color;
	ufbx_string default_camera;

	ufbx_time_mode time_mode;
	ufbx_time_protocol time_protocol;
	ufbx_snap_mode snap_mode;

	// Original settings (?)
	ufbx_coordinate_axis original_axis_up;
	ufbx_real original_unit_scale_factor;
} ufbx_scene_settings;

struct ufbx_scene {
	ufbx_metadata metadata;

	// Global settings
	ufbx_scene_settings settings;

	// Node instances in the scene
	ufbx_node *root_node;

	// Default animation descriptor
	ufbx_anim anim;

	union {
		struct {
			ufbx_unknown_list unknowns;

			// Nodes
			ufbx_node_list nodes;

			// Node attributes (common)
			ufbx_mesh_list meshes;
			ufbx_light_list lights;
			ufbx_camera_list cameras;
			ufbx_bone_list bones;
			ufbx_empty_list empties;

			// Node attributes (curves/surfaces)
			ufbx_line_curve_list line_curves;
			ufbx_nurbs_curve_list nurbs_curves;
			ufbx_patch_surface_list patch_surfaces;
			ufbx_nurbs_surface_list nurbs_surfaces;
			ufbx_nurbs_trim_surface_list nurbs_trim_surfaces;
			ufbx_nurbs_trim_boundary_list nurbs_trim_boundaries;

			// Node attributes (advanced)
			ufbx_procedural_geometry_list procedural_geometries;
			ufbx_camera_stereo_list camera_stereos;
			ufbx_camera_switcher_list camera_switchers;
			ufbx_lod_group_list lod_groups;

			// Deformers
			ufbx_skin_deformer_list skin_deformers;
			ufbx_skin_cluster_list skin_clusters;
			ufbx_blend_deformer_list blend_deformers;
			ufbx_blend_channel_list blend_channels;
			ufbx_blend_shape_list blend_shapes;
			ufbx_cache_deformer_list cache_deformers;

			// Materials
			ufbx_material_list materials;
			ufbx_texture_list textures;
			ufbx_video_list videos;
			ufbx_shader_list shaders;
			ufbx_shader_binding_list shader_bindings;

			// Animation
			ufbx_anim_stack_list anim_stacks;
			ufbx_anim_layer_list anim_layers;
			ufbx_anim_value_list anim_values;
			ufbx_anim_curve_list anim_curves;

			// Miscellaneous
			ufbx_pose_list poses;
		};

		ufbx_element_list elements_by_type[UFBX_NUM_ELEMENT_TYPES];
	};

	// All elements and connections in the whole file
	ufbx_element_list elements;           // < Sorted by `id`
	ufbx_connection_list connections_src; // < Sorted by `src,src_prop`
	ufbx_connection_list connections_dst; // < Sorted by `dst,dst_prop`

	// Elements sorted by name, type
	ufbx_name_element_list elements_by_name;
};

// -- Mesh topology

typedef enum ufbx_topo_flags {
	UFBX_TOPO_NON_MANIFOLD = 0x1, // < Edge with three or more faces
} ufbx_topo_flags;

typedef struct ufbx_topo_edge {
	int32_t index; // < Starting index of the edge
	int32_t next;  // < Ending index of the edge / next per-face `ufbx_topo_edge`, always defined
	int32_t prev;  // < Previous per-face `ufbx_topo_edge`, always defined
	int32_t twin;  // < `ufbx_topo_edge` on the opposite side, `-1` if not found
	int32_t face;  // < Index into `mesh->faces[]`, always defined
	int32_t edge;  // < Index into `mesh->edges[]`, `-1` if not found

	ufbx_topo_flags flags;
} ufbx_topo_edge;

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

// Free the allocator itself
typedef void ufbx_free_allocator_fn(void *user);

// Allocator callbacks and user context
// NOTE: The allocator will be stored to the loaded scene and will be called
// again from `ufbx_free_scene()` so make sure `user` outlives that!
typedef struct ufbx_allocator {

	// Callback functions, see `typedef`s above for information
	ufbx_alloc_fn *alloc_fn;
	ufbx_realloc_fn *realloc_fn;
	ufbx_free_fn *free_fn;
	ufbx_free_allocator_fn *free_allocator_fn;
	void *user;

	// Maximum number of bytes to allocate before failing
	size_t memory_limit;

	// Maximum number of allocations to attempt before failing
	size_t allocation_limit;

	// Threshold to swap from batched allocations to individual ones
	// NOTE: If set to `1` ufbx will allocate everything in the smallest
	// possible chunks which may be useful for debugging (eg. ASAN)
	size_t huge_threshold;

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

typedef enum ufbx_error_type {
	UFBX_ERROR_NONE,
	UFBX_ERROR_UNKNOWN,
	UFBX_ERROR_FILE_NOT_FOUND,
	UFBX_ERROR_OUT_OF_MEMORY,
	UFBX_ERROR_MEMORY_LIMIT,
	UFBX_ERROR_ALLOCATION_LIMIT,
	UFBX_ERROR_TRUNCATED_FILE,
	UFBX_ERROR_IO,
	UFBX_ERROR_CANCELLED,
} ufbx_error_type;

// Error description with detailed stack trace
// HINT: You can use `ufbx_format_error()` for formatting the error
typedef struct ufbx_error {
	ufbx_error_type type;
	const char *description;
	uint32_t stack_size;
	ufbx_error_frame stack[UFBX_ERROR_STACK_MAX_DEPTH];
} ufbx_error;

// -- Progress callbacks

typedef struct ufbx_progress {
	uint64_t bytes_read;
	uint64_t bytes_total;
} ufbx_progress;

// Called periodically with the current progress
// Return `false` to cancel further processing
typedef bool ufbx_progress_fn(void *user, const ufbx_progress *progress);

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

	// (optional) Progress reporting
	ufbx_progress_fn *progress_fn;
	void *progress_user;

	// (optional) Change the progress scope
	uint64_t progress_size_before;
	uint64_t progress_size_after;
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
	bool ignore_embedded;   // < Do not load embedded content
	bool evaluate_skinning; // < Evaluate skinning (see ufbx_mesh.skinned_vertices)

	// Don't compute `ufbx_skin_deformer` `vertices` and `weights` arrays saving
	// a bit of memory and time if not needed
	bool skip_skin_vertices;

	// Don't adjust reading the FBX file depending on the detected exporter
	bool disable_quirks;

	// Allow indices in `ufbx_vertex_TYPE` arrays that area larger than the data
	// array. Enabling this makes `ufbx_get_vertex_TYPE()` unsafe as they don't
	// do bounds checking.
	bool allow_out_of_bounds_vertex_indices;

	// Estimated file size for progress reporting
	uint64_t file_size_estimate;

	// Buffer size in bytes to use for reading from files or IO callbacks
	size_t read_buffer_size;

	// Progress reporting
	ufbx_progress_fn *progress_fn;
	void *progress_user;

} ufbx_load_opts;

typedef struct ufbx_evaluate_opts {

	ufbx_allocator temp_allocator;   // < Allocator used during evaluation
	ufbx_allocator result_allocator; // < Allocator used for the final scene
	bool evaluate_skinning; // < Evaluate skinning (see ufbx_mesh.skinned_vertices)

} ufbx_evaluate_opts;

typedef struct ufbx_subdivide_opts {

	ufbx_allocator temp_allocator;   // < Allocator used during evaluation
	ufbx_allocator result_allocator; // < Allocator used for the final scene

	ufbx_subdivision_boundary boundary;
	ufbx_subdivision_boundary uv_boundary;

	// Do not generate normals
	bool ignore_normals;

	// Interpolate existing normals using the subdivision rules
	// instead of generating new normals
	bool interpolate_normals;

	// Subdivide also tangent attributes
	bool interpolate_tangents;

} ufbx_subdivide_opts;

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
extern const size_t ufbx_element_type_size[UFBX_NUM_ELEMENT_TYPES];
extern const uint32_t ufbx_source_version;

// Load a scene from a `size` byte memory buffer at `data`
ufbx_scene *ufbx_load_memory(
	const void *data, size_t size,
	const ufbx_load_opts *opts, ufbx_error *error);

// Load a scene by opening a file named `filename`
ufbx_scene *ufbx_load_file(
	const char *filename,
	const ufbx_load_opts *opts, ufbx_error *error);
ufbx_scene *ufbx_load_file_len(
	const char *filename, size_t filename_len,
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

// Format a textual description of `error`. Always produces a NULL-terminated string
// to `char dst[dst_size]`, truncating if necessary. Returns the number of characters
// written not including the NULL terminator.
size_t ufbx_format_error(char *dst, size_t dst_size, const ufbx_error *error);

// Query

ufbx_prop *ufbx_find_prop_len(const ufbx_props *props, const char *name, size_t name_len);
ufbx_inline ufbx_prop *ufbx_find_prop(const ufbx_props *props, const char *name) { return ufbx_find_prop_len(props, name, strlen(name));}

ufbx_element *ufbx_find_element_len(ufbx_scene *scene, ufbx_element_type type, const char *name, size_t name_len);
ufbx_inline ufbx_element *ufbx_find_element(ufbx_scene *scene, ufbx_element_type type, const char *name) { return ufbx_find_element_len(scene, type, name, strlen(name));}

ufbx_node *ufbx_find_node_len(ufbx_scene *scene, const char *name, size_t name_len);
ufbx_inline ufbx_node *ufbx_find_node(ufbx_scene *scene, const char *name) { return ufbx_find_node_len(scene, name, strlen(name));}

ufbx_matrix ufbx_get_compatible_matrix_for_normals(ufbx_node *node);

// Utility

ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain);

// Animation evaluation

ufbx_real ufbx_evaluate_curve(const ufbx_anim_curve *curve, double time, ufbx_real default_value);

ufbx_real ufbx_evaluate_anim_value_real(const ufbx_anim_value *anim_value, double time);
ufbx_vec2 ufbx_evaluate_anim_value_vec2(const ufbx_anim_value *anim_value, double time);
ufbx_vec3 ufbx_evaluate_anim_value_vec3(const ufbx_anim_value *anim_value, double time);

ufbx_prop ufbx_evaluate_prop_len(ufbx_anim anim, const ufbx_element *element, const char *name, size_t name_len, double time);
ufbx_inline ufbx_prop ufbx_evaluate_prop(ufbx_anim anim, const ufbx_element *element, const char *name, double time) {
	return ufbx_evaluate_prop_len(anim, element, name, strlen(name), time);
}

ufbx_props ufbx_evaluate_props(ufbx_anim anim, ufbx_element *element, double time, ufbx_prop *buffer, size_t buffer_size);

ufbx_transform ufbx_evaluate_transform(ufbx_anim anim, const ufbx_node *node, double time);

// Evaluate the whole `scene` at a specific `time` in the animation `anim`.
// The returned scene behaves as if it had been exported at a specific time
// in the specified animation, except that animated elements' properties contain
// only the animated values, the original ones are in `props->defaults`.
//
// NOTE: The returned scene refers to the original `scene` so it cannot be
// freed until all evaluated scenes are freed.
ufbx_scene *ufbx_evaluate_scene(ufbx_scene *scene, ufbx_anim anim, double time, const ufbx_evaluate_opts *opts, ufbx_error *error);

// Materials

ufbx_texture *ufbx_find_prop_texture_len(const ufbx_material *material, const char *name, size_t name_len);
ufbx_inline ufbx_texture *ufbx_find_prop_texture(const ufbx_material *material, const char *name) {
	return ufbx_find_prop_texture_len(material, name, strlen(name));
}
ufbx_string ufbx_find_shader_prop_len(const ufbx_shader *shader, const char *name, size_t name_len);
ufbx_inline ufbx_string ufbx_find_shader_prop(const ufbx_shader *shader, const char *name) {
	return ufbx_find_shader_prop_len(shader, name, strlen(name));
}

// Math

ufbx_quat ufbx_quat_mul(ufbx_quat a, ufbx_quat b);
ufbx_quat ufbx_quat_normalize(ufbx_quat q);
ufbx_quat ufbx_quat_fix_antipodal(ufbx_quat q, ufbx_quat reference);
ufbx_quat ufbx_quat_slerp(ufbx_quat a, ufbx_quat b, ufbx_real t);
ufbx_vec3 ufbx_quat_rotate_vec3(ufbx_quat q, ufbx_vec3 v);
ufbx_vec3 ufbx_quat_to_euler(ufbx_quat q, ufbx_rotation_order order);
ufbx_quat ufbx_euler_to_quat(ufbx_vec3 v, ufbx_rotation_order order);

ufbx_matrix ufbx_matrix_mul(const ufbx_matrix *a, const ufbx_matrix *b);
ufbx_matrix ufbx_matrix_invert(const ufbx_matrix *m);
ufbx_matrix ufbx_matrix_for_normals(const ufbx_matrix *m);
ufbx_vec3 ufbx_transform_position(const ufbx_matrix *m, ufbx_vec3 v);
ufbx_vec3 ufbx_transform_direction(const ufbx_matrix *m, ufbx_vec3 v);
ufbx_matrix ufbx_transform_to_matrix(const ufbx_transform *t);
ufbx_transform ufbx_matrix_to_transform(const ufbx_matrix *m);

// Skinning

ufbx_matrix ufbx_get_skin_vertex_matrix(const ufbx_skin_deformer *skin, size_t vertex, const ufbx_matrix *fallback);

ufbx_vec3 ufbx_get_blend_shape_vertex_offset(const ufbx_blend_shape *shape, size_t vertex);
ufbx_vec3 ufbx_get_blend_vertex_offset(const ufbx_blend_deformer *blend, size_t vertex);

void ufbx_add_blend_shape_vertex_offsets(const ufbx_blend_shape *shape, ufbx_vec3 *vertices, size_t num_vertices, ufbx_real weight);
void ufbx_add_blend_vertex_offsets(const ufbx_blend_deformer *blend, ufbx_vec3 *vertices, size_t num_vertices, ufbx_real weight);

// Mesh Topology

size_t ufbx_triangulate_face(uint32_t *indices, size_t num_indices, const ufbx_mesh *mesh, ufbx_face face);

// Generate the half-edge representation of `mesh` to `topo[mesh->num_indices]`
void ufbx_compute_topology(const ufbx_mesh *mesh, ufbx_topo_edge *topo);

// Get the next/previous edge around a vertex
// NOTE: Does not return the half-edge on the opposite side (ie. `topo[index].twin`)
int32_t ufbx_topo_next_vertex_edge(const ufbx_topo_edge *topo, int32_t index);
int32_t ufbx_topo_prev_vertex_edge(const ufbx_topo_edge *topo, int32_t index);

ufbx_vec3 ufbx_get_weighted_face_normal(const ufbx_vertex_vec3 *positions, ufbx_face face);

size_t ufbx_generate_normal_mapping(const ufbx_mesh *mesh, ufbx_topo_edge *topo, int32_t *normal_indices);
void ufbx_compute_normals(const ufbx_mesh *mesh, const ufbx_vertex_vec3 *positions, int32_t *normal_indices, ufbx_vec3 *normals, size_t num_normals);

ufbx_mesh *ufbx_subdivide_mesh(const ufbx_mesh *mesh, size_t level, const ufbx_subdivide_opts *opts, ufbx_error *error);
void ufbx_free_mesh(ufbx_mesh *mesh);

// -- Inline API

ufbx_inline ufbx_real ufbx_get_vertex_real(const ufbx_vertex_real *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec2 ufbx_get_vertex_vec2(const ufbx_vertex_vec2 *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec3 ufbx_get_vertex_vec3(const ufbx_vertex_vec3 *v, size_t index) { return v->data[v->indices[index]]; }
ufbx_inline ufbx_vec4 ufbx_get_vertex_vec4(const ufbx_vertex_vec4 *v, size_t index) { return v->data[v->indices[index]]; }

#endif

#ifdef __cplusplus
}
#endif

