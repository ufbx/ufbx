#include "ufbx_write.h"

#define IM_ARG_IMPLEMENTATION
#include "../im_arg.h"

#define UFBXW_GEOMETRY_IMPLEMENTATION
#include "ufbxw_geometry.h"

#include <stdio.h>

#define arraycount(arr) (sizeof((arr)) / sizeof(*(arr)))

void make_arrow(ufbxw_scene *scene, ufbxw_mesh mesh, ufbxw_real scale)
{
	ufbxw_real h_tip = 1.5f * scale;
	ufbxw_real h_base = 1.5f * scale;
	ufbxw_real w_tip_x = 1.5f * scale;
	ufbxw_real w_tip_y = 1.0f * scale;
	ufbxw_real w_base = 0.5f * scale;

	ufbxw_vec3 positions[] = {
		{ 0.0f, 0.0f, -h_base - h_tip },

		{ w_tip_x, 0.0f, -h_base },
		{ 0.0f, w_tip_y, -h_base },

		{ w_base, 0.0f, -h_base },
		{ 0.0f, w_base, -h_base },

		{ w_base, 0.0f, 0.0f },
		{ 0.0f, w_base, 0.0f },

		{ 0.0f, 0.0f, 0.0f },
	};

	int32_t indices[] = {
		7, 0, 1, 3, 5,
		7, 6, 4, 2, 0,
		0, 2, 1,
		1, 2, 4, 3,
		3, 4, 6, 5,
		7, 5, 6,
	};

	int32_t face_offsets[] = {
		0, 5, 10, 13, 17, 21, 24,
	};

	// Center the arrow
	for (size_t i = 0; i < arraycount(positions); i++) {
		positions[i].z += (h_base + h_tip) * 0.5f;
	}

	ufbxw_mesh_set_vertices(scene, mesh,
		ufbxw_view_vec3_array(scene, positions, arraycount(positions)));

	ufbxw_mesh_set_polygons(scene, mesh,
		ufbxw_view_int_array(scene, indices, arraycount(indices)),
		ufbxw_view_int_array(scene, face_offsets, arraycount(face_offsets)));

	ufbxw_generate_flat_normals(scene, mesh);
}

void make_cube(ufbxw_scene *scene, ufbxw_mesh mesh, ufbxw_real scale)
{
	ufbxw_real w = 1.0f * scale;

	ufbxw_vec3 positions[] = {
		{ -w, -w, -w },
		{  w, -w, -w },
		{  w,  w, -w },
		{ -w,  w, -w },
		{ -w, -w,  w },
		{  w, -w,  w },
		{  w,  w,  w },
		{ -w,  w,  w },
	};

	int32_t indices[] = {
		0, 3, 2, 1,
		4, 5, 6, 7,
		0, 1, 5, 4,
		1, 2, 6, 5,
		2, 3, 7, 6,
		3, 0, 4, 7,
	};

	int32_t face_offsets[] = {
		0, 4, 8, 12, 16, 20, 24,
	};

	ufbxw_mesh_set_vertices(scene, mesh,
		ufbxw_view_vec3_array(scene, positions, arraycount(positions)));

	ufbxw_mesh_set_polygons(scene, mesh,
		ufbxw_view_int_array(scene, indices, arraycount(indices)),
		ufbxw_view_int_array(scene, face_offsets, arraycount(face_offsets)));

	ufbxw_generate_flat_normals(scene, mesh);
}

