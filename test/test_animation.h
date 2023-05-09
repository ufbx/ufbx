#undef UFBXT_TEST_GROUP
#define UFBXT_TEST_GROUP "animation"

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
	static const double values[] = {
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

	ufbxt_assert(scene->anim_layers.count == 1);
	ufbx_anim_layer *layer = scene->anim_layers.data[0];
	for (size_t i = 0; i < layer->anim_values.count; i++) {
		ufbx_anim_value *value = layer->anim_values.data[i];
		if (strcmp(value->name.data, "Lcl Translation")) continue;
		ufbx_anim_curve *curve = value->curves[0];

		size_t num_keys = 12;
		ufbxt_assert(curve->keyframes.count == num_keys);
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
			ufbxt_assert_close_double(err, keys[i].time, (double)key_ref[i].frame / 24.0);
			ufbxt_assert_close_real(err, keys[i].value, (ufbx_real)key_ref[i].value);
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
			ufbx_real value = ufbx_evaluate_curve(curve, time, 0.0);
			ufbxt_assert_close_real(err, value, (ufbx_real)values[i]);
		}

		size_t num_samples = 64 * 1024;
		for (size_t i = 0; i < num_samples; i++) {
			double time = (double)i * (5.0 / (double)num_samples);
			ufbx_real value = ufbx_evaluate_curve(curve, time, 0.0);
			ufbxt_assert(value >= -16.0f && value <= 16.0f);
		}
	}
}
#endif

UFBXT_FILE_TEST(maya_auto_clamp)
#if UFBXT_IMPL
{
	// Curve evaluated values at 24fps
	static const double values[] = {
		0.000, 0.000, 0.273, 0.515, 0.718, 0.868, 0.945, 0.920, 0.779, 0.611,
		0.591, 0.747, 1.206, 2.059, 3.191, 4.489, 5.837, 7.121, 8.228, 9.042,
		9.449, 9.694, 10.128, 10.610, 10.873, 10.927, 10.854, 10.704, 10.502,
		10.264, 10.000,
	};

	ufbxt_assert(scene->anim_layers.count == 1);
	ufbx_anim_layer *layer = scene->anim_layers.data[0];
	for (size_t i = 0; i < layer->anim_values.count; i++) {
		ufbx_anim_value *value = layer->anim_values.data[i];
		if (strcmp(value->name.data, "Lcl Translation")) continue;
		ufbx_anim_curve *curve = value->curves[0];
		ufbxt_assert(curve->keyframes.count == 4);

		for (size_t i = 0; i < ufbxt_arraycount(values); i++) {
			double time = (double)i * (1.0/24.0);
			ufbx_real value = ufbx_evaluate_curve(curve, time, 0.0);
			ufbxt_assert_close_real(err, value, (ufbx_real)values[i]);
		}
	}
}
#endif

