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

void case_light(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_light light = ufbxw_create_light(scene, node);
	ufbxw_set_name(scene, light.id, "Light");
	ufbxw_light_set_color(scene, light, vec3(1.0, 0.8, 0.6));
	ufbxw_light_set_intensity(scene, light, 50.0);
	ufbxw_light_set_type(scene, light, UFBXW_LIGHT_POINT);
	ufbxw_light_set_decay(scene, light, UFBXW_LIGHT_DECAY_QUADRATIC);
}

void case_camera(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_camera camera = ufbxw_create_camera(scene, node);
	ufbxw_set_name(scene, camera.id, "Camera");
}

void case_bone(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_bone bone = ufbxw_create_bone(scene, UFBXW_BONE_LIMB_NODE, node);
	ufbxw_set_name(scene, bone.id, "Bone");
}

void case_mesh(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_mesh mesh = ufbxw_create_mesh(scene);
	ufbxw_set_name(scene, mesh.id, "Plane");

	ufbxw_vec3 vertices[] = {
		{ -1.0, -1.0, 0.0 },
		{  1.0, -1.0, 0.0 },
		{  1.0,  1.0, 0.0 },
		{ -1.0,  1.0, 0.0 },
	};
	ufbxw_vec3 normals[] = {
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
	};
	int32_t indices[] = { 0, 1, 2, 3 };
	int32_t face_offsets[] = { 0, 4 };

	ufbxw_mesh_set_vertices(scene, mesh,
		ufbxw_view_vec3_array(scene, vertices, arraycount(vertices)));
	ufbxw_mesh_set_polygons(scene, mesh,
		ufbxw_view_int_array(scene, indices, arraycount(indices)),
		ufbxw_view_int_array(scene, face_offsets, arraycount(face_offsets)));
	ufbxw_mesh_set_normals(scene, mesh,
		ufbxw_view_vec3_array(scene, normals, arraycount(normals)),
		UFBXW_ATTRIBUTE_MAPPING_VERTEX);
	ufbxw_node_set_attribute(scene, node, mesh.id);
}

void case_uv_sets(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_mesh mesh = ufbxw_create_mesh(scene);
	ufbxw_set_name(scene, mesh.id, "Plane");

	ufbxw_vec3 vertices[] = {
		{ -1.0, -1.0, 0.0 },
		{  1.0, -1.0, 0.0 },
		{  1.0,  1.0, 0.0 },
		{ -1.0,  1.0, 0.0 },
	};
	ufbxw_vec3 normals[] = {
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
	};
	ufbxw_vec2 uvs0[] = {
		{ 0.0, 0.0 },
		{ 1.0, 0.0 },
		{ 1.0, 1.0 },
		{ 0.0, 1.0 },
	};
	ufbxw_vec2 uvs1[] = {
		{ 0.0, 0.0 },
		{ 2.0, 0.0 },
		{ 2.0, 2.0 },
		{ 0.0, 2.0 },
	};
	int32_t indices[] = { 0, 1, 2, 3 };
	int32_t face_offsets[] = { 0, 4 };

	ufbxw_mesh_set_vertices(scene, mesh,
		ufbxw_view_vec3_array(scene, vertices, arraycount(vertices)));
	ufbxw_mesh_set_polygons(scene, mesh,
		ufbxw_view_int_array(scene, indices, arraycount(indices)),
		ufbxw_view_int_array(scene, face_offsets, arraycount(face_offsets)));
	ufbxw_mesh_set_normals(scene, mesh,
		ufbxw_view_vec3_array(scene, normals, arraycount(normals)),
		UFBXW_ATTRIBUTE_MAPPING_VERTEX);
	ufbxw_mesh_set_uvs(scene, mesh, 0,
		ufbxw_view_vec2_array(scene, uvs0, arraycount(uvs0)),
		UFBXW_ATTRIBUTE_MAPPING_VERTEX);
	ufbxw_mesh_set_uvs(scene, mesh, 1,
		ufbxw_view_vec2_array(scene, uvs1, arraycount(uvs1)),
		UFBXW_ATTRIBUTE_MAPPING_VERTEX);
	ufbxw_node_set_attribute(scene, node, mesh.id);
}

