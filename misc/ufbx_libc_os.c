#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#if !defined(UFBX_NO_MALLOC)

static uint64_t ufbxi_win32_total_memory = 0;

void *ufbx_libc_allocate(size_t size, size_t *p_allocated_size)
{
	size_t min_size = 4*1024*1024;
	size_t alloc_size = size > min_size ? size : min_size;
	void *pointer = VirtualAlloc(NULL, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!pointer) return NULL;

	ufbxi_win32_total_memory += alloc_size;

	*p_allocated_size = alloc_size;
	return pointer;
}

bool ufbx_libc_free(void *pointer, size_t allocated_size)
{
	if (ufbxi_win32_total_memory >= 32*1024*1024) {
		ufbxi_win32_total_memory -= allocated_size;
		VirtualFree(pointer, 0, MEM_RELEASE);
		return TRUE;
	} else {
		return FALSE;
	}
}

#endif

#if !defined(UFBX_NO_STDIO)

#define UFBXI_WIN32_MAX_FILENAME_WCHAR 1024

bool ufbx_libc_open_file(size_t index, const char *filename, size_t filename_len, void **p_handle, uint64_t *p_file_size)
{
	WCHAR filename_wide[UFBXI_WIN32_MAX_FILENAME_WCHAR + 1] = { 0 };
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, filename_wide, UFBXI_WIN32_MAX_FILENAME_WCHAR);

	HANDLE file = CreateFileW(filename_wide, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == NULL || file == INVALID_HANDLE_VALUE) return false;

	DWORD size_hi = 0;
	DWORD size_lo = GetFileSize(file, &size_hi);

	*p_file_size = (uint64_t)size_hi << 32u | size_lo;
	*p_handle = file;
	return true;
}

size_t ufbx_libc_read_file(size_t index, void *handle, void *dst, uint64_t offset, uint32_t count, uint32_t skipped_bytes)
{
	HANDLE h = (HANDLE)handle;
	if (skipped_bytes > 0) {
		if (!SetFilePointer(h, skipped_bytes, NULL, FILE_CURRENT)) return SIZE_MAX;
	}

	DWORD read_count;
	if (!ReadFile(h, dst, (DWORD)count, &read_count, NULL)) return SIZE_MAX;
	return read_count;
}

void ufbx_libc_close_file(size_t index, void *handle)
{
	HANDLE h = (HANDLE)handle;
	CloseHandle(h);
}

#endif

#endif
