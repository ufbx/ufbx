#include "ufbx.h"

#ifndef UFBX_UFBX_C_INLCUDED
#define UFBX_UFBX_C_INLCUDED

// -- Configuration

#define UFBXI_MAX_ALLOCATION_SIZE 0x10000000

#define UFBXI_MAX_NON_ARRAY_VALUES 7

// -- Headers

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// -- Platform

#if defined(_MSC_VER)
	#define ufbxi_noinline __declspec(noinline)
	#define ufbxi_forceinline __forceinline
	#if defined(__cplusplus) && _MSC_VER >= 1900
		#define ufbxi_nodiscard [[nodiscard]]
	#else
		#define ufbxi_nodiscard _Check_return_
	#endif
#elif defined(__GNUC__) || defined(__clang__)
	#define ufbxi_noinline __attribute__((noinline))
	#define ufbxi_forceinline inline __attribute__((always_inline))
	#define ufbxi_nodiscard __attribute__((warn_unused_result))
#else
	#define ufbxi_noinline
	#define ufbxi_forceinline
	#define ufbxi_nodiscard
#endif

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable: 4200) // nonstandard extension used: zero-sized array in struct/union
	#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
	#pragma warning(disable: 4127) // conditional expression is constant
#endif

#ifndef ufbx_assert
	#include <assert.h>
	#define ufbx_assert(cond) assert(cond)
#endif

#if !defined(ufbx_static_assert)
	#if defined(__cplusplus) && __cplusplus >= 201103
		#define ufbx_static_assert(desc, cond) static_assert(cond, #desc ": " #cond)
	#else
		#define ufbx_static_assert(desc, cond) typedef char ufbxi_static_assert_##desc[(cond)?1:-1]
	#endif
#endif

// TODO: Unaligned loads for some platforms
#define ufbxi_read_u8(ptr) (*(const uint8_t*)(ptr))
#define ufbxi_read_u16(ptr) (*(const uint16_t*)(ptr))
#define ufbxi_read_u32(ptr) (*(const uint32_t*)(ptr))
#define ufbxi_read_u64(ptr) (*(const uint64_t*)(ptr))
#define ufbxi_read_f32(ptr) (*(const float*)(ptr))
#define ufbxi_read_f64(ptr) (*(const double*)(ptr))
#define ufbxi_read_i8(ptr) (int8_t)(ufbxi_read_u8(ptr))
#define ufbxi_read_i16(ptr) (int16_t)(ufbxi_read_u16(ptr))
#define ufbxi_read_i32(ptr) (int32_t)(ufbxi_read_u32(ptr))
#define ufbxi_read_i64(ptr) (int64_t)(ufbxi_read_u64(ptr))

#define ufbxi_write_u8(ptr, val) (*(uint8_t*)(ptr) = (uint8_t)(val))
#define ufbxi_write_u16(ptr, val) (*(uint16_t*)(ptr) = (uint16_t)(val))
#define ufbxi_write_u32(ptr, val) (*(uint32_t*)(ptr) = (uint32_t)(val))
#define ufbxi_write_u64(ptr, val) (*(uint64_t*)(ptr) = (uint64_t)(val))
#define ufbxi_write_f32(ptr, val) (*(float*)(ptr) = (float)(val))
#define ufbxi_write_f64(ptr, val) (*(double*)(ptr) = (double)(val))
#define ufbxi_write_i8(ptr, val) ufbxi_write_u8(ptr, val)
#define ufbxi_write_i16(ptr, val) ufbxi_write_u16(ptr, val)
#define ufbxi_write_i32(ptr, val) ufbxi_write_u32(ptr, val)

ufbx_static_assert(sizeof_bool, sizeof(bool) == 1);
ufbx_static_assert(sizeof_i8, sizeof(int8_t) == 1);
ufbx_static_assert(sizeof_i16, sizeof(int16_t) == 2);
ufbx_static_assert(sizeof_i32, sizeof(int32_t) == 4);
ufbx_static_assert(sizeof_i64, sizeof(int64_t) == 8);
ufbx_static_assert(sizeof_u8, sizeof(uint8_t) == 1);
ufbx_static_assert(sizeof_u16, sizeof(uint16_t) == 2);
ufbx_static_assert(sizeof_u32, sizeof(uint32_t) == 4);
ufbx_static_assert(sizeof_u64, sizeof(uint64_t) == 8);
ufbx_static_assert(sizeof_f32, sizeof(float) == 4);
ufbx_static_assert(sizeof_f64, sizeof(double) == 8);

// -- Utility

#define ufbxi_arraycount(arr) (sizeof(arr) / sizeof(*(arr)))
#define ufbxi_ignore(cond) (void)(cond)
#define ufbxi_for(m_type, m_name, m_begin, m_num) for (m_type *m_name = m_begin, *m_name##_end = m_name + (m_num); m_name != m_name##_end; m_name++)
#define ufbxi_for_ptr(m_type, m_name, m_begin, m_num) for (m_type **m_name = m_begin, **m_name##_end = m_name + (m_num); m_name != m_name##_end; m_name++)

static ufbxi_forceinline uint32_t ufbxi_min32(uint32_t a, uint32_t b) { return a < b ? a : b; }
static ufbxi_forceinline uint32_t ufbxi_max32(uint32_t a, uint32_t b) { return a < b ? b : a; }
static ufbxi_forceinline uint64_t ufbxi_min64(uint64_t a, uint64_t b) { return a < b ? a : b; }
static ufbxi_forceinline uint64_t ufbxi_max64(uint64_t a, uint64_t b) { return a < b ? b : a; }
static ufbxi_forceinline size_t ufbxi_min_sz(size_t a, size_t b) { return a < b ? a : b; }
static ufbxi_forceinline size_t ufbxi_max_sz(size_t a, size_t b) { return a < b ? b : a; }

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

#define RHMAP_INLINE static ufbxi_forceinline

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

// -- DEFLATE implementation
// Pretty much based on Sean Barrett's `stb_image` deflate

#if !defined(ufbx_inflate)

// Lookup data: [0:13] extra mask [13:17] extra bits [17:32] base value
// Generated by `misc/deflate_lut.py`
static const uint32_t ufbxi_deflate_length_lut[] = {
	0x00060000, 0x00080000, 0x000a0000, 0x000c0000, 0x000e0000, 0x00100000, 0x00120000, 0x00140000, 
	0x00162001, 0x001a2001, 0x001e2001, 0x00222001, 0x00264003, 0x002e4003, 0x00364003, 0x003e4003, 
	0x00466007, 0x00566007, 0x00666007, 0x00766007, 0x0086800f, 0x00a6800f, 0x00c6800f, 0x00e6800f, 
	0x0106a01f, 0x0146a01f, 0x0186a01f, 0x01c6a01f, 0x02040000, 0x00000000, 0x00000000, 
};
static const uint32_t ufbxi_deflate_dist_lut[] = {
	0x00020000, 0x00040000, 0x00060000, 0x00080000, 0x000a2001, 0x000e2001, 0x00124003, 0x001a4003, 
	0x00226007, 0x00326007, 0x0042800f, 0x0062800f, 0x0082a01f, 0x00c2a01f, 0x0102c03f, 0x0182c03f, 
	0x0202e07f, 0x0302e07f, 0x040300ff, 0x060300ff, 0x080321ff, 0x0c0321ff, 0x100343ff, 0x180343ff, 
	0x200367ff, 0x300367ff, 0x40038fff, 0x60038fff, 0x8003bfff, 0xc003bfff, 
};

static const uint8_t ufbxi_deflate_code_length_permutation[] = {
	16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15,
};

#define UFBXI_HUFF_MAX_BITS 16
#define UFBXI_HUFF_MAX_VALUE 288
#define UFBXI_HUFF_FAST_BITS 9
#define UFBXI_HUFF_FAST_SIZE (1 << UFBXI_HUFF_FAST_BITS)
#define UFBXI_HUFF_FAST_MASK (UFBXI_HUFF_FAST_SIZE - 1)

typedef struct {

	// Number of bytes left to read from `read_fn()`
	size_t input_left;

	// User-supplied read callback
	ufbx_read_fn *read_fn;
	void *read_user;

	// Buffer to read to from `read_fn()`, may point to `local_buffer` if user
	// didn't supply a suitable buffer.
	char *buffer;
	size_t buffer_size;

	// Current chunk of data to process, either the initial buffer of input
	// or part of `buffer`.
	const char *chunk_begin;    // < Begin of the buffer
	const char *chunk_ptr;      // < Next bytes to read to `bits`
	const char *chunk_end;      // < End of data before needing to call `ufbxi_bit_refill()`
	const char *chunk_real_end; // < Actual end of the data buffer

	uint64_t bits; // < Buffered bits
	size_t left;   // < Number of valid low bits in `bits`

	char local_buffer[64];
} ufbxi_bit_stream;

typedef struct {
	uint32_t num_symbols;

	uint16_t sorted_to_sym[UFBXI_HUFF_MAX_VALUE]; // < Sorted symbol index to symbol
	uint16_t past_max_code[UFBXI_HUFF_MAX_BITS];  // < One past maximum code value per bit length
	int16_t code_to_sorted[UFBXI_HUFF_MAX_BITS];  // < Code to sorted symbol index per bit length
	uint16_t fast_sym[UFBXI_HUFF_FAST_SIZE];      // < Fast symbol lookup [0:12] symbol [12:16] bits
} ufbxi_huff_tree;

typedef struct {
	ufbxi_huff_tree lit_length;
	ufbxi_huff_tree dist;
} ufbxi_trees;

typedef struct {
	bool initialized;
	ufbxi_trees static_trees;
} ufbxi_inflate_retain_imp;

ufbx_static_assert(inflate_retain_size, sizeof(ufbxi_inflate_retain_imp) <= sizeof(ufbx_inflate_retain));

typedef struct {
	ufbxi_bit_stream stream;

	char *out_begin;
	char *out_ptr;
	char *out_end;
} ufbxi_deflate_context;

static ufbxi_forceinline uint32_t
ufbxi_bit_reverse(uint32_t mask, uint32_t num_bits)
{
	ufbx_assert(num_bits <= 16);
	uint32_t x = mask;
    x = (((x & 0xaaaa) >> 1) | ((x & 0x5555) << 1));
    x = (((x & 0xcccc) >> 2) | ((x & 0x3333) << 2));
    x = (((x & 0xf0f0) >> 4) | ((x & 0x0f0f) << 4));
	x = (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));
	return x >> (16 - num_bits);
}

static ufbxi_noinline const char *
ufbxi_bit_chunk_refill(ufbxi_bit_stream *s, const char *ptr)
{
	// Copy any left-over data to the beginning of `buffer`
	size_t left = s->chunk_real_end - ptr;
	ufbx_assert(left < 64);
	memcpy(s->buffer, ptr, left);

	// Read more user data if the user supplied a `read_fn()`, otherwise
	// we assume the initial data chunk is the whole input buffer.
	if (s->read_fn) {
		size_t to_read = ufbxi_min_sz(s->input_left, s->buffer_size - left);
		if (to_read > 0) {
			size_t num_read = s->read_fn(s->read_user, s->buffer + left, to_read);
			if (num_read > to_read) num_read = 0;
			s->input_left -= num_read;
			left += num_read;
		}
	}

	// Pad the rest with zeros
	if (left < 64) {
		memset(s->buffer + left, 0, 64 - left);
		left = 64;
	}

	s->chunk_begin = s->buffer;
	s->chunk_ptr = s->buffer;
	s->chunk_end = s->buffer + left - 8;
	s->chunk_real_end = s->buffer + left;
	return s->buffer;
}

static void ufbxi_bit_stream_init(ufbxi_bit_stream *s, const ufbx_inflate_input *input)
{
	s->read_fn = input->read_fn;
	s->read_user = input->read_user;
	s->chunk_begin = (const char*)input->data;
	s->chunk_ptr = (const char*)input->data;
	s->chunk_end = (const char*)input->data + input->data_size - 8;
	s->chunk_real_end = (const char*)input->data + input->data_size;
	s->input_left = input->total_size - input->data_size;

	// Use the user buffer if it's large enough, otherwise `local_buffer`
	if (input->buffer_size >= 64) {
		s->buffer = (char*)input->buffer;
		s->buffer_size = input->buffer_size;
	} else {
		s->buffer = s->local_buffer;
		s->buffer_size = sizeof(s->local_buffer);
	}

	// Clear the initial bit buffer
	s->bits = 0;
	s->left = 0;

	// If the initial data buffer is not large enough to be read directly
	// from refill the chunk once.
	if (input->data_size < 64) {
		ufbxi_bit_chunk_refill(s, s->chunk_begin);
	}
}

static ufbxi_forceinline void
ufbxi_bit_refill(uint64_t *p_bits, size_t *p_left, const char **p_data, ufbxi_bit_stream *s)
{
	if (*p_data > s->chunk_end) {
		*p_data = ufbxi_bit_chunk_refill(s, *p_data);
	}

	// See https://fgiesen.wordpress.com/2018/02/20/reading-bits-in-far-too-many-ways-part-2/
	// variant 4. This branchless refill guarantees [56,63] bits to be valid in `*p_bits`.
	*p_bits |= ufbxi_read_u64(*p_data) << *p_left;
	*p_data += (63 - *p_left) >> 3;
	*p_left |= 56;
}

static int
ufbxi_bit_copy_bytes(void *dst, ufbxi_bit_stream *s, size_t len)
{
	ufbx_assert(s->left % 8 == 0);
	char *ptr = (char*)dst;

	// Copy the buffered bits first
	while (len > 0 && s->left > 0) {
		*ptr++ = (char)(uint8_t)s->bits;
		len -= 1;
		s->bits >>= 8;
		s->left -= 8;
	}

	// We need to clear the top bits as there may be data
	// read ahead past `s->left` in some cases
	s->bits = 0;

	// Copy the current chunk
	size_t chunk_left = s->chunk_real_end - s->chunk_ptr;
	if (chunk_left >= len) {
		memcpy(ptr, s->chunk_ptr, len);
		s->chunk_ptr += len;
		return 1;
	} else {
		memcpy(ptr, s->chunk_ptr, chunk_left);
		s->chunk_ptr += chunk_left;
		ptr += chunk_left;
	}

	// Read extra bytes from user
	size_t num_read = 0;
	if (s->read_fn) {
		num_read = s->read_fn(s->read_user, ptr, len);
	}
	return num_read == len;
}

// 0: Success
// -1: Overfull
// -2: Underfull
static ufbxi_noinline ptrdiff_t
ufbxi_huff_build(ufbxi_huff_tree *tree, uint8_t *sym_bits, uint32_t sym_count)
{
	ufbx_assert(sym_count <= UFBXI_HUFF_MAX_VALUE);
	tree->num_symbols = sym_count;

	// Count the number of codes per bit length
	// `bit_counts[0]` contains the number of non-used symbols
	uint32_t bits_counts[UFBXI_HUFF_MAX_BITS];
	memset(bits_counts, 0, sizeof(bits_counts));
	for (uint32_t i = 0; i < sym_count; i++) {
		uint32_t bits = sym_bits[i];
		ufbx_assert(bits < UFBXI_HUFF_MAX_BITS);
		bits_counts[bits]++;
	}
	uint32_t nonzero_sym_count = sym_count - bits_counts[0];

	uint32_t total_syms[UFBXI_HUFF_MAX_BITS];
	uint32_t first_code[UFBXI_HUFF_MAX_BITS];

	tree->code_to_sorted[0] = INT16_MAX;
	tree->past_max_code[0] = 0;
	total_syms[0] = 0;

	// Resolve the maximum code per bit length and ensure that the tree is not
	// overfull or underfull.
	{
		int num_codes_left = 1;
		uint32_t code = 0;
		uint32_t prev_count = 0;
		for (uint32_t bits = 1; bits < UFBXI_HUFF_MAX_BITS; bits++) {
			uint32_t count = bits_counts[bits];
			code = (code + prev_count) << 1;
			first_code[bits] = code;
			tree->past_max_code[bits] = (uint16_t)(code + count);

			uint32_t prev_syms = total_syms[bits - 1];
			total_syms[bits] = prev_syms + count;

			// Each bit level doubles the amount of codes and potentially removes some
			num_codes_left = (num_codes_left << 1) - count;
			if (num_codes_left < 0) {
				return -1;
			}

			if (count > 0) {
				tree->code_to_sorted[bits] = (int16_t)((int)prev_syms - (int)code);
			} else {
				tree->code_to_sorted[bits] = INT16_MAX;
			}
			prev_count = count;
		}

		// All codes should be used if there's more than one symbol
		if (nonzero_sym_count > 1 && num_codes_left != 0) {
			return -2;
		}
	}

	// Generate per-length sorted-to-symbol and fast lookup tables
	uint32_t bits_index[UFBXI_HUFF_MAX_BITS] = { 0 };
	memset(tree->sorted_to_sym, 0xff, sizeof(tree->sorted_to_sym));
	memset(tree->fast_sym, 0, sizeof(tree->fast_sym));
	for (uint32_t i = 0; i < sym_count; i++) {
		uint32_t bits = sym_bits[i];
		if (bits == 0) continue;

		uint32_t index = bits_index[bits]++;
		uint32_t sorted = total_syms[bits - 1] + index;
		tree->sorted_to_sym[sorted] = (uint16_t)i;

		// Reverse the code and fill all fast lookups with the reversed prefix
		uint32_t code = first_code[bits] + index;
		uint32_t rev_code = ufbxi_bit_reverse(code, bits);
		if (bits <= UFBXI_HUFF_FAST_BITS) {
			uint16_t fast_sym = (uint16_t)(i | bits << 12);
			uint32_t hi_max = 1 << (UFBXI_HUFF_FAST_BITS - bits);
			for (uint32_t hi = 0; hi < hi_max; hi++) {
				ufbx_assert(tree->fast_sym[rev_code | hi << bits] == 0);
				tree->fast_sym[rev_code | hi << bits] = fast_sym;
			}
		}
	}

	return 0;
}

static ufbxi_forceinline uint32_t
ufbxi_huff_decode_bits(const ufbxi_huff_tree *tree, uint64_t *p_bits, size_t *p_left)
{
	// If the code length is less than or equal UFBXI_HUFF_FAST_BITS we can
	// resolve the symbol and bit length directly from a lookup table.
	uint32_t fast_sym_bits = tree->fast_sym[*p_bits & UFBXI_HUFF_FAST_MASK];
	if (fast_sym_bits != 0) {
		uint32_t bits = fast_sym_bits >> 12;
		*p_bits >>= bits;
		*p_left -= bits;
		return fast_sym_bits & 0x3ff;
	}

	// The code length must be longer than UFBXI_HUFF_FAST_BITS, reverse the prefix
	// and build the code one bit at a time until we are in range for the bit length.
	uint32_t code = ufbxi_bit_reverse((uint32_t)*p_bits, UFBXI_HUFF_FAST_BITS + 1);
	*p_bits >>= UFBXI_HUFF_FAST_BITS + 1;
	*p_left -= UFBXI_HUFF_FAST_BITS + 1;
	for (uint32_t bits = UFBXI_HUFF_FAST_BITS + 1; bits < UFBXI_HUFF_MAX_BITS; bits++) {
		if (code < tree->past_max_code[bits]) {
			uint32_t sorted = code + tree->code_to_sorted[bits];
			if (sorted >= tree->num_symbols) return ~0u;
			return tree->sorted_to_sym[sorted];
		}
		code = code << 1 | (uint32_t)(*p_bits & 1);
		*p_bits >>= 1;
		*p_left -= 1;
	}

	// We shouldn't get here unless the tree is underfull _or_ has only
	// one symbol where the code `1` is invalid.
	return ~0u;
}

static void ufbxi_init_static_huff(ufbxi_trees *trees)
{
	ptrdiff_t err = 0;

	// 0-143: 8 bits, 144-255: 9 bits, 256-279: 7 bits, 280-287: 8 bits
	uint8_t lit_length_bits[288];
	memset(lit_length_bits +   0, 8, 144 -   0);
	memset(lit_length_bits + 144, 9, 256 - 144);
	memset(lit_length_bits + 256, 7, 280 - 256);
	memset(lit_length_bits + 280, 8, 288 - 280);
	err |= ufbxi_huff_build(&trees->lit_length, lit_length_bits, sizeof(lit_length_bits));

	// "Distance codes 0-31 are represented by (fixed-length) 5-bit codes"
	uint8_t dist_bits[32];
	memset(dist_bits + 0, 5, 32 - 0);
	err |= ufbxi_huff_build(&trees->dist, dist_bits, sizeof(dist_bits));

	// Building the static trees cannot fail as we use pre-defined code lengths.
	ufbx_assert(err == 0);
}

