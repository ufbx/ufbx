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

UFBXT_TEST(retain_free_null)
#if UFBXT_IMPL
{
	ufbx_retain_scene(NULL);
	ufbx_free_scene(NULL);
	ufbx_retain_mesh(NULL);
	ufbx_free_mesh(NULL);
	ufbx_retain_line_curve(NULL);
	ufbx_free_line_curve(NULL);
	ufbx_retain_geometry_cache(NULL);
	ufbx_free_geometry_cache(NULL);
	ufbx_retain_anim(NULL);
	ufbx_free_anim(NULL);
	ufbx_retain_baked_anim(NULL);
	ufbx_free_baked_anim(NULL);
}
#endif

UFBXT_TEST(thread_memory)
#if UFBXT_IMPL
{
	ufbx_retain_scene(NULL);
	ufbx_free_scene(NULL);
	ufbx_retain_mesh(NULL);
	ufbx_free_mesh(NULL);
	ufbx_retain_line_curve(NULL);
	ufbx_free_line_curve(NULL);
	ufbx_retain_geometry_cache(NULL);
	ufbx_free_geometry_cache(NULL);
	ufbx_retain_anim(NULL);
	ufbx_free_anim(NULL);
	ufbx_retain_baked_anim(NULL);
	ufbx_free_baked_anim(NULL);
}
#endif

#if UFBXT_IMPL
typedef struct {
	bool immediate;
	bool initialized;
	bool freed;
	uint32_t wait_index;
	uint32_t dispatches;
} ufbxt_single_thread_pool;

static bool ufbxt_single_thread_pool_init_fn(void *user, ufbx_thread_pool_context ctx, const ufbx_thread_pool_info *info)
{
	ufbxt_single_thread_pool *pool = (ufbxt_single_thread_pool*)user;
	pool->initialized = true;

	return true;
}

static bool ufbxt_single_thread_pool_run_fn(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t start_index, uint32_t count)
{
	ufbxt_single_thread_pool *pool = (ufbxt_single_thread_pool*)user;
	ufbxt_assert(pool->initialized);
	pool->dispatches++;
	if (!pool->immediate) return true;

	for (uint32_t i = 0; i < count; i++) {
		ufbx_thread_pool_run_task(ctx, start_index + i);
	}

	return true;
}

static bool ufbxt_single_thread_pool_wait_fn(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t max_index)
{
	ufbxt_single_thread_pool *pool = (ufbxt_single_thread_pool*)user;
	ufbxt_assert(pool->initialized);

	if (!pool->immediate) {
		for (uint32_t i = pool->wait_index; i < max_index; i++) {
			ufbx_thread_pool_run_task(ctx, i);
		}
	}

	pool->wait_index = max_index;

	return true;
}

static void ufbxt_single_thread_pool_free_fn(void *user, ufbx_thread_pool_context ctx)
{
	ufbxt_single_thread_pool *pool = (ufbxt_single_thread_pool*)user;
	pool->freed = true;
}

static void ufbxt_single_thread_pool_init(ufbx_thread_pool *dst, ufbxt_single_thread_pool *pool, bool immediate)
{
	memset(pool, 0, sizeof(ufbxt_single_thread_pool));
	pool->immediate = immediate;

	dst->init_fn = ufbxt_single_thread_pool_init_fn;
	dst->run_fn = ufbxt_single_thread_pool_run_fn;
	dst->wait_fn = ufbxt_single_thread_pool_wait_fn;
	dst->free_fn = ufbxt_single_thread_pool_free_fn;
	dst->user = pool;
}
#endif

#if UFBXT_IMPL
static bool ufbxt_is_big_endian()
{
		uint8_t buf[2];
		uint16_t val = 0xbbaa;
		memcpy(buf, &val, 2);
		return buf[0] == 0xbb;
}
#endif

UFBXT_TEST(single_thread_immediate_stream)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "blender_293_barbarian" };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		ufbxt_single_thread_pool pool;
		ufbx_load_opts opts = { 0 };
		ufbxt_single_thread_pool_init(&opts.thread_opts.pool, &pool, true);

		ufbx_error error;
		ufbx_scene *scene = ufbx_load_file(path, &opts, &error);
		if (!scene) ufbxt_log_error(&error);
		ufbxt_assert(scene);

		ufbxt_assert(pool.initialized);
		ufbxt_assert(pool.freed);
		if (ufbxt_is_big_endian()) {
			ufbxt_assert(pool.wait_index == 0);
		} else {
			ufbxt_assert(pool.wait_index >= 100);
		}

		ufbxt_check_scene(scene);
		ufbx_free_scene(scene);
	}
}
#endif

