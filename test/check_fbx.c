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
		"MotionBuilder",
	};

	const char *formats[2][2] = {
		{ "binary", "binary (big-endian)" },
		{ "ascii", "!?!?ascii (big-endian)!?!?" },
	};

	printf("FBX %u %s via %s %u.%u.%u\n",
		scene->metadata.version,
		formats[scene->metadata.ascii][scene->metadata.big_endian],
		exporters[scene->metadata.exporter],
		ufbx_version_major(scene->metadata.exporter_version),
		ufbx_version_minor(scene->metadata.exporter_version),
		ufbx_version_patch(scene->metadata.exporter_version));

	int result = 0;

	for (size_t i = 0; i < scene->unknowns.count; i++) {
		ufbx_unknown *unknown = scene->unknowns.data[i];
		fprintf(stderr, "Unknown element: %s/%s : %s\n", unknown->type.data, unknown->sub_type.data, unknown->name.data);
		result = 3;
	}

	bool known_unknown = false;
	if (strstr(scene->metadata.creator.data, "kenney")) known_unknown = true;
	if (strstr(scene->metadata.creator.data, "assetforge")) known_unknown = true;
	if (scene->metadata.version < 5800) known_unknown = true;
	ufbxt_assert(scene->metadata.exporter != UFBX_EXPORTER_UNKNOWN || known_unknown);

	ufbxt_check_scene(scene);

	ufbx_free_scene(scene);

	return result;
}


#include "../ufbx.c"