// 0: Success
// -1: Huffman Overfull
// -2: Huffman Underfull
// -3: Code 16 repeat overflow
// -4: Code 17 repeat overflow
// -5: Code 18 repeat overflow
// -6: Bad length code
static ufbxi_noinline ptrdiff_t
ufbxi_init_dynamic_huff_tree(ufbxi_deflate_context *dc, const ufbxi_huff_tree *huff_code_length,
	ufbxi_huff_tree *tree, uint32_t num_symbols)
{
	uint8_t code_lengths[UFBXI_HUFF_MAX_VALUE];
	ufbx_assert(num_symbols <= UFBXI_HUFF_MAX_VALUE);

	uint64_t bits = dc->stream.bits;
	size_t left = dc->stream.left;
	const char *data = dc->stream.chunk_ptr;

	uint32_t symbol_index = 0;
	uint8_t prev = 0;
	while (symbol_index < num_symbols) {
		ufbxi_bit_refill(&bits, &left, &data, &dc->stream);

		uint32_t inst = ufbxi_huff_decode_bits(huff_code_length, &bits, &left);
		if (inst <= 15) {
			// "0 - 15: Represent code lengths of 0 - 15"
			prev = (uint8_t)inst;
			code_lengths[symbol_index++] = (uint8_t)inst;
		} else if (inst == 16) {
			// "16: Copy the previous code length 3 - 6 times. The next 2 bits indicate repeat length."
			uint32_t num = 3 + ((uint32_t)bits & 0x3);
			bits >>= 2;
			left -= 2;
			if (symbol_index + num > num_symbols) return -3;
			memset(code_lengths + symbol_index, prev, num);
			symbol_index += num;
		} else if (inst == 17) {
			// "17: Repeat a code length of 0 for 3 - 10 times. (3 bits of length)"
			uint32_t num = 3 + ((uint32_t)bits & 0x7);
			bits >>= 3;
			left -= 3;
			if (symbol_index + num > num_symbols) return -4;
			memset(code_lengths + symbol_index, 0, num);
			symbol_index += num;
			prev = 0;
		} else if (inst == 18) {
			// "18: Repeat a code length of 0 for 11 - 138 times (7 bits of length)"
			uint32_t num = 11 + ((uint32_t)bits & 0x7f);
			bits >>= 7;
			left -= 7;
			if (symbol_index + num > num_symbols) return -5;
			memset(code_lengths + symbol_index, 0, num);
			symbol_index += num;
			prev = 0;
		} else {
			return -6;
		}
	}

	ptrdiff_t err = ufbxi_huff_build(tree, code_lengths, num_symbols);
	if (err != 0) return err;

	dc->stream.bits = bits;
	dc->stream.left = left;
	dc->stream.chunk_ptr = data;

	return 0;
}

static ptrdiff_t
ufbxi_init_dynamic_huff(ufbxi_deflate_context *dc, ufbxi_trees *trees)
{
	uint64_t bits = dc->stream.bits;
	size_t left = dc->stream.left;
	const char *data = dc->stream.chunk_ptr;
	ufbxi_bit_refill(&bits, &left, &data, &dc->stream);

	// The header contains the number of Huffman codes in each of the three trees.
	uint32_t num_lit_lengths = 257 + (bits & 0x1f);
	uint32_t num_dists = 1 + (bits >> 5 & 0x1f);
	uint32_t num_code_lengths = 4 + (bits >> 10 & 0xf);
	bits >>= 14;
	left -= 14;

	// Code lengths for the "code length" Huffman tree are represented literally
	// 3 bits in order of: 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 up to
	// `num_code_lengths`, rest of the code lengths are 0 (unused)
	uint8_t code_lengths[19];
	memset(code_lengths, 0, sizeof(code_lengths));
	for (size_t len_i = 0; len_i < num_code_lengths; len_i++) {
		if (len_i == 14) ufbxi_bit_refill(&bits, &left, &data, &dc->stream);
		code_lengths[ufbxi_deflate_code_length_permutation[len_i]] = (uint32_t)bits & 0x7;
		bits >>= 3;
		left -= 3;
	}

	dc->stream.bits = bits;
	dc->stream.left = left;
	dc->stream.chunk_ptr = data;

	ufbxi_huff_tree huff_code_length;
	ptrdiff_t err;

	// Build the temporary "code length" Huffman tree used to encode the actual
	// trees used to compress the data. Use that to build the literal/length and
	// distance trees.
	err = ufbxi_huff_build(&huff_code_length, code_lengths, ufbxi_arraycount(code_lengths));
	if (err) return -14 + 1 + err;
	err = ufbxi_init_dynamic_huff_tree(dc, &huff_code_length, &trees->lit_length, num_lit_lengths);
	if (err) return -16 + 1 + err;
	err = ufbxi_init_dynamic_huff_tree(dc, &huff_code_length, &trees->dist, num_dists);
	if (err) return -22 + 1 + err;

	return 0;
}


static uint32_t ufbxi_adler32(const void *data, size_t size)
{
	size_t a = 1, b = 0;
	const char *p = (const char*)data;

	// Adler-32 consists of two running sums modulo 65521. As an optimization
	// we can accumulate N sums before applying the modulo, where N depends on
	// the size of the type holding the sum.
	const size_t num_before_wrap = sizeof(size_t) == 8 ? 380368439u : 5552u;

	size_t size_left = size;
	while (size_left > 0) {
		size_t num = size_left <= num_before_wrap ? size_left : num_before_wrap;
		size_left -= num;
		const char *end = p + num;

		while (end - p >= 8) {
			a += (size_t)(uint8_t)p[0]; b += a;
			a += (size_t)(uint8_t)p[1]; b += a;
			a += (size_t)(uint8_t)p[2]; b += a;
			a += (size_t)(uint8_t)p[3]; b += a;
			a += (size_t)(uint8_t)p[4]; b += a;
			a += (size_t)(uint8_t)p[5]; b += a;
			a += (size_t)(uint8_t)p[6]; b += a;
			a += (size_t)(uint8_t)p[7]; b += a;
			p += 8;
		}

		while (p != end) {
			a += (size_t)(uint8_t)p[0]; b += a;
			p++;
		}

		a %= 65521u;
		b %= 65521u;
	}

	return (uint32_t)((b << 16) | (a & 0xffff));
}

static int
ufbxi_inflate_block(ufbxi_deflate_context *dc, ufbxi_trees *trees)
{
	char *out_ptr = dc->out_ptr;
	char *const out_begin = dc->out_begin;
	char *const out_end = dc->out_end;

	uint64_t bits = dc->stream.bits;
	size_t left = dc->stream.left;
	const char *data = dc->stream.chunk_ptr;

	for (;;) {
		ufbxi_bit_refill(&bits, &left, &data, &dc->stream);

		// Decode literal/length value from input stream
		uint32_t lit_length = ufbxi_huff_decode_bits(&trees->lit_length, &bits, &left);

		// If value < 256: copy value (literal byte) to output stream
		if (lit_length < 256) {
			if (out_ptr == out_end) {
				return -10;
			}
			*out_ptr++ = (char)lit_length;
		} else if (lit_length - 257 <= 285 - 257) {
			// If value = 257..285: Decode extra length and distance and copy `length` bytes
			// from `distance` bytes before in the buffer.
			uint32_t length, distance;

			// Length: Look up base length and add optional additional bits
			{
				uint32_t lut = ufbxi_deflate_length_lut[lit_length - 257];
				uint32_t base = lut >> 17;
				uint32_t offset = ((uint32_t)bits & lut & 0x1fff);
				uint32_t offset_bits = (lut >> 13) & 0xf;
				bits >>= offset_bits;
				left -= offset_bits;
				length = base + offset;
			}

			// Distance: Decode as a Huffman code and add optional additional bits
			{
				uint32_t dist = ufbxi_huff_decode_bits(&trees->dist, &bits, &left);
				if (dist >= 30) {
					return -11;
				}
				uint32_t lut = ufbxi_deflate_dist_lut[dist];
				uint32_t base = lut >> 17;
				uint32_t offset = ((uint32_t)bits & lut & 0x1fff);
				uint32_t offset_bits = (lut >> 13) & 0xf;
				bits >>= offset_bits;
				left -= offset_bits;
				distance = base + offset;
			}

			if ((ptrdiff_t)distance > out_ptr - out_begin || (ptrdiff_t)length > out_end - out_ptr) {
				return -12;
			}

			ufbx_assert(length > 0);
			const char *src = out_ptr - distance;
			char *dst = out_ptr;
			out_ptr += length;
			{
				// TODO: Do something better than per-byte copy
				char *end = dst + length;

				while (end - dst >= 4) {
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = src[3];
					dst += 4;
					src += 4;
				}

				while (dst != end) {
					*dst++ = *src++;
				}
			}
		} else if (lit_length == 256) {
			break;
		} else {
			return -13;
		}
	}

	dc->out_ptr = out_ptr;
	dc->stream.bits = bits;
	dc->stream.left = left;
	dc->stream.chunk_ptr = data;

	return 0;
}

// TODO: Error codes should have a quick test if the destination buffer overflowed
// Returns actual number of decompressed bytes or negative error:
// -1: Bad compression method (ZLIB header)
// -2: Requires dictionary (ZLIB header)
// -3: Bad FCHECK (ZLIB header)
// -4: Bad NLEN (Uncompressed LEN != ~NLEN)
// -5: Uncompressed source overflow
// -6: Uncompressed destination overflow
// -7: Bad block type
// -8: Truncated checksum (deprecated, reported as -9)
// -9: Checksum mismatch
// -10: Literal destination overflow
// -11: Bad distance code or distance of (30..31)
// -12: Match out of bounds
// -13: Bad lit/length code
// -14: Codelen Huffman Overfull
// -15: Codelen Huffman Underfull
// -16 - -21: Litlen Huffman: Overfull / Underfull / Repeat 16/17/18 overflow / Bad length code
// -22 - -27: Distance Huffman: Overfull / Underfull / Repeat 16/17/18 overflow / Bad length code
ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain)
{
	ufbxi_inflate_retain_imp *ret_imp = (ufbxi_inflate_retain_imp*)retain;

	ptrdiff_t err;
	ufbxi_deflate_context dc;
	ufbxi_bit_stream_init(&dc.stream, input);
	dc.out_begin = (char*)dst;
	dc.out_ptr = (char*)dst;
	dc.out_end = (char*)dst + dst_size;

	uint64_t bits = dc.stream.bits;
	size_t left = dc.stream.left;
	const char *data = dc.stream.chunk_ptr;

	ufbxi_bit_refill(&bits, &left, &data, &dc.stream);

	// Zlib header
	{
		size_t cmf = (size_t)(bits & 0xff);
		size_t flg = (size_t)(bits >> 8) & 0xff;
		bits >>= 16;
		left -= 16;

		if ((cmf & 0xf) != 0x8) return -1;
		if ((flg & 0x20) != 0) return -2;
		if ((cmf << 8 | flg) % 31u != 0) return -3;
	}

	for (;;) { 
		ufbxi_bit_refill(&bits, &left, &data, &dc.stream);

		// Block header: [0:1] BFINAL [1:3] BTYPE
		size_t header = (size_t)bits & 0x7;
		bits >>= 3;
		left -= 3;

		size_t type = header >> 1;
		if (type == 0) {

			// Round up to the next byte
			size_t align_bits = left & 0x7;
			bits >>= align_bits;
			left -= align_bits;

			size_t len = (size_t)(bits & 0xffff);
			size_t nlen = (size_t)((bits >> 16) & 0xffff);
			if ((len ^ nlen) != 0xffff) return -4;
			if (dc.out_end - dc.out_ptr < (ptrdiff_t)len) return -6;
			bits >>= 32;
			left -= 32;

			dc.stream.bits = bits;
			dc.stream.left = left;
			dc.stream.chunk_ptr = data;

			// Copy `len` bytes of literal data
			if (!ufbxi_bit_copy_bytes(dc.out_ptr, &dc.stream, len)) return -5;

			dc.out_ptr += len;

		} else if (type <= 2) {

			dc.stream.bits = bits;
			dc.stream.left = left;
			dc.stream.chunk_ptr = data;

			ufbxi_trees tree_data;
			ufbxi_trees *trees;
			if (type == 1) {
				// Static Huffman: Initialize the trees once and cache them in `retain`.
				if (!ret_imp->initialized) {
					ufbxi_init_static_huff(&ret_imp->static_trees);
					ret_imp->initialized = true;
				}
				trees = &ret_imp->static_trees;
			} else { 
				// Dynamic Huffman
				err = ufbxi_init_dynamic_huff(&dc, &tree_data);
				if (err) return err;
				trees = &tree_data;
			}

			err = ufbxi_inflate_block(&dc, trees);
			if (err) return err;

		} else {
			// 0b11 - reserved (error)
			return -7;
		}

		bits = dc.stream.bits;
		left = dc.stream.left;
		data = dc.stream.chunk_ptr;

		// BFINAL: End of stream
		if (header & 1) break;
	}

	// Check Adler-32
	{
		// Round up to the next byte
		size_t align_bits = left & 0x7;
		bits >>= align_bits;
		left -= align_bits;
		ufbxi_bit_refill(&bits, &left, &data, &dc.stream);

		uint32_t ref = (uint32_t)bits;
		ref = (ref>>24) | ((ref>>8)&0xff00) | ((ref<<8)&0xff0000) | (ref<<24);

		uint32_t checksum = ufbxi_adler32(dc.out_begin, dc.out_ptr - dc.out_begin);
		if (ref != checksum) {
			return -9;
		}
	}

	return dc.out_ptr - dc.out_begin;
}


#endif // !defined(ufbx_inflate)

// -- Errors

static ufbxi_noinline int ufbxi_fail_imp_err(ufbx_error *err, const char *cond, const char *func, uint32_t line)
{
	// NOTE: This is the base function all fails boil down to, place a breakpoint here to
	// break at the first error.
	if (err->stack_size < UFBX_ERROR_STACK_MAX_DEPTH) {
		ufbx_error_frame *frame = &err->stack[err->stack_size++];
		frame->description = cond;
		frame->function = func;
		frame->source_line = line;
	}
	return 0;
}

#define ufbxi_check_return_err(err, cond, ret) do { if (!(cond)) { ufbxi_fail_imp_err((err), #cond, __FUNCTION__, __LINE__); return ret; } } while (0)
#define ufbxi_fail_err(err, desc) return ufbxi_fail_imp_err(err, desc, __FUNCTION__, __LINE__)

// -- Allocator

// Returned for zero size allocations
static char ufbxi_zero_size_buffer[1];

typedef struct {
	ufbx_error *error;
	size_t current_size;
	size_t max_size;
	ufbx_allocator ator;
} ufbxi_allocator;

static ufbxi_forceinline bool ufbxi_does_overflow(size_t total, size_t a, size_t b)
{
	// If `a` and `b` have at most 4 bits per `size_t` byte, the product can't overflow.
	if (((a | b) >> sizeof(size_t)*4) != 0) {
		if (a != 0 && total / a != b) return true;
	}
	return false;
}

static void *ufbxi_alloc_size(ufbxi_allocator *ator, size_t size, size_t n)
{
	// Always succeed with an emtpy non-NULL buffer for empty allocations
	ufbx_assert(size > 0);
	if (n == 0) return ufbxi_zero_size_buffer;

	size_t total = size * n;
	ufbxi_check_return_err(ator->error, !ufbxi_does_overflow(total, size, n), NULL);
	ufbxi_check_return_err(ator->error, total <= UFBXI_MAX_ALLOCATION_SIZE, NULL);
	ufbxi_check_return_err(ator->error, total <= ator->max_size - ator->current_size, NULL);

	ator->current_size += total;

	void *ptr;
	if (ator->ator.alloc_fn) {
		ptr = ator->ator.alloc_fn(ator->ator.user, total);
	} else {
		ptr = malloc(total);
	}

	ufbxi_check_return_err(ator->error, ptr, NULL);
	return ptr;
}

static void ufbxi_free_size(ufbxi_allocator *ator, size_t size, void *ptr, size_t n);
static void *ufbxi_realloc_size(ufbxi_allocator *ator, size_t size, void *old_ptr, size_t old_n, size_t n)
{
	ufbx_assert(size > 0);
	// realloc() with zero old/new size is equivalent to alloc()/free()
	if (old_n == 0) return ufbxi_alloc_size(ator, size, n);
	if (n == 0) { ufbxi_free_size(ator, size, old_ptr, old_n); return NULL; }

	size_t old_total = size * old_n;
	size_t total = size * n;

	// The old values have been checked by a previous allocate call
	ufbx_assert(!ufbxi_does_overflow(old_total, size, old_n));
	ufbx_assert(old_total <= UFBXI_MAX_ALLOCATION_SIZE);
	ufbx_assert(old_total <= ator->current_size);

	ufbxi_check_return_err(ator->error, !ufbxi_does_overflow(total, size, n), NULL);
	ufbxi_check_return_err(ator->error, total <= UFBXI_MAX_ALLOCATION_SIZE, NULL);
	ufbxi_check_return_err(ator->error, total <= ator->max_size - ator->current_size, NULL);

	ator->current_size += total;
	ator->current_size -= old_total;

	void *ptr;
	if (ator->ator.realloc_fn) {
		ptr = ator->ator.realloc_fn(ator->ator.user, old_ptr, old_total, total);
	} else if (ator->ator.alloc_fn) {
		// Use user-provided alloc_fn() and free_fn()
		ptr = ator->ator.alloc_fn(ator->ator.user, total);
		if (ptr) memcpy(ptr, old_ptr, old_total);
		if (ator->ator.free_fn) {
			ator->ator.free_fn(ator->ator.user, old_ptr, old_total);
		}
	} else {
		ptr = realloc(old_ptr, total);
	}

	ufbxi_check_return_err(ator->error, ptr, NULL);
	return ptr;
}

static void ufbxi_free_size(ufbxi_allocator *ator, size_t size, void *ptr, size_t n)
{
	ufbx_assert(size > 0);
	if (n == 0) return;

	size_t total = size * n;

	// The old values have been checked by a previous allocate call
	ufbx_assert(!ufbxi_does_overflow(total, size, n));
	ufbx_assert(total <= UFBXI_MAX_ALLOCATION_SIZE);
	ufbx_assert(total <= ator->current_size);

	ator->current_size -= total;

	if (ator->ator.alloc_fn) {
		// Don't call default free() if there is an user-provided `alloc_fn()`
		if (ator->ator.free_fn) {
			ator->ator.free_fn(ator->ator.user, ptr, total);
		}
	} else {
		free(ptr);
	}
}

static ufbxi_forceinline void *ufbxi_alloc_zero_size(ufbxi_allocator *ator, size_t size, size_t n)
{
	void *ptr = ufbxi_alloc_size(ator, size, n);
	if (ptr) memset(ptr, 0, size * n);
	return ptr;
}

static ufbxi_forceinline void *ufbxi_realloc_zero_size(ufbxi_allocator *ator, size_t size, void *old_ptr, size_t old_n, size_t n)
{
	void *ptr = ufbxi_realloc_size(ator, size, old_ptr, old_n, n);
	if (ptr && n > old_n) memset((char*)ptr + size*old_n, 0, size*(n - old_n));
	return ptr;
}

static bool ufbxi_grow_array_size(ufbxi_allocator *ator, size_t size, void *p_ptr, size_t *p_cap, size_t n)
{
	void *ptr = *(void**)p_ptr;
	size_t old_n = *p_cap;
	size_t new_n = ufbxi_max_sz(old_n * 2, n);
	void *new_ptr = ufbxi_realloc_size(ator, size, ptr, old_n, new_n);
	if (!new_ptr) return false;
	*(void**)p_ptr = new_ptr;
	*p_cap = new_n;
	return true;
}

static bool ufbxi_grow_array_zero_size(ufbxi_allocator *ator, size_t size, void *p_ptr, size_t *p_cap, size_t n)
{
	void *ptr = *(void**)p_ptr;
	size_t old_n = *p_cap;
	size_t new_n = ufbxi_max_sz(old_n * 2, n);
	void *new_ptr = ufbxi_realloc_zero_size(ator, size, ptr, old_n, new_n);
	if (!new_ptr) return false;
	*(void**)p_ptr = new_ptr;
	*p_cap = new_n;
	return true;
}

#define ufbxi_alloc(ator, type, n) (type*)ufbxi_alloc_size((ator), sizeof(type), (n))
#define ufbxi_alloc_zero(ator, type, n) (type*)ufbxi_alloc_zero_size((ator), sizeof(type), (n))
#define ufbxi_realloc(ator, type, old_ptr, old_n, n) (type*)ufbxi_realloc_size((ator), sizeof(type), (old_ptr), (old_n), (n))
#define ufbxi_realloc_zero(ator, type, old_ptr, old_n, n) (type*)ufbxi_realloc_zero_size((ator), sizeof(type), (old_ptr), (old_n), (n))
#define ufbxi_free(ator, type, ptr, n) ufbxi_free_size((ator), sizeof(type), (ptr), (n))

#define ufbxi_grow_array(ator, p_ptr, p_cap, n) ufbxi_grow_array_size((ator), sizeof(**(p_ptr)), (p_ptr), (p_cap), (n))
#define ufbxi_grow_array_zero(ator, p_ptr, p_cap, n) ufbxi_grow_array_zero_size((ator), sizeof(**(p_ptr)), (p_ptr), (p_cap), (n))

// -- General purpose chunked buffer

typedef struct ufbxi_buf_chunk ufbxi_buf_chunk;

struct ufbxi_buf_chunk {

