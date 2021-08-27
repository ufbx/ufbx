#include <stdio.h>
#include <stdlib.h>

static void ufbxt_assert_fail(const char *func, const char *file, size_t line, const char *msg)
{
	fprintf(stderr, "%s:%zu: %s(\"%s\") failed\n", file, line, func, msg);
	exit(2);
}

#define ufbxt_assert(m_cond) do { if (!(m_cond)) ufbxt_assert_fail("ufbxt_assert", __FILE__, __LINE__, #m_cond); } while (0)
#define ufbx_assert(m_cond) do { if (!(m_cond)) ufbxt_assert_fail("ufbx_assert", __FILE__, __LINE__, #m_cond); } while (0)

#include "../ufbx.h"
#include "check_scene.h"

int main(int argc, char **argv)
{
	ufbxt_assert(argc == 2);
	const char *path = argv[1];

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
