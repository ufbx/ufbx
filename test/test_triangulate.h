

#if UFBXT_IMPL
size_t do_triangulate_test(ufbx_scene *scene)
{
	size_t num_fail = 0;

	for (size_t mesh_ix = 0; mesh_ix < scene->meshes.size; mesh_ix++) {
		ufbx_mesh *mesh = &scene->meshes.data[mesh_ix];
		bool should_be_top_left = mesh->node.name.data[0] == 'A';
		ufbxt_assert(mesh->num_faces == 1);
		ufbx_face face = mesh->faces[0];
		ufbxt_assert(face.index_begin == 0);
		ufbxt_assert(face.num_indices == 4);
		uint32_t tris[6];
		bool ok = ufbx_triangulate(tris, 6, mesh, face);
		ufbxt_assert(ok);

		size_t top_left_ix = 0;
		ufbx_real best_dot = HUGE_VALF;
		for (size_t ix = 0; ix < 4; ix++) {
			ufbx_vec3 v = ufbx_get_vertex_vec3(&mesh->vertex_position, ix);
			ufbx_real dot = v.x + v.z;
			if (dot < best_dot) {
				top_left_ix = ix;
				best_dot = dot;
			}
		}

		uint32_t top_left_count = 0;
		for (size_t i = 0; i < 6; i++) {
			if (tris[i] == top_left_ix) top_left_count++;
		}

		if (should_be_top_left != (top_left_count == 2)) {
			ufbxt_logf("Fail: %s", mesh->node.name.data);
			num_fail++;
		}
	}

	ufbxt_logf("Triangulations OK: %zu/%zu", scene->meshes.size - num_fail, scene->meshes.size);
	return num_fail;
}
#endif

UFBXT_FILE_TEST(maya_triangulate)
#if UFBXT_IMPL
{
	size_t num_fail = do_triangulate_test(scene);
	ufbxt_assert(num_fail <= 4);
}
#endif

UFBXT_FILE_TEST(maya_triangulate_down)
#if UFBXT_IMPL
{
	size_t num_fail = do_triangulate_test(scene);
	ufbxt_assert(num_fail <= 1);
}
#endif

UFBXT_FILE_TEST(maya_tri_cone)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCone1");
	ufbxt_assert(mesh);

	for (size_t i = 0; i < mesh->num_faces; i++) {
		ufbx_face face = mesh->faces[i];
		ufbxt_assert(face.num_indices >= 3 && face.num_indices <= 32);

		uint32_t tris[32];

		size_t num_tris = face.num_indices - 2;
		for (size_t i = 0; i < 32; i++) {
			bool ok = ufbx_triangulate(tris, i, mesh, face);
			ufbxt_assert(ok == (i >= num_tris * 3));
		}

		ufbxt_assert(ufbx_triangulate(tris, ufbxt_arraycount(tris), mesh, face));
	}
}
#endif
