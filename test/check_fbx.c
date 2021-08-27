#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

static void ufbxt_assert_fail(const char *func, const char *file, size_t line, const char *msg)
{
	fprintf(stderr, "%s:%zu: %s(\"%s\") failed\n", file, line, func, msg);
	exit(2);
}

#define ufbxt_assert(m_cond) do { if (!(m_cond)) ufbxt_assert_fail("ufbxt_assert", __FILE__, __LINE__, #m_cond); } while (0)
#define ufbx_assert(m_cond) do { if (!(m_cond)) ufbxt_assert_fail("ufbx_assert", __FILE__, __LINE__, #m_cond); } while (0)

#include "../ufbx.h"
#include "check_scene.h"

#ifdef _WIN32
int wmain(int argc, wchar_t **argv)
#else
int main(int argc, char **argv)
#endif
{
	ufbxt_assert(argc == 2);
#if _WIN32
	char path[1024];
	int res = WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, path, sizeof(path), NULL, NULL);
	ufbxt_assert(res > 0 && res < sizeof(path));
#else
	const char *path = argv[1];
#endif

	ufbx_error error;
	ufbx_scene *scene = ufbx_load_file(path, NULL, &error);

	if (!scene) {
		char buf[1024];
		ufbx_format_error(buf, sizeof(buf), &error);
		fprintf(stderr, "%s\n", buf);
		return 1;
	}

	const char *exporters[] = {
		"Unknown",
		"FBX SDK",
		"Blender Binary",
		"Blender ASCII",
	};

	printf("FBX %u via %s %u.%u.%u\n", scene->metadata.version,
		exporters[scene->metadata.exporter],
		ufbx_version_major(scene->metadata.exporter_version),
		ufbx_version_minor(scene->metadata.exporter_version),
		ufbx_version_patch(scene->metadata.exporter_version));

	ufbxt_assert(scene->metadata.exporter != UFBX_EXPORTER_UNKNOWN);

	ufbxt_check_scene(scene);

	ufbx_free_scene(scene);

	return 0;
}


#include "../ufbx.c"
