
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

UFBXT_FILE_TEST(maya_resampled)
#if UFBXT_IMPL
{
	static const ufbx_real values6[] = {
		0,0,0,0,0,0,0,0,0,
		-0.004, -0.022, -0.056, -0.104, -0.166, -0.241, -0.328, -0.427, -0.536, -0.654, -0.783,
		-0.919, -1.063, -1.214, -1.371, -1.533, -1.700, -1.871, -2.044, -2.220, -2.398, -2.577,
		-2.755, -2.933, -3.109, -3.283, -3.454, -3.621, -3.784, -3.941, -4.093, -4.237, -4.374,
		-4.503, -4.623, -4.733, -4.832, -4.920, -4.996, -5.059, -5.108, -5.143, -5.168, -5.186,
		-5.200, -5.209, -5.215, -5.218, -5.220, -5.220, -5.216, -5.192, -5.151, -5.091, -5.013,
		-4.919, -4.810, -4.686,
	};

	static const ufbx_real values7[] = {
		0,0,0,0,0,0,0,0,
		0.000, -0.004, -0.025, -0.061, -0.112, -0.176, -0.252, -0.337, -0.431, -0.533, -0.648, 
		-0.776, -0.915, -1.064, -1.219, -1.378, -1.539, -1.700, -1.865, -2.037, -2.216, -2.397, -2.580, 
		-2.761, -2.939, -3.111, -3.278, -3.447, -3.615, -3.782, -3.943, -4.098, -4.244, -4.379, 
		-4.500, -4.614, -4.722, -4.821, -4.911, -4.990, -5.056, -5.107, -5.143, -5.168, -5.186, -5.200, 
		-5.209, -5.215, -5.218, -5.220, -5.220, -5.215, -5.190, -5.145, -5.082, -5.002, -4.908, 
		-4.800, -4.680, -4.550, -4.403, -4.239, 
	};

	const ufbx_real *values = scene->metadata.version >= 7000 ? values7 : values6;
	size_t num_values = scene->metadata.version >= 7000 ? ufbxt_arraycount(values7) : ufbxt_arraycount(values6);

	ufbxt_assert(scene->anim_layers.size == 1);
	ufbx_anim_layer *layer = &scene->anim_layers.data[0];
	for (size_t i = 0; i < layer->props.size; i++) {
		ufbx_anim_prop *prop = &layer->props.data[i];
		if (strcmp(prop->name.data, "Lcl Translation")) continue;
		ufbx_anim_curve *curve = &prop->curves[0];

		for (size_t i = 0; i < num_values; i++) {
			double time = (double)i * (1.0/200.0);
			ufbx_real value = ufbx_evaluate_curve(curve, time);
			ufbxt_assert_close_real(err, value, values[i]);
		}
	}
}
#endif

#if UFBXT_IMPL

typedef struct {
	int frame;
	ufbx_real intensity;
	ufbx_vec3 color;
} ufbxt_anim_light_ref;

#endif

UFBXT_FILE_TEST(maya_anim_light)
#if UFBXT_IMPL
{
	ufbx_scene *state = NULL;

	static const ufbxt_anim_light_ref refs[] = {
		{  0, 3.072, { 0.148, 0.095, 0.440 } },
		{ 12, 1.638, { 0.102, 0.136, 0.335 } },
		{ 24, 1.948, { 0.020, 0.208, 0.149 } },
		{ 32, 3.676, { 0.010, 0.220, 0.113 } },
		{ 40, 4.801, { 0.118, 0.195, 0.115 } },
		{ 48, 3.690, { 0.288, 0.155, 0.117 } },
		{ 56, 1.565, { 0.421, 0.124, 0.119 } },
		{ 60, 1.145, { 0.442, 0.119, 0.119 } },
	};

	ufbx_evaluate_opts opts = { 0 };
	for (size_t i = 0; i < ufbxt_arraycount(refs); i++) {
		const ufbxt_anim_light_ref *ref = &refs[i];
		opts.reuse_scene = state;

		double time = ref->frame * (1.0/24.0);
		state = ufbx_evaluate_scene(scene, &opts, time);
		ufbxt_assert(state);

		ufbxt_check_scene(state);

		ufbx_light *light = ufbx_find_light(state, "pointLight1");
		ufbxt_assert(light);

		ufbxt_assert_close_real(err, light->intensity * 0.01f, ref->intensity);
		ufbxt_assert_close_vec3(err, light->color, ref->color);
	}

	ufbx_free_scene(state);
}
#endif

UFBXT_FILE_TEST(maya_anim_layers)
#if UFBXT_IMPL
{
	ufbx_anim_layer *x = ufbx_find_anim_layer(scene, "X");
	ufbx_anim_layer *y = ufbx_find_anim_layer(scene, "Y");
	ufbxt_assert(x && y);
	ufbxt_assert(y->compose_rotation == false);
	ufbxt_assert(y->compose_scale == false);
}
#endif

UFBXT_FILE_TEST(maya_anim_layers_acc)
#if UFBXT_IMPL
{
	ufbx_anim_layer *x = ufbx_find_anim_layer(scene, "X");
	ufbx_anim_layer *y = ufbx_find_anim_layer(scene, "Y");
	ufbxt_assert(x && y);
	ufbxt_assert(y->compose_rotation == true);
	ufbxt_assert(y->compose_scale == true);
}
#endif

UFBXT_FILE_TEST(maya_anim_layers_over)
#if UFBXT_IMPL
{
	ufbx_anim_layer *x = ufbx_find_anim_layer(scene, "X");
	ufbx_anim_layer *y = ufbx_find_anim_layer(scene, "Y");
	ufbxt_assert(x && y);
	ufbxt_assert(y->compose_rotation == false);
	ufbxt_assert(y->compose_scale == false);
}
#endif

UFBXT_FILE_TEST(maya_anim_layers_over_acc)
#if UFBXT_IMPL
{
	ufbx_anim_layer *x = ufbx_find_anim_layer(scene, "X");
	ufbx_anim_layer *y = ufbx_find_anim_layer(scene, "Y");
	ufbxt_assert(x && y);
	ufbxt_assert(y->compose_rotation == true);
	ufbxt_assert(y->compose_scale == true);
}
#endif

