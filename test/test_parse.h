#undef UFBXT_TEST_GROUP
#define UFBXT_TEST_GROUP "parse"

#if UFBXT_IMPL
static void ufbxt_check_warning(ufbx_scene *scene, ufbx_warning_type type, size_t count, const char *substring)
{
	bool found = false;
	for (size_t i = 0; i < scene->metadata.warnings.count; i++) {
		ufbx_warning *warning = &scene->metadata.warnings.data[i];
		if (warning->type == type) {
			if (substring) {
				ufbxt_assert(strstr(warning->description.data, substring));
			}
			if (count == SIZE_MAX) {
				ufbxt_assert(warning->count > 0);
			} else {
				ufbxt_assert(warning->count == count);
			}
			found = true;
			break;
		}
	}

	ufbxt_logf("Warning not found: %d", (int)type);
	ufbxt_assert(found);
}
#endif

UFBXT_TEST(thread_safety)
#if UFBXT_IMPL
{
	if (!g_allow_non_thread_safe) {
		ufbxt_assert(ufbx_is_thread_safe());
	}
}
#endif

#if UFBXT_IMPL
static ufbx_load_opts ufbxt_retain_dom_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.retain_dom = true;
	return opts;
}
static ufbx_load_opts ufbxt_strict_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.strict = true;
	return opts;
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(maya_cube_dom, maya_cube, ufbxt_retain_dom_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->dom_root);

	ufbx_dom_node *objects = ufbx_dom_find(scene->dom_root, "Objects");
	ufbxt_assert(objects);

	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node);
	bool found = false;
	for (size_t i = 0; i < objects->children.count; i++) {
		if (node->element.dom_node == objects->children.data[i]) {
			found = true;
			break;
		}
	}
	ufbxt_assert(found);

	ufbx_mesh *mesh = ufbx_as_mesh(node->attrib);
	ufbxt_assert(mesh);

	ufbx_dom_node *dom_mesh = mesh->element.dom_node;
	ufbxt_assert(dom_mesh);

	ufbx_dom_node *dom_indices = ufbx_dom_find(dom_mesh, "PolygonVertexIndex");
	ufbxt_assert(dom_indices);
	ufbxt_assert(dom_indices->values.count == 1);

	ufbx_dom_value *dom_indices_value = &dom_indices->values.data[0];
	ufbxt_assert(dom_indices_value->type == UFBX_DOM_VALUE_ARRAY_I32);
	ufbxt_assert(dom_indices_value->value_int == 24);

	size_t count = (size_t)dom_indices_value->value_int;
	int32_t *data = (int32_t*)dom_indices_value->value_blob.data;

	size_t num_negative = 0;
	for (size_t i = 0; i < count; i++) {
		if (data[i] < 0) {
			num_negative++;
		}
	}
	ufbxt_assert(num_negative == 6);
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(maya_cube_strict, maya_cube, ufbxt_strict_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(max2009_blob_dom, max2009_blob, ufbxt_retain_dom_opts, UFBXT_FILE_TEST_FLAG_FUZZ_ALWAYS|UFBXT_FILE_TEST_FLAG_FUZZ_OPTS)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->dom_root);
}
#endif

UFBXT_FILE_TEST(maya_leading_comma)
#if UFBXT_IMPL
{
	ufbxt_assert(!strcmp(scene->metadata.creator.data, "FBX SDK/FBX Plugins version 2019.2"));
}
#endif

UFBXT_FILE_TEST(maya_zero_end)
#if UFBXT_IMPL
{
	ufbxt_assert(!strcmp(scene->metadata.creator.data, "FBX SDK/FBX Plugins version 2019.2"));
}
#endif

UFBXT_TEST(error_format_long)
#if UFBXT_IMPL
{
	char data[] = "Bad FBX";
	ufbx_error error;
	ufbx_scene *scene = ufbx_load_memory(data, sizeof(data), NULL, &error);
	ufbxt_assert(!scene);

	char error_buf[512];
	size_t length = ufbx_format_error(error_buf, sizeof(error_buf), &error);
	ufbxt_assert(strlen(error_buf) == length);

	#if defined(UFBX_DEV)
		ufbxt_assert(error.stack_size >= 2);
	#endif

	char func[64] = { 0 }, desc[64] = { 0 };
	size_t line_begin = 0;
	size_t num_lines = 0;
	for (size_t i = 0; i < length; i++) {
		if (error_buf[i] == '\n') {
			if (num_lines == 0) {
				ufbxt_check_string(error.description);

				unsigned major = 0, minor = 0, patch = 0;
				int num_scanned = sscanf(error_buf + line_begin, "ufbx v%u.%u.%u error: %63[^\n]\n",
					&major, &minor, &patch, desc);

				ufbxt_assert(num_scanned == 4);
				ufbxt_assert(major == ufbx_version_major(ufbx_source_version));
				ufbxt_assert(minor == ufbx_version_minor(ufbx_source_version));
				ufbxt_assert(patch == ufbx_version_patch(ufbx_source_version));
				ufbxt_assert(!strcmp(desc, error.description.data));
			} else {
				size_t stack_ix = num_lines - 1;
				ufbxt_check_string(error.stack[stack_ix].function);
				ufbxt_check_string(error.stack[stack_ix].description);

				unsigned line = 0;
				int num_scanned = sscanf(error_buf + line_begin, "%u:%63[^:]: %63[^\n]\n", &line, func, desc);
				ufbxt_assert(num_scanned == 3);
				ufbxt_assert(stack_ix < error.stack_size);
				ufbxt_assert(line == error.stack[stack_ix].source_line);
				ufbxt_assert(!strcmp(func, error.stack[stack_ix].function.data));
				ufbxt_assert(!strcmp(desc, error.stack[stack_ix].description.data));
			}

			line_begin = i + 1;
			num_lines++;
		}
	}
	ufbxt_assert(num_lines == error.stack_size + 1);
}
#endif

UFBXT_TEST(error_format_short)
#if UFBXT_IMPL
{
	char data[] = "Bad FBX";
	ufbx_error error;
	ufbx_scene *scene = ufbx_load_memory(data, sizeof(data), NULL, &error);
	ufbxt_assert(!scene);

	char error_buf[512];
	for (size_t buf_len = 0; buf_len <= ufbxt_arraycount(error_buf); buf_len++) {
		size_t ret_len = ufbx_format_error(error_buf, buf_len, &error);
		if (buf_len == 0) {
			ufbxt_assert(ret_len == 0);
			continue;
		}

		size_t str_len = strlen(error_buf);
		ufbxt_hintf("buf_len = %zu, ret_len = %zu, str_len = %zu", buf_len, ret_len, str_len);
		ufbxt_assert(ret_len == str_len);
		if (buf_len < 16) {
			ufbxt_assert(ret_len == buf_len - 1);
		}
	}
}
#endif

