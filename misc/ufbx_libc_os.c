#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#if defined(UFBXC_HAS_MALLOC)

static size_t total_allocated = 0;

void *ufbxc_os_allocate(size_t size, size_t *p_allocated_size)
{
	size_t min_size = 4*1024*1024;
	size_t alloc_size = size > min_size ? size : min_size;
	void *pointer = VirtualAlloc(NULL, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!pointer) return NULL;

	total_allocated += alloc_size;
	*p_allocated_size = alloc_size;
	return pointer;
}

bool ufbxc_os_free(void *pointer, size_t allocated_size)
{
	if (total_allocated <= 64*1024*1024) return false;

	total_allocated -= allocated_size;
	VirtualFree(pointer, 0, MEM_RELEASE);
	return TRUE;
}

#endif

#if defined(UFBXC_HAS_STDIO)

#define MAX_OPEN_FILES 128

typedef struct {
	HANDLE file;
	HANDLE mapping;
	void *pointer;
} win32_file;

static win32_file files[MAX_OPEN_FILES];

#define MAX_FILENAME_WCHAR 1024

void *ufbxc_os_read_file(size_t index, const char *filename, size_t *p_size)
{
	if (index >= MAX_OPEN_FILES) return NULL;

	WCHAR filename_wide[MAX_FILENAME_WCHAR + 1] = { 0 };
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, filename_wide, MAX_FILENAME_WCHAR);

	HANDLE file = CreateFileW(filename_wide, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == NULL || file == INVALID_HANDLE_VALUE) return NULL;

	DWORD size_hi = 0;
	DWORD size_lo = GetFileSize(file, &size_hi);

	HANDLE mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, size_hi, size_lo, NULL);
	if (mapping == NULL || mapping ==  INVALID_HANDLE_VALUE) {
		CloseHandle(file);
		return NULL;
	}

	size_t size = (size_t)((uint64_t)size_hi << 32u | size_lo);
	void *pointer = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size);
	if (!pointer) {
		CloseHandle(file);
		CloseHandle(mapping);
		return NULL;
	}

	win32_file *f = &files[index];
	ZeroMemory(f, sizeof(win32_file));

	f->file = file;
	f->mapping = mapping;
	f->pointer = pointer;

	*p_size = size;
	return pointer;
}

void ufbxc_os_free_file(size_t index, void *data)
{
	win32_file *f = &files[index];
	UnmapViewOfFile(f->pointer);
	CloseHandle(f->mapping);
	CloseHandle(f->file);
	ZeroMemory(f, sizeof(win32_file));
}

#endif

#if defined(UFBXC_HAS_STDERR)

#define MAX_ERROR_WCHAR 1024

void ufbxc_os_print_error(const char *message, size_t length)
{
	WCHAR message_wide[MAX_ERROR_WCHAR + 1] = { 0 };
	MultiByteToWideChar(CP_UTF8, 0, message, -1, message_wide, MAX_ERROR_WCHAR);
	OutputDebugStringW(message_wide);
}

#endif

#if defined(UFBXC_HAS_EXIT)

void ufbxc_os_exit(int code)
{
	ExitProcess((UINT)code);
}

#endif

#endif
