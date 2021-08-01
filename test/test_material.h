
UFBXT_FILE_TEST(maya_slime)
#if UFBXT_IMPL
{
	// ufbx_material *material = ufbx_find_material(scene, "Slime_002:Skin");
	// ufbxt_assert(material);
	// TODO: This material has an embedded texture attached
	// check when textures are implemented...
}
#endif

UFBXT_FILE_TEST(maya_textured_cube)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "phong1");
	ufbxt_assert(material);
	ufbxt_assert(material->textures.count == 6);

	ufbxt_assert(!strcmp(material->diffuse.color_texture->relative_filename.data, "textures\\checkerboard_diffuse.png"));
	ufbxt_assert(!strcmp(material->specular.color_texture->relative_filename.data, "textures\\checkerboard_specular.png"));
	ufbxt_assert(!strcmp(material->reflection.color_texture->relative_filename.data, "textures\\checkerboard_reflection.png"));
	ufbxt_assert(!strcmp(material->transparency.color_texture->relative_filename.data, "textures\\checkerboard_transparency.png"));
	ufbxt_assert(!strcmp(material->emission.color_texture->relative_filename.data, "textures\\checkerboard_emissive.png"));
	ufbxt_assert(!strcmp(material->ambient.color_texture->relative_filename.data, "textures\\checkerboard_ambient.png"));
}
#endif

UFBXT_FILE_TEST(maya_arnold_textures)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "aiStandardSurface1");
	ufbxt_assert(material);
	ufbxt_assert(material->textures.count == 5);

	ufbxt_assert(!strcmp(material->diffuse.color_texture->relative_filename.data, "textures\\checkerboard_diffuse.png"));
	ufbxt_assert(!strcmp(material->specular.color_texture->relative_filename.data, "textures\\checkerboard_specular.png"));
	ufbxt_assert(!strcmp(material->roughness.color_texture->relative_filename.data, "textures\\checkerboard_roughness.png"));
	ufbxt_assert(!strcmp(material->metallic.color_texture->relative_filename.data, "textures\\checkerboard_metallic.png"));
	ufbxt_assert(!strcmp(material->diffuse_roughness.color_texture->relative_filename.data, "textures\\checkerboard_roughness.png"));
}
#endif