UFBXT_FILE_TEST(maya_node_attribute_zoo)
#if UFBXT_IMPL
{
	ufbx_node *node;

	ufbxt_assert(scene->settings.axes.right == UFBX_COORDINATE_AXIS_POSITIVE_X);
	ufbxt_assert(scene->settings.axes.up == UFBX_COORDINATE_AXIS_POSITIVE_Y);
	ufbxt_assert(scene->settings.axes.front == UFBX_COORDINATE_AXIS_POSITIVE_Z);
	ufbxt_assert(scene->settings.time_mode == UFBX_TIME_MODE_24_FPS);
	ufbxt_assert_close_real(err, scene->settings.unit_meters, 0.01f);
	ufbxt_assert_close_real(err, (ufbx_real)scene->settings.frames_per_second, 24.0f);

	node = ufbx_find_node(scene, "Null");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_EMPTY);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_EMPTY);

	node = ufbx_find_node(scene, "Mesh");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_MESH);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_MESH);
	ufbxt_assert(&node->mesh->element == node->attrib);

	node = ufbx_find_node(scene, "Bone");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_BONE);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_BONE);
	ufbxt_assert(&node->bone->element == node->attrib);
	ufbxt_assert_close_real(err, node->bone->radius, 0.5f);
	ufbxt_assert_close_real(err, node->bone->relative_length, 1.0f);

	node = ufbx_find_node(scene, "Camera");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_CAMERA);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_CAMERA);
	ufbxt_assert(&node->camera->element == node->attrib);
	ufbxt_assert(node->camera->gate_fit == UFBX_GATE_FIT_FILL);
	ufbxt_assert_close_real(err, node->camera->field_of_view_deg.x / 10.0f, 54.43f / 10.0f);
	ufbxt_assert_close_real(err, node->camera->focal_length_mm, 35.0f);

	node = ufbx_find_node(scene, "Light");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_LIGHT);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_LIGHT);
	ufbxt_assert(&node->light->element == node->attrib);
	ufbxt_assert_close_real(err, node->light->intensity, 1.0f);
	ufbxt_assert_close_real(err, node->light->color.x, 1.0f);
	ufbxt_assert_close_real(err, node->light->color.y, 1.0f);
	ufbxt_assert_close_real(err, node->light->color.z, 1.0f);

	node = ufbx_find_node(scene, "StereoCamera");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_STEREO_CAMERA);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_STEREO_CAMERA);
	ufbx_stereo_camera *stereo = (ufbx_stereo_camera*)node->attrib;
	ufbxt_assert(stereo->left && stereo->left->element.type == UFBX_ELEMENT_CAMERA);
	ufbxt_assert(stereo->right && stereo->right->element.type == UFBX_ELEMENT_CAMERA);
	ufbx_prop left_focal_prop = ufbx_evaluate_prop(&scene->anim, &stereo->left->element, "FocalLength", 0.5);
	ufbx_prop right_focal_prop = ufbx_evaluate_prop(&scene->anim, &stereo->right->element, "FocalLength", 0.5);
	ufbxt_assert_close_real(err, left_focal_prop.value_real, 42.011f);
	ufbxt_assert_close_real(err, right_focal_prop.value_real, 42.011f);

	node = ufbx_find_node(scene, "NurbsCurve");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_NURBS_CURVE);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_NURBS_CURVE);

	node = ufbx_find_node(scene, "NurbsSurface");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_NURBS_SURFACE);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_NURBS_SURFACE);

	node = ufbx_find_node(scene, "NurbsTrim");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_NURBS_TRIM_SURFACE);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_NURBS_TRIM_SURFACE);

	node = ufbx_find_node(scene, "LodGroup");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_LOD_GROUP);
	ufbxt_assert(node->attrib && node->attrib->type == UFBX_ELEMENT_LOD_GROUP);
}
#endif

UFBXT_FILE_TEST(synthetic_duplicate_prop)
#if UFBXT_IMPL
{
	ufbx_prop *prop;
	ufbx_node *cube = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(cube);

	prop = ufbx_find_prop(&cube->props, "Lcl Translation");
	ufbxt_assert(prop);
	ufbxt_assert_close_real(err, prop->value_vec3.x, -1.0f);

	prop = ufbx_find_prop(&cube->props, "Lcl Scaling");
	ufbxt_assert(prop);
	ufbxt_assert_close_real(err, prop->value_vec3.x, 2.0f);
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_missing_version, UFBXT_FILE_TEST_FLAG_ALLOW_STRICT_ERROR)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->metadata.version == 6100);
	ufbxt_assert(scene->metadata.exporter == UFBX_EXPORTER_FBX_SDK);
	ufbxt_assert(scene->metadata.exporter_version == ufbx_pack_version(2007, 2, 28));
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_missing_exporter, UFBXT_FILE_TEST_FLAG_ALLOW_STRICT_ERROR)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->metadata.version == 6100);
	ufbxt_assert(scene->metadata.exporter == UFBX_EXPORTER_UNKNOWN);
	ufbxt_assert(scene->metadata.exporter_version == 0);
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_blender_old_exporter, UFBXT_FILE_TEST_FLAG_ALLOW_STRICT_ERROR)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->metadata.version == 6100);
	ufbxt_assert(scene->metadata.exporter == UFBX_EXPORTER_UNKNOWN);
	ufbxt_assert(scene->metadata.exporter_version == 0);
}
#endif

UFBXT_FILE_TEST(blender_272_cube)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->metadata.version == 7400);
	ufbxt_assert(scene->metadata.exporter == UFBX_EXPORTER_BLENDER_BINARY);
	ufbxt_assert(scene->metadata.exporter_version == ufbx_pack_version(2, 72, 0));

	ufbxt_assert(!strcmp(scene->metadata.original_application.vendor.data, "Blender Foundation"));
	ufbxt_assert(!strcmp(scene->metadata.original_application.name.data, "Blender (stable FBX IO)"));
	ufbxt_assert(!strcmp(scene->metadata.original_application.version.data, "2.72 (sub 0)"));
	ufbxt_assert(!strcmp(scene->metadata.latest_application.vendor.data, "Blender Foundation"));
	ufbxt_assert(!strcmp(scene->metadata.latest_application.name.data, "Blender (stable FBX IO)"));
	ufbxt_assert(!strcmp(scene->metadata.latest_application.version.data, "2.72 (sub 0)"));
}
#endif

UFBXT_TEST(unicode_filename)
#if UFBXT_IMPL
{
	char buf[1024];
	int len = snprintf(buf, sizeof(buf), "%ssynthetic_\x61\xce\xb2\xe3\x82\xab\xf0\x9f\x98\x82_7500_ascii.fbx", data_root);
	ufbxt_assert(len > 0 && len < sizeof(buf));

	{
		ufbx_load_opts opts = { 0 };
		opts.ignore_geometry = true;

		ufbx_scene *scene = ufbx_load_file(buf, &opts, NULL);
		ufbxt_assert(scene);
		ufbxt_assert(ufbx_find_node(scene, "pCube1"));
		ufbxt_check_scene(scene);
		ufbx_free_scene(scene);
	}

	// Remove terminating \0 for explicit length test
	buf[len] = 'x';

	{
		ufbx_scene *scene = ufbx_load_file_len(buf, (size_t)len, NULL, NULL);
		ufbxt_assert(scene);
		ufbxt_assert(ufbx_find_node(scene, "pCube1"));
		ufbxt_check_scene(scene);
		ufbx_free_scene(scene);
	}
}
#endif

UFBXT_FILE_TEST(maya_cube_big_endian)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->metadata.big_endian);
}
#endif