	// Linked list of nodes
	ufbxi_buf_chunk *root;
	ufbxi_buf_chunk *prev;
	ufbxi_buf_chunk *next;

	void *align_0; // < Align to 4x pointer size (16/32 bytes)

	uint32_t size;       // < Size of the chunk `data`, excluding this header
	uint32_t pushed_pos; // < Size of valid data when pushed to the list
	uint32_t next_size;  // < Next geometrically growing chunk size to allocate

	uint32_t align_1; // < Align to 4x uint32_t (16 bytes)

	char data[]; // < Must be aligned to 8 bytes
};

ufbx_static_assert(buf_chunk_align, offsetof(ufbxi_buf_chunk, data) % 8 == 0);

typedef struct {
	ufbxi_allocator *ator;
	ufbxi_buf_chunk *chunk;

	uint32_t pos;     // < Next offset to allocate from
	uint32_t size;    // < Size of the current chunk ie. `chunk->size` (or 0 if `chunk == NULL`)
	size_t num_items; // < Number of individual items pushed to the buffer
} ufbxi_buf;

static void *ufbxi_push_size_new_block(ufbxi_buf *b, size_t size)
{
	// TODO: Huge allocations that don't invalidate current block?

	ufbxi_check_return_err(b->ator->error, size <= UFBXI_MAX_ALLOCATION_SIZE, NULL);

	ufbxi_buf_chunk *chunk = b->chunk;
	if (chunk) {
		// Store the final position for the retired chunk
		chunk->pushed_pos = b->pos;

		// Try to re-use old chunks first
		ufbxi_buf_chunk *next;
		while ((next = chunk->next) != NULL) {
			chunk = next;
			if (size <= chunk->size) {
				b->chunk = chunk;
				b->pos = (uint32_t)size;
				b->size = chunk->size;
				return chunk->data;
			} else {
				// Didn't fit, skip the whole chunk
				chunk->pushed_pos = 0;
			}
		}
	}

	// Allocate a new chunk, grow `next_size` geometrically but don't double
	// the current or previous user sizes if they are larger.
	uint32_t next_size = chunk ? chunk->next_size * 2 : 4096;
	uint32_t chunk_size = next_size - sizeof(ufbxi_buf_chunk);
	if (chunk_size < size) chunk_size = (uint32_t)size;
	ufbxi_buf_chunk *new_chunk = (ufbxi_buf_chunk*)ufbxi_alloc_size(b->ator, 1, sizeof(ufbxi_buf_chunk) + chunk_size);
	if (!new_chunk) return NULL;

	new_chunk->prev = chunk;
	new_chunk->next = NULL;
	new_chunk->size = chunk_size;
	new_chunk->next_size = next_size;

	// Link the chunk to the list and set it as the active one
	if (chunk) {
		chunk->next = new_chunk;
		new_chunk->root = chunk->root;
	} else {
		new_chunk->root = new_chunk;
	}

	b->chunk = new_chunk;
	b->pos = (uint32_t)size;
	b->size = chunk_size;

	return new_chunk->data;
}

static ufbxi_forceinline uint32_t ufbxi_align_to_mask(uint32_t value, uint32_t align_mask)
{
	return value + ((uint32_t)-(int32_t)value & align_mask);
}

static ufbxi_forceinline uint32_t ufbxi_size_align_mask(size_t size)
{
	// Align to the all bits below the lowest set one in `size`
	// up to a maximum of 0x7 (align to 8 bytes).
	return ((size ^ (size - 1)) >> 1) & 0x7;
}

static void *ufbxi_push_size(ufbxi_buf *b, size_t size, size_t n)
{
	// Always succeed with an emtpy non-NULL buffer for empty allocations
	ufbx_assert(size > 0);
	if (n == 0) return ufbxi_zero_size_buffer;

	b->num_items += n;

	size_t total = size * n;
	if (ufbxi_does_overflow(total, size, n)) return NULL;

	// Align to the natural alignment based on the size
	uint32_t align_mask = ufbxi_size_align_mask(size);
	uint32_t pos = ufbxi_align_to_mask(b->pos, align_mask);

	// Try to push to the current block. Allocate a new block
	// if the aligned size doesn't fit.
	size_t end = (size_t)pos + total;
	if (end <= (size_t)b->size) {
		b->pos = (uint32_t)end;
		return b->chunk->data + pos;
	} else {
		return ufbxi_push_size_new_block(b, total);
	}
}

static ufbxi_forceinline void *ufbxi_push_size_zero(ufbxi_buf *b, size_t size, size_t n)
{
	void *ptr = ufbxi_push_size(b, size, n);
	if (ptr) memset(ptr, 0, size * n);
	return ptr;
}

ufbxi_nodiscard static ufbxi_forceinline void *ufbxi_push_size_copy(ufbxi_buf *b, size_t size, size_t n, const void *data)
{
	void *ptr = ufbxi_push_size(b, size, n);
	if (ptr) memcpy(ptr, data, size * n);
	return ptr;
}

static void ufbxi_pop_size(ufbxi_buf *b, size_t size, size_t n, void *dst)
{
	ufbx_assert(size > 0);
	b->num_items -= n;

	char *ptr = (char*)dst;
	size_t bytes_left = size * n;
	if (!ufbxi_does_overflow(bytes_left, size, n)) {
		if (ptr) {
			ptr += bytes_left;
			uint32_t pos = b->pos;
			for (;;) {
				ufbxi_buf_chunk *chunk = b->chunk;
				if (bytes_left <= pos) {
					// Rest of the data is in this single chunk
					pos -= (uint32_t)bytes_left;
					b->pos = pos;
					ptr -= bytes_left;
					memcpy(ptr, chunk->data + pos, bytes_left);
					return;
				} else {
					// Pop the whole chunk
					ptr -= pos;
					bytes_left -= pos;
					memcpy(ptr, chunk->data, pos);
					chunk = chunk->prev;
					b->chunk = chunk;
					b->size = chunk->size;
					pos = chunk->pushed_pos;
				}
			}
		} else {
			uint32_t pos = b->pos;
			for (;;) {
				ufbxi_buf_chunk *chunk = b->chunk;
				if (bytes_left <= pos) {
					// Rest of the data is in this single chunk
					pos -= (uint32_t)bytes_left;
					b->pos = pos;
					return;
				} else {
					// Pop the whole chunk
					bytes_left -= pos;
					chunk = chunk->prev;
					b->chunk = chunk;
					b->size = chunk->size;
					pos = chunk->pushed_pos;
				}
			}
		}
	} else {
		// Slow path, equivalent to the branch above
		if (ptr) {
			for (size_t i = 0; i < n; i++) ptr += size;
		}
		while (n > 0) {
			ufbx_assert(b->chunk);
			while (b->pos == 0) {
				ufbx_assert(b->chunk->prev);
				b->chunk = b->chunk->prev;
				b->pos = b->chunk->pushed_pos;
				b->size = b->chunk->size;
			}
			ufbx_assert(b->pos >= size);
			b->pos -= (uint32_t)size;
			if (ptr) {
				ptr -= size;
				memcpy(ptr, b->chunk->data + b->pos, size);
			}
		}
	}
}

static void *ufbxi_push_pop_size(ufbxi_buf *dst, ufbxi_buf *src, size_t size, size_t n)
{
	void *data = ufbxi_push_size(dst, size, n);
	if (!data) return NULL;
	ufbxi_pop_size(src, size, n, data);
	return data;
}

static void *ufbxi_make_array_size(ufbxi_buf *b, size_t size, size_t n)
{
	// Always succeed with an emtpy non-NULL buffer for empty allocations
	ufbx_assert(size > 0);
	if (n == 0) return ufbxi_zero_size_buffer;

	size_t total = size * n;
	if (ufbxi_does_overflow(total, size, n)) return NULL;

	if (total <= b->pos) {
		return b->chunk->data + b->pos - total;
	} else {
		// Make a local copy of the current buffer state, push the
		// whole array contiguously to the buffer, and pop the values
		// from the local copy.
		ufbxi_buf tmp = *b;
		void *dst = ufbxi_push_size(b, size, n);
		if (dst) {
			ufbxi_pop_size(&tmp, size, n, dst);
		}
		return dst;
	}
}

static void ufbxi_buf_free(ufbxi_buf *buf)
{
	ufbxi_buf_chunk *chunk = buf->chunk;
	if (chunk) {
		chunk = chunk->root;
		while (chunk) {
			ufbxi_buf_chunk *next = chunk->next;
			ufbxi_free_size(buf->ator, 1, chunk, sizeof(ufbxi_buf_chunk) + chunk->size);
			chunk = next;
		}
	}
	buf->chunk = NULL;
	buf->pos = 0;
	buf->size = 0;
	buf->num_items = 0;
}

static void ufbxi_buf_clear(ufbxi_buf *buf)
{
	ufbxi_buf_chunk *chunk = buf->chunk;
	if (chunk) {
		buf->chunk = chunk->root;
		buf->pos = 0;
		buf->size = chunk->size;
		buf->num_items = 0;
	}
}

#define ufbxi_push(b, type, n) (type*)ufbxi_push_size((b), sizeof(type), (n))
#define ufbxi_push_zero(b, type, n) (type*)ufbxi_push_size_zero((b), sizeof(type), (n))
#define ufbxi_push_copy(b, type, n, data) (type*)ufbxi_push_size_copy((b), sizeof(type), (n), (data))
#define ufbxi_pop(b, type, n, dst) ufbxi_pop_size((b), sizeof(type), (n), (dst))
#define ufbxi_push_pop(dst, src, type, n) (type*)ufbxi_push_pop_size((dst), (src), sizeof(type), (n))
#define ufbxi_make_array(b, type, n) (type*)ufbxi_make_array_size((b), sizeof(type), (n))

// -- Hash map

typedef struct {
	ufbxi_allocator *ator;
	size_t data_size;

	void *items;
	rhmap map;
} ufbxi_map;

static ufbxi_noinline bool ufbxi_map_grow_size_imp(ufbxi_map *map, size_t size, size_t min_size)
{
	size_t count, alloc_size;
	rhmap_grow_inline(&map->map, &count, &alloc_size, min_size, 0.8);

	// TODO: Overflow

	size_t data_size = alloc_size + size * count;
	char *data = ufbxi_alloc(map->ator, char, data_size);
	ufbxi_check_return_err(map->ator->error, data, false);
	void *items = data + alloc_size;
	memcpy(items, map->items, size * map->map.size);

	void *old_data = rhmap_rehash_inline(&map->map, count, alloc_size, data);
	ufbxi_free(map->ator, char, old_data, map->data_size);

	map->items = items;
	map->data_size = data_size;

	return true;
}

static ufbxi_forceinline bool ufbxi_map_grow_size(ufbxi_map *map, size_t size, size_t min_size)
{
	if (map->map.size < map->map.capacity) return true;
	return ufbxi_map_grow_size_imp(map, size, min_size);
}

static ufbxi_forceinline void *ufbxi_map_find_size(ufbxi_map *map, size_t size, uint32_t *p_scan, uint32_t hash)
{
	rhmap_iter iter;
	iter.map = &map->map;
	iter.scan = *p_scan;
	iter.hash = hash;

	uint32_t index;
	if (rhmap_find_inline(&iter, &index)) {
		*p_scan = iter.scan;
		return (char*)map->items + size * index;
	} else {
		return NULL;
	}
}

static ufbxi_forceinline void *ufbxi_map_insert_size(ufbxi_map *map, size_t size, uint32_t scan, uint32_t hash)
{
	rhmap_iter iter;
	iter.map = &map->map;
	iter.scan = scan;
	iter.hash = hash;
	uint32_t index = map->map.size;
	rhmap_insert_inline(&iter, index);
	return (char*)map->items + size * index;
}

static void ufbxi_map_free(ufbxi_map *map)
{
	void *ptr = rhmap_reset_inline(&map->map);
	ufbxi_free(map->ator, char, ptr, map->data_size);
	map->items = NULL;
}

#define ufbxi_map_grow(map, type, min_size) ufbxi_map_grow_size((map), sizeof(type), (min_size))
#define ufbxi_map_find(map, type, p_scan, hash) (type*)ufbxi_map_find_size((map), sizeof(type), (p_scan), (hash))
#define ufbxi_map_insert(map, type, scan, hash) (type*)ufbxi_map_insert_size((map), sizeof(type), (scan), (hash))

// -- Hash functions

static uint32_t ufbxi_hash_string(const char *str, size_t length)
{
	uint32_t hash = 0;
	uint32_t seed = UINT32_C(0x9e3779b9);
	if (length >= 4) {
		do {
			uint32_t word = ufbxi_read_u32(str);
			hash = ((hash << 5u | hash >> 27u) ^ word) * seed;
			str += 4;
			length -= 4;
		} while (length >= 4);

		uint32_t word = ufbxi_read_u32(str + length - 4);
		hash = ((hash << 5u | hash >> 27u) ^ word) * seed;
		return hash;
	} else {
		uint32_t word = 0;
		if (length >= 1) word |= (uint32_t)(uint8_t)str[0] << 0;
		if (length >= 2) word |= (uint32_t)(uint8_t)str[1] << 8;
		if (length >= 3) word |= (uint32_t)(uint8_t)str[2] << 16;
		hash = ((hash << 5u | hash >> 27u) ^ word) * seed;
		return hash;
	}
}

static ufbxi_forceinline uint32_t ufbxi_hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    x *= UINT32_C(0x846ca68b);
    x ^= x >> 16;
    return x;	
}

static ufbxi_forceinline uint32_t ufbxi_hash64(uint64_t x)
{
    x ^= x >> 32;
    x *= UINT64_C(0xd6e8feb86659fd93);
    x ^= x >> 32;
    x *= UINT64_C(0xd6e8feb86659fd93);
    x ^= x >> 32;
    return (uint32_t)x;
}

static ufbxi_forceinline uint32_t ufbxi_hash_uptr(uintptr_t ptr)
{
	return sizeof(ptr) == 8 ? ufbxi_hash64((uint64_t)ptr) : ufbxi_hash32((uint32_t)ptr);
}

#define ufbxi_hash_ptr(ptr) ufbxi_hash_uptr((uintptr_t)(ptr))

// -- String constants

static const char ufbxi_FBXHeaderExtension[] = "FBXHeaderExtension";
static const char ufbxi_Documents[] = "Documents";
static const char ufbxi_Document[] = "Document";
static const char ufbxi_RootNode[] = "RootNode";
static const char ufbxi_FBXVersion[] = "FBXVersion";
static const char ufbxi_Creator[] = "Creator";
static const char ufbxi_Definitions[] = "Definitions";
static const char ufbxi_Objects[] = "Objects";
static const char ufbxi_Connections[] = "Connections";
static const char ufbxi_Takes[] = "Takes";
static const char ufbxi_ObjectType[] = "ObjectType";
static const char ufbxi_PropertyTemplate[] = "PropertyTemplate";
static const char ufbxi_Properties60[] = "Properties60";
static const char ufbxi_Properties70[] = "Properties70";
static const char ufbxi_Model[] = "Model";
static const char ufbxi_NodeAttribute[] = "NodeAttribute";
static const char ufbxi_Geometry[] = "Geometry";
static const char ufbxi_AnimationLayer[] = "AnimationLayer";
static const char ufbxi_AnimationCurve[] = "AnimationCurve";
static const char ufbxi_AnimationCurveNode[] = "AnimationCurveNode";
static const char ufbxi_Mesh[] = "Mesh";
static const char ufbxi_Light[] = "Light";
static const char ufbxi_Vertices[] = "Vertices";
static const char ufbxi_PolygonVertexIndex[] = "PolygonVertexIndex";
static const char ufbxi_Edges[] = "Edges";
static const char ufbxi_KeyTime[] = "KeyTime";
static const char ufbxi_KeyValueFloat[] = "KeyValueFloat";
static const char ufbxi_KeyAttrFlags[] = "KeyAttrFlags";
static const char ufbxi_KeyAttrDataFloat[] = "KeyAttrDataFloat";
static const char ufbxi_KeyAttrRefCount[] = "KeyAttrRefCount";
static const char ufbxi_Take[] = "Take";
static const char ufbxi_Channel[] = "Channel";
static const char ufbxi_Default[] = "Default";
static const char ufbxi_Key[] = "Key";
static const char ufbxi_KeyCount[] = "KeyCount";
static const char ufbxi_Transform[] = "Transform";
static const char ufbxi_T[] = "T";
static const char ufbxi_R[] = "R";
static const char ufbxi_S[] = "S";
static const char ufbxi_X[] = "X";
static const char ufbxi_Y[] = "Y";
static const char ufbxi_Z[] = "Z";
static const char ufbxi_D_X[] = "d|X";
static const char ufbxi_D_Y[] = "d|Y";
static const char ufbxi_D_Z[] = "d|Z";
static const char ufbxi_Lcl_Translation[] = "Lcl Translation";
static const char ufbxi_Lcl_Rotation[] = "Lcl Rotation";
static const char ufbxi_Lcl_Scaling[] = "Lcl Scaling";
static const char ufbxi_OO[] = "OO";
static const char ufbxi_OP[] = "OP";

static ufbx_string ufbxi_strings[] = {
	{ ufbxi_FBXHeaderExtension, sizeof(ufbxi_FBXHeaderExtension) - 1 },
	{ ufbxi_Documents, sizeof(ufbxi_Documents) - 1 },
	{ ufbxi_Document, sizeof(ufbxi_Document) - 1 },
	{ ufbxi_RootNode, sizeof(ufbxi_RootNode) - 1 },
	{ ufbxi_FBXVersion, sizeof(ufbxi_FBXVersion) - 1 },
	{ ufbxi_Creator, sizeof(ufbxi_Creator) - 1 },
	{ ufbxi_Definitions, sizeof(ufbxi_Definitions) - 1 },
	{ ufbxi_Objects, sizeof(ufbxi_Objects) - 1 },
	{ ufbxi_Connections, sizeof(ufbxi_Connections) - 1 },
	{ ufbxi_Takes, sizeof(ufbxi_Takes) - 1 },
	{ ufbxi_ObjectType, sizeof(ufbxi_ObjectType) - 1 },
	{ ufbxi_PropertyTemplate, sizeof(ufbxi_PropertyTemplate) - 1 },
	{ ufbxi_Properties60, sizeof(ufbxi_Properties60) - 1 },
	{ ufbxi_Properties70, sizeof(ufbxi_Properties70) - 1 },
	{ ufbxi_Model, sizeof(ufbxi_Model) - 1 },
	{ ufbxi_NodeAttribute, sizeof(ufbxi_NodeAttribute) - 1 },
	{ ufbxi_Geometry, sizeof(ufbxi_Geometry) - 1 },
	{ ufbxi_AnimationLayer, sizeof(ufbxi_AnimationLayer) - 1 },
	{ ufbxi_AnimationCurve, sizeof(ufbxi_AnimationCurve) - 1 },
	{ ufbxi_AnimationCurveNode, sizeof(ufbxi_AnimationCurveNode) - 1 },
	{ ufbxi_Mesh, sizeof(ufbxi_Mesh) - 1 },
	{ ufbxi_Light, sizeof(ufbxi_Light) - 1 },
	{ ufbxi_Vertices, sizeof(ufbxi_Vertices) - 1 },
	{ ufbxi_PolygonVertexIndex, sizeof(ufbxi_PolygonVertexIndex) - 1 },
	{ ufbxi_Edges, sizeof(ufbxi_Edges) - 1 },
	{ ufbxi_KeyTime, sizeof(ufbxi_KeyTime) - 1 },
	{ ufbxi_KeyValueFloat, sizeof(ufbxi_KeyValueFloat) - 1 },
	{ ufbxi_KeyAttrFlags, sizeof(ufbxi_KeyAttrFlags) - 1 },
	{ ufbxi_KeyAttrDataFloat, sizeof(ufbxi_KeyAttrDataFloat) - 1 },
	{ ufbxi_KeyAttrRefCount, sizeof(ufbxi_KeyAttrRefCount) - 1 },
	{ ufbxi_Take, sizeof(ufbxi_Take) - 1 },
	{ ufbxi_Channel, sizeof(ufbxi_Channel) - 1 },
	{ ufbxi_Default, sizeof(ufbxi_Default) - 1 },
	{ ufbxi_Key, sizeof(ufbxi_Key) - 1 },
	{ ufbxi_KeyCount, sizeof(ufbxi_KeyCount) - 1 },
	{ ufbxi_Transform, sizeof(ufbxi_Transform) - 1 },
	{ ufbxi_T, sizeof(ufbxi_T) - 1 },
	{ ufbxi_R, sizeof(ufbxi_R) - 1 },
	{ ufbxi_S, sizeof(ufbxi_S) - 1 },
	{ ufbxi_X, sizeof(ufbxi_X) - 1 },
	{ ufbxi_Y, sizeof(ufbxi_Y) - 1 },
	{ ufbxi_Z, sizeof(ufbxi_Z) - 1 },
	{ ufbxi_D_X, sizeof(ufbxi_D_X) - 1 },
	{ ufbxi_D_Y, sizeof(ufbxi_D_Y) - 1 },
	{ ufbxi_D_Z, sizeof(ufbxi_D_Z) - 1 },
	{ ufbxi_Lcl_Translation, sizeof(ufbxi_Lcl_Translation) - 1 },
	{ ufbxi_Lcl_Rotation, sizeof(ufbxi_Lcl_Rotation) - 1 },
	{ ufbxi_Lcl_Scaling, sizeof(ufbxi_Lcl_Scaling) - 1 },
	{ ufbxi_OO, sizeof(ufbxi_OO) - 1 },
	{ ufbxi_OP, sizeof(ufbxi_OP) - 1 },
};

