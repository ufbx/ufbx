
UFBXT_FILE_TEST(maya_slime)
#if UFBXT_IMPL
{
	ufbx_node *node_high = ufbx_find_node(scene, "Slime_002:Slime_Body_high");
	ufbxt_assert(node_high);
	ufbxt_assert(!node_high->visible);
}
#endif

UFBXT_FILE_TEST(blender_293_barbarian)
#if UFBXT_IMPL
{
}
#endif