UFBXT_FILE_TEST(synthetic_cube_nan)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_assert(mesh->num_vertices >= 2);
	ufbxt_assert(isnan(mesh->vertices.data[0].x));
	ufbxt_assert(isinf(mesh->vertices.data[0].y) && mesh->vertices.data[0].y < 0.0f);
	ufbxt_assert(isinf(mesh->vertices.data[0].z) && mesh->vertices.data[0].z > 0.0f);
	ufbxt_assert_close_real(err, mesh->vertices.data[1].x, 0.5f);
	ufbxt_assert_close_real(err, mesh->vertices.data[1].y, -0.5f);
	ufbxt_assert_close_real(err, mesh->vertices.data[1].z, 0.5f);
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_string_collision, UFBXT_FILE_TEST_FLAG_HEAVY_TO_FUZZ|UFBXT_FILE_TEST_FLAG_SKIP_LOAD_OPTS_CHECKS)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node);
	ufbx_prop *prop;

	prop = ufbx_find_prop(&node->props, "BIiMTNT");
	ufbxt_assert(prop);
	ufbxt_assert(!strcmp(prop->value_str.data, "BJKKUfZ"));

	prop = ufbx_find_prop(&node->props, "BbAJerP");
	ufbxt_assert(prop);
	ufbxt_assert(!strcmp(prop->value_str.data, "AAAJKop"));

	prop = ufbx_find_prop(&node->props, "BpgrcZR");
	ufbxt_assert(prop);
	ufbxt_assert(!strcmp(prop->value_str.data, "APGAmLj"));
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_id_collision, UFBXT_FILE_TEST_FLAG_HEAVY_TO_FUZZ|UFBXT_FILE_TEST_FLAG_SKIP_LOAD_OPTS_CHECKS)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->nodes.count == 10002);
	ufbx_node *node = ufbx_find_node(scene, "First");
	for (int32_t depth = 10000; depth >= 0; depth--) {
		ufbxt_assert(node);
		ufbxt_assert(node->node_depth == depth);
		node = node->parent;
	}
	ufbxt_assert(!node);
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_node_depth_fail, UFBXT_FILE_TEST_FLAG_ALLOW_ERROR)
#if UFBXT_IMPL
{
}
#endif

UFBXT_TEST(open_file)
#if UFBXT_IMPL
{
	const char *name = "maya_cube_7500_ascii.fbx";

	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s", data_root, name);

	for (size_t i = 0; i < 2; i++) {
		ufbx_stream stream = { 0 };
		bool ok = ufbx_open_file(&stream, buf, i == 0 ? SIZE_MAX : strlen(buf));
		ufbxt_assert(ok);
		ufbxt_assert(stream.skip_fn);
		ufbxt_assert(stream.read_fn);
		ufbxt_assert(stream.close_fn);

		ufbxt_assert(stream.skip_fn(stream.user, 2));

		char result[3];
		size_t num_read = stream.read_fn(stream.user, result, 3);
		ufbxt_assert(num_read == 3);

		ufbxt_assert(!memcmp(result, "FBX", 3));

		stream.close_fn(stream.user);
	}

}
#endif

UFBXT_TEST(file_not_found)
#if UFBXT_IMPL
{
	const char *name = "maya_cube_9999_emoji.fbx";

	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s", data_root, name);

	ufbx_error error;
	ufbx_scene *scene = ufbx_load_file(buf, NULL, &error);
	ufbxt_assert(!scene);
	ufbxt_assert(error.type == UFBX_ERROR_FILE_NOT_FOUND);
	ufbxt_assert(error.info_length == strlen(error.info));
	ufbxt_assert(!strcmp(error.info, buf));
}
#endif

UFBXT_TEST(retain_scene)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "maya_cube" };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		ufbx_load_opts opts = { 0 };

		ufbx_scene *scene = ufbx_load_file(path, &opts, NULL);
		ufbxt_assert(scene);

		ufbxt_check_scene(scene);

		ufbx_scene *eval_scene = ufbx_evaluate_scene(scene, NULL, 0.1, NULL, NULL);
		ufbx_free_scene(scene);

		// eval_scene should retain references
		ufbx_node *node = ufbx_find_node(eval_scene, "pCube1");
		ufbxt_assert(node);
		ufbxt_assert(!strcmp(node->name.data, "pCube1"));

		// No-op retain and release
		ufbx_retain_scene(eval_scene);
		ufbx_free_scene(eval_scene);

		ufbx_free_scene(eval_scene);
	}
}
#endif

UFBXT_FILE_TEST(synthetic_empty_elements)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
}
#endif

#if UFBXT_IMPL
static void ufbxt_check_binary_prop(const ufbx_props *props)
{
	ufbx_prop *prop = ufbx_find_prop(props, "Binary");
	ufbxt_assert(prop);
	ufbxt_assert(prop->type == UFBX_PROP_BLOB);
	// TODO: Assert value
}
#endif

UFBXT_FILE_TEST(synthetic_binary_props)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
	ufbxt_check_binary_prop(&node->props);
	ufbxt_check_binary_prop(&node->mesh->props);
	ufbxt_check_binary_prop(&scene->settings.props);
}
#endif

UFBXT_FILE_TEST(maya_unicode)
#if UFBXT_IMPL
{
	// WHAT? Maya turns U+03B2 (Greek Small Letter Beta) into U+00DF (Latin Small Letter Sharp S)
	// The larger codepoints just get turned into one or two underscores which makes a bit more sense...
	ufbx_node *node = ufbx_find_node(scene, "\x61\xc3\x9f___");
	ufbxt_assert(node);
}
#endif

UFBXT_FILE_TEST(max_unicode)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "\x61\xce\xb2\xe3\x82\xab\xf0\x9f\x98\x82");
	ufbxt_assert(node);
}
#endif

UFBXT_FILE_TEST(blender_279_unicode)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "\x61\xce\xb2\xe3\x82\xab\xf0\x9f\x98\x82");
	ufbxt_assert(node);
}
#endif

#if UFBXT_IMPL
static uint32_t ufbxt_decode_hex_char(char c)
{
	if (c >= '0' && c <= '9') {
		return (uint32_t)c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return (uint32_t)c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return (uint32_t)c - 'A' + 10;
	} else {
		ufbxt_assert(false && "Bad hex character");
		return 0;
	}
}

