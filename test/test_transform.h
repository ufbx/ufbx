
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
		
		ufbx_free_scene(scene);
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
	ufbxt_assert(any_found);
}
#endif


