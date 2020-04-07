
UFBXT_FILE_TEST(maya_pivots)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);

	ufbx_vec3 origin_ref = { 0.7211236250, 1.8317762500, -0.6038020000 };
	ufbxt_assert_close_vec3(err, mesh->node.transform.translation, origin_ref);
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
