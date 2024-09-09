
#define UFBX_DEV
#define UFBX_NO_ASSERT
#include "../../ufbx.h"

#define UFBXC_HAS_MALLOC
#define UFBXC_HAS_STDERR
#define UFBXC_HAS_EXIT
#include "../ufbx_libc.h"

void ufbxt_assert_fail(const char *message);

#if defined(__wasm__)
	#define wasm_import(name) __attribute__((import_module("host"), import_name(name)))
	#define wasm_export(name) __attribute__((export_name(name)))
#else
	#define wasm_import(name)
	#define wasm_export(name)
#endif

#define ufbxt_assert(cond) do { if (!(cond)) ufbxt_assert_fail(#cond); } while (0)
#define ufbxh_assert(cond) ufbxt_assert(cond)

int ufbxh_snprintf(char *buffer, size_t size, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vsnprintf(buffer, size, fmt, args);
	va_end(args);
	return ret;
}

typedef struct {
	int unused;
} ufbxh_FILE;

int ufbxh_fprintf(ufbxh_FILE *f, ...)
{
	return 0;
}

int ufbxh_fflush(ufbxh_FILE *f)
{
	return 0;
}

#include "../../test/check_scene.h"
#include "../../test/hash_scene.h"

wasm_import("hostError") void host_error(const char *message, size_t length);
wasm_import("hostVerbose") void host_verbose(const char *message, size_t length);
wasm_import("hostExit") void host_exit(int code);

void verbosef(const char *fmt, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, fmt);
	int ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (ret > 0) {
		host_verbose(buffer, (size_t)ret);
	}
}

union {
	uint64_t align;
	char data[16*1024*1024];
} g_memory;

void *ufbxc_os_allocate(size_t size, size_t *p_allocated_size)
{
	*p_allocated_size = sizeof(g_memory.data);
	return g_memory.data;
}

bool ufbxc_os_free(void *pointer, size_t allocated_size)
{
	return false;
}

void ufbxc_os_print_error(const char *message, size_t length)
{
	host_error(message, length);
}

void ufbxc_os_exit(int code)
{
	host_exit(code);
}

void ufbxt_assert_fail(const char *message)
{
	ufbxc_fprintf(stderr, "ufbxc_assert() fail: %s\n", message);
	ufbxc_os_exit(1);
}

wasm_export("hashAlloc")
void *hash_alloc(size_t size)
{
	return malloc(size);
}

wasm_export("hashFree")
void hash_free(void *pointer)
{
	free(pointer);
}

wasm_export("hashScene")
int hash_scene(uint64_t *p_hash, const void *data, size_t size)
{
	ufbx_load_opts opts = { 0 };
	opts.load_external_files = true;
	opts.ignore_missing_external_files = true;
	opts.evaluate_caches = true;
	opts.evaluate_skinning = true;
	opts.target_axes = ufbx_axes_right_handed_y_up;
	opts.target_unit_meters = 1.0f;

	verbosef("Loading scene... %zu bytes", size);

	ufbx_error error;
	ufbx_scene *scene = ufbx_load_memory(data, size, &opts, &error);
	if (!scene) {
		char err[512];
		verbosef("Failed to load the scene: %u", error.type);
		ufbx_format_error(err, sizeof(err), &error);
		fprintf(stderr, "%s\n", err);
		return 1;
	}

	verbosef("Checking scene...");
	ufbxt_check_scene(scene);
	verbosef("Check OK!");

	verbosef("Hashing scene...");
	uint64_t hash = ufbxt_hash_scene(scene, NULL);

	ufbx_free_scene(scene);

	*p_hash = hash;

	return 0;
}

#include "../ufbx_libc.c"
#include "../fdlibm.c"
#include "../../ufbx.c"