// -- Type definitions

typedef struct ufbxi_node ufbxi_node;

typedef enum {
	UFBXI_VALUE_NONE,
	UFBXI_VALUE_NUMBER,
	UFBXI_VALUE_STRING,
	UFBXI_VALUE_ARRAY,
} ufbxi_value_type;

typedef union {
	struct { double f; int64_t i; }; // if `UFBXI_PROP_NUMBER`
	ufbx_string s;                   // if `UFBXI_PROP_STRING`
} ufbxi_value;

typedef struct {
	void *data;  // < Pointer to `size` bool/int32_t/int64_t/float/double elements
	size_t size; // < Number of elements
	char type;   // < FBX type code: b/i/l/f/d
} ufbxi_value_array;

struct ufbxi_node {
	const char *name;      // < Name of the node (pooled, comapre with == to ufbxi_* strings)
	uint32_t num_children; // < Number of child nodes
	uint8_t name_len;      // < Length of `name` in bytes

	// If `value_type_mask == UFBXI_PROP_ARRAY` then the node is an array
	// (`array` field is valid) otherwise the node has N values in `vals`
	// where the type of each value is stored in 2 bits per value from LSB.
	// ie. `vals[ix]` type is `(value_type_mask >> (ix*2)) & 0x3`
	uint16_t value_type_mask;

	ufbxi_node *children;
	union {
		ufbxi_value_array *array; // if `prop_type_mask == UFBXI_PROP_ARRAY`
		ufbxi_value *vals;        // otherwise
	};
};

#define UFBXI_SCENE_IMP_MAGIC 0x58424655

typedef struct {
	ufbx_scene scene;
	uint32_t magic;

	ufbxi_allocator ator;
	ufbxi_buf result_buf;
	ufbxi_buf string_buf;
} ufbxi_scene_imp;

typedef enum {
	UFBXI_CONNECTABLE_UNKNOWN,
	UFBXI_CONNECTABLE_MODEL,
	UFBXI_CONNECTABLE_MESH,
	UFBXI_CONNECTABLE_LIGHT,
	UFBXI_CONNECTABLE_GEOMETRY,
	UFBXI_CONNECTABLE_ANIM_LAYER,
	UFBXI_CONNECTABLE_ANIM_PROP,
	UFBXI_CONNECTABLE_ANIM_CURVE,
	UFBXI_CONNECTABLE_ATTRIBUTE,
} ufbxi_connectable_type;

typedef struct {
	uint64_t id;
	ufbxi_connectable_type type;
	uint32_t index;
} ufbxi_connectable;

typedef struct {
	uint64_t parent_id;
	uint64_t child_id;
	const char *prop_name;
} ufbxi_connection;

typedef struct {
	ufbxi_connectable_type parent_type;
	uint32_t parent_index;
	ufbx_props props;
} ufbxi_attribute;

typedef struct {

	uint32_t version;
	bool from_ascii;
	bool big_endian;

	ufbx_load_opts opts;

	// IO
	uint64_t data_offset;

	ufbx_read_fn *read_fn;
	void *read_user;

	char *read_buffer;
	size_t read_buffer_size;

	const char *data_begin;
	const char *data;
	size_t data_size;

	// Allocators
	ufbxi_allocator ator_result;
	ufbxi_allocator ator_tmp;

	// Temporary maps
	ufbxi_map string_map;
	ufbxi_map prop_type_map;
	ufbxi_map connectable_map;

	// Conversion source buffer
	char *convert_buffer;
	size_t convert_buffer_size;

	// Temporary buffers
	ufbxi_buf tmp;
	ufbxi_buf tmp_node;
	ufbxi_buf tmp_template;
	ufbxi_buf tmp_connection;

	// Temporary arrays
	ufbxi_buf tmp_arr_models;
	ufbxi_buf tmp_arr_meshes;
	ufbxi_buf tmp_arr_lights;
	ufbxi_buf tmp_arr_anim_layers;
	ufbxi_buf tmp_arr_anim_props;
	ufbxi_buf tmp_arr_anim_curves;
	ufbxi_buf tmp_arr_attributes;

	// Result buffers
	ufbxi_buf result;
	ufbxi_buf string_buf;

	ufbxi_node root;

	ufbxi_attribute *attributes;

	ufbx_scene scene;
	ufbxi_scene_imp *scene_imp;

	ufbx_error error;

	ufbx_inflate_retain *inflate_retain;

	double ktime_to_sec;

} ufbxi_context;

static ufbxi_noinline int ufbxi_fail_imp(ufbxi_context *uc, const char *cond, const char *func, uint32_t line)
{
	return ufbxi_fail_imp_err(&uc->error, cond, func, line);
}

#define ufbxi_check(cond) if (!(cond)) return ufbxi_fail_imp(uc, #cond, __FUNCTION__, __LINE__)
#define ufbxi_check_return(cond, ret) do { if (!(cond)) { ufbxi_fail_imp(uc, #cond, __FUNCTION__, __LINE__); return ret; } } while (0)

#define ufbxi_fail(desc) return ufbxi_fail_imp(uc, desc, __FUNCTION__, __LINE__)
#define ufbxi_fail_uc(uc, desc) return ufbxi_fail_imp(uc, desc, __FUNCTION__, __LINE__)

// -- String pool

ufbxi_nodiscard static const char *ufbxi_push_string_imp(ufbxi_context *uc, const char *str, size_t length, bool copy)
{
	if (length == 0) return "";
	ufbxi_check_return(length <= uc->opts.max_string_length, NULL);

	// TODO: Set the initial size based on experimental data
	ufbxi_check_return(ufbxi_map_grow(&uc->string_map, ufbx_string, 16), NULL);
	ufbxi_check_return(uc->string_map.map.size <= uc->opts.max_strings, NULL);

	uint32_t hash = ufbxi_hash_string(str, length);
	uint32_t scan = 0;
	ufbx_string *entry;
	while ((entry = ufbxi_map_find(&uc->string_map, ufbx_string, &scan, hash)) != NULL) {
		if (entry->length == length && !memcmp(entry->data, str, length)) {
			return entry->data;
		}
	}
	entry = ufbxi_map_insert(&uc->string_map, ufbx_string, scan, hash);
	entry->length = length;
	if (copy) {
		char *dst = ufbxi_push(&uc->string_buf, char, length + 1);
		ufbxi_check_return(dst, NULL);
		memcpy(dst, str, length);
		dst[length] = '\0';
		entry->data = dst;
	} else {
		entry->data = str;
	}
	return entry->data;
}

