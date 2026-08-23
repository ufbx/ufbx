#include <stdlib.h>
#include <string.h>

#if defined(USE_AFL)
#include <unistd.h>

__AFL_COVERAGE();
#else
#include <stdio.h>
#endif

#if defined(_WIN32)
#define ufbx_assert(cond) do { \
		if (!(cond)) __debugbreak(); \
	} while (0)
#else
#define ufbx_assert(cond) do { \
		if (!(cond)) __builtin_trap(); \
	} while (0)
#endif

#define ufbxt_assert_fail(file, line, msg) ufbx_assert(false)
#define ufbxt_assert(m_cond) ufbx_assert(m_cond)

#include "ufbx.h"

#include "../check_scene.h"

char g_buffer[1024*1024];

static bool accept_format(const void *data, size_t size)
{
#if defined(FUZZ_ASCII)
	static const char ascii_magic[] = "; FBX ";
	if (size >= sizeof(ascii_magic) - 1 && memcmp(data, ascii_magic, sizeof(ascii_magic) - 1) == 0) {
		return true;
	}
#endif
#if defined(FUZZ_BINARY)
	static const char binary_magic[] = "Kaydara FBX Binary  \x00\x1a";
	if (size >= sizeof(binary_magic) - 1 && memcmp(data, binary_magic, sizeof(binary_magic) - 1) == 0) {
		return true;
	}
#endif
	return false;
}

static void inline_thread_run(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t start_index, uint32_t count)
{
	(void)user;
	(void)group;
	for (uint32_t i = 0; i < count; i++) {
		ufbx_thread_pool_run_task(ctx, start_index + i);
	}
}

static void inline_thread_wait(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t max_index)
{
	(void)user;
	(void)ctx;
	(void)group;
	(void)max_index;
}

static ufbx_scene *fuzz_scene(const void *data, size_t size, bool use_threads)
{
	ufbx_load_opts opts = { 0 };
	opts.temp_allocator.huge_threshold = 1;
	opts.result_allocator.huge_threshold = 1;
	opts.file_format = UFBX_FILE_FORMAT_FBX;
	if (use_threads) {
		opts.thread_opts.pool.run_fn = &inline_thread_run;
		opts.thread_opts.pool.wait_fn = &inline_thread_wait;
	}

	ufbx_scene *scene = ufbx_load_memory(data, size, &opts, NULL);
	if (scene) {
		ufbxt_check_scene(scene);
	}
	return scene;
}

static void fuzz_input(const void *data, size_t size)
{
	void *input = malloc(size ? size : 1);
	ufbx_assert(input);
	memcpy(input, data, size);

	ufbx_scene *scene = fuzz_scene(input, size, false);
	ufbx_scene *thread_scene = fuzz_scene(input, size, true);

	if (scene) {
		ufbx_scene *eval = ufbx_evaluate_scene(scene, scene->anim, 0.5, NULL, NULL);
		ufbxt_assert(eval);
		ufbxt_check_scene(eval);
		ufbx_free_scene(eval);
	}
	ufbx_free_scene(scene);
	ufbx_free_scene(thread_scene);

	free(input);
}

int main(void)
{
#if defined(USE_AFL)
	while (__AFL_LOOP(10000)) {
		size_t size = (size_t)read(0, g_buffer, sizeof(g_buffer));
		if (!accept_format(g_buffer, size)) {
			__AFL_COVERAGE_SKIP();
			continue;
		}
		fuzz_input(g_buffer, size);
	}
#else
	size_t size = fread(g_buffer, 1, sizeof(g_buffer), stdin);
	if (accept_format(g_buffer, size)) {
		fuzz_input(g_buffer, size);
	}
#endif

	return 0;
}
