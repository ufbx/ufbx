#ifndef UFBX_UFBX_H_INLCUDED
#define UFBX_UFBX_H_INLCUDED

#include <stdint.h>

#define UFBX_ERROR_DESC_MAX_LENGTH 255

typedef struct ufbx_error_s {
	uint32_t source_line;
	char desc[UFBX_ERROR_DESC_MAX_LENGTH + 1];
} ufbx_error;

#endif
