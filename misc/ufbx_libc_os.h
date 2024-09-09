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
	// Read and return file contents.
	void *ufbxc_os_read_file(size_t index, const char *filename, size_t *p_size);

	// Free previously read file.
	void ufbxc_os_free_file(size_t index, void *data);
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

