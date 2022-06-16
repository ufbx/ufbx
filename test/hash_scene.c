#define _CRT_SECURE_NO_WARNINGS

#include "../ufbx.h"
#include "hash_scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

ufbx_scene *load_scene(const char *filename, int frame)
{
	ufbx_load_opts opts = { 0 };
	opts.load_external_files = true;
	opts.evaluate_caches = true;
	opts.evaluate_skinning = true;

	ufbx_error error;
	ufbx_scene *scene = ufbx_load_file(filename, &opts, &error);
	if (!scene) {
		fprintf(stderr, "Failed to load scene: %s\n", error.description.data);
		exit(2);
	}

	if (frame > 0) {
		ufbx_evaluate_opts eval_opts = { 0 };
		eval_opts.evaluate_caches = true;
		eval_opts.evaluate_skinning = true;
		eval_opts.load_external_files = true;

		double time = scene->anim.time_begin + frame / scene->settings.frames_per_second;
		ufbx_scene *state = ufbx_evaluate_scene(scene, NULL, time, NULL, &error);
		if (!state) {
			fprintf(stderr, "Failed to evaluate scene: %s\n", error.description.data);
			exit(2);
		}
		ufbx_free_scene(scene);
		scene = state;
	}

	return scene;
}

int main(int argc, char **argv)
{
	const char *filename = NULL;
	const char *dump_filename = NULL;
	int max_dump_errors = -1;
	bool do_check = false;
	bool dump_all = false;
	bool verbose = false;

	int frame = -1;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--frame")) {
			if (++i < argc) frame = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--verbose")) {
			verbose = true;
		} else if (!strcmp(argv[i], "--check")) {
			do_check = true;
		} else if (!strcmp(argv[i], "--dump")) {
			if (++i < argc) dump_filename = argv[i];
		} else if (!strcmp(argv[i], "--dump-all")) {
			dump_all = true;
		} else if (!strcmp(argv[i], "--max-dump-errors")) {
			if (++i < argc) max_dump_errors = atoi(argv[i]);
		} else {
			if (filename) {
				if (argv[i][0] == '-') {
					fprintf(stderr, "Unknown flag: %s\n", argv[i]);
					exit(1);
				} else {
					fprintf(stderr, "Error: Multiple input files\n");
					exit(1);
				}
			}
			filename = argv[i];
		}
	}

	if (!filename) {
		fprintf(stderr, "Usage: hash_scene <file.fbx>\n");
		fprintf(stderr, "       hash_scene --check <hashes.txt>\n");
		return 1;
	}

	int num_fail = 0;
	int num_total = 0;

	FILE *dump_file = NULL;

	if (do_check) {
		FILE *f = fopen(filename, "r");
		if (!f) {
			fprintf(stderr, "Failed to open hash file\n");
			return 1;
		}

		uint64_t fbx_hash = 0;
		char fbx_file[1024];
		while (fscanf(f, "%" SCNx64 " %d %s", &fbx_hash, &frame, fbx_file) == 3) {
			ufbx_scene *scene = load_scene(fbx_file, frame);

			uint64_t hash = ufbxt_hash_scene(scene, NULL);
			if (hash != fbx_hash || dump_all) {
				if (num_fail < max_dump_errors || dump_all) {
					if (!dump_file) {
						dump_file = fopen(dump_filename, "wb");
						assert(dump_file);
					}

					fprintf(dump_file, "\n-- %d %s\n\n", frame, fbx_file);
					ufbxt_hash_scene(scene, dump_file);
				}

				if (!dump_all) {
					printf("%s: FAIL %" PRIx64 " (local) vs %" PRIx64 " (reference)\n",
						fbx_file, hash, fbx_hash);
					num_fail++;
				}
			} else {
				if (verbose) {
					printf("%s: OK\n", fbx_file);
				}
			}
			num_total++;
		}

		fclose(f);

		if (!dump_all) {
			if (verbose || num_fail > 0) {
				printf("\n");
			}
			printf("%d/%d hashes match\n", num_total - num_fail, num_total);
		}
	} else {
		ufbx_scene *scene = load_scene(filename, frame);

		if (dump_filename) {
			dump_file = fopen(dump_filename, "wb");
			assert(dump_file);
		}

		uint64_t hash = ufbxt_hash_scene(scene, dump_file);

		printf("%016" PRIx64 "\n", hash);
	}

	if (dump_file) {
		fclose(dump_file);
	}

	return num_fail > 0 ? 3 : 0;
}

#define UFBX_NO_MATH_H
#define UFBX_MATH_PREFIX fdlibm_

#include "../ufbx.c"

