#ifndef UFBX_LIBC_H
#define UFBX_LIBC_H

// Tell ufbx not to include any libc
#define UFBX_NO_LIBC

// SSE headers get confused by us overriding libc stuff..
#define UFBX_NO_SSE

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

#define sqrt fdlibm_sqrt
#define sin fdlibm_sin
#define cos fdlibm_cos
#define tan fdlibm_tan
#define asin fdlibm_asin
#define acos fdlibm_acos
#define atan fdlibm_atan
#define atan2 fdlibm_atan2
#define pow fdlibm_pow
#define fmin fdlibm_fmin
#define fmax fdlibm_fmax
#define fabs fdlibm_fabs
#define copysign fdlibm_copysign
#define nextafter fdlibm_nextafter
#define rint fdlibm_rint
#define ceil fdlibm_ceil
#define isnan fdlibm_isnan

#define INFINITY (1e+300 * 1e+300)
#define NAN (INFINITY * 0.0f)

// <string.h>

size_t ufbxc_strlen(const char *str);
void *ufbxc_memcpy(void *dst, const void *src, size_t count);
void *ufbxc_memmove(void *dst, const void *src, size_t count);
void *ufbxc_memset(void *dst, int ch, size_t count);
const void *ufbxc_memchr(const void *ptr, int value, size_t num);
int ufbxc_memcmp(const void *a, const void *b, size_t count);
int ufbxc_strcmp(const char *a, const char *b);
int ufbxc_strncmp(const char *a, const char *b, size_t count);

#define strlen ufbxc_strlen
#define memcpy ufbxc_memcpy
#define memmove ufbxc_memmove
#define memset ufbxc_memset
#define memchr ufbxc_memchr
#define memcmp ufbxc_memcmp
#define strcmp ufbxc_strcmp
#define strncmp ufbxc_strncmp

// Implement these to use this.

// <stdlib.h>

float ufbxc_strtof(const char *str, char **end);
double ufbxc_strtod(const char *str, char **end);

// Only called with base=10 or base=16
unsigned long ufbxc_strtoul(const char *str, char **end, int base);

void ufbxc_qsort(void *ptr, size_t count, size_t size, int (*cmp_fn)(const void*, const void*));
#define strtof ufbxc_strtof
#define strtod ufbxc_strtod
#define strtoul ufbxc_strtoul
#define qsort ufbxc_qsort

// Optional: Global memory allocator may be disabled
#if defined(UFBXC_HAS_MALLOC)
	void *ufbxc_malloc(size_t size);
	void *ufbxc_realloc(void *ptr, size_t new_size);
	void ufbxc_free(void *ptr);

	#define malloc ufbxc_malloc
	#define realloc ufbxc_realloc
	#define free ufbxc_free
#else
	#define UFBX_NO_MALLOC
#endif

// <stdio.h>

// Required specifiers: %s  %.*s  %u  %zu
int ufbxc_vsnprintf(char *buffer, size_t count, const char *format, va_list args);

#define vsnprintf ufbxc_vsnprintf

// Optional: Needed by both STDIO and STDERR
#if defined(UFBXC_HAS_STDIO) || defined(UFBXC_HAS_STDERR)
	typedef struct ufbxc_file ufbxc_file;
	#define FILE ufbxc_file
#endif

// Optional: Used for opening files
#if defined(UFBXC_HAS_STDIO)
	#define fpos_t int64_t

	FILE *ufbxc_fopen(const char *filename, const char *mode);
	size_t ufbxc_fread(void *buffer, size_t size, size_t count, FILE *f);
	void ufbxc_fclose(FILE *f);
	long ufbxc_ftell(FILE *f);
	int ufbxc_ferror(FILE *f);
	int ufbxc_fseek(FILE *f, long offset, int origin);
	int ufbxc_fgetpos(FILE *f, fpos_t *pos);
	int ufbxc_fsetpos(FILE *f, const fpos_t *pos);
	void ufbxc_rewind(FILE *f);

	#define SEEK_CUR 1
	#define SEEK_END 2

	#define fopen ufbxc_fopen
	#define fread ufbxc_fread
	#define fclose ufbxc_fclose
	#define ftell ufbxc_ftell
	#define ferror ufbxc_ferror
	#define fseek ufbxc_fseek
	#define fgetpos ufbxc_fgetpos
	#define fsetpos ufbxc_fsetpos
	#define rewind ufbxc_rewind
#else
	#define UFBX_NO_STDIO
#endif

// Optional: Used for error logging
#if defined(UFBXC_HAS_STDERR)
	#define stderr ((FILE*)1)

	// Required specifiers: %s
	// file is always specified to `stderr`
	void ufbxc_fprintf(FILE *file, const char *fmt, ...);

	#define fprintf ufbxc_fprintf
#else
	#define UFBX_NO_STDERR
#endif

#if defined(__cplusplus)
}
#endif

#endif