void case_rotation_order(ufbxw_scene* scene)
{
	ufbxw_mesh arrow_mesh = ufbxw_create_mesh(scene);
	ufbxw_set_name(scene, arrow_mesh.id, "Arrow");
	make_arrow(scene, arrow_mesh, 1.0f);

	typedef struct {
		ufbxw_vec3 rotation;
	} params_t;

	typedef struct {
		ufbxw_rotation_order order;
		bool rotation_active;
	} config_t;

	params_t params[] = {
		{ { 0.0, 0.0, 0.0 } },
		{ { 45.0, 0.0, 0.0 } },
		{ { 0.0, 45.0, 0.0 } },
		{ { 0.0, 0.0, 45.0 } },
		{ { 90.0, 45.0, 0.0 } },
		{ { 90.0, 0.0, 45.0 } },
		{ { 45.0, 90.0, 0.0 } },
		{ { 0.0, 90.0, 45.0 } },
		{ { 45.0, 0.0, 90.0 } },
		{ { 0.0, 45.0, 90.0 } },
		{ { 30.0, 60.0, 90.0 } },
		{ { 45.0, -45.0, 90.0 } },
		{ { 90.0, 90.0, 90.0 } },
	};

	config_t configs[] = {
		{ UFBXW_ROTATION_ORDER_XYZ, true },
		{ UFBXW_ROTATION_ORDER_XZY, true },
		{ UFBXW_ROTATION_ORDER_YZX, true },
		{ UFBXW_ROTATION_ORDER_YXZ, true },
		{ UFBXW_ROTATION_ORDER_ZXY, true },
		{ UFBXW_ROTATION_ORDER_ZYX, true },
		{ UFBXW_ROTATION_ORDER_XYZ, false },
		{ UFBXW_ROTATION_ORDER_XZY, false },
		{ UFBXW_ROTATION_ORDER_YZX, false },
		{ UFBXW_ROTATION_ORDER_YXZ, false },
		{ UFBXW_ROTATION_ORDER_ZXY, false },
		{ UFBXW_ROTATION_ORDER_ZYX, false },
	};

	for (uint32_t config_ix = 0; config_ix < arraycount(configs); config_ix++) {
		for (uint32_t param_ix = 0; param_ix < arraycount(params); param_ix++) {
			config_t config = configs[config_ix];
			params_t param = params[param_ix];

			char name[256];
			snprintf(name, sizeof(name), "Node_%02u_%02u", config_ix, param_ix);

			ufbxw_node node = ufbxw_create_node(scene);
			ufbxw_set_name(scene, node.id, name);

			ufbxw_vec3 position;
			position.x = (ufbxw_real)param_ix * 4.0f;
			position.y = 0.0f;
			position.z = (ufbxw_real)config_ix * 4.0f;
			ufbxw_node_set_translation(scene, node, position);

			ufbxw_set_bool(scene, node.id, "RotationActive", config.rotation_active);
			ufbxw_node_set_rotation(scene, node, param.rotation);
			ufbxw_node_set_rotation_order(scene, node, config.order);

			ufbxw_mesh_add_instance(scene, arrow_mesh, node);
		}
	}
}

