#ifndef UFBX_LIBC_OS_H
#define UFBX_LIBC_OS_H

#include <stddef.h>
#include <stdbool.h>

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

	// Free previously read file.
	void ufbxc_os_close_file(size_t index);
#endif

#if defined(UFBXC_HAS_STDERR)
	// Print error message
	void ufbxc_os_print_error(const char *message, size_t length);
#endif

#if defined(UFBXC_HAS_EXIT)
	// Exit the process
	void ufbxc_os_exit(int code);
#endif

#endif

