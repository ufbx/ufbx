#ifndef UFBX_UFBX_LIBC_C_INCLUDED
#define UFBX_UFBX_LIBC_C_INCLUDED

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

// -- string

ufbx_libc_abi size_t ufbx_strlen(const char *str)
{
	size_t length = 0;
	while (str[length]) {
		length++;
	}
	return length;
}

ufbx_libc_abi void *ufbx_memcpy(void *dst, const void *src, size_t count)
{
	char *d = (char*)dst;
	const char *s = (const char*)src;
	for (size_t i = 0; i < count; i++) {
		d[i] = s[i];
	}
	return dst;
}

ufbx_libc_abi void *ufbx_memmove(void *dst, const void *src, size_t count)
{
	char *d = (char*)dst;
	const char *s = (const char*)src;
	if ((uintptr_t)d < (uintptr_t)s) {
		for (size_t i = 0; i < count; i++) {
			d[i] = s[i];
		}
	} else {
		for (size_t i = count; i-- > 0; ) {
			d[i] = s[i];
		}
	}
	return dst;
}

ufbx_libc_abi void *ufbx_memset(void *dst, int ch, size_t count)
{
	char *d = (char*)dst;
	char c = (char)ch;
	for (size_t i = 0; i < count; i++) {
		d[i] = c;
	}
	return dst;
}

ufbx_libc_abi const void *ufbx_memchr(const void *ptr, int value, size_t num)
{
	const char *p = (const char*)ptr;
	char c = (char)value;
	for (size_t i = 0; i < num; i++) {
		if (p[i] == c) {
			return p + i;
		}
	}
	return NULL;
}

ufbx_libc_abi int ufbx_memcmp(const void *a, const void *b, size_t count)
{
	const char *pa = (const char*)a;
	const char *pb = (const char*)b;
	for (size_t i = 0; i < count; i++) {
		if (pa[i] != pb[i]) {
			return (unsigned char)pa[i] < (unsigned char)pb[i] ? -1 : 1;
		}
	}
	return 0;
}

ufbx_libc_abi int ufbx_strcmp(const char *a, const char *b)
{
	const char *pa = (const char*)a;
	const char *pb = (const char*)b;
	for (size_t i = 0; ; i++) {
		if (pa[i] != pb[i]) {
			return (unsigned char)pa[i] < (unsigned char)pb[i] ? -1 : 1;
		} else if (pa[i] == 0) {
			return 0;
		}
	}
}

ufbx_libc_abi int ufbx_strncmp(const char *a, const char *b, size_t count)
{
	const char *pa = (const char*)a;
	const char *pb = (const char*)b;
	for (size_t i = 0; i < count; i++) {
		if (pa[i] != pb[i]) {
			return (unsigned char)pa[i] < (unsigned char)pb[i] ? -1 : 1;
		} else if (pa[i] == 0) {
			return 0;
		}
	}
	return 0;
}

// -- malloc

#if !defined(UFBX_NO_MALLOC)

ufbx_libc_extern_abi void *ufbx_libc_allocate(size_t size, size_t *p_allocated_size);
ufbx_libc_extern_abi bool ufbx_libc_free(void *pointer, size_t allocated_size);

#define UFBXI_MALLOC_FREE 0x1
#define UFBXI_MALLOC_USED 0x2
#define UFBXI_MALLOC_BLOCK 0x4

typedef struct ufbxi_malloc_node {
	struct ufbxi_malloc_node *prev_all, *next_all;
	struct ufbxi_malloc_node **prev_next_free, *next_free;
	size_t flags;
	size_t size;
} ufbxi_malloc_node;

#define UFBXI_MALLOC_NUM_SIZE_CLASSES 32

static ufbxi_malloc_node ufbxi_malloc_root;
static ufbxi_malloc_node *ufbxi_malloc_free_list[UFBXI_MALLOC_NUM_SIZE_CLASSES];

static size_t ufbxi_malloc_size_class(size_t size)
{
	size_t sc = 0;
	while (size > 16 && sc + 1 < UFBXI_MALLOC_NUM_SIZE_CLASSES) {
		sc++;
		size /= 2;
	}
	return sc;
}

static void ufbxi_malloc_link(ufbxi_malloc_node *prev, ufbxi_malloc_node *next)
{
	if (prev->next_all) prev->next_all->prev_all = next;
	next->next_all = prev->next_all;
	next->prev_all = prev;
	prev->next_all = next;
}

static void ufbxi_malloc_unlink(ufbxi_malloc_node *node)
{
	ufbxi_malloc_node *prev = node->prev_all, *next = node->next_all;
	if (next) next->prev_all = prev;
	prev->next_all = next;
	node->prev_all = node->next_all = NULL;
}