ufbxi_nodiscard static ufbxi_forceinline const char *ufbxi_push_string(ufbxi_context *uc, const char *str, size_t length)
{
	return ufbxi_push_string_imp(uc, str, length, true);
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_push_string_place(ufbxi_context *uc, const char **p_str, size_t length)
{
	const char *str = *p_str;
	ufbxi_check(str || length == 0);
	str = ufbxi_push_string(uc, str, length);
	ufbxi_check(str);
	*p_str = str;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_push_string_place_str(ufbxi_context *uc, ufbx_string *p_str)
{
	ufbxi_check(p_str);
	return ufbxi_push_string_place(uc, &p_str->data, p_str->length);
}

// -- IO

static ufbxi_noinline const char *ufbxi_refill(ufbxi_context *uc, size_t size)
{
	ufbx_assert(uc->data_size < size);
	ufbxi_check_return(uc->read_fn, NULL);

	// Grow the read buffer if necessary
	if (size > uc->read_buffer_size) {
		size_t new_size = ufbxi_max_sz(size, uc->opts.read_buffer_size);
		ufbxi_check_return(ufbxi_grow_array(&uc->ator_tmp, &uc->read_buffer, &uc->read_buffer_size, new_size), NULL);
	}

	// Copy the remains of the previous buffer to the beginning of the new one
	size_t num_read = uc->data_size;
	memmove(uc->read_buffer, uc->data, num_read);

	// Fill the rest of the buffer with user data
	size_t to_read = uc->read_buffer_size - num_read;
	size_t read_result = uc->read_fn(uc->read_user, uc->read_buffer + num_read, to_read);
	ufbxi_check_return(read_result <= to_read, NULL);

	num_read += read_result;
	ufbxi_check_return(num_read >= size, NULL);

	uc->data_offset += uc->data - uc->data_begin;
	uc->data_begin = uc->data = uc->read_buffer;
	uc->data_size = num_read;

	return uc->read_buffer;
}

static ufbxi_forceinline const char *ufbxi_peek_bytes(ufbxi_context *uc, size_t size)
{
	if (uc->data_size >= size) {
		return uc->data;
	} else {
		return ufbxi_refill(uc, size);
	}
}

static ufbxi_forceinline const char *ufbxi_read_bytes(ufbxi_context *uc, size_t size)
{
	// Refill the current buffer if necessary
	const char *ret;
	if (uc->data_size >= size) {
		ret = uc->data;
	} else {
		ret = ufbxi_refill(uc, size);
		if (!ret) return NULL;
	}

	// Advance the read position inside the current buffer
	uc->data_size -= size;
	uc->data = ret + size;
	return ret;
}

static ufbxi_forceinline void ufbxi_consume_bytes(ufbxi_context *uc, size_t size)
{
	// Bytes must have been checked first with `ufbxi_peek_bytes()`
	ufbx_assert(size <= uc->data_size);
	uc->data_size -= size;
	uc->data += size;
}

ufbxi_nodiscard static int ufbxi_skip_bytes(ufbxi_context *uc, uint64_t size)
{
	// Read nd discard bytes in reasonable chunks
	while (size > 0) {
		uint64_t to_skip = ufbxi_min64(size, uc->opts.read_buffer_size);
		ufbxi_check(ufbxi_read_bytes(uc, (size_t)to_skip));
		size -= to_skip;
	}

	return 1;
}

static int ufbxi_read_to(ufbxi_context *uc, void *dst, size_t size)
{
	char *ptr = (char*)dst;

	// Copy data from the current buffer first
	size_t len = ufbxi_min_sz(uc->data_size, size);
	memcpy(ptr, uc->data, len);
	uc->data += len;
	uc->data_size -= len;
	ptr += len;
	size -= len;

	// If there's data left to copy try to read from user IO
	if (size > 0) {
		uc->data_offset += uc->data - uc->data_begin;

		uc->data_begin = uc->data = NULL;
		uc->data_size = 0;
		ufbxi_check(uc->read_fn);
		len = uc->read_fn(uc->read_user, ptr, size);
		ufbxi_check(len == size);

		uc->data_offset += size;
	}

	return 1;
}

static ufbxi_forceinline uint64_t ufbxi_get_read_offset(ufbxi_context *uc)
{
	return uc->data_offset + (uc->data - uc->data_begin);
}

// -- FBX value type information

static ufbxi_forceinline bool ufbxi_parse_bool(char c)
{
	// Treat \0, 'F', 'N' as falsey values, other values are true
	return (c == 0 || c == 'F' || c == 'N' || c == '0') ? 0 : 1;
}

static char ufbxi_normalize_array_type(char type) {
	switch (type) {
	case 'r': return sizeof(ufbx_real) == sizeof(float) ? 'f' : 'd';
	case 'c': return 'b';
	default: return type;
	}
}

size_t ufbxi_array_type_size(char type)
{
	switch (type) {
	case 'r': return sizeof(ufbx_real);
	case 'b': return sizeof(bool);
	case 'i': return sizeof(int32_t);
	case 'l': return sizeof(int64_t);
	case 'f': return sizeof(float);
	case 'd': return sizeof(double);
	default: return 1;
	}
}

// -- Node operations

static ufbxi_node *ufbxi_find_child(ufbxi_node *node, const char *name)
{
	ufbxi_for(ufbxi_node, c, node->children, node->num_children) {
		if (c->name == name) return c;
	}
	return NULL;
}

// Retrieve values from nodes with type codes:
// Any: '_' (ignore)
// NUMBER: 'I' int32_t 'L' int64_t 'F' float 'D' double 'R' ufbxi_real 'B' bool 'Z' size_t
// STRING: 'S' ufbx_string 'C' const char*

ufbxi_nodiscard ufbxi_forceinline static int ufbxi_get_val_at(ufbxi_node *node, size_t ix, char fmt, void *v)
{
	ufbxi_value_type type = (ufbxi_value_type)((node->value_type_mask >> (ix*2)) & 0x3);
	switch (fmt) {
	case '_': return 1;
	case 'I': if (type == UFBXI_VALUE_NUMBER) { *(int32_t*)v = (int32_t)node->vals[ix].i; return 1; } else return 0;
	case 'L': if (type == UFBXI_VALUE_NUMBER) { *(int64_t*)v = (int64_t)node->vals[ix].i; return 1; } else return 0;
	case 'F': if (type == UFBXI_VALUE_NUMBER) { *(float*)v = (float)node->vals[ix].f; return 1; } else return 0;
	case 'D': if (type == UFBXI_VALUE_NUMBER) { *(double*)v = (double)node->vals[ix].f; return 1; } else return 0;
	case 'R': if (type == UFBXI_VALUE_NUMBER) { *(ufbx_real*)v = (ufbx_real)node->vals[ix].f; return 1; } else return 0;
	case 'B': if (type == UFBXI_VALUE_NUMBER) { *(bool*)v = node->vals[ix].i != 0; return 1; } else return 0;
	case 'Z': if (type == UFBXI_VALUE_NUMBER) { if (node->vals[ix].i < 0) return 0; *(size_t*)v = (size_t)node->vals[ix].i; return 1; } else return 0;
	case 'S': if (type == UFBXI_VALUE_STRING) { *(ufbx_string*)v = node->vals[ix].s; return 1; } else return 0;
	case 'C': if (type == UFBXI_VALUE_STRING) { *(const char**)v = node->vals[ix].s.data; return 1; } else return 0;
	default:
		ufbx_assert(0 && "Bad format char");
		return 0;
	}
}

ufbxi_nodiscard ufbxi_forceinline static ufbxi_value_array *ufbxi_get_array(ufbxi_node *node, char fmt)
{
	if (node->value_type_mask != UFBXI_VALUE_ARRAY) return NULL;
	ufbxi_value_array *array = node->array;
	if (fmt != '?') {
		fmt = ufbxi_normalize_array_type(fmt);
		if (array->type != fmt) return NULL;
	}
	return array;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_get_val1(ufbxi_node *node, const char *fmt, void *v0)
{
	if (!ufbxi_get_val_at(node, 0, fmt[0], v0)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_get_val2(ufbxi_node *node, const char *fmt, void *v0, void *v1)
{
	if (!ufbxi_get_val_at(node, 0, fmt[0], v0)) return 0;
	if (!ufbxi_get_val_at(node, 1, fmt[1], v1)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_get_val3(ufbxi_node *node, const char *fmt, void *v0, void *v1, void *v2)
{
	if (!ufbxi_get_val_at(node, 0, fmt[0], v0)) return 0;
	if (!ufbxi_get_val_at(node, 1, fmt[1], v1)) return 0;
	if (!ufbxi_get_val_at(node, 2, fmt[2], v2)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_find_val1(ufbxi_node *node, const char *name, const char *fmt, void *v0)
{
	ufbxi_node *child = ufbxi_find_child(node, name);
	if (!child) return 0;
	if (!ufbxi_get_val_at(child, 0, fmt[0], v0)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_find_val2(ufbxi_node *node, const char *name, const char *fmt, void *v0, void *v1)
{
	ufbxi_node *child = ufbxi_find_child(node, name);
	if (!child) return 0;
	if (!ufbxi_get_val_at(child, 0, fmt[0], v0)) return 0;
	if (!ufbxi_get_val_at(child, 1, fmt[1], v1)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline ufbxi_value_array *ufbxi_find_array(ufbxi_node *node, const char *name, char fmt)
{
	ufbxi_node *child = ufbxi_find_child(node, name);
	if (!child) return NULL;
	return ufbxi_get_array(child, fmt);
}


// -- Parsing state machine

typedef enum {
	UFBXI_PARSE_ROOT,
	UFBXI_PARSE_DEFINITIONS,
	UFBXI_PARSE_OBJECTS,
	UFBXI_PARSE_TAKES,
	UFBXI_PARSE_MODEL,
	UFBXI_PARSE_GEOMETRY,
	UFBXI_PARSE_ANIMATION_CURVE,
	UFBXI_PARSE_TAKE,
	UFBXI_PARSE_TAKE_OBJECT,
	UFBXI_PARSE_CHANNEL,
	UFBXI_PARSE_UNKNOWN,
} ufbxi_parse_state;

typedef struct {
	char type;
	bool result;
} ufbxi_array_info;

static ufbxi_parse_state ufbxi_update_parse_state(ufbxi_parse_state parent, const char *name)
{
	switch (parent) {

	case UFBXI_PARSE_ROOT:
		if (name == ufbxi_Definitions) return UFBXI_PARSE_DEFINITIONS;
		if (name == ufbxi_Objects) return UFBXI_PARSE_OBJECTS;
		if (name == ufbxi_Takes) return UFBXI_PARSE_TAKES;
		break;

	case UFBXI_PARSE_OBJECTS:
		if (name == ufbxi_Model) return UFBXI_PARSE_MODEL;
		if (name == ufbxi_Geometry) return UFBXI_PARSE_GEOMETRY;
		if (name == ufbxi_AnimationCurve) return UFBXI_PARSE_ANIMATION_CURVE;
		break;

	case UFBXI_PARSE_TAKES:
		if (name == ufbxi_Take) return UFBXI_PARSE_TAKE;
		break;

	case UFBXI_PARSE_TAKE:
		return UFBXI_PARSE_TAKE_OBJECT;

	case UFBXI_PARSE_TAKE_OBJECT:
		if (name == ufbxi_Channel) return UFBXI_PARSE_CHANNEL;
		break;

	case UFBXI_PARSE_CHANNEL:
		if (name == ufbxi_Channel) return UFBXI_PARSE_CHANNEL;
		break;

	default:
		break;

	}

	return UFBXI_PARSE_UNKNOWN;
}

static bool ufbxi_is_array_node(ufbxi_context *uc, ufbxi_parse_state parent, const char *name, ufbxi_array_info *info)
{
	switch (parent) {

	case UFBXI_PARSE_GEOMETRY:
	case UFBXI_PARSE_MODEL:
		if (name == ufbxi_Vertices) {
			info->type = 'r';
			info->result = true;
			return true;
		} else if (name == ufbxi_PolygonVertexIndex) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_Edges) {
			info->type = 'i';
			info->result = false;
			return true;
		}
		break;

	case UFBXI_PARSE_ANIMATION_CURVE:
		if (name == ufbxi_KeyTime) {
			info->type = 'l';
			info->result = false;
			return true;
		} else if (name == ufbxi_KeyValueFloat) {
			info->type = 'r';
			info->result = false;
			return true;
		} else if (name == ufbxi_KeyAttrFlags) {
			info->type = 'i';
			info->result = false;
			return true;
		} else if (name == ufbxi_KeyAttrDataFloat) {
			// ?? Float data of this specific array is represented as integers in ASCII
			info->type = uc->from_ascii ? 'i' : 'f';
			info->result = false;
			return true;
		} else if (name == ufbxi_KeyAttrRefCount) {
			info->type = 'i';
			info->result = false;
			return true;
		}
		break;

	case UFBXI_PARSE_CHANNEL:
		if (name == ufbxi_Key) {
			info->type = 'd';
			info->result = false;
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

// -- Binary parsing

// Read and convert a post-7000 FBX data array into a different format. `src_type` may be equal to `dst_type`
// if the platform is not binary compatible with the FBX data representation.
ufbxi_nodiscard static ufbxi_noinline int ufbxi_binary_convert_array(ufbxi_context *uc, char src_type, char dst_type, const void *src, void *dst, size_t size)
{
	switch (dst_type)
	{

	#define ufbxi_convert_loop(m_dst, m_size, m_expr) { \
		const char *val = (const char*)src, *val_end = val + size*m_size; \
		m_dst *d = (m_dst*)dst; \
		while (val != val_end) { *d++ = (m_dst)(m_expr); val += m_size; } }

	#define ufbxi_convert_switch(m_dst) \
		switch (src_type) { \
		case 'b': ufbxi_convert_loop(m_dst, 1, ufbxi_parse_bool(*val)); break; \
		case 'i': ufbxi_convert_loop(m_dst, 4, ufbxi_read_i32(val)); break; \
		case 'l': ufbxi_convert_loop(m_dst, 8, ufbxi_read_i64(val)); break; \
		case 'f': ufbxi_convert_loop(m_dst, 4, ufbxi_read_f32(val)); break; \
		case 'd': ufbxi_convert_loop(m_dst, 8, ufbxi_read_f64(val)); break; \
		default: ufbxi_fail("Bad array source type"); \
		} \
		break; \

	case 'b':
		switch (src_type) {
		case 'b': ufbxi_convert_loop(char, 1, ufbxi_parse_bool(*val)); break;
		case 'i': ufbxi_convert_loop(char, 4, ufbxi_read_i32(val) != 0); break;
		case 'l': ufbxi_convert_loop(char, 8, ufbxi_read_i64(val) != 0); break;
		case 'f': ufbxi_convert_loop(char, 4, ufbxi_read_f32(val) != 0); break;
		case 'd': ufbxi_convert_loop(char, 8, ufbxi_read_f64(val) != 0); break;
		default: ufbxi_fail("Bad array source type");
		}
		break;

	case 'i': ufbxi_convert_switch(int32_t); break;
	case 'l': ufbxi_convert_switch(int64_t); break;
	case 'f': ufbxi_convert_switch(float); break;
	case 'd': ufbxi_convert_switch(double); break;

	default: return 0;

	}

	return 1;
}

// Read pre-7000 separate properties as an array.
ufbxi_nodiscard static ufbxi_noinline int ufbxi_binary_parse_multivalue_array(ufbxi_context *uc, char dst_type, void *dst, size_t size)
{
	if (size == 0) return 1;
	const char *val;
	size_t val_size;

	switch (dst_type)
	{

	#define ufbxi_convert_parse(m_dst, m_size, m_expr) \
		*d++ = (m_dst)(m_expr); val_size = m_size + 1; \

	#define ufbxi_convert_parse_switch(m_dst) { \
		m_dst *d = (m_dst*)dst; \
		for (size_t i = 0; i < size; i++) { \
			val = ufbxi_peek_bytes(uc, 13); \
			ufbxi_check(val); \
			switch (*val++) { \
				case 'C': \
				case 'B': ufbxi_convert_parse(m_dst, 1, *val); break; \
				case 'Y': ufbxi_convert_parse(m_dst, 2, ufbxi_read_i16(val)); break; \
				case 'I': ufbxi_convert_parse(m_dst, 4, ufbxi_read_i32(val)); break; \
				case 'L': ufbxi_convert_parse(m_dst, 8, ufbxi_read_i64(val)); break; \
				case 'F': ufbxi_convert_parse(m_dst, 4, ufbxi_read_f32(val)); break; \
				case 'D': ufbxi_convert_parse(m_dst, 8, ufbxi_read_f64(val)); break; \
				default: ufbxi_fail("Bad multivalue array type"); \
			} \
			ufbxi_consume_bytes(uc, val_size); \
		} \
	} \

	case 'b':
	{
		char *d = (char*)dst;
		for (size_t i = 0; i < size; i++) {
			val = ufbxi_peek_bytes(uc, 13);
			ufbxi_check(val);
			switch (*val++) {
				case 'C':
				case 'B': ufbxi_convert_parse(char, 1, *val != 0); break;
				case 'Y': ufbxi_convert_parse(char, 2, ufbxi_read_i16(val) != 0); break;
				case 'I': ufbxi_convert_parse(char, 4, ufbxi_read_i32(val) != 0); break;
				case 'L': ufbxi_convert_parse(char, 8, ufbxi_read_i64(val) != 0); break;
				case 'F': ufbxi_convert_parse(char, 4, ufbxi_read_f32(val) != 0); break;
				case 'D': ufbxi_convert_parse(char, 8, ufbxi_read_f64(val) != 0); break;
				default: ufbxi_fail("Bad multivalue array type");
			}
			ufbxi_consume_bytes(uc, val_size);
		}
	}
	break;

	case 'i': ufbxi_convert_parse_switch(int32_t); break;
	case 'l': ufbxi_convert_parse_switch(int64_t); break;
	case 'f': ufbxi_convert_parse_switch(float); break;
	case 'd': ufbxi_convert_parse_switch(double); break;

	default: return 0;

	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_binary_parse_node(ufbxi_context *uc, uint32_t depth, ufbxi_parse_state parent_state, bool *p_end)
{
	// https://code.blender.org/2013/08/fbx-binary-file-format-specification
	// Parse an FBX document node in the binary format

	ufbxi_check(depth < uc->opts.max_node_depth);

	// Parse the node header, post-7500 versions use 64-bit values for most
	// header fields. 
	uint64_t end_offset, num_values64, values_len;
	uint8_t name_len;
	size_t header_size = (uc->version >= 7500) ? 25 : 13;
	const char *header = ufbxi_read_bytes(uc, header_size);
	ufbxi_check(header);
	if (uc->version >= 7500) {
		end_offset = ufbxi_read_u64(header + 0);
		num_values64 = ufbxi_read_u64(header + 8);
		values_len = ufbxi_read_u64(header + 16);
		name_len = ufbxi_read_u8(header + 24);
	} else {
		end_offset = ufbxi_read_u32(header + 0);
		num_values64 = ufbxi_read_u32(header + 4);
		values_len = ufbxi_read_u32(header + 8);
		name_len = ufbxi_read_u8(header + 12);
	}

	// We support at most UINT32_MAX values (`max_node_values` is `uint32_t`)
	ufbxi_check(num_values64 <= (uint64_t)uc->opts.max_node_values);
	uint32_t num_values = (uint32_t)num_values64;

	// If `end_offset` is zero we treat as the node as a NULL-sentinel
	// that terminates a node list.
	if (end_offset == 0) {
		*p_end = true;
		return 1;
	}

	// Push the parsed node into the `tmp_node` buffer, the nodes will be popped by
	// calling code after its done parsing all of it's children.
	ufbxi_node *node = ufbxi_push_zero(&uc->tmp_node, ufbxi_node, 1);
	ufbxi_check(node);

	// Parse and intern the name to the string pool.
	const char *name = ufbxi_read_bytes(uc, name_len);
	ufbxi_check(name);
	name = ufbxi_push_string(uc, name, name_len);
	ufbxi_check(name);
	node->name_len = name_len;
	node->name = name;

	uint64_t values_end_offset = ufbxi_get_read_offset(uc) + values_len;

	// Check if the values of the node we're parsing currently should be
	// treated as an array.
	ufbxi_array_info arr_info;
	if (ufbxi_is_array_node(uc, parent_state, name, &arr_info)) {

		// Normalize the array type (eg. 'r' to 'f'/'d' depending on the build)
		// and get the per-element size of the array.
		char dst_type = ufbxi_normalize_array_type(arr_info.type);
		size_t dst_elem_size = ufbxi_array_type_size(dst_type);

		ufbxi_value_array *arr = ufbxi_push(&uc->tmp, ufbxi_value_array, 1);
		ufbxi_check(arr);

		node->value_type_mask = UFBXI_VALUE_ARRAY;
		node->array = arr;
		arr->type = dst_type;

		// The array may be pushed either to the result or temporary buffer depending
		// if it's already in the right format
		ufbxi_buf *arr_buf = arr_info.result ? &uc->result : &uc->tmp;

		// Peek the first bytes of the array. We can always look at least 13 bytes
		// ahead safely as valid FBX files must end in a 13/25 byte NULL record.
		const char *data = ufbxi_peek_bytes(uc, 13);
		ufbxi_check(data);

		// Check if the data type is one of the explicit array types (post-7000).
		// Otherwise we form the array by concatenating all the normal values of the
		// node (pre-7000)
		char c = data[0];
		if (c=='c' || c=='b' || c=='i' || c=='l' || c =='f' || c=='d') {

			// Parse the array header from the prefix we already peeked above.
			char src_type = data[0];
			uint32_t size = ufbxi_read_u32(data + 1); 
			uint32_t encoding = ufbxi_read_u32(data + 5); 
			uint32_t encoded_size = ufbxi_read_u32(data + 9); 
			ufbxi_consume_bytes(uc, 13);

			ufbxi_check(size <= uc->opts.max_array_size);

			// Normalize the source type as well, but don't convert UFBX-specific
			// 'r' to 'f'/'d', but fail later instead.
			if (src_type != 'r') src_type = ufbxi_normalize_array_type(src_type);
			size_t src_elem_size = ufbxi_array_type_size(src_type);
			size_t decoded_data_size = src_elem_size * size;

			// Allocate `size` elements for the array.
			char *arr_data = (char*)ufbxi_push_size(arr_buf, dst_elem_size, size);
			ufbxi_check(arr_data);

			// If the source and destination types are equal and our build is binary-compatible
			// with the FBX format we can read the decoded data directly into the array buffer.
			// Otherwise we need a temporary buffer to decode the array into before conversion.
			// TODO: Streaming array conversion?
			void *decoded_data = arr_data;
			if (src_type != dst_type || uc->big_endian) {
				if (uc->convert_buffer_size < decoded_data_size) {
					ufbxi_grow_array(&uc->ator_tmp, &uc->convert_buffer, &uc->convert_buffer_size, decoded_data_size);
				}
				decoded_data = uc->convert_buffer;
			}

			if (encoding == 0) {
				// Encoding 0: Plain binary data.

				// If the array is contained in the current read buffer and we need to convert
				// the data anyway we can use the read buffer as the decoded array source, otherwise
				// do a plain byte copy to the array/conversion buffer.
				if (uc->data_size >= encoded_size && decoded_data != arr_data) {
					decoded_data = (void*)uc->data;
					ufbxi_consume_bytes(uc, encoded_size);
				} else {
					ufbxi_check(ufbxi_read_to(uc, decoded_data, encoded_size));
				}
			} else if (encoding == 1) {
				// Encoding 1: DEFLATE

				// We re-use the internal read buffer for inflating the data, so make sure it's large enough.
				if (uc->read_buffer_size < uc->opts.read_buffer_size) {
					size_t new_size = uc->opts.read_buffer_size;
					ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->read_buffer, &uc->read_buffer_size, new_size));
				}

				// Inflate the data from the user-provided IO buffer / read callbacks
				ufbx_inflate_input input;
				input.total_size = encoded_size;
				input.data = uc->data;
				input.data_size = uc->data_size;
				input.buffer = uc->read_buffer;
				input.buffer_size = uc->read_buffer_size;
				input.read_fn = uc->read_fn;
				input.read_user = uc->read_user;
				ptrdiff_t res = ufbx_inflate(decoded_data, decoded_data_size, &input, uc->inflate_retain);
				ufbxi_check(res == decoded_data_size);

				// Consume the IO buffer / advance offset as necessary
				if (encoded_size > input.data_size) {
					uc->data_offset += encoded_size - input.data_size;
					uc->data += input.data_size;
					uc->data_size = 0;
				} else {
					uc->data += encoded_size;
					uc->data_size -= encoded_size;
				}

			} else {
				ufbxi_fail("Bad array encoding");
			}

			// Convert the decoded array if necessary. If we didn't perform conversion but use the
			// "bool" type we need to normalize the array contents afterwards.
			if (decoded_data != arr_data) {
				ufbxi_check(ufbxi_binary_convert_array(uc, src_type, dst_type, decoded_data, arr_data, size));
			} else if (dst_type == 'b') {
				ufbxi_for(char, c, (char*)arr_data, size) {
					*c = (char)ufbxi_parse_bool(*c);
				}
			}

			arr->data = arr_data;
			arr->size = size;

		} else {
			// Allocate `num_values` elements for the array and parse single values into it.
			ufbxi_check(num_values <= uc->opts.max_array_size);
			char *arr_data = (char*)ufbxi_push_size(arr_buf, dst_elem_size, num_values);
			ufbxi_check(arr_data);
			ufbxi_check(ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_data, num_values));
			arr->data = arr_data;
			arr->size = num_values;
		}

	} else {
		// Parse up to UFBXI_MAX_NON_ARRAY_VALUES as plain values
		num_values = ufbxi_min32(num_values, UFBXI_MAX_NON_ARRAY_VALUES);
		ufbxi_value *vals = ufbxi_push(&uc->tmp, ufbxi_value, num_values);
		ufbxi_check(vals);
		node->vals = vals;

		uint32_t type_mask = 0;
		for (size_t i = 0; i < (size_t)num_values; i++) {
			// The file must end in a 13/25 byte NULL record, so we can peek
			// up to 13 bytes safely here.
			const char *data = ufbxi_peek_bytes(uc, 13);
			ufbxi_check(data);

			switch (data[0]) {

			case 'C': case 'B':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].f = (double)(vals[i].i = (int64_t)data[1]);
				ufbxi_consume_bytes(uc, 2);
				break;

			case 'Y':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].f = (double)(vals[i].i = ufbxi_read_i16(data + 1));
				ufbxi_consume_bytes(uc, 3);
				break;

			case 'I':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].f = (double)(vals[i].i = ufbxi_read_i32(data + 1));
				ufbxi_consume_bytes(uc, 5);
				break;

			case 'L':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].f = (double)(vals[i].i = ufbxi_read_i64(data + 1));
				ufbxi_consume_bytes(uc, 9);
				break;

			case 'F':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].i = (int64_t)(vals[i].f = ufbxi_read_f32(data + 1));
				ufbxi_consume_bytes(uc, 5);
				break;

			case 'D':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].i = (int64_t)(vals[i].f = ufbxi_read_f64(data + 1));
				ufbxi_consume_bytes(uc, 9);
				break;

			case 'S': case 'R':
			{
				size_t len = ufbxi_read_u32(data + 1);
				ufbxi_consume_bytes(uc, 5);
				vals[i].s.data = ufbxi_read_bytes(uc, len);
				vals[i].s.length = len;
				ufbxi_check(ufbxi_push_string_place_str(uc, &vals[i].s));
				type_mask |= UFBXI_VALUE_STRING << (i*2);
			}
			break;

			// Treat arrays as non-values and skip them
			case 'c': case 'b': case 'i': case 'l': case 'f': case 'd':
			{
				uint32_t encoded_size = ufbxi_read_u32(data + 9);
				ufbxi_consume_bytes(uc, 13);
				ufbxi_check(ufbxi_skip_bytes(uc, encoded_size));
			}
			break;

			default:
				ufbxi_fail("Bad values type");

			}
		}

		node->value_type_mask = (uint16_t)type_mask;
	}

	// Skip over remaining values if necessary if we for example truncated
	// the list of values or if there are values after an array
	uint64_t offset = ufbxi_get_read_offset(uc);
	ufbxi_check(offset <= values_end_offset);
	if (offset < values_end_offset) {
		ufbxi_check(ufbxi_skip_bytes(uc, values_end_offset - offset));
	}

	// Recursively parse the children of this node. Update the parse state
	// to provide context for child node parsing.
	ufbxi_parse_state parse_state = ufbxi_update_parse_state(parent_state, node->name);
	uint32_t num_children = 0;
	for (;;) {
		ufbxi_check(num_children < uc->opts.max_node_children);

		// Stop at end offset
		uint64_t offset = ufbxi_get_read_offset(uc);
		if (offset >= end_offset) {
			ufbxi_check(offset == end_offset);
			break;
		}

		bool end = false;
		ufbxi_check(ufbxi_binary_parse_node(uc, depth + 1, parse_state, &end));
		if (end) break;
		num_children++;
	}

	// Pop children from `tmp_node` to a contiguous array
	node->num_children = num_children;
	node->children = ufbxi_push_pop(&uc->tmp, &uc->tmp_node, ufbxi_node, num_children);
	ufbxi_check(node->children);

	return 1;
}

#define UFBXI_BINARY_MAGIC_SIZE 23
#define UFBXI_BINARY_HEADER_SIZE 27
static const char ufbxi_binary_magic[] = "Kaydara FBX Binary  \x00\x1a\x00";

ufbxi_nodiscard static int ufbxi_binary_parse(ufbxi_context *uc)
{
	// Parse top-level nodes until we hit a NULL record
	uint32_t num_nodes = 0;
	for (;;) {
		bool end = false;
		ufbxi_check(ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end));
		if (end) break;
		num_nodes++;
	}

	// Pop top-level nodes from `tmp_node` to a contiguous array
	ufbxi_node *top_nodes = ufbxi_push_pop(&uc->tmp, &uc->tmp_node, ufbxi_node, num_nodes);
	ufbxi_check(top_nodes);

	// We don't need `tmp_node` after this point anymore
	ufbxi_buf_free(&uc->tmp_node);

	// Setup the root node
	uc->root.children = top_nodes;
	uc->root.num_children = num_nodes;

	return 1;
}

// -- ASCII parsing

#define UFBXI_ASCII_END '\0'
#define UFBXI_ASCII_NAME 'N'
#define UFBXI_ASCII_BARE_WORD 'B'
#define UFBXI_ASCII_INT 'I'
#define UFBXI_ASCII_FLOAT 'F'
#define UFBXI_ASCII_STRING 'S'

typedef struct {
	// Semantic string data and length eg. for a string token
	// this string doesn't include the quotes.
	char *str_data;
	size_t str_len;
	size_t str_cap;

	// Type of the token, either single character such as '{' or ':'
	// or one of UFBXI_ASCII_* defines.
	char type;

	// Parsed semantic value
	union {
		double f64;
		int64_t i64;
		size_t name_len;
	} value;
} ufbxi_ascii_token;

typedef struct {
	ufbxi_context *uc;

	size_t max_token_length;

	const char *src;
	const char *src_end;

	ufbxi_ascii_token prev_token;
	ufbxi_ascii_token token;
} ufbxi_ascii;

static ufbxi_noinline char ufbxi_ascii_refill(ufbxi_ascii *ua)
{
	ufbxi_context *uc = ua->uc;
	if (uc->read_fn) {
		// Grow the read buffer if necessary
		if (uc->read_buffer_size < uc->opts.read_buffer_size) {
			size_t new_size = uc->opts.read_buffer_size;
			ufbxi_check_return(ufbxi_grow_array(&uc->ator_tmp, &uc->read_buffer, &uc->read_buffer_size, new_size), '\0');
		}

		// Read user data, return '\0' on EOF
		size_t num_read = uc->read_fn(uc->read_user, uc->read_buffer, uc->read_buffer_size);
		ufbxi_check(num_read <= uc->read_buffer_size);
		if (num_read == 0) return '\0';

		ua->src = uc->read_buffer;
		ua->src_end = uc->read_buffer + num_read;
		return *ua->src;
	} else {
		// If the user didn't specify a `read_fn()` treat anything
		// past the initial data buffer as EOF.
		ua->src = "";
		ua->src_end = ua->src + 1;
		return '\0';
	}
}

static ufbxi_forceinline char ufbxi_ascii_peek(ufbxi_ascii *ua)
{
	if (ua->src == ua->src_end) return ufbxi_ascii_refill(ua);
	return *ua->src;
}

static ufbxi_forceinline char ufbxi_ascii_next(ufbxi_ascii *ua)
{
	if (ua->src == ua->src_end) return ufbxi_ascii_refill(ua);
	ua->src++;
	if (ua->src == ua->src_end) return ufbxi_ascii_refill(ua);
	return *ua->src;
}

static char ufbxi_ascii_skip_whitespace(ufbxi_ascii *ua)
{
	// Ignore whitespace
	char c = ufbxi_ascii_peek(ua);
	for (;;) {
		while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			c = ufbxi_ascii_next(ua);
		}

		// Line comment
		if (c == ';') {
			c = ufbxi_ascii_next(ua);
			while (c != '\n' && c != '\0') {
				c = ufbxi_ascii_next(ua);
			}
		} else {
			break;
		}
	}
	return c;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_ascii_push_token_char(ufbxi_ascii *ua, ufbxi_ascii_token *token, char c)
{
	ufbxi_context *uc = ua->uc;

	// Grow the string data buffer if necessary
	if (token->str_len == token->str_cap) {
		size_t len = ufbxi_max_sz(token->str_len + 1, 256);
		ufbxi_check(len <= uc->opts.max_ascii_token_length);
		ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &token->str_data, &token->str_cap, len));
	}

	token->str_data[token->str_len++] = c;

	return 1;
}

ufbxi_nodiscard static int ufbxi_ascii_next_token(ufbxi_ascii *ua, ufbxi_ascii_token *token)
{
	ufbxi_context *uc = ua->uc;

	char c = ufbxi_ascii_skip_whitespace(ua);
	token->str_len = 0;

	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
		token->type = UFBXI_ASCII_BARE_WORD;
		while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_') {
			ufbxi_check(ufbxi_ascii_push_token_char(ua, token, c));
			c = ufbxi_ascii_next(ua);
		}

		// Skip whitespace to find if there's a following ':'
		c = ufbxi_ascii_skip_whitespace(ua);
		if (c == ':') {
			token->value.name_len = token->str_len;
			token->type = UFBXI_ASCII_NAME;
			ufbxi_ascii_next(ua);
		}
	} else if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
		token->type = UFBXI_ASCII_INT;

		while ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
			if (c == '.' || c == 'e' || c == 'E') {
				token->type = UFBXI_ASCII_FLOAT;
			}
			ufbxi_check(ufbxi_ascii_push_token_char(ua, token, c));
			c = ufbxi_ascii_next(ua);
		}
		ufbxi_check(ufbxi_ascii_push_token_char(ua, token, '\0'));

		char *end;
		if (token->type == UFBXI_ASCII_INT) {
			token->value.i64 = strtoll(token->str_data, &end, 10);
			ufbxi_check(end == token->str_data + token->str_len - 1);
		} else if (token->type == UFBXI_ASCII_FLOAT) {
			token->value.f64 = strtod(token->str_data, &end);
			ufbxi_check(end == token->str_data + token->str_len - 1);
		}
	} else if (c == '"') {
		token->type = UFBXI_ASCII_STRING;
		c = ufbxi_ascii_next(ua);
		while (c != '"') {
			ufbxi_check(ufbxi_ascii_push_token_char(ua, token, c));
			c = ufbxi_ascii_next(ua);
			ufbxi_check(c != '\0');
		}
		// Skip closing quote
		ufbxi_ascii_next(ua);
	} else {
		// Single character token
		token->type = c;
		ufbxi_ascii_next(ua);
	}

	return 1;
}


