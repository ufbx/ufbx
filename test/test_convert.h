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

ufbx_load_opts ufbxt_geometry_transform_helper_name_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
	opts.geometry_transform_helper_name.data = "(ufbxt helper)";
	opts.geometry_transform_helper_name.length = SIZE_MAX;
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

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_geometry_transform_helper, max_geometry_transform, ufbxt_geometry_transform_helper_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS|UFBXT_FILE_TEST_FLAG_DIFF_ALWAYS)
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

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_geometry_transform_modify, max_geometry_transform, ufbxt_geometry_transform_modify_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS|UFBXT_FILE_TEST_FLAG_DIFF_ALWAYS)
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

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_transformed_skin_helpers, max_transformed_skin, ufbxt_geometry_transform_helper_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS|UFBXT_FILE_TEST_FLAG_DIFF_ALWAYS)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Box001");
	ufbxt_assert(node);
	ufbxt_assert(!node->mesh);
	ufbxt_assert(!node->is_geometry_transform_helper);
	ufbxt_assert(node->geometry_transform_helper);
	ufbxt_assert(!node->has_geometry_transform);
	ufbx_node *geo_node = node->geometry_transform_helper;
	ufbxt_assert(geo_node->mesh);
	ufbxt_assert(geo_node->is_geometry_transform_helper);
	ufbxt_assert(!geo_node->geometry_transform_helper);
	ufbxt_assert(!geo_node->has_geometry_transform);

	ufbxt_check_frame(scene, err, false, "max_transformed_skin_5", NULL, 5.0/30.0);
	ufbxt_check_frame(scene, err, false, "max_transformed_skin_15", NULL, 15.0/30.0);
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_transformed_skin_modify, max_transformed_skin, ufbxt_geometry_transform_modify_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS|UFBXT_FILE_TEST_FLAG_DIFF_ALWAYS)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Box001");
	ufbxt_assert(node);
	ufbxt_assert(node->mesh);
	ufbxt_assert(!node->is_geometry_transform_helper);
	ufbxt_assert(!node->geometry_transform_helper);
	ufbxt_assert(!node->has_geometry_transform);

	ufbxt_check_frame(scene, err, false, "max_transformed_skin_5", NULL, 5.0/30.0);
	ufbxt_check_frame(scene, err, false, "max_transformed_skin_15", NULL, 15.0/30.0);
}
#endif

UFBXT_FILE_TEST(max_geometry_transform_instances)
#if UFBXT_IMPL
{
	if (scene->metadata.version >= 7000) {
		ufbxt_assert(scene->meshes.count == 1);
	} else {
		ufbxt_assert(scene->meshes.count == 4);
	}

	ufbxt_assert(scene->nodes.count == 5);
	for (size_t i = 1; i < scene->nodes.count; i++) {
		ufbx_node *node = scene->nodes.data[i];
		ufbxt_assert(node->has_geometry_transform);
		ufbxt_assert(node->mesh);
		ufbxt_assert(!node->is_geometry_transform_helper);
		ufbxt_assert(!node->geometry_transform_helper);
	}
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_geometry_transform_instances_helper, max_geometry_transform_instances, ufbxt_geometry_transform_helper_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS|UFBXT_FILE_TEST_FLAG_DIFF_ALWAYS)
#if UFBXT_IMPL
{
	if (scene->metadata.version >= 7000) {
		ufbxt_assert(scene->meshes.count == 1);
	} else {
		ufbxt_assert(scene->meshes.count == 4);
	}

	ufbxt_assert(scene->nodes.count == 9);
	for (size_t i = 1; i < scene->nodes.count; i++) {
		ufbx_node *node = scene->nodes.data[i];
		ufbxt_assert(!node->has_geometry_transform);
		if (node->name.length > 0) {
			ufbxt_assert(!node->mesh);
			ufbxt_assert(!node->is_geometry_transform_helper);
			ufbxt_assert(node->geometry_transform_helper);
		} else {
			ufbxt_assert(node->mesh);
			ufbxt_assert(node->is_geometry_transform_helper);
			ufbxt_assert(!node->geometry_transform_helper);
		}
	}
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_geometry_transform_instances_modify, max_geometry_transform_instances, ufbxt_geometry_transform_modify_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS)
#if UFBXT_IMPL
{
	if (scene->metadata.version >= 7000) {
		ufbxt_assert(scene->meshes.count == 1);
	} else {
		ufbxt_assert(scene->meshes.count == 4);
	}

	ufbxt_assert(scene->nodes.count == 5);
	for (size_t i = 1; i < scene->nodes.count; i++) {
		ufbx_node *node = scene->nodes.data[i];
		ufbxt_assert(!node->has_geometry_transform);
		ufbxt_assert(node->mesh);
		ufbxt_assert(!node->is_geometry_transform_helper);
		ufbxt_assert(!node->geometry_transform_helper);
	}

	// No diff as it would fail by design
	// TODO: Some way to check that diff actually fails?
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max_geometry_transform_helper_names, max_geometry_transform, ufbxt_geometry_transform_helper_name_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->nodes.count == 5);

	{
		ufbx_node *node = ufbx_find_node(scene, "Box001");
		ufbxt_assert(node->geometry_transform_helper != NULL);
		node = node->geometry_transform_helper;
		ufbxt_assert(!strcmp(node->name.data, "(ufbxt helper)"));
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Box002");
		ufbxt_assert(node->geometry_transform_helper != NULL);
		node = node->geometry_transform_helper;
		ufbxt_assert(!strcmp(node->name.data, "(ufbxt helper)"));
	}
}
#endif

