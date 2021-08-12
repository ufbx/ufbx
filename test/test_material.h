
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

	ufbxt_assert(material->shader_type == UFBX_SHADER_FBX_LAMBERT);
	ufbxt_assert(!strcmp(material->fbx.diffuse_color.texture->relative_filename.data, "textures\\checkerboard_ambient.png")); // sic: test has wrong texture
	ufbxt_assert(!strcmp(material->fbx.diffuse_factor.texture->relative_filename.data, "textures\\checkerboard_diffuse.png"));
	ufbxt_assert(!strcmp(material->fbx.emission_color.texture->relative_filename.data, "textures\\checkerboard_emissive.png"));
	ufbxt_assert(!strcmp(material->fbx.ambient_color.texture->relative_filename.data, "textures\\checkerboard_ambient.png"));
	ufbxt_assert(!strcmp(material->fbx.transparency_color.texture->relative_filename.data, "textures\\checkerboard_transparency.png"));
	ufbxt_assert(!strcmp(material->fbx.bump.texture->relative_filename.data, "textures\\checkerboard_bump.png"));

	material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "Special");
	ufbxt_assert(material->shader_type == UFBX_SHADER_FBX_PHONG);
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

	ufbxt_assert(material->shader_type == UFBX_SHADER_ARNOLD);
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

	ufbxt_assert(material->shader_type == UFBX_SHADER_FBX_PHONG);
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

	ufbxt_assert(material->shader_type == UFBX_SHADER_BLENDER_PHONG);
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

	ufbxt_assert(material->shader_type == UFBX_SHADER_BLENDER_PHONG);
	ufbxt_assert_close_real(err, material->fbx.specular_exponent.value.x, 76.913f);
	ufbxt_assert_close_real(err, material->fbx.transparency_factor.value.x, 0.544f);
	ufbxt_assert_close_real(err, material->pbr.opacity.value.x, 0.456f);
	ufbxt_assert_close_real(err, material->pbr.roughness.value.x, 0.123f);
}
#endif

UFBXT_FILE_TEST(maya_different_shaders)
#if UFBXT_IMPL
{
	ufbx_material *lambert1 = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "lambert1");
	ufbx_material *phong1 = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "phong1");
	ufbx_material *arnold = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "aiStandardSurface1");
	ufbxt_assert(lambert1 && phong1 && arnold);

	ufbx_vec3 r = { 1.0f, 0.0f, 0.0f };
	ufbx_vec3 g = { 0.0f, 1.0f, 0.0f };
	ufbx_vec3 b = { 0.0f, 0.0f, 1.0f };

	ufbxt_assert(lambert1->shader_type == UFBX_SHADER_FBX_LAMBERT);
	ufbxt_assert(!strcmp(lambert1->shading_model_name.data, "lambert"));
	ufbxt_assert_close_vec3(err, lambert1->fbx.diffuse_color.value, g);
	ufbxt_assert_close_vec3(err, lambert1->pbr.base_color.value, g);
	ufbxt_assert_close_real(err, lambert1->pbr.specular_factor.value.x, 0.0f);

	ufbxt_assert(phong1->shader_type == UFBX_SHADER_FBX_PHONG);
	ufbxt_assert(!strcmp(phong1->shading_model_name.data, "phong"));
	ufbxt_assert_close_vec3(err, phong1->fbx.diffuse_color.value, b);
	ufbxt_assert_close_vec3(err, phong1->pbr.base_color.value, b);
	ufbxt_assert_close_real(err, phong1->pbr.specular_factor.value.x, 1.0f);

	ufbxt_assert(arnold->shader_type == UFBX_SHADER_ARNOLD);
	ufbxt_assert(!strcmp(arnold->shading_model_name.data, "unknown"));
	ufbxt_assert_close_vec3(err, arnold->pbr.base_color.value, r);
}
#endif

