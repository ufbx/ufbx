#ifndef UFBXT_CHECK_MATERIAL_H_INCLUDED
#define UFBXT_CHECK_MATERIAL_H_INCLUDED

#include "../ufbx.h"
#include <stdlib.h>

static const char *ufbxt_fbx_map_name(ufbx_material_fbx_map map)
{
	switch (map) {
	case UFBX_MATERIAL_FBX_DIFFUSE_FACTOR: return "diffuse_factor";
	case UFBX_MATERIAL_FBX_DIFFUSE_COLOR: return "diffuse_color";
	case UFBX_MATERIAL_FBX_SPECULAR_FACTOR: return "specular_factor";
	case UFBX_MATERIAL_FBX_SPECULAR_COLOR: return "specular_color";
	case UFBX_MATERIAL_FBX_SPECULAR_EXPONENT: return "specular_exponent";
	case UFBX_MATERIAL_FBX_REFLECTION_FACTOR: return "reflection_factor";
	case UFBX_MATERIAL_FBX_REFLECTION_COLOR: return "reflection_color";
	case UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR: return "transparency_factor";
	case UFBX_MATERIAL_FBX_TRANSPARENCY_COLOR: return "transparency_color";
	case UFBX_MATERIAL_FBX_EMISSION_FACTOR: return "emission_factor";
	case UFBX_MATERIAL_FBX_EMISSION_COLOR: return "emission_color";
	case UFBX_MATERIAL_FBX_AMBIENT_FACTOR: return "ambient_factor";
	case UFBX_MATERIAL_FBX_AMBIENT_COLOR: return "ambient_color";
	case UFBX_MATERIAL_FBX_NORMAL_MAP: return "normal_map";
	case UFBX_MATERIAL_FBX_BUMP: return "bump";
	case UFBX_MATERIAL_FBX_BUMP_FACTOR: return "bump_factor";
	case UFBX_MATERIAL_FBX_DISPLACEMENT_FACTOR: return "displacement_factor";
	case UFBX_MATERIAL_FBX_DISPLACEMENT: return "displacement";
	case UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT_FACTOR: return "vector_displacement_factor";
	case UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT: return "vector_displacement";
	}

	ufbxt_assert(0 && "Unhandled PBR map name");
	return NULL;
}