static void ufbxi_malloc_create(ufbxi_malloc_node *prev, ufbxi_malloc_node *node, size_t size, uint32_t flags)
{
	ufbxi_malloc_link(prev, node);
	node->size = size;
	node->flags = flags;
	node->prev_next_free = NULL;
	node->next_free = NULL;
}

static void ufbxi_malloc_link_free(ufbxi_malloc_node *node)
{
	size_t sc = ufbxi_malloc_size_class(node->size);
	ufbxi_malloc_node **p_free = &ufbxi_malloc_free_list[sc];
	ufbxi_malloc_node *free_node = *p_free;
	if (free_node) free_node->prev_next_free = &node->next_free;
	node->prev_next_free = p_free;
	node->next_free = *p_free;
	*p_free = node;
}

static void ufbxi_malloc_unlink_free(ufbxi_malloc_node *node)
{
	if (node->next_free) node->next_free->prev_next_free = node->prev_next_free;
	*node->prev_next_free = node->next_free;
	node->prev_next_free = NULL;
	node->next_free = NULL;
}

static bool ufbxi_malloc_block_end(ufbxi_malloc_node *node)
{
	if (!node) return true;
	if (!node->flags || node->flags == UFBXI_MALLOC_BLOCK) return true;
	return false;
}

ufbx_libc_abi void *ufbx_malloc(size_t size)
{
	if (size == 0) {
		size = 1;
	}

	size_t align = 2 * sizeof(void*);
	size = (size + align - 1) & ~(align - 1);

	ufbxi_malloc_node *node = NULL;

	size_t search_attempts = 16;
	size_t sc = ufbxi_malloc_size_class(size);
	for (; !node && sc < UFBXI_MALLOC_NUM_SIZE_CLASSES; sc++) {
		ufbxi_malloc_node *free_node = ufbxi_malloc_free_list[sc];
		if (!free_node) continue;

		for (size_t i = 0; i < search_attempts; i++) {
			if (!free_node) break;

			if (free_node->size >= size) {
				node = free_node;
				ufbxi_malloc_unlink_free(node);
				break;
			}

			free_node = free_node->next_free;
		}
	}

	if (node == NULL) {
		size_t allocated_size = 2 * sizeof(ufbxi_malloc_node) + size + align;
		void *memory = ufbx_libc_allocate(allocated_size, &allocated_size);
		if (!memory) return NULL;

		// Can't do anything if the memory is too small
		if (allocated_size <= 2 * sizeof(ufbxi_malloc_node) + align) {
			ufbx_libc_free(memory, allocated_size);
			return NULL;
		}

		size_t align_bytes = (align - (uintptr_t)memory % align) % align;
		char *aligned_memory = (char*)memory + align_bytes;
		size_t aligned_size = allocated_size - align_bytes;
		size_t free_space = aligned_size - sizeof(ufbxi_malloc_node) * 2;

		ufbxi_malloc_node *header = (ufbxi_malloc_node*)aligned_memory;
		ufbxi_malloc_node *block = header + 1;

		ufbxi_malloc_create(&ufbxi_malloc_root, header, allocated_size, UFBXI_MALLOC_BLOCK);
		header->next_free = memory;

		ufbxi_malloc_create(header, block, free_space, UFBXI_MALLOC_FREE);

		if (free_space >= size) {
			node = block;
		} else {
			return NULL;
		}
	}

	if (node == NULL) return NULL;

	char *data = (char*)(node + 1);

	size_t slack = node->size - size;
	if (slack >= 2 * sizeof(ufbxi_malloc_node)) {
		ufbxi_malloc_node *next = (ufbxi_malloc_node*)(data + size);
		ufbxi_malloc_create(node, next, slack - sizeof(ufbxi_malloc_node), UFBXI_MALLOC_FREE);
		ufbxi_malloc_link_free(next);
		node->size = size;
	}

	node->flags = UFBXI_MALLOC_USED;
	return data;
}

ufbx_libc_abi void ufbx_free(void *ptr, size_t old_size)
{
	ufbxi_malloc_node *node = (ufbxi_malloc_node*)ptr - 1;
	node->flags = UFBXI_MALLOC_FREE;

	while (node->prev_all->flags == UFBXI_MALLOC_FREE) {
		node = node->prev_all;
		node->size += node->next_all->size + sizeof(ufbxi_malloc_node);
		ufbxi_malloc_unlink_free(node);
		ufbxi_malloc_unlink(node->next_all);
	}
	while (node->next_all && node->next_all->flags == UFBXI_MALLOC_FREE) {
		node->size += node->next_all->size + sizeof(ufbxi_malloc_node);
		ufbxi_malloc_unlink_free(node->next_all);
		ufbxi_malloc_unlink(node->next_all);
	}

	if (node->prev_all->flags == UFBXI_MALLOC_BLOCK && ufbxi_malloc_block_end(node->next_all)) {
		ufbxi_malloc_node *header = node->prev_all;
		ufbxi_malloc_unlink(header);
		ufbxi_malloc_unlink(node);
		if (!ufbx_libc_free(header->next_free, header->size)) {
			ufbxi_malloc_link(&ufbxi_malloc_root, header);
			ufbxi_malloc_link(header, node);
		} else {
			node = NULL;
		}
	}

	if (node) {
		ufbxi_malloc_link_free(node);
	}
}