UFBXT_FILE_TEST(maya_resampled)
#if UFBXT_IMPL
{
	static const double values6[] = {
		0,0,0,0,0,0,0,0,0,
		-0.004, -0.022, -0.056, -0.104, -0.166, -0.241, -0.328, -0.427, -0.536, -0.654, -0.783,
		-0.919, -1.063, -1.214, -1.371, -1.533, -1.700, -1.871, -2.044, -2.220, -2.398, -2.577,
		-2.755, -2.933, -3.109, -3.283, -3.454, -3.621, -3.784, -3.941, -4.093, -4.237, -4.374,
		-4.503, -4.623, -4.733, -4.832, -4.920, -4.996, -5.059, -5.108, -5.143, -5.168, -5.186,
		-5.200, -5.209, -5.215, -5.218, -5.220, -5.220, -5.216, -5.192, -5.151, -5.091, -5.013,
		-4.919, -4.810, -4.686,
	};

	static const double values7[] = {
		0,0,0,0,0,0,0,0,
		0.000, -0.004, -0.025, -0.061, -0.112, -0.176, -0.252, -0.337, -0.431, -0.533, -0.648, 
		-0.776, -0.915, -1.064, -1.219, -1.378, -1.539, -1.700, -1.865, -2.037, -2.216, -2.397, -2.580, 
		-2.761, -2.939, -3.111, -3.278, -3.447, -3.615, -3.782, -3.943, -4.098, -4.244, -4.379, 
		-4.500, -4.614, -4.722, -4.821, -4.911, -4.990, -5.056, -5.107, -5.143, -5.168, -5.186, -5.200, 
		-5.209, -5.215, -5.218, -5.220, -5.220, -5.215, -5.190, -5.145, -5.082, -5.002, -4.908, 
		-4.800, -4.680, -4.550, -4.403, -4.239, 
	};

	const double *values = scene->metadata.version >= 7000 ? values7 : values6;
	size_t num_values = scene->metadata.version >= 7000 ? ufbxt_arraycount(values7) : ufbxt_arraycount(values6);

	ufbxt_assert(scene->anim_layers.count == 1);
	ufbx_anim_layer *layer = scene->anim_layers.data[0];
	for (size_t i = 0; i < layer->anim_values.count; i++) {
		ufbx_anim_value *value = layer->anim_values.data[i];
		if (strcmp(value->name.data, "Lcl Translation")) continue;
		ufbx_anim_curve *curve = value->curves[0];

		for (size_t i = 0; i < num_values; i++) {
			double time = (double)i * (1.0/200.0);
			ufbx_real value = ufbx_evaluate_curve(curve, time, 0.0);
			ufbxt_assert_close_real(err, value, (ufbx_real)values[i]);
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

typedef struct {
	int frame;
	ufbx_vec3 translation;
	ufbx_vec3 rotation_euler;
	ufbx_vec3 scale;
} ufbxt_anim_transform_ref;

#endif

UFBXT_FILE_TEST(maya_anim_light)
#if UFBXT_IMPL
{
	static const ufbxt_anim_light_ref refs[] = {
		{  0, 3.072f, { 0.148f, 0.095f, 0.440f } },
		{ 12, 1.638f, { 0.102f, 0.136f, 0.335f } },
		{ 24, 1.948f, { 0.020f, 0.208f, 0.149f } },
		{ 32, 3.676f, { 0.010f, 0.220f, 0.113f } },
		{ 40, 4.801f, { 0.118f, 0.195f, 0.115f } },
		{ 48, 3.690f, { 0.288f, 0.155f, 0.117f } },
		{ 56, 1.565f, { 0.421f, 0.124f, 0.119f } },
		{ 60, 1.145f, { 0.442f, 0.119f, 0.119f } },
	};

	for (size_t i = 0; i < ufbxt_arraycount(refs); i++) {
		const ufbxt_anim_light_ref *ref = &refs[i];

		double time = ref->frame * (1.0/24.0);
		ufbx_scene *state = ufbx_evaluate_scene(scene, &scene->anim, time, NULL, NULL);
		ufbxt_assert(state);

		ufbxt_check_scene(state);

		ufbx_node *light_node = ufbx_find_node(state, "pointLight1");
		ufbxt_assert(light_node);
		ufbx_light *light = light_node->light;
		ufbxt_assert(light);

		ufbxt_assert_close_real(err, light->intensity, ref->intensity);
		ufbxt_assert_close_vec3(err, light->color, ref->color);

		ufbx_free_scene(state);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "pointLight1");
		ufbxt_assert(node && node->light);
		uint32_t element_id = node->light->element.element_id;

		ufbx_prop_override overrides[] = {
			{ element_id, "Intensity", { (ufbx_real)10.0 } },
			{ element_id, "Color", { (ufbx_real)0.3, (ufbx_real)0.6, (ufbx_real)0.9 } },
			{ element_id, "|NewProp", { 10, 20, 30 }, "Test" },
			{ element_id, "IntProp", { 0, 0, 0 }, "", 15 },
		};

		ufbx_anim anim = scene->anim;
		anim.prop_overrides = ufbx_prepare_prop_overrides(overrides, ufbxt_arraycount(overrides));

		ufbx_scene *state = ufbx_evaluate_scene(scene, &anim, 1.0f, NULL, NULL);
		ufbxt_assert(state);

		ufbxt_check_scene(state);

		ufbx_node *light_node = ufbx_find_node(state, "pointLight1");
		ufbxt_assert(light_node);
		ufbx_light *light = light_node->light;
		ufbxt_assert(light);

		ufbx_vec3 ref_color = { (ufbx_real)0.3, (ufbx_real)0.6, (ufbx_real)0.9 };
		ufbxt_assert_close_real(err, light->intensity, 0.1f);
		ufbxt_assert_close_vec3(err, light->color, ref_color);

		{
			ufbx_vec3 ref_new = { 10, 20, 30 };
			ufbx_prop *new_prop = ufbx_find_prop(&light->props, "|NewProp");
			ufbxt_assert(new_prop);
			ufbxt_assert((new_prop->flags & UFBX_PROP_FLAG_OVERRIDDEN) != 0);
			ufbxt_assert(!strcmp(new_prop->value_str.data, "Test"));
			ufbxt_assert(new_prop->value_int == 10);
			ufbxt_assert_close_vec3(err, new_prop->value_vec3, ref_new);

			ufbx_prop *int_prop = ufbx_find_prop(&light->props, "IntProp");
			ufbxt_assert(int_prop);
			ufbxt_assert((int_prop->flags & UFBX_PROP_FLAG_OVERRIDDEN) != 0);
			ufbxt_assert_close_real(err, int_prop->value_real, 15.0f);
			ufbxt_assert(int_prop->value_int == 15);
		}

		{
			ufbx_element *original_light = &node->light->element;

			ufbx_prop color = ufbx_evaluate_prop(&anim, original_light, "Color", 1.0);
			ufbxt_assert((color.flags & UFBX_PROP_FLAG_OVERRIDDEN) != 0);
			ufbxt_assert_close_vec3(err, color.value_vec3, ref_color);

			ufbx_prop intensity = ufbx_evaluate_prop(&anim, original_light, "Intensity", 1.0);
			ufbxt_assert((intensity.flags & UFBX_PROP_FLAG_OVERRIDDEN) != 0);
			ufbxt_assert_close_real(err, intensity.value_real, 10.0f);

			ufbx_vec3 ref_new = { 10, 20, 30 };
			ufbx_prop new_prop = ufbx_evaluate_prop(&anim, original_light, "|NewProp", 1.0);
			ufbxt_assert((new_prop.flags & UFBX_PROP_FLAG_OVERRIDDEN) != 0);
			ufbxt_assert(!strcmp(new_prop.value_str.data, "Test"));
			ufbxt_assert(new_prop.value_int == 10);
			ufbxt_assert_close_vec3(err, new_prop.value_vec3, ref_new);

			ufbx_prop int_prop = ufbx_evaluate_prop(&anim, original_light, "IntProp", 1.0);
			ufbxt_assert((int_prop.flags & UFBX_PROP_FLAG_OVERRIDDEN) != 0);
			ufbxt_assert_close_real(err, int_prop.value_real, 15.0f);
			ufbxt_assert(int_prop.value_int == 15);
		}

		ufbx_free_scene(state);
	}

	{
		ufbx_anim_layer *layer = scene->anim_layers.data[0];
		ufbx_node *node = ufbx_find_node(scene, "pointLight1");
		ufbxt_assert(node && node->light);
		ufbx_light *light = node->light;

		{
			ufbx_anim_prop_list props = ufbx_find_anim_props(layer, &node->element);
			ufbxt_assert(props.count == 3);
			ufbxt_assert(!strcmp(props.data[0].prop_name.data, "Lcl Rotation"));
			ufbxt_assert(!strcmp(props.data[1].prop_name.data, "Lcl Scaling"));
			ufbxt_assert(!strcmp(props.data[2].prop_name.data, "Lcl Translation"));

			ufbx_anim_prop *prop;
			prop = ufbx_find_anim_prop(layer, &node->element, "Lcl Rotation");
			ufbxt_assert(prop && !strcmp(prop->prop_name.data, "Lcl Rotation"));
			prop = ufbx_find_anim_prop(layer, &node->element, "Lcl Scaling");
			ufbxt_assert(prop && !strcmp(prop->prop_name.data, "Lcl Scaling"));
			prop = ufbx_find_anim_prop(layer, &node->element, "Lcl Translation");
			ufbxt_assert(prop && !strcmp(prop->prop_name.data, "Lcl Translation"));
		}

		{
			ufbx_anim_prop_list props = ufbx_find_anim_props(layer, &light->element);
			ufbxt_assert(props.count == 2);
			ufbxt_assert(!strcmp(props.data[0].prop_name.data, "Color"));
			ufbxt_assert(!strcmp(props.data[1].prop_name.data, "Intensity"));

			ufbx_anim_prop *prop;
			prop = ufbx_find_anim_prop(layer, &light->element, "Color");
			ufbxt_assert(prop && !strcmp(prop->prop_name.data, "Color"));
			prop = ufbx_find_anim_prop(layer, &light->element, "Intensity");
			ufbxt_assert(prop && !strcmp(prop->prop_name.data, "Intensity"));

			prop = ufbx_find_anim_prop(layer, &light->element, "Nonexistent");
			ufbxt_assert(prop == NULL);
		}

		{
			ufbx_anim_prop_list props = ufbx_find_anim_props(layer, &layer->element);
			ufbxt_assert(props.count == 0);

			ufbx_anim_prop *prop = ufbx_find_anim_prop(layer, &layer->element, "Weight");
			ufbxt_assert(prop == NULL);
		}
	}
}
#endif

UFBXT_FILE_TEST(maya_transform_animation)
#if UFBXT_IMPL
{
	static const ufbxt_anim_transform_ref refs[] = {
		{  1, {  0.000f,  0.000f,  0.000f }, {   0.000f,   0.000f,   0.000f }, { 1.000f, 1.000f, 1.000f } },
		{  5, {  0.226f,  0.452f,  0.677f }, {   2.258f,   4.515f,   6.773f }, { 1.023f, 1.045f, 1.068f } },
		{ 14, {  1.000f,  2.000f,  3.000f }, {  10.000f,  20.000f,  30.000f }, { 1.100f, 1.200f, 1.300f } },
		{ 20, { -0.296f, -0.592f, -0.888f }, {  -2.960f,  -5.920f,  -8.880f }, { 0.970f, 0.941f, 0.911f } },
		{ 24, { -1.000f, -2.000f, -3.000f }, { -10.000f, -20.000f, -30.000f }, { 0.900f, 0.800f, 0.700f } },
	};

	ufbx_node *node = ufbx_find_node(scene, "pCube1");

	for (size_t i = 0; i < ufbxt_arraycount(refs); i++) {
		const ufbxt_anim_transform_ref *ref = &refs[i];
		double time = ref->frame * (1.0/24.0);

		ufbx_scene *state = ufbx_evaluate_scene(scene, &scene->anim, time, NULL, NULL);
		ufbxt_assert(state);
		ufbxt_check_scene(state);

		ufbx_transform t1 = state->nodes.data[node->element.typed_id]->local_transform;
		ufbx_transform t2 = ufbx_evaluate_transform(&scene->anim, node, time);

		ufbx_vec3 t1_euler = ufbx_quat_to_euler(t1.rotation, UFBX_ROTATION_ORDER_XYZ);
		ufbx_vec3 t2_euler = ufbx_quat_to_euler(t2.rotation, UFBX_ROTATION_ORDER_XYZ);

		ufbxt_assert_close_vec3(err, ref->translation, t1.translation);
		ufbxt_assert_close_vec3(err, ref->translation, t2.translation);
		ufbxt_assert_close_vec3(err, ref->rotation_euler, t1_euler);
		ufbxt_assert_close_vec3(err, ref->rotation_euler, t2_euler);
		ufbxt_assert_close_vec3(err, ref->scale, t1.scale);
		ufbxt_assert_close_vec3(err, ref->scale, t2.scale);

		ufbx_free_scene(state);
	}

	{
		uint32_t element_id = node->element.element_id;
		ufbxt_anim_transform_ref ref = refs[2];
		ref.translation.x -= 0.1f;
		ref.translation.y -= 0.2f;
		ref.translation.z -= 0.3f;
		ref.scale.x = 2.0f;
		ref.scale.y = 3.0f;
		ref.scale.z = 4.0f;

		ufbx_prop_override overrides[] = {
			{ element_id, "Color", { (ufbx_real)0.3, (ufbx_real)0.6, (ufbx_real)0.9 } },
			{ element_id, "|NewProp", { 10, 20, 30 }, "Test", },
			{ element_id, "Lcl Scaling", { 2.0f, 3.0f, 4.0f } },
			{ element_id, "RotationOffset", { -0.1f, -0.2f, -0.3f } },
		};

		double time = 14.0/24.0;
		ufbx_anim anim = scene->anim;
		anim.prop_overrides = ufbx_prepare_prop_overrides(overrides, ufbxt_arraycount(overrides));
		ufbx_scene *state = ufbx_evaluate_scene(scene, &anim, time, NULL, NULL);
		ufbxt_assert(state);
		ufbxt_check_scene(state);

		ufbx_transform t1 = state->nodes.data[node->element.typed_id]->local_transform;
		ufbx_transform t2 = ufbx_evaluate_transform(&anim, node, time);

		ufbx_vec3 t1_euler = ufbx_quat_to_euler(t1.rotation, UFBX_ROTATION_ORDER_XYZ);
		ufbx_vec3 t2_euler = ufbx_quat_to_euler(t2.rotation, UFBX_ROTATION_ORDER_XYZ);

		ufbxt_assert_close_vec3(err, ref.translation, t1.translation);
		ufbxt_assert_close_vec3(err, ref.translation, t2.translation);
		ufbxt_assert_close_vec3(err, ref.rotation_euler, t1_euler);
		ufbxt_assert_close_vec3(err, ref.rotation_euler, t2_euler);
		ufbxt_assert_close_vec3(err, ref.scale, t1.scale);
		ufbxt_assert_close_vec3(err, ref.scale, t2.scale);

		ufbx_free_scene(state);
	}
}
#endif


UFBXT_FILE_TEST(maya_anim_layers)
#if UFBXT_IMPL
{
	ufbx_anim_layer *x = (ufbx_anim_layer*)ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "X");
	ufbx_anim_layer *y = (ufbx_anim_layer*)ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "Y");
	ufbxt_assert(x && y);
	ufbxt_assert(y->compose_rotation == false);
	ufbxt_assert(y->compose_scale == false);
}
#endif

