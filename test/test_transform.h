
UFBXT_FILE_TEST(maya_pivots)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node);

	ufbx_vec3 origin_ref = { 0.7211236250, 1.8317762500, -0.6038020000 };
	ufbxt_assert_close_vec3(err, node->local_transform.translation, origin_ref);
}
#endif

#if UFBXT_IMPL
static void ufbxt_check_rotation_order(ufbx_scene *scene, const char *name, ufbx_rotation_order order)
{
	ufbx_node *node = ufbx_find_node(scene, name);
	ufbxt_assert(node);
	ufbx_prop *prop = ufbx_find_prop(&node->props, "RotationOrder");
	ufbxt_assert(prop);
	ufbxt_assert((ufbx_rotation_order)prop->value_int == order);
}
#endif

UFBXT_FILE_TEST(maya_rotation_order)
#if UFBXT_IMPL
{
	ufbxt_check_rotation_order(scene, "XYZ", UFBX_ROTATION_XYZ);
	ufbxt_check_rotation_order(scene, "XZY", UFBX_ROTATION_XZY);
	ufbxt_check_rotation_order(scene, "YZX", UFBX_ROTATION_YZX);
	ufbxt_check_rotation_order(scene, "YXZ", UFBX_ROTATION_YXZ);
	ufbxt_check_rotation_order(scene, "ZXY", UFBX_ROTATION_ZXY);
	ufbxt_check_rotation_order(scene, "ZYX", UFBX_ROTATION_ZYX);
}
#endif

UFBXT_FILE_TEST(maya_post_rotate_order)
#if UFBXT_IMPL
{
	ufbxt_check_rotation_order(scene, "pCube1", UFBX_ROTATION_XYZ);
	ufbxt_check_rotation_order(scene, "pCube2", UFBX_ROTATION_ZYX);
}
#endif

UFBXT_FILE_TEST(synthetic_pre_post_rotate)
#if UFBXT_IMPL
{
	ufbxt_check_rotation_order(scene, "pCube1", UFBX_ROTATION_XYZ);
	ufbxt_check_rotation_order(scene, "pCube2", UFBX_ROTATION_ZYX);
}
#endif

UFBXT_FILE_TEST(maya_parented_cubes)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST(synthetic_geometric_squish)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pSphere1");
	ufbxt_assert(node);
	ufbxt_assert_close_real(err, node->geometry_transform.scale.y, 0.01f);
}
#endif

UFBXT_FILE_TEST(synthetic_geometric_transform)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Parent");
	ufbxt_assert(node);
	ufbx_vec3 euler = ufbx_quat_to_euler(node->geometry_transform.rotation, UFBX_ROTATION_XYZ);
	ufbxt_assert_close_real(err, node->geometry_transform.translation.x, -0.5f);
	ufbxt_assert_close_real(err, node->geometry_transform.translation.y, -1.0f);
	ufbxt_assert_close_real(err, node->geometry_transform.translation.z, -1.5f);
	ufbxt_assert_close_real(err, euler.x, 20.0f);
	ufbxt_assert_close_real(err, euler.y, 40.0f);
	ufbxt_assert_close_real(err, euler.z, 60.0f);
	ufbxt_assert_close_real(err, node->geometry_transform.scale.x, 2.0f);
	ufbxt_assert_close_real(err, node->geometry_transform.scale.y, 3.0f);
	ufbxt_assert_close_real(err, node->geometry_transform.scale.z, 4.0f);
}
#endif

UFBXT_FILE_TEST(maya_cube_hidden)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node);
	ufbxt_assert(!node->visible);
}
#endif

