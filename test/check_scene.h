#ifndef UFBXT_CHECK_SCENE_H_INCLUDED
#define UFBXT_CHECK_SCENE_H_INCLUDED

#ifndef ufbxt_assert
#include <assert.h>
#define ufbxt_assert(cond) assert(cond)
#endif

#ifndef ufbxt_arraycount
#define ufbxt_arraycount(arr) (sizeof(arr) / sizeof(*(arr)))
#endif

#include <string.h>
#include "../ufbx.h"

static void ufbxt_check_string(ufbx_string str)
{
	// Data may never be NULL, empty strings should have data = ""
	ufbxt_assert(str.data != NULL);
	ufbxt_assert(strlen(str.data) == str.length);
}

static void ufbxt_check_vertex_element(ufbx_scene *scene, ufbx_mesh *mesh, void *void_elem, size_t elem_size)
{
	ufbx_vertex_void *elem = (ufbx_vertex_void*)void_elem;
	if (elem->data == NULL) {
		ufbxt_assert(elem->indices == NULL);
		ufbxt_assert(elem->num_elements == 0);
		return;
	}

	ufbxt_assert(elem->num_elements >= 0);
	ufbxt_assert(elem->indices != NULL);

	// Check that the indices are in range
	for (size_t i = 0; i < mesh->num_indices; i++) {
		int32_t ix = elem->indices[i];
		ufbxt_assert(ix >= -1 && ix < elem->num_elements);
	}

	// Check that the data at invalid index is valid and zero
	char zero[32] = { 0 };
	ufbxt_assert(elem_size <= 32);
	ufbxt_assert(!memcmp((char*)elem->data - elem_size, zero, elem_size));
}

static void ufbxt_check_props(ufbx_scene *scene, ufbx_props *props, bool top)
{
	ufbx_prop *prev = NULL;
	for (size_t i = 0; i < props->num_props; i++) {
		ufbx_prop *prop = &props->props[i];

		ufbxt_assert(prop->type < UFBX_NUM_PROP_TYPES);
		ufbxt_check_string(prop->name);
		ufbxt_check_string(prop->value_str);

		// Properties should be sorted by name and duplicates should be removed
		if (prev) {
			ufbxt_assert(prop->imp_key >= prev->imp_key);
			ufbxt_assert(strcmp(prop->name.data, prev->name.data) > 0);
		}

		if (top) {
			ufbx_prop *ref = ufbx_find_prop(props, prop->name.data);
			ufbxt_assert(prop == ref);
		}

		prev = prop;
	}

	if (props->defaults) {
		ufbxt_check_props(scene, props->defaults, false);
	}
}

static void ufbxt_check_node(ufbx_scene *scene, ufbx_node *node)
{
	ufbxt_check_string(node->name);
	ufbxt_check_props(scene, &node->props, true);

	if (node->parent) {
		bool found = false;
		for (size_t i = 0; i < node->parent->children.size; i++) {
			if (node->parent->children.data[i] == node) {
				found = true;
				break;
			}
		}
		ufbxt_assert(found);
	}

	for (size_t i = 0; i < node->children.size; i++) {
		ufbxt_assert(node->children.data[i]->parent == node);
	}
}

