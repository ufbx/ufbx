
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

	ufbxt_assert(!strcmp(material->fbx.diffuse_color.texture->relative_filename.data, "textures\\checkerboard_diffuse.png"));
	ufbxt_assert(!strcmp(material->fbx.specular_color.texture->relative_filename.data, "textures\\checkerboard_specular.png"));
	ufbxt_assert(!strcmp(material->fbx.reflection_color.texture->relative_filename.data, "textures\\checkerboard_reflection.png"));
	ufbxt_assert(!strcmp(material->fbx.transparency_color.texture->relative_filename.data, "textures\\checkerboard_transparency.png"));
	ufbxt_assert(!strcmp(material->fbx.emission_color.texture->relative_filename.data, "textures\\checkerboard_emissive.png"));
	ufbxt_assert(!strcmp(material->fbx.ambient_color.texture->relative_filename.data, "textures\\checkerboard_ambient.png"));
}
#endif

UFBXT_FILE_TEST(maya_shared_textures)
#if UFBXT_IMPL
{
	ufbx_material *material;

	material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "Shared");
	ufbxt_assert(material);
	ufbxt_assert(material->textures.count == 6);

	ufbxt_assert(!strcmp(material->fbx.diffuse_color.texture->relative_filename.data, "textures\\checkerboard_ambient.png")); // sic: test has wrong texture
	ufbxt_assert(!strcmp(material->fbx.diffuse_factor.texture->relative_filename.data, "textures\\checkerboard_diffuse.png"));
	ufbxt_assert(!strcmp(material->fbx.emission_color.texture->relative_filename.data, "textures\\checkerboard_emissive.png"));
	ufbxt_assert(!strcmp(material->fbx.ambient_color.texture->relative_filename.data, "textures\\checkerboard_ambient.png"));
	ufbxt_assert(!strcmp(material->fbx.transparency_color.texture->relative_filename.data, "textures\\checkerboard_transparency.png"));
	ufbxt_assert(!strcmp(material->fbx.bump.texture->relative_filename.data, "textures\\checkerboard_bump.png"));

	material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "Special");
	ufbxt_assert(material);
	ufbxt_assert(material->textures.count == 1);

	ufbxt_assert(!strcmp(material->fbx.diffuse_color.texture->relative_filename.data, "textures\\tiny_clouds.png"));
}
#endif

UFBXT_FILE_TEST(maya_arnold_textures)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "aiStandardSurface1");
	ufbxt_assert(material);
	ufbxt_assert(material->textures.count == 5);

	ufbxt_assert(!strcmp(material->pbr.base_color.texture->relative_filename.data, "textures\\checkerboard_diffuse.png"));
	ufbxt_assert(!strcmp(material->pbr.specular_color.texture->relative_filename.data, "textures\\checkerboard_specular.png"));
	ufbxt_assert(!strcmp(material->pbr.roughness.texture->relative_filename.data, "textures\\checkerboard_roughness.png"));
	ufbxt_assert(!strcmp(material->pbr.metallic.texture->relative_filename.data, "textures\\checkerboard_metallic.png"));
	ufbxt_assert(!strcmp(material->pbr.diffuse_roughness.texture->relative_filename.data, "textures\\checkerboard_roughness.png"));
}
#endif

UFBXT_FILE_TEST(blender_279_internal_textures)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "Material.001");
	ufbxt_assert(material);
	ufbxt_assert(material->textures.count == 5);

	ufbxt_assert(!strcmp(material->fbx.diffuse_color.texture->relative_filename.data, "textures\\checkerboard_diffuse.png"));
	ufbxt_assert(!strcmp(material->fbx.specular_color.texture->relative_filename.data, "textures\\checkerboard_specular.png"));
	ufbxt_assert(!strcmp(material->fbx.specular_factor.texture->relative_filename.data, "textures\\checkerboard_weight.png"));
	ufbxt_assert(!strcmp(material->fbx.ambient_factor.texture->relative_filename.data, "textures\\checkerboard_ambient.png"));
	ufbxt_assert(!strcmp(material->fbx.emission_factor.texture->relative_filename.data, "textures\\checkerboard_emissive.png"));
}
#endif

UFBXT_FILE_TEST(blender_293_textures)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "Material.001");
	ufbxt_assert(material);
	ufbxt_assert(material->textures.count == 5);

	ufbxt_assert(!strcmp(material->pbr.base_color.texture->relative_filename.data, "textures\\checkerboard_diffuse.png"));
	ufbxt_assert(!strcmp(material->pbr.roughness.texture->relative_filename.data, "textures\\checkerboard_roughness.png"));
	ufbxt_assert(!strcmp(material->pbr.metallic.texture->relative_filename.data, "textures\\checkerboard_metallic.png"));
	ufbxt_assert(!strcmp(material->pbr.emission_color.texture->relative_filename.data, "textures\\checkerboard_emissive.png"));
	ufbxt_assert(!strcmp(material->pbr.opacity.texture->relative_filename.data, "textures\\checkerboard_weight.png"));
}
#endif

UFBXT_FILE_TEST(blender_293_material_mapping)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "Material.001");
	ufbxt_assert(material);

	ufbxt_assert_close_real(err, material->fbx.specular_exponent.value.x, 76.913f);
	ufbxt_assert_close_real(err, material->fbx.transparency_factor.value.x, 0.544f);
	ufbxt_assert_close_real(err, material->pbr.opacity.value.x, 0.456f);
	ufbxt_assert_close_real(err, material->pbr.roughness.value.x, 0.123f);
}
#endif
