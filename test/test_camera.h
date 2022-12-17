#undef UFBXT_TEST_GROUP
#define UFBXT_TEST_GROUP "camera"

#if UFBXT_IMPL

static void ufbxt_check_ortho_camera(ufbxt_diff_error *err, ufbx_scene *scene, const char *name, ufbx_gate_fit gate_fit, ufbx_real extent, ufbx_real width, ufbx_real height)
{
	ufbxt_hintf("Cameara %s", name);
	ufbx_node *node = ufbx_find_node(scene, name);
	ufbxt_assert(node && node->camera);
	ufbx_camera *camera = node->camera;

	ufbxt_assert(camera->projection_mode == UFBX_PROJECTION_MODE_ORTHOGRAPHIC);
	ufbxt_assert(camera->gate_fit == gate_fit);
	ufbxt_assert_close_real(err, camera->orthographic_extent, extent);
	ufbxt_assert_close_real(err, camera->orthographic_size.x, width);
	ufbxt_assert_close_real(err, camera->orthographic_size.y, height);
}

#endif

UFBXT_FILE_TEST(maya_ortho_camera_400x200)
#if UFBXT_IMPL
{
	ufbxt_check_ortho_camera(err, scene, "Fill", UFBX_GATE_FIT_FILL, 30.0f, 30.0f, 15.0f);
	ufbxt_check_ortho_camera(err, scene, "Horizontal", UFBX_GATE_FIT_HORIZONTAL, 30.0f, 30.0f, 15.0f);
	ufbxt_check_ortho_camera(err, scene, "Vertical", UFBX_GATE_FIT_VERTICAL, 30.0f, 60.0f, 30.0f);
	ufbxt_check_ortho_camera(err, scene, "Overscan", UFBX_GATE_FIT_OVERSCAN, 30.0f, 60.0f, 30.0f);
}
#endif

UFBXT_FILE_TEST(maya_ortho_camera_200x300)
#if UFBXT_IMPL
{
	ufbxt_check_ortho_camera(err, scene, "Fill", UFBX_GATE_FIT_FILL, 30.0f, 20.0f, 30.0f);
	ufbxt_check_ortho_camera(err, scene, "Horizontal", UFBX_GATE_FIT_HORIZONTAL, 30.0f, 30.0f, 45.0f);
	ufbxt_check_ortho_camera(err, scene, "Vertical", UFBX_GATE_FIT_VERTICAL, 30.0f, 20.0f, 30.0f);
	ufbxt_check_ortho_camera(err, scene, "Overscan", UFBX_GATE_FIT_OVERSCAN, 30.0f, 30.0f, 45.0f);
}
#endif

UFBXT_FILE_TEST(maya_ortho_camera_size)
#if UFBXT_IMPL
{
	ufbxt_check_ortho_camera(err, scene, "Ortho_10", UFBX_GATE_FIT_FILL, 10.0f, 10.0f, 10.0f);
	ufbxt_check_ortho_camera(err, scene, "Ortho_30", UFBX_GATE_FIT_FILL, 30.0f, 30.0f, 30.0f);
	ufbxt_check_ortho_camera(err, scene, "Ortho_35", UFBX_GATE_FIT_FILL, 35.0f, 35.0f, 35.0f);
	ufbxt_check_ortho_camera(err, scene, "Ortho_100", UFBX_GATE_FIT_FILL, 100.0f, 100.0f, 100.0f);
}
#endif

