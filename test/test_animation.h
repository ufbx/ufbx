
#if UFBXT_IMPL
typedef struct {
	int frame;
	double value;
} ufbxt_key_ref;
#endif

UFBXT_FILE_TEST(maya_interpolation_modes)
#if UFBXT_IMPL
{
	// Curve evaluated values at 24fps
	static const ufbx_real values[] = {
		-8.653366, // Start from zero time
		-8.653366,-8.602998,-8.464664,-8.257528,-8.00075,-7.713489,-7.414906,-7.124163,-6.86042,
		-6.642837,-6.490576,-6.388305,-6.306414,-6.242637,-6.19471,-6.160368,-6.137348,-6.123385,
		-6.116215,-6.113573,-6.113196,-5.969524,-5.825851,-5.682179,-5.538507,-5.394835,-5.251163,
		-5.107491,-4.963819,-4.820146,-4.676474,-4.532802,-4.38913,-4.245458,-4.101785,-3.958113,-4.1529,
		-4.347686,-4.542472,-4.737258,-4.932045,-5.126832,-5.321618,-5.516404,-5.71119,-5.905977,-5.767788,
		-5.315578,-4.954943,-4.83559,-4.856855,-4.960766,-5.118543,-4.976541,-4.885909,-4.865979,-4.93845,
		-5.099224,-5.270246,-5.359269,-5.349404,-5.261964,-5.118543,-5.264501,-5.33535,-5.285445,-5.058857,
		-4.69383,-4.357775,-4.124978,-3.981697,-3.904232,-3.875225,-3.875225,-3.875225,-3.875225,-3.875225,
		-3.875225,-3.875225,-2.942738,-2.942738,-2.942738,-2.942738,-2.942738,-2.942738,-2.942738,-2.942738,
		-2.942738,-1.243537,-1.243537,-1.243537,-1.243537,-1.243537,-1.243537,-1.243537,5.603338,5.603338,
		5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,
		5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338,5.603338
	};

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
			ufbxt_assert_close_real(err, keys[i].time, (double)key_ref[i].frame / 24.0);
			ufbxt_assert_close_real(err, keys[i].value, key_ref[i].value);
			if (i > 0) ufbxt_assert(keys[i].left.dx > 0.0f);
			if (i + 1 < num_keys) ufbxt_assert(keys[i].right.dx > 0.0f);
		}

		ufbxt_assert(keys[0].interpolation == UFBX_INTERPOLATION_CUBIC);
		ufbxt_assert(keys[0].right.dy == 0.0f);
		ufbxt_assert(keys[1].interpolation == UFBX_INTERPOLATION_CUBIC);
		ufbxt_assert_close_real(err, keys[1].left.dy/keys[1].left.dx, keys[1].right.dy/keys[1].left.dx);
		ufbxt_assert(keys[2].interpolation == UFBX_INTERPOLATION_LINEAR);
		ufbxt_assert_close_real(err, keys[3].left.dy/keys[3].left.dx, keys[2].right.dy/keys[2].right.dx);
		ufbxt_assert(keys[3].interpolation == UFBX_INTERPOLATION_LINEAR);
		ufbxt_assert_close_real(err, keys[4].left.dy/keys[4].left.dx, keys[3].right.dy/keys[3].right.dx);
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

		for (size_t i = 0; i < ufbxt_arraycount(values); i++) {
			// Round up to the next frame to make stepped tangents consistent
			double time = (double)i * (1.0/24.0) + 0.000001;
			ufbx_real value = ufbx_evaluate_curve(curve, time);
			ufbxt_assert_close_real(err, value, values[i]);
		}
	}
}
#endif

UFBXT_FILE_TEST(maya_auto_clamp)
#if UFBXT_IMPL
{
	// Curve evaluated values at 24fps
	static const ufbx_real values[] = {
		0.000, 0.000, 0.273, 0.515, 0.718, 0.868, 0.945, 0.920, 0.779, 0.611,
		0.591, 0.747, 1.206, 2.059, 3.191, 4.489, 5.837, 7.121, 8.228, 9.042,
		9.449, 9.694, 10.128, 10.610, 10.873, 10.927, 10.854, 10.704, 10.502,
		10.264, 10.000,
	};

	ufbxt_assert(scene->anim_layers.size == 1);
	ufbx_anim_layer *layer = &scene->anim_layers.data[0];
	for (size_t i = 0; i < layer->props.size; i++) {
		ufbx_anim_prop *prop = &layer->props.data[i];
		if (strcmp(prop->name.data, "Lcl Translation")) continue;
		ufbx_anim_curve *curve = &prop->curves[0];
		ufbxt_assert(curve->keyframes.size == 4);

		for (size_t i = 0; i < ufbxt_arraycount(values); i++) {
			double time = (double)i * (1.0/24.0);
			ufbx_real value = ufbx_evaluate_curve(curve, time);
			ufbxt_assert_close_real(err, value, values[i]);
		}
	}
}
#endif

UFBXT_FILE_TEST(synthetic_missing_version)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->metadata.version == 6100);
}
#endif
