
UFBXT_FILE_TEST(max2009_blob)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->lights.count == 3);
	ufbxt_assert(scene->cameras.count == 1);

	{
		ufbx_node *node = ufbx_find_node(scene, "Box01");
		ufbxt_assert(node);
		ufbxt_assert(node->mesh);
		ufbxt_assert(node->children.count == 16);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Omni01");
		ufbxt_assert(node && node->light);
		ufbx_light *light = node->light;
		ufbxt_assert(light->type == UFBX_LIGHT_POINT);
		if (scene->metadata.version < 6000) {
			ufbxt_assert(light->decay == UFBX_LIGHT_DECAY_QUADRATIC);
		}
		ufbx_vec3 color = { 0.172549024224281f, 0.364705890417099f, 1.0f };
		ufbxt_assert_close_vec3(err, light->color, color);
		ufbxt_assert_close_real(err, light->intensity, 1.0f);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Fspot01");
		ufbxt_assert(node && node->light);
		ufbx_light *light = node->light;
		ufbxt_assert(light->type == UFBX_LIGHT_SPOT);
		if (scene->metadata.version < 6000) {
			ufbxt_assert(light->decay == UFBX_LIGHT_DECAY_QUADRATIC);
		}
		ufbx_vec3 color = { 0.972549080848694f ,0.0705882385373116f, 0.0705882385373116f };
		ufbxt_assert_close_vec3(err, light->color, color);
		ufbxt_assert_close_real(err, light->intensity, 1.0f);
		ufbxt_assert_close_real(err, light->outer_angle, 45.0f);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "FDirect02");
		ufbxt_assert(node && node->light);
		ufbx_light *light = node->light;
		ufbxt_assert(light->type == UFBX_LIGHT_DIRECTIONAL);
		if (scene->metadata.version < 6000) {
			ufbxt_assert(light->decay == UFBX_LIGHT_DECAY_NONE);
		}
		ufbx_vec3 color = { 0.533333361148834f ,0.858823597431183f, 0.647058844566345f };
		ufbxt_assert_close_vec3(err, light->color, color);
		ufbxt_assert_close_real(err, light->intensity, 1.0f);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Camera01");
		ufbxt_assert(node && node->camera);
		ufbx_camera *camera = node->camera;
		ufbxt_assert(camera->aspect_mode == UFBX_ASPECT_MODE_WINDOW_SIZE);
		ufbxt_assert(camera->aperture_mode == UFBX_APERTURE_MODE_HORIZONTAL);
		ufbx_vec2 aperture = { 1.41732287406921f ,1.06299209594727f };
		ufbxt_assert_close_real(err, camera->focal_length_mm, 43.4558439883016f);
		ufbxt_assert_close_vec2(err, camera->film_size_inch, aperture);
		ufbxt_assert_close_vec2(err, camera->aperture_size_inch, aperture);
	}

	ufbxt_check_frame(scene, err, false, "max2009_blob_8", NULL, 8.0/30.0);
	ufbxt_check_frame(scene, err, false, "max2009_blob_18", NULL, 18.0/30.0);
}
#endif

UFBXT_FILE_TEST(max2009_sausage)
#if UFBXT_IMPL
{
}
#endif