UFBXT_TEST(blender_phong_quirks)
#if UFBXT_IMPL
{
	for (int quirks = 0; quirks <= 1; quirks++) {
		ufbx_load_opts opts = { 0 };
		opts.disable_quirks = (quirks == 0);

		char buf[512];
		snprintf(buf, sizeof(buf), "%s%s", data_root, "blender_293_textures_7400_binary.fbx");

		ufbx_scene *scene = ufbx_load_file(buf, &opts, NULL);
		ufbxt_assert(scene);

		// Exporter should be detected even with quirks off
		ufbxt_assert(scene->metadata.exporter == UFBX_EXPORTER_BLENDER_BINARY);
		ufbxt_assert(scene->metadata.exporter_version == ufbx_pack_version(4, 22, 0));

		ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "Material.001");
		ufbxt_assert(material);
		if (quirks) {
			ufbxt_assert(material->shader_type == UFBX_SHADER_BLENDER_PHONG);
		} else {
			ufbxt_assert(material->shader_type == UFBX_SHADER_FBX_PHONG);
		}

		ufbx_free_scene(scene);
	}
}
#endif

UFBXT_FILE_TEST(maya_arnold_properties)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "aiStandardSurface1");
	ufbxt_assert(material);

	ufbxt_assert( 1 == (int)round(100.0f * material->pbr.base_factor.value.x));
	ufbxt_assert( 2 == (int)round(100.0f * material->pbr.base_color.value.x));
	ufbxt_assert( 3 == (int)round(100.0f * material->pbr.base_color.value.y));
	ufbxt_assert( 4 == (int)round(100.0f * material->pbr.base_color.value.z));
	ufbxt_assert( 5 == (int)round(100.0f * material->pbr.diffuse_roughness.value.x));
	ufbxt_assert( 6 == (int)round(100.0f * material->pbr.metallic.value.x));
	ufbxt_assert( 7 == (int)round(100.0f * material->pbr.specular_factor.value.x));
	ufbxt_assert( 8 == (int)round(100.0f * material->pbr.specular_color.value.x));
	ufbxt_assert( 9 == (int)round(100.0f * material->pbr.specular_color.value.y));
	ufbxt_assert(10 == (int)round(100.0f * material->pbr.specular_color.value.z));
	ufbxt_assert(11 == (int)round(100.0f * material->pbr.roughness.value.x));
	ufbxt_assert(12 == (int)round(100.0f * material->pbr.specular_ior.value.x));
	ufbxt_assert(13 == (int)round(100.0f * material->pbr.specular_anisotropy.value.x));
	ufbxt_assert(14 == (int)round(100.0f * material->pbr.specular_rotation.value.x));
	ufbxt_assert(15 == (int)round(100.0f * material->pbr.transmission_factor.value.x));
	ufbxt_assert(16 == (int)round(100.0f * material->pbr.transmission_color.value.x));
	ufbxt_assert(17 == (int)round(100.0f * material->pbr.transmission_color.value.y));
	ufbxt_assert(18 == (int)round(100.0f * material->pbr.transmission_color.value.z));
	ufbxt_assert(19 == (int)round(100.0f * material->pbr.transmission_depth.value.x));
	ufbxt_assert(20 == (int)round(100.0f * material->pbr.transmission_scatter.value.x));
	ufbxt_assert(21 == (int)round(100.0f * material->pbr.transmission_scatter.value.y));
	ufbxt_assert(22 == (int)round(100.0f * material->pbr.transmission_scatter.value.z));
	ufbxt_assert(23 == (int)round(100.0f * material->pbr.transmission_scatter_anisotropy.value.x));
	ufbxt_assert(24 == (int)round(100.0f * material->pbr.transmission_dispersion.value.x));
	ufbxt_assert(25 == (int)round(100.0f * material->pbr.transmission_roughness.value.x));
	ufbxt_assert(26 == (int)round(100.0f * material->pbr.subsurface_factor.value.x));
	ufbxt_assert(27 == (int)round(100.0f * material->pbr.subsurface_color.value.x));
	ufbxt_assert(28 == (int)round(100.0f * material->pbr.subsurface_color.value.y));
	ufbxt_assert(29 == (int)round(100.0f * material->pbr.subsurface_color.value.z));
	ufbxt_assert(30 == (int)round(100.0f * material->pbr.subsurface_radius.value.x));
	ufbxt_assert(31 == (int)round(100.0f * material->pbr.subsurface_radius.value.y));
	ufbxt_assert(32 == (int)round(100.0f * material->pbr.subsurface_radius.value.z));
	ufbxt_assert(33 == (int)round(100.0f * material->pbr.subsurface_scale.value.x));
	ufbxt_assert(34 == (int)round(100.0f * material->pbr.subsurface_anisotropy.value.x));
	ufbxt_assert(35 == (int)round(100.0f * material->pbr.coat_factor.value.x));
	ufbxt_assert(36 == (int)round(100.0f * material->pbr.coat_color.value.x));
	ufbxt_assert(37 == (int)round(100.0f * material->pbr.coat_color.value.y));
	ufbxt_assert(38 == (int)round(100.0f * material->pbr.coat_color.value.z));
	ufbxt_assert(39 == (int)round(100.0f * material->pbr.coat_roughness.value.x));
	ufbxt_assert(40 == (int)round(100.0f * material->pbr.coat_ior.value.x));
	ufbxt_assert(41 == (int)round(100.0f * material->pbr.coat_anisotropy.value.x));
	ufbxt_assert(42 == (int)round(100.0f * material->pbr.coat_rotation.value.x));
	ufbxt_assert(43 == (int)round(100.0f * material->pbr.coat_normal.value.x));
	ufbxt_assert(44 == (int)round(100.0f * material->pbr.coat_normal.value.y));
	ufbxt_assert(45 == (int)round(100.0f * material->pbr.coat_normal.value.z));
	ufbxt_assert(46 == (int)round(100.0f * material->pbr.sheen_factor.value.x));
	ufbxt_assert(47 == (int)round(100.0f * material->pbr.sheen_color.value.x));
	ufbxt_assert(48 == (int)round(100.0f * material->pbr.sheen_color.value.y));
	ufbxt_assert(49 == (int)round(100.0f * material->pbr.sheen_color.value.z));
	ufbxt_assert(50 == (int)round(100.0f * material->pbr.sheen_roughness.value.x));
	ufbxt_assert(51 == (int)round(100.0f * material->pbr.emission_factor.value.x));
	ufbxt_assert(52 == (int)round(100.0f * material->pbr.emission_color.value.x));
	ufbxt_assert(53 == (int)round(100.0f * material->pbr.emission_color.value.y));
	ufbxt_assert(54 == (int)round(100.0f * material->pbr.emission_color.value.z));
	ufbxt_assert(55 == (int)round(100.0f * material->pbr.thin_film_thickness.value.x));
	ufbxt_assert(56 == (int)round(100.0f * material->pbr.thin_film_ior.value.x));
	ufbxt_assert(57 == (int)round(100.0f * material->pbr.opacity.value.x));
	ufbxt_assert(58 == (int)round(100.0f * material->pbr.opacity.value.y));
	ufbxt_assert(59 == (int)round(100.0f * material->pbr.opacity.value.z));
	ufbxt_assert(60 == (int)round(100.0f * material->pbr.tangent_map.value.x));
	ufbxt_assert(61 == (int)round(100.0f * material->pbr.tangent_map.value.y));
	ufbxt_assert(62 == (int)round(100.0f * material->pbr.tangent_map.value.z));
	ufbxt_assert(63 == (int)round(100.0f * material->pbr.indirect_diffuse.value.x));
	ufbxt_assert(64 == (int)round(100.0f * material->pbr.indirect_specular.value.x));
	ufbxt_assert(65 == (int)round(100.0f * material->pbr.matte_color.value.x));
	ufbxt_assert(66 == (int)round(100.0f * material->pbr.matte_color.value.y));
	ufbxt_assert(67 == (int)round(100.0f * material->pbr.matte_color.value.z));
	ufbxt_assert(68 == (int)round(100.0f * material->pbr.matte_factor.value.x));
	ufbxt_assert(69 == material->pbr.transmission_priority.value_int);

	ufbxt_assert(material->pbr.subsurface_type.value_int == 1);
	ufbxt_assert(material->pbr.transmission_enable_in_aov.value_int != 0);
	ufbxt_assert(material->pbr.thin_walled.value_int != 0);
	ufbxt_assert(material->pbr.matte_enabled.value_int != 0);
	ufbxt_assert(material->pbr.caustics.value_int != 0);
	ufbxt_assert(material->pbr.internal_reflections.value_int != 0);
	ufbxt_assert(material->pbr.exit_to_background.value_int != 0);
}
#endif