void case_rotation_space(ufbxw_scene* scene)
{
	ufbxw_mesh arrow_mesh = ufbxw_create_mesh(scene);
	ufbxw_set_name(scene, arrow_mesh.id, "Arrow");
	make_arrow(scene, arrow_mesh, 1.0f);

	typedef struct {
		ufbxw_vec3 rotation;
		ufbxw_vec3 pre_rotation;
		ufbxw_vec3 post_rotation;
	} params_t;

	typedef struct {
		ufbxw_rotation_order order;
		bool rotation_active;
		bool rotation_space_for_limit_only;
	} config_t;

	params_t params[] = {
		{ { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 } },
		{ { 30.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 } },
		{ { 0.0, 0.0, 0.0 }, { 30.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 } },
		{ { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, { 30.0, 0.0, 0.0 } },
		{ { 30.0, 60.0, 90.0 }, { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 } },
		{ { 0.0, 0.0, 0.0 }, { 30.0, 60.0, 90.0 }, { 0.0, 0.0, 0.0 } },
		{ { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, { 30.0, 60.0, 90.0 } },
		{ { 30.0, 60.0, 90.0 }, { -30.0, -60.0, -90.0 }, { 0.0, 0.0, 0.0 } },
		{ { 30.0, 60.0, 90.0 }, { 0.0, 0.0, 0.0 }, { -30.0, -60.0, -90.0 } },
		{ { 30.0, 60.0, 90.0 }, { 30.0, 60.0, 90.0 }, { 30.0, 60.0, 90.0 } },
	};

	config_t configs[] = {
		{ UFBXW_ROTATION_ORDER_XYZ, false, false },
		{ UFBXW_ROTATION_ORDER_XZY, false, false },
		{ UFBXW_ROTATION_ORDER_ZYX, false, false },
		{ UFBXW_ROTATION_ORDER_XYZ, true, false },
		{ UFBXW_ROTATION_ORDER_XZY, true, false },
		{ UFBXW_ROTATION_ORDER_ZYX, true, false },
		{ UFBXW_ROTATION_ORDER_XYZ, false, true },
		{ UFBXW_ROTATION_ORDER_XZY, false, true },
		{ UFBXW_ROTATION_ORDER_ZYX, false, true },
		{ UFBXW_ROTATION_ORDER_XYZ, true, true },
		{ UFBXW_ROTATION_ORDER_XZY, true, true },
		{ UFBXW_ROTATION_ORDER_ZYX, true, true },
	};

	for (uint32_t config_ix = 0; config_ix < arraycount(configs); config_ix++) {
		for (uint32_t param_ix = 0; param_ix < arraycount(params); param_ix++) {
			config_t config = configs[config_ix];
			params_t param = params[param_ix];

			char name[256];
			snprintf(name, sizeof(name), "Node_%02u_%02u", config_ix, param_ix);

			ufbxw_node node = ufbxw_create_node(scene);
			ufbxw_set_name(scene, node.id, name);

			ufbxw_vec3 position;
			position.x = (ufbxw_real)param_ix * 4.0f;
			position.y = 0.0f;
			position.z = (ufbxw_real)config_ix * 4.0f;
			ufbxw_node_set_translation(scene, node, position);

			ufbxw_node_set_rotation(scene, node, param.rotation);
			ufbxw_node_set_pre_rotation(scene, node, param.pre_rotation);
			ufbxw_node_set_post_rotation(scene, node, param.post_rotation);

			ufbxw_node_set_rotation_order(scene, node, config.order);
			ufbxw_set_bool(scene, node.id, "RotationActive", config.rotation_active);
			ufbxw_set_bool(scene, node.id, "RotationSpaceForLimitOnly", config.rotation_space_for_limit_only);

			ufbxw_mesh_add_instance(scene, arrow_mesh, node);
		}
	}
}

void case_tangent_auto_linear_constant(ufbxw_scene* scene)
{
	ufbxw_mesh cube_mesh = ufbxw_create_mesh(scene);
	ufbxw_set_name(scene, cube_mesh.id, "Cube");
	make_cube(scene, cube_mesh, 1.0f);

	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Cube");
	ufbxw_mesh_add_instance(scene, cube_mesh, node);

	ufbxw_anim_layer layer = ufbxw_get_default_anim_layer(scene);
	ufbxw_anim_prop anim_prop = ufbxw_animate_prop(scene, node.id, "Lcl Translation", layer);

	ufbxw_anim_curve curve_x = ufbxw_anim_get_curve(scene, anim_prop, 0);
	ufbxw_anim_curve curve_z = ufbxw_anim_get_curve(scene, anim_prop, 2);

	ufbxw_keyframe_real keys_x[] = {
		{ 0 * UFBXW_KTIME_SECOND, 0.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 1 * UFBXW_KTIME_SECOND, 1.0f, UFBXW_KEYFRAME_INTERPOLATION_LINEAR },
		{ 2 * UFBXW_KTIME_SECOND, 2.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 3 * UFBXW_KTIME_SECOND, 3.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 4 * UFBXW_KTIME_SECOND, 2.0f, UFBXW_KEYFRAME_INTERPOLATION_LINEAR },
		{ 5 * UFBXW_KTIME_SECOND, 3.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 6 * UFBXW_KTIME_SECOND, 4.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 7 * UFBXW_KTIME_SECOND, 20.0f, UFBXW_KEYFRAME_INTERPOLATION_LINEAR },
		{ 8 * UFBXW_KTIME_SECOND, 22.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 9 * UFBXW_KTIME_SECOND, 50.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
	};

	ufbxw_keyframe_real keys_z[] = {
		{ 0 * UFBXW_KTIME_SECOND, 0.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 1 * UFBXW_KTIME_SECOND, 1.0f, UFBXW_KEYFRAME_INTERPOLATION_CONSTANT },
		{ 2 * UFBXW_KTIME_SECOND, 2.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 3 * UFBXW_KTIME_SECOND, 3.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 4 * UFBXW_KTIME_SECOND, 2.0f, UFBXW_KEYFRAME_INTERPOLATION_CONSTANT },
		{ 5 * UFBXW_KTIME_SECOND, 3.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 6 * UFBXW_KTIME_SECOND, 4.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 7 * UFBXW_KTIME_SECOND, 20.0f, UFBXW_KEYFRAME_INTERPOLATION_CONSTANT },
		{ 8 * UFBXW_KTIME_SECOND, 22.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
		{ 9 * UFBXW_KTIME_SECOND, 50.0f, UFBXW_KEYFRAME_INTERPOLATION_CUBIC | UFBXW_KEYFRAME_TANGENT_AUTO },
	};
	
	for (size_t i = 0; i < arraycount(keys_x); i++) {
		ufbxw_anim_curve_add_keyframe_key(scene, curve_x, keys_x[i]);
	}
	
	for (size_t i = 0; i < arraycount(keys_z); i++) {
		ufbxw_anim_curve_add_keyframe_key(scene, curve_z, keys_z[i]);
	}
}

typedef void case_create_fn(ufbxw_scene *scene);

typedef struct {
	const char *name;
	case_create_fn *create_fn;
} case_desc;

typedef struct {
	const char *output_path;
	ufbxw_save_format format;
	uint32_t version;
} gen_settings;

case_desc cases[] = {
	{ "rotation_order", &case_rotation_order },
	{ "rotation_space", &case_rotation_space },
	{ "tangent_auto_linear_constant", &case_tangent_auto_linear_constant },
};

void generate_case(const case_desc *desc, const gen_settings *settings)
{
	ufbxw_scene *scene = ufbxw_create_scene(NULL);

	desc->create_fn(scene);

	ufbxw_save_info save_info = { 0 };
	save_info.application_name = ufbxw_str("casegen");
	save_info.application_vendor = ufbxw_str("ufbx");
	save_info.application_version = ufbxw_str("0.1");
	ufbxw_set_save_info(scene, &save_info);

	ufbxw_prepare_scene(scene, NULL);

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
	settings.output_path = "data";

	im_arg_begin_c(argc, argv);
	while (im_arg_next()) {
		im_arg_help("--help", "Show this help");

		if (im_arg("case", "Name of the case to generate")) {
			case_name = im_arg_str(0);
		}
	}

	if (!case_name) {
		fprintf(stderr, "No case specified\n");
		exit(1);
	}

	ufbxw_save_format formats[] = {
		UFBXW_SAVE_FORMAT_ASCII,
		UFBXW_SAVE_FORMAT_BINARY,
	};

	settings.version = 7500;

	for (uint32_t case_ix = 0; case_ix < arraycount(cases); case_ix++) {
		if (strcmp(cases[case_ix].name, case_name) != 0) {
			continue;
		}

		for (uint32_t format_ix = 0; format_ix < 2; format_ix++) {
			settings.format = formats[format_ix];
			generate_case(&cases[case_ix], &settings);
		}
	}

	return 0;
}
