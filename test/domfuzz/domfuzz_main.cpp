#define _CRT_SECURE_NO_WARNINGS

#include "fbxdom.h"
#include <vector>
#include <string>
#include <stdio.h>
#include <algorithm>
#include <stdlib.h>
#include "../../ufbx.h"
#include "../check_scene.h"

bool g_verbose = false;

std::vector<char> read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	std::vector<char> data;
	data.resize(ftell(f));
	fseek(f, 0, SEEK_SET);
	fread(data.data(), 1, data.size(), f);
	fclose(f);
	return data;
}

using mutate_simple_fn = bool(fbxdom::node_ptr node, uint32_t index);

bool mutate_remove_child(fbxdom::node_ptr node, uint32_t index)
{
	if (index >= node->children.size()) return false;
	node->children.erase(node->children.begin() + index);
	return true;
}

bool mutate_duplicate_child(fbxdom::node_ptr node, uint32_t index)
{
	if (index >= node->children.size()) return false;
	fbxdom::node_ptr child = node->children[index];
	node->children.insert(node->children.begin() + index, std::move(child));
	return true;
}

bool mutate_reverse_children(fbxdom::node_ptr node, uint32_t index)
{
	if (index > 0) return false;
	std::reverse(node->children.begin(), node->children.end());
	return true;
}

bool mutate_remove_value(fbxdom::node_ptr node, uint32_t index)
{
	if (node->values.size() > 10) return false;
	if (index >= node->values.size()) return false;
	node->values.erase(node->values.begin() + index);
	return true;
}

bool mutate_duplicate_value(fbxdom::node_ptr node, uint32_t index)
{
	if (node->values.size() > 10) return false;
	if (index >= node->values.size()) return false;
	fbxdom::value value = node->values[index];
	node->values.insert(node->values.begin() + index, std::move(value));
	return true;
}

bool mutate_reverse_values(fbxdom::node_ptr node, uint32_t index)
{
	if (index > 0) return false;
	std::reverse(node->values.begin(), node->values.end());
	return true;
}

struct mutator
{
	const char *name;
	mutate_simple_fn *simple_fn;
};

static const mutator mutators[] = {
	{ "remove child", &mutate_remove_child },
	{ "duplicate child", &mutate_duplicate_child },
	{ "reverse children", &mutate_reverse_children },
	{ "remove value", &mutate_remove_value },
	{ "duplicate value", &mutate_duplicate_value },
	{ "reverse values", &mutate_reverse_values },
};

static const size_t num_mutators = sizeof(mutators) / sizeof(*mutators);

struct mutable_result
{
	fbxdom::node_ptr self;
	fbxdom::node_ptr target;
};

fbxdom::node_ptr find_path(fbxdom::node_ptr root, const std::vector<uint32_t> &path)
{
	fbxdom::node_ptr node = root;
	for (uint32_t ix : path) {
		node = node->children[ix];
	}
	return node;
}

void format_path(char *buf, size_t buf_size, fbxdom::node_ptr root, const std::vector<uint32_t> &path)
{
	int res = snprintf(buf, buf_size, "Root");
	if (res < 0 || res >= buf_size) return;
	size_t pos = (size_t)res;

	fbxdom::node_ptr node = root;
	for (uint32_t ix : path) {
		node = node->children[ix];
		int res = snprintf(buf + pos, buf_size - pos, "/%u/%.*s", ix, (int)node->name.size(), node->name.data());
		pos += res;
		if (res < 0 || pos >= buf_size) break;
	}
}

fbxdom::node_ptr mutate_path(fbxdom::node_ptr &node, const std::vector<uint32_t> &path, uint32_t depth=0)
{
	fbxdom::node_ptr copy = node->copy();
	if (depth < path.size()) {
		fbxdom::node_ptr &child_ref = copy->children[path[depth]];
		fbxdom::node_ptr child = mutate_path(child_ref, path, depth + 1);
		node = copy;
		return child;
	} else {
		node = copy;
		return copy;
	}
}

bool increment_path(fbxdom::node_ptr root, std::vector<uint32_t> &path)
{
	{
		fbxdom::node_ptr node = find_path(root, path);
		if (!node->children.empty()) {
			path.push_back(0);
			return true;
		}
	}

	while (!path.empty()) {
		uint32_t index = path.back();
		path.pop_back();
		fbxdom::node_ptr current = find_path(root, path);
		if (index + 1 < current->children.size()) {
			path.push_back(index + 1);
			return true;
		}
	}

	return false;
}

struct mutation_iterator
{
	std::vector<uint32_t> path = { };
	uint32_t mutator_index = 0;
	uint32_t mutator_internal_index = 0;
};

fbxdom::node_ptr next_mutation(fbxdom::node_ptr root, mutation_iterator &iter)
{
	for (;;) {
		fbxdom::node_ptr new_root = root;
		fbxdom::node_ptr new_node = mutate_path(new_root, iter.path);

		if (mutators[iter.mutator_index].simple_fn(new_node, iter.mutator_internal_index)) {
			iter.mutator_internal_index++;
			return new_root;
		}

		iter.mutator_internal_index = 0;
		if (iter.mutator_index + 1 < num_mutators) {
			iter.mutator_index++;
			continue;
		}

		iter.mutator_index = 0;
		if (increment_path(root, iter.path)) {
			continue;
		}

		return { };
	}
}

static char dump_buffer[16*1024*1024];

int main(int argc, char **argv)
{
	int selected_step = -1;
	int start_step = -1;
	const char *input_file = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			g_verbose = 1;
		} else if (!strcmp(argv[i], "--step")) {
			if (++i < argc) selected_step = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--start")) {
			if (++i < argc) start_step = atoi(argv[i]);
		} else {
			input_file = argv[i];
		}
	}


	std::vector<char> data = read_file(input_file);

	{
		ufbx_scene *scene = ufbx_load_memory(data.data(), data.size(), NULL, NULL);
		if (!scene) return 0;
		if (scene->metadata.ascii || scene->metadata.big_endian) {
			ufbx_free_scene(scene);
			return 0;
		}
		ufbx_free_scene(scene);
	}

	fbxdom::node_ptr root = fbxdom::parse(data.data(), data.size());
	ufbx_error error = { };

	{
		size_t dump_size = fbxdom::dump(dump_buffer, sizeof(dump_buffer), root);
		ufbx_scene *scene = ufbx_load_memory(data.data(), data.size(), NULL, &error);

		ufbxt_assert(scene);
		ufbxt_check_scene(scene);

		ufbx_free_scene(scene);
	}

	char path_str[512];

	mutation_iterator iter;
	fbxdom::node_ptr mut_root;
	int current_step = 0;
	while ((mut_root = next_mutation(root, iter)) != nullptr) {
		if (selected_step >= 0) {
			if (current_step != selected_step) {
				current_step++;
				continue;
			}
		}
		if (start_step >= 0) {
			if (current_step < start_step) {
				current_step++;
				continue;
			}
		}

		format_path(path_str, sizeof(path_str), root, iter.path);
		printf("%d: %s/ %s %u: ", current_step, path_str, mutators[iter.mutator_index].name, iter.mutator_internal_index);

		size_t dump_size = fbxdom::dump(dump_buffer, sizeof(dump_buffer), mut_root);
		ufbx_scene *scene = ufbx_load_memory(dump_buffer, dump_size, NULL, &error);

		if (scene) {
			ufbxt_check_scene(scene);
			printf("OK!\n");
		} else {
			printf("%s\n", error.description);
		}

		ufbx_free_scene(scene);
		current_step++;
	}

	return 0;
}