static void ufbxt_check_mesh(ufbx_scene *scene, ufbx_mesh *mesh)
{
	ufbx_mesh *found = ufbx_find_mesh(scene, mesh->node.name.data);
	ufbxt_assert(found && !strcmp(found->node.name.data, mesh->node.name.data));

	ufbxt_check_vertex_element(scene, mesh, &mesh->vertex_position, sizeof(ufbx_vec3));
	ufbxt_check_vertex_element(scene, mesh, &mesh->vertex_normal, sizeof(ufbx_vec3));
	ufbxt_check_vertex_element(scene, mesh, &mesh->vertex_binormal, sizeof(ufbx_vec3));
	ufbxt_check_vertex_element(scene, mesh, &mesh->vertex_tangent, sizeof(ufbx_vec3));
	ufbxt_check_vertex_element(scene, mesh, &mesh->vertex_uv, sizeof(ufbx_vec2));
	ufbxt_check_vertex_element(scene, mesh, &mesh->vertex_color, sizeof(ufbx_vec4));

	ufbxt_assert(mesh->num_vertices == mesh->vertex_position.num_elements);
	ufbxt_assert(mesh->num_triangles <= mesh->num_indices);

	uint32_t prev_end = 0;
	for (size_t i = 0; i < mesh->num_faces + mesh->num_bad_faces; i++) {
		ufbx_face face = mesh->faces[i];
		if (i == mesh->num_faces) prev_end = 0;
		if (mesh->num_bad_faces == 0) {
			ufbxt_assert(face.index_begin == prev_end);
		} else {
			ufbxt_assert(face.index_begin >= prev_end);
		}
		if (i < mesh->num_faces) {
			ufbxt_assert(face.num_indices >= 3);
		} else {
			ufbxt_assert(face.num_indices > 0 && face.num_indices < 3);
		}
		prev_end = face.index_begin + face.num_indices;
		ufbxt_assert(prev_end <= mesh->num_indices);

		for (size_t j = face.index_begin; j < face.index_begin + face.num_indices; j++) {
			ufbx_face *p_face = ufbx_find_face(mesh, j);
			ufbxt_assert(p_face - mesh->faces == i);
		}

		if (face.num_indices >= 3) {
			size_t num_tris = face.num_indices - 2;
			ufbxt_assert(face.num_indices <= 1024);
			uint32_t tris[1024];
			size_t ix_count[1024];
			memset(tris, 0xff, num_tris * 3 * sizeof(uint32_t));
			memset(ix_count, 0, face.num_indices * sizeof(uint32_t));
			ufbxt_assert(ufbx_triangulate(tris, ufbxt_arraycount(tris), mesh, face));

			for (size_t i = 0; i < num_tris; i++) {
				uint32_t a = tris[i*3 + 0];
				uint32_t b = tris[i*3 + 1];
				uint32_t c = tris[i*3 + 2];
				ufbxt_assert(a != b);
				ufbxt_assert(b != c);
				ufbxt_assert(a >= face.index_begin && a - face.index_begin < face.num_indices);
				ufbxt_assert(b >= face.index_begin && b - face.index_begin < face.num_indices);
				ufbxt_assert(c >= face.index_begin && c - face.index_begin < face.num_indices);
				ix_count[a - face.index_begin]++;
				ix_count[b - face.index_begin]++;
				ix_count[c - face.index_begin]++;
			}

			for (uint32_t i = 0; i < face.num_indices; i++) {
				ufbxt_assert(ix_count[i] >= 0);
			}
		}
	}

	for (size_t i = 0; i < mesh->num_edges; i++) {
		ufbx_edge edge = mesh->edges[i];
		ufbxt_assert(edge.indices[0] < mesh->num_indices);
		ufbxt_assert(edge.indices[1] < mesh->num_indices);
	}

	for (size_t i = 0; i < mesh->uv_sets.size; i++) {
		ufbx_uv_set *set = &mesh->uv_sets.data[i];
		if (i == 0) {
			ufbxt_assert(mesh->vertex_uv.data == set->vertex_uv.data);
			ufbxt_assert(mesh->vertex_uv.indices == set->vertex_uv.indices);
			ufbxt_assert(mesh->vertex_uv.num_elements == set->vertex_uv.num_elements);
		}
		ufbxt_check_string(set->name);
		ufbxt_check_vertex_element(scene, mesh, &set->vertex_uv, sizeof(ufbx_vec2));
	}

	for (size_t i = 0; i < mesh->color_sets.size; i++) {
		ufbx_color_set *set = &mesh->color_sets.data[i];
		if (i == 0) {
			ufbxt_assert(mesh->vertex_color.data == set->vertex_color.data);
			ufbxt_assert(mesh->vertex_color.indices == set->vertex_color.indices);
			ufbxt_assert(mesh->vertex_color.num_elements == set->vertex_color.num_elements);
		}
		ufbxt_check_string(set->name);
		ufbxt_check_vertex_element(scene, mesh, &set->vertex_color, sizeof(ufbx_vec4));
	}

	for (size_t i = 0; i < mesh->num_edges; i++) {
		ufbx_edge edge = mesh->edges[i];
		ufbxt_assert(edge.indices[0] < mesh->num_indices);
		ufbxt_assert(edge.indices[1] < mesh->num_indices);
		ufbx_face *face = ufbx_find_face(mesh, edge.indices[0]);
		ufbxt_assert(face);
		ufbxt_assert(face == ufbx_find_face(mesh, edge.indices[1]));
	}

	if (mesh->face_material) {
		for (size_t i = 0; i < mesh->num_faces; i++) {
			int32_t material = mesh->face_material[i];
			ufbxt_assert(material >= 0 && material < mesh->materials.size);
		}
	}

	for (size_t i = 0; i < mesh->skins.size; i++) {
		ufbx_skin *skin = &mesh->skins.data[i];
		ufbxt_assert(skin->bone);
		ufbxt_check_node(scene, skin->bone);

		for (size_t j = 0; j < skin->num_weights; j++) {
			ufbxt_assert(skin->indices[j] >= -1 && skin->indices[j] < mesh->num_vertices);
		}
	}
}

