
UFBXT_FILE_TEST(zbrush_vertex_color)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->nodes.count == 2);
	ufbx_node *node = scene->nodes.data[1];
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	ufbxt_assert(mesh->vertex_color.exists);
	ufbxt_assert(mesh->vertex_color.unique_per_vertex);

	ufbxt_assert(mesh->num_vertices == 6);
	for (size_t i = 0; i < mesh->num_vertices; i++) {
		ufbx_vec3 pos = mesh->vertex_position.values.data[i];
		ufbx_vec4 color = ufbx_get_vertex_vec4(&mesh->vertex_color, mesh->vertex_first_index.data[i]);
		ufbx_vec4 ref = { 0.0f, 0.0f, 0.0f, 1.0f };

		pos.y -= 1.0f;

		if (pos.x < -0.5f) {
			ref.x = 1.0f;
		} else if (pos.x > 0.5f) {
			ref.y = ref.z = 1.0f;
		} else if (pos.y > 0.5f) {
			ref.y = 1.0f;
		} else if (pos.y < -0.5f) {
			ref.x = ref.z = 1.0f;
		} else if (pos.z > 0.5f) {
			ref.z = 1.0f;
		} else if (pos.z < -0.5f) {
			ref.x = ref.y = 1.0f;
		}

		ufbxt_assert_close_vec4(err, color, ref);
		ufbxt_assert_close_vec4(err, color, ref);
	}

}
#endif

UFBXT_FILE_TEST(synthetic_color_suzanne)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->nodes.count == 2);
	ufbx_node *node = scene->nodes.data[1];
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	ufbxt_assert(mesh->vertex_color.exists);
	ufbxt_assert(mesh->vertex_color.unique_per_vertex);

	ufbxt_assert(mesh->num_faces == 500);
	ufbxt_assert(mesh->num_triangles == 968);

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbx_vec3 position = ufbx_get_vertex_vec3(&mesh->vertex_position, i);
		ufbx_vec4 color = ufbx_get_vertex_vec4(&mesh->vertex_color, i);

		ufbx_vec3 col = { color.x, color.y, color.z };

		ufbx_vec3 ref;
		ref.x = ufbxt_clamp(position.x * 0.5f + 0.5f, 0.0f, 1.0f);
		ref.y = ufbxt_clamp(position.y * 0.5f + 0.5f, 0.0f, 1.0f);
		ref.z = ufbxt_clamp(position.z * 0.5f + 0.5f, 0.0f, 1.0f);

		ufbxt_assert_close_vec3_threshold(err, col, ref, (ufbx_real)(1.0/256.0));
	}
}
#endif
