#undef UFBXT_TEST_GROUP
#define UFBXT_TEST_GROUP "convert"

#if UFBXT_IMPL
ufbx_load_opts ufbxt_geometry_transform_helper_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
	return opts;
}

ufbx_load_opts ufbxt_geometry_transform_modify_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
	return opts;
}
#endif

#if UFBXT_IMPL
typedef struct {
	ufbx_vec3 translation;
	ufbx_vec3 rotation_euler;
	ufbx_vec3 scale;
} ufbxt_ref_transform;

static void ufbxt_check_transform(ufbxt_diff_error *err, const char *name, ufbx_transform transform, ufbxt_ref_transform ref)
{
	ufbx_vec3 rotation_euler = ufbx_quat_to_euler(transform.rotation, UFBX_ROTATION_XYZ);
	ufbxt_hintf("%s { { %.2f, %.2f, %.2f }, { %.2f, %.2f, %.2f }, { %.2f, %.2f, %.2f } }", name,
		transform.translation.x, transform.translation.y, transform.translation.z,
		rotation_euler.x, rotation_euler.y, rotation_euler.z,
		transform.scale.x, transform.scale.y, transform.scale.z);

	ufbxt_assert_close_vec3(err, transform.translation, ref.translation);
	ufbxt_assert_close_vec3(err, rotation_euler, ref.rotation_euler);
	ufbxt_assert_close_vec3(err, transform.scale, ref.scale);
}

static const ufbxt_ref_transform ufbxt_ref_transform_identity = {
	{ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f },
};
#endif