UFBXT_TEST(single_thread_immediate_memory)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "blender_293_barbarian" };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		ufbxt_single_thread_pool pool;
		ufbx_load_opts opts = { 0 };
		ufbxt_single_thread_pool_init(&opts.thread_opts.pool, &pool, true);

		size_t size = 0;
		void *data = ufbxt_read_file(path, &size);
		ufbxt_assert(data);

		ufbx_error error;
		ufbx_scene *scene = ufbx_load_memory(data, size, &opts, &error);
		if (!scene) ufbxt_log_error(&error);
		ufbxt_assert(scene);

		ufbxt_assert(pool.initialized);
		ufbxt_assert(pool.freed);
		if (ufbxt_is_big_endian()) {
			ufbxt_assert(pool.wait_index == 0);
		} else {
			ufbxt_assert(pool.wait_index >= 100);
		}

		ufbxt_check_scene(scene);
		ufbx_free_scene(scene);
		free(data);
	}
}
#endif

UFBXT_TEST(single_thread_deferred_stream)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "blender_293_barbarian" };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		ufbxt_single_thread_pool pool;
		ufbx_load_opts opts = { 0 };
		ufbxt_single_thread_pool_init(&opts.thread_opts.pool, &pool, false);

		ufbx_error error;
		ufbx_scene *scene = ufbx_load_file(path, &opts, &error);
		if (!scene) ufbxt_log_error(&error);
		ufbxt_assert(scene);

		ufbxt_assert(pool.initialized);
		ufbxt_assert(pool.freed);
		if (ufbxt_is_big_endian()) {
			ufbxt_assert(pool.wait_index == 0);
		} else {
			ufbxt_assert(pool.wait_index >= 100);
		}

		ufbxt_check_scene(scene);
		ufbx_free_scene(scene);
	}
}
#endif

UFBXT_TEST(single_thread_deferred_memory)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "blender_293_barbarian" };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		ufbxt_single_thread_pool pool;
		ufbx_load_opts opts = { 0 };
		ufbxt_single_thread_pool_init(&opts.thread_opts.pool, &pool, false);

		size_t size = 0;
		void *data = ufbxt_read_file(path, &size);
		ufbxt_assert(data);

		ufbx_error error;
		ufbx_scene *scene = ufbx_load_memory(data, size, &opts, &error);
		if (!scene) ufbxt_log_error(&error);
		ufbxt_assert(scene);

		ufbxt_assert(pool.initialized);
		ufbxt_assert(pool.freed);
		if (ufbxt_is_big_endian()) {
			ufbxt_assert(pool.wait_index == 0);
		} else {
			ufbxt_assert(pool.wait_index >= 100);
		}

		ufbxt_check_scene(scene);
		ufbx_free_scene(scene);
		free(data);
	}
}
#endif

UFBXT_TEST(thread_memory_limit)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "blender_293_barbarian" };
	size_t prev_dispatches = 0;
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		for (size_t i = 0; i < 24; i++) {
			ufbxt_single_thread_pool pool;
			ufbx_load_opts opts = { 0 };
			ufbxt_single_thread_pool_init(&opts.thread_opts.pool, &pool, true);
			opts.thread_opts.memory_limit = (size_t)1 << i;

			size_t size = 0;
			void *data = ufbxt_read_file(path, &size);
			ufbxt_assert(data);

			ufbx_error error;
			ufbx_scene *scene = ufbx_load_memory(data, size, &opts, &error);
			if (!scene) ufbxt_log_error(&error);
			ufbxt_assert(scene);

			if (pool.dispatches != prev_dispatches) {
				ufbxt_logf("limit %zu dispatches: %u", opts.thread_opts.memory_limit, pool.dispatches);
				prev_dispatches = pool.dispatches;
			}

			ufbxt_assert(pool.initialized);
			ufbxt_assert(pool.freed);
			if (ufbxt_is_big_endian()) {
				ufbxt_assert(pool.wait_index == 0);
			} else {
				ufbxt_assert(pool.wait_index >= 100);
			}

			ufbxt_check_scene(scene);
			ufbx_free_scene(scene);
			free(data);
		}
	}
}
#endif

