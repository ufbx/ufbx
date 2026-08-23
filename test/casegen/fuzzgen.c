#include "ufbx_write.h"

#define IM_ARG_IMPLEMENTATION
#include "../im_arg.h"

#define UFBXW_GEOMETRY_IMPLEMENTATION
#include "ufbxw_geometry.h"

#include <stdio.h>

#define arraycount(arr) (sizeof((arr)) / sizeof(*(arr)))

static ufbxw_vec3 vec3(ufbxw_real x, ufbxw_real y, ufbxw_real z)
{
	ufbxw_vec3 v = { x, y, z };
	return v;
}

void case_empty_file(ufbxw_scene *scene)
{
}

void case_scene_info(ufbxw_scene *scene)
{
	ufbxw_id id = ufbxw_create_element(scene, UFBXW_ELEMENT_SCENE_INFO);
	ufbxw_set_name(scene, id, "GlobalInfo");

	ufbxw_save_info info = { 0 };
	info.document_url = ufbxw_str("/home/user/project/scene.fbx");
	info.application_vendor = ufbxw_str("ufbx");
	info.application_name = ufbxw_str("fuzzgen");
	info.application_version = ufbxw_str("0.1");
	info.src_document_url = ufbxw_str("/home/user/project/source.fbx");
	info.original_filename = ufbxw_str("/home/user/project/original.fbx");
	info.original_application_vendor = ufbxw_str("original_vendor");
	info.original_application_name = ufbxw_str("original_app");
	info.original_application_version = ufbxw_str("1.0");
	info.no_default_date_time = true;
	info.date_time_utc = (ufbxw_datetime){ 2020, 1, 2, 3, 4, 5, 6 };
	info.original_date_time_utc = (ufbxw_datetime){ 2019, 6, 7, 8, 9, 10, 11 };

	ufbxw_set_save_info(scene, &info);
}

void case_global_settings(ufbxw_scene *scene)
{
}

void case_node(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_node_set_translation(scene, node, vec3(1.0, 2.0, 3.0));
	ufbxw_node_set_rotation(scene, node, vec3(90.0, 0.0, 0.0));
	ufbxw_node_set_scaling(scene, node, vec3(1.0, 1.5, 2.0));
}

typedef void case_create_fn(ufbxw_scene *scene);

typedef enum {
	CASE_ANIMATION = 0x1,
	CASE_GLOBAL_SETTINGS = 0x2,
} case_flags;

typedef struct {
	const char *name;
	case_create_fn *create_fn;
	case_flags flags;
} case_desc;

typedef struct {
	const char *output_path;
	ufbxw_save_format format;
	uint32_t version;
} gen_settings;

case_desc cases[] = {
	{ "empty_file", &case_empty_file, 0 },
	{ "scene_info", &case_scene_info, 0 },
	{ "global_settings", &case_global_settings, CASE_GLOBAL_SETTINGS },
	{ "node", &case_node, 0 },
};

void generate_case(const case_desc *desc, const gen_settings *settings)
{
	ufbxw_scene_opts scene_opts = { 0 };
	scene_opts.no_default_scene_info = true;
	if ((desc->flags & CASE_GLOBAL_SETTINGS) == 0) {
		scene_opts.no_default_global_settings = true;
	}
	if ((desc->flags & CASE_ANIMATION) == 0) {
		scene_opts.no_default_anim_layer = true;
		scene_opts.no_default_anim_stack = true;
	}

	ufbxw_scene *scene = ufbxw_create_scene(&scene_opts);

	ufbxw_prepare_opts prepare = ufbxw_default_prepare_opts;

	desc->create_fn(scene, &prepare);

	ufbxw_prepare_scene(scene, &prepare);

	const char *format_name = "";
	switch (settings->format) {
	case UFBXW_SAVE_FORMAT_ASCII:
		format_name = "ascii";
		break;
	case UFBXW_SAVE_FORMAT_BINARY:
		format_name = "binary";
		break;
	}

	char filename[256];
	snprintf(filename, sizeof(filename), "casegen_%s_%u_%s.fbx", desc->name, settings->version, format_name);

	char path[256];
	snprintf(path, sizeof(path), "%s/%s", settings->output_path, filename);

	ufbxw_save_opts opts = { 0 };
	opts.format = settings->format;
	opts.version = settings->version;
	opts.local_timestamp.year = 1970;
	opts.local_timestamp.month = 1;
	opts.local_timestamp.day = 1;
	opts.no_default_timestamp = true;

	ufbxw_error error;
	bool ok = ufbxw_save_file(scene, path, &opts, &error);
	if (!ok) {
		fprintf(stderr, "failed to save scene: %s\n", error.description);
		exit(1);
	}

	ufbxw_free_scene(scene);
}

int main(int argc, char **argv)
{
	const char *case_name = NULL;
	gen_settings settings = { 0 };

	im_arg_begin_c(argc, argv);
	while (im_arg_next()) {
		im_arg_help("--help", "Show this help");

		if (im_arg("-o path", "Output path")) {
			settings.output_path = im_arg_str(0);
		}
	}

	ufbxw_save_format formats[] = {
		UFBXW_SAVE_FORMAT_ASCII,
		UFBXW_SAVE_FORMAT_BINARY,
	};

	settings.version = 7400;

	for (uint32_t case_ix = 0; case_ix < arraycount(cases); case_ix++) {
		for (uint32_t format_ix = 0; format_ix < 2; format_ix++) {
			settings.format = formats[format_ix];
			generate_case(&cases[case_ix], &settings);
		}
	}

	return 0;
}
