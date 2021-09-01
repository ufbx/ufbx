
UFBXT_FILE_TEST(max2009_blob)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Box01");
	ufbxt_assert(node);
	ufbxt_assert(node->mesh);
	ufbxt_assert(node->children.count == 16);

	ufbxt_check_frame(scene, err, false, "max2009_blob_8", NULL, 8.0/30.0);
	ufbxt_check_frame(scene, err, false, "max2009_blob_18", NULL, 18.0/30.0);
}
#endif

UFBXT_FILE_TEST(max2009_sausage)
#if UFBXT_IMPL
{
}
#endif