ufbxi_nodiscard static int ufbxi_ascii_accept(ufbxi_ascii *ua, char type)
{
	ufbxi_context *uc = ua->uc;

	if (ua->token.type == type) {

		// Replace `prev_token` with `token` but swap the buffers so `token` uses
		// the now-unused string buffer of the old `prev_token`.
		char *swap_data = ua->prev_token.str_data;
		size_t swap_cap = ua->prev_token.str_cap;
		ua->prev_token = ua->token;
		ua->token.str_data = swap_data;
		ua->token.str_cap = swap_cap;

		ufbxi_check(ufbxi_ascii_next_token(ua, &ua->token));
		return 1;
	} else {
		return 0;
	}
}

ufbxi_nodiscard static int ufbxi_ascii_parse_node(ufbxi_ascii *ua, uint32_t depth, ufbxi_parse_state parent_state)
{
	ufbxi_context *uc = ua->uc;

	// Parse the name eg. "Node:" token and intern the name
	ufbxi_check(depth < uc->opts.max_node_depth);
	ufbxi_check(ufbxi_ascii_accept(ua, UFBXI_ASCII_NAME));
	size_t name_len = ua->prev_token.value.name_len;
	ufbxi_check(name_len <= 0xff);
	const char *name = ufbxi_push_string(uc, ua->prev_token.str_data, ua->prev_token.str_len);
	ufbxi_check(name);

	// Push the parsed node into the `tmp_node` buffer, the nodes will be popped by
	// calling code after its done parsing all of it's children.
	ufbxi_node *node = ufbxi_push_zero(&uc->tmp_node, ufbxi_node, 1);
	node->name = name;
	node->name_len = (uint8_t)name_len;

	bool in_ascii_array = false;

	uint32_t num_values = 0;
	uint32_t type_mask = 0;

	int arr_type = 0;
	ufbxi_buf *arr_buf = NULL;

	// Check if the values of the node we're parsing currently should be
	// treated as an array.
	ufbxi_array_info arr_info;
	if (ufbxi_is_array_node(uc, parent_state, name, &arr_info)) {
		arr_type = ufbxi_normalize_array_type(arr_info.type);
		arr_buf = arr_info.result ? &uc->result : &uc->tmp;

		ufbxi_value_array *arr = ufbxi_push(&uc->tmp, ufbxi_value_array, 1);
		ufbxi_check(arr);
		node->value_type_mask = UFBXI_VALUE_ARRAY;
		node->array = arr;
		arr->type = arr_type;
	}

	ufbxi_value vals[UFBXI_MAX_NON_ARRAY_VALUES];

	// NOTE: Infinite loop to allow skipping the comma parsing via `continue`.
	for (;;) {
		ufbxi_check(num_values <= (arr_type ? uc->opts.max_array_size : uc->opts.max_node_values));

		ufbxi_ascii_token *tok = &ua->prev_token;
		if (ufbxi_ascii_accept(ua, UFBXI_ASCII_STRING)) {

			if (num_values < UFBXI_MAX_NON_ARRAY_VALUES && !arr_type) {
				type_mask |= UFBXI_VALUE_STRING << (num_values*2);
				ufbxi_value *v = &vals[num_values];
				v->s.data = tok->str_data;
				v->s.length = tok->str_len;
				ufbxi_check(ufbxi_push_string_place_str(uc, &v->s));
			}

		} else if (ufbxi_ascii_accept(ua, UFBXI_ASCII_INT)) {
			int64_t val = tok->value.i64;

			switch (arr_type) {

			case 0:
				if (num_values < UFBXI_MAX_NON_ARRAY_VALUES) {
					type_mask |= UFBXI_VALUE_NUMBER << (num_values*2);
					ufbxi_value *v = &vals[num_values];
					v->f = (double)(v->i = val);
				}
				break;

			case 'b': { bool *v = ufbxi_push(arr_buf, bool, 1); ufbxi_check(v); *v = val != 0; } break;
			case 'i': { int32_t *v = ufbxi_push(arr_buf, int32_t, 1); ufbxi_check(v); *v = (int32_t)val; } break;
			case 'l': { int64_t *v = ufbxi_push(arr_buf, int64_t, 1); ufbxi_check(v); *v = (int64_t)val; } break;
			case 'f': { float *v = ufbxi_push(arr_buf, float, 1); ufbxi_check(v); *v = (float)val; } break;
			case 'd': { double *v = ufbxi_push(arr_buf, double, 1); ufbxi_check(v); *v = (double)val; } break;

			default:
				ufbxi_fail("Bad array dst type");

			}

		} else if (ufbxi_ascii_accept(ua, UFBXI_ASCII_FLOAT)) {
			double val = tok->value.f64;

			switch (arr_type) {

			case 0:
				if (num_values < UFBXI_MAX_NON_ARRAY_VALUES) {
					type_mask |= UFBXI_VALUE_NUMBER << (num_values*2);
					ufbxi_value *v = &vals[num_values];
					v->i = (int64_t)(v->f = val);
				}
				break;

			case 'b': { bool *v = ufbxi_push(arr_buf, bool, 1); ufbxi_check(v); *v = val != 0; } break;
			case 'i': { int32_t *v = ufbxi_push(arr_buf, int32_t, 1); ufbxi_check(v); *v = (int32_t)val; } break;
			case 'l': { int64_t *v = ufbxi_push(arr_buf, int64_t, 1); ufbxi_check(v); *v = (int64_t)val; } break;
			case 'f': { float *v = ufbxi_push(arr_buf, float, 1); ufbxi_check(v); *v = (float)val; } break;
			case 'd': { double *v = ufbxi_push(arr_buf, double, 1); ufbxi_check(v); *v = (double)val; } break;

			default:
				ufbxi_fail("Bad array dst type");

			}

		} else if (ufbxi_ascii_accept(ua, UFBXI_ASCII_BARE_WORD)) {

			int64_t val = 0;
			if (tok->str_len >= 1) {
				val = (int64_t)tok->str_data[0];
			}

			switch (arr_type) {

			case 0:
				if (num_values < UFBXI_MAX_NON_ARRAY_VALUES) {
					type_mask |= UFBXI_VALUE_NUMBER << (num_values*2);
					ufbxi_value *v = &vals[num_values];
					v->f = (double)(v->i = val);
				}
				break;

			case 'b': { bool *v = ufbxi_push(arr_buf, bool, 1); ufbxi_check(v); *v = val != 0; } break;
			case 'i': { int32_t *v = ufbxi_push(arr_buf, int32_t, 1); ufbxi_check(v); *v = (int32_t)val; } break;
			case 'l': { int64_t *v = ufbxi_push(arr_buf, int64_t, 1); ufbxi_check(v); *v = (int64_t)val; } break;
			case 'f': { float *v = ufbxi_push(arr_buf, float, 1); ufbxi_check(v); *v = (float)val; } break;
			case 'd': { double *v = ufbxi_push(arr_buf, double, 1); ufbxi_check(v); *v = (double)val; } break;

			}

		} else if (ufbxi_ascii_accept(ua, '*')) {
			// Parse a post-7000 ASCII array eg. "*3 { 1,2,3 }"
			ufbxi_check(!in_ascii_array);
			ufbxi_check(ufbxi_ascii_accept(ua, UFBXI_ASCII_INT));

			if (ufbxi_ascii_accept(ua, '{')) {
				ufbxi_check(ufbxi_ascii_accept(ua, UFBXI_ASCII_NAME));

				// NOTE: This `continue` skips incrementing `num_values` and parsing
				// a comma, continuing to parse the values in the array.
				in_ascii_array = true;
				continue;
			}
		} else {
			break;
		}

		// Add value and keep parsing if there's a comma. This part may be
		// skipped if we enter an array block.
		num_values++;
		if (!ufbxi_ascii_accept(ua, ',')) break;
	}

	// Close the ASCII array if we are in one
	if (in_ascii_array) {
		ufbxi_check(ufbxi_ascii_accept(ua, '}'));
	}

	if (arr_type) {
		size_t arr_elem_size = ufbxi_array_type_size(arr_type);
		void *arr_data = ufbxi_make_array_size(arr_buf, arr_elem_size, num_values);
		ufbxi_check(arr_data);
		node->array->data = arr_data;
		node->array->size = num_values;
	} else {
		num_values = ufbxi_min32(num_values, UFBXI_MAX_NON_ARRAY_VALUES);
		node->value_type_mask = (uint16_t)type_mask;
		node->vals = ufbxi_push_copy(&uc->tmp, ufbxi_value, num_values, vals);
	}

	// Recursively parse the children of this node. Update the parse state
	// to provide context for child node parsing.
	ufbxi_parse_state parse_state = ufbxi_update_parse_state(parent_state, node->name);
	if (ufbxi_ascii_accept(ua, '{')) {
		size_t num_children = 0;
		while (!ufbxi_ascii_accept(ua, '}')) {
			ufbxi_check(num_children < uc->opts.max_node_children);
			ufbxi_check(ufbxi_ascii_parse_node(ua, depth + 1, parse_state));
			num_children++;
		}

		// Pop children from `tmp_node` to a contiguous array
		node->children = ufbxi_push_pop(&uc->tmp, &uc->tmp_node, ufbxi_node, num_children);
		ufbxi_check(node->children);
		node->num_children = (uint32_t)num_children;
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_ascii_parse_imp(ufbxi_ascii *ua)
{
	ufbxi_context *uc = ua->uc;

	// Parse top-level nodes until we hit `UFBXI_ASCII_END`
	uint32_t num_nodes = 0;
	ufbxi_check(ufbxi_ascii_next_token(ua, &ua->token));
	while (!ufbxi_ascii_accept(ua, UFBXI_ASCII_END)) {
		ufbxi_check(num_nodes < uc->opts.max_node_children);
		ufbxi_check(ufbxi_ascii_parse_node(ua, 0, UFBXI_PARSE_ROOT));
		num_nodes++;
	}

	// Pop top-level nodes from `tmp_node` to a contiguous array
	ufbxi_node *top_nodes = ufbxi_push_pop(&uc->tmp, &uc->tmp_node, ufbxi_node, num_nodes);
	ufbxi_check(top_nodes);

	uc->root.children = top_nodes;
	uc->root.num_children = num_nodes;

	// We don't need `tmp_node` after this point anymore
	ufbxi_buf_free(&uc->tmp_node);

	return 1;
}

ufbxi_nodiscard static int ufbxi_ascii_parse(ufbxi_context *uc)
{
	ufbxi_ascii ua = { 0 };
	ua.uc = uc;

	// Use the current read buffer as the initial parse buffer
	ua.src = uc->data;
	ua.src_end = uc->data + uc->data_size;

	int ret = ufbxi_ascii_parse_imp(&ua);

	// Free temporary token string data
	ufbxi_free(&uc->ator_tmp, char, ua.token.str_data, ua.token.str_cap);
	ufbxi_free(&uc->ator_tmp, char, ua.prev_token.str_data, ua.prev_token.str_cap);

	return ret;
}

// -- Setup

ufbxi_nodiscard static int ufbxi_load_strings(ufbxi_context *uc)
{
	// Push all the global 'ufbxi_*' strings into the pool without copying them
	// This allows us to compare name pointers to the global values
	ufbxi_for(ufbx_string, str, ufbxi_strings, ufbxi_arraycount(ufbxi_strings)) {
		ufbxi_check(ufbxi_push_string_imp(uc, str->data, str->length, false));
	}

	return 1;
}

typedef struct {
	ufbx_prop_type type;
	const char *name;
} ufbxi_prop_type_name;

const ufbxi_prop_type_name ufbxi_prop_type_names[] = {
	{ UFBX_PROP_BOOLEAN, "Boolean" },
	{ UFBX_PROP_BOOLEAN, "bool" },
	{ UFBX_PROP_INTEGER, "Integer" },
	{ UFBX_PROP_INTEGER, "int" },
	{ UFBX_PROP_INTEGER, "enum" },
	{ UFBX_PROP_NUMBER, "Number" },
	{ UFBX_PROP_NUMBER, "double" },
	{ UFBX_PROP_VECTOR, "Vector" },
	{ UFBX_PROP_VECTOR, "Vector3D" },
	{ UFBX_PROP_COLOR, "Color" },
	{ UFBX_PROP_COLOR, "ColorRGB" },
	{ UFBX_PROP_STRING, "String" },
	{ UFBX_PROP_STRING, "KString" },
	{ UFBX_PROP_DATE_TIME, "DateTime" },
	{ UFBX_PROP_TRANSLATION, "Lcl Translation" },
	{ UFBX_PROP_ROTATION, "Lcl Rotation" },
	{ UFBX_PROP_SCALING, "Lcl Scaling" },
};

ufbxi_nodiscard static int ufbxi_load_maps(ufbxi_context *uc)
{
	ufbxi_check(ufbxi_map_grow(&uc->prop_type_map, ufbxi_prop_type_name, ufbxi_arraycount(ufbxi_prop_type_names)));
	ufbxi_for(const ufbxi_prop_type_name, name, ufbxi_prop_type_names, ufbxi_arraycount(ufbxi_prop_type_names)) {
		const char *pooled = ufbxi_push_string_imp(uc, name->name, strlen(name->name), false);
		ufbxi_check(pooled);
		uint32_t hash = ufbxi_hash_ptr(pooled);
		ufbxi_prop_type_name *entry = ufbxi_map_insert(&uc->prop_type_map, ufbxi_prop_type_name, 0, hash);
		entry->type = name->type;
		entry->name = pooled;
	}

	return 1;
}

static ufbx_prop_type ufbxi_get_prop_type(ufbxi_context *uc, const char *name)
{
	uint32_t hash = ufbxi_hash_ptr(name);
	uint32_t scan = 0;
	ufbxi_prop_type_name *entry;
	while ((entry = ufbxi_map_find(&uc->prop_type_map, ufbxi_prop_type_name, &scan, hash)) != NULL) {
		if (entry->name == name) {
			return entry->type;
		}
	}
	return UFBX_PROP_UNKNOWN;
}

// -- General parsing

ufbxi_nodiscard static int ufbxi_parse(ufbxi_context *uc)
{
	const char *header = ufbxi_peek_bytes(uc, UFBXI_BINARY_HEADER_SIZE);
	ufbxi_check(header);

	// If the file starts with the binary magic parse it as binary, otherwise
	// treat it as an ASCII file.
	if (!memcmp(header, ufbxi_binary_magic, UFBXI_BINARY_MAGIC_SIZE)) {

		// Read the version directly from the header
		uc->version = ufbxi_read_u32(header + UFBXI_BINARY_MAGIC_SIZE);
		ufbxi_consume_bytes(uc, UFBXI_BINARY_HEADER_SIZE);

		ufbxi_check(ufbxi_binary_parse(uc));

	} else {
		uc->from_ascii = true;

		ufbxi_check(ufbxi_ascii_parse(uc));

		// TODO: Parse the version from the initial magic comment eg. "; FBX 6.1.0 project file"
		// The magic comment seems to be _required_ by some FBX parsers, so it seems pretty safe
		// to assume it's always present

		// Default to version 7400 if not found in header
		uc->version = 7400;

		// Try to get the version from the header
		ufbxi_node *header_extension = ufbxi_find_child(&uc->root, ufbxi_FBXHeaderExtension);
		if (header_extension) {
			// Doesn't matter if it's not found
			ufbxi_ignore(ufbxi_find_val1(header_extension, ufbxi_FBXVersion, "I", &uc->version));
		}
	}

	return 1;
}

// -- Find implementation

static ufbxi_forceinline uint32_t ufbxi_get_name_key(const char *name, size_t len)
{
	uint32_t key = 0;
	if (len >= 4) {
		key = (uint8_t)name[0]<<24 | (uint8_t)name[1]<<16 | (uint8_t)name[2]<<8 | (uint8_t)name[3];
	} else {
		for (size_t i = 0; i < 4; i++) {
			key <<= 8;
			if (i < len) key |= (uint8_t)name[i];
		}
	}
	return key;
}


// -- Reading the parsed data

typedef struct {
	ufbx_string name;
	ufbx_string sub_type;
	uint64_t id;
	ufbx_props props;
} ufbxi_object;

ufbxi_nodiscard static int ufbxi_read_header_extension(ufbxi_context *uc, ufbxi_node *header)
{
	ufbxi_ignore(ufbxi_find_val1(header, ufbxi_Creator, "S", &uc->scene.metadata.creator));

	// TODO: Read TCDefinition and adjust timestamps
	uc->ktime_to_sec = (1.0 / 46186158000.0);

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_property(ufbxi_context *uc, ufbxi_node *node, ufbx_prop *prop, int version)
{
	const char *type_str = NULL, *subtype_str = NULL;
	ufbxi_check(ufbxi_get_val2(node, "SC", &prop->name, (char**)&type_str));
	uint32_t val_ix = 2;
	if (version == 70) {
		ufbxi_check(ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_str));
	}

	// Skip flags
	val_ix++;

	prop->imp_key = ufbxi_get_name_key(node->name, node->name_len);
	prop->type = ufbxi_get_prop_type(uc, type_str);
	if (prop->type == UFBX_PROP_UNKNOWN && subtype_str) {
		prop->type = ufbxi_get_prop_type(uc, subtype_str);
	}

	ufbxi_ignore(ufbxi_get_val_at(node, val_ix, 'S', &prop->value_str));
	ufbxi_ignore(ufbxi_get_val_at(node, val_ix, 'L', &prop->value_int));
	for (size_t i = 0; i < 3; i++) {
		if (!ufbxi_get_val_at(node, val_ix + i, 'R', &prop->value_real[i])) break;
	}
	
	return 1;
}

static int ufbxi_cmp_prop(const void *va, const void *vb)
{
	const ufbx_prop *a = (const ufbx_prop*)va, *b = (const ufbx_prop*)vb;
	if (a->imp_key < b->imp_key) return -1;
	if (a->imp_key > b->imp_key) return +1;
	return strcmp(a->name.data, b->name.data);
}

ufbxi_nodiscard static int ufbxi_read_properties(ufbxi_context *uc, ufbxi_node *parent, ufbx_props *props, ufbxi_buf *buf)
{
	int version = 70;
	ufbxi_node *node = ufbxi_find_child(parent, ufbxi_Properties70);
	if (!node) {
		node = ufbxi_find_child(parent, ufbxi_Properties60);
		if (!node) {
			// No properties found, not an error
			return 1;
		}
		version = 60;
	}

	// Parse properties directly to `result` buffer and linearize them using `ufbxi_make_array()`
	ufbxi_check(node->num_children < uc->opts.max_properties);
	ufbxi_for(ufbxi_node, prop_node, node->children, node->num_children) {
		ufbx_prop *prop = ufbxi_push_zero(&uc->result, ufbx_prop, 1);
		ufbxi_check(prop);
		ufbxi_check(ufbxi_read_property(uc, prop_node, prop, version));
	}

	props->props = ufbxi_make_array(buf, ufbx_prop, node->num_children);
	props->num_props = node->num_children;
	ufbxi_check(props->props);

	// Sort the properties by `name_hash`
	qsort(props->props, props->num_props, sizeof(ufbx_prop), ufbxi_cmp_prop);

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_definitions(ufbxi_context *uc, ufbxi_node *definitions)
{
#if 0
	ufbxi_for(ufbxi_node, object, definitions->children, definitions->num_children) {
		if (object->name != ufbxi_ObjectType) continue;

		ufbx_template *tmpl = ufbxi_push_zero(&uc->tmp_template, ufbx_template, 1);
		ufbxi_check(tmpl);
		ufbxi_check(ufbxi_get_val1(object, "S", &tmpl->type_str));

		// Pre-7000 FBX versions don't have property templates, they just have
		// the object counts by themselves.
		ufbxi_node *props = ufbxi_find_child(object, ufbxi_PropertyTemplate);
		if (props) {
			ufbxi_check(ufbxi_get_val1(props, "S", &tmpl->name));
			ufbxi_check(ufbxi_read_properties(uc, props, &tmpl->props));
		}
	}

	// Copy the templates to the destination buffer
	size_t num = uc->tmp_template.num_items;
	uc->scene.templates.data = ufbxi_push_pop(&uc->result, &uc->tmp_template, ufbx_template, num);
	uc->scene.templates.size = num;
	ufbxi_check(uc->scene.templates.data);

	ufbxi_buf_free(&uc->tmp_template);
#endif

	return 1;
}

static float ufbxi_convert_weight(const float *p_value)
{
	// Even though FBX stores the values as floats _and_ the record is named
	// KeyAttrDataFloat, for some reason tangents weights are stored as integers.
	int32_t val_i = *(int32_t*)p_value;
}

ufbxi_nodiscard static int ufbxi_add_connection(ufbxi_context *uc, uint64_t parent_id, uint64_t child_id, const char *prop_name)
{
	ufbxi_connection *conn = ufbxi_push(&uc->tmp_connection, ufbxi_connection, 1);
	ufbxi_check(conn);

	conn->parent_id = parent_id;
	conn->child_id = child_id;
	conn->prop_name = prop_name;

	return 1;
}

ufbxi_nodiscard static int ufbxi_add_connectable(ufbxi_context *uc, ufbxi_connectable_type type, uint64_t id, size_t index)
{
	ufbxi_check(ufbxi_map_grow(&uc->connectable_map, ufbxi_connectable, 64));
	ufbxi_check(index <= UINT32_MAX);

	uint32_t hash = ufbxi_hash64(id);
	ufbxi_connectable *conn = ufbxi_map_insert(&uc->connectable_map, ufbxi_connectable, 0, hash);
	conn->id = id;
	conn->type = type;
	conn->index = (uint32_t)index;

	return 1;
}

static ufbxi_connectable *ufbxi_find_connectable(ufbxi_context *uc, uint64_t id)
{
	uint32_t hash = ufbxi_hash64(id);
	uint32_t scan = 0;
	ufbxi_connectable *conn;
	while ((conn = ufbxi_map_find(&uc->connectable_map, ufbxi_connectable, &scan, hash)) != NULL) {
		if (conn->id == id) {
			return conn;
		}
	}
	return NULL;
}

ufbxi_nodiscard static int ufbxi_read_model(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	ufbx_node *scene_node = NULL;
	if (object->sub_type.data == ufbxi_Mesh) {
		ufbx_mesh *mesh = ufbxi_push_zero(&uc->tmp_arr_meshes, ufbx_mesh, 1);
		ufbxi_check(mesh);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_MESH, object->id, uc->tmp_arr_models.num_items - 1));
		mesh->node.type = UFBX_NODE_MESH;
		scene_node = &mesh->node;
	} else if (object->sub_type.data == ufbxi_Light) {
		ufbx_light *light = ufbxi_push_zero(&uc->tmp_arr_lights, ufbx_light, 1);
		ufbxi_check(light);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_LIGHT, object->id, uc->tmp_arr_lights.num_items - 1));
		light->node.type = UFBX_NODE_LIGHT;
		scene_node = &light->node;
	}

	if (scene_node) {
		scene_node->name = object->name;
		scene_node->props = object->props;
	}

	// TODO: Read model properties

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_node_attribute(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	if (object->sub_type.data == ufbxi_Light) {
		ufbxi_attribute *attr = ufbxi_push_zero(&uc->tmp_arr_attributes, ufbxi_attribute, 1);
		ufbxi_check(attr);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ATTRIBUTE, object->id, uc->tmp_arr_attributes.num_items - 1));

		attr->props = object->props;
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_animation_layer(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	ufbx_anim_layer *layer = ufbxi_push_zero(&uc->tmp_arr_anim_layers, ufbx_anim_layer, 1);
	ufbxi_check(layer);
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_LAYER, object->id, uc->tmp_arr_anim_layers.num_items - 1));

	layer->name = object->name;

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_animation_curve(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	ufbx_anim_curve *curve = ufbxi_push_zero(&uc->tmp_arr_anim_curves, ufbx_anim_curve, 1);
	ufbxi_check(curve);
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_CURVE, object->id, uc->tmp_arr_anim_curves.num_items - 1));

	ufbxi_value_array *times, *values, *flags, *attrs, *refs;
	ufbxi_check(times = ufbxi_find_array(node, ufbxi_KeyTime, 'l'));
	ufbxi_check(values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r'));
	ufbxi_check(flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags, 'i'));
	ufbxi_check(attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, '?'));
	ufbxi_check(refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i'));

	// Time and value arrays that define the keyframes should be parallel
	ufbxi_check(times->size == values->size);

	// Flags and attributes are run-length encoded where KeyAttrRefCount (refs)
	// is an array that describes how many times to repeat a given flag/attribute.
	// Attributes consist of 4 32-bit floating point values per key.
	ufbxi_check(flags->size == refs->size);
	ufbxi_check(attrs->size == refs->size * 4u);

	size_t num_keys = times->size;
	ufbx_keyframe *keys = ufbxi_push(&uc->result, ufbx_keyframe, num_keys);
	ufbxi_check(keys);

	curve->keyframes.data = keys;
	curve->keyframes.size = num_keys;

	int64_t *p_time = (int64_t*)times->data;
	ufbx_real *p_value = (ufbx_real*)values->data;
	int32_t *p_flag = (int32_t*)flags->data;
	float *p_attr = (float*)attrs->data;
	int32_t *p_ref = (int32_t*)refs->data;

	// The previous key defines the weight/slope of the left tangent
	float slope_left = 0.0f;
	float weight_left = 0.333333f;

	double prev_time = 0.0;
	double next_time = 0.0;

	if (num_keys > 0) {
		next_time = (double)p_time[0] * uc->ktime_to_sec;
	}

	for (size_t i = 0; i < num_keys; i++) {
		ufbx_keyframe *key = &keys[i];

		key->time = next_time;
		key->value = *p_value;

		if (i + 1 < num_keys) {
			next_time = (double)p_time[1] * uc->ktime_to_sec;
		}

		uint32_t flags = (uint32_t)*p_flag;

		float slope_right = p_attr[0];
		float weight_right = 0.333333f;
		float next_slope_left = p_attr[1];
		float next_weight_left = 0.333333f;

		if (flags & 0x3000000) {
			// At least one of the tangents is weighted. The weights are encoded as
			// two 0.4 _decimal_ fixed point values that are packed into 32 bits and
			// interpreted as a 32-bit float.
			uint32_t packed_weights;
			memcpy(&packed_weights, &p_attr[2], sizeof(uint32_t));

			if (flags & 0x1000000) {
				// Right tangent is weighted
				weight_right = (float)(packed_weights & 0xffff) * 0.0001f;
			}

			if (flags & 0x2000000) {
				// Next left tangent is weighted
				next_weight_left = (float)(packed_weights >> 16) * 0.0001f;
			}
		}

		if (flags & 0x2) {
			// Constant interpolation: Set cubic tangents to flat.

			if (flags & 0x100) {
				// Take constant value from next key
				key->interpolation = UFBX_INTERPOLATION_CONSTANT_NEXT;

			} else {
				// Take constant value from the previous key
				key->interpolation = UFBX_INTERPOLATION_CONSTANT_PREV;
			}

			weight_right = next_weight_left = 0.333333f;
			slope_right = next_slope_left = 0.0f;

		} else if (flags & 0x8) {
			// Cubic interpolation
			key->interpolation = UFBX_INTERPOLATION_CUBIC;

			if (flags & 0x400) {
				// User tangents

				if (flags & 0x800) {
					// Broken tangents: No need to modify slopes
				} else {
					// Unified tangents: Use right slope for both sides
					// TODO: ??? slope_left = slope_right;
				}

			} else {
				// Automatic (0x100) or unknown tangents
				// TODO: TCB tangents (0x200)
				// TODO: Auto break (0x800)

				// Automatic tangents are specified via the previous tangent's
				// NextLeftSlope value
				slope_right = slope_left;
			}

		} else {
			// Linear (0x4) or unknown interpolation: Set cubic tangents to match
			// the linear interpolation with weights of 1/3.
			key->interpolation = UFBX_INTERPOLATION_LINEAR;

			weight_right = 0.333333f;
			next_weight_left = 0.333333f;

			if (next_time > key->time) {
				double slope = (p_value[1] - key->value) / (next_time - key->time);
				slope_right = next_slope_left = (float)slope;
			} else {
				slope_right = next_slope_left = 0.0f;
			}
		}

		// Set the tangents based on weights (dx relative to the time difference
		// between the previous/next key) and slope (simply dy = slope * dx)

		if (key->time > prev_time) {
			key->left.dx = (float)(weight_left * (key->time - prev_time));
			key->left.dy = key->left.dx * slope_left;
		} else {
			key->left.dx = 0.0f;
			key->left.dy = 0.0f;
		}

		if (next_time > key->time) {
			key->right.dx = (float)(weight_right * (next_time - key->time));
			key->right.dy = key->right.dx * slope_right;
		} else {
			key->right.dx = 0.0f;
			key->right.dy = 0.0f;
		}

		slope_left = next_slope_left;
		weight_left = next_weight_left;
		prev_time = key->time;

		// Decrement attribute refcount and potentially move to the next one.
		int32_t refs = --*p_ref;
		ufbxi_check(refs >= 0);
		if (refs == 0) {
			p_flag++;
			p_attr += 4;
			p_ref++;
		}
		p_time++;
		p_value++;
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_animation_curve_node(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	ufbx_anim_prop *prop = ufbxi_push_zero(&uc->tmp_arr_anim_props, ufbx_anim_prop, 1);
	ufbxi_check(prop);
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_PROP, object->id, uc->tmp_arr_anim_props.num_items - 1));

	ufbxi_for(ufbx_prop, def, object->props.props, object->props.num_props) {
		if (def->type != UFBX_PROP_NUMBER) continue;

		size_t index = 0;
		if (def->name.data == ufbxi_Y || def->name.data == ufbxi_D_Y) index = 1;
		if (def->name.data == ufbxi_Z || def->name.data == ufbxi_D_Z) index = 2;
		prop->defaults[index] = def->value_real[0];
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_split_type_and_name(ufbxi_context *uc, ufbx_string type_and_name, ufbx_string *type, ufbx_string *name)
{
	// Name and type are packed in a single property as Type::Name (in ASCII)
	// or Type\x01\x02Name (in binary)
	const char *sep = uc->from_ascii ? "::" : "\x00\x01";
	size_t type_end;
	for (type_end = 2; type_end < type_and_name.length; type_end++) {
		const char *ch = type_and_name.data + type_end - 2;
		if (ch[0] == sep[0] && ch[1] == sep[1]) break;
	}

	// ???: ASCII and binary store type and name in different order
	if (type_end < type_and_name.length) {
		if (uc->from_ascii) {
			name->data = type_and_name.data + type_end;
			name->length = type_and_name.length - type_end;
			type->data = type_and_name.data;
			type->length = type_end - 2;
		} else {
			name->data = type_and_name.data;
			name->length = type_end - 2;
			type->data = type_and_name.data + type_end;
			type->length = type_and_name.length - type_end;
		}
	} else {
		*type = type_and_name;
		name->data = NULL;
		name->length = 0;
	}

	ufbxi_check(ufbxi_push_string_place_str(uc, type));
	ufbxi_check(ufbxi_push_string_place_str(uc, name));

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_objects(ufbxi_context *uc, ufbxi_node *objects)
{
	ufbxi_object object = { 0 };
	ufbxi_for(ufbxi_node, node, objects->children, objects->num_children) {
		ufbx_string type_and_name;

		// Failing to parse the object properties is not an error since
		// there's some weird objects mixed in every now and then.
		// FBX version 7000 and up uses 64-bit unique IDs per object,
		// older FBX versions just use name/type pairs, which we can
		// use as IDs since all strings are interned into a string pool.
		if (uc->version >= 7000) {
			if (!ufbxi_get_val3(node, "LSS", &object.id, &type_and_name, &object.sub_type)) continue;
		} else {
			if (!ufbxi_get_val2(node, "SS", &type_and_name, &object.sub_type)) continue;
			object.id = (uintptr_t)type_and_name.data;
		}

		ufbx_string type_str;
		ufbxi_check(ufbxi_split_type_and_name(uc, type_and_name, &type_str, &object.name));

		// TODO: Read to temp if the node has attributes (which is almost always?)
		ufbxi_check(ufbxi_read_properties(uc, node, &object.props, &uc->result));

		const char *name = node->name;
		if (name == ufbxi_Model) {
			ufbxi_check(ufbxi_read_model(uc, node, &object));
		} else if (name == ufbxi_NodeAttribute) {
			ufbxi_check(ufbxi_read_node_attribute(uc, node, &object));
		} else if (name == ufbxi_AnimationLayer) {
			ufbxi_check(ufbxi_read_animation_layer(uc, node, &object));
		} else if (name == ufbxi_AnimationCurve) {
			ufbxi_check(ufbxi_read_animation_curve(uc, node, &object));
		} else if (name == ufbxi_AnimationCurveNode) {
			ufbxi_check(ufbxi_read_animation_curve_node(uc, node, &object));
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_take_anim_channel(ufbxi_context *uc, ufbxi_node *node, uint64_t parent_id, const char *name, ufbx_real *p_default)
{
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_Default, "R", p_default));

	// Find the key array, early return with success if not found as we may have only a default
	ufbxi_value_array *keys = ufbxi_find_array(node, ufbxi_Key, 'd');
	if (!keys) return 1;

	ufbx_anim_curve *curve = ufbxi_push_zero(&uc->tmp_arr_anim_curves, ufbx_anim_curve, 1);
	ufbxi_check(curve);

	// Add a "virtual" connection between the animation curve and the property
	uint64_t id = (uintptr_t)curve;
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_CURVE, id, uc->tmp_arr_anim_curves.num_items - 1));
	ufbxi_check(ufbxi_add_connection(uc, parent_id, id, name));

	size_t num_keys;
	ufbxi_check(ufbxi_find_val1(node, ufbxi_KeyCount, "Z", &num_keys));
	curve->keyframes.data = ufbxi_push(&uc->result, ufbx_keyframe, num_keys);
	curve->keyframes.size = num_keys;
	ufbxi_check(curve->keyframes.data);

	float slope_left = 0.0f;
	float weight_left = 0.333333f;

	double next_time = 0.0;
	double next_value = 0.0;
	double prev_time = 0.0;

	// The pre-7000 keyframe data is stored as a _heterogenous_ array containing 64-bit integers,
	// floating point values, and _bare characters_. We cast all values to double and interpret them.
	double *data = (double*)keys->data, *data_end = data + keys->size;

	if (num_keys > 0) {
		ufbxi_check(data_end - data >= 2);
		next_time = data[0] * uc->ktime_to_sec;
		next_value = data[1];
	}

	for (size_t i = 0; i < num_keys; i++) {
		ufbx_keyframe *key = &curve->keyframes.data[i];

		// First three values: Time, Value, InterpolationMode
		ufbxi_check(data_end - data >= 3);
		key->time = next_time;
		key->value = (ufbx_real)next_value;
		char mode = (char)data[2];
		data += 3;

		float slope_right = 0.0f;
		float weight_right = 0.333333f;
		float next_slope_left = 0.0f;
		float next_weight_left = 0.333333f;

		if (mode == 'U') {
			// Cubic interpolation: At least 4 parameters: mode/broken (ignored), RightSlope, NextLeftSlope, weight mode
			key->interpolation = UFBX_INTERPOLATION_CUBIC;
			ufbxi_check(data_end - data >= 4);
			slope_right = (float)data[1];
			next_slope_left = (float)data[2];
			char weight_mode = (char)data[3];
			data += 4;

			if (weight_mode == 'n') {
				// Automatic weights (0.3333...)
			} else if (weight_mode == 'a') {
				// Manual weights: RightWeight, NextLeftWeight
				ufbxi_check(data_end - data >= 2);
				weight_right = (float)data[0];
				next_weight_left = (float)data[1];
				data += 2;
			}

		} else if (mode == 'L') {
			// Linear interpolation: No parameters
			key->interpolation = UFBX_INTERPOLATION_LINEAR;
		} else if (mode == 'C') {
			// Constant interpolation: Single parameter (use prev/next)
			ufbxi_check(data_end - data >= 1);
			key->interpolation = (char)data[0] == 'n' ? UFBX_INTERPOLATION_CONSTANT_NEXT : UFBX_INTERPOLATION_CONSTANT_PREV;
			data += 1;
		} else {
			ufbxi_fail("Unknown key mode");
		}

		// Retrieve next key and value
		if (i + 1 < num_keys) {
			ufbxi_check(data_end - data >= 2);
			next_time = data[0] * uc->ktime_to_sec;
			next_value = data[1];
		}

		// Set up linear cubic tangents if necessary
		if (key->interpolation == UFBX_INTERPOLATION_LINEAR) {
			if (next_time > key->time) {
				double slope = (next_value - key->value) / (next_time - key->time);
				slope_right = next_slope_left = (float)slope;
			} else {
				slope_right = next_slope_left = 0.0f;
			}
		}

		if (key->time > prev_time) {
			key->left.dx = (float)(weight_left * (key->time - prev_time));
			key->left.dy = key->left.dx * slope_left;
		} else {
			key->left.dx = 0.0f;
			key->left.dy = 0.0f;
		}

		if (next_time > key->time) {
			key->right.dx = (float)(weight_right * (next_time - key->time));
			key->right.dy = key->right.dx * slope_right;
		} else {
			key->right.dx = 0.0f;
			key->right.dy = 0.0f;
		}

		slope_left = next_slope_left;
		weight_left = next_weight_left;
		prev_time = key->time;
	}

	ufbxi_check(data == data_end);

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_take_prop_channel(ufbxi_context *uc, ufbxi_node *node, uint64_t node_id, uint64_t layer_id, const char *name)
{
	if (name == ufbxi_Transform) {
		// Pre-7000 have transform keyframes in a deeply nested structure,
		// flatten it to make it resemble post-7000 structure a bit closer:
		// old: Model: { Channel: "Transform" { Channel: "T" { Channel "X": { ... } } } }
		// new: Model: { Channel: "Lcl Translation" { Channel "X": { ... } } }

		ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
			if (child->name != ufbxi_Channel) continue;

			const char *old_name;
			ufbxi_check(ufbxi_get_val1(child, "C", (char**)&old_name));

			const char *new_name = NULL;
			if (old_name == ufbxi_T) new_name = ufbxi_Lcl_Translation;
			else if (old_name == ufbxi_R) new_name = ufbxi_Lcl_Rotation;
			else if (old_name == ufbxi_S) new_name = ufbxi_Lcl_Scaling;
			else {
				continue;
			}

			// Read child as a top-level property channel
			ufbxi_check(ufbxi_read_take_prop_channel(uc, child, node_id, layer_id, new_name));
		}

	} else {

		// Find 1-3 channel nodes thast contain a `Key:` node
		ufbxi_node *channel_nodes[3] = { 0 };
		const char *channel_names[3] = { 0 };
		size_t num_channel_nodes = 0;

		if (ufbxi_find_child(node, ufbxi_Key) || ufbxi_find_child(node, ufbxi_Default)) {
			// Channel has only a single curve
			channel_nodes[0] = node;
			channel_names[0] = name;
			num_channel_nodes = 1;
		} else {
			// Channel is a compound of multiple curves
			ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
				if (child->name != ufbxi_Channel) continue;
				if (!ufbxi_find_child(child, ufbxi_Key) && !ufbxi_find_child(child, ufbxi_Default)) continue;
				if (!ufbxi_get_val1(child, "C", (char**)&channel_names[num_channel_nodes])) continue;
				channel_nodes[num_channel_nodes] = child;
				if (++num_channel_nodes == 3) break;
			}
		}

		// Early return: No valid channels found, not an error
		if (num_channel_nodes == 0) return 1;

		ufbx_anim_prop *prop = ufbxi_push_zero(&uc->tmp_arr_anim_props, ufbx_anim_prop, 1);
		ufbxi_check(prop);
		prop->name.data = name;
		prop->name.length = strlen(name);
		prop->imp_key = ufbxi_get_name_key(name, prop->name.length);

		// Add a "virtual" connection between the animated property and the layer/node
		uint64_t id = (uintptr_t)prop;
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_PROP, id, uc->tmp_arr_anim_props.num_items - 1));
		ufbxi_check(ufbxi_add_connection(uc, layer_id, id, name));
		ufbxi_check(ufbxi_add_connection(uc, node_id, id, name));

		for (size_t i = 0; i < num_channel_nodes; i++) {
			ufbxi_check(ufbxi_read_take_anim_channel(uc, channel_nodes[i], id, channel_names[i], &prop->defaults[i]));
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_take_object(ufbxi_context *uc, ufbxi_node *node, uint64_t layer_id)
{
	// Takes are used only in pre-7000 FBX versions so objects are identified
	// by their unique Type::Name pair that we use as unique IDs through the
	// pooled interned string pointers.
	const char *type_and_name;
	ufbxi_check(ufbxi_get_val1(node, "C", (char**)&type_and_name));
	uint64_t node_id = (uintptr_t)type_and_name;

	// Add all suitable Channels as animated properties
	ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
		const char *name;
		if (child->name != ufbxi_Channel) continue;
		if (!ufbxi_get_val1(child, "C", (char**)&name)) continue;

		ufbxi_check(ufbxi_read_take_prop_channel(uc, child, node_id, layer_id, name));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_take(ufbxi_context *uc, ufbxi_node *node)
{
	// Treat the Take as a post-7000 version animation layer.
	ufbx_anim_layer *layer = ufbxi_push_zero(&uc->tmp_arr_anim_layers, ufbx_anim_layer, 1);
	ufbxi_check(layer);
	ufbxi_check(ufbxi_get_val1(node, "S", &layer->name));

	// Add a "virtual" connectable layer instead of connecting all the animated
	// properties and curves directly to keep the code consistent with post-7000.
	uint64_t layer_id = (uintptr_t)layer;
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_LAYER, layer_id, uc->tmp_arr_anim_layers.num_items - 1));

	// Read all properties of objects included in the take
	ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
		// TODO: Do some object types have another name?
		if (child->name != ufbxi_Model) continue;

		ufbxi_check(ufbxi_read_take_object(uc, child, layer_id));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_takes(ufbxi_context *uc, ufbxi_node *takes)
{
	ufbxi_for(ufbxi_node, node, takes->children, takes->num_children) {
		if (node->name == ufbxi_Take) {
			ufbxi_check(ufbxi_read_take(uc, node));
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_connections(ufbxi_context *uc, ufbxi_node *connections)
{
	// Retain attributes to temporary memory, no more attributes should be added after this
	// point and we need the array to connect the attributes to the node IDs.
	{
		uc->attributes = ufbxi_make_array(&uc->tmp_arr_attributes, ufbxi_attribute, uc->tmp_arr_attributes.num_items);
		ufbxi_check(uc->attributes);
	}

	// Read the connections to the list first
	ufbxi_for(ufbxi_node, node, connections->children, connections->num_children) {
		const char *type;

		uint64_t parent_id, child_id;
		if (uc->version < 7000) {
			const char *parent_name, *child_name;
			// Pre-7000 versions use Type::Name pairs as identifiers
			if (!ufbxi_get_val3(node, "CCC", (char**)&type, (char**)&child_name, (char**)&parent_name)) continue;
			parent_id = (uintptr_t)parent_name;
			child_id = (uintptr_t)child_name;
		} else {
			// Post-7000 versions use proper unique 64-bit IDs
			if (!ufbxi_get_val3(node, "CLL", (char**)&type, &child_id, &parent_id)) continue;
		}

		const char *prop = NULL;
		if (type == ufbxi_OP) {
			ufbxi_check(ufbxi_get_val_at(node, 3, 'C', (char**)&prop));
		}

		// Connect attributes to node IDs
		ufbxi_connectable *child = ufbxi_find_connectable(uc, child_id);
		if (child && child->type == UFBXI_CONNECTABLE_ATTRIBUTE) {
			ufbxi_connectable *parent = ufbxi_find_connectable(uc, parent_id);
			ufbxi_attribute *attr = &uc->attributes[child->index];
			switch (parent->type) {
			case UFBXI_CONNECTABLE_MODEL:
			case UFBXI_CONNECTABLE_MESH:
			case UFBXI_CONNECTABLE_LIGHT:
				attr->parent_type = parent->type;
				attr->parent_index = parent->index;
				break;
			default: break;
			}
		}

		ufbxi_check(ufbxi_add_connection(uc, parent_id, child_id, prop));
	}


	return 1;
}

ufbxi_nodiscard static int ufbxi_read_root(ufbxi_context *uc)
{
	// Initialize root node before reading any models
	{
		uint64_t root_id = 0;

		if (uc->version >= 7000) {
			// Post-7000: Try to find the first document node and root ID.
			// TODO: Multiple documents / roots?
			ufbxi_node *documents = ufbxi_find_child(&uc->root, ufbxi_Documents);
			if (documents) {
				ufbxi_node *document = ufbxi_find_child(documents, ufbxi_Document);
				if (document) {
					ufbxi_ignore(ufbxi_find_val1(document, ufbxi_RootNode, "L", &root_id));
				}
			}
		} else {
			// Pre-7000: Root node has a specific type-name pair "Model::Scene"
			// (or reversed in binary). Use the interned name as ID as usual.
			const char *root_name = uc->from_ascii ? "Model::Scene" : "Scene\x00\x01Model";
			root_name = ufbxi_push_string_imp(uc, root_name, 12, false);
			ufbxi_check(root_name);
			root_id = (uintptr_t)root_name;
		}

		// The root is always the first model in the array
		ufbx_assert(uc->tmp_arr_models.size == 0);
		ufbx_model *model = ufbxi_push_zero(&uc->tmp_arr_models, ufbx_model, 1);
		ufbxi_check(model);
		model->node.type = UFBX_NODE_MODEL;
		model->node.name.data = "Root";
		model->node.name.length = 4;

		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_MODEL, root_id, 0));
	}

	// FBXHeaderExtension: Some metadata (optional)
	ufbxi_node *header = ufbxi_find_child(&uc->root, ufbxi_FBXHeaderExtension);
	if (header) {
		ufbxi_check(ufbxi_read_header_extension(uc, header));
	}

	// Definitions: Object type counts and property templates (optional)
	ufbxi_node *definitions = ufbxi_find_child(&uc->root, ufbxi_Definitions);
	if (definitions) {
		ufbxi_check(ufbxi_read_definitions(uc, definitions));
	}

	// Objects: Actual scene data (required)
	ufbxi_node *objects = ufbxi_find_child(&uc->root, ufbxi_Objects);
	ufbxi_check(objects);
	ufbxi_check(ufbxi_read_objects(uc, objects));

	// Takes: Pre-7000 animations, don't even try to read them in
	// post-7000 versions as the code has some assumptions about the version.
	if (uc->version < 7000) {
		ufbxi_node *takes = ufbxi_find_child(&uc->root, ufbxi_Takes);
		if (takes) {
			ufbxi_check(ufbxi_read_takes(uc, takes));
		}
	}

	// Connections: Relationships between nodes (required)
	ufbxi_node *connections = ufbxi_find_child(&uc->root, ufbxi_Connections);
	ufbxi_check(connections);
	ufbxi_check(ufbxi_read_connections(uc, connections));

	return 1;
}

// -- Loading

typedef struct {
	void *data;
	size_t size;
} ufbxi_void_array;

typedef struct {
	ufbx_node *node;
	ufbx_model *model;
	ufbx_mesh *mesh;
	ufbx_light *light;
	ufbx_anim_layer *anim_layer;
	ufbx_anim_prop *anim_prop;
	ufbx_anim_curve *anim_curve;
	ufbxi_attribute *attribute;
} ufbxi_connectable_data;

ufbxi_nodiscard static int ufbxi_retain_array(ufbxi_context *uc, size_t size, void *p_array, ufbxi_buf *buf)
{
	ufbxi_void_array *arr = (ufbxi_void_array*)p_array;
	arr->size = buf->num_items;
	arr->data = ufbxi_push_pop_size(&uc->result, buf, size, buf->num_items);
	ufbxi_check(arr->data);

	ufbxi_buf_free(buf);

	return 1;
}

ufbxi_nodiscard static int ufbxi_find_connectable_data(ufbxi_context *uc, ufbxi_connectable_data *data, uint64_t id)
{
	ufbxi_connectable *conn = ufbxi_find_connectable(uc, id);
	if (!conn) return 0;

	memset(data, 0, sizeof(ufbxi_connectable_data));

	ufbxi_connectable_type type = conn->type;
	uint32_t index = conn->index;

	// "Proxy" attribute connections to the nodes they are connected to
	if (type == UFBXI_CONNECTABLE_ATTRIBUTE) {
		ufbxi_attribute *attr = &uc->attributes[index];
		data->attribute = attr;
		type = attr->parent_type;
		index = attr->parent_index;
	}

	switch (type) {
	case UFBXI_CONNECTABLE_UNKNOWN:
		// Nop
		break;
	case UFBXI_CONNECTABLE_MODEL:
		data->model = &uc->scene.models.data[index];
		data->node = &data->model->node;
		break;
	case UFBXI_CONNECTABLE_MESH:
		data->mesh = &uc->scene.meshes.data[index];
		data->node = &data->mesh->node;
		break;
	case UFBXI_CONNECTABLE_LIGHT:
		data->light = &uc->scene.lights.data[index];
		data->node = &data->light->node;
		break;
	case UFBXI_CONNECTABLE_GEOMETRY:
		// TODO
		break;
	case UFBXI_CONNECTABLE_ANIM_LAYER:
		data->anim_layer = &uc->scene.anim_layers.data[index];
		break;
	case UFBXI_CONNECTABLE_ANIM_PROP:
		data->anim_prop = &uc->scene.anim_props.data[index];
		break;
	case UFBXI_CONNECTABLE_ANIM_CURVE:
		data->anim_curve = &uc->scene.anim_curves.data[index];
		break;
	case UFBXI_CONNECTABLE_ATTRIBUTE:
		// Handled above
		break;
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_collect_nodes(ufbxi_context *uc, size_t size, ufbx_node ***p_dst, void *data, size_t num)
{
	// Collect pointers to the node headers of unknown node "sub-classes"
	ufbx_node **dst = *p_dst;
	char *ptr = (char*)data;
	for (size_t i = 0; i < num; i++) {
		ufbx_node *node = (ufbx_node*)ptr;

		// Allocate space for children and reset count
		node->children.data = ufbxi_push(&uc->result, ufbx_node*, node->children.size);
		node->children.size = 0;

		*dst++ = (ufbx_node*)node;
		ptr += size;
	}
	*p_dst = dst;

	return 1;
}

ufbxi_nodiscard static int ufbxi_finalize_scene(ufbxi_context *uc)
{
	// Retrieve all temporary arrays
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_model), &uc->scene.models, &uc->tmp_arr_models));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_mesh), &uc->scene.meshes, &uc->tmp_arr_meshes));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_light), &uc->scene.lights, &uc->tmp_arr_lights));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_anim_layer), &uc->scene.anim_layers, &uc->tmp_arr_anim_layers));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_anim_prop), &uc->scene.anim_props, &uc->tmp_arr_anim_props));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_anim_curve), &uc->scene.anim_curves, &uc->tmp_arr_anim_curves));

	// Linearize the connections into an array for processing. This includes both
	// connections read from `Connections` and "virtual" connections added elsewhere using
	// `ufbxi_add_connection()`
	size_t num_conns = uc->tmp_connection.num_items;
	ufbxi_connection *conns = ufbxi_make_array(&uc->tmp_connection, ufbxi_connection, num_conns);
	ufbxi_check(conns);

	// Process the connections
	ufbxi_for(ufbxi_connection, conn, conns, num_conns) {
		ufbxi_connectable_data parent, child;
		if (!ufbxi_find_connectable_data(uc, &parent, conn->parent_id)) continue;
		if (!ufbxi_find_connectable_data(uc, &child, conn->child_id)) continue;
		const char *prop = conn->prop_name;

		if (parent.node) {
			// We need to be careful not to parent objects to themselves, which could
			// happen as we "proxy" attribute connections to their nodes.
			if (child.node && child.node != parent.node) {
				child.node->parent = parent.node;
				parent.node->children.size++;
			}
		}

		if (parent.node) {
			if (child.anim_prop) {
				if (prop) {
					size_t len = strlen(prop);
					child.anim_prop->node = parent.node;
					child.anim_prop->name.data = prop;
					child.anim_prop->name.length = strlen(prop);
					child.anim_prop->imp_key = ufbxi_get_name_key(prop, len);
				} else {
					child.anim_prop->name.data = "";
				}
			}
		}

		if (parent.anim_layer) {
			if (child.anim_prop) {
				parent.anim_layer->props.size++;
				child.anim_prop->layer = parent.anim_layer;
			}
		}

		if (parent.anim_prop) {
			if (child.anim_curve) {
				child.anim_curve->prop = parent.anim_prop;

				size_t index = 0;
				if (prop) {
					if (prop == ufbxi_Y || prop == ufbxi_D_Y) index = 1;
					if (prop == ufbxi_Z || prop == ufbxi_D_Z) index = 2;
				}

				parent.anim_prop->curves[index] = *child.anim_curve;
			}
		}
	}

	// Allocate storage for child arrays
	ufbxi_for(ufbx_anim_layer, layer, uc->scene.anim_layers.data, uc->scene.anim_layers.size) {
		layer->props.data = ufbxi_push(&uc->result, ufbx_anim_prop, layer->props.size);
		layer->props.size = 0;
	}

	// Add all nodes to the scenes node list
	size_t num_nodes = uc->scene.models.size + uc->scene.meshes.size + uc->scene.lights.size;
	ufbx_node **nodes = ufbxi_push(&uc->result, ufbx_node*, num_nodes);
	ufbxi_check(nodes);
	uc->scene.nodes.data = nodes;
	uc->scene.nodes.size = num_nodes;
	ufbxi_check(ufbxi_collect_nodes(uc, sizeof(ufbx_model), &nodes, uc->scene.models.data, uc->scene.models.size));
	ufbxi_check(ufbxi_collect_nodes(uc, sizeof(ufbx_mesh), &nodes, uc->scene.meshes.data, uc->scene.meshes.size));
	ufbxi_check(ufbxi_collect_nodes(uc, sizeof(ufbx_light), &nodes, uc->scene.lights.data, uc->scene.lights.size));

	// Fill child arrays
	ufbxi_for_ptr(ufbx_node, p_node, uc->scene.nodes.data, uc->scene.nodes.size) {
		ufbx_node *parent = (*p_node)->parent;
		if (!parent) continue;
		parent->children.data[parent->children.size++] = *p_node;
	}
	ufbxi_for(ufbx_anim_prop, prop, uc->scene.anim_props.data, uc->scene.anim_props.size) {
		ufbx_anim_layer *layer = prop->layer;
		if (!layer) continue;
		layer->props.data[layer->props.size++] = *prop;
	}

	uc->scene.root = &uc->scene.models.data[0];

	return 1;
}