UFBXT_FILE_TEST(maya_anim_layers_acc)
#if UFBXT_IMPL
{
	ufbx_anim_layer *x = (ufbx_anim_layer*)ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "X");
	ufbx_anim_layer *y = (ufbx_anim_layer*)ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "Y");
	ufbxt_assert(x && y);
	ufbxt_assert(y->compose_rotation == true);
	ufbxt_assert(y->compose_scale == true);
}
#endif

UFBXT_FILE_TEST(maya_anim_layers_over)
#if UFBXT_IMPL
{
	ufbx_anim_layer *x = (ufbx_anim_layer*)ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "X");
	ufbx_anim_layer *y = (ufbx_anim_layer*)ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "Y");
	ufbxt_assert(x && y);
	ufbxt_assert(y->compose_rotation == false);
	ufbxt_assert(y->compose_scale == false);
}
#endif

UFBXT_FILE_TEST(maya_anim_layers_over_acc)
#if UFBXT_IMPL
{
	ufbx_anim_layer *x = (ufbx_anim_layer*)ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "X");
	ufbx_anim_layer *y = (ufbx_anim_layer*)ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "Y");
	ufbxt_assert(x && y);
	ufbxt_assert(y->compose_rotation == true);
	ufbxt_assert(y->compose_scale == true);
}
#endif

