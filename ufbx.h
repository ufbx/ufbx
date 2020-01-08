#ifndef UFBX_UFBX_H_INLCUDED
#define UFBX_UFBX_H_INLCUDED

#include <stdint.h>

#define UFBX_ERROR_DESC_MAX_LENGTH 255
#define UFBX_ERROR_STACK_MAX_DEPTH 8
#define UFBX_ERROR_STACK_NAME_MAX_LENGTH 31

typedef struct ufbx_error_s {
	uint32_t source_line;
	uint32_t stack_size;
	char desc[UFBX_ERROR_DESC_MAX_LENGTH + 1];
	char stack[UFBX_ERROR_STACK_MAX_DEPTH][UFBX_ERROR_DESC_MAX_LENGTH + 1];
} ufbx_error;

#endif