void case_color_sets(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_mesh mesh = ufbxw_create_mesh(scene);
	ufbxw_set_name(scene, mesh.id, "Plane");

	ufbxw_vec3 vertices[] = {
		{ -1.0, -1.0, 0.0 },
		{  1.0, -1.0, 0.0 },
		{  1.0,  1.0, 0.0 },
		{ -1.0,  1.0, 0.0 },
	};
	ufbxw_vec3 normals[] = {
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0 },
	};
	ufbxw_vec4 colors0[] = {
		{ 1.0, 0.0, 0.0, 1.0 },
		{ 0.0, 1.0, 0.0, 1.0 },
		{ 0.0, 0.0, 1.0, 1.0 },
		{ 1.0, 1.0, 0.0, 1.0 },
	};
	ufbxw_vec4 colors1[] = {
		{ 0.0, 1.0, 1.0, 1.0 },
		{ 1.0, 0.0, 1.0, 1.0 },
		{ 1.0, 1.0, 1.0, 1.0 },
		{ 0.0, 0.0, 0.0, 1.0 },
	};
	int32_t indices[] = { 0, 1, 2, 3 };
	int32_t face_offsets[] = { 0, 4 };

	ufbxw_mesh_set_vertices(scene, mesh,
		ufbxw_view_vec3_array(scene, vertices, arraycount(vertices)));
	ufbxw_mesh_set_polygons(scene, mesh,
		ufbxw_view_int_array(scene, indices, arraycount(indices)),
		ufbxw_view_int_array(scene, face_offsets, arraycount(face_offsets)));
	ufbxw_mesh_set_normals(scene, mesh,
		ufbxw_view_vec3_array(scene, normals, arraycount(normals)),
		UFBXW_ATTRIBUTE_MAPPING_VERTEX);
	ufbxw_mesh_set_colors(scene, mesh, 0,
		ufbxw_view_vec4_array(scene, colors0, arraycount(colors0)),
		UFBXW_ATTRIBUTE_MAPPING_VERTEX);
	ufbxw_mesh_set_colors(scene, mesh, 1,
		ufbxw_view_vec4_array(scene, colors1, arraycount(colors1)),
		UFBXW_ATTRIBUTE_MAPPING_VERTEX);
	ufbxw_node_set_attribute(scene, node, mesh.id);
}

void case_deflate_array(ufbxw_scene *scene)
{
	ufbxw_mesh mesh = ufbxw_create_mesh(scene);
	ufbxw_set_name(scene, mesh.id, "Mesh");

	ufbxw_vec3 vertices[32];
	for (size_t i = 0; i < arraycount(vertices); i++) {
		vertices[i] = vec3(
			(ufbxw_real)(i * 3 + 1) * 0.001,
			(ufbxw_real)(i * 3 + 2) * 0.001,
			(ufbxw_real)(i * 3 + 3) * 0.001);
	}
	int32_t indices[32];
	for (size_t i = 0; i < arraycount(indices); i++) {
		indices[i] = (int32_t)i;
	}
	int32_t face_offsets[] = { 0, 32 };

	ufbxw_mesh_set_vertices(scene, mesh,
		ufbxw_view_vec3_array(scene, vertices, arraycount(vertices)));
	ufbxw_mesh_set_polygons(scene, mesh,
		ufbxw_view_int_array(scene, indices, arraycount(indices)),
		ufbxw_view_int_array(scene, face_offsets, arraycount(face_offsets)));
}

void case_animation(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_anim_layer layer = ufbxw_get_default_anim_layer(scene);
	ufbxw_anim_prop visibility = ufbxw_animate_prop(scene, node.id, "Visibility", layer);
	ufbxw_anim_set_default_value(scene, visibility, 0, 1.0);
	ufbxw_anim_add_keyframe_real(scene, visibility, 0 * UFBXW_KTIME_SECOND,
		1.0, UFBXW_KEYFRAME_CUBIC_AUTO);
	ufbxw_anim_add_keyframe_real(scene, visibility, 1 * UFBXW_KTIME_SECOND,
		0.0, UFBXW_KEYFRAME_CUBIC_AUTO);
	ufbxw_anim_add_keyframe_real(scene, visibility, 2 * UFBXW_KTIME_SECOND,
		1.0, UFBXW_KEYFRAME_CUBIC_AUTO);
}

void case_material(ufbxw_scene *scene)
{
	ufbxw_node node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, node.id, "Node");

	ufbxw_material material = ufbxw_create_material(scene, UFBXW_MATERIAL_FBX_LAMBERT);
	ufbxw_set_name(scene, material.id, "Material");
	ufbxw_set_vec3(scene, material.id, "DiffuseColor", vec3(0.8, 0.4, 0.2));
	ufbxw_set_real(scene, material.id, "DiffuseFactor", 1.0);
	ufbxw_node_set_material(scene, node, 0, material);
}

void case_texture(ufbxw_scene *scene)
{
	ufbxw_texture texture = ufbxw_create_texture(scene, UFBXW_TEXTURE_FILE);
	ufbxw_set_name(scene, texture.id, "Texture");
	ufbxw_texture_set_filename(scene, texture, "/home/user/project/textures/diffuse.png");
	ufbxw_texture_set_relative_filename(scene, texture, "textures/diffuse.png");
}

