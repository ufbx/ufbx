
UFBXT_FILE_TEST(blender_279_default)
#if UFBXT_IMPL
{
	ufbx_light *light = ufbx_find_light(scene, "Lamp");
	ufbxt_assert(light);

	// Light attribute properties
	ufbx_vec3 color_ref = { 1.0, 1.0, 1.0 };
	ufbx_prop *color = ufbx_find_prop(&light->node.props, "Color");
	ufbxt_assert(color && color->type == UFBX_PROP_COLOR);
	ufbxt_assert_close_vec3(err, color->value_vec3, color_ref);

	ufbx_prop *intensity = ufbx_find_prop(&light->node.props, "Intensity");
	ufbxt_assert(intensity && intensity->type == UFBX_PROP_NUMBER);
	ufbxt_assert_close_real(err, intensity->value_real, 100.0);

	// Model properties
	ufbx_vec3 translation_ref = { 4.076245307922363, 5.903861999511719, -1.0054539442062378 };
	ufbx_prop *translation = ufbx_find_prop(&light->node.props, "Lcl Translation");
	ufbxt_assert(translation && translation->type == UFBX_PROP_TRANSLATION);
	ufbxt_assert_close_vec3(err, translation->value_vec3, translation_ref);

	// Model defaults
	ufbx_vec3 scaling_ref = { 1.0, 1.0, 1.0 };
	ufbx_prop *scaling = ufbx_find_prop(&light->node.props, "GeometricScaling");
	ufbxt_assert(scaling && scaling->type == UFBX_PROP_VECTOR);
	ufbxt_assert_close_vec3(err, scaling->value_vec3, scaling_ref);
}
#endif

UFBXT_FILE_TEST(blender_282_suzanne)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(blender_282_suzanne_and_transform)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(maya_cube)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);

	for (size_t face_i = 0; face_i < mesh->num_faces; face_i++) {
		ufbx_face face = mesh->faces[face_i];
		for (size_t i = face.index_begin; i < face.index_begin + face.num_indices; i++) {
			ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->vertex_normal, i);
			ufbx_vec3 b = ufbx_get_vertex_vec3(&mesh->vertex_binormal, i);
			ufbx_vec3 t = ufbx_get_vertex_vec3(&mesh->vertex_tangent, i);
			ufbxt_assert_close_real(err, ufbxt_dot3(n, n), 1.0);
			ufbxt_assert_close_real(err, ufbxt_dot3(b, b), 1.0);
			ufbxt_assert_close_real(err, ufbxt_dot3(t, t), 1.0);
			ufbxt_assert_close_real(err, ufbxt_dot3(n, b), 0.0);
			ufbxt_assert_close_real(err, ufbxt_dot3(n, t), 0.0);
			ufbxt_assert_close_real(err, ufbxt_dot3(b, t), 0.0);

			for (size_t j = 0; j < face.num_indices; j++) {
				ufbx_vec3 p0 = ufbx_get_vertex_vec3(&mesh->vertex_position, face.index_begin + j);
				ufbx_vec3 p1 = ufbx_get_vertex_vec3(&mesh->vertex_position, face.index_begin + (j + 1) % face.num_indices);
				ufbx_vec3 edge;
				edge.x = p1.x - p0.x;
				edge.y = p1.y - p0.y;
				edge.z = p1.z - p0.z;
				ufbxt_assert_close_real(err, ufbxt_dot3(edge, edge), 1.0);
				ufbxt_assert_close_real(err, ufbxt_dot3(n, edge), 0.0);
			}

		}
	}
}
#endif

UFBXT_FILE_TEST(maya_color_sets)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->color_sets.size == 4);
	ufbxt_assert(!strcmp(mesh->color_sets.data[0].name.data, "RGBCube"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[1].name.data, "White"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[2].name.data, "Black"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[3].name.data, "Alpha"));

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, i);
		ufbx_vec4 refs[4] = {
			{ 0.0, 0.0, 0.0, 1.0 },
			{ 1.0, 1.0, 1.0, 1.0 },
			{ 0.0, 0.0, 0.0, 1.0 },
			{ 1.0, 1.0, 1.0, 0.0 },
		};

		refs[0].x = pos.x + 0.5;
		refs[0].y = pos.y + 0.5;
		refs[0].z = pos.z + 0.5;
		refs[3].w = (pos.x + 0.5) * 0.1 + (pos.y + 0.5) * 0.2 + (pos.z + 0.5) * 0.4;

		for (size_t set_i = 0; set_i < 4; set_i++) {
			ufbx_vec4 color = ufbx_get_vertex_vec4(&mesh->color_sets.data[set_i].vertex_color, i);
			ufbxt_assert_close_vec4(err, color, refs[set_i]);
		}
	}
}
#endif