ufbxi_nodiscard static int ufbxi_load_imp(ufbxi_context *uc)
{
	ufbxi_check(ufbxi_load_strings(uc));
	ufbxi_check(ufbxi_load_maps(uc));
	ufbxi_check(ufbxi_parse(uc));
	ufbxi_check(ufbxi_read_root(uc));
	ufbxi_check(ufbxi_finalize_scene(uc));

	// Copy local data to the scene
	uc->scene.metadata.version = uc->version;
	uc->scene.metadata.ascii = uc->from_ascii;

	// Retain the scene, this must be the final allocation as we copy
	// `ator_result` to `ufbx_scene_imp`.
	ufbxi_scene_imp *imp = ufbxi_push(&uc->result, ufbxi_scene_imp, 1);
	ufbxi_check(imp);

	imp->magic = UFBXI_SCENE_IMP_MAGIC;
	imp->scene = uc->scene;
	imp->ator = uc->ator_result;
	imp->ator.error = NULL;
	imp->result_buf = uc->result;
	imp->result_buf.ator = &imp->ator;
	imp->string_buf = uc->string_buf;
	imp->string_buf.ator = &imp->ator;

	uc->scene_imp = imp;

	return 1;
}

static void ufbxi_free_temp(ufbxi_context *uc)
{
	ufbxi_buf_free(&uc->tmp);
	ufbxi_buf_free(&uc->tmp_node);
	ufbxi_buf_free(&uc->tmp_template);
	ufbxi_map_free(&uc->string_map);
	ufbxi_free(&uc->ator_tmp, char, uc->read_buffer, uc->read_buffer_size);
	ufbxi_free(&uc->ator_tmp, char, uc->convert_buffer, uc->convert_buffer_size);
}