UFBXT_TEST(root_transform)
#if UFBXT_IMPL
{
	const char *name = "maya_cube";
	char buf[512];

	ufbxt_diff_error err = { 0 };
	bool any_found = false;

	for (uint32_t vi = 0; vi < ufbxt_arraycount(ufbxt_file_versions); vi++) {
		for (uint32_t fi = 0; fi < 2; fi++) {
			uint32_t version = ufbxt_file_versions[vi];
			const char *format = fi == 1 ? "ascii" : "binary";
			snprintf(buf, sizeof(buf), "%s%s_%u_%s.fbx", data_root, name, version, format);
		}

		ufbx_error error;
		ufbx_load_opts opts = { 0 };

		ufbx_vec3 euler = { { 90.0f, 0.0f, 0.0f } };

		opts.use_root_transform = true;
		opts.root_transform.translation.x = -1.0f;
		opts.root_transform.translation.y = -2.0f;
		opts.root_transform.translation.z = -3.0f;
		opts.root_transform.rotation = ufbx_euler_to_quat(euler, UFBX_ROTATION_XYZ);
		opts.root_transform.scale.x = 2.0f;
		opts.root_transform.scale.y = 3.0f;
		opts.root_transform.scale.z = 4.0f;

		ufbx_scene *scene = ufbx_load_file(buf, &opts, &error);
		if (error.type == UFBX_ERROR_FILE_NOT_FOUND) continue;
		ufbxt_assert(scene);
		any_found = true;

		ufbxt_check_scene(scene);

		ufbxt_assert_close_vec3(&err, scene->root_node->local_transform.translation, opts.root_transform.translation);
		ufbxt_assert_close_quat(&err, scene->root_node->local_transform.rotation, opts.root_transform.rotation);
		ufbxt_assert_close_vec3(&err, scene->root_node->local_transform.scale, opts.root_transform.scale);

		ufbx_transform eval = ufbx_evaluate_transform(&scene->anim, scene->root_node, 0.1);
		ufbxt_assert_close_vec3(&err, eval.translation, opts.root_transform.translation);
		ufbxt_assert_close_quat(&err, eval.rotation, opts.root_transform.rotation);
		ufbxt_assert_close_vec3(&err, eval.scale, opts.root_transform.scale);

		ufbx_scene *state = ufbx_evaluate_scene(scene, &scene->anim, 0.1, NULL, NULL);
		ufbxt_assert(state);

		ufbxt_assert_close_vec3(&err, state->root_node->local_transform.translation, opts.root_transform.translation);
		ufbxt_assert_close_quat(&err, state->root_node->local_transform.rotation, opts.root_transform.rotation);
		ufbxt_assert_close_vec3(&err, state->root_node->local_transform.scale, opts.root_transform.scale);
		
		ufbx_free_scene(state);
		ufbx_free_scene(scene);
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
	ufbxt_assert(any_found);
}
#endif

UFBXT_TEST(blender_axes)
#if UFBXT_IMPL
{
	char buf[512];

	ufbxt_diff_error err = { 0 };

	static const char *axis_names[] = {
		"px", "nx", "py", "ny", "pz", "nz",
	};

	for (uint32_t fwd_ix = 0; fwd_ix < 6; fwd_ix++) {
		for (uint32_t up_ix = 0; up_ix < 6; up_ix++) {

			// Don't allow collinear axes
			if ((fwd_ix >> 1) == (up_ix >> 1)) continue;

			ufbx_coordinate_axis axis_fwd = (ufbx_coordinate_axis)fwd_ix;
			ufbx_coordinate_axis axis_up = (ufbx_coordinate_axis)up_ix;

			bool any_found = false;

			for (uint32_t vi = 0; vi < ufbxt_arraycount(ufbxt_file_versions); vi++) {
				for (uint32_t fi = 0; fi < 2; fi++) {
					uint32_t version = ufbxt_file_versions[vi];
					const char *format = fi == 1 ? "ascii" : "binary";

					snprintf(buf, sizeof(buf), "%sblender_axes/axes_%s%s_%u_%s.fbx", data_root,
						axis_names[fwd_ix], axis_names[up_ix], version, format);

					// Load normally and check axes
					{
						ufbx_error error;

						ufbx_scene *scene = ufbx_load_file(buf, NULL, &error);
						if (error.type == UFBX_ERROR_FILE_NOT_FOUND) continue;
						ufbxt_assert(scene);
						any_found = true;

						ufbx_coordinate_axis axis_front = axis_fwd ^ 1;

						ufbxt_assert(scene->settings.axes.front == axis_front);
						ufbxt_assert(scene->settings.axes.up == axis_up);
					}

					// Axis conversion
					{
						ufbx_load_opts opts = { 0 };

						opts.target_axes = ufbx_axes_right_handed_z_up;
						opts.target_unit_meters = 1.0f;

						ufbx_scene *scene = ufbx_load_file(buf, &opts, NULL);
						ufbxt_assert(scene);
						any_found = true;

						ufbx_node *plane = ufbx_find_node(scene, "Plane");
						ufbxt_assert(plane && plane->mesh);
						ufbx_mesh *mesh = plane->mesh;

						ufbxt_assert(mesh->num_faces == 1);
						ufbx_face face = mesh->faces[0];
						ufbxt_assert(face.num_indices == 3);

						for (uint32_t i = 0; i < face.num_indices; i++) {
							ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, face.index_begin + i);
							ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, face.index_begin + i);

							pos = ufbx_transform_position(&plane->geometry_to_world, pos);

							ufbx_vec3 ref;
							if (uv.x < 0.5f && uv.y < 0.5f) {
								ref.x = 1.0f;
								ref.y = 1.0f;
								ref.z = 3.0f;
							} else if (uv.x > 0.5f && uv.y < 0.5f) {
								ref.x = 2.0f;
								ref.y = 2.0f;
								ref.z = 3.0f;
							} else if (uv.x < 0.5f && uv.y > 0.5f) {
								ref.x = 1.0f;
								ref.y = 2.0f;
								ref.z = 4.0f;
							} else {
								ufbxt_assert(0 && "Shouldn't exist");
							}

							ufbxt_assert_close_vec3(&err, pos, ref);
						}
					}
				}
			}

			ufbxt_assert(any_found);
		}
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
}
#endif


