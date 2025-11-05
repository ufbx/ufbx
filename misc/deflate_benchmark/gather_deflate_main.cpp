#define _CRT_SECURE_NO_WARNINGS

#include "../../test/domfuzz/fbxdom.h"
#include <vector>
#include <string>
#include <stdio.h>
#include <unordered_map>

const char *g_root;
std::unordered_map<std::string, uint32_t> g_counters;
FILE *g_json;
FILE *g_data;
size_t g_data_offset;
size_t g_array_count;

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

void dump_deflate_arrays(fbxdom::node_ptr node, const std::string& path)
{
	std::string name;
	std::string full_name;
	for (char c : node->name) {
		if (isalnum(c)) {
			name += (char)tolower(c);
		}
		if (c >= 0x20 && c <= 0x7d && c != '\\') {
			full_name += c;
		}
	}

	std::string child_path = path;
	if (!full_name.empty())
		child_path += "/" + full_name;

	for (fbxdom::value &value : node->values) {
		if (value.data_array.encoding == 1) {
			uint32_t decompressed_size = 0;
			switch (value.type) {
			case 'c': case 'b': decompressed_size = value.data_array.length * 1; break;
			case 'i': case 'f': decompressed_size = value.data_array.length * 4; break;
			case 'l': case 'd': decompressed_size = value.data_array.length * 8; break;
			}

			if (decompressed_size <= 4096) {
				continue;
			}

			uint32_t count = g_counters[name]++;

			char filename[256];
			snprintf(filename, sizeof(filename), "%s/%s_%u.zlib", g_root, name.c_str(), count);

			fprintf(g_json, "\t\t{\n"
				"\t\t\t\"type\": \"%c\",\n"
				"\t\t\t\"path\": \"%s\",\n"
				"\t\t\t\"size_u\": %u,\n"
				"\t\t\t\"size_z\": %u,\n"
				"\t\t\t\"offset\": %zu\n"
				"\t\t},\n",
				value.type,
				child_path.c_str(),
				decompressed_size,
				value.data_array.compressed_length,
				g_data_offset);

			fwrite(value.data.data(), 1, value.data.size(), g_data);
			g_data_offset += value.data.size();
			g_array_count++;
		}
	}

	for (fbxdom::node_ptr &child : node->children) {
		dump_deflate_arrays(child, child_path);
	}
}

int main(int argc, char **argv)
{
	g_root = argv[2];
	const char *filename = argv[1];

	std::vector<char> data = read_file(argv[1]);
	fbxdom::node_ptr root = fbxdom::parse(data.data(), data.size());
	if (!root) return 0;

	char json_path[256];
	snprintf(json_path, sizeof(json_path), "%s.json", g_root);
	g_json = fopen(json_path, "wb");

	char data_path[256];
	snprintf(data_path, sizeof(data_path), "%s.bin", g_root);
	g_data = fopen(data_path, "wb");

	fprintf(g_json, "{\n"
		"\t\"source_path\": \"%s\",\n"
		"\t\"data_path\": \"%s\",\n"
		"\t\"arrays\": [\n",
		filename,
		data_path);

	dump_deflate_arrays(root, "");

	if (g_array_count > 0) {
		// Remove trailing comma
		fseek(g_json, -2, SEEK_CUR);
	}

	fprintf(g_json, "\n\t],\n"
		"\t\"data_size\": %zu\n"
		"}\n",
		g_data_offset);

	fclose(g_json);
	fclose(g_data);

	return 0;
}
