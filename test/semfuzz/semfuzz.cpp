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

#include "decode_file.h"
#include "../../ufbx.h"
#include "../../test/check_scene.h"
#include "../../test/hash_scene.h"

#if defined(AFL)
	#include <unistd.h>
#else
	#include <filesystem>
	namespace fs = std::filesystem;
#endif

char g_src[1024*1024];
char g_dst[4*1024*1024];
semfuzz::Field g_fields[128*1024];
semfuzz::Value g_values[128*1024];

static void init_opts(ufbx_load_opts &opts, const semfuzz::File &file)
{
#if 0
	if (file.temp_limit > 0) {
		opts.temp_allocator.allocation_limit = file.temp_limit;
		opts.temp_allocator.huge_threshold = 1;
	}
	if (file.result_limit > 0) {
		opts.result_allocator.allocation_limit = file.result_limit;
		opts.result_allocator.huge_threshold = 1;
	}
#endif

#if defined(ASAN)
	opts.temp_allocator.huge_threshold = 1;
	opts.result_allocator.huge_threshold = 1;
#endif

	opts.file_format = UFBX_FILE_FORMAT_FBX;
	opts.result_allocator.memory_limit = UINT64_C(4000000000);
	opts.temp_allocator.memory_limit = UINT64_C(4000000000);

	uint32_t flags = file.flags;
	bool lefthanded = false;
	bool z_up = false;

    if ((flags & 0x00000003u) == 0x00000001u) opts.space_conversion = UFBX_SPACE_CONVERSION_TRANSFORM_ROOT;
    if ((flags & 0x00000003u) == 0x00000002u) opts.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
    if ((flags & 0x00000003u) == 0x00000003u) opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    if ((flags & 0x0000000cu) == 0x00000004u) opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
    if ((flags & 0x0000000cu) == 0x00000008u) opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
    if ((flags & 0x0000000cu) == 0x0000000cu) opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY_NO_FALLBACK;
    if ((flags & 0x00000030u) == 0x00000010u) opts.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
    if ((flags & 0x00000030u) == 0x00000020u) opts.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_COMPENSATE;
    if ((flags & 0x00000030u) == 0x00000030u) opts.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_IGNORE;
    if ((flags & 0x000000c0u) == 0x00000040u) opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_X;
    if ((flags & 0x000000c0u) == 0x00000080u) opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Y;
    if ((flags & 0x000000c0u) == 0x000000c0u) opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
    if ((flags & 0x00000100u) == 0x00000100u) opts.ignore_geometry = true;
    if ((flags & 0x00000200u) == 0x00000200u) opts.ignore_animation = true;
    if ((flags & 0x00000400u) == 0x00000400u) opts.ignore_embedded = true;
    if ((flags & 0x00000800u) == 0x00000800u) opts.disable_quirks = true;
    if ((flags & 0x00001000u) == 0x00001000u) opts.strict = true;
    if ((flags & 0x00002000u) == 0x00002000u) opts.connect_broken_elements = true;
    if ((flags & 0x00004000u) == 0x00004000u) opts.allow_nodes_out_of_root = true;
    if ((flags & 0x00008000u) == 0x00008000u) opts.allow_missing_vertex_position = true;
    if ((flags & 0x00010000u) == 0x00010000u) opts.allow_empty_faces = true;
    if ((flags & 0x00020000u) == 0x00020000u) opts.generate_missing_normals = true;
    if ((flags & 0x00040000u) == 0x00040000u) opts.reverse_winding = true;
    if ((flags & 0x00080000u) == 0x00080000u) opts.normalize_normals = true;
    if ((flags & 0x00100000u) == 0x00100000u) opts.normalize_tangents = true;
    if ((flags & 0x00200000u) == 0x00200000u) opts.retain_dom = true;
	if ((flags & 0x00c00000u) == 0x00400000u) opts.index_error_handling = UFBX_INDEX_ERROR_HANDLING_NO_INDEX;
    if ((flags & 0x00c00000u) == 0x00800000u) opts.index_error_handling = UFBX_INDEX_ERROR_HANDLING_ABORT_LOADING;
    if ((flags & 0x10000000u) == 0x10000000u) lefthanded = true;
    if ((flags & 0x20000000u) == 0x20000000u) z_up = true;

	opts.target_unit_meters = 1.0f;
	if (lefthanded) {
		opts.target_axes = z_up ? ufbx_axes_left_handed_z_up : ufbx_axes_left_handed_y_up;
	} else {
		opts.target_axes = z_up ? ufbx_axes_right_handed_z_up : ufbx_axes_right_handed_y_up;
	}
}

