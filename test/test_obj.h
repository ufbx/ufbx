
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

#if UFBXT_IMPL
static void ufbxt_check_obj_elements(ufbxt_diff_error *err, ufbx_scene *scene, int32_t v, int32_t vt, int32_t vn, int32_t vc, const char *name)
{
	ufbxt_hintf("name = \"%s\"", name);

	ufbx_node *node = ufbx_find_node(scene, name);
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_assert(!strcmp(mesh->name.data, name));

	ufbxt_assert(mesh->num_faces == 1);
	ufbxt_assert(mesh->num_triangles == 1);

	ufbx_face face = mesh->faces.data[0];
	ufbxt_assert(face.index_begin == 0);
	ufbxt_assert(face.num_indices == 3);

	if (v > 0) {
		ufbxt_assert(mesh->vertex_position.exists);
		ufbxt_assert(mesh->vertex_position.indices.count == 3);
		const ufbx_vec3 refs[] = {
			{ (ufbx_real)-v, 0.0f, (ufbx_real)(v - 1) },
			{ (ufbx_real)+v, 0.0f, (ufbx_real)(v - 1) },
			{ 0.0f, (ufbx_real)+v, (ufbx_real)(v - 1) },
		};
		for (size_t ix = 0; ix < 3; ix++) {
			ufbx_vec3 val = ufbx_get_vertex_vec3(&mesh->vertex_position, ix);
			ufbxt_assert_close_vec3(err, val, refs[ix]);
		}
	} else {
		ufbxt_assert(!mesh->vertex_position.exists);
	}

	if (vt > 0) {
		ufbxt_assert(mesh->vertex_uv.exists);
		ufbxt_assert(mesh->vertex_uv.indices.count == 3);
		const ufbx_vec2 refs[] = {
			{ 0.0f, 0.0f },
			{ (ufbx_real)vt, 0.0f },
			{ 0.0f, (ufbx_real)vt },
		};
		for (size_t ix = 0; ix < 3; ix++) {
			ufbx_vec2 val = ufbx_get_vertex_vec2(&mesh->vertex_uv, ix);
			ufbxt_assert_close_vec2(err, val, refs[ix]);
		}
	} else {
		ufbxt_assert(!mesh->vertex_uv.exists);
	}

	if (vn > 0) {
		ufbxt_assert(mesh->vertex_normal.exists);
		ufbxt_assert(mesh->vertex_normal.indices.count == 3);
		const ufbx_vec3 refs[] = {
			{ 0.0f, (ufbx_real)-vn, 0.0f },
			{ 0.0f, (ufbx_real)-vn, 0.0f },
			{ 0.0f, (ufbx_real)+vn, 0.0f },
		};
		for (size_t ix = 0; ix < 3; ix++) {
			ufbx_vec3 val = ufbx_get_vertex_vec3(&mesh->vertex_normal, ix);
			ufbxt_assert_close_vec3(err, val, refs[ix]);
		}
	} else {
		ufbxt_assert(!mesh->vertex_normal.exists);
	}

	if (vc > 0) {
		ufbxt_assert(mesh->vertex_color.exists);
		ufbxt_assert(mesh->vertex_color.indices.count == 3);
		const ufbx_vec4 refs[] = {
			{ (ufbx_real)vc, 0.0f, 0.0f, 1.0f },
			{ 0.0f, (ufbx_real)vc, 0.0f, 1.0f },
			{ 0.0f, 0.0f, (ufbx_real)vc, 1.0f },
		};
		for (size_t ix = 0; ix < 3; ix++) {
			ufbx_vec4 val = ufbx_get_vertex_vec4(&mesh->vertex_color, ix);
			ufbxt_assert_close_vec4(err, val, refs[ix]);
		}
	} else {
		ufbxt_assert(!mesh->vertex_color.exists);
	}
}
#endif

UFBXT_FILE_TEST(synthetic_mixed_attribs)
#if UFBXT_IMPL
{
	ufbxt_check_obj_elements(err, scene, 1, 0, 0, 0, "V");
	ufbxt_check_obj_elements(err, scene, 2, 1, 0, 0, "VT");
	ufbxt_check_obj_elements(err, scene, 3, 0, 1, 0, "VN");
	ufbxt_check_obj_elements(err, scene, 4, 2, 2, 0, "VTN");
	ufbxt_check_obj_elements(err, scene, 5, 0, 0, 1, "VC");
	ufbxt_check_obj_elements(err, scene, 6, 3, 0, 2, "VTC");
	ufbxt_check_obj_elements(err, scene, 7, 0, 3, 3, "VNC");
	ufbxt_check_obj_elements(err, scene, 8, 4, 4, 4, "VTNC");
}
#endif

UFBXT_FILE_TEST(synthetic_mixed_attribs_reverse)
#if UFBXT_IMPL
{
	ufbxt_check_obj_elements(err, scene, 1, 0, 0, 0, "V");
	ufbxt_check_obj_elements(err, scene, 2, 1, 0, 0, "VT");
	ufbxt_check_obj_elements(err, scene, 3, 0, 1, 0, "VN");
	ufbxt_check_obj_elements(err, scene, 4, 2, 2, 0, "VTN");
	ufbxt_check_obj_elements(err, scene, 5, 0, 0, 1, "VC");
	ufbxt_check_obj_elements(err, scene, 6, 3, 0, 2, "VTC");
	ufbxt_check_obj_elements(err, scene, 7, 0, 3, 3, "VNC");
	ufbxt_check_obj_elements(err, scene, 8, 4, 4, 4, "VTNC");
}
#endif

UFBXT_FILE_TEST(synthetic_mixed_attribs_reuse)
#if UFBXT_IMPL
{
	ufbxt_check_obj_elements(err, scene, 1, 0, 0, 0, "V");
	ufbxt_check_obj_elements(err, scene, 1, 1, 0, 0, "VT");
	ufbxt_check_obj_elements(err, scene, 1, 0, 1, 0, "VN");
	ufbxt_check_obj_elements(err, scene, 1, 1, 1, 0, "VTN");
	ufbxt_check_obj_elements(err, scene, 2, 0, 0, 1, "VC");
	ufbxt_check_obj_elements(err, scene, 2, 1, 0, 1, "VTC");
	ufbxt_check_obj_elements(err, scene, 2, 0, 1, 1, "VNC");
	ufbxt_check_obj_elements(err, scene, 2, 1, 1, 1, "VTNC");
}
#endif

