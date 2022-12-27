#define _CRT_SECURE_NO_WARNINGS

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

#include "../ufbx.c"
#include "../test/check_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char g_buffer[1024*1024];

int main(int argc, char **argv)
{
	ufbx_load_opts opts = { 0 };

#if defined(DISCRETE_ALLOCATIONS)
	opts.temp_allocator.huge_threshold = 1;
	opts.result_allocator.huge_threshold = 1;
#endif

#if defined(LOAD_OBJ)
	opts.file_format = UFBX_FILE_FORMAT_OBJ;
#elif defined(LOAD_MTL)
	opts.file_format = UFBX_FILE_FORMAT_MTL;
#elif defined(LOAD_GUESS)
#else
	opts.file_format = UFBX_FILE_FORMAT_FBX;
#endif

#if defined(NO_AFL)
	size_t size = (size_t)read(0, g_buffer, sizeof(g_buffer));
	for (size_t i = 0; i < 10000; i++) {
#else
	while (__AFL_LOOP(10000)) {
		size_t size = (size_t)read(0, g_buffer, sizeof(g_buffer));
#endif

		ufbx_scene *scene = ufbx_load_memory(g_buffer, size, &opts, NULL);
		if (scene) {
			ufbxt_check_scene(scene);
		}
		ufbx_free_scene(scene);
	}

	return 0;
}

