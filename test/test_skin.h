
#if UFBXT_IMPL
void ufbxt_check_frame(ufbx_scene *scene, ufbxt_diff_error *err, const char *file_name, const char *anim_name, ufbx_real time)
{
	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s.obj", data_root, file_name);
	size_t obj_size = 0;
	void *obj_data = ufbxt_read_file(buf, &obj_size);
	ufbxt_obj_file *obj_file = obj_data ? ufbxt_load_obj(obj_data, obj_size) : NULL;
	ufbxt_assert(obj_file);
	free(obj_data);

	ufbx_evaluate_opts opts = { 0 };

	opts.evaluate_skinned_vertices = true;

	if (anim_name) {
		for (size_t i = 0; i < scene->anim_layers.size; i++) {
			if (strstr(scene->anim_layers.data[i].name.data, anim_name)) {
				opts.layer = &scene->anim_layers.data[i];
				break;
			}
		}
		ufbxt_assert(opts.layer);
	}

	ufbx_scene *eval = ufbx_evaluate_scene(scene, &opts, time);
	ufbxt_assert(eval);

	ufbxt_check_scene(eval);

	ufbxt_diff_to_obj(eval, obj_file, err, true);

	ufbx_free_scene(eval);
	free(obj_file);
}
#endif

UFBXT_FILE_TEST(blender_279_sausage)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, "blender_279_sausage_base_0", "Base", 0.0);
	ufbxt_check_frame(scene, err, "blender_279_sausage_wiggle_20", "Wiggle", 20.0/24.0);
	ufbxt_check_frame(scene, err, "blender_279_sausage_spin_15", "Spin", 15.0/24.0);
}
#endif

UFBXT_FILE_TEST(maya_game_sausage)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST_SUFFIX(maya_game_sausage, wiggle)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, "maya_game_sausage_wiggle_10", NULL, 10.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_wiggle_18", NULL, 18.0/24.0);
}
#endif

UFBXT_FILE_TEST_SUFFIX(maya_game_sausage, spin)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, "maya_game_sausage_spin_7", NULL, 27.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_spin_15", NULL, 35.0/24.0);
}
#endif

UFBXT_FILE_TEST_SUFFIX(maya_game_sausage, deform)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, "maya_game_sausage_deform_8", NULL, 48.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_deform_15", NULL, 55.0/24.0);
}
#endif

UFBXT_FILE_TEST_SUFFIX(maya_game_sausage, combined)
#if UFBXT_IMPL
{
	// TODO: These need proper AnimationStack support..
#if 0
	ufbxt_check_frame(scene, err, "maya_game_sausage_wiggle_10", "wiggle", 10.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_wiggle_18", "wiggle", 18.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_spin_7", "spin", 27.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_spin_15", "spin", 35.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_deform_8", NULL, 48.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_deform_15", NULL, 55.0/24.0);
#endif
}
#endif
