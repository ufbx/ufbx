#ifndef UFBX_LIBC_H
#define UFBX_LIBC_H

// Tell ufbx not to include any libc
#define UFBX_NO_LIBC

// Use system headers for types and varargs.
// You could define these yourself if you want to, but then you should define
// `UFBX_NO_LIBC_TYPES` when including `ufbx.h`.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#if defined(__cplusplus)
extern "C" {
#endif

// Not required by ufbx
void ufbxc_assert_fail(const char *cond, const char *file, int line);
#define ufbxc_assert(cond) do { if (!(cond)) ufbxc_assert_fail(#cond, __FILE__, __LINE__); } while (0)

// <math.h>
// Provided by fdlibm

double fdlibm_sqrt(double x);
double fdlibm_sin(double x);
double fdlibm_cos(double x);
double fdlibm_tan(double x);
double fdlibm_asin(double x);
double fdlibm_acos(double x);
double fdlibm_atan(double x);
double fdlibm_atan2(double y, double x);
double fdlibm_pow(double x, double y);
double fdlibm_fmin(double a, double b);
double fdlibm_fmax(double a, double b);
double fdlibm_fabs(double x);
double fdlibm_copysign(double x, double y);
double fdlibm_nextafter(double x, double y);
double fdlibm_rint(double x);
double fdlibm_ceil(double x);
int fdlibm_isnan(double x);

#define UFBX_MATH_PREFIX fdlibm_

#define UFBX_INFINITY (1e+300 * 1e+300)
#define UFBX_NAN (UFBX_INFINITY * 0.0f)

// <string.h>

size_t ufbxc_strlen(const char *str);
void *ufbxc_memcpy(void *dst, const void *src, size_t count);
void *ufbxc_memmove(void *dst, const void *src, size_t count);
void *ufbxc_memset(void *dst, int ch, size_t count);
const void *ufbxc_memchr(const void *ptr, int value, size_t num);
int ufbxc_memcmp(const void *a, const void *b, size_t count);
int ufbxc_strcmp(const char *a, const char *b);
int ufbxc_strncmp(const char *a, const char *b, size_t count);

// ufbx internally does not remap the <string.h> names by default.
#define UFBX_STRING_PREFIX ufbxc_

// <stdlib.h>

float ufbxc_strtof(const char *str, char **end);
double ufbxc_strtod(const char *str, char **end);
unsigned long ufbxc_strtoul(const char *str, char **end, int base);
void ufbxc_qsort(void *ptr, size_t count, size_t size, int (*cmp_fn)(const void*, const void*));

// Optional: Global memory allocator may be disabled
#if defined(UFBXC_HAS_MALLOC)
	void *ufbxc_malloc(size_t size);
	void *ufbxc_realloc(void *ptr, size_t new_size);
	void ufbxc_free(void *ptr);
#else
	#define UFBX_NO_MALLOC
#endif

// <stdio.h>

typedef struct ufbxc_FILE ufbxc_FILE;
int ufbxc_vsnprintf(char *buffer, size_t count, const char *format, va_list args);

// Optional: Used for opening files
#if defined(UFBXC_HAS_STDIO)
	typedef int64_t ufbxc_fpos;
	#define ufbxc_SEEK_CUR 1
	#define ufbxc_SEEK_END 2
	ufbxc_FILE *ufbxc_fopen(const char *filename, const char *mode);
	size_t ufbxc_fread(void *buffer, size_t size, size_t count, ufbxc_FILE *f);
	void ufbxc_fclose(ufbxc_FILE *f);
	long ufbxc_ftell(ufbxc_FILE *f);
	int ufbxc_ferror(ufbxc_FILE *f);
	int ufbxc_fseek(ufbxc_FILE *f, long offset, int origin);
	int ufbxc_fgetpos(ufbxc_FILE *f, ufbxc_fpos *pos);
	int ufbxc_fsetpos(ufbxc_FILE *f, const ufbxc_fpos *pos);
	void ufbxc_rewind(ufbxc_FILE *f);
#else
	#define UFBX_NO_STDIO
#endif

// Optional: Used for error logging
#if defined(UFBXC_HAS_STDERR)
	#define ufbxc_stderr ((ufbxc_FILE*)1)
	void ufbxc_fprintf(ufbxc_FILE *file, const char *fmt, ...);
#else
	#define UFBX_NO_STDERR
#endif

// -- OS interface requried for optional features

#if defined(UFBXC_HAS_MALLOC)
	// Minimal implementation of this API could be:
	//
	// char memory[16*1024*1024];
	// bool allocated = false;
	// void *ufbxc_os_allocate(size_t size, size_t *p_allocated_size) {
	//     if (allocated) return false;
	//     allocated = true;
	//     *p_allocated_size = sizeof(memory);
	//     return memory;
	// }
	// bool ufbxc_os_free() {
	//     return false;
	// }
	//

	// Allocate memory from the OS, you can return more memory than requested via `p_allocated_size`.
	void *ufbxc_os_allocate(size_t size, size_t *p_allocated_size);

	// Free memory returned by `ufbxc_os_allocate()`.
	// Return `true` if the memory was freed. If you return `false` ufbxc will re-use the memory.
	bool ufbxc_os_free(void *pointer, size_t allocated_size);
#endif

#if defined(UFBXC_HAS_STDIO)
	// Open a file for reading.
	// You may optionally specify the file size `p_file_size` which changes the read behavior.
	bool ufbxc_os_open_file(size_t index, const char *filename, size_t *p_file_size);

	// Read `count` bytes from the file into `dst`.
	// Offset is guaranteed to be always past the previous read end position.
	//
	// `p_file_size` specified: `offset + count` is guaranteed to be in bounds of the reported file size.
	//
	// `p_file_size` not specified: If you do not specify `p_file_size` in `ufbxc_os_open_file()`,
	//  this is guaranteed to be called with sequential offsets with no gaps.
	size_t ufbxc_os_read_file(size_t index, void *dst, size_t offset, size_t count);

	// Close previously opened file.
	void ufbxc_os_close_file(size_t index);
#endif

#if defined(UFBXC_HAS_STDERR)
	// Print an error message.
	// `message` is '\0' terminated and matches `length`.
	void ufbxc_os_print_error(const char *message, size_t length);
#endif

#if defined(UFBXC_HAS_EXIT)
	// Exit the process.
	void ufbxc_os_exit(int code);
#endif

#if defined(__cplusplus)
}
#endif

#endif

