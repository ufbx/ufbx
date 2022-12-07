#undef UFBXT_TEST_GROUP
#define UFBXT_TEST_GROUP "topology"

#if UFBXT_IMPL
void ufbxt_check_generated_normals(ufbx_mesh *mesh, ufbxt_diff_error *err, size_t expected_normals)
{
	ufbx_topo_edge *topo = calloc(mesh->num_indices, sizeof(ufbx_topo_edge));
	ufbxt_assert(topo);

	ufbx_compute_topology(mesh, topo, mesh->num_indices);

	uint32_t *normal_indices = calloc(mesh->num_indices, sizeof(uint32_t));
	size_t num_normals = ufbx_generate_normal_mapping(mesh, topo, mesh->num_indices, normal_indices, mesh->num_indices, false);

	if (expected_normals > 0) {
		ufbxt_assert(num_normals == expected_normals);
	}

	ufbx_vec3 *normals = calloc(num_normals, sizeof(ufbx_vec3));
	ufbx_compute_normals(mesh, &mesh->vertex_position, normal_indices, mesh->num_indices, normals, num_normals);

	ufbx_vertex_vec3 new_normals = { 0 };
	new_normals.exists = true;
	new_normals.values.data = normals;
	new_normals.values.count = num_normals;
	new_normals.indices.data = normal_indices;
	new_normals.indices.count = mesh->num_indices;

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbx_vec3 fn = ufbx_get_vertex_vec3(&mesh->vertex_normal, i);
		ufbx_vec3 rn = ufbx_get_vertex_vec3(&new_normals, i);

		ufbxt_assert_close_vec3(err, fn, rn);
	}

	free(normals);
	free(normal_indices);
	free(topo);
}
#endif

UFBXT_FILE_TEST(maya_edge_smoothing)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_check_generated_normals(mesh, err, 16);
}
#endif

UFBXT_FILE_TEST(maya_no_smoothing)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_check_generated_normals(mesh, err, 16);
}
#endif

UFBXT_FILE_TEST(maya_planar_ngon)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pDisc1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_check_generated_normals(mesh, err, 0);
}
#endif

UFBXT_FILE_TEST(maya_subsurf_cube)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(maya_subsurf_plane)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(maya_subsurf_cube_crease)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(blender_293_suzanne_subsurf)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST_ALT(subsurf_alloc_fail, maya_subsurf_cube)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	for (size_t max_temp = 1; max_temp < 10000; max_temp++) {
		ufbx_subdivide_opts opts = { 0 };
		opts.temp_allocator.huge_threshold = 1;
		opts.temp_allocator.allocation_limit = max_temp;

		ufbxt_hintf("Temp limit: %zu", max_temp);

		ufbx_error error;
		ufbx_mesh *sub_mesh = ufbx_subdivide_mesh(mesh, 2, &opts, &error);
		if (sub_mesh) {
			ufbxt_logf(".. Tested up to %zu temporary allocations", max_temp);
			ufbx_free_mesh(sub_mesh);
			break;
		}
		ufbxt_assert(error.type == UFBX_ERROR_ALLOCATION_LIMIT);
	}

	for (size_t max_result = 1; max_result < 10000; max_result++) {
		ufbx_subdivide_opts opts = { 0 };
		opts.result_allocator.huge_threshold = 1;
		opts.result_allocator.allocation_limit = max_result;

		ufbxt_hintf("Result limit: %zu", max_result);

		ufbx_error error;
		ufbx_mesh *sub_mesh = ufbx_subdivide_mesh(mesh, 2, &opts, &error);
		if (sub_mesh) {
			ufbxt_logf(".. Tested up to %zu result allocations", max_result);
			ufbx_free_mesh(sub_mesh);
			break;
		}
		ufbxt_assert(error.type == UFBX_ERROR_ALLOCATION_LIMIT);
	}
}
#endif