#if UFBXT_IMPL
typedef struct {
	double time;
	bool visible;
} ufbxt_visibility_ref;
#endif

UFBXT_FILE_TEST(maya_cube_blinky)
#if UFBXT_IMPL
{
	ufbxt_visibility_ref refs[] = {
		{ 1.0, false },
		{ 9.5, false },
		{ 10.5, true },
		{ 11.5, false },
		{ 15.0, false },
		{ 19.5, false },
		{ 20.5, false },
		{ 25.0, false },
		{ 29.5, false },
		{ 30.5, true },
		{ 40.0, true },
		{ 50.0, true },
	};

	for (size_t i = 0; i < ufbxt_arraycount(refs); i++) {
		double time = refs[i].time / 24.0;
		ufbx_scene *state = ufbx_evaluate_scene(scene, &scene->anim, time, NULL, NULL);
		ufbxt_assert(state);

		ufbxt_check_scene(state);

		ufbx_node *node = ufbx_find_node(state, "pCube1");
		ufbxt_assert(node);
		ufbxt_assert(node->visible == refs[i].visible);

		ufbx_free_scene(state);
	}
}
#endif

#if UFBXT_IMPL
typedef struct {
	double time;
	double value;
} ufbxt_anim_ref;
#endif

UFBXT_FILE_TEST(maya_anim_interpolation)
#if UFBXT_IMPL
{
	ufbxt_anim_ref anim_ref[] = {
		// Cubic
		{ 0.0 / 30.0, 0.0 },
		{ 1.0 / 30.0, -0.855245 },
		{ 2.0 / 30.0, -1.13344 },
		{ 3.0 / 30.0, -1.17802 },
		{ 4.0 / 30.0, -1.10882 },
		{ 5.0 / 30.0, -0.991537 },
		{ 6.0 / 30.0, -0.875223 },
		{ 7.0 / 30.0, -0.808958 },
		{ 8.0 / 30.0, -0.858419 },
		{ 9.0 / 30.0, -1.14293 },
		// Linear
		{ 10.0 / 30.0, -2.0 },
		// Constant previous
		{ 20.0 / 30.0, -4.0 },
		{ 25.0 / 30.0 - 0.001, -4.0 },
		// Constant next
		{ 25.0 / 30.0, -6.0 },
		{ 25.0 / 30.0 + 0.001, -8.0 },
		// Constant previous
		{ 30.0 / 30.0, -8.0 },
		{ 35.0 / 30.0 - 0.001, -8.0 },
		// Linear
		{ 35.0 / 30.0, -10.0 },
		// Constant next
		{ 40.0 / 30.0, -12.0 },
		{ 40.0 / 30.0 + 0.001, -14.0 },
		// Constant previous
		{ 45.0 / 30.0, -14.0 },
		{ 50.0 / 30.0 - 0.001, -14.0 },
		// Constant next
		{ 50.0 / 30.0, -16.0 },
		{ 50.0 / 30.0 + 0.001, -14.0 },
		// inal
		{ 55.0 / 30.0, -14.0, },
	};

	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node);

	for (size_t i = 0; i < ufbxt_arraycount(anim_ref); i++) {
		ufbxt_anim_ref ref = anim_ref[i];
		ufbxt_hintf("%zu: %f (frame %.2f)", i, ref.time, ref.time * 30.0f);

		ufbx_prop p = ufbx_evaluate_prop(&scene->anim, &node->element, "Lcl Translation", ref.time);
		ufbxt_assert_close_real(err, p.value_vec3.x, (ufbx_real)ref.value);
	}
}
#endif