UFBXT_FILE_TEST(maya_uv_sets)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->uv_sets.size == 3);
	ufbxt_assert(!strcmp(mesh->uv_sets.data[0].name.data, "Default"));
	ufbxt_assert(!strcmp(mesh->uv_sets.data[1].name.data, "PerFace"));
	ufbxt_assert(!strcmp(mesh->uv_sets.data[2].name.data, "Row"));

	size_t counts1[2][2] = { 0 };
	size_t counts2[7][2] = { 0 };

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbx_vec2 uv0 = ufbx_get_vertex_vec2(&mesh->uv_sets.data[0].vertex_uv, i);
		ufbx_vec2 uv1 = ufbx_get_vertex_vec2(&mesh->uv_sets.data[1].vertex_uv, i);
		ufbx_vec2 uv2 = ufbx_get_vertex_vec2(&mesh->uv_sets.data[2].vertex_uv, i);

		ufbxt_assert(uv0.x > 0.05f && uv0.y > 0.05f && uv0.x < 0.95f && uv0.y < 0.95f);
		int x1 = (int)(uv1.x + 0.5f), y1 = (int)(uv1.y + 0.5f);
		int x2 = (int)(uv2.x + 0.5f), y2 = (int)(uv2.y + 0.5f);
		ufbxt_assert_close_real(err, uv1.x - (ufbx_real)x1, 0.0);
		ufbxt_assert_close_real(err, uv1.y - (ufbx_real)y1, 0.0);
		ufbxt_assert_close_real(err, uv2.x - (ufbx_real)x2, 0.0);
		ufbxt_assert_close_real(err, uv2.y - (ufbx_real)y2, 0.0);
		ufbxt_assert(x1 >= 0 && x1 <= 1 && y1 >= 0 && y1 <= 1);
		ufbxt_assert(x2 >= 0 && x2 <= 6 && y2 >= 0 && y2 <= 1);
		counts1[x1][y1]++;
		counts2[x2][y2]++;
	}

	ufbxt_assert(counts1[0][0] == 6);
	ufbxt_assert(counts1[0][1] == 6);
	ufbxt_assert(counts1[1][0] == 6);
	ufbxt_assert(counts1[1][1] == 6);

	for (size_t i = 0; i < 7; i++) {
		size_t n = (i == 0 || i == 6) ? 1 : 2;
		ufbxt_assert(counts2[i][0] == n);
		ufbxt_assert(counts2[i][1] == n);
	}
}
#endif

UFBXT_FILE_TEST(blender_279_color_sets)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "Cube");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->color_sets.size == 3);
	ufbxt_assert(!strcmp(mesh->color_sets.data[0].name.data, "RGBCube"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[1].name.data, "White"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[2].name.data, "Black"));

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, i);
		ufbx_vec4 refs[3] = {
			{ 0.0, 0.0, 0.0, 1.0 },
			{ 1.0, 1.0, 1.0, 1.0 },
			{ 0.0, 0.0, 0.0, 1.0 },
		};

		refs[0].x = pos.x + 0.5;
		refs[0].y = pos.y + 0.5;
		refs[0].z = pos.z + 0.5;

		for (size_t set_i = 0; set_i < 3; set_i++) {
			ufbx_vec4 color = ufbx_get_vertex_vec4(&mesh->color_sets.data[set_i].vertex_color, i);
			ufbxt_assert_close_vec4(err, color, refs[set_i]);
		}
	}
}
#endif