static const char *ufbxt_pbr_map_name(ufbx_material_pbr_map map)
{
	switch (map) {
	case UFBX_MATERIAL_PBR_BASE_FACTOR: return "base_factor";
	case UFBX_MATERIAL_PBR_BASE_COLOR: return "base_color";
	case UFBX_MATERIAL_PBR_ROUGHNESS: return "roughness";
	case UFBX_MATERIAL_PBR_METALNESS: return "metalness";
	case UFBX_MATERIAL_PBR_DIFFUSE_ROUGHNESS: return "diffuse_roughness";
	case UFBX_MATERIAL_PBR_SPECULAR_FACTOR: return "specular_factor";
	case UFBX_MATERIAL_PBR_SPECULAR_COLOR: return "specular_color";
	case UFBX_MATERIAL_PBR_SPECULAR_IOR: return "specular_ior";
	case UFBX_MATERIAL_PBR_SPECULAR_ANISOTROPY: return "specular_anisotropy";
	case UFBX_MATERIAL_PBR_SPECULAR_ROTATION: return "specular_rotation";
	case UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR: return "transmission_factor";
	case UFBX_MATERIAL_PBR_TRANSMISSION_COLOR: return "transmission_color";
	case UFBX_MATERIAL_PBR_TRANSMISSION_DEPTH: return "transmission_depth";
	case UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER: return "transmission_scatter";
	case UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER_ANISOTROPY: return "transmission_scatter_anisotropy";
	case UFBX_MATERIAL_PBR_TRANSMISSION_DISPERSION: return "transmission_dispersion";
	case UFBX_MATERIAL_PBR_TRANSMISSION_ROUGHNESS: return "transmission_roughness";
	case UFBX_MATERIAL_PBR_TRANSMISSION_EXTRA_ROUGHNESS: return "transmission_extra_roughness";
	case UFBX_MATERIAL_PBR_TRANSMISSION_PRIORITY: return "transmission_priority";
	case UFBX_MATERIAL_PBR_TRANSMISSION_ENABLE_IN_AOV: return "transmission_enable_in_aov";
	case UFBX_MATERIAL_PBR_SUBSURFACE_FACTOR: return "subsurface_factor";
	case UFBX_MATERIAL_PBR_SUBSURFACE_COLOR: return "subsurface_color";
	case UFBX_MATERIAL_PBR_SUBSURFACE_RADIUS: return "subsurface_radius";
	case UFBX_MATERIAL_PBR_SUBSURFACE_SCALE: return "subsurface_scale";
	case UFBX_MATERIAL_PBR_SUBSURFACE_ANISOTROPY: return "subsurface_anisotropy";
	case UFBX_MATERIAL_PBR_SUBSURFACE_TINT_COLOR: return "subsurface_tint_color";
	case UFBX_MATERIAL_PBR_SUBSURFACE_TYPE: return "subsurface_type";
	case UFBX_MATERIAL_PBR_SHEEN_FACTOR: return "sheen_factor";
	case UFBX_MATERIAL_PBR_SHEEN_COLOR: return "sheen_color";
	case UFBX_MATERIAL_PBR_SHEEN_ROUGHNESS: return "sheen_roughness";
	case UFBX_MATERIAL_PBR_COAT_FACTOR: return "coat_factor";
	case UFBX_MATERIAL_PBR_COAT_COLOR: return "coat_color";
	case UFBX_MATERIAL_PBR_COAT_ROUGHNESS: return "coat_roughness";
	case UFBX_MATERIAL_PBR_COAT_IOR: return "coat_ior";
	case UFBX_MATERIAL_PBR_COAT_ANISOTROPY: return "coat_anisotropy";
	case UFBX_MATERIAL_PBR_COAT_ROTATION: return "coat_rotation";
	case UFBX_MATERIAL_PBR_COAT_NORMAL: return "coat_normal";
	case UFBX_MATERIAL_PBR_COAT_AFFECT_BASE_COLOR: return "coat_affect_base_color";
	case UFBX_MATERIAL_PBR_COAT_AFFECT_BASE_ROUGHNESS: return "coat_affect_base_roughness";
	case UFBX_MATERIAL_PBR_THIN_FILM_THICKNESS: return "thin_film_thickness";
	case UFBX_MATERIAL_PBR_THIN_FILM_IOR: return "thin_film_ior";
	case UFBX_MATERIAL_PBR_EMISSION_FACTOR: return "emission_factor";
	case UFBX_MATERIAL_PBR_EMISSION_COLOR: return "emission_color";
	case UFBX_MATERIAL_PBR_OPACITY: return "opacity";
	case UFBX_MATERIAL_PBR_INDIRECT_DIFFUSE: return "indirect_diffuse";
	case UFBX_MATERIAL_PBR_INDIRECT_SPECULAR: return "indirect_specular";
	case UFBX_MATERIAL_PBR_NORMAL_MAP: return "normal_map";
	case UFBX_MATERIAL_PBR_TANGENT_MAP: return "tangent_map";
	case UFBX_MATERIAL_PBR_DISPLACEMENT_MAP: return "displacement_map";
	case UFBX_MATERIAL_PBR_MATTE_FACTOR: return "matte_factor";
	case UFBX_MATERIAL_PBR_MATTE_COLOR: return "matte_color";
	case UFBX_MATERIAL_PBR_AMBIENT_OCCLUSION: return "ambient_occlusion";
	case UFBX_MATERIAL_PBR_GLOSSINESS: return "glossiness";
	case UFBX_MATERIAL_PBR_COAT_GLOSSINESS: return "coat_glossiness";
	case UFBX_MATERIAL_PBR_TRANSMISSION_GLOSSINESS: return "transmission_glossiness";
	}

	ufbxt_assert(0 && "Unhandled PBR map name");
	return 0;
}

