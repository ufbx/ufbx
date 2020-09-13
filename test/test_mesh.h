
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

#if UFBXT_IMPL
static void ufbxt_check_tangent_space(ufbxt_diff_error *err, ufbx_mesh *mesh)
{
	for (size_t set_i = 0; set_i < mesh->uv_sets.size; set_i++) {
		ufbx_uv_set set = mesh->uv_sets.data[set_i];
		ufbxt_assert(set.vertex_uv.data);
		ufbxt_assert(set.vertex_binormal.data);
		ufbxt_assert(set.vertex_tangent.data);

		for (size_t face_i = 0; face_i < mesh->num_faces; face_i++) {
			ufbx_face face = mesh->faces[face_i];

			for (size_t i = 0; i < face.num_indices; i++) {
				size_t a = face.index_begin + i;
				size_t b = face.index_begin + (i + 1) % face.num_indices;

				ufbx_vec3 pa = ufbx_get_vertex_vec3(&mesh->vertex_position, a);
				ufbx_vec3 pb = ufbx_get_vertex_vec3(&mesh->vertex_position, b);
				ufbx_vec3 ba = ufbx_get_vertex_vec3(&set.vertex_binormal, a);
				ufbx_vec3 bb = ufbx_get_vertex_vec3(&set.vertex_binormal, b);
				ufbx_vec3 ta = ufbx_get_vertex_vec3(&set.vertex_tangent, a);
				ufbx_vec3 tb = ufbx_get_vertex_vec3(&set.vertex_tangent, b);
				ufbx_vec2 ua = ufbx_get_vertex_vec2(&set.vertex_uv, a);
				ufbx_vec2 ub = ufbx_get_vertex_vec2(&set.vertex_uv, b);

				ufbx_vec3 dp = ufbxt_sub3(pb, pa);
				ufbx_vec2 du = ufbxt_sub2(ua, ub);

				ufbx_real dp_len = sqrt(ufbxt_dot3(dp, dp));
				dp.x /= dp_len;
				dp.y /= dp_len;
				dp.z /= dp_len;

				ufbx_real du_len = sqrt(ufbxt_dot2(du, du));
				du.x /= du_len;
				du.y /= du_len;

				ufbx_real dba = ufbxt_dot3(dp, ba);
				ufbx_real dbb = ufbxt_dot3(dp, bb);
				ufbx_real dta = ufbxt_dot3(dp, ta);
				ufbx_real dtb = ufbxt_dot3(dp, tb);
				ufbxt_assert_close_real(err, dba, dbb);
				ufbxt_assert_close_real(err, dta, dtb);

				ufbxt_assert_close_real(err, ub.x - ua.x, dta);
				ufbxt_assert_close_real(err, ub.y - ua.y, dba);
			}
		}
	}
}
#endif

UFBXT_FILE_TEST(maya_uv_set_tangents)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pPlane1");
	ufbxt_assert(mesh);
	ufbxt_check_tangent_space(err, mesh);
}
#endif

UFBXT_FILE_TEST(blender_279_uv_set_tangents)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "Plane");
	ufbxt_assert(mesh);
	ufbxt_check_tangent_space(err, mesh);
}
#endif

UFBXT_FILE_TEST(synthetic_tangents_reorder)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pPlane1");
	ufbxt_assert(mesh);
	ufbxt_check_tangent_space(err, mesh);
}
#endif

UFBXT_FILE_TEST(blender_279_ball)
#if UFBXT_IMPL
{
	ufbx_material *red = ufbx_find_material(scene, "Red");
	ufbx_material *white = ufbx_find_material(scene, "White");
	ufbxt_assert(!strcmp(red->name.data, "Red"));
	ufbxt_assert(!strcmp(white->name.data, "White"));

	ufbx_vec3 red_ref = { 0.8, 0.0, 0.0 };
	ufbx_vec3 white_ref = { 0.8, 0.8, 0.8 };
	ufbxt_assert_close_vec3(err, red->diffuse_color, red_ref);
	ufbxt_assert_close_vec3(err, white->diffuse_color, white_ref);

	ufbx_mesh *mesh = ufbx_find_mesh(scene, "Icosphere");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->face_material);
	ufbxt_assert(mesh->face_smoothing);

	ufbxt_assert(mesh->materials.size == 2);
	ufbxt_assert(mesh->materials.data[0] == red);
	ufbxt_assert(mesh->materials.data[1] == white);

	for (size_t face_i = 0; face_i < mesh->num_faces; face_i++) {
		ufbx_face face = mesh->faces[face_i];
		ufbx_vec3 mid = { 0 };
		for (size_t i = 0; i < face.num_indices; i++) {
			mid = ufbxt_add3(mid, ufbx_get_vertex_vec3(&mesh->vertex_position, face.index_begin + i));
		}
		mid.x /= (ufbx_real)face.num_indices;
		mid.y /= (ufbx_real)face.num_indices;
		mid.z /= (ufbx_real)face.num_indices;

		bool smoothing = mesh->face_smoothing[face_i];
		int32_t material = mesh->face_material[face_i];
		ufbxt_assert(smoothing == (mid.x > 0.0));
		ufbxt_assert(material == (mid.z < 0.0 ? 1 : 0));
	}
}
#endif

UFBXT_FILE_TEST(synthetic_broken_material)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->materials.size == 0);
	ufbxt_assert(mesh->face_material == NULL);
}
#endif

UFBXT_FILE_TEST(maya_uv_and_color_sets)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->uv_sets.size == 2);
	ufbxt_assert(mesh->color_sets.size == 2);
	ufbxt_assert(!strcmp(mesh->uv_sets.data[0].name.data, "UVA"));
	ufbxt_assert(!strcmp(mesh->uv_sets.data[1].name.data, "UVB"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[0].name.data, "ColorA"));
	ufbxt_assert(!strcmp(mesh->color_sets.data[1].name.data, "ColorB"));
}
#endif

UFBXT_FILE_TEST(maya_bad_face)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->num_faces == 4);
	ufbxt_assert(mesh->num_bad_faces == 2);
	ufbxt_assert(mesh->faces[0].num_indices == 3);
	ufbxt_assert(mesh->faces[1].num_indices == 4);
	ufbxt_assert(mesh->faces[2].num_indices == 4);
	ufbxt_assert(mesh->faces[3].num_indices == 4);
	ufbxt_assert(mesh->faces[4].num_indices == 1);
	ufbxt_assert(mesh->faces[5].num_indices == 2);
}
#endif

UFBXT_FILE_TEST(blender_279_edge_vertex)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "Plane");
	ufbxt_assert(mesh);
	ufbxt_assert(mesh->num_vertices == 3);
	ufbxt_assert(mesh->num_faces == 0);
	ufbxt_assert(mesh->num_bad_faces == 1);
	if (scene->metadata.ascii) {
		ufbxt_assert(mesh->num_edges == 2);
	} else {
		ufbxt_assert(mesh->num_edges == 1);
	}
}
#endif

UFBXT_FILE_TEST(blender_279_edge_circle)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "Circle");
	ufbxt_assert(mesh);
}
#endif