UFBXT_FILE_TEST(blender_279_uv_sets)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "Cube");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->uv_sets.size == 3);
	ufbxt_assert(!strcmp(mesh->uv_sets.data[0].name.data, "Default"));
	ufbxt_assert(!strcmp(mesh->uv_sets.data[1].name.data, "PerFace"));
	ufbxt_assert(!strcmp(mesh->uv_sets.data[2].name.data, "Row"));

	size_t counts1[2][2] = { 0 };
	size_t counts2[7][2] = { 0 };

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbx_vec2 uv0 = ufbx_get_vertex_vec2(&mesh->uv_sets.data[0].vertex_uv, i);
		ufbx_vec2 uv1 = ufbx_get_vertex_vec2(&mesh->uv_sets.data[1].vertex_uv, i);
		ufbx_vec2 uv2 = ufbx_get_vertex_vec2(&mesh->uv_sets.data[2].vertex_uv, i);

		ufbxt_assert(uv0.x > 0.05f && uv0.y > 0.05f && uv0.x < 0.95f && uv0.y < 0.95f);
		int x1 = (int)(uv1.x + 0.5f), y1 = (int)(uv1.y + 0.5f);
		int x2 = (int)(uv2.x + 0.5f), y2 = (int)(uv2.y + 0.5f);
		ufbxt_assert_close_real(err, uv1.x - (ufbx_real)x1, 0.0);
		ufbxt_assert_close_real(err, uv1.y - (ufbx_real)y1, 0.0);
		ufbxt_assert_close_real(err, uv2.x - (ufbx_real)x2, 0.0);
		ufbxt_assert_close_real(err, uv2.y - (ufbx_real)y2, 0.0);
		ufbxt_assert(x1 >= 0 && x1 <= 1 && y1 >= 0 && y1 <= 1);
		ufbxt_assert(x2 >= 0 && x2 <= 6 && y2 >= 0 && y2 <= 1);
		counts1[x1][y1]++;
		counts2[x2][y2]++;
	}

	ufbxt_assert(counts1[0][0] == 6);
	ufbxt_assert(counts1[0][1] == 6);
	ufbxt_assert(counts1[1][0] == 6);
	ufbxt_assert(counts1[1][1] == 6);

	for (size_t i = 0; i < 7; i++) {
		size_t n = (i == 0 || i == 6) ? 1 : 2;
		ufbxt_assert(counts2[i][0] == n);
		ufbxt_assert(counts2[i][1] == n);
	}
}
#endif

UFBXT_FILE_TEST(synthetic_sets_reorder)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->color_sets.size == 4);
	ufbxt_assert(!strcmp(mesh->color_sets.data[0].name.data, "RGBCube"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[1].name.data, "White"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[2].name.data, "Black"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[3].name.data, "Alpha"));
	ufbxt_assert(!strcmp(mesh->uv_sets.data[0].name.data, "Default"));
	ufbxt_assert(!strcmp(mesh->uv_sets.data[1].name.data, "PerFace"));
	ufbxt_assert(!strcmp(mesh->uv_sets.data[2].name.data, "Row"));
}
#endif

UFBXT_FILE_TEST(maya_cone)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCone1");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->vertex_crease.data);
	ufbxt_assert(mesh->edges);
	ufbxt_assert(mesh->edge_crease);
	ufbxt_assert(mesh->edge_smoothing);

	ufbxt_assert(mesh->faces[0].num_indices == 16);

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, i);
		ufbx_real crease = ufbx_get_vertex_real(&mesh->vertex_crease, i);

		ufbxt_assert_close_real(err, crease, pos.y > 0.0 ? 0.998 : 0.0);
	}

	for (size_t i = 0; i < mesh->num_edges; i++) {
		ufbx_edge edge = mesh->edges[i];
		ufbx_real crease = mesh->edge_crease[i];
		bool smoothing = mesh->edge_smoothing[i];
		ufbx_vec3 a = ufbx_get_vertex_vec3(&mesh->vertex_position, edge.indices[0]);
		ufbx_vec3 b = ufbx_get_vertex_vec3(&mesh->vertex_position, edge.indices[1]);

		if (a.y < 0.0 && b.y < 0.0) {
			ufbxt_assert_close_real(err, crease, 0.583);
			ufbxt_assert(!smoothing);
		} else {
			ufbxt_assert(a.y > 0.0 || b.y > 0.0);
			ufbxt_assert_close_real(err, crease, 0.0);
			ufbxt_assert(smoothing);
		}
	}
}
#endif