UFBXT_TEST(single_thread_file_not_found)
#if UFBXT_IMPL
{
	ufbxt_single_thread_pool pool;
	ufbx_load_opts opts = { 0 };
	ufbxt_single_thread_pool_init(&opts.thread_opts.pool, &pool, true);

	ufbx_error error;
	ufbx_scene *scene = ufbx_load_file("<doesnotexist>.fbx", &opts, &error);
	ufbxt_assert(!scene);
	ufbxt_assert(error.type == UFBX_ERROR_FILE_NOT_FOUND);
	ufbxt_assert(strstr(error.info, "<doesnotexist>.fbx"));

	ufbxt_assert(!pool.initialized);
	ufbxt_assert(!pool.freed);
	ufbxt_assert(pool.wait_index == 0);
}
#endif

UFBXT_TEST(empty_file_memory)
#if UFBXT_IMPL
{
	{
		ufbx_error error;
		ufbx_scene *scene = ufbx_load_memory(NULL, 0, NULL, &error);
		ufbxt_assert(!scene);
		ufbxt_assert(error.type == UFBX_ERROR_EMPTY_FILE);
	}

	{
		ufbx_load_opts opts = { 0 };
		opts.file_format = UFBX_FILE_FORMAT_FBX;
		ufbx_error error;
		ufbx_scene *scene = ufbx_load_memory(NULL, 0, &opts, &error);
		ufbxt_assert(!scene);
		ufbxt_assert(error.type == UFBX_ERROR_EMPTY_FILE);
	}
}
#endif

#if UFBXT_IMPL
static size_t ufbxt_empty_stream_read_fn(void *user, void *data, size_t size)
{
	return 0;
}
#endif

UFBXT_TEST(empty_file_stream)
#if UFBXT_IMPL
{
	ufbx_stream stream = { 0 };
	stream.read_fn = &ufbxt_empty_stream_read_fn;

	{
		ufbx_error error;
		ufbx_scene *scene = ufbx_load_stream(&stream, NULL, &error);
		ufbxt_assert(!scene);
		ufbxt_assert(error.type == UFBX_ERROR_EMPTY_FILE);
	}

	{
		ufbx_load_opts opts = { 0 };
		opts.file_format = UFBX_FILE_FORMAT_FBX;
		ufbx_error error;
		ufbx_scene *scene = ufbx_load_stream(&stream, &opts, &error);
		ufbxt_assert(!scene);
		ufbxt_assert(error.type == UFBX_ERROR_EMPTY_FILE);
	}
}
#endif

UFBXT_TEST(file_format_lookahead)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "maya_cube" };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		for (size_t i = 0; i <= 16; i++) {
			ufbxt_hintf("i=%zu", i);

			ufbx_load_opts opts = { 0 };
			opts.file_format_lookahead = i * i * i;

			ufbx_error error;
			ufbx_scene *scene = ufbx_load_file(path, &opts, &error);
			if (!scene) ufbxt_log_error(&error);
			ufbxt_assert(scene);

			ufbxt_check_scene(scene);
			ufbx_free_scene(scene);
		}
	}
}
#endif

#if UFBXT_IMPL
static void *ufbxt_multiuse_realloc(void *user, void *old_ptr, size_t old_size, size_t new_size)
{
	if (new_size == 0) {
		free(old_ptr);
		return NULL;
	} else if (old_size > 0) {
		return realloc(old_ptr, new_size);
	} else {
		return malloc(new_size);
	}
}
#endif

UFBXT_TEST(multiuse_realloc)
#if UFBXT_IMPL
{
	char path[512];
	ufbxt_file_iterator iter = { "maya_cube" };
	while (ufbxt_next_file(&iter, path, sizeof(path))) {
		for (size_t i = 0; i <= 16; i++) {
			ufbx_load_opts opts = { 0 };
			opts.temp_allocator.allocator.realloc_fn = &ufbxt_multiuse_realloc;
			opts.result_allocator.allocator.realloc_fn = &ufbxt_multiuse_realloc;

			ufbx_error error;
			ufbx_scene *scene = ufbx_load_file(path, &opts, &error);
			if (!scene) ufbxt_log_error(&error);
			ufbxt_assert(scene);
			ufbxt_check_scene(scene);
			ufbx_free_scene(scene);
		}
	}
}
#endif

