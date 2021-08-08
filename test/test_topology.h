
#if UFBXT_IMPL
void ufbxt_check_generated_normals(ufbx_mesh *mesh, ufbxt_diff_error *err, size_t expected_normals)
{
	ufbx_topo_index *indices = calloc(mesh->num_indices, sizeof(ufbx_topo_index));
	ufbx_topo_vertex *vertices = calloc(mesh->num_vertices, sizeof(ufbx_topo_index));
	ufbxt_assert(indices && vertices);

	ufbx_get_mesh_topology(mesh, indices, vertices);

	int32_t *normal_indices = calloc(mesh->num_indices, sizeof(int32_t));
	size_t num_normals = ufbx_generate_normal_mapping(mesh, indices, vertices, normal_indices);

	if (expected_normals > 0) {
		ufbxt_assert(num_normals == expected_normals);
	}

	ufbx_vec3 *normals = calloc(num_normals, sizeof(ufbx_vec3));
	ufbx_recompute_normals(mesh, normal_indices, normals, num_normals);

	ufbx_vertex_vec3 new_normals = { 0 };
	new_normals.data = normals;
	new_normals.by_index = normal_indices;
	new_normals.num_elements = num_normals;

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbx_vec3 fn = ufbx_get_by_index_vec3(&mesh->vertex_normal, i);
		ufbx_vec3 rn = ufbx_get_by_index_vec3(&new_normals, i);

		ufbxt_assert_close_vec3(err, fn, rn);
	}

	free(normals);
	free(normal_indices);
	free(indices);
	free(vertices);
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