static size_t ufbxt_decode_hex(uint8_t *dst, size_t dst_len, ufbx_string src)
{
	ufbxt_assert(src.length % 2 == 0);

	size_t num = src.length / 2;
	ufbxt_assert(num <= dst_len);

	for (size_t i = 0; i < num; i++) {
		dst[i] = (uint8_t)(
			ufbxt_decode_hex_char(src.data[i * 2 + 0]) << 4u |
			ufbxt_decode_hex_char(src.data[i * 2 + 1]) << 0u );
	}

	return num;
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_unicode, UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE|UFBXT_FILE_TEST_FLAG_HEAVY_TO_FUZZ|UFBXT_FILE_TEST_FLAG_SKIP_LOAD_OPTS_CHECKS)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->metadata.warnings.count == 1);
	ufbxt_assert(scene->metadata.warnings.data[0].type == UFBX_WARNING_BAD_UNICODE);
	ufbxt_assert(scene->metadata.warnings.data[0].count >= 6000);

	uint8_t ref[128];

	{
		ufbx_node *node = ufbx_find_node(scene, "Good");
		ufbxt_assert(node);

		for (size_t prop_ix = 0; prop_ix < node->props.props.count; prop_ix++) {
			ufbx_prop *prop = &node->props.props.data[prop_ix];
			size_t ref_len = ufbxt_decode_hex(ref, ufbxt_arraycount(ref), prop->name);

			ufbx_string src = prop->value_str;

			size_t src_ix = 0;
			for (size_t ref_ix = 0; ref_ix < ref_len; ref_ix++) {
				uint8_t rc = ref[ref_ix];
				if (rc == 0) {
					ufbxt_assert(src_ix + 3 <= src.length);
					ufbxt_assert((uint8_t)src.data[src_ix + 0] == 0xef);
					ufbxt_assert((uint8_t)src.data[src_ix + 1] == 0xbf);
					ufbxt_assert((uint8_t)src.data[src_ix + 2] == 0xbd);
					src_ix += 3;
				} else {
					ufbxt_assert(src_ix < src.length);
					ufbxt_assert((uint8_t)src.data[src_ix] == rc);
					src_ix += 1;
				}
			}
			ufbxt_assert(src_ix == src.length);
		}
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Ok");
		ufbxt_assert(node);

		for (size_t prop_ix = 0; prop_ix < node->props.props.count; prop_ix++) {
			ufbx_prop *prop = &node->props.props.data[prop_ix];
			size_t ref_len = ufbxt_decode_hex(ref, ufbxt_arraycount(ref), prop->name);

			ufbx_string src = prop->value_str;

			ufbxt_assert(ref_len >= 1);
			ufbxt_assert((uint8_t)ref[0] == 0xff);

			size_t src_ix = 0;
			ufbxt_assert((uint8_t)src.data[src_ix + 0] == 0xef);
			ufbxt_assert((uint8_t)src.data[src_ix + 1] == 0xbf);
			ufbxt_assert((uint8_t)src.data[src_ix + 2] == 0xbd);
			src_ix += 3;

			for (size_t ref_ix = 1; ref_ix < ref_len; ref_ix++) {
				uint8_t rc = ref[ref_ix];
				if (rc == 0) {
					ufbxt_assert(src_ix + 3 <= src.length);
					ufbxt_assert((uint8_t)src.data[src_ix + 0] == 0xef);
					ufbxt_assert((uint8_t)src.data[src_ix + 1] == 0xbf);
					ufbxt_assert((uint8_t)src.data[src_ix + 2] == 0xbd);
					src_ix += 3;
				} else {
					ufbxt_assert(src_ix < src.length);
					ufbxt_assert((uint8_t)src.data[src_ix] == rc);
					src_ix += 1;
				}
			}
			ufbxt_assert(src_ix == src.length);
		}
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "Bad");
		ufbxt_assert(node);

		for (size_t prop_ix = 0; prop_ix < node->props.props.count; prop_ix++) {
			ufbx_prop *prop = &node->props.props.data[prop_ix];
			size_t ref_len = ufbxt_decode_hex(ref, ufbxt_arraycount(ref), prop->name);

			ufbx_string src = prop->value_str;
			const char *replacement = strstr(src.data, "\xef\xbf\xbd");
			ufbxt_assert(replacement);
		}
	}
}
#endif

UFBXT_FILE_TEST(max_quote)
#if UFBXT_IMPL
{
	if (scene->metadata.ascii && scene->metadata.version == 6100) {
		ufbxt_check_warning(scene, UFBX_WARNING_DUPLICATE_OBJECT_ID, 1, NULL);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "\"'&\"");
		ufbxt_assert(node);
		ufbxt_assert(node->mesh);
		ufbx_mesh *mesh = node->mesh;
		ufbxt_assert(mesh->materials.count == 1);
		ufbx_material *material = mesh->materials.data[0].material;
		if (scene->metadata.ascii) {
			ufbxt_assert(!strcmp(material->name.data, "&&q&qu&quo&quot\"\""));
		} else {
			ufbxt_assert(!strcmp(material->name.data, "&&q&qu&quo&quot&quot;\""));
		}

	}

	{
		ufbx_node *node = ufbx_find_node(scene, "\"");
		ufbxt_assert(node);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "&quot;");
		if (scene->metadata.ascii) {
			ufbxt_assert(!node);
		} else {
			ufbxt_assert(node);
		}
	}
}
#endif

UFBXT_FILE_TEST(max_colon_name)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Cube::Model");
	ufbxt_assert(node);
	ufbxt_assert(node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_assert(mesh->materials.count == 1);
	ufbx_material *material = mesh->materials.data[0].material;
	ufbxt_assert(!strcmp(material->name.data, "Material::Pink"));
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_truncated_quot_fail, UFBXT_FILE_TEST_FLAG_ALLOW_ERROR)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_unicode_error_identity, UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE)
#if UFBXT_IMPL
{
	ufbx_node *parent = ufbx_find_node(scene, "Parent");
	ufbxt_assert(parent);
	ufbxt_assert(parent->children.count == 2);

	ufbxt_assert(!strcmp(parent->children.data[0]->name.data, parent->children.data[1]->name.data));
	ufbxt_assert(parent->children.data[0]->name.data == parent->children.data[1]->name.data);

	for (size_t i = 0; i < parent->children.count; i++) {
		ufbx_node *child = parent->children.data[i];
		bool left = child->world_transform.translation.x < 0.0f;

		ufbxt_assert(!strcmp(child->name.data, "Child_\xef\xbf\xbd"));

		ufbx_mesh *mesh = child->mesh;
		ufbxt_assert(mesh);
		ufbxt_assert(mesh->materials.count == 1);

		ufbx_material *material = mesh->materials.data[0].material;
		ufbxt_assert(!strcmp(material->name.data, "Material_\xef\xbf\xbd"));

		if (left) {
			ufbx_vec3 ref = { 1.0f, 0.0f, 0.0f };
			ufbxt_assert_close_vec3(err, material->fbx.diffuse_color.value_vec3, ref);
		} else {
			ufbx_vec3 ref = { 0.0f, 1.0f, 0.0f };
			ufbxt_assert_close_vec3(err, material->fbx.diffuse_color.value_vec3, ref);
		}
	}
}
#endif

#if UFBXT_IMPL
ufbx_load_opts ufbxt_unicode_error_question_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.unicode_error_handling = UFBX_UNICODE_ERROR_HANDLING_QUESTION_MARK;
	return opts;
}
#endif