void case_texture_video(ufbxw_scene *scene)
{
	ufbxw_texture texture = ufbxw_create_texture(scene, UFBXW_TEXTURE_FILE);
	ufbxw_set_name(scene, texture.id, "Texture");
	ufbxw_texture_set_filename(scene, texture, "/home/user/project/textures/diffuse.png");
	ufbxw_texture_set_relative_filename(scene, texture, "textures/diffuse.png");

	ufbxw_video video = ufbxw_create_video(scene);
	ufbxw_set_name(scene, video.id, "Video");
	ufbxw_video_set_filename(scene, video, "/home/user/project/videos/diffuse.mp4");
	ufbxw_video_set_relative_filename(scene, video, "videos/diffuse.mp4");
	ufbxw_texture_set_video(scene, texture, video);
}

void case_skinning(ufbxw_scene *scene)
{
	ufbxw_node mesh_node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, mesh_node.id, "Node");

	ufbxw_mesh mesh = ufbxw_create_mesh(scene);
	ufbxw_set_name(scene, mesh.id, "Plane");

	ufbxw_vec3 vertices[] = {
		{ -1.0, -1.0, 0.0 },
		{  1.0, -1.0, 0.0 },
		{  1.0,  1.0, 0.0 },
		{ -1.0,  1.0, 0.0 },
	};
	int32_t indices[] = { 0, 1, 2, 3 };
	int32_t face_offsets[] = { 0, 4 };

	ufbxw_mesh_set_vertices(scene, mesh,
		ufbxw_view_vec3_array(scene, vertices, arraycount(vertices)));
	ufbxw_mesh_set_polygons(scene, mesh,
		ufbxw_view_int_array(scene, indices, arraycount(indices)),
		ufbxw_view_int_array(scene, face_offsets, arraycount(face_offsets)));
	ufbxw_node_set_attribute(scene, mesh_node, mesh.id);

	ufbxw_node left_node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, left_node.id, "LeftBone");
	ufbxw_create_bone(scene, UFBXW_BONE_LIMB_NODE, left_node);

	ufbxw_node right_node = ufbxw_create_node(scene);
	ufbxw_set_name(scene, right_node.id, "RightBone");
	ufbxw_create_bone(scene, UFBXW_BONE_LIMB_NODE, right_node);

	ufbxw_skin_deformer skin = ufbxw_create_skin_deformer(scene, mesh);
	ufbxw_skin_deformer_set_skinning_type(scene, skin, UFBXW_SKINNING_TYPE_LINEAR);

	int32_t left_indices[] = { 0, 3 };
	ufbxw_real left_weights[] = { 1.0, 1.0 };
	ufbxw_skin_cluster left_cluster = ufbxw_create_skin_cluster(scene, skin, left_node);
	ufbxw_skin_cluster_set_weights(scene, left_cluster,
		ufbxw_view_int_array(scene, left_indices, arraycount(left_indices)),
		ufbxw_view_real_array(scene, left_weights, arraycount(left_weights)));

	int32_t right_indices[] = { 1, 2 };
	ufbxw_real right_weights[] = { 1.0, 1.0 };
	ufbxw_skin_cluster right_cluster = ufbxw_create_skin_cluster(scene, skin, right_node);
	ufbxw_skin_cluster_set_weights(scene, right_cluster,
		ufbxw_view_int_array(scene, right_indices, arraycount(right_indices)),
		ufbxw_view_real_array(scene, right_weights, arraycount(right_weights)));
}

typedef void case_create_fn(ufbxw_scene *scene);

typedef enum {
	CASE_ANIMATION = 0x1,
	CASE_GLOBAL_SETTINGS = 0x2,
	CASE_NO_MISSING_VIDEOS = 0x4,
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
	{ "light", &case_light, 0 },
	{ "camera", &case_camera, 0 },
	{ "bone", &case_bone, 0 },
	{ "mesh", &case_mesh, 0 },
	{ "uv_sets", &case_uv_sets, 0 },
	{ "color_sets", &case_color_sets, 0 },
	{ "deflate_array", &case_deflate_array, 0 },
	{ "animation", &case_animation, CASE_ANIMATION },
	{ "material", &case_material, 0 },
	{ "texture", &case_texture, CASE_NO_MISSING_VIDEOS },
	{ "texture_video", &case_texture_video, 0 },
	{ "skinning", &case_skinning, 0 },
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
	if ((desc->flags & CASE_NO_MISSING_VIDEOS) != 0) {
		prepare.add_missing_videos = false;
	}

	desc->create_fn(scene);

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
	opts.no_template_properties = true;
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
