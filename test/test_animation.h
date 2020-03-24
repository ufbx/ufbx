
#if UFBXT_IMPL
typedef struct {
	int frame;
	double value;
} ufbxt_key_ref;
#endif

UFBXT_FILE_TEST(maya_interpolation_modes)
#if UFBXT_IMPL
{
	ufbxt_diff_error err = { 0 };

	ufbxt_assert(scene->anim_layers.size == 1);
	ufbx_anim_layer *layer = &scene->anim_layers.data[0];
	for (size_t i = 0; i < layer->props.size; i++) {
		ufbx_anim_prop *prop = &layer->props.data[i];
		if (strcmp(prop->name.data, "Lcl Translation")) continue;
		ufbx_anim_curve *curve = &prop->curves[0];

		size_t num_keys = 12;
		ufbxt_assert(curve->keyframes.size == num_keys);
		ufbx_keyframe *keys = curve->keyframes.data;

		static const ufbxt_key_ref key_ref[] = {
			{ 1, -8.653366 },
			{ 11, -6.490576 },
			{ 21, -6.113196 },
			{ 36, -3.958113 },
			{ 46, -5.905977 },
			{ 53, -5.118543 },
			{ 63, -5.118543 },
			{ 73, -3.875225 },
			{ 80, -2.942738 },
			{ 89, -1.927362 },
			{ 96, -1.243537 },
			{ 120, 5.603338 },
		};
		ufbxt_assert(ufbxt_arraycount(key_ref) == num_keys);

		for (size_t i = 0; i < num_keys; i++) {
			ufbxt_assert_close_real(&err, keys[i].time, (double)key_ref[i].frame / 24.0);
			ufbxt_assert_close_real(&err, keys[i].value, key_ref[i].value);
			if (i > 0) ufbxt_assert(keys[i].left.dx > 0.0f);
			if (i + 1 < num_keys) ufbxt_assert(keys[i].right.dx > 0.0f);
		}

		ufbxt_assert(keys[0].interpolation == UFBX_INTERPOLATION_CUBIC);
		ufbxt_assert(keys[0].right.dy == 0.0f);
		ufbxt_assert(keys[1].interpolation == UFBX_INTERPOLATION_CUBIC);
		ufbxt_assert_close_real(&err, keys[1].left.dy/keys[1].left.dx, keys[1].right.dy/keys[1].left.dx);
		ufbxt_assert(keys[2].interpolation == UFBX_INTERPOLATION_LINEAR);
		ufbxt_assert_close_real(&err, keys[3].left.dy/keys[3].left.dx, keys[2].right.dy/keys[2].right.dx);
		ufbxt_assert(keys[3].interpolation == UFBX_INTERPOLATION_LINEAR);
		ufbxt_assert_close_real(&err, keys[4].left.dy/keys[4].left.dx, keys[3].right.dy/keys[3].right.dx);
		ufbxt_assert(keys[4].interpolation == UFBX_INTERPOLATION_CUBIC);
		ufbxt_assert(keys[4].right.dy == 0.0f);
		ufbxt_assert(keys[5].interpolation == UFBX_INTERPOLATION_CUBIC);
		ufbxt_assert(keys[5].left.dy < 0.0f);
		ufbxt_assert(keys[5].right.dy > 0.0f);
		ufbxt_assert(keys[6].interpolation == UFBX_INTERPOLATION_CUBIC);
		ufbxt_assert(keys[6].left.dy > 0.0f);
		ufbxt_assert(keys[6].right.dy < 0.0f);
		ufbxt_assert(keys[7].interpolation == UFBX_INTERPOLATION_CONSTANT_PREV);
		ufbxt_assert(keys[8].interpolation == UFBX_INTERPOLATION_CONSTANT_PREV);
		ufbxt_assert(keys[9].interpolation == UFBX_INTERPOLATION_CONSTANT_NEXT);
		ufbxt_assert(keys[10].interpolation == UFBX_INTERPOLATION_CONSTANT_NEXT);
	}

	ufbx_real avg = err.sum / (ufbx_real)err.num;
	ufbxt_logf(".. Absolute key diff: avg %.3g, max %.3g (%zu tests)", avg, err.max, err.num);
}
#endif