UFBXT_FILE_TEST_OPTS_FLAGS(synthetic_broken_filename, ufbxt_unicode_error_question_opts, UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE)
#if UFBXT_IMPL
{
	ufbxt_assert(scene->textures.count == 1);

	const char *abs_path_safe = "D:/Dev/clean/ufbx/data/textures/??????_diffuse.png";
	const char *rel_path_safe = "textures\\??????_diffuse.png";
	const char *abs_path_raw = "D:/Dev/clean/ufbx/data/textures/\xaa\xbb\xcc\xdd\xee\xff_diffuse.png";
	const char *rel_path_raw = "textures\\\xaa\xbb\xcc\xdd\xee\xff_diffuse.png";
	const char *name_path_raw = "\xaa\xbb\xcc\xdd\xee\xff_diffuse.png";

	ufbx_texture *texture = scene->textures.data[0];

	ufbxt_assert(texture->absolute_filename.length == strlen(abs_path_safe));
	ufbxt_assert(texture->relative_filename.length == strlen(rel_path_safe));
	ufbxt_assert(!strcmp(texture->absolute_filename.data, abs_path_safe));
	ufbxt_assert(!strcmp(texture->relative_filename.data, rel_path_safe));

	ufbxt_assert(texture->raw_absolute_filename.size == strlen(abs_path_raw));
	ufbxt_assert(texture->raw_relative_filename.size == strlen(rel_path_raw));
	ufbxt_assert(!memcmp(texture->raw_absolute_filename.data, abs_path_raw, texture->raw_absolute_filename.size));
	ufbxt_assert(!memcmp(texture->raw_relative_filename.data, rel_path_raw, texture->raw_relative_filename.size));

	{
		ufbxt_assert(strstr(texture->filename.data, "??????_diffuse.png"));
		const char *begin = memchr((const char*)texture->raw_filename.data, '\xaa', texture->raw_filename.size);
		ufbxt_assert(begin);
		size_t offset = begin - (const char*)texture->raw_filename.data;
		size_t left = texture->raw_filename.size - offset;
		ufbxt_assert(left == strlen(name_path_raw));
		ufbxt_assert(!memcmp(begin, name_path_raw, left));
	}

	ufbx_video *video = scene->videos.data[0];

	ufbxt_assert(video->absolute_filename.length == strlen(abs_path_safe));
	ufbxt_assert(video->relative_filename.length == strlen(rel_path_safe));
	ufbxt_assert(!strcmp(video->absolute_filename.data, abs_path_safe));
	ufbxt_assert(!strcmp(video->relative_filename.data, rel_path_safe));

	ufbxt_assert(video->raw_absolute_filename.size == strlen(abs_path_raw));
	ufbxt_assert(video->raw_relative_filename.size == strlen(rel_path_raw));
	ufbxt_assert(!memcmp(video->raw_absolute_filename.data, abs_path_raw, video->raw_absolute_filename.size));
	ufbxt_assert(!memcmp(video->raw_relative_filename.data, rel_path_raw, video->raw_relative_filename.size));

	{
		ufbxt_assert(strstr(video->filename.data, "??????_diffuse.png"));
		const char *begin = memchr((const char*)video->raw_filename.data, '\xaa', video->raw_filename.size);
		ufbxt_assert(begin);
		size_t offset = begin - (const char*)video->raw_filename.data;
		size_t left = video->raw_filename.size - offset;
		ufbxt_assert(left == strlen(name_path_raw));
		ufbxt_assert(!memcmp(begin, name_path_raw, left));
	}

	{
		ufbx_string abs_path = ufbx_find_string(&video->props, "Path", ufbx_empty_string);
		ufbx_string rel_path = ufbx_find_string(&video->props, "RelPath", ufbx_empty_string);
		ufbxt_assert(abs_path.length == strlen(abs_path_safe));
		ufbxt_assert(rel_path.length == strlen(rel_path_safe));
		ufbxt_assert(!strcmp(abs_path.data, abs_path_safe));
		ufbxt_assert(!strcmp(rel_path.data, rel_path_safe));
	}

	{
		ufbx_blob abs_path = ufbx_find_blob(&video->props, "Path", ufbx_empty_blob);
		ufbx_blob rel_path = ufbx_find_blob(&video->props, "RelPath", ufbx_empty_blob);
		ufbxt_assert(abs_path.size == strlen(abs_path_raw));
		ufbxt_assert(rel_path.size == strlen(rel_path_raw));
		ufbxt_assert(!memcmp(abs_path.data, abs_path_raw, abs_path.size));
		ufbxt_assert(!memcmp(rel_path.data, rel_path_raw, rel_path.size));
	}
}
#endif

#if UFBXT_IMPL
static uint32_t ufbxt_fnv1a(const void *data, size_t size)
{
	const char *src = (const char*)data;
	uint32_t hash = 0x811c9dc5u;
	for (size_t i = 0; i < size; i++) {
		hash = (hash ^ (uint32_t)(uint8_t)src[i]) * 0x01000193u;
	}
	return hash;
}
#endif

UFBXT_FILE_TEST(revit_empty)
#if UFBXT_IMPL
{
	bool found_refs3 = false;
	bool found_refs = false;
	for (size_t i = 0; i < scene->unknowns.count; i++) {
		ufbx_unknown *unknown = scene->unknowns.data[i];
		if (!strcmp(unknown->name.data, "ADSKAssetReferencesVersion3.0")) {
			ufbx_prop *prop = ufbx_find_prop(&unknown->props, "ADSKAssetReferencesBlobVersion3.0");
			uint32_t size = scene->metadata.version == 7700 ? 11228u : 11305u;
			uint32_t hash = scene->metadata.version == 7700 ? 0x5eee86b6u : 0x7f39429du;

			ufbxt_assert(prop);
			ufbxt_assert(prop->type == UFBX_PROP_BLOB);
			ufbxt_assert(prop->value_real == (ufbx_real)size);
			ufbxt_assert(prop->value_int == size);
			ufbxt_assert(prop->value_blob.size == size);

			ufbx_blob blob = ufbx_find_blob(&unknown->props, "ADSKAssetReferencesBlobVersion3.0", ufbx_empty_blob);
			ufbxt_assert(blob.data == prop->value_blob.data);
			ufbxt_assert(blob.size == prop->value_blob.size);

			uint32_t hash_ref = ufbxt_fnv1a(blob.data, blob.size);
			ufbxt_assert(hash_ref == hash);

			ufbxt_assert(!found_refs3);
			found_refs3 = true;
		} else if (!strcmp(unknown->name.data, "ADSKAssetReferences")) {
			ufbx_prop *prop = ufbx_find_prop(&unknown->props, "ADSKAssetReferencesBlob");
			uint32_t size = scene->metadata.version == 7700 ? 7910u : 7885u;
			uint32_t hash = scene->metadata.version == 7700 ? 0xa081f491u : 0x5aaeae9au;

			ufbxt_assert(prop);
			ufbxt_assert(prop->type == UFBX_PROP_BLOB);
			ufbxt_assert(prop->value_real == size);
			ufbxt_assert(prop->value_int == size);
			ufbxt_assert(prop->value_blob.size == size);

			ufbx_blob blob = ufbx_find_blob(&unknown->props, "ADSKAssetReferencesBlob", ufbx_empty_blob);
			ufbxt_assert(blob.data == prop->value_blob.data);
			ufbxt_assert(blob.size == prop->value_blob.size);

			uint32_t hash_ref = ufbxt_fnv1a(blob.data, blob.size);
			ufbxt_assert(hash_ref == hash);

			ufbxt_assert(!found_refs);
			found_refs = true;
		}
	}

	ufbxt_assert(found_refs);
	ufbxt_assert(found_refs3);
}
#endif