static void ufbxt_check_material(ufbx_scene *scene, ufbx_material *material)
{
	ufbxt_check_string(material->name);
	ufbxt_check_props(scene, &material->props, true);
}

static void ufbxt_check_anim_stack(ufbx_scene *scene, ufbx_anim_stack *anim_stack)
{
	ufbxt_check_string(anim_stack->name);
	ufbxt_check_props(scene, &anim_stack->props, true);
	for (size_t i = 0; i < anim_stack->layers.size; i++) {
		ufbx_anim_layer *layer = anim_stack->layers.data[i];
		ptrdiff_t layer_i = scene->anim_layers.data - layer;
		ufbxt_assert(layer >= scene->anim_layers.data);
		ufbxt_assert(layer < scene->anim_layers.data + scene->anim_layers.size);
	}
}

static void ufbxt_check_anim_layer(ufbx_scene *scene, ufbx_anim_layer *anim_layer)
{
	ufbxt_check_string(anim_layer->name);
	ufbxt_check_props(scene, &anim_layer->layer_props, true);
}

static void ufbxt_check_anim_prop(ufbx_scene *scene, ufbx_anim_prop *anim_prop)
{
	ufbxt_check_string(anim_prop->name);
	switch (anim_prop->target)
	{
	case UFBX_ANIM_UNKNOWN: /* Nop */ break;
	case UFBX_ANIM_MODEL: ufbxt_assert(anim_prop->index < scene->models.size); break;
	case UFBX_ANIM_MESH: ufbxt_assert(anim_prop->index < scene->meshes.size); break;
	case UFBX_ANIM_LIGHT: ufbxt_assert(anim_prop->index < scene->lights.size); break;
	case UFBX_ANIM_MATERIAL: ufbxt_assert(anim_prop->index < scene->materials.size); break;
	case UFBX_ANIM_BONE: ufbxt_assert(anim_prop->index < scene->bones.size); break;
	case UFBX_ANIM_ANIM_LAYER: ufbxt_assert(anim_prop->index < scene->anim_layers.size); break;
	case UFBX_ANIM_BLEND_CHANNEL: ufbxt_assert(anim_prop->index < scene->blend_channels.size); break;
	default: ufbxt_assert(0 && "Bad anim target"); break;
	}
}

static void ufbxt_check_scene(ufbx_scene *scene)
{
	ufbxt_check_string(scene->metadata.creator);

	for (size_t i = 0; i < scene->nodes.size; i++) {
		ufbxt_check_node(scene, scene->nodes.data[i]);
	}

	for (size_t i = 0; i < scene->meshes.size; i++) {
		ufbxt_check_mesh(scene, &scene->meshes.data[i]);
	}

	for (size_t i = 0; i < scene->materials.size; i++) {
		ufbxt_check_material(scene, &scene->materials.data[i]);
	}

	for (size_t i = 0; i < scene->anim_stacks.size; i++) {
		ufbxt_check_anim_stack(scene, &scene->anim_stacks.data[i]);
	}

	for (size_t i = 0; i < scene->anim_layers.size; i++) {
		ufbxt_check_anim_layer(scene, &scene->anim_layers.data[i]);
	}

	for (size_t i = 0; i < scene->anim_props.size; i++) {
		ufbxt_check_anim_prop(scene, &scene->anim_props.data[i]);
	}
}

#endif