void load_file(const semfuzz::File &file, int index)
{
	size_t dst_size = semfuzz::write_ascii(g_dst, sizeof(g_dst), file);
	if (dst_size == 0) return;

	ufbx_load_opts opts = { };
	init_opts(opts, file);

	ufbx_error error;
	ufbx_scene *scene = ufbx_load_memory(g_dst, dst_size, &opts, &error);

	if (scene) {
		ufbxt_check_scene(scene);

		{
			ufbx_evaluate_opts eval_opts = { };
			eval_opts.evaluate_skinning = true;
			ufbx_scene *state = ufbx_evaluate_scene(scene, scene->anim, 0.1, &eval_opts, &error);
			ufbxt_assert(state);
			ufbxt_check_scene(state);
			ufbx_free_scene(state);
		}

		{
			ufbx_baked_anim *bake = ufbx_bake_anim(scene, scene->anim, NULL, NULL);
			ufbxt_assert(bake);
			ufbx_free_baked_anim(bake);
		}

		{
			FILE *hash_a = NULL, *hash_b = NULL;
			if (index == -1) {
				hash_a = fopen("hash_a.txt", "w");
				hash_b = fopen("hash_b.txt", "w");
			}

			uint64_t scene_hash = ufbxt_hash_scene(scene, hash_a);
			ufbx_scene *scene_alt = ufbx_load_memory(g_dst, dst_size, &opts, &error);
			ufbxt_assert(scene_alt);

			uint64_t alt_hash = ufbxt_hash_scene(scene_alt, hash_b);

			if (hash_a) fclose(hash_a);
			if (hash_b) fclose(hash_b);

			ufbxt_assert(scene_hash == alt_hash);

			ufbx_free_scene(scene_alt);
		}
	}

	ufbx_free_scene(scene);
}

int main(int argc, char **argv)
{
	semfuzz::File file;
	file.fields = g_fields;
	file.max_fields = sizeof(g_fields) / sizeof(*g_fields);
	file.values = g_values;
	file.max_values = sizeof(g_values) / sizeof(*g_values);

#if defined(AFL)
	while (__AFL_LOOP(10000)) {
		size_t src_size = (size_t)read(0, g_src, sizeof(g_src));
		if (!semfuzz::read_fbb(file, g_src, src_size)) continue;
		load_file(file, 0);
	}
#else
	if (argc > 1) {
		fs::path path{argv[1]};
		std::vector<fs::path> paths;

		int target_index = -1;
		if (argc > 2) {
			target_index = atoi(argv[2]);
		}

		if (fs::is_directory(path)) {
			for (const fs::directory_entry &entry : fs::directory_iterator(path)) {
				if (!entry.is_regular_file()) continue;
				paths.push_back(entry.path());
			}
		} else {
			paths.push_back(path);
		}

		int index = 0;
		for (const fs::path &path : paths) {

			if (target_index >= 0 && index != target_index) {
				index++;
				continue;
			}

			FILE *f = nullptr;
			#if defined(_WIN32)
			{
				std::u16string str = path.u16string();
				wprintf(L"[%d] %s\n", index, (wchar_t*)str.c_str());
				_wfopen_s(&f, (wchar_t*)str.c_str(), L"rb");
			}
			#else
			{
				std::string str = path.u8string();
				printf("[%d] %s\n", index, str.c_str());
				f = fopen(path.c_str(), "rb");
			}
			#endif
			ufbxt_assert(f);

			size_t src_size = fread(g_src, 1, sizeof(g_src), f);
			fclose(f);
			if (!semfuzz::read_fbb(file, g_src, src_size)) {
				return 1;
			}

			load_file(file, index);

			index++;
		}
	}
#endif

	return 0;
}