UFBXT_FILE_TEST(maya_lock_mute)
#if UFBXT_IMPL
{
	uint32_t any_lock = UFBX_PROP_FLAG_LOCK_X | UFBX_PROP_FLAG_LOCK_Y | UFBX_PROP_FLAG_LOCK_Z | UFBX_PROP_FLAG_LOCK_W;
	uint32_t any_mute = UFBX_PROP_FLAG_MUTE_X | UFBX_PROP_FLAG_MUTE_Y | UFBX_PROP_FLAG_MUTE_Z | UFBX_PROP_FLAG_MUTE_W;
	uint32_t any_lock_mute = any_lock | any_mute;

	{
		ufbx_prop *prop;
		ufbx_node *node = ufbx_find_node(scene, "pCube1");
		ufbxt_assert(node);
		prop = ufbx_find_prop(&node->props, "Lcl Translation");
		ufbxt_assert(prop && (prop->flags & any_lock_mute) == (UFBX_PROP_FLAG_MUTE_X | UFBX_PROP_FLAG_LOCK_Z));
		prop = ufbx_find_prop(&node->props, "Lcl Rotation");
		ufbxt_assert(prop && (prop->flags & any_lock_mute) == (UFBX_PROP_FLAG_MUTE_Y | UFBX_PROP_FLAG_LOCK_Y));
		prop = ufbx_find_prop(&node->props, "Lcl Scaling");
		ufbxt_assert(prop && (prop->flags & any_lock_mute) == (UFBX_PROP_FLAG_MUTE_Z | UFBX_PROP_FLAG_LOCK_X));
	}

	{
		ufbx_prop *prop;
		ufbx_node *node = ufbx_find_node(scene, "pPlane1");
		ufbxt_assert(node);
		prop = ufbx_find_prop(&node->props, "Lcl Translation");
		ufbxt_assert(prop && (prop->flags & any_lock_mute) == (UFBX_PROP_FLAG_MUTE_X | UFBX_PROP_FLAG_MUTE_Y | UFBX_PROP_FLAG_MUTE_Z));
		prop = ufbx_find_prop(&node->props, "Lcl Rotation");
		ufbxt_assert(prop && (prop->flags & any_lock_mute) == (UFBX_PROP_FLAG_LOCK_X | UFBX_PROP_FLAG_LOCK_Y | UFBX_PROP_FLAG_LOCK_Z));
		prop = ufbx_find_prop(&node->props, "Lcl Scaling");
		ufbxt_assert(prop && (prop->flags & any_lock_mute) == (UFBX_PROP_FLAG_MUTE_X | UFBX_PROP_FLAG_MUTE_Y | UFBX_PROP_FLAG_MUTE_Z | UFBX_PROP_FLAG_LOCK_X | UFBX_PROP_FLAG_LOCK_Y | UFBX_PROP_FLAG_LOCK_Z));
	}
}
#endif

#if UFBXT_IMPL
static ufbx_load_opts ufbxt_filename_load_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.filename.data = "fake/path/to/file.fbx";
	opts.filename.length = SIZE_MAX;
	opts.path_separator = '/';
	return opts;
}
#endif

UFBXT_FILE_TEST_OPTS(synthetic_parent_directory, ufbxt_filename_load_opts)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "lambert1");
	ufbxt_assert(material);

	ufbx_texture *temporary = material->fbx.diffuse_color.texture;
	ufbxt_assert(temporary && temporary->video);
	ufbx_video *temporary_video = temporary->video;

	ufbx_texture *inner = material->fbx.transparency_color.texture;
	ufbxt_assert(inner && inner->video);
	ufbx_video *inner_video = inner->video;

	ufbxt_assert(!strcmp(temporary->filename.data, "temporary.png"));
	ufbxt_assert(!strcmp(temporary_video->filename.data, "fake/path/temporary.png"));
	ufbxt_assert(!strcmp(inner->filename.data, "../directory/inner.png"));
	ufbxt_assert(!strcmp(inner_video->filename.data, "fake/path/directory/inner.png"));
}
#endif

#if UFBXT_IMPL
static ufbx_load_opts ufbxt_parent_dir_filename_load_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.filename.data = "fake/../path/./file.fbx";
	opts.filename.length = SIZE_MAX;
	opts.path_separator = '/';
	return opts;
}
#endif

UFBXT_FILE_TEST_OPTS_ALT(synthetic_parent_directory_parent, synthetic_parent_directory, ufbxt_parent_dir_filename_load_opts)
#if UFBXT_IMPL
{
	ufbx_material *material = (ufbx_material*)ufbx_find_element(scene, UFBX_ELEMENT_MATERIAL, "lambert1");
	ufbxt_assert(material);

	ufbx_texture *temporary = material->fbx.diffuse_color.texture;
	ufbxt_assert(temporary && temporary->video);
	ufbx_video *temporary_video = temporary->video;

	ufbx_texture *inner = material->fbx.transparency_color.texture;
	ufbxt_assert(inner && inner->video);
	ufbx_video *inner_video = inner->video;

	ufbxt_assert(!strcmp(temporary->filename.data, "fake/../../../temporary.png"));
	ufbxt_assert(!strcmp(temporary_video->filename.data, "fake/../temporary.png"));
	ufbxt_assert(!strcmp(inner->filename.data, "fake/../../../../directory/inner.png"));
	ufbxt_assert(!strcmp(inner_video->filename.data, "fake/../directory/inner.png"));
}
#endif

UFBXT_TEST(unsafe_options)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "maya_cube" };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		for (uint32_t mask = 0; mask < 0x8; mask++) {
			bool allow_unsafe = (mask & 0x1) != 0;
			uint32_t unsafe_mask = mask >> 1;

			ufbx_load_opts opts = { 0 };
			opts.allow_unsafe = allow_unsafe;
			if ((unsafe_mask & 0x1) != 0) {
				opts.index_error_handling = UFBX_INDEX_ERROR_HANDLING_UNSAFE_IGNORE;
			}
			if ((unsafe_mask & 0x2) != 0) {
				opts.unicode_error_handling = UFBX_UNICODE_ERROR_HANDLING_UNSAFE_IGNORE;
			}
			ufbx_error error;
			ufbx_scene *scene = ufbx_load_file(path, &opts, &error);
			if (allow_unsafe || unsafe_mask == 0) {
				ufbxt_assert(scene);
				ufbx_free_scene(scene);
			} else {
				ufbxt_assert(!scene);
				ufbxt_assert(error.type == UFBX_ERROR_UNSAFE_OPTIONS);
			}
		}
	}
}
#endif

#if UFBXT_IMPL
static ufbx_load_opts ufbxt_underscore_no_index_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.index_error_handling = UFBX_INDEX_ERROR_HANDLING_NO_INDEX;
	opts.unicode_error_handling = UFBX_UNICODE_ERROR_HANDLING_UNDERSCORE;
	return opts;
}
static ufbx_load_opts ufbxt_all_unsafe_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.allow_unsafe = true;
	opts.index_error_handling = UFBX_INDEX_ERROR_HANDLING_UNSAFE_IGNORE;
	opts.unicode_error_handling = UFBX_UNICODE_ERROR_HANDLING_UNSAFE_IGNORE;
	return opts;
}
static ufbx_load_opts ufbxt_fail_index_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.index_error_handling = UFBX_INDEX_ERROR_HANDLING_ABORT_LOADING;
	return opts;
}
static ufbx_load_opts ufbxt_fail_unicode_opts()
{
	ufbx_load_opts opts = { 0 };
	opts.unicode_error_handling = UFBX_UNICODE_ERROR_HANDLING_ABORT_LOADING;
	return opts;
}
#endif

