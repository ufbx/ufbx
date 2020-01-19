#ifndef UFBX_UFBX_H_INLCUDED
#define UFBX_UFBX_H_INLCUDED

#include <stdint.h>
#include <stddef.h>

#define UFBX_ERROR_DESC_MAX_LENGTH 255
#define UFBX_ERROR_STACK_MAX_DEPTH 8
#define UFBX_ERROR_STACK_NAME_MAX_LENGTH 31

typedef struct ufbx_error {
	uint32_t source_line;
	uint32_t stack_size;
	char desc[UFBX_ERROR_DESC_MAX_LENGTH + 1];
	char stack[UFBX_ERROR_STACK_MAX_DEPTH][UFBX_ERROR_DESC_MAX_LENGTH + 1];
} ufbx_error;

typedef struct ufbx_metadata {
	int ascii;
	uint32_t version;
	 const char *creator;
} ufbx_metadata;

typedef struct ufbx_scene {
	ufbx_metadata metadata;
} ufbx_scene;

ufbx_scene *ufbx_load_memory(const void *data, size_t size, ufbx_error *error);
void ufbx_free_scene(ufbx_scene *scene);

#endif
