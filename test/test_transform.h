
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