UFBXT_FILE_TEST_FLAGS(synthetic_unsafe_cube, UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE)
#if UFBXT_IMPL
{
	ufbxt_check_warning(scene, UFBX_WARNING_INDEX_CLAMPED, 4, NULL);
	ufbxt_check_warning(scene, UFBX_WARNING_BAD_UNICODE, 1, NULL);

	ufbx_node *node = ufbx_find_node(scene, "pC" "\xef\xbf\xbd" "\xef\xbf\xbd" "e1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_assert(mesh->vertex_uv.indices.data[0] == 0);
	ufbxt_assert(mesh->vertex_uv.indices.data[1] == 1);
	ufbxt_assert(mesh->vertex_uv.indices.data[2] == 3);
	ufbxt_assert(mesh->vertex_uv.indices.data[3] == 13);
	ufbxt_assert(mesh->vertex_uv.indices.data[4] == 13);
	ufbxt_assert(mesh->vertex_uv.indices.data[5] == 13);
	ufbxt_assert(mesh->vertex_uv.indices.data[6] == 13);
	ufbxt_assert(mesh->vertex_uv.indices.data[7] == 4);
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(synthetic_unsafe_cube_underscore_no_index, synthetic_unsafe_cube, ufbxt_underscore_no_index_opts, UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pC__e1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_assert(mesh->vertex_uv.indices.data[0] == 0);
	ufbxt_assert(mesh->vertex_uv.indices.data[1] == 1);
	ufbxt_assert(mesh->vertex_uv.indices.data[2] == 3);
	ufbxt_assert(mesh->vertex_uv.indices.data[3] == UFBX_NO_INDEX);
	ufbxt_assert(mesh->vertex_uv.indices.data[4] == UFBX_NO_INDEX);
	ufbxt_assert(mesh->vertex_uv.indices.data[5] == UFBX_NO_INDEX);
	ufbxt_assert(mesh->vertex_uv.indices.data[6] == UFBX_NO_INDEX);
	ufbxt_assert(mesh->vertex_uv.indices.data[7] == 4);

	for (size_t i = 3; i <= 6; i++) {
		ufbx_vec2 v = ufbx_get_vertex_vec2(&mesh->vertex_uv, i);
		ufbxt_assert_close_vec2(err, v, ufbx_zero_vec2);
	}
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(synthetic_unsafe_cube_unsafe, synthetic_unsafe_cube, ufbxt_all_unsafe_opts, UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node_len(scene, "pC" "\xff" "\x00" "e1", 6);
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;
	ufbxt_assert(mesh->vertex_uv.indices.data[0] == 0);
	ufbxt_assert(mesh->vertex_uv.indices.data[1] == 1);
	ufbxt_assert(mesh->vertex_uv.indices.data[2] == 3);
	ufbxt_assert(mesh->vertex_uv.indices.data[3] == 0xffu);
	ufbxt_assert(mesh->vertex_uv.indices.data[4] == 0xffffu);
	ufbxt_assert(mesh->vertex_uv.indices.data[5] == 0xffffffu);
	ufbxt_assert(mesh->vertex_uv.indices.data[6] == 0xffffffffu);
	ufbxt_assert(mesh->vertex_uv.indices.data[7] == 4);
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(synthetic_unsafe_cube_fail_index, synthetic_unsafe_cube, ufbxt_fail_index_opts, UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE | UFBXT_FILE_TEST_FLAG_ALLOW_ERROR)
#if UFBXT_IMPL
{
	ufbxt_assert(!scene);
	ufbxt_assert(load_error->type == UFBX_ERROR_BAD_INDEX);
}
#endif

UFBXT_FILE_TEST_OPTS_ALT_FLAGS(synthetic_unsafe_cube_fail_unicode, synthetic_unsafe_cube, ufbxt_fail_unicode_opts, UFBXT_FILE_TEST_FLAG_ALLOW_INVALID_UNICODE | UFBXT_FILE_TEST_FLAG_ALLOW_ERROR)
#if UFBXT_IMPL
{
	ufbxt_assert(!scene);
	ufbxt_assert(load_error->type == UFBX_ERROR_INVALID_UTF8);
}
#endif

UFBXT_FILE_TEST_ALT(find_prop_concat, maya_node_attribute_zoo)
#if UFBXT_IMPL
{
	{
		ufbx_node *node = ufbx_find_node(scene, "Null");
		ufbxt_assert(node);

		{
			ufbx_string parts[] = {
				{ "Geometric", 9 },
				{ "Rotation", 8 },
			};

			ufbx_prop *prop = ufbx_find_prop_concat(&node->props, parts, 2);
			ufbxt_assert(prop);
			ufbxt_assert(!strcmp(prop->name.data, "GeometricRotation"));
		}

		{
			ufbx_string parts[] = {
				{ "Geometric", SIZE_MAX },
				{ "Translation", SIZE_MAX },
			};

			ufbx_prop *prop = ufbx_find_prop_concat(&node->props, parts, 2);
			ufbxt_assert(prop);
			ufbxt_assert(!strcmp(prop->name.data, "GeometricTranslation"));
		}
	}

	char chars[1024];
	ufbx_string parts[512];
	size_t num_parts = 0;

	for (size_t elem_ix = 0; elem_ix < scene->elements.count; elem_ix++) {
		ufbx_element *element = scene->elements.data[elem_ix];
		ufbx_props *props = &element->props;

		while (props) {
			for (size_t prop_ix = 0; prop_ix < props->props.count; prop_ix++) {
				ufbx_prop *prop = &props->props.data[prop_ix];
				ufbx_string name = prop->name;
				ufbxt_assert(name.length * 2 < ufbxt_arraycount(parts));

				// One single part
				num_parts = 0;
				parts[num_parts] = name;
				num_parts++;
				ufbxt_assert(ufbx_find_prop_concat(props, parts, num_parts) == prop);

				// One single part (NULL terminated)
				num_parts = 0;
				parts[num_parts].data = name.data;
				parts[num_parts].length = SIZE_MAX;
				num_parts++;
				ufbxt_assert(ufbx_find_prop_concat(props, parts, num_parts) == prop);

				// Single characters
				num_parts = 0;
				for (size_t i = 0; i < name.length; i++) {
					parts[num_parts].data = &name.data[i];
					parts[num_parts].length = 1;
					num_parts++;
				}
				ufbxt_assert(ufbx_find_prop_concat(props, parts, num_parts) == prop);

				// Single characters (NULL terminated)
				num_parts = 0;
				for (size_t i = 0; i < name.length; i++) {
					char *part = chars + num_parts*2;
					part[0] = name.data[i];
					part[1] = '\0';
					parts[num_parts].data = part;
					parts[num_parts].length = SIZE_MAX;
					num_parts++;
				}
				ufbxt_assert(ufbx_find_prop_concat(props, parts, num_parts) == prop);

				// Single characters with empty in between
				num_parts = 0;
				for (size_t i = 0; i < name.length; i++) {
					parts[num_parts].data = &name.data[i];
					parts[num_parts].length = 1;
					num_parts++;
					parts[num_parts].data = NULL;
					parts[num_parts].length = 0;
					num_parts++;
				}
				ufbxt_assert(ufbx_find_prop_concat(props, parts, num_parts) == prop);

				// Even parts
				for (size_t step = 1; step < name.length; step++) {
					num_parts = 0;
					for (size_t i = 0; i < name.length; i += step) {
						parts[num_parts].data = name.data + i;
						parts[num_parts].length = step;
						if (i + step > name.length) {
							parts[num_parts].length = SIZE_MAX;
						}
						num_parts++;
					}
					ufbxt_assert(ufbx_find_prop_concat(props, parts, num_parts) == prop);
				}

			}

			props = props->defaults;
		}
	}
}
#endif

UFBXT_FILE_TEST(synthetic_duplicate_id)
#if UFBXT_IMPL
{
	if (scene->metadata.version >= 7000) {
		ufbxt_check_warning(scene, UFBX_WARNING_DUPLICATE_OBJECT_ID, 5, NULL);
	} else {
		ufbxt_check_warning(scene, UFBX_WARNING_DUPLICATE_OBJECT_ID, 3, NULL);
	}

	// Don't make any assumptions about the scene, ufbxt_check_scene() makes sure
	// that it doesn't break any of the API guarantees.
}
#endif

UFBXT_FILE_TEST_OPTS(synthetic_recursive_transform, ufbxt_retain_dom_opts)
#if UFBXT_IMPL
{
	ufbx_dom_node *takes = ufbx_dom_find(scene->dom_root, "Takes");
	ufbxt_assert(takes);
	ufbx_dom_node *take = ufbx_dom_find(takes, "Take");
	ufbxt_assert(take);
	ufbx_dom_node *model = ufbx_dom_find(take, "Model");
	ufbxt_assert(model);
	ufbx_dom_node *channel = ufbx_dom_find(model, "Channel");
	ufbxt_assert(channel);

	ufbxt_assert(channel->values.count == 1);
	ufbxt_assert(channel->values.data[0].type == UFBX_DOM_VALUE_STRING);
	ufbxt_assert(!strcmp(channel->values.data[0].value_str.data, "Transform"));

	ufbx_dom_node *rec_channel = ufbx_dom_find(channel, "Channel");
	ufbxt_assert(rec_channel);

	size_t count = 1;
	for (;;) {
		ufbxt_hintf("count=%zu", count);
		count += 1;
		ufbxt_assert(rec_channel->values.count == 1);
		ufbxt_assert(rec_channel->values.data[0].type == UFBX_DOM_VALUE_STRING);
		ufbxt_assert(!strcmp(rec_channel->values.data[0].value_str.data, "Transform"));

		if (rec_channel->children.count == 0) break;
		ufbxt_assert(rec_channel->children.count == 1);
		rec_channel = rec_channel->children.data[0];
	}

	ufbxt_assert(count == 29);
}
#endif

UFBXT_FILE_TEST(synthetic_recursive_connections)
#if UFBXT_IMPL
{
	{
		ufbx_node *node = ufbx_find_node(scene, "pCube1");
		ufbxt_assert(node);

		ufbx_prop *prop = ufbx_find_prop(&node->props, "Lcl Translation");
		ufbxt_assert(prop);
		ufbxt_assert((prop->flags & UFBX_PROP_FLAG_CONNECTED) != 0);

		ufbx_vec3 ref = { 1.0f, 0.0f, 0.0f };
		ufbx_prop value = ufbx_evaluate_prop(&scene->anim, &node->element, "Lcl Translation", 0.5);
		ufbxt_assert_close_vec3(err, value.value_vec3, ref);
	}

	{
		ufbx_node *node = ufbx_find_node(scene, "pCube3");
		ufbxt_assert(node);

		ufbx_prop *prop = ufbx_find_prop(&node->props, "Lcl Translation");
		ufbxt_assert(prop);
		ufbxt_assert((prop->flags & UFBX_PROP_FLAG_CONNECTED) != 0);

		ufbx_vec3 ref = { 0.0f, 0.0f, 1.0f };
		ufbx_prop value = ufbx_evaluate_prop(&scene->anim, &node->element, "Lcl Translation", 0.5);
		ufbxt_assert_close_vec3(err, value.value_vec3, ref);
	}
}
#endif

UFBXT_FILE_TEST(synthetic_rotation_order_layers)
#if UFBXT_IMPL
{
	ufbx_anim_layer *layer_base_layer = ufbx_as_anim_layer(ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "BaseLayer"));
	ufbx_anim_layer *layer_base = ufbx_as_anim_layer(ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "Base"));
	ufbx_anim_layer *layer_rotation = ufbx_as_anim_layer(ufbx_find_element(scene, UFBX_ELEMENT_ANIM_LAYER, "Rotation"));

	ufbx_anim_layer_desc layers[] = {
		{ layer_base_layer, 1.0f },
		{ layer_base, 0.5f },
		{ layer_rotation, 0.25f },
	};

	ufbx_anim anim = { 0 };
	anim.layers.data = layers;
	anim.layers.count = ufbxt_arraycount(layers);

	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node);

	// We don't really care about the result here as runtime rotation order is not really supported,
	// just want to make sure we don't hit any infinite recursion with rotation order layers
	(void)ufbx_evaluate_prop(&anim, &node->element, "Lcl Rotation", 0.2f);
}
#endif

UFBXT_FILE_TEST(maya_notes)
#if UFBXT_IMPL
{
	{
		ufbx_node *node = ufbx_find_node(scene, "pCube1");
		ufbxt_assert(node);
		ufbx_prop *notes = ufbx_find_prop(&node->props, "notes");
		ufbxt_assert(notes);

		const char *lines[] = {
			"pCube1 notes:\n",
		};

		const char *str = notes->value_str.data;
		for (size_t i = 0; i < ufbxt_arraycount(lines); i++) {
			ufbxt_hintf("i=%zu", i);
			const char *end = strchr(str, '\n');
			if (!end) {
				end = str + strlen(str);
			} else {
				end += 1;
			}

			size_t len = (size_t)(end - str);
			ufbxt_assert(!strncmp(str, lines[i], len));
			ufbxt_assert(len == strlen(lines[i]));
			str = end;
		}

		for (size_t i = 0; i < 4096; i++) {
			char *end;
			unsigned long l = strtoul(str, &end, 10);
			ufbxt_assert(l == i);
			if (i + 1 < 16384) {
				ufbxt_assert(*end == ' ');
				str = end + 1;
			}
		}
	}

	{
		ufbx_material *material = ufbx_find_material(scene, "lambert1");
		ufbxt_assert(material);
		ufbx_prop *notes = ufbx_find_prop(&material->props, "notes");
		ufbxt_assert(notes);

		const char *lines[] = {
			"lambert1 notes:\n",
			"\n",
			"- material\n",
			"- \"lambertian\"\n",
			"- unicode: \x61\xc3\x9f???\n",
			"- tab\t\n",
			"- cr \n",
		};

		const char *str = notes->value_str.data;
		for (size_t i = 0; i < ufbxt_arraycount(lines); i++) {
			ufbxt_hintf("i=%zu", i);
			const char *end = strchr(str, '\n');
			if (!end) {
				end = str + strlen(str);
			} else {
				end += 1;
			}

			size_t len = (size_t)(end - str);
			ufbxt_assert(!strncmp(str, lines[i], len));
			ufbxt_assert(len == strlen(lines[i]));
			str = end;
		}

		ufbxt_assert(*str == '\0');
	}
}
#endif

