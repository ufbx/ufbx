
#if UFBXT_IMPL
void ufbxt_check_generated_normals(ufbx_mesh *mesh, ufbxt_diff_error *err, size_t expected_normals)
{
	ufbx_topo_edge *topo = calloc(mesh->num_indices, sizeof(ufbx_topo_edge));
	ufbxt_assert(topo);

	ufbx_compute_topology(mesh, topo);

	int32_t *normal_indices = calloc(mesh->num_indices, sizeof(int32_t));
	size_t num_normals = ufbx_generate_normal_mapping(mesh, topo, normal_indices);

	if (expected_normals > 0) {
		ufbxt_assert(num_normals == expected_normals);
	}

	ufbx_vec3 *normals = calloc(num_normals, sizeof(ufbx_vec3));
	ufbx_compute_normals(mesh, &mesh->vertex_position, normal_indices, normals, num_normals);

	ufbx_vertex_vec3 new_normals = { 0 };
	new_normals.data = normals;
	new_normals.indices = normal_indices;
	new_normals.num_values = num_normals;

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
		ufbxt_assert_close_real(err, mesh->edge_crease[i], 1.0f);
	}

	size_t num_edge = 0;
	size_t num_center = 0;
	for (size_t i = 0; i < subdivided->num_edges; i++) {
		ufbx_edge edge = subdivided->edges[i];
		ufbx_vec3 a = ufbx_get_vertex_vec3(&subdivided->vertex_position, edge.indices[0]);
		ufbx_vec3 b = ufbx_get_vertex_vec3(&subdivided->vertex_position, edge.indices[1]);
		ufbx_real a_len = a.x*a.x + a.y*a.y + a.z*a.z;
		ufbx_real b_len = b.x*b.x + b.y*b.y + b.z*b.z;

		if (a_len < 0.01f || b_len < 0.01f) {
			ufbxt_assert_close_real(err, subdivided->edge_crease[i], 0.0f);
			num_center++;
		} else {
			ufbxt_assert_close_real(err, subdivided->edge_crease[i], 1.0f);
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
		ufbx_edge edge = subdivided->edges[i];
		ufbx_vec3 a = ufbx_get_vertex_vec3(&subdivided->vertex_position, edge.indices[0]);
		ufbx_vec3 b = ufbx_get_vertex_vec3(&subdivided->vertex_position, edge.indices[1]);
		ufbx_real a_len = a.x*a.x + a.z*a.z;
		ufbx_real b_len = b.x*b.x + b.z*b.z;

		if (a.y < -0.49f && b.y < -0.49f && a_len > 0.01f && b_len > 0.01f) {
			ufbxt_assert_close_real(err, subdivided->edge_crease[i], 0.8f);
			num_bottom++;
		} else if (a.y > +0.49f && b.y > +0.49f && a_len > 0.01f && b_len > 0.01f) {
			ufbxt_assert_close_real(err, subdivided->edge_crease[i], 1.0f);
			num_top++;
		} else {
			ufbxt_assert_close_real(err, subdivided->edge_crease[i], 0.0f);
		}
		a = a;
	}

	ufbxt_assert(num_top == 8);
	ufbxt_assert(num_bottom == 8);

	ufbx_free_mesh(subdivided);
}
#endif
