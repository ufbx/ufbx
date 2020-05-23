
#if UFBXT_IMPL
void ufbxt_check_stack_times(ufbx_scene *scene, ufbxt_diff_error *err, const char *stack_name, double begin, double end)
{
	ufbx_anim_stack *stack = ufbx_find_anim_stack(scene, stack_name);
	ufbxt_assert(stack);
	ufbxt_assert(!strcmp(stack->name.data, stack_name));
	ufbxt_assert_close_real(err, (ufbx_real)stack->time_begin, (ufbx_real)begin);
	ufbxt_assert_close_real(err, (ufbx_real)stack->time_end, (ufbx_real)end);
}

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
		for (size_t i = 0; i < scene->anim_stacks.size; i++) {
			ufbx_anim_stack *stack = &scene->anim_stacks.data[i];
			if (strstr(stack->name.data, anim_name)) {
				ufbxt_assert(stack->layers.size > 0);
				opts.layer = stack->layers.data[0];
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
	if (scene->metadata.ascii) {
		// ???: In the 6100 ASCII file Spin Take starts from -1 frames
		ufbxt_check_stack_times(scene, err, "Base", 0.0, 1.0/24.0);
		ufbxt_check_stack_times(scene, err, "Spin", -1.0/24.0, 18.0/24.0);
		ufbxt_check_stack_times(scene, err, "Wiggle", 0.0, 19.0/24.0);
	} else {
		ufbxt_check_stack_times(scene, err, "Skeleton|Base", 0.0, 1.0/24.0);
		ufbxt_check_stack_times(scene, err, "Skeleton|Spin", 0.0, 19.0/24.0);
		ufbxt_check_stack_times(scene, err, "Skeleton|Wiggle", 0.0, 19.0/24.0);
	}

	ufbxt_check_frame(scene, err, "blender_279_sausage_base_0", "Base", 0.0);
	ufbxt_check_frame(scene, err, "blender_279_sausage_spin_15", "Spin", 15.0/24.0);
	ufbxt_check_frame(scene, err, "blender_279_sausage_wiggle_20", "Wiggle", 20.0/24.0);
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
	ufbxt_check_stack_times(scene, err, "wiggle", 1.0/24.0, 20.0/24.0);
	ufbxt_check_stack_times(scene, err, "spin", 20.0/24.0, 40.0/24.0);
	ufbxt_check_stack_times(scene, err, "deform", 40.0/24.0, 60.0/24.0);

	ufbxt_check_frame(scene, err, "maya_game_sausage_wiggle_10", "wiggle", 10.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_wiggle_18", "wiggle", 18.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_spin_7", "spin", 27.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_spin_15", "spin", 35.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_deform_8", "deform", 48.0/24.0);
	ufbxt_check_frame(scene, err, "maya_game_sausage_deform_15", "deform", 55.0/24.0);
}
#endif