static void ufbxi_free_result(ufbxi_context *uc)
{
	ufbxi_buf_free(&uc->result);
	ufbxi_buf_free(&uc->string_buf);
}

#define ufbxi_default_opt(name, value) if (!opts->name) opts->name = value

static void ufbxi_expand_defaults(ufbx_load_opts *opts)
{
	ufbxi_default_opt(max_temp_memory, 0x10000000);
	ufbxi_default_opt(max_result_memory, 0x10000000);
	ufbxi_default_opt(max_ascii_token_length, 0x10000000);
	ufbxi_default_opt(read_buffer_size, 4096);
	ufbxi_default_opt(max_properties, 0x10000000);
	ufbxi_default_opt(max_string_length, 0x10000000);
	ufbxi_default_opt(max_strings, 0x10000000);
	ufbxi_default_opt(max_node_depth, 0x10000000);
	ufbxi_default_opt(max_node_values, 0x10000000);
	ufbxi_default_opt(max_node_children, 0x10000000);
	ufbxi_default_opt(max_array_size, 0x10000000);
}

static ufbx_scene *ufbxi_load(ufbxi_context *uc, const ufbx_load_opts *user_opts, ufbx_error *p_error)
{
	// Test endianness
	{
		uint8_t buf[2];
		uint16_t val = 0xbbaa;
		memcpy(buf, &val, 2);
		uc->big_endian = buf[0] == 0xbb;
	}

	if (user_opts) {
		uc->opts = *user_opts;
	} else {
		memset(&uc->opts, 0, sizeof(uc->opts));
	}
	ufbxi_expand_defaults(&uc->opts);

	ufbx_inflate_retain inflate_retain;
	inflate_retain.initialized = false;

	// Setup allocators
	uc->ator_tmp.error = &uc->error;
	uc->ator_tmp.ator = uc->opts.temp_allocator;
	uc->ator_tmp.max_size = uc->opts.max_temp_memory;
	uc->ator_result.error = &uc->error;
	uc->ator_result.ator = uc->opts.result_allocator;
	uc->ator_result.max_size = uc->opts.max_result_memory;

	uc->string_map.ator = &uc->ator_tmp;
	uc->prop_type_map.ator = &uc->ator_tmp;
	uc->connectable_map.ator = &uc->ator_tmp;

	uc->tmp.ator = &uc->ator_tmp;
	uc->tmp_node.ator = &uc->ator_tmp;
	uc->tmp_template.ator = &uc->ator_tmp;
	uc->tmp_connection.ator = &uc->ator_tmp;
	uc->tmp_arr_models.ator = &uc->ator_tmp;
	uc->tmp_arr_meshes.ator = &uc->ator_tmp;
	uc->tmp_arr_lights.ator = &uc->ator_tmp;
	uc->tmp_arr_anim_layers.ator = &uc->ator_tmp;
	uc->tmp_arr_anim_props.ator = &uc->ator_tmp;
	uc->tmp_arr_anim_curves.ator = &uc->ator_tmp;
	uc->tmp_arr_attributes.ator = &uc->ator_tmp;

	uc->result.ator = &uc->ator_result;
	uc->string_buf.ator = &uc->ator_result;

	uc->inflate_retain = &inflate_retain;

	if (ufbxi_load_imp(uc)) {
		if (p_error) p_error->stack_size = 0;
		ufbxi_free_temp(uc);
		return &uc->scene_imp->scene;
	} else {
		if (p_error) *p_error = uc->error;
		ufbxi_free_temp(uc);
		ufbxi_free_result(uc);
		return NULL;
	}
}

// -- File IO

static size_t ufbxi_file_read(void *user, void *data, size_t max_size)
{
	FILE *file = (FILE*)user;
	return fread(data, 1, max_size, file);
}

// -- API

#ifdef __cplusplus
extern "C" {
#endif

const ufbx_string ufbx_empty_string = { "", 0 };

ufbx_scene *ufbx_load_memory(const void *data, size_t size, const ufbx_load_opts *opts, ufbx_error *error)
{
	ufbxi_context uc = { 0 };
	uc.data_begin = uc.data = (const char *)data;
	uc.data_size = size;
	return ufbxi_load(&uc, opts, error);
}

ufbx_scene *ufbx_load_file(const char *filename, const ufbx_load_opts *opts, ufbx_error *error)
{
	FILE *file;
	#ifdef _WIN32
		if (fopen_s(&file, filename, "rb")) file = NULL;
	#else
		file = fopen(filename, "rb");
	#endif
	if (!file) {
		if (error) {
			error->stack_size = 1;
			error->stack[0].description = "File not found";
			error->stack[0].function = __FUNCTION__;
			error->stack[0].source_line = __LINE__;
		}
		return NULL;
	}

	ufbxi_context uc = { 0 };
	uc.read_fn = &ufbxi_file_read;
	uc.read_user = file;
	ufbx_scene *scene = ufbxi_load(&uc, opts, error);

	fclose(file);

	return scene;
}

void ufbx_free_scene(ufbx_scene *scene)
{
	if (!scene) return;

	ufbxi_scene_imp *imp = (ufbxi_scene_imp*)scene;
	ufbx_assert(imp->magic == UFBXI_SCENE_IMP_MAGIC);
	if (imp->magic != UFBXI_SCENE_IMP_MAGIC) return;

	ufbxi_buf_free(&imp->string_buf);

	// We need to free `result_buf` last and be careful to copy it to
	// the stack since the `ufbxi_scene_imp` that contains it is allocated
	// from the same result buffer!
	ufbxi_buf result = imp->result_buf;
	ufbxi_buf_free(&result);
}

ufbx_mesh *ufbx_find_mesh_len(const ufbx_scene *scene, const char *name, size_t name_len)
{
	ufbxi_for(ufbx_mesh, mesh, scene->meshes.data, scene->meshes.size) {
		if (mesh->node.name.length == name_len && !memcmp(mesh->node.name.data, name, name_len)) {
			return mesh;
		}
	}
	return NULL;
}

#ifdef __cplusplus
}
#endif

#endif