UFBXT_FILE_TEST(max_geometry_transform)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->nodes.count == 3);

	{
		ufbx_node *node = ufbx_find_node(scene, "Box001");
		ufbxt_assert(node);
		ufbxt_assert(!node->is_geometry_transform_helper);
		ufbxt_assert(node->geometry_transform_helper == NULL);
		ufbxt_assert(node->mesh);

		ufbxt_ref_transform local_transform = {
			{ 0.0f, -10.0f, 0.0f }, { 0.0f, 0.0f, -90.0f }, { 1.0f, 2.0f, 1.0f },
		};
		ufbxt_ref_transform geometry_transform = {
			{ 0.0f, 0.0f, 10.0f }, { 0.0f, 90.0f, 0.0f }, { 1.0f, 1.0f, 2.0f },
		};

		ufbxt_check_transform(err, "Box001 local", node->local_transform, local_transform);
		ufbxt_check_transform(err, "Box001 geometry", node->geometry_transform, geometry_transform);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Box002");
		ufbxt_assert(node);
		ufbxt_assert(!node->is_geometry_transform_helper);
		ufbxt_assert(node->geometry_transform_helper == NULL);
		ufbxt_assert(node->mesh);

		ufbxt_ref_transform local_transform = {
			{ 0.0f, 0.0f, 20.0f }, { 0.0f, 0.0f, -180.0f }, { 1.0f, 0.5f, 1.0f },
		};
		ufbxt_ref_transform geometry_transform = {
			{ 10.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f },
		};

		ufbxt_check_transform(err, "Box002 local", node->local_transform, local_transform);
		ufbxt_check_transform(err, "Box002 geometry", node->geometry_transform, geometry_transform);
	}
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_geometry_transform_helper, max_geometry_transform, ufbxt_geometry_transform_helper_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->nodes.count == 5);

	{
		ufbx_node *node = ufbx_find_node(scene, "Box001");
		ufbxt_assert(node);
		ufbxt_assert(!node->is_geometry_transform_helper);
		ufbxt_assert(node->geometry_transform_helper != NULL);
		ufbxt_assert(!node->mesh);

		ufbxt_ref_transform local_transform = {
			{ 0.0f, -10.0f, 0.0f }, { 0.0f, 0.0f, -90.0f }, { 1.0f, 2.0f, 1.0f },
		};
		ufbxt_ref_transform geometry_transform = {
			{ 0.0f, 0.0f, 10.0f }, { 0.0f, 90.0f, 0.0f }, { 1.0f, 1.0f, 2.0f },
		};

		ufbxt_check_transform(err, "Box001 local", node->local_transform, local_transform);
		ufbxt_check_transform(err, "Box001 geometry", node->geometry_transform, ufbxt_ref_transform_identity);

		node = node->geometry_transform_helper;
		ufbxt_assert(node->is_geometry_transform_helper);
		ufbxt_assert(node->geometry_transform_helper == NULL);
		ufbxt_assert(node->mesh);

		ufbxt_check_transform(err, "Box001 helper local", node->local_transform, geometry_transform);
		ufbxt_check_transform(err, "Box001 helper geometry", node->geometry_transform, ufbxt_ref_transform_identity);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Box002");
		ufbxt_assert(node);
		ufbxt_assert(!node->is_geometry_transform_helper);
		ufbxt_assert(node->geometry_transform_helper != NULL);
		ufbxt_assert(!node->mesh);

		ufbxt_ref_transform local_transform = {
			{ 0.0f, 0.0f, 20.0f }, { 0.0f, 0.0f, -180.0f }, { 1.0f, 0.5f, 1.0f },
		};
		ufbxt_ref_transform geometry_transform = {
			{ 10.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f },
		};

		ufbxt_check_transform(err, "Box002 local", node->local_transform, local_transform);
		ufbxt_check_transform(err, "Box002 geometry", node->geometry_transform, ufbxt_ref_transform_identity);

		node = node->geometry_transform_helper;
		ufbxt_assert(node->is_geometry_transform_helper);
		ufbxt_assert(node->geometry_transform_helper == NULL);
		ufbxt_assert(node->mesh);

		ufbxt_check_transform(err, "Box002 helper local", node->local_transform, geometry_transform);
		ufbxt_check_transform(err, "Box002 helper geometry", node->geometry_transform, ufbxt_ref_transform_identity);
	}
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_geometry_transform_modify, max_geometry_transform, ufbxt_geometry_transform_modify_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->nodes.count == 3);

	{
		ufbx_node *node = ufbx_find_node(scene, "Box001");
		ufbxt_assert(node);
		ufbxt_assert(!node->is_geometry_transform_helper);
		ufbxt_assert(node->geometry_transform_helper == NULL);
		ufbxt_assert(node->mesh);

		ufbxt_ref_transform local_transform = {
			{ 0.0f, -10.0f, 0.0f }, { 0.0f, 0.0f, -90.0f }, { 1.0f, 2.0f, 1.0f },
		};

		ufbxt_check_transform(err, "Box001 local", node->local_transform, local_transform);
		ufbxt_check_transform(err, "Box001 geometry", node->geometry_transform, ufbxt_ref_transform_identity);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Box002");
		ufbxt_assert(node);
		ufbxt_assert(!node->is_geometry_transform_helper);
		ufbxt_assert(node->geometry_transform_helper == NULL);
		ufbxt_assert(node->mesh);

		ufbxt_ref_transform local_transform = {
			{ 0.0f, 0.0f, 20.0f }, { 0.0f, 0.0f, -180.0f }, { 1.0f, 0.5f, 1.0f },
		};

		ufbxt_check_transform(err, "Box002 local", node->local_transform, local_transform);
		ufbxt_check_transform(err, "Box002 geometry", node->geometry_transform, ufbxt_ref_transform_identity);
	}
}
#endif

UFBXT_FILE_TEST_OPTS_ALT(no_unnecessary_geometry_helpers, maya_cube, ufbxt_geometry_transform_helper_opts)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->nodes.count == 2);
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node);
	ufbxt_assert(!node->is_geometry_transform_helper);
	ufbxt_assert(node->geometry_transform_helper == NULL);
}
#endif

