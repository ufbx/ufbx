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

#include "../ufbx.c"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char g_buffer[1024*1024];

int main(int argc, char **argv)
{
	ufbx_load_opts opts = { 0 };

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d")) {
			opts.temp_huge_size = 1;
			opts.result_huge_size = 1;
		}
	}

	while (__AFL_LOOP(10000)) {
		size_t size = (size_t)read(0, g_buffer, sizeof(g_buffer));

		ufbx_scene *scene = ufbx_load_memory(g_buffer, size, &opts, NULL);
		ufbx_free_scene(scene);
	}

	return 0;
}

