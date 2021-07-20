
#if UFBXT_IMPL
void ufbxt_check_stack_times(ufbx_scene *scene, ufbxt_diff_error *err, const char *stack_name, double begin, double end)
{
	ufbx_anim_stack *stack = ufbx_find_anim_stack(scene, stack_name);
	ufbxt_assert(stack);
	ufbxt_assert(!strcmp(stack->name.data, stack_name));
	ufbxt_assert_close_real(err, (ufbx_real)stack->time_begin, (ufbx_real)begin);
	ufbxt_assert_close_real(err, (ufbx_real)stack->time_end, (ufbx_real)end);
}

void ufbxt_check_frame(ufbx_scene *scene, ufbxt_diff_error *err, double min_normal_dot, const char *file_name, const char *anim_name, ufbx_real time)
{
	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s.obj", data_root, file_name);

	ufbxt_hintf("Frame from '%s' %s time %.2fs",
		anim_name ? anim_name : "(implicit animation)",
		buf, time);

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

	ufbxt_diff_to_obj(eval, obj_file, err, min_normal_dot);

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

	ufbxt_check_frame(scene, err, 0.0, "blender_279_sausage_base_0", "Base", 0.0);
	ufbxt_check_frame(scene, err, 0.0, "blender_279_sausage_spin_15", "Spin", 15.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "blender_279_sausage_wiggle_20", "Wiggle", 20.0/24.0);
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
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_wiggle_10", NULL, 10.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_wiggle_18", NULL, 18.0/24.0);
}
#endif

UFBXT_FILE_TEST_SUFFIX(maya_game_sausage, spin)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_spin_7", NULL, 27.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_spin_15", NULL, 35.0/24.0);
}
#endif

UFBXT_FILE_TEST_SUFFIX(maya_game_sausage, deform)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_deform_8", NULL, 48.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_deform_15", NULL, 55.0/24.0);
}
#endif

UFBXT_FILE_TEST_SUFFIX(maya_game_sausage, combined)
#if UFBXT_IMPL
{
	ufbxt_check_stack_times(scene, err, "wiggle", 1.0/24.0, 20.0/24.0);
	ufbxt_check_stack_times(scene, err, "spin", 20.0/24.0, 40.0/24.0);
	ufbxt_check_stack_times(scene, err, "deform", 40.0/24.0, 60.0/24.0);

	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_wiggle_10", "wiggle", 10.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_wiggle_18", "wiggle", 18.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_spin_7", "spin", 27.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_spin_15", "spin", 35.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_deform_8", "deform", 48.0/24.0);
	ufbxt_check_frame(scene, err, 0.0, "maya_game_sausage_deform_15", "deform", 55.0/24.0);
}
#endif


UFBXT_FILE_TEST(maya_blend_shape_cube)
#if UFBXT_IMPL
{
	ufbx_mesh *mesh = ufbx_find_mesh(scene, "pCube1");

	ufbxt_assert(mesh);
	ufbxt_assert(mesh->blend_channels.size == 2);

	ufbx_blend_channel *top[2] = { NULL, NULL };
	for (size_t i = 0; i < mesh->blend_channels.size; i++) {
		ufbx_blend_channel *chan = mesh->blend_channels.data[i];
		if (strstr(chan->name.data, "TopH")) top[0] = chan;
		if (strstr(chan->name.data, "TopV")) top[1] = chan;
	}
	ufbxt_assert(top[0] && top[1]);

	ufbxt_assert_close_real(err, top[0]->weight, 1.0);
	ufbxt_assert_close_real(err, top[1]->weight, 1.0);

	double keyframes[][3] = {
		{ 1.0/24.0, 1.0, 1.0 },
		{ 16.0/24.0, 0.279, 0.670 },
		{ 53.0/24.0, 0.901, 0.168 },
		{ 120.0/24.0, 1.0, 1.0 },
	};

	for (size_t chan_ix = 0; chan_ix < 2; chan_ix++) {
		ufbx_blend_channel *chan = top[chan_ix];
		ufbxt_assert(chan->keyframes.size == 1);

		ufbxt_assert_close_real(err, chan->keyframes.data[0].target_weight, 1.0);
		ufbx_blend_shape *shape = chan->keyframes.data[0].shape;

		ufbx_anim_prop *props = ufbx_find_blend_channel_anim_prop_begin(scene, NULL, chan);
		ufbxt_assert(props);

		size_t count = ufbx_anim_prop_count(props);
		ufbxt_assert(count == 1);

		ufbx_anim_prop *percent = ufbx_find_anim_prop(props, "DeformPercent");
		ufbxt_assert(percent);

		for (size_t key_ix = 0; key_ix < ufbxt_arraycount(keyframes); key_ix++) {
			double *frame = keyframes[key_ix];
			double time = frame[0];

			ufbx_real ref = (ufbx_real)frame[1 + chan_ix];
			ufbx_real value = ufbx_evaluate_curve(&percent->curves[0], time) / 100.0;
			ufbxt_assert_close_real(err, value, ref);
		}
	}

	ufbx_scene *eval = NULL;
	for (int eval_skin = 0; eval_skin <= 1; eval_skin++) {
		for (size_t key_ix = 0; key_ix < ufbxt_arraycount(keyframes); key_ix++) {
			double *frame = keyframes[key_ix];
			double time = frame[0];

			ufbx_evaluate_opts opts = { 0 };

			opts.reuse_scene = eval;
			opts.evaluate_skinned_vertices = eval_skin != 0;

			eval = ufbx_evaluate_scene(scene, &opts, time);
			ufbxt_assert(eval);

			for (size_t chan_ix = 0; chan_ix < 2; chan_ix++) {
				ufbx_real ref = (ufbx_real)frame[1 + chan_ix];

				ufbx_blend_channel *chan = ufbx_find_blend_channel(eval, top[chan_ix]->name.data);
				ufbxt_assert(chan);

				ufbx_prop *prop = ufbx_find_prop(&chan->props, "DeformPercent");
				ufbxt_assert(prop);

				ufbxt_assert(chan->keyframes.size == 1);
				ufbxt_assert_close_real(err, chan->keyframes.data[0].effective_weight, chan->weight);

				ufbxt_assert_close_real(err, prop->value_real / 100.0, ref);
				ufbxt_assert_close_real(err, chan->weight, ref);
			}

			ufbxt_check_scene(eval);
		}
	}

	ufbx_free_scene(eval);
}
#endif


UFBXT_FILE_TEST(maya_blend_inbetween)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, -1.0, "maya_blend_inbetween_1", NULL, 1.0/24.0);
	ufbxt_check_frame(scene, err, -1.0, "maya_blend_inbetween_30", NULL, 30.0/24.0);
	ufbxt_check_frame(scene, err, -1.0, "maya_blend_inbetween_60", NULL, 60.0/24.0);
	ufbxt_check_frame(scene, err, -1.0, "maya_blend_inbetween_65", NULL, 65.0/24.0);
	ufbxt_check_frame(scene, err, -1.0, "maya_blend_inbetween_71", NULL, 71.0/24.0);
	ufbxt_check_frame(scene, err, -1.0, "maya_blend_inbetween_80", NULL, 80.0/24.0);
	ufbxt_check_frame(scene, err, -1.0, "maya_blend_inbetween_89", NULL, 89.0/24.0);
	ufbxt_check_frame(scene, err, -1.0, "maya_blend_inbetween_120", NULL, 120.0/24.0);
}
#endif
