
UFBXT_FILE_TEST(max2009_blob)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Box02");
	ufbxt_assert(node);
	ufbxt_assert(node->mesh);
	ufbxt_assert(node->children.count == 16);
}
#endif

UFBXT_FILE_TEST(max2009_sausage)
#if UFBXT_IMPL
{
}
#endif