ufbx_libc_abi void *ufbx_realloc(void *ptr, size_t old_size, size_t new_size)
{
	void *new_ptr = ufbx_malloc(new_size);
	if (!new_ptr) return NULL;
	ufbx_memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
	return new_ptr;
}

#endif

#if !defined(UFBX_NO_STDIO)

ufbx_libc_extern_abi bool ufbx_libc_open_file(size_t index, const char *filename, size_t filename_len, void **p_handle, uint64_t *p_file_size);
ufbx_libc_extern_abi size_t ufbx_libc_read_file(size_t index, void *handle, void *dst, uint64_t offset, uint32_t count, uint32_t skipped_bytes);
ufbx_libc_extern_abi void ufbx_libc_close_file(size_t index, void *handle);

typedef struct {
	bool used;
	uint64_t last_read;
	uint64_t position;
	uint64_t size;
	void *handle;
} ufbxi_libc_file;

#define UFBXI_LIBC_MAX_FILES 128

ufbxi_libc_file ufbxi_libc_files[UFBXI_LIBC_MAX_FILES];

ufbx_libc_abi void *ufbx_stdio_open(const char *path, size_t path_len)
{
	char path_copy[512];
	if (path_len + 1 >= sizeof(path_copy)) return NULL;

	ufbx_memcpy(path_copy, path, path_len);
	path_copy[path_len] = '\0';

	for (uint32_t i = 0; i < UFBXI_LIBC_MAX_FILES; i++) {
		ufbxi_libc_file *file = &ufbxi_libc_files[i];
		if (!file->used) {
			file->size = UINT64_MAX;
			bool ok = ufbx_libc_open_file(i, path_copy, path_len, &file->handle, &file->size);
			if (!ok) return NULL;
			file->used = true;
			return file;
		}
	}
	return NULL;
}

ufbx_libc_abi size_t ufbx_stdio_read(void *file, void *data, size_t size)
{
	ufbxi_libc_file *f = (ufbxi_libc_file*)file;
	size_t index = (size_t)(f - ufbxi_libc_files);

	size_t offset = 0;
	size_t num_left = size;

	if (f->size != UINT64_MAX) {
		uint64_t max_left = f->size - f->position;
		if (num_left > max_left) {
			num_left = (size_t)max_left;
		}
	}

	while (num_left > 0) {
		uint32_t to_read = (uint32_t)(num_left < 0x40000000 ? num_left : 0x40000000u);
		uint32_t skipped = (uint32_t)(f->position - f->last_read);
		size_t num_read = ufbx_libc_read_file(index, f->handle, (char*)data + offset, f->position, to_read, skipped);
		if (num_read == 0) break;
		if (num_read == SIZE_MAX) return SIZE_MAX;

		f->position += num_read;
		f->last_read = f->position;
		offset += num_read;
		num_left -= num_read;
	}

	return offset;
}

ufbx_libc_abi bool ufbx_stdio_skip(void *file, size_t size)
{
	ufbxi_libc_file *f = (ufbxi_libc_file*)file;
	uint64_t target_pos = f->position + size;
	while (f->position < target_pos) {
		uint64_t max_pos = f->last_read + 0x40000000;
		if (target_pos <= max_pos) {
			f->position = target_pos;
			break;
		} else {
			f->position = max_pos;
			char byte[1];
			if (ufbx_stdio_read(file, byte, 1) != 1) return false;
		}
	}
	return true;
}

ufbx_libc_abi uint64_t ufbx_stdio_size(void *file)
{
	ufbxi_libc_file *f = (ufbxi_libc_file*)file;
	return f->size != UINT64_MAX ? f->size : 0;
}

ufbx_libc_abi void ufbx_stdio_close(void *file)
{
	ufbxi_libc_file *f = (ufbxi_libc_file*)file;
	size_t index = (size_t)(f - ufbxi_libc_files);
	ufbx_libc_close_file(index, f->handle);
	ufbx_memset(f, 0, sizeof(ufbxi_libc_file));
}

#endif

#if defined(__cplusplus)
}
#endif

#endif

