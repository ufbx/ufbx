#undef UFBXT_TEST_GROUP
#define UFBXT_TEST_GROUP "api"

#if UFBXT_IMPL

static void ufbxt_close_memory(void *user, void *data, size_t data_size)
{
	free(data);
}

static bool ufbxt_open_file_memory_default(void *user, ufbx_stream *stream, const char *path, size_t path_len, const ufbx_open_file_info *info)
{
	++*(size_t*)user;

	size_t size;
	void *data = ufbxt_read_file(path, &size);
	if (!data) return false;

	bool ok = ufbx_open_memory(stream, data, size, NULL, NULL);
	free(data);
	return ok;
}

static bool ufbxt_open_file_memory_temp(void *user, ufbx_stream *stream, const char *path, size_t path_len, const ufbx_open_file_info *info)
{
	++*(size_t*)user;

	size_t size;
	void *data = ufbxt_read_file(path, &size);
	if (!data) return false;

	ufbx_open_memory_opts opts = { 0 };
	opts.allocator.allocator = info->temp_allocator;

	bool ok = ufbx_open_memory(stream, data, size, &opts, NULL);
	free(data);
	return ok;
}

static bool ufbxt_open_file_memory_ref(void *user, ufbx_stream *stream, const char *path, size_t path_len, const ufbx_open_file_info *info)
{
	++*(size_t*)user;

	size_t size;
	void *data = ufbxt_read_file(path, &size);
	if (!data) return false;

	ufbx_open_memory_opts opts = { 0 };
	opts.no_copy = true;
	opts.close_cb.fn = &ufbxt_close_memory;
	return ufbx_open_memory(stream, data, size, &opts, NULL);
}

#endif

#if UFBXT_IMPL
static void ufbxt_do_open_memory_test(const char *filename, size_t expected_calls_fbx, size_t expected_calls_obj, ufbx_open_file_fn *open_file_fn)
{
	char path[512];
	ufbxt_file_iterator iter = { filename };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		for (size_t i = 0; i < 2; i++) {
			ufbx_load_opts opts = { 0 };
			size_t num_calls = 0;

			opts.open_file_cb.fn = open_file_fn;
			opts.open_file_cb.user = &num_calls;
			opts.load_external_files = true;
			if (i == 1) {
				opts.read_buffer_size = 1;
			}

			ufbx_error error;
			ufbx_scene *scene = ufbx_load_file(path, &opts, &error);
			if (!scene) ufbxt_log_error(&error);
			ufbxt_assert(scene);

			ufbxt_check_scene(scene);

			if (scene->metadata.file_format == UFBX_FILE_FORMAT_FBX) {
				ufbxt_assert(num_calls == expected_calls_fbx);
			} else if (scene->metadata.file_format == UFBX_FILE_FORMAT_OBJ) {
				ufbxt_assert(num_calls == expected_calls_obj);
			} else {
				ufbxt_assert(false);
			}

			ufbx_free_scene(scene);
		}
	}
}
#endif

UFBXT_TEST(open_memory_default)
#if UFBXT_IMPL
{
	ufbxt_do_open_memory_test("maya_cache_sine", 5, 0, ufbxt_open_file_memory_default);
}
#endif

UFBXT_TEST(open_memory_temp)
#if UFBXT_IMPL
{
	ufbxt_do_open_memory_test("maya_cache_sine", 5, 0, ufbxt_open_file_memory_temp);
}
#endif

UFBXT_TEST(open_memory_ref)
#if UFBXT_IMPL
{
	ufbxt_do_open_memory_test("maya_cache_sine", 5, 0, ufbxt_open_file_memory_ref);
}
#endif

UFBXT_TEST(obj_open_memory_default)
#if UFBXT_IMPL
{
	ufbxt_do_open_memory_test("blender_279_ball", 1, 2, ufbxt_open_file_memory_default);
}
#endif

UFBXT_TEST(obj_open_memory_temp)
#if UFBXT_IMPL
{
	ufbxt_do_open_memory_test("blender_279_ball", 1, 2, ufbxt_open_file_memory_temp);
}
#endif

UFBXT_TEST(obj_open_memory_ref)
#if UFBXT_IMPL
{
	ufbxt_do_open_memory_test("blender_279_ball", 1, 2, ufbxt_open_file_memory_ref);
}
#endif

UFBXT_FILE_TEST_ALT(find_face_index, maya_cube)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	for (size_t i = 0; i < mesh->num_indices; i++) {
		ufbxt_hintf("i=%zu", i);
		uint32_t face_ix = ufbx_find_face_index(mesh, i);
		ufbxt_assert(face_ix == i / 4);
	}

	{
		uint32_t face_ix = ufbx_find_face_index(mesh, mesh->num_indices);
		ufbxt_assert(face_ix == UFBX_NO_INDEX);
	}

	{
		uint32_t face_ix = ufbx_find_face_index(mesh, SIZE_MAX);
		ufbxt_assert(face_ix == UFBX_NO_INDEX);
	}
}
#endif

UFBXT_FILE_TEST_ALT(find_face_index_bad_face, maya_bad_face)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "pCube1");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	ufbxt_assert(ufbx_find_face_index(mesh, 0) == 0);
	ufbxt_assert(ufbx_find_face_index(mesh, 1) == 1);
	ufbxt_assert(ufbx_find_face_index(mesh, 2) == 1);
	ufbxt_assert(ufbx_find_face_index(mesh, 3) == 2);
	ufbxt_assert(ufbx_find_face_index(mesh, 4) == 2);
	ufbxt_assert(ufbx_find_face_index(mesh, 5) == 2);


	for (size_t face_ix = 0; face_ix < mesh->faces.count; face_ix++) {
		ufbx_face face = mesh->faces.data[face_ix];
		for (size_t i = 0; i < face.num_indices; i++) {
			ufbxt_hintf("face_ix=%zu i=%zu", face_ix, i);
			ufbxt_assert(ufbx_find_face_index(mesh, face.index_begin + i) == face_ix);
		}
	}
}
#endif

