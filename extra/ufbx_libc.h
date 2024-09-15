#ifndef UFBX_UFBX_LIBC_H_INCLUDED
#define UFBX_UFBX_LIBC_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef ufbx_libc_abi
#define ufbx_libc_abi
#endif

#ifndef ufbx_libc_extern_abi
#define ufbx_libc_extern_abi
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// -- API used by ufbx

ufbx_libc_abi size_t ufbx_strlen(const char *str);
ufbx_libc_abi void *ufbx_memcpy(void *dst, const void *src, size_t count);
ufbx_libc_abi void *ufbx_memmove(void *dst, const void *src, size_t count);
ufbx_libc_abi void *ufbx_memset(void *dst, int ch, size_t count);
ufbx_libc_abi const void *ufbx_memchr(const void *ptr, int value, size_t count);
ufbx_libc_abi int ufbx_memcmp(const void *a, const void *b, size_t count);
ufbx_libc_abi int ufbx_strcmp(const char *a, const char *b);
ufbx_libc_abi int ufbx_strncmp(const char *a, const char *b, size_t count);

#if !defined(UFBX_NO_MALLOC)
	ufbx_libc_abi void *ufbx_malloc(size_t size);
	ufbx_libc_abi void *ufbx_realloc(void *ptr, size_t old_size, size_t new_size);
	ufbx_libc_abi void *ufbx_free(void *ptr, size_t old_size);
#endif

#if !defined(UFBX_NO_STDIO)
	ufbx_libc_abi void *ufbx_stdio_open(const char *path, size_t path_len);
	ufbx_libc_abi size_t ufbx_stdio_read(void *file, void *data, size_t size);
	ufbx_libc_abi bool ufbx_stdio_skip(void *file, size_t size);
	ufbx_libc_abi uint64_t ufbx_stdio_size(void *file);
	ufbx_libc_abi void ufbx_stdio_close(void *file);
#endif

// -- External functions that must be implemented by the user

#if !defined(UFBX_NO_MALLOC)
	// Minimal implementation of this API could be:
	//
	// char memory[16*1024*1024];
	// bool allocated = false;
	// void *ufbx_libc_allocate(size_t size, size_t *p_allocated_size) {
	//     if (allocated) return false;
	//     allocated = true;
	//     *p_allocated_size = sizeof(memory);
	//     return memory;
	// }
	// bool ufbx_libc_free() {
	//     return false;
	// }
	//

	// Allocate memory from the OS, you can return more memory than requested via `p_allocated_size`.
	void *ufbx_libc_allocate(size_t size, size_t *p_allocated_size);

	// Free memory returned by `ufbxc_os_allocate()`.
	// Return `true` if the memory was freed. If you return `false` ufbxc will re-use the memory.
	bool ufbx_libc_free(void *pointer, size_t allocated_size);
#endif

#if !defined(UFBX_NO_STDIO)
	// Minimal example implementation with FILE-style API
	//
	// bool ufbx_libc_open_file(size_t index, const char *filename, size_t filename_len, void **p_handle, uint64_t *p_file_size) {
	//     *p_handle = fopen(filename, "rb");
	//     return *p_handle != NULL;
	// }
	// size_t ufbx_libc_read_file(size_t index, void *handle, void *dst, uint64_t offset, uint32_t count, uint32_t skipped_bytes) {
	//     return fread(dst, 1, count, (FILE*)handle);
	// }
	// void ufbx_libc_close_file(size_t index, void *handle) {
	//     fclose((FILE*)handle);
	// }

	// Minimal example implementation with memory-backed files
	//
	// bool ufbx_libc_open_file(size_t index, const char *filename, size_t filename_len, void **p_handle, uint64_t *p_file_size) {
	//     size_t size;
	//     void *memory = read_file(filename, &size);
	//     if (!memory) return false;
	//     *p_handle = memory;
	//     *p_file_size = size;
	//     return true;
	// }
	// size_t ufbx_libc_read_file(size_t index, void *handle, void *dst, uint64_t offset, uint32_t count, uint32_t skipped_bytes) {
	//     memcpy(dst, (char*)handle + (size_t)offset, count);
	// }
	// void ufbx_libc_close_file(size_t index, void *handle) {
	//     free(handle);
	// }

	// Open a file for reading.
	// You may optionally specify the file size `p_file_size` which changes the read behavior.
	// `filename` is always null-terminated.
	bool ufbx_libc_open_file(size_t index, const char *filename, size_t filename_len, void **p_handle, uint64_t *p_file_size);

	// Read `count` bytes from the file into `dst`.
	// `offset` is guaranteed to be `[prev](offset + count) + skipped_bytes`.
	// If `p_file_size` specified: `offset + count` is guaranteed to be in bounds of the reported file size.
	// If `p_file_size` not specified: `skipped_bytes` will always be zero..
	size_t ufbx_libc_read_file(size_t index, void *handle, void *dst, uint64_t offset, uint32_t count, uint32_t skipped_bytes);

	// Close previously opened file.
	void ufbx_libc_close_file(size_t index, void *handle);
#endif

#if defined(__cplusplus)
}
#endif

#endif
