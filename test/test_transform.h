
UFBXT_FILE_TEST(maya_pivots)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");
	ufbxt_assert(mesh);

	ufbx_vec3 origin_ref = { 0.7211236250, 1.8317762500, -0.6038020000 };
	ufbxt_assert_close_vec3(err, mesh->node.transform.translation, origin_ref);
}
#endif
