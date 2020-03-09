

UFBXT_FILE_TEST(blender_279_default)
#if UFBXT_IMPL
{
	if (scene->metadata.ascii) {
		ufbxt_assert(!strcmp(scene->metadata.creator.data, "FBX SDK/FBX Plugins build 20070228"));
	} else {
		ufbxt_assert(!strcmp(scene->metadata.creator.data, "Blender (stable FBX IO) - 2.79 (sub 0) - 3.7.13"));
	}

	for (size_t i = 0; i < scene->nodes.size; i++) {
		ufbx_node *node = scene->nodes.data[i];
		if (node->type != UFBX_NODE_MODEL) continue;
		if (!ufbxi_streq(node->name, "Cube")) continue;

		ufbx_prop *prop = ufbx_get_prop(node, "Lcl Rotation");
		ufbxt_assert(prop != NULL);
		ufbxt_assert(ufbxi_streq(prop->name, "Lcl Rotation"));
		ufbxt_assert(prop->type == UFBX_PROP_ROTATION);
		ufbxt_assert(fabs(prop->value_vec3.x - -90.0) <= 0.1);
		ufbxt_assert(fabs(prop->value_vec3.y - 0.0) <= 0.1);
		ufbxt_assert(fabs(prop->value_vec3.z - 0.0) <= 0.1);
	}
}
#endif

UFBXT_FILE_TEST(maya_cube)
#if UFBXT_IMPL
{
	ufbxt_assert(!strcmp(scene->metadata.creator.data, "FBX SDK/FBX Plugins version 2019.2"));

	ufbx_model *model = ufbx_find_model(scene, "pCube1");
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
