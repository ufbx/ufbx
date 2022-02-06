

#if UFBXT_IMPL
size_t do_triangulate_test(ufbx_scene *scene)
{
	size_t num_fail = 0;

	for (size_t mesh_ix = 0; mesh_ix < scene->meshes.count; mesh_ix++) {
		ufbx_mesh *mesh = scene->meshes.data[mesh_ix];
		ufbxt_assert(mesh->instances.count == 1);
		bool should_be_top_left = mesh->instances.data[0]->name.data[0] == 'A';
		ufbxt_assert(mesh->num_faces == 1);
		ufbx_face face = mesh->faces[0];
		ufbxt_assert(face.index_begin == 0);
		ufbxt_assert(face.num_indices == 4);
		uint32_t tris[6];
		bool ok = ufbx_triangulate_face(tris, 6, mesh, face);
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
			ufbxt_logf("Fail: %s", mesh->instances.data[0]->name.data);
			num_fail++;
		}
	}

	ufbxt_logf("Triangulations OK: %zu/%zu", scene->meshes.count - num_fail, scene->meshes.count);
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
	ufbx_node *node = ufbx_find_node(scene, "pCone1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	for (size_t i = 0; i < mesh->num_faces; i++) {
		ufbx_face face = mesh->faces[i];
		ufbxt_assert(face.num_indices >= 3 && face.num_indices <= 32);

		uint32_t tris[32];

		size_t num_tris = face.num_indices - 2;
		for (size_t i = 0; i < 32; i++) {
			bool ok = ufbx_triangulate_face(tris, i, mesh, face) != 0;
			ufbxt_assert(ok == (i >= num_tris * 3));
		}

		ufbxt_assert(ufbx_triangulate_face(tris, ufbxt_arraycount(tris), mesh, face));
	}
}
#endif

#if UFBXT_IMPL

static void ufbxt_ngon_write_obj(const char *path, ufbx_mesh *mesh, const uint32_t *indices, size_t num_triangles)
{
	FILE *f = fopen(path, "w");

	for (size_t i = 0; i < mesh->num_vertices; i++) {
		ufbx_vec3 v = mesh->vertices[i];
		fprintf(f, "v %f %f %f\n", v.x, v.y, v.z);
	}

	fprintf(f, "\n");
	for (size_t i = 0; i < num_triangles; i++) {
		const uint32_t *tri = indices + i * 3;
		fprintf(f, "f %u %u %u\n",
			mesh->vertex_indices[tri[0]] + 1,
			mesh->vertex_indices[tri[1]] + 1,
			mesh->vertex_indices[tri[2]] + 1);
	}

	fclose(f);
}

static void ufbxt_check_ngon_triangulation(ufbx_mesh *mesh, uint32_t *indices)
{
}

#endif

UFBXT_FILE_TEST(blender_300_ngon_intersection)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Plane");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	ufbxt_assert(mesh->num_faces == 1);

	uint32_t indices[3*3];
	size_t num_tris = ufbx_triangulate_face(indices, ufbxt_arraycount(indices), mesh, mesh->faces[0]);
	ufbxt_assert(num_tris == 3);
}
#endif

UFBXT_FILE_TEST(blender_300_ngon_e)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Plane");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	ufbxt_assert(mesh->num_faces == 1);

	uint32_t indices[10*3];
	size_t num_tris = ufbx_triangulate_face(indices, ufbxt_arraycount(indices), mesh, mesh->faces[0]);
	ufbxt_assert(num_tris == 10);
}
#endif

UFBXT_FILE_TEST(blender_300_ngon_abstract)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Plane");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	ufbxt_assert(mesh->num_faces == 1);

	uint32_t indices[144*3];
	size_t num_tris = ufbx_triangulate_face(indices, ufbxt_arraycount(indices), mesh, mesh->faces[0]);
	ufbxt_assert(num_tris == 144);
}
#endif

UFBXT_FILE_TEST(blender_300_ngon_big)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Plane");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	ufbxt_assert(mesh->num_faces == 1);

	uint32_t expected_tris = 8028;
	uint32_t *indices = malloc(expected_tris * 3 * sizeof(uint32_t));
	ufbxt_assert(indices);

	size_t num_tris = ufbx_triangulate_face(indices, expected_tris * 3, mesh, mesh->faces[0]);
	ufbxt_assert(num_tris == expected_tris);

	free(indices);
}
#endif