static const char *ufbxt_material_feature_name(ufbx_material_pbr_map map)
{
	switch (map) {
	case UFBX_MATERIAL_FEATURE_PBR: return "pbr";
	case UFBX_MATERIAL_FEATURE_METALNESS: return "metalness";
	case UFBX_MATERIAL_FEATURE_DIFFUSE: return "diffuse";
	case UFBX_MATERIAL_FEATURE_SPECULAR: return "specular";
	case UFBX_MATERIAL_FEATURE_EMISSION: return "emission";
	case UFBX_MATERIAL_FEATURE_TRANSMISSION: return "transmission";
	case UFBX_MATERIAL_FEATURE_COAT: return "coat";
	case UFBX_MATERIAL_FEATURE_SHEEN: return "sheen";
	case UFBX_MATERIAL_FEATURE_OPACITY: return "opacity";
	case UFBX_MATERIAL_FEATURE_AMBIENT_OCCLUSION: return "ambient_occlusion";
	case UFBX_MATERIAL_FEATURE_MATTE: return "matte";
	case UFBX_MATERIAL_FEATURE_UNLIT: return "unlit";
	case UFBX_MATERIAL_FEATURE_IOR: return "ior";
	case UFBX_MATERIAL_FEATURE_DIFFUSE_ROUGHNESS: return "diffuse_roughness";
	case UFBX_MATERIAL_FEATURE_TRANSMISSION_ROUGHNESS: return "transmission_roughness";
	case UFBX_MATERIAL_FEATURE_THIN_WALLED: return "thin_walled";
	case UFBX_MATERIAL_FEATURE_CAUSTICS: return "caustics";
	case UFBX_MATERIAL_FEATURE_EXIT_TO_BACKGROUND: return "exit_to_background";
	case UFBX_MATERIAL_FEATURE_INTERNAL_REFLECTIONS: return "internal_reflections";
	case UFBX_MATERIAL_FEATURE_DOUBLE_SIDED: return "double_sided";
	case UFBX_MATERIAL_FEATURE_ROUGHNESS_AS_GLOSSINESS: return "roughness_as_glossiness";
	case UFBX_MATERIAL_FEATURE_COAT_ROUGHNESS_AS_GLOSSINESS: return "coat_roughness_as_glossiness";
	case UFBX_MATERIAL_FEATURE_TRANSMISSION_ROUGHNESS_AS_GLOSSINESS: return "transmission_roughness_as_glossiness";
	}

	ufbxt_assert(0 && "Unhandled material feature name");
	return 0;
}

static const char *ufbxt_shader_type_name(ufbx_shader_type map)
{
	switch (map) {
	case UFBX_SHADER_UNKNOWN: return "unknown";
	case UFBX_SHADER_FBX_LAMBERT: return "fbx_lambert";
	case UFBX_SHADER_FBX_PHONG: return "fbx_phong";
	case UFBX_SHADER_OSL_STANDARD_SURFACE: return "osl_standard_surface";
	case UFBX_SHADER_ARNOLD_STANDARD_SURFACE: return "arnold_standard_surface";
	case UFBX_SHADER_3DS_MAX_PHYSICAL_MATERIAL: return "3ds_max_physical_material";
	case UFBX_SHADER_3DS_MAX_PBR_METAL_ROUGH: return "3ds_max_pbr_metal_rough";
	case UFBX_SHADER_3DS_MAX_PBR_SPEC_GLOSS: return "3ds_max_pbr_spec_gloss";
	case UFBX_SHADER_GLTF_MATERIAL: return "gltf_material";
	case UFBX_SHADER_SHADERFX_GRAPH: return "shaderfx_graph";
	case UFBX_SHADER_BLENDER_PHONG: return "blender_phong";
	case UFBX_SHADER_WAVEFRONT_MTL: return "wavefront_mtl";
	}

	ufbxt_assert(0 && "Unhandled material feature name");
	return 0;
}

static size_t ufbxt_tokenize_line(char *buf, size_t buf_len, const char **tokens, size_t max_tokens, const char **p_line, const char *sep_chars)
{
	bool prev_sep = true;

	size_t num_tokens = 0;
	size_t buf_pos = 0;

	const char *p = *p_line;
	for (;;) {
		char c = *p;
		if (c == '\0' || c == '\n') break;

		bool sep = false;
		for (const char *s = sep_chars; *s; s++) {
			if (*s == c) {
				sep = true;
				break;
			}
		}

		if (!sep) {
			if (prev_sep) {
				ufbxt_assert(buf_pos < buf_len);
				buf[buf_pos++] = '\0';

				ufbxt_assert(num_tokens < max_tokens);
				tokens[num_tokens++] = buf + buf_pos;
			}

			if (c == '"') {
				p++;
				while (*p != '"') {
					c = *p;
					if (c == '\\') {
						p++;
						switch (*p) {
						case 'n': c = '\n'; break;
						case 'r': c = '\r'; break;
						case 't': c = '\t'; break;
						case '\\': c = '\\'; break;
						case '"': c = '"'; break;
						default:
							fprintf(stderr, "Bad escape '\\%c'\n", *p);
							ufbxt_assert(0 && "Bad escape");
							return 0;
						}
					}
					ufbxt_assert(buf_pos < buf_len);
					buf[buf_pos++] = c;
					p++;
				}
			} else {
				ufbxt_assert(buf_pos < buf_len);
				buf[buf_pos++] = c;
			}
		}
		prev_sep = sep;
		p++;
	}

	ufbxt_assert(buf_pos < buf_len);
	buf[buf_pos++] = '\0';

	for (size_t i = num_tokens; i < max_tokens; i++) {
		tokens[i] = "";
	}

	if (*p == '\n') p += 1;
	*p_line = p;

	return num_tokens;
}

