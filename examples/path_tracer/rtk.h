#ifndef RTK_H_INLCUDED
#define RTK_H_INLCUDED

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4201) // warning C4201:  nonstandard extension used: nameless struct/union
#endif

// -- Configuration

typedef float rtk_real;

#define RTK_INF (rtk_real)INFINITY
#define RTK_RAY_INF (rtk_real)INFINITY

#define RTK_HIT_MAX_PARENTS 4

// -- Common types

typedef enum rtk_hit_geometry {
	rtk_hit_triangle = -1,
	rtk_hit_sphere = -2,
	rtk_hit_plane = -3,
} rtk_hit_geometry;

typedef struct rtk_scene rtk_scene;

typedef struct rtk_vec2 {
	union {
		struct {
			rtk_real x, y;
		};
		rtk_real v[2];
	};
} rtk_vec2;

typedef struct rtk_vec3 {
	union {
		struct {
			rtk_real x, y, z;
		};
		rtk_real v[3];
	};
} rtk_vec3;

typedef struct rtk_bounds {
	rtk_vec3 min, max;
} rtk_bounds;

typedef struct {
	union {
		struct {
			rtk_real m00, m10, m20;
			rtk_real m01, m11, m21;
			rtk_real m02, m12, m22;
			rtk_real m03, m13, m23;
		};
		rtk_vec3 cols[4];
		rtk_real v[12];
	};
} rtk_matrix;

typedef struct {
	uintptr_t user;
	size_t index;
} rtk_object;

typedef struct rtk_ray {
	rtk_vec3 origin;
	rtk_vec3 direction;

	rtk_real min_t;
} rtk_ray;

typedef struct rtk_surface {
	rtk_real u, v;
	rtk_vec3 normal;
	rtk_vec3 dp_du, dp_dv;
} rtk_surface;

typedef struct rtk_hit {
	rtk_real t;
	rtk_surface	geom;
	rtk_surface	interp;
	void *user;
	rtk_object object;
	uint32_t vertex_index[3];
	rtk_vec3 vertex_pos[3];
	rtk_object parent_objects[RTK_HIT_MAX_PARENTS];
	uint32_t num_parents;
	int32_t geometry_type;
} rtk_hit;

typedef struct rtk_mesh {

	const rtk_vec3 *vertices;
	size_t vertices_stride;

	// Optional attributes
	const rtk_vec2 *uvs;
	const rtk_vec3 *normals;
	size_t uvs_stride;
	size_t normals_stride;

	const uint32_t *indices;
	size_t num_triangles;
	rtk_matrix transform;
	rtk_object object;
} rtk_mesh;

typedef struct rtk_triangle {
	rtk_vec3 v[3];
	rtk_object object;
} rtk_triangle;

typedef struct rtk_primitive rtk_primitive;

typedef int (*rtk_intersect_fn)(const rtk_primitive *p, const rtk_ray *ray, rtk_hit *hit);

struct rtk_primitive {
	// AABB in local space
	rtk_bounds bounds;

	// Intersection callback
	rtk_intersect_fn intersect_fn;
	void *user;

	// Transform from local to scene space
	rtk_matrix transform;

	rtk_object object;
};

typedef struct rtk_scene_desc {

	const rtk_mesh *meshes;
	size_t num_meshes;

	const rtk_triangle *triangles;
	size_t num_triangles;

	const rtk_primitive *primitives;
	size_t num_primitives;

} rtk_scene_desc;

typedef struct rtk_bvh {
	rtk_bounds bounds;  // < Bounds of the node
	uintptr_t child[4]; // < UINTPTR_MAX if leaf
	uintptr_t leaf;     // < Non-zero if leaf, zero if internal node
} rtk_bvh;

typedef struct rtk_leaf_triangle {
	rtk_vec3 v[3];
	uint32_t index[3];
	rtk_object object;
} rtk_leaf_triangle;

typedef struct rtk_leaf {
	rtk_leaf_triangle triangles[64];
	size_t num_triangles;

	rtk_primitive primitives[64];
	size_t num_primitives;
} rtk_leaf;

extern const rtk_matrix rtk_identity;

// Create a scene from `desc`
rtk_scene *rtk_create_scene(const rtk_scene_desc *desc);

// Free a scene returned by `rtk_create_scene()`
void rtk_free_scene(rtk_scene *s);

// Fire a ray into the scene.
// Returns 1 if the ray hit something and `hit` is valid, 0 otherwise.
// `max_t` is the maximum distance along the ray, use `RTK_RAY_INF` for unbounded.
// `hit` is not written to if the ray doesn't hit anything.
int rtk_raytrace(const rtk_scene *s, const rtk_ray *ray, rtk_hit *hit, rtk_real max_t);

// Fire multiple rays into the scene.
// `hit.t` must be set to the maximum distance for each ray.
void rtk_raytrace_many(const rtk_scene *s, const rtk_ray *rays, rtk_hit *hits, size_t num);

// Returns the amount of memory used by the scene in bytes.
size_t rtk_used_memory(const rtk_scene *s);

// Return the bounding AABB for the scene.
rtk_bounds rtk_scene_bounds(const rtk_scene *s);

// Retrieve the underlying BVH for visualization purposes.
// Call with `index=0` to obtain root, pass `rtk_bvh.child[]` to expand children.
rtk_bvh rtk_get_bvh(const rtk_scene *s, uintptr_t index);

// Retrieve a leaf from the BVH.
// Call with `rtk_bvh.leaf` returned from `rtk_get_bvh()`.
void rtk_get_leaf(const rtk_scene *s, uintptr_t index, rtk_leaf *leaf);

// Initialize `p` as an instance of `scene`.
// Only modifies `p->bounds`, `p->intersect_fn`, `p->transform`. Optional transform matrix `transform`.
void rtk_init_subscene(rtk_primitive *p, const rtk_scene *scene, const rtk_matrix *transform);

// Initialize `p` as a sphere at `origin` with `radius`.
// Only modifies `p->bounds`, `p->intersect_fn`, `p->transform`. Optional transform matrix `transform`.
void rtk_init_sphere(rtk_primitive *p, rtk_vec3 origin, rtk_real radius, const rtk_matrix *transform);

// Initialize `p` as an infinite plane facing `normal` at offset `d`.
// Only modifies `p->bounds` and `p->intersect_fn`, `p->transform`. Optional transform matrix `transform`.
void rtk_init_plane(rtk_primitive *p, rtk_vec3 normal, rtk_real d, const rtk_matrix *transform);

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif