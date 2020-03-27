#ifndef RHMAP_H_INCLUDED
#define RHMAP_H_INCLUDED

#ifndef RHMAP_NO_STDINT
#include <stddef.h>
#include <stdint.h>
#endif

typedef struct rhmap_s {
	uint64_t *entries;
	uint32_t mask;

	uint32_t capacity;
	uint32_t size;
} rhmap;

typedef struct rhmap_iter_s {
	rhmap *map;
	uint32_t hash;

	uint32_t scan;
} rhmap_iter;

void rhmap_init(rhmap *map);
void *rhmap_reset(rhmap *map);
void rhmap_clear(rhmap *map);

void rhmap_grow(rhmap *map, size_t *count, size_t *alloc_size, size_t min_size, double load_factor);
void rhmap_shrink(rhmap *map, size_t *count, size_t *alloc_size, size_t min_size, double load_factor);
void *rhmap_rehash(rhmap *map, size_t count, size_t alloc_size, void *data_ptr);

int rhmap_find(rhmap_iter *iter, uint32_t *value);
void rhmap_insert(rhmap_iter *iter, uint32_t value);
void rhmap_remove(rhmap_iter *iter);
void rhmap_set(rhmap_iter *iter, uint32_t value);
int rhmap_next(rhmap_iter *iter, uint32_t *hash, uint32_t *value);

void rhmap_update(rhmap *map, uint32_t hash, uint32_t old_value, uint32_t new_value);

#endif

// -- Inline rhmap.h

#if defined(RHMAP_IMPLEMENTATION) || defined(RHMAP_INLINE)
#if (defined(RHMAP_IMPLEMENTATION) && !defined(RHMAP_H_IMPLEMENTED)) || (defined(RHMAP_INLINE) && !defined(RHMAP_H_INLINED))
#ifdef RHMAP_IMPLEMENTATION
#define RHMAP_H_IMPLEMENTED
#endif
#ifdef RHMAP_INLINE
#define RHMAP_H_INLINED
#endif

#ifndef RHMAP_MEMSET
#ifdef RHMAP_NO_STDLIB
static void rhmap_imp_memset(void *data, int value, size_t num)
{
	uint64_t *ptr = (uint64_t*)data, *end = ptr + num / 8;
	while (ptr != end) *ptr++ = 0;
}
#define RHMAP_MEMSET rhmap_imp_memset
#else
#include <string.h>
#define RHMAP_MEMSET memset
#endif
#endif

#ifdef RHMAP_INLINE
RHMAP_INLINE void rhmap_init_inline(rhmap *map)
#else
void rhmap_init(rhmap *map)
#endif
{
	map->entries = 0;
	map->mask = map->capacity = map->size = 0;
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void *rhmap_reset_inline(rhmap *map)
#else
void *rhmap_reset(rhmap *map)
#endif
{
	void *data = map->entries;
	map->entries = 0;
	map->mask = map->capacity = map->size = 0;
	return data;
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void rhmap_clear_inline(rhmap *map)
#else
void rhmap_clear(rhmap *map)
#endif
{
	map->size = 0;
	RHMAP_MEMSET(map->entries, 0, sizeof(uint64_t) * (map->mask + 1));
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void rhmap_grow_inline(rhmap *map, size_t *count, size_t *alloc_size, size_t min_size, double load_factor)
#else
void rhmap_grow(rhmap *map, size_t *count, size_t *alloc_size, size_t min_size, double load_factor)
#endif
{
	size_t num_entries = map->mask + 1;
	size_t size = (size_t)((double)num_entries * load_factor);
	if (min_size < map->capacity + 1) min_size = map->capacity + 1;
	while (size < min_size) {
		num_entries *= 2;
		size = (size_t)((double)num_entries * load_factor);
	}
	*count = size;
	*alloc_size = num_entries * sizeof(uint64_t);
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void rhmap_shrink_inline(rhmap *map, size_t *count, size_t *alloc_size, size_t min_size, double load_factor)
#else
void rhmap_shrink(rhmap *map, size_t *count, size_t *alloc_size, size_t min_size, double load_factor)
#endif
{
	size_t num_entries = 1;
	size_t size = (size_t)((double)num_entries * load_factor);
	if (min_size < map->size) min_size = map->size;
	while (size < min_size) {
		num_entries *= 2;
		size = (size_t)((double)num_entries * load_factor);
	}
	*count = size;
	*alloc_size = num_entries * sizeof(uint64_t);
}

#ifdef RHMAP_INLINE
RHMAP_INLINE int rhmap_find_inline(rhmap_iter *iter, uint32_t *value)
#else
int rhmap_find(rhmap_iter *iter, uint32_t *value)
#endif
{
	rhmap *map = iter->map;
	uint64_t *entries = map->entries;
	uint32_t mask = map->mask, hash = iter->hash, scan = iter->scan;
	uint32_t ref = hash & ~mask;
	if (!mask) return 0;
	for (;;) {
		uint64_t entry = entries[(hash + scan) & mask];
		scan += 1;
		if ((uint32_t)entry == ref + scan) {
			iter->scan = scan;
			*value = (uint32_t)(entry >> 32u);
			return 1;
		} else if ((entry & mask) < scan) {
			iter->scan = scan - 1;
			return 0;
		}
	}
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void rhmap_insert_inline(rhmap_iter *iter, uint32_t value)
#else
void rhmap_insert(rhmap_iter *iter, uint32_t value)
#endif
{
	rhmap *map = iter->map;
	uint64_t *entries = map->entries;
	uint32_t mask = map->mask, hash = iter->hash, scan = iter->scan;
	uint32_t slot = (hash + scan) & mask;
	uint64_t entry, new_entry = (uint64_t)value << 32u | (hash & ~mask);
	scan += 1;
	while ((entry = entries[slot]) != 0) {
		uint32_t entry_scan = (entry & mask);
		if (entry_scan < scan) {
			entries[slot] = new_entry + scan;
			new_entry = (entry & ~(uint64_t)mask);
			scan = entry_scan;
		}
		scan += 1;
		slot = (slot + 1) & mask;
	}
	entries[slot] = new_entry + scan;
	map->size++;
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void *rhmap_rehash_inline(rhmap *map, size_t count, size_t alloc_size, void *data_ptr)
#else
void *rhmap_rehash(rhmap *map, size_t count, size_t alloc_size, void *data_ptr)
#endif
{
	rhmap old_map = *map;
	size_t num_entries = alloc_size / sizeof(uint64_t);
	uint64_t *entries = (uint64_t*)data_ptr;
	uint32_t mask = old_map.mask;
	map->entries = entries;
	map->mask = (uint32_t)(num_entries) - 1;
	map->capacity = (uint32_t)count;
	map->size = 0;
	RHMAP_MEMSET(entries, 0, sizeof(uint64_t) * num_entries);
	if (old_map.mask) {
		uint32_t i;
		for (i = 0; i <= old_map.mask; i++) {
			uint64_t entry = old_map.entries[i];
			if (entry) {
				uint32_t scan = (uint32_t)(entry & mask) - 1;
				uint32_t hash = ((uint32_t)entry & ~mask) | ((i - scan) & mask);
				uint32_t value = (uint32_t)(entry >> 32u);
				rhmap_iter iter = { map, hash };
				#ifdef RHMAP_INLINE
					rhmap_insert_inline(&iter, value);
				#else
					rhmap_insert(&iter, value);
				#endif
			}
		}
	}
	return old_map.entries;
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void rhmap_remove_inline(rhmap_iter *iter)
#else
void rhmap_remove(rhmap_iter *iter)
#endif
{
	rhmap *map = iter->map;
	uint64_t *entries = map->entries;
	uint32_t mask = map->mask, hash = iter->hash, scan = iter->scan;
	uint32_t slot = (hash + scan - 1) & mask;
	iter->scan = scan - 1;
	for (;;) {
		uint32_t next_slot = (slot + 1) & mask;
		uint64_t next_entry = entries[next_slot];
		uint32_t next_scan = (next_entry & mask);
		if (next_scan <= 1) break;
		entries[slot] = next_entry - 1;
		slot = next_slot;
	}
	entries[slot] = 0;
	map->size--;
}

#ifdef RHMAP_INLINE
RHMAP_INLINE int rhmap_next_inline(rhmap_iter *iter, uint32_t *hash, uint32_t *value)
#else
int rhmap_next(rhmap_iter *iter, uint32_t *hash, uint32_t *value)
#endif
{
	rhmap *map = iter->map;
	uint64_t *entries = map->entries;
	uint32_t mask = map->mask;
	uint32_t scan = (iter->hash & mask) + iter->scan;
	if (!mask) return 0;
	while (scan != mask + 1) {
		uint64_t entry = entries[scan & mask];
		scan += 1;
		if (entry) {
			uint32_t ref_scan = (uint32_t)(entry & mask) - 1;
			uint32_t ref_hash = ((uint32_t)entry & ~mask) | ((scan - ref_scan - 1) & mask);
			*hash = ref_hash;
			*value = (uint64_t)entry >> 32u;
			iter->hash = ref_hash;
			iter->scan = ref_scan + 1;
			return 1;
		}
	}
	return 0;
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void rhmap_set_inline(rhmap_iter *iter, uint32_t value)
#else
void rhmap_set(rhmap_iter *iter, uint32_t value)
#endif
{
	rhmap *map = iter->map;
	uint32_t mask = map->mask, hash = iter->hash, scan = iter->scan;
	uint32_t slot = (hash + scan - 1) & mask;
	uint64_t *entries = map->entries;
	entries[slot] = (entries[slot] & 0xffffffffu) | (uint64_t)value << 32u;
}

#ifdef RHMAP_INLINE
RHMAP_INLINE void rhmap_update_inline(rhmap *map, uint32_t hash, uint32_t old_value, uint32_t new_value)
#else
void rhmap_update(rhmap *map, uint32_t hash, uint32_t old_value, uint32_t new_value)
#endif
{
	uint64_t *entries = map->entries;
	uint32_t mask = map->mask, scan = 0;
	uint64_t old_entry = (uint64_t)old_value << 32u | (hash & ~mask);
	uint64_t new_entry = (uint64_t)new_value << 32u | (hash & ~mask);
	for (;;) {
		uint32_t slot = (hash + scan) & mask;
		scan += 1;
		if (entries[slot] == old_entry + scan) {
			entries[slot] = new_entry + scan;
			return;
		}
	}
}

#endif
#endif

