
UFBXT_FILE_TEST(maya_slime)
#if UFBXT_IMPL
{
	ufbx_material *material = ufbx_find_material(scene, "Slime_002:Skin");
	ufbxt_assert(material);
	// TODO: This material has an embedded texture attached
	// check when textures are implemented...
}
#endif