UFBXT_FILE_TEST(blender_293_suzanne_subsurf_uv)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(blender_293x_nonmanifold_subsurf)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(blender_293_ngon_subsurf)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(blender_293x_subsurf_boundary)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(blender_293x_subsurf_max_crease)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = (ufbx_mesh*)ufbx_find_element(scene, UFBX_ELEMENT_MESH, "Plane");
	ufbx_mesh *subdivided = ufbx_subdivide_mesh(mesh, 1, NULL, NULL);

	for (size_t i = 0; i < mesh->num_edges; i++) {
		ufbxt_assert_close_real(err, mesh->edge_crease.data[i], 1.0f);
	}

	size_t num_edge = 0;
	size_t num_center = 0;
	for (size_t i = 0; i < subdivided->num_edges; i++) {
		ufbx_edge edge = subdivided->edges.data[i];
		ufbx_vec3 a = ufbx_get_vertex_vec3(&subdivided->vertex_position, edge.a);
		ufbx_vec3 b = ufbx_get_vertex_vec3(&subdivided->vertex_position, edge.b);
		ufbx_real a_len = a.x*a.x + a.y*a.y + a.z*a.z;
		ufbx_real b_len = b.x*b.x + b.y*b.y + b.z*b.z;

		if (a_len < 0.01f || b_len < 0.01f) {
			ufbxt_assert_close_real(err, subdivided->edge_crease.data[i], 0.0f);
			num_center++;
		} else {
			ufbxt_assert_close_real(err, subdivided->edge_crease.data[i], 1.0f);
			num_edge++;
		}
	}

	ufbxt_assert(num_edge == 8);
	ufbxt_assert(num_center == 4);

	ufbx_free_mesh(subdivided);
}
#endif

UFBXT_FILE_TEST(maya_subsurf_max_crease)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *subdivided = ufbx_subdivide_mesh(node->mesh, 1, NULL, NULL);
	ufbxt_assert(subdivided);

	size_t num_top = 0;
	size_t num_bottom = 0;
	for (size_t i = 0; i < subdivided->num_edges; i++) {
		ufbx_edge edge = subdivided->edges.data[i];
		ufbx_vec3 a = ufbx_get_vertex_vec3(&subdivided->vertex_position, edge.a);
		ufbx_vec3 b = ufbx_get_vertex_vec3(&subdivided->vertex_position, edge.b);
		ufbx_real a_len = a.x*a.x + a.z*a.z;
		ufbx_real b_len = b.x*b.x + b.z*b.z;

		if (a.y < -0.49f && b.y < -0.49f && a_len > 0.01f && b_len > 0.01f) {
			ufbxt_assert_close_real(err, subdivided->edge_crease.data[i], 0.8f);
			num_bottom++;
		} else if (a.y > +0.49f && b.y > +0.49f && a_len > 0.01f && b_len > 0.01f) {
			ufbxt_assert_close_real(err, subdivided->edge_crease.data[i], 1.0f);
			num_top++;
		} else {
			ufbxt_assert_close_real(err, subdivided->edge_crease.data[i], 0.0f);
		}
		a = a;
	}

	ufbxt_assert(num_top == 8);
	ufbxt_assert(num_bottom == 8);

	ufbx_free_mesh(subdivided);
}
#endif

UFBXT_FILE_TEST(maya_subsurf_3x_cube)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(maya_subsurf_3x_cube_crease)
#if UFBXT_IMPL
{
}
#endif

#if UFBXT_IMPL
typedef struct {
	ufbx_vec3 position;
	ufbx_vec3 normal;
} ufbxt_vertex_pn;
#endif

UFBXT_FILE_TEST(blender_293_half_smooth_cube)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Cube");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_assert(mesh->num_indices == 6*4);
	ufbxt_assert(mesh->num_triangles == 6*2);

	ufbxt_vertex_pn vertices[36];
	uint32_t indices[36];
	size_t num_indices = 0;

	uint32_t tri[64];
	for (size_t fi = 0; fi < mesh->num_faces; fi++) {
		size_t num_tris = ufbx_triangulate_face(tri, 64, mesh, mesh->faces.data[fi]);
		for (size_t ti = 0; ti < num_tris * 3; ti++) {
			vertices[num_indices].position = ufbx_get_vertex_vec3(&mesh->vertex_position, tri[ti]);
			vertices[num_indices].normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, tri[ti]);
			num_indices++;
		}
	}

	ufbx_vertex_stream stream = { vertices, sizeof(ufbxt_vertex_pn) };
	size_t num_vertices = ufbx_generate_indices(&stream, 1, indices, num_indices, NULL, NULL);
	ufbxt_assert(num_vertices == 12);

}
#endif

UFBXT_FILE_TEST(maya_vertex_crease_single)
#if UFBXT_IMPL
{
}
#endif


UFBXT_FILE_TEST(maya_vertex_crease)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(blender_312x_vertex_crease)
#if UFBXT_IMPL
{
}
#endif