static bool ufbxt_check_materials(ufbx_scene *scene, const char *spec, const char *filename)
{
	bool ok = true;

	char line_buf[512];
	const char *tokens[8];

	char dot_buf[512];
	const char *dots[8];

	bool seen_materials[256];

	ufbx_material *material = NULL;
	size_t num_materials = 0;
	size_t num_props = 0;

	double err_sum = 0.0;
	double err_max = 0.0;
	size_t err_num = 0;

	bool material_error = false;
	bool require_all = false;

	long version = 0;

	const long current_version = 2;

	int line = 0;
	while (*spec != '\0') {
		size_t num_tokens = ufbxt_tokenize_line(line_buf, sizeof(line_buf), tokens, ufbxt_arraycount(tokens), &spec, " \t\r");
		line++;

		if (num_tokens == 0) continue;

		const char *first_token = tokens[0];
		size_t num_dots = ufbxt_tokenize_line(dot_buf, sizeof(dot_buf), dots, ufbxt_arraycount(dots), &first_token, ".");

		if (!strcmp(dots[0], "version")) {
			char *end = NULL;
			version = strtol(tokens[1], &end, 10);
			if (!end || *end != '\0') {
				fprintf(stderr, "%s:%d: Bad value in '%s': '%s'\n", filename, line, tokens[0], tokens[1]);
				ok = false;
				continue;
			}

			if (version > current_version)
			{
				fprintf(stderr, "%s:%d: \"version %ld\" is too high for current check_material.h, maximum supported is %ld\n", filename, line,
					version, current_version);
				ok = false;
				break;
			}

			continue;
		} else if (!strcmp(dots[0], "require")) {
			if (!strcmp(tokens[1], "all")) {
				if (scene->materials.count > ufbxt_arraycount(seen_materials)) {
					fprintf(stderr, "%s:%d: Too many materials in file for 'reqiure all': %zu (max %zu)\n", filename, line,
						scene->materials.count, (size_t)ufbxt_arraycount(seen_materials));
					ok = false;
					continue;
				}
				require_all = true;
				memset(seen_materials, 0, scene->materials.count * sizeof(bool));
			} else {
				fprintf(stderr, "%s:%d: Bad require directive: '%s'\n", filename, line, tokens[1]);
				ok = false;
				continue;
			}
			continue;
		} else if (!strcmp(dots[0], "material")) {
			if (*tokens[1] == '\0') {
				fprintf(stderr, "%s:%d: Expected material name for 'material'\n", filename, line);
				ok = false;
			}

			material = ufbx_find_material(scene, tokens[1]);
			if (!material) {
				fprintf(stderr, "%s:%d: Material not found: '%s'\n", filename, line, tokens[1]);
				ok = false;
			}
			material_error = !material;

			if (material) {
				seen_materials[material->typed_id] = true;
			}

			num_materials++;
			continue;
		}

		if (!material) {
			if (!material_error) {
				fprintf(stderr, "%s:%d: Statement '%s' needs to have a material defined\n", filename, line, tokens[0]);
			}
			ok = false;
			continue;
		}

		num_props++;

		if (!strcmp(dots[0], "fbx") || !strcmp(dots[0], "pbr")) {
			ufbx_material_map *map = NULL;
			if (!strcmp(dots[0], "fbx")) {
				ufbx_material_fbx_map name = (ufbx_material_fbx_map)UFBX_MATERIAL_FBX_MAP_COUNT;
				for (size_t i = 0; i < UFBX_MATERIAL_FBX_MAP_COUNT; i++) {
					const char *str = ufbxt_fbx_map_name((ufbx_material_fbx_map)i);
					if (!strcmp(dots[1], str)) {
						name = (ufbx_material_fbx_map)i;
						break;
					}
				}
				if (name == (ufbx_material_fbx_map)UFBX_MATERIAL_FBX_MAP_COUNT) {
					fprintf(stderr, "%s:%d: Unknown FBX material map '%s' in '%s'\n", filename, line, dots[1], tokens[0]);
					ok = false;
				} else {
					map = &material->fbx.maps[name];
				}
			} else if (!strcmp(dots[0], "pbr")) {
				ufbx_material_pbr_map name = (ufbx_material_pbr_map)UFBX_MATERIAL_PBR_MAP_COUNT;
				for (size_t i = 0; i < UFBX_MATERIAL_PBR_MAP_COUNT; i++) {
					const char *str = ufbxt_pbr_map_name((ufbx_material_pbr_map)i);
					if (!strcmp(dots[1], str)) {
						name = (ufbx_material_pbr_map)i;
						break;
					}
				}
				if (name == (ufbx_material_pbr_map)UFBX_MATERIAL_PBR_MAP_COUNT) {
					fprintf(stderr, "%s:%d: Unknown PBR material map '%s' in '%s'\n", filename, line, dots[1], tokens[0]);
					ok = false;
				} else {
					map = &material->pbr.maps[name];
				}
			} else {
				ufbxt_assert(0 && "Unhandled branch");
			}

			// Errors reported above, but set ok to false just to be sure..
			if (!map) {
				ok = false;
				continue;
			}

			if (!strcmp(dots[2], "texture")) {
				size_t tok_start = 1;
				bool content = false;
				for (;;) {
					const char *tok = tokens[tok_start];
					if (!strcmp(tok, "content")) {
						content = true;
					} else {
						break;
					}
					tok_start++;
				}

				const char *tex_name = tokens[tok_start];
				if (map->texture) {
					if (strcmp(map->texture->relative_filename.data, tex_name) != 0) {
						fprintf(stderr, "%s:%d: Material '%s' %s.%s is different: got '%s', expected '%s'\n", filename, line,
							material->name.data, dots[0], dots[1], map->texture->relative_filename.data, tex_name);
						ok = false;
					}
					if (content && map->texture->content.size == 0) {
						fprintf(stderr, "%s:%d: Material '%s' %s.%s expected content for the texture %s\n", filename, line,
							material->name.data, dots[0], dots[1], map->texture->relative_filename.data);
						ok = false;
					}
				} else {
					fprintf(stderr, "%s:%d: Material '%s' %s.%s missing texture, expected '%s'\n", filename, line,
						material->name.data, dots[0], dots[1], tex_name);
					ok = false;
				}
			} else {
				size_t tok_start = 1;
				bool implicit = false;
				bool widen = false;
				for (;;) {
					const char *tok = tokens[tok_start];
					if (!strcmp(tok, "implicit")) {
						implicit = true;
					} else if (!strcmp(tok, "widen")) {
						widen = true;
					} else {
						break;
					}
					tok_start++;
				}

				double ref[4];
				size_t num_ref = 0;
				for (; num_ref < 4; num_ref++) {
					const char *tok = tokens[tok_start + num_ref];
					if (!*tok) break;

					char *end = NULL;
					ref[num_ref] = strtod(tok, &end);
					if (!end || *end != '\0') {
						fprintf(stderr, "%s:%d: Bad number in '%s': '%s'\n", filename, line, tokens[0], tok);
						ok = false;
						num_ref = 0;
						break;
					}
				}

				if (num_ref == 0) continue;

				if (!implicit && !map->has_value) {
					fprintf(stderr, "%s:%d: Material '%s' %s.%s not defined in material\n", filename, line,
						material->name.data, dots[0], dots[1]);
					ok = false;
					continue;
				} else if (implicit && map->has_value) {
					fprintf(stderr, "%s:%d: Material '%s' %s.%s defined in material, expected implicit\n", filename, line,
						material->name.data, dots[0], dots[1]);
					ok = false;
					continue;
				}

				if (!implicit && !widen && (size_t)map->value_components != num_ref) {
					fprintf(stderr, "%s:%d: Material '%s' %s.%s has wrong number of components: got %zu, expected %zu\n", filename, line,
						material->name.data, dots[0], dots[1], (size_t)map->value_components, num_ref);
					ok = false;
					continue;
				}

				if (widen && map->value_components >= num_ref) {
					fprintf(stderr, "%s:%d: Material '%s' %s.%s exected to be widened got %zu components, expected %zu\n", filename, line,
						material->name.data, dots[0], dots[1], (size_t)map->value_components, num_ref);
					ok = false;
					continue;
				}

				char mat_str[128];
				char ref_str[128];
				size_t mat_pos = 0;
				size_t ref_pos = 0;

				bool equal = true;
				for (size_t i = 0; i < num_ref; i++) {
					double mat_value = map->value_vec4.v[i];
					double ref_value = ref[i];
					double err = fabs(mat_value - ref_value);
					if (err > 0.002) {
						equal = false;
					}

					err_sum += err;
					if (err > err_max) err_max = err;
					err_num += 1;

					if (i > 0) {
						mat_pos += (size_t)snprintf(mat_str + mat_pos, sizeof(mat_str) - mat_pos, ", ");
						ref_pos += (size_t)snprintf(ref_str + ref_pos, sizeof(ref_str) - ref_pos, ", ");
					}

					mat_pos += (size_t)snprintf(mat_str + mat_pos, sizeof(mat_str) - mat_pos, "%.3f", mat_value);
					ref_pos += (size_t)snprintf(ref_str + ref_pos, sizeof(ref_str) - ref_pos, "%.3f", ref_value);
				}

				if (!equal) {
					fprintf(stderr, "%s:%d: Material '%s' %s.%s has wrong value: got (%s), expected (%s)\n", filename, line,
						material->name.data, dots[0], dots[1], mat_str, ref_str);
					ok = false;
				}
			}

		} else if (!strcmp(dots[0], "features")) {
			ufbx_material_feature name = (ufbx_material_feature)UFBX_MATERIAL_FEATURE_COUNT;
			for (size_t i = 0; i < UFBX_MATERIAL_FEATURE_COUNT; i++) {
				const char *str = ufbxt_material_feature_name((ufbx_material_feature)i);
				if (!strcmp(dots[1], str)) {
					name = (ufbx_material_feature)i;
					break;
				}
			}
			if (name == (ufbx_material_feature)UFBX_MATERIAL_FEATURE_COUNT) {
				fprintf(stderr, "%s:%d: Unknown material feature '%s' in '%s'\n", filename, line, dots[1], tokens[0]);
				ok = false;
			}

			ufbx_material_feature_info *feature = &material->features.features[name];

			char *end = NULL;
			long enabled = strtol(tokens[1], &end, 10);
			if (!end || *end != '\0') {
				fprintf(stderr, "%s:%d: Bad value in '%s': '%s'\n", filename, line, tokens[0], tokens[1]);
				ok = false;
				continue;
			}

			if (enabled != (long)feature->enabled) {
				fprintf(stderr, "%s:%d: Material '%s' features.%s mismatch: got %ld, expected %ld\n", filename, line,
					material->name.data, dots[1], (long)feature->enabled, enabled);
				ok = false;
				continue;
			}
		} else if (!strcmp(dots[0], "shader_type")) {
			ufbx_shader_type name = (ufbx_shader_type)UFBX_SHADER_TYPE_COUNT;
			for (size_t i = 0; i < UFBX_SHADER_TYPE_COUNT; i++) {
				const char *str = ufbxt_shader_type_name((ufbx_shader_type)i);
				if (!strcmp(tokens[1], str)) {
					name = (ufbx_shader_type)i;
					break;
				}
			}
			if (name == (ufbx_shader_type)UFBX_SHADER_TYPE_COUNT) {
				fprintf(stderr, "%s:%d: Unknown shader type '%s' in '%s'\n", filename, line, tokens[1], tokens[0]);
				ok = false;
			}

			if (material->shader_type != name) {
				const char *mat_name = ufbxt_shader_type_name(material->shader_type);
				fprintf(stderr, "%s:%d: Material '%s' shader type mismatch: got '%s', expected '%s'\n", filename, line,
					material->name.data, mat_name, tokens[1]);
			}
		} else {
			fprintf(stderr, "%s:%d: Invalid token '%s'\n", filename, line, tokens[0]);
			ok = false;
		}
	}

	if (require_all) {
		for (size_t i = 0; i < scene->materials.count; i++) {
			if (!seen_materials[i]) {
				fprintf(stderr, "%s: 'require all': Material in FBX not described: '%s'\n", filename,
					scene->materials.data[i]->name.data);
				ok = false;
			}
		}
	}

	if (ok) {
		double avg = err_num > 0 ? err_sum / (double)err_num : 0.0;
		printf("Checked %zu materials, %zu properties (error avg %.3g, max %.3g, %zu tests)\n", num_materials, num_props, avg, err_max, err_num);
	}

	return ok;
}

#endif
