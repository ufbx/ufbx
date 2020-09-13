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
#include <math.h>

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

#if defined(__GNUC__) && !defined(__clang__)
	#define ufbxi_ignore(cond) (void)!(cond)
#else
	#define ufbxi_ignore(cond) (void)(cond)
#endif

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable: 4200) // nonstandard extension used: zero-sized array in struct/union
	#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
	#pragma warning(disable: 4127) // conditional expression is constant
	#pragma warning(disable: 4706) // assignment within conditional expression
	#pragma warning(disable: 4789) // buffer 'type_and_name' of size 8 bytes will be overrun; 16 bytes will be written starting at offset 0
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

#define ufbxi_read_u8(ptr) (*(const uint8_t*)(ptr))

#if (defined(_M_IX86) || defined(__i386__) || defined(_M_X64) || defined(__x86_64__) || defined(_M_ARM64) || defined(__aarch64__)) && !defined(UFBX_NO_UNALIGNED_LOADS)
	#define ufbxi_read_u16(ptr) (*(const uint16_t*)(ptr))
	#define ufbxi_read_u32(ptr) (*(const uint32_t*)(ptr))
	#define ufbxi_read_u64(ptr) (*(const uint64_t*)(ptr))
	#define ufbxi_read_f32(ptr) (*(const float*)(ptr))
	#define ufbxi_read_f64(ptr) (*(const double*)(ptr))
#elif defined(__wasm__) || defined(__EMSCRIPTEN__)
	#define ufbxi_read_u16(ptr) ((uint16_t)*(const unsigned short ((aligned(1)))*)(ptr))
	#define ufbxi_read_u32(ptr) ((uint32_t)*(const unsigned int ((aligned(1)))*)(ptr))
	#define ufbxi_read_u64(ptr) ((uint64_t)*(const unsigned long long ((aligned(1)))*)(ptr))
	#define ufbxi_read_f32(ptr) ((float)*(const float ((aligned(1)))*)(ptr))
	#define ufbxi_read_f64(ptr) ((double)*(const double ((aligned(1)))*)(ptr))
#else
	static ufbxi_forceinline uint16_t ufbxi_read_u16(const void *ptr) {
		const char *p = ptr;
		return (uint16_t)(
			(unsigned)(uint8_t)p[0] << 0u |
			(unsigned)(uint8_t)p[1] << 8u );
	}
	static ufbxi_forceinline uint32_t ufbxi_read_u32(const void *ptr) {
		const char *p = ptr;
		return (uint32_t)(
			(unsigned)(uint8_t)p[0] <<  0u |
			(unsigned)(uint8_t)p[1] <<  8u |
			(unsigned)(uint8_t)p[2] << 16u |
			(unsigned)(uint8_t)p[3] << 24u );
	}
	static ufbxi_forceinline uint64_t ufbxi_read_u64(const void *ptr) {
		const char *p = ptr;
		return (uint64_t)(
			(uint64_t)(uint8_t)p[0] <<  0u |
			(uint64_t)(uint8_t)p[1] <<  8u |
			(uint64_t)(uint8_t)p[2] << 16u |
			(uint64_t)(uint8_t)p[3] << 24u |
			(uint64_t)(uint8_t)p[4] << 32u |
			(uint64_t)(uint8_t)p[5] << 40u |
			(uint64_t)(uint8_t)p[6] << 48u |
			(uint64_t)(uint8_t)p[7] << 56u );
	}
	static ufbxi_forceinline float ufbxi_read_f32(const void *ptr) {
		uint32_t u = ufbxi_read_u32(ptr);
		float f;
		memcpy(&f, &u, 4);
		return f;
	}
	static ufbxi_forceinline double ufbxi_read_f64(const void *ptr) {
		uint64_t u = ufbxi_read_u64(ptr);
		double f;
		memcpy(&f, &u, 8);
		return f;
	}
#endif

#define ufbxi_read_i8(ptr) (int8_t)(ufbxi_read_u8(ptr))
#define ufbxi_read_i16(ptr) (int16_t)(ufbxi_read_u16(ptr))
#define ufbxi_read_i32(ptr) (int32_t)(ufbxi_read_u32(ptr))
#define ufbxi_read_i64(ptr) (int64_t)(ufbxi_read_u64(ptr))

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
				rhmap_iter iter;
				iter.map = map;
				iter.hash = hash;
				iter.scan = 0;
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
	memmove(s->buffer, ptr, left);

	// Read more user data if the user supplied a `read_fn()`, otherwise
	// we assume the initial data chunk is the whole input buffer.
	if (s->read_fn) {
		size_t to_read = ufbxi_min_sz(s->input_left, s->buffer_size - left);
		if (to_read > 0) {
			size_t num_read = s->read_fn(s->read_user, s->buffer + left, to_read);
			if (num_read > to_read) num_read = 0;
			ufbx_assert(s->input_left >= num_read);
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
	size_t data_size = input->data_size;
	if (data_size > input->total_size) {
		data_size = input->total_size;
	}

	s->read_fn = input->read_fn;
	s->read_user = input->read_user;
	s->chunk_begin = (const char*)input->data;
	s->chunk_ptr = (const char*)input->data;
	s->chunk_end = (const char*)input->data + data_size - 8;
	s->chunk_real_end = (const char*)input->data + data_size;
	s->input_left = input->total_size - data_size;

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
	if (data_size < 64) {
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
		len -= chunk_left;
	}

	// Read extra bytes from user
	if (len > s->input_left) return 0;
	size_t num_read = 0;
	if (s->read_fn) {
		num_read = s->read_fn(s->read_user, ptr, len);
		s->input_left -= num_read;
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
	// break at the first error
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
	size_t allocs_left;
	size_t huge_size;
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
	ufbxi_check_return_err(ator->error, ator->allocs_left > 1, NULL);
	ator->allocs_left--;

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
	ufbxi_check_return_err(ator->error, ator->allocs_left > 1, NULL);
	ator->allocs_left--;

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
	ufbx_assert(ptr);

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

ufbxi_nodiscard static ufbxi_forceinline void *ufbxi_alloc_zero_size(ufbxi_allocator *ator, size_t size, size_t n)
{
	void *ptr = ufbxi_alloc_size(ator, size, n);
	if (ptr) memset(ptr, 0, size * n);
	return ptr;
}

ufbxi_nodiscard static ufbxi_forceinline void *ufbxi_realloc_zero_size(ufbxi_allocator *ator, size_t size, void *old_ptr, size_t old_n, size_t n)
{
	void *ptr = ufbxi_realloc_size(ator, size, old_ptr, old_n, n);
	if (ptr && n > old_n) memset((char*)ptr + size*old_n, 0, size*(n - old_n));
	return ptr;
}

ufbxi_nodiscard static bool ufbxi_grow_array_size(ufbxi_allocator *ator, size_t size, void *p_ptr, size_t *p_cap, size_t n)
{
	void *ptr = *(void**)p_ptr;
	size_t old_n = *p_cap;
	if (old_n >= n) return true;
	size_t new_n = ufbxi_max_sz(old_n * 2, n);
	void *new_ptr = ufbxi_realloc_size(ator, size, ptr, old_n, new_n);
	if (!new_ptr) return false;
	*(void**)p_ptr = new_ptr;
	*p_cap = new_n;
	return true;
}

ufbxi_nodiscard static bool ufbxi_grow_array_zero_size(ufbxi_allocator *ator, size_t size, void *p_ptr, size_t *p_cap, size_t n)
{
	void *ptr = *(void**)p_ptr;
	size_t old_n = *p_cap;
	if (old_n >= n) return true;
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

typedef struct {
	ufbxi_buf_chunk *chunk;
	uint32_t pos;
	size_t num_items;
} ufbxi_buf_state;

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
	uint32_t chunk_size, next_size;

	// If `size` is larger than `huge_size` don't grow `next_size` geometrically,
	// but use a dedicated allocation.
	if (size >= b->ator->huge_size) {
		 next_size = chunk ? chunk->next_size : 4096;
		 chunk_size = (uint32_t)size;
	} else {
		 next_size = chunk ? chunk->next_size * 2 : 4096;
		chunk_size = next_size - sizeof(ufbxi_buf_chunk);
		if (chunk_size < size) chunk_size = (uint32_t)size;
	}

	// Align chunk sizes to 16 bytes
	chunk_size = ufbxi_align_to_mask(chunk_size, 0xf);
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

static void *ufbxi_push_size(ufbxi_buf *b, size_t size, size_t n)
{
	// Always succeed with an emtpy non-NULL buffer for empty allocations
	ufbx_assert(size > 0);
	if (n == 0) return ufbxi_zero_size_buffer;

	b->num_items += n;

	size_t total = size * n;
	if (ufbxi_does_overflow(total, size, n)) return NULL;
	if (b->ator->allocs_left <= 1) return NULL;
	b->ator->allocs_left--;

	// Align to the natural alignment based on the size
	uint32_t align_mask = ufbxi_size_align_mask(size);
	uint32_t pos = ufbxi_align_to_mask(b->pos, align_mask);

	// Try to push to the current block. Allocate a new block
	// if the aligned size doesn't fit.
	if (total <= (size_t)(b->size - pos)) {
		b->pos = (uint32_t)(pos + total);
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
	if (b->ator->allocs_left <= 1) return NULL;
	b->ator->allocs_left--;

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
		buf->size = buf->chunk->size;
		buf->num_items = 0;
	}
}

static ufbxi_forceinline ufbxi_buf_state ufbxi_buf_push_state(ufbxi_buf *buf)
{
	ufbxi_buf_state s;
	s.chunk = buf->chunk;
	s.pos = buf->pos;
	s.num_items = buf->num_items;
	return s;
}

static void ufbxi_buf_pop_state(ufbxi_buf *buf, const ufbxi_buf_state *state)
{
	if (state->chunk) {
		buf->chunk = state->chunk;
	} else if (buf->chunk) {
		buf->chunk = buf->chunk->root;
	}
	if (buf->chunk) {
		buf->size = buf->chunk->size;
	}
	buf->num_items = state->num_items;
	buf->pos = state->pos;
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
	if (map->ator->allocs_left <= 1) return false;
	map->ator->allocs_left--;

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
static const char ufbxi_AnimationStack[] = "AnimationStack";
static const char ufbxi_AnimationLayer[] = "AnimationLayer";
static const char ufbxi_AnimationCurve[] = "AnimationCurve";
static const char ufbxi_AnimationCurveNode[] = "AnimationCurveNode";
static const char ufbxi_Material[] = "Material";
static const char ufbxi_Deformer[] = "Deformer";
static const char ufbxi_SubDeformer[] = "SubDeformer";
static const char ufbxi_Limb[] = "Limb";
static const char ufbxi_LimbNode[] = "LimbNode";
static const char ufbxi_Mesh[] = "Mesh";
static const char ufbxi_Light[] = "Light";
static const char ufbxi_Cluster[] = "Cluster";
static const char ufbxi_Skin[] = "Skin";
static const char ufbxi_Vertices[] = "Vertices";
static const char ufbxi_PolygonVertexIndex[] = "PolygonVertexIndex";
static const char ufbxi_Edges[] = "Edges";
static const char ufbxi_Layer[] = "Layer";
static const char ufbxi_LayerElement[] = "LayerElement";
static const char ufbxi_Type[] = "Type";
static const char ufbxi_TypedIndex[] = "TypedIndex";
static const char ufbxi_LayerElementNormal[] = "LayerElementNormal";
static const char ufbxi_LayerElementBinormal[] = "LayerElementBinormal";
static const char ufbxi_LayerElementTangent[] = "LayerElementTangent";
static const char ufbxi_LayerElementUV[] = "LayerElementUV";
static const char ufbxi_LayerElementColor[] = "LayerElementColor";
static const char ufbxi_LayerElementVertexCrease[] = "LayerElementVertexCrease";
static const char ufbxi_LayerElementEdgeCrease[] = "LayerElementEdgeCrease";
static const char ufbxi_LayerElementSmoothing[] = "LayerElementSmoothing";
static const char ufbxi_LayerElementMaterial[] = "LayerElementMaterial";
static const char ufbxi_Normals[] = "Normals";
static const char ufbxi_NormalIndex[] = "NormalIndex";
static const char ufbxi_Binormals[] = "Binormals";
static const char ufbxi_BinormalIndex[] = "BinormalIndex";
static const char ufbxi_Tangents[] = "Tangents";
static const char ufbxi_TangentIndex[] = "TangentIndex";
static const char ufbxi_UV[] = "UV";
static const char ufbxi_UVIndex[] = "UVIndex";
static const char ufbxi_Colors[] = "Colors";
static const char ufbxi_ColorIndex[] = "ColorIndex";
static const char ufbxi_VertexCrease[] = "VertexCrease";
static const char ufbxi_VertexCreaseIndex[] = "VertexCreaseIndex";
static const char ufbxi_EdgeCrease[] = "EdgeCrease";
static const char ufbxi_Smoothing[] = "Smoothing";
static const char ufbxi_Materials[] = "Materials";
static const char ufbxi_MappingInformationType[] = "MappingInformationType";
static const char ufbxi_Name[] = "Name";
static const char ufbxi_ByVertex[] = "ByVertex";
static const char ufbxi_ByVertice[] = "ByVertice";
static const char ufbxi_ByPolygonVertex[] = "ByPolygonVertex";
static const char ufbxi_ByEdge[] = "ByEdge";
static const char ufbxi_ByPolygon[] = "ByPolygon";
static const char ufbxi_AllSame[] = "AllSame";
static const char ufbxi_KeyTime[] = "KeyTime";
static const char ufbxi_KeyValueFloat[] = "KeyValueFloat";
static const char ufbxi_KeyAttrFlags[] = "KeyAttrFlags";
static const char ufbxi_KeyAttrDataFloat[] = "KeyAttrDataFloat";
static const char ufbxi_KeyAttrRefCount[] = "KeyAttrRefCount";
static const char ufbxi_Take[] = "Take";
static const char ufbxi_LocalTime[] = "LocalTime";
static const char ufbxi_ReferenceTime[] = "ReferenceTime";
static const char ufbxi_LocalStart[] = "LocalStart";
static const char ufbxi_LocalStop[] = "LocalStop";
static const char ufbxi_ReferenceStart[] = "ReferenceStart";
static const char ufbxi_ReferenceStop[] = "ReferenceStop";
static const char ufbxi_Channel[] = "Channel";
static const char ufbxi_Default[] = "Default";
static const char ufbxi_Key[] = "Key";
static const char ufbxi_KeyCount[] = "KeyCount";
static const char ufbxi_T[] = "T";
static const char ufbxi_R[] = "R";
static const char ufbxi_S[] = "S";
static const char ufbxi_X[] = "X";
static const char ufbxi_Y[] = "Y";
static const char ufbxi_Z[] = "Z";
static const char ufbxi_D_X[] = "d|X";
static const char ufbxi_D_Y[] = "d|Y";
static const char ufbxi_D_Z[] = "d|Z";
static const char ufbxi_Indexes[] = "Indexes";
static const char ufbxi_Weights[] = "Weights";
static const char ufbxi_Transform[] = "Transform";
static const char ufbxi_TransformLink[] = "TransformLink";
static const char ufbxi_Lcl_Translation[] = "Lcl Translation";
static const char ufbxi_Lcl_Rotation[] = "Lcl Rotation";
static const char ufbxi_Lcl_Scaling[] = "Lcl Scaling";
static const char ufbxi_PreRotation[] = "PreRotation";
static const char ufbxi_PostRotation[] = "PostRotation";
static const char ufbxi_OO[] = "OO";
static const char ufbxi_OP[] = "OP";
static const char ufbxi_ScalingPivot[] = "ScalingPivot";
static const char ufbxi_RotationPivot[] = "RotationPivot";
static const char ufbxi_ScalingOffset[] = "ScalingOffset";
static const char ufbxi_RotationOffset[] = "RotationOffset";
static const char ufbxi_RotationOrder[] = "RotationOrder";
static const char ufbxi_InheritType[] = "InheritType";
static const char ufbxi_DiffuseColor[] = "DiffuseColor";
static const char ufbxi_SpecularColor[] = "SpecularColor";
static const char ufbxi_Color[] = "Color";
static const char ufbxi_Intensity[] = "Intensity";
static const char ufbxi_Size[] = "Size";
static const char ufbxi_RotationAccumulationMode[] = "RotationAccumulationMode";
static const char ufbxi_ScaleAccumulationMode[] = "ScaleAccumulationMode";
static const char ufbxi_Weight[] = "Weight";

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
	{ ufbxi_AnimationStack, sizeof(ufbxi_AnimationStack) - 1 },
	{ ufbxi_AnimationLayer, sizeof(ufbxi_AnimationLayer) - 1 },
	{ ufbxi_AnimationCurve, sizeof(ufbxi_AnimationCurve) - 1 },
	{ ufbxi_AnimationCurveNode, sizeof(ufbxi_AnimationCurveNode) - 1 },
	{ ufbxi_Material, sizeof(ufbxi_Material) - 1 },
	{ ufbxi_Deformer, sizeof(ufbxi_Deformer) - 1 },
	{ ufbxi_SubDeformer, sizeof(ufbxi_SubDeformer) - 1 },
	{ ufbxi_Limb, sizeof(ufbxi_Limb) - 1 },
	{ ufbxi_LimbNode, sizeof(ufbxi_LimbNode) - 1 },
	{ ufbxi_Mesh, sizeof(ufbxi_Mesh) - 1 },
	{ ufbxi_Light, sizeof(ufbxi_Light) - 1 },
	{ ufbxi_Cluster, sizeof(ufbxi_Cluster) - 1 },
	{ ufbxi_Skin, sizeof(ufbxi_Skin) - 1 },
	{ ufbxi_Vertices, sizeof(ufbxi_Vertices) - 1 },
	{ ufbxi_PolygonVertexIndex, sizeof(ufbxi_PolygonVertexIndex) - 1 },
	{ ufbxi_Edges, sizeof(ufbxi_Edges) - 1 },
	{ ufbxi_Layer, sizeof(ufbxi_Layer) - 1 },
	{ ufbxi_LayerElement, sizeof(ufbxi_LayerElement) - 1 },
	{ ufbxi_Type, sizeof(ufbxi_Type) - 1 },
	{ ufbxi_TypedIndex, sizeof(ufbxi_TypedIndex) - 1 },
	{ ufbxi_LayerElementNormal, sizeof(ufbxi_LayerElementNormal) - 1 },
	{ ufbxi_LayerElementBinormal, sizeof(ufbxi_LayerElementBinormal) - 1 },
	{ ufbxi_LayerElementTangent, sizeof(ufbxi_LayerElementTangent) - 1 },
	{ ufbxi_LayerElementUV, sizeof(ufbxi_LayerElementUV) - 1 },
	{ ufbxi_LayerElementColor, sizeof(ufbxi_LayerElementColor) - 1 },
	{ ufbxi_LayerElementVertexCrease, sizeof(ufbxi_LayerElementVertexCrease) - 1 },
	{ ufbxi_LayerElementEdgeCrease, sizeof(ufbxi_LayerElementEdgeCrease) - 1 },
	{ ufbxi_LayerElementSmoothing, sizeof(ufbxi_LayerElementSmoothing) - 1 },
	{ ufbxi_LayerElementMaterial, sizeof(ufbxi_LayerElementMaterial) - 1 },
	{ ufbxi_Normals, sizeof(ufbxi_Normals) - 1 },
	{ ufbxi_NormalIndex, sizeof(ufbxi_NormalIndex) - 1 },
	{ ufbxi_Binormals, sizeof(ufbxi_Binormals) - 1 },
	{ ufbxi_BinormalIndex, sizeof(ufbxi_BinormalIndex) - 1 },
	{ ufbxi_Tangents, sizeof(ufbxi_Tangents) - 1 },
	{ ufbxi_TangentIndex, sizeof(ufbxi_TangentIndex) - 1 },
	{ ufbxi_UV, sizeof(ufbxi_UV) - 1 },
	{ ufbxi_UVIndex, sizeof(ufbxi_UVIndex) - 1 },
	{ ufbxi_Colors, sizeof(ufbxi_Colors) - 1 },
	{ ufbxi_ColorIndex, sizeof(ufbxi_ColorIndex) - 1 },
	{ ufbxi_VertexCrease, sizeof(ufbxi_VertexCrease) - 1 },
	{ ufbxi_VertexCreaseIndex, sizeof(ufbxi_VertexCreaseIndex) - 1 },
	{ ufbxi_EdgeCrease, sizeof(ufbxi_EdgeCrease) - 1 },
	{ ufbxi_Smoothing, sizeof(ufbxi_Smoothing) - 1 },
	{ ufbxi_Materials, sizeof(ufbxi_Materials) - 1 },
	{ ufbxi_MappingInformationType, sizeof(ufbxi_MappingInformationType) - 1 },
	{ ufbxi_Name, sizeof(ufbxi_Name) - 1 },
	{ ufbxi_ByVertex, sizeof(ufbxi_ByVertex) - 1 },
	{ ufbxi_ByVertice, sizeof(ufbxi_ByVertice) - 1 },
	{ ufbxi_ByPolygonVertex, sizeof(ufbxi_ByPolygonVertex) - 1 },
	{ ufbxi_ByEdge, sizeof(ufbxi_ByEdge) - 1 },
	{ ufbxi_ByPolygon, sizeof(ufbxi_ByPolygon) - 1 },
	{ ufbxi_AllSame, sizeof(ufbxi_AllSame) - 1 },
	{ ufbxi_KeyTime, sizeof(ufbxi_KeyTime) - 1 },
	{ ufbxi_KeyValueFloat, sizeof(ufbxi_KeyValueFloat) - 1 },
	{ ufbxi_KeyAttrFlags, sizeof(ufbxi_KeyAttrFlags) - 1 },
	{ ufbxi_KeyAttrDataFloat, sizeof(ufbxi_KeyAttrDataFloat) - 1 },
	{ ufbxi_KeyAttrRefCount, sizeof(ufbxi_KeyAttrRefCount) - 1 },
	{ ufbxi_Take, sizeof(ufbxi_Take) - 1 },
	{ ufbxi_LocalTime, sizeof(ufbxi_LocalTime) - 1 },
	{ ufbxi_ReferenceTime, sizeof(ufbxi_ReferenceTime) - 1 },
	{ ufbxi_LocalStart, sizeof(ufbxi_LocalStart) - 1 },
	{ ufbxi_LocalStop, sizeof(ufbxi_LocalStop) - 1 },
	{ ufbxi_ReferenceStart, sizeof(ufbxi_ReferenceStart) - 1 },
	{ ufbxi_ReferenceStop, sizeof(ufbxi_ReferenceStop) - 1 },
	{ ufbxi_Channel, sizeof(ufbxi_Channel) - 1 },
	{ ufbxi_Default, sizeof(ufbxi_Default) - 1 },
	{ ufbxi_Key, sizeof(ufbxi_Key) - 1 },
	{ ufbxi_KeyCount, sizeof(ufbxi_KeyCount) - 1 },
	{ ufbxi_T, sizeof(ufbxi_T) - 1 },
	{ ufbxi_R, sizeof(ufbxi_R) - 1 },
	{ ufbxi_S, sizeof(ufbxi_S) - 1 },
	{ ufbxi_X, sizeof(ufbxi_X) - 1 },
	{ ufbxi_Y, sizeof(ufbxi_Y) - 1 },
	{ ufbxi_Z, sizeof(ufbxi_Z) - 1 },
	{ ufbxi_D_X, sizeof(ufbxi_D_X) - 1 },
	{ ufbxi_D_Y, sizeof(ufbxi_D_Y) - 1 },
	{ ufbxi_D_Z, sizeof(ufbxi_D_Z) - 1 },
	{ ufbxi_Indexes, sizeof(ufbxi_Indexes) - 1 },
	{ ufbxi_Weights, sizeof(ufbxi_Weights) - 1 },
	{ ufbxi_Transform, sizeof(ufbxi_Transform) - 1 },
	{ ufbxi_TransformLink, sizeof(ufbxi_TransformLink) - 1 },
	{ ufbxi_Lcl_Translation, sizeof(ufbxi_Lcl_Translation) - 1 },
	{ ufbxi_Lcl_Rotation, sizeof(ufbxi_Lcl_Rotation) - 1 },
	{ ufbxi_Lcl_Scaling, sizeof(ufbxi_Lcl_Scaling) - 1 },
	{ ufbxi_PreRotation, sizeof(ufbxi_PreRotation) - 1 },
	{ ufbxi_PostRotation, sizeof(ufbxi_PostRotation) - 1 },
	{ ufbxi_OO, sizeof(ufbxi_OO) - 1 },
	{ ufbxi_OP, sizeof(ufbxi_OP) - 1 },
	{ ufbxi_ScalingPivot, sizeof(ufbxi_ScalingPivot) - 1 },
	{ ufbxi_RotationPivot, sizeof(ufbxi_RotationPivot) - 1 },
	{ ufbxi_ScalingOffset, sizeof(ufbxi_ScalingOffset) - 1 },
	{ ufbxi_RotationOffset, sizeof(ufbxi_RotationOffset) - 1 },
	{ ufbxi_RotationOrder, sizeof(ufbxi_RotationOrder) - 1 },
	{ ufbxi_InheritType, sizeof(ufbxi_InheritType) - 1 },
	{ ufbxi_DiffuseColor, sizeof(ufbxi_DiffuseColor) - 1 },
	{ ufbxi_SpecularColor, sizeof(ufbxi_SpecularColor) - 1 },
	{ ufbxi_Color, sizeof(ufbxi_Color) - 1 },
	{ ufbxi_Intensity, sizeof(ufbxi_Intensity) - 1 },
	{ ufbxi_Size, sizeof(ufbxi_Size) - 1 },
	{ ufbxi_RotationAccumulationMode, sizeof(ufbxi_RotationAccumulationMode) - 1 },
	{ ufbxi_ScaleAccumulationMode, sizeof(ufbxi_ScaleAccumulationMode) - 1 },
	{ ufbxi_Weight, sizeof(ufbxi_Weight) - 1 },
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

	char *memory_block;
	size_t memory_block_size;
} ufbxi_scene_imp;

typedef enum {
	UFBXI_CONNECTABLE_UNKNOWN,
	UFBXI_CONNECTABLE_MODEL,
	UFBXI_CONNECTABLE_MESH,
	UFBXI_CONNECTABLE_LIGHT,
	UFBXI_CONNECTABLE_BONE,
	UFBXI_CONNECTABLE_GEOMETRY,
	UFBXI_CONNECTABLE_MATERIAL,
	UFBXI_CONNECTABLE_ANIM_STACK,
	UFBXI_CONNECTABLE_ANIM_LAYER,
	UFBXI_CONNECTABLE_ANIM_PROP,
	UFBXI_CONNECTABLE_ANIM_CURVE,
	UFBXI_CONNECTABLE_SKIN_CLUSTER,
	UFBXI_CONNECTABLE_SKIN_DEFORMER,
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
	size_t num_skins;
	uint32_t *skin_index;
} ufbxi_skin_deformer;

typedef struct {
	const char *type;
	ufbx_string sub_type;
	ufbx_props props;
} ufbxi_template;

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
	size_t max_token_length;

	const char *src;
	const char *src_end;

	bool read_first_comment;
	bool found_version;
	bool parse_as_f32;

	ufbxi_ascii_token prev_token;
	ufbxi_ascii_token token;
} ufbxi_ascii;

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

	// Generated index buffers
	size_t max_zero_indices;
	size_t max_consecutive_indices;

	// Temporary data
	ufbx_mesh *geometries;
	size_t num_geometries;
	ufbxi_template *templates;
	size_t num_templates;

	// Temporary buffers
	ufbxi_buf tmp;
	ufbxi_buf tmp_parse;
	ufbxi_buf tmp_stack;
	ufbxi_buf tmp_connection;
	ufbxi_buf tmp_sort;

	// Temporary arrays
	ufbxi_buf tmp_arr_models;
	ufbxi_buf tmp_arr_meshes;
	ufbxi_buf tmp_arr_geometry;
	ufbxi_buf tmp_arr_materials;
	ufbxi_buf tmp_arr_lights;
	ufbxi_buf tmp_arr_bones;
	ufbxi_buf tmp_arr_anim_stacks;
	ufbxi_buf tmp_arr_anim_layers;
	ufbxi_buf tmp_arr_anim_props;
	ufbxi_buf tmp_arr_anim_curves;
	ufbxi_buf tmp_arr_attributes;
	ufbxi_buf tmp_arr_skin_clusters;
	ufbxi_buf tmp_arr_skin_deformers;

	// Result buffers
	ufbxi_buf result;
	ufbxi_buf string_buf;

	// Top-level state
	ufbxi_node *top_nodes;
	size_t top_nodes_len, top_nodes_cap;

	// "Focused" top-level node and child index, if `top_child_index == SIZE_MAX`
	// the children are parsed on demand.
	ufbxi_node *top_node;
	size_t top_child_index;
	ufbxi_node top_child;
	bool has_next_child;

	ufbxi_ascii ascii;

	ufbxi_node root;

	ufbxi_attribute *attributes;
	ufbx_skin *skin_clusters;
	ufbxi_skin_deformer *skin_deformers;

	ufbx_props *default_props;

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

static ufbxi_forceinline bool ufbxi_streq(ufbx_string a, ufbx_string b)
{
	return a.length == b.length && !memcmp(a.data, b.data, a.length);
}

const char ufbxi_empty_char[1] = { '\0' };

ufbxi_nodiscard static const char *ufbxi_push_string_imp(ufbxi_context *uc, const char *str, size_t length, bool copy)
{
	if (length == 0) return ufbxi_empty_char;
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

	void *data_to_free = NULL;
	size_t size_to_free = 0;

	// Grow the read buffer if necessary
	if (size > uc->read_buffer_size) {
		size_t new_size = ufbxi_max_sz(size, uc->opts.read_buffer_size);
		new_size = ufbxi_max_sz(new_size, uc->read_buffer_size * 2);
		size_to_free = uc->read_buffer_size;
		data_to_free = uc->read_buffer;
		char *new_buffer = ufbxi_alloc(&uc->ator_tmp, char, new_size);
		ufbxi_check_return(new_buffer, NULL);
		uc->read_buffer = new_buffer;
		uc->read_buffer_size = new_size;
	}

	// Copy the remains of the previous buffer to the beginning of the new one
	size_t num_read = uc->data_size;
	memmove(uc->read_buffer, uc->data, num_read);

	if (size_to_free) {
		ufbxi_free(&uc->ator_tmp, char, data_to_free, size_to_free);
	}

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

static ufbxi_forceinline bool ufbxi_check_string(ufbx_string s)
{
	return memchr(s.data, 0, s.length) == NULL;
}

// Retrieve values from nodes with type codes:
// Any: '_' (ignore)
// NUMBER: 'I' int32_t 'L' int64_t 'F' float 'D' double 'R' ufbxi_real 'B' bool 'Z' size_t
// STRING: 'S' ufbx_string 'C' const char* (checked) 's' ufbx_string 'c' const char * (unchecked)

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
	case 'S': if (type == UFBXI_VALUE_STRING && ufbxi_check_string(node->vals[ix].s)) { *(ufbx_string*)v = node->vals[ix].s; return 1; } else return 0;
	case 'C': if (type == UFBXI_VALUE_STRING && ufbxi_check_string(node->vals[ix].s)) { *(const char**)v = node->vals[ix].s.data; return 1; } else return 0;
	case 's': if (type == UFBXI_VALUE_STRING) { *(ufbx_string*)v = node->vals[ix].s; return 1; } else return 0;
	case 'c': if (type == UFBXI_VALUE_STRING) { *(const char**)v = node->vals[ix].s.data; return 1; } else return 0;
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
	UFBXI_PARSE_FBX_HEADER_EXTENSION,
	UFBXI_PARSE_DEFINITIONS,
	UFBXI_PARSE_OBJECTS,
	UFBXI_PARSE_TAKES,
	UFBXI_PARSE_FBX_VERSION,
	UFBXI_PARSE_MODEL,
	UFBXI_PARSE_GEOMETRY,
	UFBXI_PARSE_ANIMATION_CURVE,
	UFBXI_PARSE_DEFORMER,
	UFBXI_PARSE_LAYER_ELEMENT_NORMAL,
	UFBXI_PARSE_LAYER_ELEMENT_BINORMAL,
	UFBXI_PARSE_LAYER_ELEMENT_TANGENT,
	UFBXI_PARSE_LAYER_ELEMENT_UV,
	UFBXI_PARSE_LAYER_ELEMENT_COLOR,
	UFBXI_PARSE_LAYER_ELEMENT_VERTEX_CREASE,
	UFBXI_PARSE_LAYER_ELEMENT_EDGE_CREASE,
	UFBXI_PARSE_LAYER_ELEMENT_SMOOTHING,
	UFBXI_PARSE_LAYER_ELEMENT_MATERIAL,
	UFBXI_PARSE_TAKE,
	UFBXI_PARSE_TAKE_OBJECT,
	UFBXI_PARSE_CHANNEL,
	UFBXI_PARSE_UNKNOWN,
} ufbxi_parse_state;

typedef struct {
	char type;      // < FBX type code of the array: b,i,l,f,d (or 'r' meaning ufbx_real)
	bool result;    // < Alloacte the array from the result buffer
	bool pad_begin; // < Pad the begin of the array with 4 zero elements to guard from invalid -1 index accesses
} ufbxi_array_info;

static ufbxi_parse_state ufbxi_update_parse_state(ufbxi_parse_state parent, const char *name)
{
	switch (parent) {

	case UFBXI_PARSE_ROOT:
		if (name == ufbxi_FBXHeaderExtension) return UFBXI_PARSE_FBX_HEADER_EXTENSION;
		if (name == ufbxi_Definitions) return UFBXI_PARSE_DEFINITIONS;
		if (name == ufbxi_Objects) return UFBXI_PARSE_OBJECTS;
		if (name == ufbxi_Takes) return UFBXI_PARSE_TAKES;
		break;

	case UFBXI_PARSE_FBX_HEADER_EXTENSION:
		if (name == ufbxi_FBXVersion) return UFBXI_PARSE_FBX_VERSION;
		break;

	case UFBXI_PARSE_OBJECTS:
		if (name == ufbxi_Model) return UFBXI_PARSE_MODEL;
		if (name == ufbxi_Geometry) return UFBXI_PARSE_GEOMETRY;
		if (name == ufbxi_AnimationCurve) return UFBXI_PARSE_ANIMATION_CURVE;
		if (name == ufbxi_Deformer) return UFBXI_PARSE_DEFORMER;
		break;

	case UFBXI_PARSE_MODEL:
	case UFBXI_PARSE_GEOMETRY:
		if (name == ufbxi_LayerElementNormal) return UFBXI_PARSE_LAYER_ELEMENT_NORMAL;
		if (name == ufbxi_LayerElementBinormal) return UFBXI_PARSE_LAYER_ELEMENT_BINORMAL;
		if (name == ufbxi_LayerElementTangent) return UFBXI_PARSE_LAYER_ELEMENT_TANGENT;
		if (name == ufbxi_LayerElementUV) return UFBXI_PARSE_LAYER_ELEMENT_UV;
		if (name == ufbxi_LayerElementColor) return UFBXI_PARSE_LAYER_ELEMENT_COLOR;
		if (name == ufbxi_LayerElementVertexCrease) return UFBXI_PARSE_LAYER_ELEMENT_VERTEX_CREASE;
		if (name == ufbxi_LayerElementEdgeCrease) return UFBXI_PARSE_LAYER_ELEMENT_EDGE_CREASE;
		if (name == ufbxi_LayerElementSmoothing) return UFBXI_PARSE_LAYER_ELEMENT_SMOOTHING;
		if (name == ufbxi_LayerElementMaterial) return UFBXI_PARSE_LAYER_ELEMENT_MATERIAL;
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
	info->result = false;
	info->pad_begin = false;
	switch (parent) {

	case UFBXI_PARSE_GEOMETRY:
	case UFBXI_PARSE_MODEL:
		if (name == ufbxi_Vertices && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_PolygonVertexIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_Edges && !uc->opts.ignore_geometry) {
			info->type = 'i';
			return true;
		}
		break;

	case UFBXI_PARSE_ANIMATION_CURVE:
		if (name == ufbxi_KeyTime && !uc->opts.ignore_animation) {
			info->type = 'l';
			return true;
		} else if (name == ufbxi_KeyValueFloat && !uc->opts.ignore_animation) {
			info->type = 'r';
			return true;
		} else if (name == ufbxi_KeyAttrFlags && !uc->opts.ignore_animation) {
			info->type = 'i';
			return true;
		} else if (name == ufbxi_KeyAttrDataFloat && !uc->opts.ignore_animation) {
			// The float data in a keyframe attribute array is represented as integers
			// in versions >= 7200 as some of the elements aren't actually floats (!)
			info->type = uc->from_ascii && uc->version >= 7200 ? 'i' : 'f';
			return true;
		} else if (name == ufbxi_KeyAttrRefCount && !uc->opts.ignore_animation) {
			info->type = 'i';
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_NORMAL:
		if (name == ufbxi_Normals && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_NormalIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_BINORMAL:
		if (name == ufbxi_Binormals && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_BinormalIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_TANGENT:
		if (name == ufbxi_Tangents && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_TangentIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_UV:
		if (name == ufbxi_UV && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_UVIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_COLOR:
		if (name == ufbxi_Colors && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_ColorIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_VERTEX_CREASE:
		if (name == ufbxi_VertexCrease && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_VertexCreaseIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_EDGE_CREASE:
		if (name == ufbxi_EdgeCrease && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_SMOOTHING:
		if (name == ufbxi_Smoothing && !uc->opts.ignore_geometry) {
			info->type = 'b';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_MATERIAL:
		if (name == ufbxi_Materials && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_DEFORMER:
		if (name == ufbxi_Transform) {
			info->type = 'r';
			info->result = false;
			return true;
		} else if (name == ufbxi_TransformLink) {
			info->type = 'r';
			info->result = false;
			return true;
		} else if (name == ufbxi_Indexes && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_Weights && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_CHANNEL:
		if (name == ufbxi_Key && !uc->opts.ignore_animation) {
			info->type = 'd';
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
		case 'b': ufbxi_convert_loop(m_dst, 1, *val != 0); break; \
		case 'i': ufbxi_convert_loop(m_dst, 4, ufbxi_read_i32(val)); break; \
		case 'l': ufbxi_convert_loop(m_dst, 8, ufbxi_read_i64(val)); break; \
		case 'f': ufbxi_convert_loop(m_dst, 4, ufbxi_read_f32(val)); break; \
		case 'd': ufbxi_convert_loop(m_dst, 8, ufbxi_read_f64(val)); break; \
		default: ufbxi_fail("Bad array source type"); \
		} \
		break; \

	case 'b':
		switch (src_type) {
		case 'b': ufbxi_convert_loop(char, 1, *val != 0); break;
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

ufbxi_nodiscard static void *ufbxi_push_array_data(ufbxi_context *uc, const ufbxi_array_info *info, size_t size, ufbxi_buf *tmp_buf)
{
	ufbxi_check_return(size <= uc->opts.max_array_size, NULL);

	char type = ufbxi_normalize_array_type(info->type);
	size_t elem_size = ufbxi_array_type_size(type);
	if (info->pad_begin) size += 4;

	// The array may be pushed either to the result or temporary buffer depending
	// if it's already in the right format
	ufbxi_buf *arr_buf = info->result ? &uc->result : tmp_buf;
	char *data = (char*)ufbxi_push_size(arr_buf, elem_size, size);
	ufbxi_check_return(data, NULL);

	if (info->pad_begin) {
		memset(data, 0, elem_size * 4);
		data += elem_size * 4;
	}

	return data;
}

ufbxi_nodiscard static int ufbxi_binary_parse_node(ufbxi_context *uc, uint32_t depth, ufbxi_parse_state parent_state, bool *p_end, ufbxi_buf *tmp_buf, bool recursive)
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

	// If `end_offset` and `name_len` is zero we treat as the node as a NULL-sentinel
	// that terminates a node list.
	if (end_offset == 0 && name_len == 0) {
		*p_end = true;
		return 1;
	}

	// Push the parsed node into the `tmp_stack` buffer, the nodes will be popped by
	// calling code after its done parsing all of it's children.
	ufbxi_node *node = ufbxi_push_zero(&uc->tmp_stack, ufbxi_node, 1);
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

		ufbxi_value_array *arr = ufbxi_push(tmp_buf, ufbxi_value_array, 1);
		ufbxi_check(arr);

		node->value_type_mask = UFBXI_VALUE_ARRAY;
		node->array = arr;
		arr->type = dst_type;

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
			char *arr_data = (char*)ufbxi_push_array_data(uc, &arr_info, size, tmp_buf);
			ufbxi_check(arr_data);

			// If the source and destination types are equal and our build is binary-compatible
			// with the FBX format we can read the decoded data directly into the array buffer.
			// Otherwise we need a temporary buffer to decode the array into before conversion.
			// TODO: Streaming array conversion?
			void *decoded_data = arr_data;
			if (src_type != dst_type || uc->big_endian) {
				if (uc->convert_buffer_size < decoded_data_size) {
					ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->convert_buffer, &uc->convert_buffer_size, decoded_data_size));
				}
				decoded_data = uc->convert_buffer;
			}

			if (encoding == 0) {
				// Encoding 0: Plain binary data.
				ufbxi_check(encoded_size == decoded_data_size);

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
				ufbxi_check(res == (ptrdiff_t)decoded_data_size);

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
				ufbxi_for(char, b, (char*)arr_data, size) {
					*b = (char)(b != 0);
				}
			}

			arr->data = arr_data;
			arr->size = size;

		} else {
			// Allocate `num_values` elements for the array and parse single values into it.
			ufbxi_check(num_values <= uc->opts.max_array_size);
			char *arr_data = (char*)ufbxi_push_array_data(uc, &arr_info, num_values, tmp_buf);
			ufbxi_check(arr_data);
			ufbxi_check(ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_data, num_values));
			arr->data = arr_data;
			arr->size = num_values;
		}

	} else {
		// Parse up to UFBXI_MAX_NON_ARRAY_VALUES as plain values
		num_values = ufbxi_min32(num_values, UFBXI_MAX_NON_ARRAY_VALUES);
		ufbxi_value *vals = ufbxi_push(tmp_buf, ufbxi_value, num_values);
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

	if (recursive) {
		ufbxi_buf_state stack_state = ufbxi_buf_push_state(&uc->tmp_stack);

		// Recursively parse the children of this node. Update the parse state
		// to provide context for child node parsing.
		ufbxi_parse_state parse_state = ufbxi_update_parse_state(parent_state, node->name);
		uint32_t num_children = 0;
		for (;;) {
			ufbxi_check(num_children < uc->opts.max_node_children);

			// Stop at end offset
			uint64_t current_offset = ufbxi_get_read_offset(uc);
			if (current_offset >= end_offset) {
				ufbxi_check(current_offset == end_offset || end_offset == 0);
				break;
			}

			bool end = false;
			ufbxi_check(ufbxi_binary_parse_node(uc, depth + 1, parse_state, &end, tmp_buf, true));
			if (end) break;
			num_children++;
		}

		// Pop children from `tmp_stack` to a contiguous array
		node->num_children = num_children;
		node->children = ufbxi_push_pop(tmp_buf, &uc->tmp_stack, ufbxi_node, num_children);
		ufbxi_check(node->children);

		ufbxi_buf_pop_state(&uc->tmp_stack, &stack_state);
	} else {
		uint64_t current_offset = ufbxi_get_read_offset(uc);
		uc->has_next_child = (current_offset < end_offset);
	}

	return 1;
}

#define UFBXI_BINARY_MAGIC_SIZE 23
#define UFBXI_BINARY_HEADER_SIZE 27
static const char ufbxi_binary_magic[] = "Kaydara FBX Binary  \x00\x1a\x00";

// -- ASCII parsing

#define UFBXI_ASCII_END '\0'
#define UFBXI_ASCII_NAME 'N'
#define UFBXI_ASCII_BARE_WORD 'B'
#define UFBXI_ASCII_INT 'I'
#define UFBXI_ASCII_FLOAT 'F'
#define UFBXI_ASCII_STRING 'S'

static ufbxi_noinline char ufbxi_ascii_refill(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;
	if (uc->read_fn) {
		// Grow the read buffer if necessary
		if (uc->read_buffer_size < uc->opts.read_buffer_size) {
			size_t new_size = uc->opts.read_buffer_size;
			ufbxi_check_return(ufbxi_grow_array(&uc->ator_tmp, &uc->read_buffer, &uc->read_buffer_size, new_size), '\0');
		}

		// Read user data, return '\0' on EOF
		size_t num_read = uc->read_fn(uc->read_user, uc->read_buffer, uc->read_buffer_size);
		ufbxi_check_return(num_read <= uc->read_buffer_size, '\0');
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

static ufbxi_forceinline char ufbxi_ascii_peek(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;
	if (ua->src == ua->src_end) return ufbxi_ascii_refill(uc);
	return *ua->src;
}

static ufbxi_forceinline char ufbxi_ascii_next(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;
	if (ua->src == ua->src_end) return ufbxi_ascii_refill(uc);
	ua->src++;
	if (ua->src == ua->src_end) return ufbxi_ascii_refill(uc);
	return *ua->src;
}

static uint32_t ufbxi_ascii_parse_version(ufbxi_context *uc)
{
	uint8_t digits[3];
	uint32_t num_digits = 0;

	char c = ufbxi_ascii_next(uc);

	const char fmt[] = " FBX ?.?.?";
	uint32_t ix = 0;
	while (num_digits < 3) {
		char ref = fmt[ix++];
		switch (ref) {

		// Digit
		case '?':
			if (c < '0' || c > '9') return 0;
			digits[num_digits++] = (uint8_t)(c - '0');
			c = ufbxi_ascii_next(uc);
			break;

		// Whitespace 
		case ' ':
			while (c == ' ' || c == '\t') {
				c = ufbxi_ascii_next(uc);
			}
			break;

		// Literal character
		default:
			if (c != ref) return 0;
			c = ufbxi_ascii_next(uc);
			break;
		}
	}

	return 1000*digits[0] + 100*digits[1] + 10*digits[2];
}

static char ufbxi_ascii_skip_whitespace(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;

	// Ignore whitespace
	char c = ufbxi_ascii_peek(uc);
	for (;;) {
		while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			c = ufbxi_ascii_next(uc);
		}

		// Line comment
		if (c == ';') {

			// FBX ASCII files begin with a magic comment of form "; FBX 7.7.0 project file"
			// Try to extract the version number from the magic comment
			if (!ua->read_first_comment) {
				ua->read_first_comment = true;
				uint32_t version = ufbxi_ascii_parse_version(uc);
				if (version) {
					uc->version = version;
					ua->found_version = true;
				}
			}

			c = ufbxi_ascii_next(uc);
			while (c != '\n' && c != '\0') {
				c = ufbxi_ascii_next(uc);
			}
		} else {
			break;
		}
	}
	return c;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_ascii_push_token_char(ufbxi_context *uc, ufbxi_ascii_token *token, char c)
{
	// Grow the string data buffer if necessary
	if (token->str_len == token->str_cap) {
		size_t len = ufbxi_max_sz(token->str_len + 1, 256);
		ufbxi_check(len <= uc->opts.max_ascii_token_length);
		ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &token->str_data, &token->str_cap, len));
	}

	token->str_data[token->str_len++] = c;

	return 1;
}

ufbxi_nodiscard static int ufbxi_ascii_next_token(ufbxi_context *uc, ufbxi_ascii_token *token)
{
	ufbxi_ascii *ua = &uc->ascii;

	// Replace `prev_token` with `token` but swap the buffers so `token` uses
	// the now-unused string buffer of the old `prev_token`.
	char *swap_data = ua->prev_token.str_data;
	size_t swap_cap = ua->prev_token.str_cap;
	ua->prev_token = ua->token;
	ua->token.str_data = swap_data;
	ua->token.str_cap = swap_cap;

	char c = ufbxi_ascii_skip_whitespace(uc);
	token->str_len = 0;

	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
		token->type = UFBXI_ASCII_BARE_WORD;
		while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_') {
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, c));
			c = ufbxi_ascii_next(uc);
		}

		// Skip whitespace to find if there's a following ':'
		c = ufbxi_ascii_skip_whitespace(uc);
		if (c == ':') {
			token->value.name_len = token->str_len;
			token->type = UFBXI_ASCII_NAME;
			ufbxi_ascii_next(uc);
		}
	} else if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
		token->type = UFBXI_ASCII_INT;

		while ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
			if (c == '.' || c == 'e' || c == 'E') {
				token->type = UFBXI_ASCII_FLOAT;
			}
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, c));
			c = ufbxi_ascii_next(uc);
		}
		ufbxi_check(ufbxi_ascii_push_token_char(uc, token, '\0'));

		char *end;
		if (token->type == UFBXI_ASCII_INT) {
			token->value.i64 = strtoll(token->str_data, &end, 10);
			ufbxi_check(end == token->str_data + token->str_len - 1);
		} else if (token->type == UFBXI_ASCII_FLOAT) {
			if (ua->parse_as_f32) {
				token->value.f64 = strtof(token->str_data, &end);
			} else {
				token->value.f64 = strtod(token->str_data, &end);
			}
			ufbxi_check(end == token->str_data + token->str_len - 1);
		}
	} else if (c == '"') {
		token->type = UFBXI_ASCII_STRING;
		c = ufbxi_ascii_next(uc);
		while (c != '"') {
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, c));
			c = ufbxi_ascii_next(uc);
			ufbxi_check(c != '\0');
		}
		// Skip closing quote
		ufbxi_ascii_next(uc);
	} else {
		// Single character token
		token->type = c;
		ufbxi_ascii_next(uc);
	}

	return 1;
}


ufbxi_nodiscard static int ufbxi_ascii_accept(ufbxi_context *uc, char type)
{
	ufbxi_ascii *ua = &uc->ascii;

	if (ua->token.type == type) {
		ufbxi_check(ufbxi_ascii_next_token(uc, &ua->token));
		return 1;
	} else {
		return 0;
	}
}

ufbxi_nodiscard static int ufbxi_ascii_parse_node(ufbxi_context *uc, uint32_t depth, ufbxi_parse_state parent_state, bool *p_end, ufbxi_buf *tmp_buf, bool recursive)
{
	ufbxi_ascii *ua = &uc->ascii;

	if (ua->token.type == '}') {
		ufbxi_check(ufbxi_ascii_next_token(uc, &ua->token));
		*p_end = true;
		return 1;
	}

	if (ua->token.type == UFBXI_ASCII_END) {
		ufbxi_check(depth == 0);
		*p_end = true;
		return 1;
	}

	// Parse the name eg. "Node:" token and intern the name
	ufbxi_check(depth < uc->opts.max_node_depth);
	ufbxi_check(ufbxi_ascii_accept(uc, UFBXI_ASCII_NAME));
	size_t name_len = ua->prev_token.value.name_len;
	ufbxi_check(name_len <= 0xff);
	const char *name = ufbxi_push_string(uc, ua->prev_token.str_data, ua->prev_token.str_len);
	ufbxi_check(name);

	// Some fields in ASCII may have leading commas eg. `Content: , "base64-string"`
	if (ua->token.type == ',') {
		ufbxi_check(ufbxi_ascii_next_token(uc, &ua->token));
	}

	// Push the parsed node into the `tmp_stack` buffer, the nodes will be popped by
	// calling code after its done parsing all of it's children.
	ufbxi_node *node = ufbxi_push_zero(&uc->tmp_stack, ufbxi_node, 1);
	ufbxi_check(node);
	node->name = name;
	node->name_len = (uint8_t)name_len;

	bool in_ascii_array = false;

	uint32_t num_values = 0;
	uint32_t type_mask = 0;

	int arr_type = 0;
	ufbxi_buf *arr_buf = NULL;
	size_t arr_elem_size = 0;

	// Check if the values of the node we're parsing currently should be
	// treated as an array.
	ufbxi_array_info arr_info;
	if (ufbxi_is_array_node(uc, parent_state, name, &arr_info)) {
		arr_type = ufbxi_normalize_array_type(arr_info.type);
		arr_buf = arr_info.result ? &uc->result : tmp_buf;

		ufbxi_value_array *arr = ufbxi_push(tmp_buf, ufbxi_value_array, 1);
		ufbxi_check(arr);
		node->value_type_mask = UFBXI_VALUE_ARRAY;
		node->array = arr;
		arr->type = (char)arr_type;

		// HACK: Parse array values using strtof() if the array destination is 32-bit float
		// since KeyAttrDataFloat packs integer data (!) into floating point values so we
		// should try to be as exact as possible.
		if (arr->type == 'f') {
			ua->parse_as_f32 = true;
		}

		arr_elem_size = ufbxi_array_type_size((char)arr_type);

		// Pad with 4 zero elements to make indexing with `-1` safe.
		if (arr_info.pad_begin) {
			ufbxi_push_size_zero(arr_buf, arr_elem_size, 4);
			num_values += 4;
		}
	}


	ufbxi_parse_state parse_state = ufbxi_update_parse_state(parent_state, node->name);
	ufbxi_value vals[UFBXI_MAX_NON_ARRAY_VALUES];

	// NOTE: Infinite loop to allow skipping the comma parsing via `continue`.
	for (;;) {
		ufbxi_check(num_values <= (arr_type ? uc->opts.max_array_size : uc->opts.max_node_values));

		ufbxi_ascii_token *tok = &ua->prev_token;
		if (ufbxi_ascii_accept(uc, UFBXI_ASCII_STRING)) {

			if (arr_type) {
				// Ignore strings in arrays, decrement `num_values` as it will be incremented
				// after the loop iteration is done to ignore it.
				num_values--;
			} else if (num_values < UFBXI_MAX_NON_ARRAY_VALUES) {
				type_mask |= UFBXI_VALUE_STRING << (num_values*2);
				ufbxi_value *v = &vals[num_values];
				v->s.data = tok->str_data;
				v->s.length = tok->str_len;
				ufbxi_check(ufbxi_push_string_place_str(uc, &v->s));
			}

		} else if (ufbxi_ascii_accept(uc, UFBXI_ASCII_INT)) {
			int64_t val = tok->value.i64;

			switch (arr_type) {

			case 0:
				// Parse version from comment if there was no magic comment
				if (!ua->found_version && parse_state == UFBXI_PARSE_FBX_VERSION && num_values == 0) {
					if (val >= 6000 && val <= 10000) {
						ua->found_version = true;
						uc->version = (uint32_t)val;
					}
				}

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

		} else if (ufbxi_ascii_accept(uc, UFBXI_ASCII_FLOAT)) {
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

		} else if (ufbxi_ascii_accept(uc, UFBXI_ASCII_BARE_WORD)) {

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

		} else if (ufbxi_ascii_accept(uc, '*')) {
			// Parse a post-7000 ASCII array eg. "*3 { 1,2,3 }"
			ufbxi_check(!in_ascii_array);
			ufbxi_check(ufbxi_ascii_accept(uc, UFBXI_ASCII_INT));

			if (ufbxi_ascii_accept(uc, '{')) {
				ufbxi_check(ufbxi_ascii_accept(uc, UFBXI_ASCII_NAME));

				// NOTE: This `continue` skips incrementing `num_values` and parsing
				// a comma, continuing to parse the values in the array.
				in_ascii_array = true;
			}
			continue;
		} else {
			break;
		}

		// Add value and keep parsing if there's a comma. This part may be
		// skipped if we enter an array block.
		num_values++;
		if (!ufbxi_ascii_accept(uc, ',')) break;
	}

	// Close the ASCII array if we are in one
	if (in_ascii_array) {
		ufbxi_check(ufbxi_ascii_accept(uc, '}'));
	}

	ua->parse_as_f32 = false;

	if (arr_type) {
		void *arr_data = ufbxi_make_array_size(arr_buf, arr_elem_size, num_values);
		ufbxi_check(arr_data);
		if (arr_info.pad_begin) {
			node->array->data = (char*)arr_data + 4*arr_elem_size;
			node->array->size = num_values - 4;
		} else {
			node->array->data = arr_data;
			node->array->size = num_values;
		}
	} else {
		num_values = ufbxi_min32(num_values, UFBXI_MAX_NON_ARRAY_VALUES);
		node->value_type_mask = (uint16_t)type_mask;
		node->vals = ufbxi_push_copy(tmp_buf, ufbxi_value, num_values, vals);
		ufbxi_check(node->vals);
	}

	// Recursively parse the children of this node. Update the parse state
	// to provide context for child node parsing.
	if (ufbxi_ascii_accept(uc, '{')) {
		if (recursive) {
			ufbxi_buf_state stack_state = ufbxi_buf_push_state(&uc->tmp_stack);

			size_t num_children = 0;
			for (;;) {
				ufbxi_check(num_children < uc->opts.max_node_children);

				bool end = false;
				ufbxi_check(ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end, tmp_buf, recursive));
				if (end) break;
				num_children++;
			}

			// Pop children from `tmp_stack` to a contiguous array
			node->children = ufbxi_push_pop(tmp_buf, &uc->tmp_stack, ufbxi_node, num_children);
			ufbxi_check(node->children);
			node->num_children = (uint32_t)num_children;

			ufbxi_buf_pop_state(&uc->tmp_stack, &stack_state);
		}

		uc->has_next_child = true;
	} else {
		uc->has_next_child = false;
	}

	return 1;
}

// -- General parsing

ufbxi_nodiscard static int ufbxi_begin_parse(ufbxi_context *uc)
{
	const char *header = ufbxi_peek_bytes(uc, UFBXI_BINARY_HEADER_SIZE);
	ufbxi_check(header);

	// If the file starts with the binary magic parse it as binary, otherwise
	// treat it as an ASCII file.
	if (!memcmp(header, ufbxi_binary_magic, UFBXI_BINARY_MAGIC_SIZE)) {

		// Read the version directly from the header
		uc->version = ufbxi_read_u32(header + UFBXI_BINARY_MAGIC_SIZE);
		ufbxi_consume_bytes(uc, UFBXI_BINARY_HEADER_SIZE);

	} else {
		uc->from_ascii = true;

		// Use the current read buffer as the initial parse buffer
		memset(&uc->ascii, 0, sizeof(uc->ascii));
		uc->ascii.src = uc->data;
		uc->ascii.src_end = uc->data + uc->data_size;

		// Default to version 7400 if not found in header
		uc->version = 7400;

		// Initialize the first token
		ufbxi_check(ufbxi_ascii_next_token(uc, &uc->ascii.token));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_parse_toplevel_child_imp(ufbxi_context *uc, ufbxi_parse_state state, ufbxi_buf *buf, bool *p_end)
{
	if (uc->from_ascii) {
		ufbxi_check(ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, true));
	} else {
		ufbxi_check(ufbxi_binary_parse_node(uc, 0, state, p_end, buf, true));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_parse_toplevel(ufbxi_context *uc, const char *name)
{
	// Check if the top-level node has already been parsed
	ufbxi_for(ufbxi_node, node, uc->top_nodes, uc->top_nodes_len) {
		if (node->name == name) {
			uc->top_node = node;
			uc->top_child_index = 0;
			return 1;
		}
	}

	for (;;) {
		// Parse the next top-level node
		bool end = false;
		if (uc->from_ascii) {
			ufbxi_check(ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &uc->tmp, false));
		} else {
			ufbxi_check(ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &uc->tmp, false));
		}

		// Top-level node not found
		if (end) {
			uc->top_node = NULL;
			uc->top_child_index = 0;
			return 1;
		}

		uc->top_nodes_len++;
		ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->top_nodes, &uc->top_nodes_cap, uc->top_nodes_len));
		ufbxi_node *node = &uc->top_nodes[uc->top_nodes_len - 1];
		ufbxi_pop(&uc->tmp_stack, ufbxi_node, 1, node);

		// Return if we parsed the right one
		if (node->name == name) {
			uc->top_node = node;
			uc->top_child_index = SIZE_MAX;
			return 1;
		}

		// If not we need to parse all the children of the node for later
		uint32_t num_children = 0;
		ufbxi_parse_state state = ufbxi_update_parse_state(UFBXI_PARSE_ROOT, node->name);
		if (uc->has_next_child) {
			for (;;) {
				ufbxi_check(num_children < uc->opts.max_node_children);
				ufbxi_check(ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &end));
				if (end) break;
				num_children++;
			}
		}

		node->num_children = num_children;
		node->children = ufbxi_push_pop(&uc->tmp, &uc->tmp_stack, ufbxi_node, num_children);
		ufbxi_check(node->children);
	}
}

ufbxi_nodiscard static int ufbxi_parse_toplevel_child(ufbxi_context *uc, ufbxi_node **p_node)
{
	// Top-level node not found
	if (!uc->top_node) {
		*p_node = NULL;
		return 1;
	}

	if (uc->top_child_index == SIZE_MAX) {
		// Parse children on demand
		ufbxi_buf_clear(&uc->tmp_parse);
		bool end = false;
		ufbxi_parse_state state = ufbxi_update_parse_state(UFBXI_PARSE_ROOT, uc->top_node->name);
		ufbxi_check(ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_parse, &end));
		if (end) {
			*p_node = NULL;
		} else {
			ufbxi_pop(&uc->tmp_stack, ufbxi_node, 1, &uc->top_child);
			*p_node = &uc->top_child;
		}
	} else {
		// Iterate already parsed nodes
		if (uc->top_child_index == uc->top_node->num_children) {
			*p_node = NULL;
		} else {
			*p_node = &uc->top_node->children[uc->top_child_index++];
		}
	}

	return 1;
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
	{ UFBX_PROP_BOOLEAN, "Bool" },
	{ UFBX_PROP_INTEGER, "Integer" },
	{ UFBX_PROP_INTEGER, "int" },
	{ UFBX_PROP_INTEGER, "enum" },
	{ UFBX_PROP_INTEGER, "Visibility" },
	{ UFBX_PROP_INTEGER, "Visibility Inheritance" },
	{ UFBX_PROP_INTEGER, "KTime" },
	{ UFBX_PROP_NUMBER, "Number" },
	{ UFBX_PROP_NUMBER, "double" },
	{ UFBX_PROP_NUMBER, "Real" },
	{ UFBX_PROP_NUMBER, "Float" },
	{ UFBX_PROP_NUMBER, "Intensity" },
	{ UFBX_PROP_VECTOR, "Vector" },
	{ UFBX_PROP_VECTOR, "Vector3D" },
	{ UFBX_PROP_COLOR, "Color" },
	{ UFBX_PROP_COLOR, "ColorRGB" },
	{ UFBX_PROP_STRING, "String" },
	{ UFBX_PROP_STRING, "KString" },
	{ UFBX_PROP_STRING, "object" },
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

typedef struct {
	const char *name;
	uint32_t name_len;
	ufbx_prop_type type;
	const char *value_str;
	ufbx_real value_real[3];
} ufbxi_default_prop;

// Generated by `misc/gen_default_props.py`
static const ufbxi_default_prop ufbxi_default_props[] = {
	{ "AmbientColor", 12, UFBX_PROP_COLOR, 0, 0.2, 0.2, 0.2 },
	{ "AmbientFactor", 13, UFBX_PROP_NUMBER, 0, 1.0 },
	{ "AreaLightShape", 14, UFBX_PROP_INTEGER, 0, 0.0 },
	{ "AxisLen", 7, UFBX_PROP_NUMBER, 0, 10.0 },
	{ "BBoxMax", 7, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "BBoxMin", 7, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "BottomBarnDoor", 14, UFBX_PROP_NUMBER, 0, 20.0 },
	{ "Bump", 4, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "BumpFactor", 10, UFBX_PROP_NUMBER, 0, 1.0 },
	{ "CastLightOnObject", 17, UFBX_PROP_BOOLEAN, 0, 1.0 },
	{ "CastShadows", 11, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "Casts Shadows", 13, UFBX_PROP_BOOLEAN, 0, 1.0 },
	{ "Color", 5, UFBX_PROP_COLOR, 0, 0.8, 0.8, 0.8 },
	{ "DecayStart", 10, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "DecayType", 9, UFBX_PROP_INTEGER, 0, 0.0 },
	{ "DefaultAttributeIndex", 21, UFBX_PROP_INTEGER, 0, -1.0 },
	{ "DiffuseColor", 12, UFBX_PROP_COLOR, 0, 0.8, 0.8, 0.8 },
	{ "DiffuseFactor", 13, UFBX_PROP_NUMBER, 0, 1.0 },
	{ "DisplacementColor", 17, UFBX_PROP_COLOR, 0, 0.0, 0.0, 0.0 },
	{ "DisplacementFactor", 18, UFBX_PROP_NUMBER, 0, 1.0 },
	{ "DrawFrontFacingVolumetricLight", 30, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "DrawGroundProjection", 20, UFBX_PROP_BOOLEAN, 0, 1.0 },
	{ "DrawVolumetricLight", 19, UFBX_PROP_BOOLEAN, 0, 1.0 },
	{ "EmissiveColor", 13, UFBX_PROP_COLOR, 0, 0.0, 0.0, 0.0 },
	{ "EmissiveFactor", 14, UFBX_PROP_NUMBER, 0, 1.0 },
	{ "EnableBarnDoor", 14, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "EnableFarAttenuation", 20, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "EnableNearAttenuation", 21, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "FarAttenuationEnd", 17, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "FarAttenuationStart", 19, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "FileName", 8, UFBX_PROP_STRING, "" },
	{ "Fog", 3, UFBX_PROP_NUMBER, 0, 50.0 },
	{ "Freeze", 6, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "GeometricRotation", 17, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "GeometricScaling", 16, UFBX_PROP_VECTOR, 0, 1.0, 1.0, 1.0 },
	{ "GeometricTranslation", 20, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "InheritType", 11, UFBX_PROP_INTEGER, 0, 0.0 },
	{ "InnerAngle", 10, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "Intensity", 9, UFBX_PROP_NUMBER, 0, 100.0 },
	{ "LODBox", 6, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "Lcl Rotation", 12, UFBX_PROP_ROTATION, 0, 0.0, 0.0, 0.0 },
	{ "Lcl Scaling", 11, UFBX_PROP_SCALING, 0, 1.0, 1.0, 1.0 },
	{ "Lcl Translation", 15, UFBX_PROP_TRANSLATION, 0, 0.0, 0.0, 0.0 },
	{ "LeftBarnDoor", 12, UFBX_PROP_NUMBER, 0, 20.0 },
	{ "LookAtProperty", 14, UFBX_PROP_STRING, 0 },
	{ "MaxDampRangeX", 13, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MaxDampRangeY", 13, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MaxDampRangeZ", 13, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MaxDampStrengthX", 16, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MaxDampStrengthY", 16, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MaxDampStrengthZ", 16, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MinDampRangeX", 13, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MinDampRangeY", 13, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MinDampRangeZ", 13, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MinDampStrengthX", 16, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MinDampStrengthY", 16, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MinDampStrengthZ", 16, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "MultiLayer", 10, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "NearAttenuationEnd", 18, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "NearAttenuationStart", 20, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "NegativePercentShapeSupport", 27, UFBX_PROP_BOOLEAN, 0, 1.0 },
	{ "NormalMap", 9, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "OuterAngle", 10, UFBX_PROP_NUMBER, 0, 45.0 },
	{ "PostRotation", 12, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "PreRotation", 11, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "PreferedAngleX", 14, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "PreferedAngleY", 14, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "PreferedAngleZ", 14, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "Primary Visibility", 18, UFBX_PROP_BOOLEAN, 0, 1.0 },
	{ "QuaternionInterpolate", 21, UFBX_PROP_INTEGER, 0, 0.0 },
	{ "Receive Shadows", 15, UFBX_PROP_BOOLEAN, 0, 1.0 },
	{ "RightBarnDoor", 13, UFBX_PROP_NUMBER, 0, 20.0 },
	{ "RotationActive", 14, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "RotationMax", 11, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "RotationMaxX", 12, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "RotationMaxY", 12, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "RotationMaxZ", 12, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "RotationMin", 11, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "RotationMinX", 12, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "RotationMinY", 12, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "RotationMinZ", 12, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "RotationOffset", 14, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "RotationOrder", 13, UFBX_PROP_INTEGER, 0, 0.0 },
	{ "RotationPivot", 13, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "RotationSpaceForLimitOnly", 25, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "RotationStiffnessX", 18, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "RotationStiffnessY", 18, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "RotationStiffnessZ", 18, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "ScalingActive", 13, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "ScalingMax", 10, UFBX_PROP_VECTOR, 0, 1.0, 1.0, 1.0 },
	{ "ScalingMaxX", 11, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "ScalingMaxY", 11, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "ScalingMaxZ", 11, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "ScalingMin", 10, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "ScalingMinX", 11, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "ScalingMinY", 11, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "ScalingMinZ", 11, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "ScalingOffset", 13, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "ScalingPivot", 12, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "ShadingModel", 12, UFBX_PROP_STRING, "Lambert" },
	{ "ShadowColor", 11, UFBX_PROP_COLOR, 0, 0.0, 0.0, 0.0 },
	{ "Show", 4, UFBX_PROP_BOOLEAN, 0, 1.0 },
	{ "TopBarnDoor", 11, UFBX_PROP_NUMBER, 0, 20.0 },
	{ "TranslationActive", 17, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "TranslationMax", 14, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "TranslationMaxX", 15, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "TranslationMaxY", 15, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "TranslationMaxZ", 15, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "TranslationMin", 14, UFBX_PROP_VECTOR, 0, 0.0, 0.0, 0.0 },
	{ "TranslationMinX", 15, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "TranslationMinY", 15, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "TranslationMinZ", 15, UFBX_PROP_BOOLEAN, 0, 0.0 },
	{ "TransparencyFactor", 18, UFBX_PROP_NUMBER, 0, 0.0 },
	{ "TransparentColor", 16, UFBX_PROP_COLOR, 0, 0.0, 0.0, 0.0 },
	{ "UpVectorProperty", 16, UFBX_PROP_STRING, 0 },
	{ "VectorDisplacementColor", 23, UFBX_PROP_COLOR, 0, 0.0, 0.0, 0.0 },
	{ "VectorDisplacementFactor", 24, UFBX_PROP_NUMBER, 0, 1.0 },
	{ "Visibility", 10, UFBX_PROP_INTEGER, 0, 1.0 },
	{ "Visibility Inheritance", 22, UFBX_PROP_INTEGER, 0, 1.0 },
};

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

ufbxi_nodiscard static int ufbxi_load_default_props(ufbxi_context *uc)
{
	size_t num = ufbxi_arraycount(ufbxi_default_props);
	ufbx_props *defs = ufbxi_push(&uc->result, ufbx_props, 1);
	ufbxi_check(defs);
	uc->default_props = defs;

	defs->num_props = num;
	defs->props = ufbxi_push(&uc->result, ufbx_prop, num);
	defs->defaults = NULL;
	ufbxi_check(defs->props);

	for (size_t i = 0; i < num; i++) {
		const ufbxi_default_prop *def = &ufbxi_default_props[i];
		ufbx_prop *prop = &defs->props[i];

		prop->name.data = ufbxi_push_string_imp(uc, def->name, def->name_len, false);
		prop->name.length = def->name_len;
		ufbxi_check(prop->name.data);
		prop->imp_key = ufbxi_get_name_key(def->name, def->name_len);
		prop->type = def->type;
		if (def->value_str) {
			size_t value_len = strlen(def->value_str);
			prop->value_str.data = ufbxi_push_string_imp(uc, def->value_str, value_len, false);
			prop->value_str.length = value_len;
			ufbxi_check(prop->value_str.data);
		} else {
			prop->value_str = ufbx_empty_string;
		}
		prop->value_int = (int64_t)def->value_real[0];
		prop->value_real_arr[0] = def->value_real[0];
		prop->value_real_arr[1] = def->value_real[1];
		prop->value_real_arr[2] = def->value_real[2];
	}

	return 1;
}

// -- Reading the parsed data

typedef struct {
	ufbx_string name;
	ufbx_string sub_type;
	uint64_t id;
	ufbx_props props;
} ufbxi_object;

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

ufbxi_nodiscard static int ufbxi_read_header_extension(ufbxi_context *uc)
{
	// TODO: Read TCDefinition and adjust timestamps
	uc->ktime_to_sec = (1.0 / 46186158000.0);

	for (;;) {
		ufbxi_node *child;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &child));
		if (!child) break;

		if (child->name == ufbxi_Creator) {
			ufbxi_ignore(ufbxi_get_val1(child, "S", &uc->scene.metadata.creator));
		}

	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_document(ufbxi_context *uc)
{
	uint64_t root_id = 0;
	bool found_root_id = 0;

	for (;;) {
		ufbxi_node *child;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &child));
		if (!child) break;

		if (child->name == ufbxi_Document && !found_root_id) {
			// Post-7000: Try to find the first document node and root ID.
			// TODO: Multiple documents / roots?
			if (ufbxi_find_val1(child, ufbxi_RootNode, "L", &root_id)) {
				found_root_id = true;
			}
		}
	}

	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_MODEL, root_id, 0));

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

	prop->imp_key = ufbxi_get_name_key(prop->name.data, prop->name.length);
	prop->type = ufbxi_get_prop_type(uc, type_str);
	if (prop->type == UFBX_PROP_UNKNOWN && subtype_str) {
		prop->type = ufbxi_get_prop_type(uc, subtype_str);
	}

	if (!ufbxi_get_val_at(node, val_ix, 'S', &prop->value_str)) {
		prop->value_str = ufbx_empty_string;
	}

	ufbxi_ignore(ufbxi_get_val_at(node, val_ix, 'L', &prop->value_int));
	for (size_t i = 0; i < 3; i++) {
		if (!ufbxi_get_val_at(node, val_ix + i, 'R', &prop->value_real_arr[i])) break;
	}
	
	return 1;
}

static ufbxi_forceinline int ufbxi_cmp_prop(const void *va, const void *vb)
{
	const ufbx_prop *a = (const ufbx_prop*)va, *b = (const ufbx_prop*)vb;
	if (a->imp_key < b->imp_key) return -1;
	if (a->imp_key > b->imp_key) return +1;
	return strcmp(a->name.data, b->name.data);
}

ufbxi_nodiscard static int ufbxi_merge_properties(ufbxi_context *uc, ufbx_props *dst, const ufbx_props *p_a, const ufbx_props *p_b, ufbxi_buf *buf)
{
	ufbx_props a, b;
	if (p_a) a = *p_a; else a.num_props = 0;
	if (p_b) b = *p_b; else b.num_props = 0;

	size_t max_dst = a.num_props + b.num_props;
	dst->props = ufbxi_push(buf, ufbx_prop, max_dst);
	dst->num_props = 0;
	ufbxi_check(dst->props);

	// Merge properties preferring values from `b`
	size_t ai = 0, bi = 0, di = 0;
	while (ai < a.num_props && bi < b.num_props) {
		int cmp = ufbxi_cmp_prop(&a.props[ai], &b.props[bi]);
		if (cmp >= 0) {
			dst->props[di++] = b.props[bi++];
			if (cmp == 0) ai++;
		} else {
			dst->props[di++] = a.props[ai++];
		}
	}

	// Add rest of the properties from both
	while (ai < a.num_props) dst->props[di++] = a.props[ai++];
	while (bi < b.num_props) dst->props[di++] = b.props[bi++];

	// Pop unused values from the end of `result`
	ufbxi_pop(buf, ufbx_prop, max_dst - di, NULL);
	dst->num_props = di;

	return 1;
}

ufbxi_nodiscard static int ufbxi_sort_properties(ufbxi_context *uc, ufbx_props *dst, ufbxi_buf *buf)
{
	size_t num = dst->num_props;
	if (num > 32) {
		// Merge sort
		size_t mid = num / 2;
		ufbx_props left, right;
		left.props = dst->props;
		left.num_props = mid;
		right.props = dst->props + mid;
		right.num_props = num - mid;
		ufbxi_check(ufbxi_sort_properties(uc, &left, &uc->tmp_sort));
		ufbxi_check(ufbxi_sort_properties(uc, &right, &uc->tmp_sort));
		ufbxi_check(ufbxi_merge_properties(uc, dst, &left, &right, buf));
	} else {
		// Insertion sort

		// Top-level properties are in temporary storage
		if (buf != &uc->tmp_sort) {
			dst->props = ufbxi_push_copy(buf, ufbx_prop, num, dst->props);
			ufbxi_check(dst->props);
		}

		ufbx_prop *props = dst->props;
		size_t num_removed = 0;
		for (size_t hi = 1; hi < num; hi++) {
			size_t lo = hi - num_removed;
			int cmp = -1;
			ufbx_prop prop = props[hi];
			for (; lo > 0; lo--) {
				cmp = ufbxi_cmp_prop(&props[lo - 1], &prop);
				if (cmp <= 0) break;
				props[lo] = props[lo - 1];
			}

			if (cmp == 0) {
				for (size_t i = lo + 1; i < hi - num_removed; i++) {
					props[i] = props[i + 1];
				}
				props[lo - 1] = prop;
				num_removed++;
			} else {
				props[lo] = prop;
			}
		}
		dst->num_props -= num_removed;
	}

	return 1;
}

static ufbxi_forceinline bool ufbxi_prop_value_equal(const ufbx_prop *a, const ufbx_prop *b)
{
	// Strings are interned so we can do shallow compare of the pointers,
	// this also includes the check for `value_str.length`!
	if (a->value_str.data != b->value_str.data) return false;
	if (a->value_int != b->value_int) return false;
	if (a->value_real_arr[0] != b->value_real_arr[0]) return false;
	if (a->value_real_arr[1] != b->value_real_arr[1]) return false;
	if (a->value_real_arr[2] != b->value_real_arr[2]) return false;
	return true;
}

static void ufbxi_remove_default_properties(ufbx_props *dst, ufbx_props *defaults, ufbxi_buf *buf)
{
	size_t num_dst = dst->num_props, num_def = defaults->num_props;
	size_t dst_i = 0, src_i = 0, def_i = 0;
	ufbx_prop *dst_props = dst->props;
	ufbx_prop *def_props = defaults->props;

	// Iterate through `dst` and `defaults` in lock-step, removing properties
	// from `dst` if they are equal to the default values.
	while (src_i < num_dst && def_i < num_def) {
		int cmp = ufbxi_cmp_prop(&dst_props[src_i], &def_props[def_i]);
		if (cmp == 0) {
			if (!ufbxi_prop_value_equal(&dst_props[src_i], &def_props[def_i])) {
				if (dst_i != src_i) dst_props[dst_i] = dst_props[src_i];
				dst_i++;
			}
			src_i++;
		} else if (cmp < 0) {
			if (dst_i != src_i) dst_props[dst_i] = dst_props[src_i];
			src_i++;
			dst_i++;
		} else {
			def_i++;
		}
	}

	// Shift down the remaining values if necessary
	while (src_i < num_dst) {
		if (dst_i != src_i) dst_props[dst_i] = dst_props[src_i];
		src_i++;
		dst_i++;
	}

	// Pop removed default values
	ufbxi_pop(buf, ufbx_prop, dst->num_props - dst_i, NULL);

	dst->num_props = dst_i;
	dst->defaults = defaults;
}

ufbxi_nodiscard static int ufbxi_read_properties(ufbxi_context *uc, ufbxi_node *parent, ufbx_props *props, ufbxi_buf *buf)
{
	props->defaults = NULL;

	int version = 70;
	ufbxi_node *node = ufbxi_find_child(parent, ufbxi_Properties70);
	if (!node) {
		node = ufbxi_find_child(parent, ufbxi_Properties60);
		if (!node) {
			// No properties found, not an error
			props->props = NULL;
			props->num_props = 0;
			return 1;
		}
		version = 60;
	}

	// Parse properties directly to `result` buffer and linearize them using `ufbxi_make_array()`
	ufbxi_check(node->num_children < uc->opts.max_properties);
	ufbxi_for(ufbxi_node, prop_node, node->children, node->num_children) {
		ufbx_prop *prop = ufbxi_push_zero(&uc->tmp_sort, ufbx_prop, 1);
		ufbxi_check(prop);
		ufbxi_check(ufbxi_read_property(uc, prop_node, prop, version));
	}

	props->props = ufbxi_make_array(&uc->tmp_sort, ufbx_prop, node->num_children);
	props->num_props = node->num_children;
	ufbxi_check(props->props);

	// Sort and deduplicate the properties
	ufbxi_check(ufbxi_sort_properties(uc, props, buf));
	ufbxi_buf_clear(&uc->tmp_sort);

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_definitions(ufbxi_context *uc)
{
	ufbxi_buf_state stack_state = ufbxi_buf_push_state(&uc->tmp_stack);

	for (;;) {
		ufbxi_node *object;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &object));
		if (!object) break;

		if (object->name != ufbxi_ObjectType) continue;

		// Pre-7000 FBX versions don't have property templates, they just have
		// the object counts by themselves.
		ufbxi_node *props = ufbxi_find_child(object, ufbxi_PropertyTemplate);
		if (props) {
			ufbxi_template *tmpl = ufbxi_push_zero(&uc->tmp_stack, ufbxi_template, 1);
			ufbxi_check(tmpl);
			ufbxi_check(ufbxi_get_val1(props, "S", &tmpl->sub_type));

			// Ignore potential "Fbx" prefix.
			if (tmpl->sub_type.length > 3 && !strncmp(tmpl->sub_type.data, "Fbx", 3)) {
				tmpl->sub_type.data += 3;
				tmpl->sub_type.length -= 3;
			}

			ufbxi_check(ufbxi_get_val1(object, "C", (char**)&tmpl->type));
			ufbxi_check(ufbxi_read_properties(uc, props, &tmpl->props, &uc->tmp));
			ufbxi_remove_default_properties(&tmpl->props, uc->default_props, &uc->tmp);
		}
	}

	// Copy the templates to the destination buffer
	uc->num_templates = uc->tmp_stack.num_items - stack_state.num_items;
	uc->templates = ufbxi_push_pop(&uc->tmp, &uc->tmp_stack, ufbxi_template, uc->num_templates);
	ufbxi_check(uc->templates);

	ufbxi_buf_pop_state(&uc->tmp_stack, &stack_state);

	return 1;
}

ufbxi_nodiscard static ufbx_props *ufbxi_find_template(ufbxi_context *uc, const char *name, ufbx_string sub_type)
{
	ufbxi_for(ufbxi_template, tmpl, uc->templates, uc->num_templates) {
		if (tmpl->type == name) {

			// Check that sub_type matches unless the type is Material, Model, AnimationStack, AnimationLayer
			// those match to all sub-types.
			if (tmpl->type != ufbxi_Material && tmpl->type != ufbxi_Model
				&& tmpl->type != ufbxi_AnimationStack && tmpl->type != ufbxi_AnimationLayer) {
				if (sub_type.length > 3 && !strncmp(sub_type.data, "Fbx", 3)) {
					sub_type.data += 3;
					sub_type.length -= 3;
				}

				if (!ufbxi_streq(sub_type, tmpl->sub_type)) {
					return NULL;
				}
			}

			if (tmpl->props.num_props > 0) {
				return &tmpl->props;
			} else {
				return NULL;
			}
		}
	}
	return NULL;
}

ufbxi_nodiscard static int ufbxi_check_indices(ufbxi_context *uc, ufbx_mesh *mesh, int32_t **p_dst, int32_t *indices, bool owns_indices, size_t num_indices, size_t num_elems)
{
	int32_t invalid_index = -1;
	if (!uc->opts.allow_nonexistent_indices) {
		invalid_index = num_elems > 0 ? (int32_t)num_elems - 1 : 0;
	}

	// If the indices are truncated extend them with `invalid_index`
	if (num_indices < mesh->num_indices) {
		int32_t *new_indices = ufbxi_push(&uc->result, int32_t, mesh->num_indices);
		ufbxi_check(new_indices);

		memcpy(new_indices, indices, sizeof(int32_t) * num_indices);
		for (size_t i = num_indices; i < mesh->num_indices; i++) {
			new_indices[i] = invalid_index;
		}

		indices = new_indices;
		num_indices = mesh->num_indices;
		owns_indices = true;
	}

	// Normalize out-of-bounds indices to `invalid_index`
	for (size_t i = 0; i < num_indices; i++) {
		int32_t ix = indices[i];
		if (ix < 0 || (uint32_t)ix >= num_elems) {
			// If the indices refer to an external buffer we need to
			// allocate a separate buffer for them
			if (!owns_indices) {
				int32_t *new_indices = ufbxi_push(&uc->result, int32_t, num_indices);
				ufbxi_check(new_indices);
				memcpy(new_indices, indices, sizeof(int32_t) * num_indices);
				indices = new_indices;
				owns_indices = true;
			}

			indices[i] = invalid_index;
		}
	}

	*p_dst = indices;

	return 1;
}

static ufbx_real ufbxi_zero_element[8] = { 0 };

// Sentinel pointers used for zero/sequential index buffers
static const int32_t ufbxi_sentinel_index_zero[1] = { 100000000 };
static const int32_t ufbxi_sentinel_index_consecutive[1] = { 123456789 };

ufbxi_nodiscard static int ufbxi_read_vertex_element(ufbxi_context *uc, ufbx_mesh *mesh, ufbxi_node *node,
	void *p_dst_data_void, int32_t **p_dst_index, size_t *p_num_elems, const char *data_name, const char *index_name, char data_type, size_t num_components)
{
	ufbx_real **p_dst_data = (ufbx_real**)p_dst_data_void;

	ufbxi_value_array *data = ufbxi_find_array(node, data_name, data_type);
	ufbxi_value_array *indices = ufbxi_find_array(node, index_name, 'i');
	ufbxi_check(data);
	ufbxi_check(data->size % num_components == 0);

	size_t num_elems = data->size / num_components;

	size_t mesh_num_indices = mesh->num_indices;

	const char *mapping;
	ufbxi_check(ufbxi_find_val1(node, ufbxi_MappingInformationType, "C", (char**)&mapping));

	if (num_elems > mesh->num_indices) {
		num_elems = mesh->num_indices;
	}
	*p_num_elems = num_elems ? num_elems : 1;

	// Data array is always used as-is, if empty set the data to a global
	// zero buffer so invalid zero index can point to some valid data.
	// The zero data is offset by 4 elements to accomodate for invalid index (-1)
	if (num_elems > 0) {
		*p_dst_data = (ufbx_real*)data->data;
	} else {
		*p_dst_data = ufbxi_zero_element + 4;
	}

	size_t elem_size = ufbxi_array_type_size(data_type);

	if (indices) {
		size_t num_indices = indices->size;
		int32_t *index_data = (int32_t*)indices->data;

		if (mapping == ufbxi_ByPolygonVertex) {

			// Indexed by polygon vertex: We can use the provided indices directly.
			ufbxi_check(ufbxi_check_indices(uc, mesh, p_dst_index, index_data, true, num_indices, num_elems));

		} else if (mapping == ufbxi_ByVertex || mapping == ufbxi_ByVertice) {

			// Indexed by vertex: Follow through the position index mapping to get the
			// final indices.
			int32_t *new_index_data = ufbxi_push(&uc->result, int32_t, mesh->num_indices);
			ufbxi_check(new_index_data);

			int32_t *vert_ix = mesh->vertex_position.indices;
			for (size_t i = 0; i < mesh_num_indices; i++) {
				int32_t ix = index_data[i];
				if (ix >= 0 && (uint32_t)ix < mesh->num_vertices) {
					new_index_data[i] = vert_ix[ix];
				} else {
					new_index_data[i] = -1;
				}
			}

			ufbxi_check(ufbxi_check_indices(uc, mesh, p_dst_index, new_index_data, true, num_indices, num_elems));

		} else if (mapping == ufbxi_AllSame) {

			// Indexed by all same: ??? This could be possibly used for making
			// holes with invalid indices, but that seems really fringe.
			// Just use the shared zero index buffer for this.
			uc->max_zero_indices = ufbxi_max_sz(uc->max_zero_indices, mesh->num_indices);
			*p_dst_index = (int32_t*)ufbxi_sentinel_index_zero;

		} else {
			ufbxi_fail("Invalid mapping");
		}

	} else {

		if (mapping == ufbxi_ByPolygonVertex) {

			// Direct by polygon index: Use shared consecutive array if there's enough
			// elements, otherwise use a unique truncated consecutive index array.
			if (num_elems >= mesh->num_indices) {
				uc->max_consecutive_indices = ufbxi_max_sz(uc->max_consecutive_indices, mesh->num_indices);
				*p_dst_index = (int32_t*)ufbxi_sentinel_index_consecutive;
			} else {
				int32_t *index_data = ufbxi_push(&uc->result, int32_t, mesh->num_indices);
				for (size_t i = 0; i < mesh->num_indices; i++) {
					index_data[i] = (int32_t)i;
				}
				ufbxi_check(ufbxi_check_indices(uc, mesh, p_dst_index, index_data, true, mesh->num_indices, num_elems));
			}

		} else if (mapping == ufbxi_ByVertex || mapping == ufbxi_ByVertice) {

			// Direct by vertex: We can re-use the position indices.
			ufbxi_check(ufbxi_check_indices(uc, mesh, p_dst_index, mesh->vertex_position.indices, false, mesh->num_indices, num_elems));

		} else if (mapping == ufbxi_AllSame) {

			// Direct by all same: This cannot fail as the index list is just zero.
			uc->max_zero_indices = ufbxi_max_sz(uc->max_zero_indices, mesh->num_indices);
			*p_dst_index = (int32_t*)ufbxi_sentinel_index_zero;

		} else {
			ufbxi_fail("Invalid mapping");
		}
	}

	return 1;
}

static int ufbxi_cmp_uv_set(const void *va, const void *vb)
{
	const ufbx_uv_set *a = (const ufbx_uv_set*)va, *b = (const ufbx_uv_set*)vb;
	if (a->index < b->index) return -1;
	if (a->index > b->index) return +1;
	return 0;
}

static int ufbxi_cmp_color_set(const void *va, const void *vb)
{
	const ufbx_color_set *a = (const ufbx_color_set*)va, *b = (const ufbx_color_set*)vb;
	if (a->index < b->index) return -1;
	if (a->index > b->index) return +1;
	return 0;
}

ufbxi_nodiscard static int ufbxi_read_truncated_array(ufbxi_context *uc, void *p_data, ufbxi_node *node, const char *name, char fmt, size_t size)
{
	ufbxi_value_array *arr = ufbxi_find_array(node, name, fmt);
	ufbxi_check(arr);

	void *data = arr->data;
	if (arr->size < size) {
		size_t elem_size = ufbxi_array_type_size(fmt);
		void *new_data = ufbxi_push_size(&uc->result, elem_size, size);
		ufbxi_check(new_data);
		memcpy(new_data, data, arr->size * elem_size);
		memset((char*)new_data + arr->size * elem_size, 0, (size - arr->size) * elem_size);
		data = new_data;
	}

	*(void**)p_data = data;
	return 1;
}

typedef struct {
	ufbx_vertex_vec3 elem;
	int32_t index;
} ufbxi_tangent_layer;

ufbxi_nodiscard static int ufbxi_read_geometry(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	// Only read polygon meshes, ignore eg. NURBS without error
	ufbxi_node *node_vertices = ufbxi_find_child(node, ufbxi_Vertices);
	ufbxi_node *node_indices = ufbxi_find_child(node, ufbxi_PolygonVertexIndex);
	if (!node_vertices || !node_indices) return 1;

	ufbx_mesh *mesh = ufbxi_push_zero(&uc->tmp_arr_geometry, ufbx_mesh, 1);
	ufbxi_check(mesh);
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_GEOMETRY, object->id, uc->tmp_arr_geometry.num_items - 1));
	mesh->node.props = object->props;

	if (uc->opts.ignore_geometry) return 1;

	ufbxi_value_array *vertices = ufbxi_get_array(node_vertices, 'r');
	ufbxi_value_array *indices = ufbxi_get_array(node_indices, 'i');
	ufbxi_value_array *edge_indices = ufbxi_find_array(node, ufbxi_Edges, 'i');
	ufbxi_check(vertices && indices);
	ufbxi_check(vertices->size % 3 == 0);

	mesh->num_vertices = vertices->size / 3;
	mesh->num_indices = indices->size;

	int32_t *index_data = (int32_t*)indices->data;

	mesh->vertex_position.data = (ufbx_vec3*)vertices->data;
	mesh->vertex_position.indices = index_data;
	mesh->vertex_position.num_elements = mesh->num_vertices;

	// Check that the last index is negated (last of polygon)
	if (mesh->num_indices > 0) {
		ufbxi_check(index_data[mesh->num_indices - 1] < 0);
	}

	// Read edges before un-negating the indices
	if (edge_indices) {
		size_t num_edges = edge_indices->size;
		ufbx_edge *edges = ufbxi_push(&uc->result, ufbx_edge, num_edges);
		ufbxi_check(edges);

		// Edges are represented using a single index into PolygonVertexIndex.
		// The edge is between two consecutive vertices in the polygon.
		int32_t *edge_data = (int32_t*)edge_indices->data;
		for (size_t i = 0; i < num_edges; i++) {
			int32_t index_ix = edge_data[i];
			ufbxi_check(index_ix >= 0 && (size_t)index_ix < mesh->num_indices);
			edges[i].indices[0] = index_ix;
			if (index_data[index_ix] < 0) {
				// Previous index is the last one of this polygon, rewind to first index.
				while (index_ix > 0 && index_data[index_ix - 1] >= 0) {
					index_ix--;
				}
			} else {
				// Connect to the next index in the same polygon
				index_ix++;
			}
			ufbxi_check(index_ix >= 0 && (size_t)index_ix < mesh->num_indices);
			edges[i].indices[1] = index_ix;
		}

		mesh->edges = edges;
		mesh->num_edges = num_edges;
	}

	// Count the number of faces and allocate the index list
	// Indices less than zero (~actual_index) ends a polygon
	size_t num_total_faces = 0;
	ufbxi_for (int32_t, p_ix, index_data, mesh->num_indices) {
		if (*p_ix < 0) num_total_faces++;
	}
	mesh->faces = ufbxi_push(&uc->result, ufbx_face, num_total_faces);
	ufbxi_check(mesh->faces);

	size_t num_triangles = 0;

	ufbx_face *dst_face = mesh->faces;
	ufbx_face *dst_bad_face = mesh->faces + num_total_faces;
	int32_t *p_face_begin = index_data;
	ufbxi_for (int32_t, p_ix, index_data, mesh->num_indices) {
		int32_t ix = *p_ix;
		// Un-negate final indices of polygons
		if (ix < 0) {
			ix = ~ix;
			*p_ix =  ix;
			uint32_t num_indices = (uint32_t)((p_ix - p_face_begin) + 1);
			if (num_indices >= 3) {
				dst_face->index_begin = (uint32_t)(p_face_begin - index_data);
				dst_face->num_indices = num_indices;
				num_triangles += num_indices - 2;
				dst_face++;
			} else {
				dst_bad_face--;
				dst_bad_face->index_begin = (uint32_t)(p_face_begin - index_data);
				dst_bad_face->num_indices = num_indices;
			}
			p_face_begin = p_ix + 1;
		}
		ufbxi_check(ix < mesh->num_vertices);
	}
	ufbx_assert(dst_face == dst_bad_face);

	mesh->vertex_position.indices = index_data;
	mesh->num_faces = dst_face - mesh->faces;
	mesh->num_bad_faces = num_total_faces - mesh->num_faces;

	// Swap bad faces
	if (mesh->num_bad_faces > 0) {
		ufbx_face *lo = mesh->faces + mesh->num_faces;
		ufbx_face *hi = lo + mesh->num_bad_faces - 1;
		while (lo < hi) {
			ufbx_face tmp = *lo;
			*lo = *hi;
			*hi = tmp;
			lo++;
			hi--;
		}
	}

	mesh->num_triangles = num_triangles;

	// Count the number of UV/color sets
	size_t num_uv = 0, num_color = 0, num_binormals = 0, num_tangents = 0;
	ufbxi_for (ufbxi_node, n, node->children, node->num_children) {
		if (n->name == ufbxi_LayerElementUV) num_uv++;
		if (n->name == ufbxi_LayerElementColor) num_color++;
		if (n->name == ufbxi_LayerElementBinormal) num_binormals++;
		if (n->name == ufbxi_LayerElementTangent) num_tangents++;
	}

	ufbxi_buf_state stack_state = ufbxi_buf_push_state(&uc->tmp_stack);

	ufbxi_tangent_layer *binormals = ufbxi_push(&uc->tmp_stack, ufbxi_tangent_layer, num_binormals);
	ufbxi_tangent_layer *tangents = ufbxi_push(&uc->tmp_stack, ufbxi_tangent_layer, num_tangents);
	ufbxi_check(binormals);
	ufbxi_check(tangents);

	mesh->uv_sets.data = ufbxi_push_zero(&uc->result, ufbx_uv_set, num_uv);
	mesh->color_sets.data = ufbxi_push_zero(&uc->result, ufbx_color_set, num_color);
	ufbxi_check(mesh->uv_sets.data);
	ufbxi_check(mesh->color_sets.data);

	size_t num_binormals_read = 0, num_tangents_read = 0;
	ufbxi_for (ufbxi_node, n, node->children, node->num_children) {
		if (n->name[0] != 'L') continue; // All names start with 'LayerElement*'

		if (n->name == ufbxi_LayerElementNormal) {
			if (mesh->vertex_normal.data) continue;
			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_normal.data,
				&mesh->vertex_normal.indices, &mesh->vertex_normal.num_elements, ufbxi_Normals, ufbxi_NormalIndex, 'r', 3));
		} else if (n->name == ufbxi_LayerElementBinormal) {
			ufbxi_tangent_layer *layer = &binormals[num_binormals_read++];
			ufbxi_ignore(ufbxi_get_val1(n, "I", &layer->index));
			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.data,
				&layer->elem.indices, &layer->elem.num_elements, ufbxi_Binormals, ufbxi_BinormalIndex, 'r', 3));

		} else if (n->name == ufbxi_LayerElementTangent) {
			ufbxi_tangent_layer *layer = &tangents[num_tangents_read++];
			ufbxi_ignore(ufbxi_get_val1(n, "I", &layer->index));
			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.data,
				&layer->elem.indices, &layer->elem.num_elements, ufbxi_Tangents, ufbxi_TangentIndex, 'r', 3));
		} else if (n->name == ufbxi_LayerElementUV) {
			ufbx_uv_set *set = &mesh->uv_sets.data[mesh->uv_sets.size++];

			ufbxi_ignore(ufbxi_get_val1(n, "I", &set->index));
			if (!ufbxi_find_val1(n, ufbxi_Name, "S", &set->name)) {
				set->name = ufbx_empty_string;
			}

			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_uv.data,
				&set->vertex_uv.indices, &set->vertex_uv.num_elements, ufbxi_UV, ufbxi_UVIndex, 'r', 2));
		} else if (n->name == ufbxi_LayerElementColor) {
			ufbx_color_set *set = &mesh->color_sets.data[mesh->color_sets.size++];

			ufbxi_ignore(ufbxi_get_val1(n, "I", &set->index));
			if (!ufbxi_find_val1(n, ufbxi_Name, "S", &set->name)) {
				set->name = ufbx_empty_string;
			}

			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_color.data,
				&set->vertex_color.indices, &set->vertex_color.num_elements, ufbxi_Colors, ufbxi_ColorIndex, 'r', 4));
		} else if (n->name == ufbxi_LayerElementVertexCrease) {
			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_crease.data,
				&mesh->vertex_crease.indices, &mesh->vertex_crease.num_elements, ufbxi_VertexCrease, ufbxi_VertexCreaseIndex, 'r', 1));
		} else if (n->name == ufbxi_LayerElementEdgeCrease) {
			const char *mapping;
			ufbxi_check(ufbxi_find_val1(n, ufbxi_MappingInformationType, "C", (char**)&mapping));
			if (mapping == ufbxi_ByEdge) {
				if (mesh->edge_crease) continue;
				ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->edge_crease, n, ufbxi_EdgeCrease, 'r', mesh->num_edges));
			}
		} else if (n->name == ufbxi_LayerElementSmoothing) {
			const char *mapping;
			ufbxi_check(ufbxi_find_val1(n, ufbxi_MappingInformationType, "C", (char**)&mapping));
			if (mapping == ufbxi_ByEdge) {
				if (mesh->edge_smoothing) continue;
				ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->edge_smoothing, n, ufbxi_Smoothing, 'b', mesh->num_edges));
			} else if (mapping == ufbxi_ByPolygon) {
				if (mesh->face_smoothing) continue;
				ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->face_smoothing, n, ufbxi_Smoothing, 'b', mesh->num_faces));
			}
		} else if (n->name == ufbxi_LayerElementMaterial) {
			if (mesh->face_material) continue;
			const char *mapping;
			ufbxi_check(ufbxi_find_val1(n, ufbxi_MappingInformationType, "C", (char**)&mapping));
			if (mapping == ufbxi_ByPolygon) {
				ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->face_material, n, ufbxi_Materials, 'i', mesh->num_faces));
			} else if (mapping == ufbxi_AllSame) {
				ufbxi_value_array *arr = ufbxi_find_array(n, ufbxi_Materials, 'i');
				ufbxi_check(arr && arr->size >= 1);
				int32_t material = *(int32_t*)arr->data;
				if (material == 0) {
					uc->max_zero_indices = ufbxi_max_sz(uc->max_zero_indices, mesh->num_faces);
					mesh->face_material = (int32_t*)ufbxi_sentinel_index_zero;
				} else {
					mesh->face_material = ufbxi_push(&uc->result, int32_t, mesh->num_faces);
					ufbxi_check(mesh->face_material);
					ufbxi_for(int32_t, p_mat, mesh->face_material, mesh->num_faces) {
						*p_mat = material;
					}
				}
			}
		}
	}

	ufbx_assert(mesh->uv_sets.size == num_uv);
	ufbx_assert(mesh->color_sets.size == num_color);
	ufbx_assert(num_binormals_read == num_binormals);
	ufbx_assert(num_tangents_read == num_tangents);

	// Connect binormals/tangents to UV sets
	ufbxi_for (ufbxi_node, n, node->children, node->num_children) {
		if (n->name != ufbxi_Layer) continue;
		ufbx_uv_set *uv_set = NULL;
		ufbxi_tangent_layer *binormal_layer = NULL;
		ufbxi_tangent_layer *tangent_layer = NULL;

		ufbxi_for (ufbxi_node, c, n->children, n->num_children) {
			int32_t index;
			const char *type;
			if (c->name != ufbxi_LayerElement) continue;
			if (!ufbxi_find_val1(c, ufbxi_TypedIndex, "I", &index)) continue;
			if (!ufbxi_find_val1(c, ufbxi_Type, "C", (char**)&type)) continue;

			if (type == ufbxi_LayerElementUV) {
				ufbxi_for(ufbx_uv_set, set, mesh->uv_sets.data, mesh->uv_sets.size) {
					if (set->index == index) {
						uv_set = set;
						break;
					}
				}
			} else if (type == ufbxi_LayerElementBinormal) {
				ufbxi_for(ufbxi_tangent_layer, layer, binormals, num_binormals) {
					if (layer->index == index) {
						binormal_layer = layer;
						break;
					}
				}
			} else if (type == ufbxi_LayerElementTangent) {
				ufbxi_for(ufbxi_tangent_layer, layer, tangents, num_tangents) {
					if (layer->index == index) {
						tangent_layer = layer;
						break;
					}
				}
			}
		}

		if (uv_set) {
			if (binormal_layer) {
				uv_set->vertex_binormal = binormal_layer->elem;
			}
			if (tangent_layer) {
				uv_set->vertex_tangent = tangent_layer->elem;
			}
		}
	}

	// Sort UV and color sets by set index
	// TODO: Stable sort
	qsort(mesh->uv_sets.data, mesh->uv_sets.size, sizeof(ufbx_uv_set), &ufbxi_cmp_uv_set);
	qsort(mesh->color_sets.data, mesh->color_sets.size, sizeof(ufbx_color_set), &ufbxi_cmp_color_set);

	ufbxi_buf_pop_state(&uc->tmp_stack, &stack_state);

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_material(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	ufbx_material *material = ufbxi_push_zero(&uc->tmp_arr_materials, ufbx_material, 1);
	ufbxi_check(material);
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_MATERIAL, object->id, uc->tmp_arr_materials.num_items - 1));

	material->name = object->name;

	// Merge defaults immediately
	ufbxi_check(ufbxi_merge_properties(uc, &material->props, object->props.defaults, &object->props, &uc->result));
	ufbxi_remove_default_properties(&material->props, uc->default_props, &uc->result);

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_model(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	ufbx_node *scene_node = NULL;
	if (object->sub_type.data == ufbxi_Mesh) {
		ufbx_mesh *mesh = ufbxi_push_zero(&uc->tmp_arr_meshes, ufbx_mesh, 1);
		ufbxi_check(mesh);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_MESH, object->id, uc->tmp_arr_meshes.num_items - 1));
		mesh->node.type = UFBX_NODE_MESH;
		scene_node = &mesh->node;

		// Pre-7000 FBX stores mesh geometry in the same node as the mesh
		if (!uc->opts.ignore_geometry && ufbxi_find_array(node, ufbxi_Vertices, '?')) {
			ufbxi_object geom_obj = { 0 };
			geom_obj.id = (uintptr_t)mesh;
			ufbxi_check(ufbxi_read_geometry(uc, node, &geom_obj));

			// Add "virtual" connection between the mesh and the geometry
			ufbxi_check(ufbxi_add_connection(uc, object->id, geom_obj.id, NULL));
		}

	} else if (object->sub_type.data == ufbxi_Light) {
		ufbx_light *light = ufbxi_push_zero(&uc->tmp_arr_lights, ufbx_light, 1);
		ufbxi_check(light);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_LIGHT, object->id, uc->tmp_arr_lights.num_items - 1));
		light->node.type = UFBX_NODE_LIGHT;
		scene_node = &light->node;
	} else if (object->sub_type.data == ufbxi_LimbNode || object->sub_type.data == ufbxi_Limb) {
		ufbx_bone *bone = ufbxi_push_zero(&uc->tmp_arr_bones, ufbx_bone, 1);
		ufbxi_check(bone);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_BONE, object->id, uc->tmp_arr_bones.num_items - 1));
		bone->node.type = UFBX_NODE_BONE;
		scene_node = &bone->node;
	} else {
		ufbx_model *model = ufbxi_push_zero(&uc->tmp_arr_models, ufbx_model, 1);
		ufbxi_check(model);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_MODEL, object->id, uc->tmp_arr_models.num_items - 1));
		model->node.type = UFBX_NODE_MODEL;
		scene_node = &model->node;
	}

	if (scene_node) {
		scene_node->name = object->name;
		scene_node->props = object->props;
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_node_attribute(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	if (object->sub_type.data == ufbxi_Light || object->sub_type.data == ufbxi_LimbNode || object->sub_type.data == ufbxi_Limb) {
		ufbxi_attribute *attr = ufbxi_push_zero(&uc->tmp_arr_attributes, ufbxi_attribute, 1);
		ufbxi_check(attr);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ATTRIBUTE, object->id, uc->tmp_arr_attributes.num_items - 1));
		attr->props = object->props;
	}

	(void)node;

	return 1;
}

static void ufbxi_read_transform_matrix(ufbx_matrix *m, ufbx_real *data)
{
	m->m00 = data[ 0]; m->m10 = data[ 1]; m->m20 = data[ 2];
	m->m01 = data[ 4]; m->m11 = data[ 5]; m->m21 = data[ 6];
	m->m02 = data[ 8]; m->m12 = data[ 9]; m->m22 = data[10];
	m->m03 = data[12]; m->m13 = data[13]; m->m23 = data[14];
}

ufbxi_nodiscard static int ufbxi_read_deformer(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	if (object->sub_type.data == ufbxi_Cluster) {
		ufbxi_value_array *indices = ufbxi_find_array(node, ufbxi_Indexes, 'i');
		ufbxi_value_array *weights = ufbxi_find_array(node, ufbxi_Weights, 'r');
		ufbxi_value_array *transform = ufbxi_find_array(node, ufbxi_Transform, 'r');
		ufbxi_value_array *transform_link = ufbxi_find_array(node, ufbxi_TransformLink, 'r');

		// TODO: Transform and TransformLink may be missing (?) use BindPose node in that case
		if (transform && transform_link) {
			ufbx_skin *skin = ufbxi_push_zero(&uc->tmp_arr_skin_clusters, ufbx_skin, 1);
			ufbxi_check(skin);
			ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_SKIN_CLUSTER, object->id, uc->tmp_arr_skin_clusters.num_items - 1));

			ufbxi_check(transform->size >= 16);
			ufbxi_check(transform_link->size >= 16);

			if (indices && weights) {
				ufbxi_check(indices->size == weights->size);
				skin->num_weights = indices->size;
				skin->indices = (int32_t*)indices->data;
				skin->weights = (ufbx_real*)weights->data;
			}

			ufbxi_read_transform_matrix(&skin->mesh_to_bind, (ufbx_real*)transform->data);
			ufbxi_read_transform_matrix(&skin->bind_to_world, (ufbx_real*)transform_link->data);
		}
	} else if (object->sub_type.data == ufbxi_Skin) {
		ufbxi_skin_deformer *deformer = ufbxi_push_zero(&uc->tmp_arr_skin_deformers, ufbxi_skin_deformer, 1);
		ufbxi_check(deformer);
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_SKIN_DEFORMER, object->id, uc->tmp_arr_skin_deformers.num_items - 1));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_animation_stack(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	ufbx_anim_stack *stack = ufbxi_push_zero(&uc->tmp_arr_anim_stacks, ufbx_anim_stack, 1);
	ufbxi_check(stack);
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_STACK, object->id, uc->tmp_arr_anim_stacks.num_items - 1));

	stack->name = object->name;

	// Merge defaults immediately
	ufbxi_check(ufbxi_merge_properties(uc, &stack->props, object->props.defaults, &object->props, &uc->result));
	ufbxi_remove_default_properties(&stack->props, uc->default_props, &uc->result);

	(void)node;

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_animation_layer(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	ufbx_anim_layer *layer = ufbxi_push_zero(&uc->tmp_arr_anim_layers, ufbx_anim_layer, 1);
	ufbxi_check(layer);
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_LAYER, object->id, uc->tmp_arr_anim_layers.num_items - 1));

	layer->name = object->name;

	// Merge defaults immediately
	ufbxi_check(ufbxi_merge_properties(uc, &layer->layer_props, object->props.defaults, &object->props, &uc->result));
	ufbxi_remove_default_properties(&layer->layer_props, uc->default_props, &uc->result);

	(void)node;

	return 1;
}

static ufbxi_forceinline float ufbxi_solve_auto_tangent(double prev_time, double time, double next_time, ufbx_real prev_value, ufbx_real value, ufbx_real next_value, float weight_left, float weight_right)
{
	// In between two keyframes: Set the initial slope to be the difference between
	// the two keyframes. Prevent overshooting by clamping the slope in case either
	// tangent goes above/below the endpoints.
	double slope = (next_value - prev_value) / (next_time - prev_time);

	// Split the slope to sign and a non-negative absolute value
	double slope_sign = slope >= 0.0 ? 1.0 : -1.0;
	double abs_slope = slope_sign * slope;

	// Find limits for the absolute value of the sign
	double max_left = slope_sign * (value - prev_value) / (weight_left * (time - prev_time));
	double max_right = slope_sign * (next_value - value) / (weight_right * (next_time - time));

	// Clamp negative values and NaNs (in case weight*delta_time underflows) to zero 
	if (!(max_left > 0.0)) max_left = 0.0;
	if (!(max_right > 0.0)) max_right = 0.0;

	// Clamp the absolute slope from both sides
	if (abs_slope > max_left) abs_slope = max_left;
	if (abs_slope > max_right) abs_slope = max_right;

	return (float)(slope_sign * abs_slope);
}

ufbxi_nodiscard static int ufbxi_read_animation_curve(ufbxi_context *uc, ufbxi_node *node, ufbxi_object *object)
{
	if (uc->opts.ignore_animation) return 1;

	ufbx_anim_curve *curve = ufbxi_push_zero(&uc->tmp_arr_anim_curves, ufbx_anim_curve, 1);
	ufbxi_check(curve);
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_CURVE, object->id, uc->tmp_arr_anim_curves.num_items - 1));

	ufbxi_value_array *times, *values, *attr_flags, *attrs, *refs;
	ufbxi_check(times = ufbxi_find_array(node, ufbxi_KeyTime, 'l'));
	ufbxi_check(values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r'));
	ufbxi_check(attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags, 'i'));
	ufbxi_check(attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, '?'));
	ufbxi_check(refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i'));

	// Time and value arrays that define the keyframes should be parallel
	ufbxi_check(times->size == values->size);

	// Flags and attributes are run-length encoded where KeyAttrRefCount (refs)
	// is an array that describes how many times to repeat a given flag/attribute.
	// Attributes consist of 4 32-bit floating point values per key.
	ufbxi_check(attr_flags->size == refs->size);
	ufbxi_check(attrs->size == refs->size * 4u);

	size_t num_keys = times->size;
	ufbx_keyframe *keys = ufbxi_push(&uc->result, ufbx_keyframe, num_keys);
	ufbxi_check(keys);

	curve->keyframes.data = keys;
	curve->keyframes.size = num_keys;

	int64_t *p_time = (int64_t*)times->data;
	ufbx_real *p_value = (ufbx_real*)values->data;
	int32_t *p_flag = (int32_t*)attr_flags->data;
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

				if (i > 0 && i + 1 < num_keys && key->time > prev_time && next_time > key->time) {
					slope_left = slope_right = ufbxi_solve_auto_tangent(
						prev_time, key->time, next_time,
						p_value[-1], key->value, p_value[1],
						weight_left, weight_right);
				} else {
					// Endpoint / invalid keyframe: Set both slopes to zero
					slope_left = slope_right = 0.0f;
				}
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
		// between the previous/next key) and slope (simply d = slope * dx)

		if (key->time > prev_time) {
			double delta = key->time - prev_time;
			key->left.dx = (float)(weight_left * delta);
			key->left.dy = key->left.dx * slope_left;
		} else {
			key->left.dx = 0.0f;
			key->left.dy = 0.0f;
		}

		if (next_time > key->time) {
			double delta = next_time - key->time;
			key->right.dx = (float)(weight_right * delta);
			key->right.dy = key->right.dx * slope_right;
		} else {
			key->right.dx = 0.0f;
			key->right.dy = 0.0f;
		}

		slope_left = next_slope_left;
		weight_left = next_weight_left;
		prev_time = key->time;

		// Decrement attribute refcount and potentially move to the next one.
		int32_t refs_left = --*p_ref;
		ufbxi_check(refs_left >= 0);
		if (refs_left == 0) {
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

	prop->name = ufbx_empty_string;

	ufbxi_for(ufbx_prop, def, object->props.props, object->props.num_props) {
		if (def->type != UFBX_PROP_NUMBER) continue;

		size_t index = 0;
		if (def->name.data == ufbxi_Y || def->name.data == ufbxi_D_Y) index = 1;
		if (def->name.data == ufbxi_Z || def->name.data == ufbxi_D_Z) index = 2;
		prop->curves[index].default_value = def->value_real;
	}

	(void)node;

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
	ufbxi_check(ufbxi_check_string(*type));
	ufbxi_check(ufbxi_check_string(*name));

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_objects(ufbxi_context *uc)
{
	ufbxi_object object = { 0 };
	for (;;) {
		ufbxi_node *node;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &node));
		if (!node) break;

		ufbx_string type_and_name;

		// Failing to parse the object properties is not an error since
		// there's some weird objects mixed in every now and then.
		// FBX version 7000 and up uses 64-bit unique IDs per object,
		// older FBX versions just use name/type pairs, which we can
		// use as IDs since all strings are interned into a string pool.
		if (uc->version >= 7000) {
			if (!ufbxi_get_val3(node, "LsS", &object.id, &type_and_name, &object.sub_type)) continue;
		} else {
			if (!ufbxi_get_val2(node, "sS", &type_and_name, &object.sub_type)) continue;
			object.id = (uintptr_t)type_and_name.data;
		}

		ufbx_string type_str;
		ufbxi_check(ufbxi_split_type_and_name(uc, type_and_name, &type_str, &object.name));

		const char *name = node->name;

		if (uc->version >= 7000) {
			// Post-7000 FBX files have templates and potentially need to merge
			// attributes so read all the properties to `tmp` for now.
			ufbxi_check(ufbxi_read_properties(uc, node, &object.props, &uc->tmp));
			object.props.defaults = ufbxi_find_template(uc, name, object.sub_type);
		} else {
			// Pre-7000 FBX files don't need to further property merging so we can
			// read the properties to `result` and remove defaults immediately.
			ufbxi_check(ufbxi_read_properties(uc, node, &object.props, &uc->result));
			ufbxi_remove_default_properties(&object.props, uc->default_props, &uc->result);
		}

		if (name == ufbxi_Model) {
			ufbxi_check(ufbxi_read_model(uc, node, &object));
		} else if (name == ufbxi_Geometry) {
			ufbxi_check(ufbxi_read_geometry(uc, node, &object));
		} else if (name == ufbxi_Material) {
			ufbxi_check(ufbxi_read_material(uc, node, &object));
		} else if (name == ufbxi_NodeAttribute) {
			ufbxi_check(ufbxi_read_node_attribute(uc, node, &object));
		} else if (name == ufbxi_AnimationStack) {
			ufbxi_check(ufbxi_read_animation_stack(uc, node, &object));
		} else if (name == ufbxi_AnimationLayer) {
			ufbxi_check(ufbxi_read_animation_layer(uc, node, &object));
		} else if (name == ufbxi_AnimationCurve) {
			ufbxi_check(ufbxi_read_animation_curve(uc, node, &object));
		} else if (name == ufbxi_AnimationCurveNode) {
			ufbxi_check(ufbxi_read_animation_curve_node(uc, node, &object));
		} else if (name == ufbxi_Deformer) {
			ufbxi_check(ufbxi_read_deformer(uc, node, &object));
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_take_anim_channel(ufbxi_context *uc, ufbxi_node *node, uint64_t parent_id, const char *name, ufbx_real *p_default)
{
	if (uc->opts.ignore_animation) return 1;

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
		bool auto_slope = false;

		if (mode == 'U') {
			// Cubic interpolation
			key->interpolation = UFBX_INTERPOLATION_CUBIC;

			ufbxi_check(data_end - data >= 1);
			char slope_mode = (char)data[0];
			data += 1;

			if (slope_mode == 's' || slope_mode == 'b') {
				// Slope mode 's'/'b' (standard? broken?) always have two explicit slopes
				ufbxi_check(data_end - data >= 2);
				slope_right = (float)data[0];
				next_slope_left = (float)data[1];
				data += 2;
			} else if (slope_mode == 'a') {
				// Parameterless slope mode 'a' seems to appear in baked animations. Let's just assume
				// automatic tangents for now as they're the least likely to break with
				// objectionable artifacts. We need to defer the automatic tangent resolve
				// until we have read the next time/value.
				// TODO: Solve what this is more throroughly
				auto_slope = true;
			} else {
				ufbxi_fail("Unknown slope mode");
			}

			ufbxi_check(data_end - data >= 1);
			char weight_mode = (char)data[0];
			data += 1;

			if (weight_mode == 'n') {
				// Automatic weights (0.3333...)
			} else if (weight_mode == 'a') {
				// Manual weights: RightWeight, NextLeftWeight
				ufbxi_check(data_end - data >= 2);
				weight_right = (float)data[0];
				next_weight_left = (float)data[1];
				data += 2;
			} else {
				ufbxi_fail("Unknown weight mode");
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

		if (auto_slope) {
			if (i > 0) {
				slope_left = slope_right = ufbxi_solve_auto_tangent(
					prev_time, key->time, next_time,
					key[-1].value, key->value, next_value,
					weight_left, weight_right);
			} else {
				slope_left = slope_right = 0.0f;
			}
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
			double delta = key->time - prev_time;
			key->left.dx = (float)(weight_left * delta);
			key->left.dy = key->left.dx * slope_left;
		} else {
			key->left.dx = 0.0f;
			key->left.dy = 0.0f;
		}

		if (next_time > key->time) {
			double delta = next_time - key->time;
			key->right.dx = (float)(weight_right * delta);
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
			ufbxi_check(ufbxi_read_take_anim_channel(uc, channel_nodes[i], id, channel_names[i], &prop->curves[i].default_value));
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
	ufbxi_check(ufbxi_get_val1(node, "c", (char**)&type_and_name));
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
	// Treat the Take as a post-7000 version animation stack and layer.
	ufbx_anim_stack *stack = ufbxi_push_zero(&uc->tmp_arr_anim_stacks, ufbx_anim_stack, 1);
	ufbxi_check(stack);
	ufbxi_check(ufbxi_get_val1(node, "S", &stack->name));

	ufbx_anim_layer *layer = ufbxi_push_zero(&uc->tmp_arr_anim_layers, ufbx_anim_layer, 1);
	ufbxi_check(layer);
	layer->name.data = "BaseLayer";
	layer->name.length = 9;
	ufbxi_check(ufbxi_push_string_place_str(uc, &layer->name));

	// Set default properties
	stack->props.defaults = uc->default_props;
	layer->layer_props.defaults = uc->default_props;

	// Add a "virtual" connectable layer instead of connecting all the animated
	// properties and curves directly to keep the code consistent with post-7000.
	uint64_t stack_id = (uintptr_t)stack;
	uint64_t layer_id = (uintptr_t)layer;
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_STACK, stack_id, uc->tmp_arr_anim_stacks.num_items - 1));
	ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_ANIM_LAYER, layer_id, uc->tmp_arr_anim_layers.num_items - 1));

	ufbxi_check(ufbxi_add_connection(uc, stack_id, layer_id, NULL));

	// Read stack properties from node
	int64_t begin = 0, end = 0;
	if (!ufbxi_find_val2(node, ufbxi_LocalTime, "LL", &begin, &end)) {
		ufbxi_check(ufbxi_find_val2(node, ufbxi_ReferenceTime, "LL", &begin, &end));
	}
	stack->time_begin = (double)begin * uc->ktime_to_sec;
	stack->time_end = (double)end * uc->ktime_to_sec;

	// Read all properties of objects included in the take
	ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
		// TODO: Do some object types have another name?
		if (child->name != ufbxi_Model) continue;

		ufbxi_check(ufbxi_read_take_object(uc, child, layer_id));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_takes(ufbxi_context *uc)
{
	for (;;) {
		ufbxi_node *node;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &node));
		if (!node) break;

		if (node->name == ufbxi_Take) {
			ufbxi_check(ufbxi_read_take(uc, node));
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_connections(ufbxi_context *uc)
{
	// Retain attributes to temporary memory, no more attributes should be added after this
	// point and we need the array to connect the attributes to the node IDs.
	{
		uc->attributes = ufbxi_make_array(&uc->tmp_arr_attributes, ufbxi_attribute, uc->tmp_arr_attributes.num_items);
		ufbxi_check(uc->attributes);
	}

	// Retain skin deformers to eagerly count the number of clusters to avoid a third connection pass.
	{
		uc->skin_deformers = ufbxi_make_array(&uc->tmp_arr_skin_deformers, ufbxi_skin_deformer, uc->tmp_arr_skin_deformers.num_items);
		ufbxi_check(uc->skin_deformers);
	}

	// Read the connections to the list first
	for (;;) {
		ufbxi_node *node;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &node));
		if (!node) break;

		const char *type;

		uint64_t parent_id, child_id;
		if (uc->version < 7000) {
			const char *parent_name, *child_name;
			// Pre-7000 versions use Type::Name pairs as identifiers
			if (!ufbxi_get_val3(node, "Ccc", (char**)&type, (char**)&child_name, (char**)&parent_name)) continue;
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
		ufbxi_connectable *parent = ufbxi_find_connectable(uc, parent_id);
		if (!child || !parent) continue;

		if (parent->type == UFBXI_CONNECTABLE_SKIN_DEFORMER && child->type == UFBXI_CONNECTABLE_SKIN_CLUSTER) {
			uc->skin_deformers[parent->index].num_skins++;
		}

		if (child->type == UFBXI_CONNECTABLE_ATTRIBUTE) {
			ufbxi_attribute *attr = &uc->attributes[child->index];
			switch (parent->type) {
			case UFBXI_CONNECTABLE_MODEL:
			case UFBXI_CONNECTABLE_MESH:
			case UFBXI_CONNECTABLE_LIGHT:
			case UFBXI_CONNECTABLE_BONE:
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
	// Initialize the scene
	{
		uc->scene.metadata.creator = ufbx_empty_string;
	}

	// Initialize root node before reading any models
	{

		// The root is always the first model in the array
		ufbx_assert(uc->tmp_arr_models.size == 0);
		ufbx_model *model = ufbxi_push_zero(&uc->tmp_arr_models, ufbx_model, 1);
		ufbxi_check(model);
		model->node.type = UFBX_NODE_MODEL;
		model->node.name = ufbx_empty_string;
		model->node.transform.scale.x = 1.0;
		model->node.transform.scale.y = 1.0;
		model->node.transform.scale.z = 1.0;
	}

	// FBXHeaderExtension: Some metadata (optional)
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension));
	ufbxi_check(ufbxi_read_header_extension(uc));

	// Document: Read root ID
	if (uc->version >= 7000) {
		ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Documents));
		ufbxi_check(ufbxi_read_document(uc));
	} else {
		// Pre-7000: Root node has a specific type-name pair "Model::Scene"
		// (or reversed in binary). Use the interned name as ID as usual.
		const char *root_name = uc->from_ascii ? "Model::Scene" : "Scene\x00\x01Model";
		root_name = ufbxi_push_string_imp(uc, root_name, 12, false);
		ufbxi_check(root_name);
		uint64_t root_id = (uintptr_t)root_name;
		ufbxi_check(ufbxi_add_connectable(uc, UFBXI_CONNECTABLE_MODEL, root_id, 0));
	}

	// Definitions: Object type counts and property templates (optional)
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Definitions));
	ufbxi_check(ufbxi_read_definitions(uc));

	// Objects: Actual scene data (required)
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Objects));
	ufbxi_check(ufbxi_read_objects(uc));

	// Connections: Relationships between nodes (required)
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Connections));
	ufbxi_check(ufbxi_read_connections(uc));

	// Takes: Pre-7000 animations, don't even try to read them in
	// post-7000 versions as the code has some assumptions about the version.
	if (uc->version < 7000) {
		ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Takes));
		ufbxi_check(ufbxi_read_takes(uc));
	}

	return 1;
}

// -- Curve evaluation

static ufbxi_forceinline double ufbxi_find_cubic_bezier_t(double p1, double p2, double x0)
{
	double p1_3 = p1 * 3.0, p2_3 = p2 * 3.0;
	double a = p1_3 - p2_3 + 1.0;
	double b = p2_3 - p1_3 - p1_3;
	double c = p1_3;

	double a_3 = 3.0*a, b_2 = 2.0*b;
	double t = x0;
	double x1, t2, t3;

	// Manually unroll three iterations of Newton-Rhapson, this is enough
	// for most tangents
	t2 = t*t; t3 = t2*t; x1 = a*t3 + b*t2 + c*t - x0;
	t -= x1 / (a_3*t2 + b_2*t + c);

	t2 = t*t; t3 = t2*t; x1 = a*t3 + b*t2 + c*t - x0;
	t -= x1 / (a_3*t2 + b_2*t + c);

	t2 = t*t; t3 = t2*t; x1 = a*t3 + b*t2 + c*t - x0;
	t -= x1 / (a_3*t2 + b_2*t + c);

	const double eps = 0.00001;
	if (x1 >= -eps && x1 <= eps) return t;

	// Perform more iterations until we reach desired accuracy
	for (size_t i = 0; i < 4; i++) {
		t2 = t*t; t3 = t2*t; x1 = a*t3 + b*t2 + c*t - x0;
		t -= x1 / (a_3*t2 + b_2*t + c);
		if (x1 >= -eps && x1 <= eps) break;
	}
	return t;
}

// -- Property interpretation

ufbx_prop *ufbxi_find_prop_imp(const ufbx_props *props, const char *name, uint32_t key)
{
	do {
		ufbx_prop *prop_data = props->props;
		size_t begin = 0;
		size_t end = props->num_props;
		while (end - begin >= 16) {
			size_t mid = (begin + end) >> 1;
			const ufbx_prop *p = &prop_data[mid];
			if (p->imp_key < key) {
				begin = mid + 1;
			} else { 
				end = mid;
			}
		}

		end = props->num_props;
		for (; begin < end; begin++) {
			const ufbx_prop *p = &prop_data[begin];
			if (p->imp_key > key) break;
			if (p->name.data == name) {
				return (ufbx_prop*)p;
			}
		}

		props = props->defaults;
	} while (props);

	return NULL;
}

#define ufbxi_find_prop(props, name) ufbxi_find_prop_imp((props), (name), \
	(name[0] << 24) | (name[1] << 16) | (name[2] << 8) | name[3])

#define ufbxi_init_prop_int(m_prop, m_name, m_x) \
	(m_prop)->name.data = m_name; \
	(m_prop)->name.length = sizeof(m_name) - 1; \
	(m_prop)->imp_key = (m_name[0] << 24) | (m_name[1] << 16) | (m_name[2] << 8) | m_name[3]; \
	(m_prop)->type = UFBX_PROP_INTEGER; \
	(m_prop)->value_int = m_x; \
	(m_prop)->value_real = (double)(m_x)

#define ufbxi_init_prop_vec3(m_prop, m_name, m_x, m_y, m_z) \
	(m_prop)->name.data = m_name; \
	(m_prop)->name.length = sizeof(m_name) - 1; \
	(m_prop)->imp_key = (m_name[0] << 24) | (m_name[1] << 16) | (m_name[2] << 8) | m_name[3]; \
	(m_prop)->type = UFBX_PROP_VECTOR; \
	(m_prop)->value_int = (int64_t)(m_x); \
	(m_prop)->value_real_arr[0] = m_x; \
	(m_prop)->value_real_arr[1] = m_y; \
	(m_prop)->value_real_arr[2] = m_z

static ufbxi_forceinline ufbx_real ufbxi_find_real(const ufbx_props *props, const char *name)
{
	ufbx_prop *prop = ufbxi_find_prop(props, name);
	if (prop) {
		return prop->value_real;
	} else {
		return 0.0;
	}
}

static ufbxi_forceinline ufbx_vec3 ufbxi_find_vec3(const ufbx_props *props, const char *name)
{
	ufbx_prop *prop = ufbxi_find_prop(props, name);
	if (prop) {
		return prop->value_vec3;
	} else {
		ufbx_vec3 zero = { 0 };
		return zero;
	}
}

static ufbxi_forceinline int64_t ufbxi_find_int(const ufbx_props *props, const char *name)
{
	ufbx_prop *prop = ufbxi_find_prop(props, name);
	if (prop) {
		return prop->value_int;
	} else {
		return 0;
	}
}

static ufbxi_forceinline void ufbxi_add_translate(ufbx_transform *t, ufbx_vec3 v)
{
	t->translation.x += v.x;
	t->translation.y += v.y;
	t->translation.z += v.z;
}

static ufbxi_forceinline void ufbxi_sub_translate(ufbx_transform *t, ufbx_vec3 v)
{
	t->translation.x -= v.x;
	t->translation.y -= v.y;
	t->translation.z -= v.z;
}

static ufbxi_forceinline void ufbxi_mul_scale(ufbx_transform *t, ufbx_vec3 v)
{
	t->translation.x *= v.x;
	t->translation.y *= v.y;
	t->translation.z *= v.z;
	t->scale.x *= v.x;
	t->scale.y *= v.y;
	t->scale.z *= v.z;
}

#define UFBXI_PI ((ufbx_real)3.14159265358979323846)
#define UFBXI_DEG_TO_RAD ((ufbx_real)(UFBXI_PI / 180.0))

static ufbx_vec4 ufbxi_to_quat(ufbx_vec3 v, ufbx_rotation_order order)
{
	v.x *= UFBXI_DEG_TO_RAD * 0.5;
	v.y *= UFBXI_DEG_TO_RAD * 0.5;
	v.z *= UFBXI_DEG_TO_RAD * 0.5;
	ufbx_real cx = (ufbx_real)cos(v.x), sx = (ufbx_real)sin(v.x);
	ufbx_real cy = (ufbx_real)cos(v.y), sy = (ufbx_real)sin(v.y);
	ufbx_real cz = (ufbx_real)cos(v.z), sz = (ufbx_real)sin(v.z);
	ufbx_vec4 q;

	// Generated by `misc/gen_rotation_order.py`
	switch (order) {
	case UFBX_ROTATION_XYZ:
		q.x = -cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy + cy*sx*sz;
		q.z = cx*cy*sz - cz*sx*sy;
		q.w = cx*cy*cz + sx*sy*sz;
		break;
	case UFBX_ROTATION_XZY:
		q.x = cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy + cy*sx*sz;
		q.z = cx*cy*sz - cz*sx*sy;
		q.w = cx*cy*cz - sx*sy*sz;
		break;
	case UFBX_ROTATION_YZX:
		q.x = -cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy - cy*sx*sz;
		q.z = cx*cy*sz + cz*sx*sy;
		q.w = cx*cy*cz + sx*sy*sz;
		break;
	case UFBX_ROTATION_YXZ:
		q.x = -cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy + cy*sx*sz;
		q.z = cx*cy*sz + cz*sx*sy;
		q.w = cx*cy*cz - sx*sy*sz;
		break;
	case UFBX_ROTATION_ZXY:
		q.x = cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy - cy*sx*sz;
		q.z = cx*cy*sz - cz*sx*sy;
		q.w = cx*cy*cz + sx*sy*sz;
		break;
	case UFBX_ROTATION_ZYX:
		q.x = cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy - cy*sx*sz;
		q.z = cx*cy*sz + cz*sx*sy;
		q.w = cx*cy*cz - sx*sy*sz;
		break;
	default:
		q.x = q.y = q.z = 0.0; q.w = 1.0;
		break;
	}

	return q;
}

static ufbxi_forceinline ufbx_vec4 ufbxi_mul_quat(ufbx_vec4 a, ufbx_vec4 b)
{
	ufbx_vec4 r;
	r.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
	r.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
	r.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
	r.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
	return r;
}

static ufbxi_forceinline bool ufbxi_is_vec3_zero(ufbx_vec3 v)
{
	return (v.x == 0.0) & (v.y == 0.0) && (v.z == 0.0);
}

static void ufbxi_mul_rotate(ufbx_transform *t, ufbx_vec3 v, ufbx_rotation_order order)
{
	if (ufbxi_is_vec3_zero(v)) return;

	ufbx_vec4 q = ufbxi_to_quat(v, order);
	if (t->rotation.w != 1.0) {
		t->rotation = ufbxi_mul_quat(q, t->rotation);
	} else {
		t->rotation = q;
	}

	if (!ufbxi_is_vec3_zero(t->translation)) {
		t->translation = ufbx_rotate_vector(q, t->translation);
	}
}

static void ufbxi_mul_inv_rotate(ufbx_transform *t, ufbx_vec3 v, ufbx_rotation_order order)
{
	if (ufbxi_is_vec3_zero(v)) return;

	ufbx_vec4 q = ufbxi_to_quat(v, order);
	q.x = -q.x; q.y = -q.y; q.z = -q.z;
	if (t->rotation.w != 1.0) {
		t->rotation = ufbxi_mul_quat(q, t->rotation);
	} else {
		t->rotation = q;
	}

	if (!ufbxi_is_vec3_zero(t->translation)) {
		t->translation = ufbx_rotate_vector(q, t->translation);
	}
}

static ufbx_transform ufbxi_get_transform(const ufbx_props *props)
{

	ufbx_vec3 scale_pivot = ufbxi_find_vec3(props, ufbxi_ScalingPivot);
	ufbx_vec3 rot_pivot = ufbxi_find_vec3(props, ufbxi_RotationPivot);
	ufbx_vec3 scale_offset = ufbxi_find_vec3(props, ufbxi_ScalingOffset);
	ufbx_vec3 rot_offset = ufbxi_find_vec3(props, ufbxi_RotationOffset);

	ufbx_vec3 translation = ufbxi_find_vec3(props, ufbxi_Lcl_Translation);
	ufbx_vec3 rotation = ufbxi_find_vec3(props, ufbxi_Lcl_Rotation);
	ufbx_vec3 scaling = ufbxi_find_vec3(props, ufbxi_Lcl_Scaling);

	ufbx_vec3 pre_rotation = ufbxi_find_vec3(props, ufbxi_PreRotation);
	ufbx_vec3 post_rotation = ufbxi_find_vec3(props, ufbxi_PostRotation);

	ufbx_rotation_order order = UFBX_ROTATION_XYZ;
	ufbx_prop *order_prop = ufbxi_find_prop(props, ufbxi_RotationOrder);
	if (order_prop) {
		if (order_prop->value_int >= 0 && order_prop->value_int <= 5) {
			order = (ufbx_rotation_order)order_prop->value_int;
		}
	}

	ufbx_transform t = { { 0,0,0 }, { 0,0,0,1 }, { 1,1,1 }};

	// WorldTransform = ParentWorldTransform * T * Roff * Rp * Rpre * R * Rpost * Rp-1 * Soff * Sp * S * Sp-1
	// NOTE: Rpost is inverted (!) after converting from PostRotation Euler angles

	ufbxi_sub_translate(&t, scale_pivot);
	ufbxi_mul_scale(&t, scaling);
	ufbxi_add_translate(&t, scale_pivot);

	ufbxi_add_translate(&t, scale_offset);

	ufbxi_sub_translate(&t, rot_pivot);
	ufbxi_mul_inv_rotate(&t, post_rotation, UFBX_ROTATION_XYZ);
	ufbxi_mul_rotate(&t, rotation, order);
	ufbxi_mul_rotate(&t, pre_rotation, UFBX_ROTATION_XYZ);
	ufbxi_add_translate(&t, rot_pivot);

	ufbxi_add_translate(&t, rot_offset);

	ufbxi_add_translate(&t, translation);

	return t;
}

static void ufbxi_get_light_properties(ufbx_light *light)
{
	light->color = ufbxi_find_vec3(&light->node.props, ufbxi_Color);
	light->intensity = ufbxi_find_real(&light->node.props, ufbxi_Intensity);
}

static void ufbxi_get_bone_properties(ufbx_bone *bone)
{
	bone->length = ufbxi_find_real(&bone->node.props, ufbxi_Size);
}

static void ufbxi_get_material_properties(ufbx_material *material)
{
	material->diffuse_color = ufbxi_find_vec3(&material->props, ufbxi_DiffuseColor);
	material->specular_color = ufbxi_find_vec3(&material->props, ufbxi_SpecularColor);
}

static void ufbxi_get_anim_stack_properties(ufbx_anim_stack *stack, ufbx_scene *scene)
{
	ufbx_prop *begin, *end;
	begin = ufbxi_find_prop(&stack->props, ufbxi_LocalStart);
	end = ufbxi_find_prop(&stack->props, ufbxi_LocalStop);
	if (begin && end) {
		stack->time_begin = (double)begin->value_int * scene->metadata.ktime_to_sec;
		stack->time_end = (double)end->value_int * scene->metadata.ktime_to_sec;
	} else {
		begin = ufbxi_find_prop(&stack->props, ufbxi_ReferenceStart);
		end = ufbxi_find_prop(&stack->props, ufbxi_ReferenceStop);
		if (begin && end) {
			stack->time_begin = (double)begin->value_int * scene->metadata.ktime_to_sec;
			stack->time_end = (double)end->value_int * scene->metadata.ktime_to_sec;
		}
	}
}

static void ufbxi_get_anim_layer_properties(ufbx_anim_layer *layer)
{
	layer->weight = ufbxi_find_real(&layer->layer_props, ufbxi_Weight);
	layer->compose_rotation = ufbxi_find_int(&layer->layer_props, ufbxi_RotationAccumulationMode) == 0;
	layer->compose_scale = ufbxi_find_int(&layer->layer_props, ufbxi_ScaleAccumulationMode) == 0;
}

static void ufbxi_get_properties(ufbx_scene *scene)
{
	ufbxi_for_ptr(ufbx_node, p_node, scene->nodes.data, scene->nodes.size) {
		ufbx_node *node = *p_node;
		node->transform = ufbxi_get_transform(&node->props);

		ufbx_inherit_type inherit_type = UFBX_INHERIT_NORMAL;
		ufbx_prop *inherit_prop = ufbxi_find_prop(&node->props, ufbxi_InheritType);
		if (inherit_prop) {
			if (inherit_prop->value_int >= 0 && inherit_prop->value_int <= 2) {
				inherit_type = (ufbx_inherit_type)inherit_prop->value_int;
			}
		}
		node->inherit_type = inherit_type;
	}

	ufbxi_for(ufbx_light, light, scene->lights.data, scene->lights.size) {
		ufbxi_get_light_properties(light);
	}

	ufbxi_for(ufbx_bone, bone, scene->bones.data, scene->bones.size) {
		ufbxi_get_bone_properties(bone);
	}

	ufbxi_for(ufbx_material, material, scene->materials.data, scene->materials.size) {
		ufbxi_get_material_properties(material);
	}

	ufbxi_for(ufbx_anim_stack, stack, scene->anim_stacks.data, scene->anim_stacks.size) {
		ufbxi_get_anim_stack_properties(stack, scene);
	}

	ufbxi_for(ufbx_anim_layer, layer, scene->anim_layers.data, scene->anim_layers.size) {
		ufbxi_get_anim_layer_properties(layer);
	}
}

static void ufbxi_update_transform_matrix(ufbx_node *node, ufbx_node *parent)
{
	node->to_parent = ufbx_get_transform_matrix(&node->transform);

	if (parent) {

		node->world_transform.rotation = ufbxi_mul_quat(parent->world_transform.rotation, node->transform.rotation);
		node->world_transform.translation = ufbx_transform_position(&parent->to_root, node->transform.translation);
		if (node->inherit_type != UFBX_INHERIT_NO_SCALE) {
			node->world_transform.scale.x = parent->world_transform.scale.x * node->transform.scale.x;
			node->world_transform.scale.y = parent->world_transform.scale.y * node->transform.scale.y;
			node->world_transform.scale.z = parent->world_transform.scale.z * node->transform.scale.z;
		} else {
			node->world_transform.scale = node->transform.scale;
		}

		if (node->inherit_type == UFBX_INHERIT_NORMAL) {
			ufbx_matrix_mul(&node->to_root, &parent->to_root, &node->to_parent);
		} else {
			node->to_root = ufbx_get_transform_matrix(&node->world_transform);
		}
	} else {
		node->to_root = node->to_parent;
		node->world_transform = node->transform;
	}

	ufbxi_for_ptr(ufbx_node, p_child, node->children.data, node->children.size) {
		ufbxi_update_transform_matrix(*p_child, node);
	}
}

// -- Loading

typedef struct {
	void *data;
	size_t size;
} ufbxi_void_array;

typedef struct {
	ufbxi_connectable_type type;
	uint32_t index;

	ufbx_node *node;
	ufbx_model *model;
	ufbx_mesh *mesh;
	ufbx_light *light;
	ufbx_bone *bone;
	ufbx_mesh *geometry;
	ufbx_material *material;
	ufbx_anim_stack *anim_stack;
	ufbx_anim_layer *anim_layer;
	ufbx_anim_prop *anim_prop;
	ufbx_anim_curve *anim_curve;
	ufbxi_attribute *attribute;
	ufbx_skin *skin_cluster;
	ufbxi_skin_deformer *skin_deformer;
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

	data->type = type;
	data->index = index;

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
	case UFBXI_CONNECTABLE_BONE:
		data->bone = &uc->scene.bones.data[index];
		data->node = &data->bone->node;
		break;
	case UFBXI_CONNECTABLE_GEOMETRY:
		data->geometry = &uc->geometries[index];
		break;
	case UFBXI_CONNECTABLE_MATERIAL:
		data->material = &uc->scene.materials.data[index];
		break;
	case UFBXI_CONNECTABLE_ANIM_STACK:
		data->anim_stack = &uc->scene.anim_stacks.data[index];
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
	case UFBXI_CONNECTABLE_SKIN_CLUSTER:
		data->skin_cluster = &uc->skin_clusters[index];
		break;
	case UFBXI_CONNECTABLE_SKIN_DEFORMER:
		data->skin_deformer = &uc->skin_deformers[index];
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
		uc->scene.metadata.num_total_child_refs += node->children.size;
		node->children.data = ufbxi_push(&uc->result, ufbx_node*, node->children.size);
		node->children.size = 0;
		ufbxi_check(node->children.data);

		// Remove default properties for nodes that were not merged. Merge default
		// values into the properties before removing defaults.
		if (node->props.defaults != uc->default_props) {
			ufbxi_check(ufbxi_merge_properties(uc, &node->props, node->props.defaults, &node->props, &uc->result));
			ufbxi_remove_default_properties(&node->props, uc->default_props, &uc->result);
		}

		*dst++ = (ufbx_node*)node;
		ptr += size;
	}
	*p_dst = dst;

	return 1;
}

ufbxi_forceinline static void ufbxi_patch_index(int32_t **p_index, int32_t *zero, int32_t *consecutive)
{
	if (*p_index == ufbxi_sentinel_index_zero) {
		*p_index = zero;
	} else if (*p_index == ufbxi_sentinel_index_consecutive) {
		*p_index = consecutive;
	}
}

ufbxi_nodiscard static int ufbxi_merge_attribute_properties(ufbxi_context *uc, ufbx_props *props, ufbx_props *attr)
{
	ufbxi_buf_state stack_state = ufbxi_buf_push_state(&uc->tmp_stack);

	// Merge default values into the node first if necessary, store the result in `tmp_stack`
	// as the properties will be re-merged immediately afterwards. Merge defaults from both
	// the node and the attribute.
	if (props->defaults != uc->default_props) {
		if (attr->defaults) {
			ufbxi_check(ufbxi_merge_properties(uc, props, attr->defaults, props, &uc->tmp_stack));
		}
		ufbxi_check(ufbxi_merge_properties(uc, props, props->defaults, props, &uc->tmp_stack));

	}

	// Merge the attributes to the properties
	ufbxi_check(ufbxi_merge_properties(uc, props, props, attr, &uc->result));

	ufbxi_buf_pop_state(&uc->tmp_stack, &stack_state);

	ufbxi_remove_default_properties(props, uc->default_props, &uc->result);

	return 1;
}

ufbxi_nodiscard static int ufbxi_check_node_depth(ufbxi_context *uc, ufbx_node *node, uint32_t depth)
{
	ufbxi_check(depth < uc->opts.max_child_depth);
	ufbxi_for_ptr(ufbx_node, p_child, node->children.data, node->children.size) {
		ufbxi_check(ufbxi_check_node_depth(uc, *p_child, depth + 1));
	}

	return 1;
}

static ufbxi_forceinline int ufbxi_cmp_anim_prop(const void *va, const void *vb)
{
	const ufbx_anim_prop *a = (const ufbx_anim_prop*)va, *b = (const ufbx_anim_prop*)vb;
	if (a->target < b->target) return -1;
	if (a->target > b->target) return +1;
	if (a->index < b->index) return -1;
	if (a->index > b->index) return +1;
	if (a->imp_key < b->imp_key) return -1;
	if (a->imp_key > b->imp_key) return +1;
	return strcmp(a->name.data, b->name.data);
}

static ufbxi_forceinline int ufbxi_cmp_anim_prop_imp(const ufbx_anim_prop *a, ufbx_anim_target target, uint32_t index)
{
	if (a->target < target) return -1;
	if (a->target > target) return +1;
	if (a->index < index) return -1;
	if (a->index > index) return +1;
	return 0;
}

ufbxi_nodiscard static int ufbxi_finalize_scene(ufbxi_context *uc)
{
	// Retrieve all temporary arrays
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_model), &uc->scene.models, &uc->tmp_arr_models));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_mesh), &uc->scene.meshes, &uc->tmp_arr_meshes));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_material), &uc->scene.materials, &uc->tmp_arr_materials));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_light), &uc->scene.lights, &uc->tmp_arr_lights));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_bone), &uc->scene.bones, &uc->tmp_arr_bones));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_anim_stack), &uc->scene.anim_stacks, &uc->tmp_arr_anim_stacks));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_anim_layer), &uc->scene.anim_layers, &uc->tmp_arr_anim_layers));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_anim_prop), &uc->scene.anim_props, &uc->tmp_arr_anim_props));
	ufbxi_check(ufbxi_retain_array(uc, sizeof(ufbx_anim_curve), &uc->scene.anim_curves, &uc->tmp_arr_anim_curves));

	// Retain temporary arrays
	uc->skin_clusters = ufbxi_make_array(&uc->tmp_arr_skin_clusters, ufbx_skin, uc->tmp_arr_skin_clusters.num_items);
	ufbxi_check(uc->skin_clusters);

	// Linearize the connections into an array for processing. This includes both
	// connections read from `Connections` and "virtual" connections added elsewhere using
	// `ufbxi_add_connection()`
	size_t num_conns = uc->tmp_connection.num_items;
	ufbxi_connection *conns = ufbxi_make_array(&uc->tmp_connection, ufbxi_connection, num_conns);
	ufbxi_check(conns);

	uc->num_geometries = uc->tmp_arr_geometry.num_items;
	uc->geometries = ufbxi_make_array(&uc->tmp_arr_geometry, ufbx_mesh, uc->num_geometries);
	ufbxi_check(uc->geometries);

	// Generate and patch procedural index buffers
	int32_t *zero_indices = ufbxi_push(&uc->result, int32_t, uc->max_zero_indices);
	int32_t *consecutive_indices = ufbxi_push(&uc->result, int32_t, uc->max_consecutive_indices);
	{
		ufbxi_check(zero_indices && consecutive_indices);
		memset(zero_indices, 0, sizeof(int32_t) * uc->max_zero_indices);
		for (size_t i = 0; i < uc->max_consecutive_indices; i++) {
			consecutive_indices[i] = (int32_t)i;
		}

		ufbxi_for(ufbx_mesh, geom, uc->geometries, uc->num_geometries) {
			ufbxi_patch_index(&geom->vertex_normal.indices, zero_indices, consecutive_indices);
			ufbxi_patch_index(&geom->vertex_binormal.indices, zero_indices, consecutive_indices);
			ufbxi_patch_index(&geom->vertex_tangent.indices, zero_indices, consecutive_indices);
			ufbxi_patch_index(&geom->face_material, zero_indices, consecutive_indices);

			ufbxi_for(ufbx_uv_set, set, geom->uv_sets.data, geom->uv_sets.size) {
				ufbxi_patch_index(&set->vertex_uv.indices, zero_indices, consecutive_indices);
				ufbxi_patch_index(&set->vertex_binormal.indices, zero_indices, consecutive_indices);
				ufbxi_patch_index(&set->vertex_tangent.indices, zero_indices, consecutive_indices);
			}

			ufbxi_for(ufbx_color_set, set, geom->color_sets.data, geom->color_sets.size) {
				ufbxi_patch_index(&set->vertex_color.indices, zero_indices, consecutive_indices);
			}

			// Assign first UV and color sets as the "canonical" ones
			if (geom->uv_sets.size > 0) {
				geom->vertex_uv = geom->uv_sets.data[0].vertex_uv;
				geom->vertex_binormal = geom->uv_sets.data[0].vertex_binormal;
				geom->vertex_tangent = geom->uv_sets.data[0].vertex_tangent;
			}
			if (geom->color_sets.size > 0) geom->vertex_color = geom->color_sets.data[0].vertex_color;
		}
	}

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

			// Merge attribute properties to the node
			if (child.attribute) {
				ufbxi_check(ufbxi_merge_attribute_properties(uc, &parent.node->props, &child.attribute->props));
			}
		}

		// Skin clusters are the parents of their bones for some reason (?)
		if (parent.skin_cluster) {
			if (child.node) {
				parent.skin_cluster->bone = child.node;
			}
		}

		// Count animation stack layers
		if (parent.anim_stack) {
			if (child.anim_layer) {
				parent.anim_stack->layers.size++;
			}
		}

		if (child.anim_prop && prop) {

			ufbx_anim_target target;
			switch (parent.type) {
			case UFBXI_CONNECTABLE_MODEL: target = UFBX_ANIM_MODEL; break;
			case UFBXI_CONNECTABLE_MESH: target = UFBX_ANIM_MESH; break;
			case UFBXI_CONNECTABLE_LIGHT: target = UFBX_ANIM_LIGHT; break;
			case UFBXI_CONNECTABLE_BONE: target = UFBX_ANIM_BONE; break;
			case UFBXI_CONNECTABLE_MATERIAL: target = UFBX_ANIM_MATERIAL; break;
			case UFBXI_CONNECTABLE_ANIM_LAYER: target = UFBX_ANIM_ANIM_LAYER; break;
			default: target = UFBX_ANIM_UNKNOWN;
			}

			if (target != UFBX_ANIM_UNKNOWN) {
				size_t len = strlen(prop);
				child.anim_prop->target = target;
				child.anim_prop->index = parent.index;
				child.anim_prop->name.data = prop;
				child.anim_prop->name.length = strlen(prop);
				child.anim_prop->imp_key = ufbxi_get_name_key(prop, len);
			}
		}

		if (parent.mesh) {
			if (child.geometry) {
				// Retain some properties of the mesh node (not geometry)
				size_t num_materials = parent.mesh->materials.size;

				// Copy all non-node properties
				ufbxi_check(ufbxi_merge_attribute_properties(uc, &parent.node->props, &child.geometry->node.props));
				memcpy((char*)parent.mesh + sizeof(ufbx_node), (char*)child.geometry + sizeof(ufbx_node), sizeof(ufbx_mesh) - sizeof(ufbx_node));

				// Restore the mesh properties
				parent.mesh->materials.size = num_materials;

				// HACK: Store the parent mesh in the geometry's node `parent`
				child.geometry->node.parent = &parent.mesh->node;
			}

			if (child.material) {
				parent.mesh->materials.size++;
			}
		}

		if (parent.skin_deformer) {
			if (child.skin_cluster) {
				// Store skin deformers as indices as they will be patched during the first pass as well
				if (parent.skin_deformer->skin_index == NULL) {
					parent.skin_deformer->skin_index = ufbxi_push(&uc->tmp, uint32_t, parent.skin_deformer->num_skins);
					ufbxi_check(parent.skin_deformer->skin_index);
					parent.skin_deformer->num_skins = 0;
				}
				parent.skin_deformer->skin_index[parent.skin_deformer->num_skins++] = child.index;
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
				uint32_t index = 0;
				if (prop) {
					if (prop == ufbxi_Y || prop == ufbxi_D_Y) index = 1;
					if (prop == ufbxi_Z || prop == ufbxi_D_Z) index = 2;
				}

				child.anim_curve->prop = parent.anim_prop;
				child.anim_curve->index = index;
				child.anim_curve->default_value = parent.anim_prop->curves[index].default_value;
				parent.anim_prop->curves[index] = *child.anim_curve;
			}
		}
	}

	// Allocate storage for child arrays
	ufbxi_for(ufbx_anim_stack, stack, uc->scene.anim_stacks.data, uc->scene.anim_stacks.size) {
		stack->layers.data = ufbxi_push(&uc->result, ufbx_anim_layer*, stack->layers.size);
		ufbxi_check(stack->layers.data);
		stack->layers.size = 0;
	}

	ufbxi_for(ufbx_anim_layer, layer, uc->scene.anim_layers.data, uc->scene.anim_layers.size) {
		layer->props.data = ufbxi_push(&uc->result, ufbx_anim_prop, layer->props.size + 1);
		ufbxi_check(layer->props.data);
		layer->props.size = 0;
	}

	ufbxi_for(ufbx_mesh, mesh, uc->scene.meshes.data, uc->scene.meshes.size) {

		// Clamp `face_material` array / remove it if there's no materials
		size_t num_materials = mesh->materials.size;
		if (num_materials == 0) {
			mesh->face_material = NULL;
		} else if (mesh->face_material && mesh->face_material != zero_indices) {
			ufbxi_for(int32_t, p_mat, mesh->face_material, mesh->num_faces) {
				int32_t mat = *p_mat;
				if (mat < 0 || (size_t)mat >= num_materials) {
					*p_mat = 0;
				}
			}
		}

		uc->scene.metadata.num_total_material_refs += mesh->materials.size;
		mesh->materials.data = ufbxi_push(&uc->result, ufbx_material*, mesh->materials.size);
		ufbxi_check(mesh->materials.data);
		mesh->materials.size = 0;
	}

	// Add all nodes to the scenes node list
	size_t num_nodes = uc->scene.models.size + uc->scene.meshes.size + uc->scene.lights.size + uc->scene.bones.size;
	ufbx_node **nodes = ufbxi_push(&uc->result, ufbx_node*, num_nodes);
	ufbxi_check(nodes);
	uc->scene.nodes.data = nodes;
	uc->scene.nodes.size = num_nodes;
	ufbxi_check(ufbxi_collect_nodes(uc, sizeof(ufbx_model), &nodes, uc->scene.models.data, uc->scene.models.size));
	ufbxi_check(ufbxi_collect_nodes(uc, sizeof(ufbx_mesh), &nodes, uc->scene.meshes.data, uc->scene.meshes.size));
	ufbxi_check(ufbxi_collect_nodes(uc, sizeof(ufbx_light), &nodes, uc->scene.lights.data, uc->scene.lights.size));
	ufbxi_check(ufbxi_collect_nodes(uc, sizeof(ufbx_bone), &nodes, uc->scene.bones.data, uc->scene.bones.size));
	ufbx_assert(nodes == uc->scene.nodes.data + uc->scene.nodes.size);

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

	// Second pass of connections
	ufbxi_for(ufbxi_connection, conn, conns, num_conns) {
		ufbxi_connectable_data parent, child;
		if (!ufbxi_find_connectable_data(uc, &parent, conn->parent_id)) continue;
		if (!ufbxi_find_connectable_data(uc, &child, conn->child_id)) continue;
		const char *prop = conn->prop_name;

		if (parent.mesh) {
			if (child.material) {
				parent.mesh->materials.data[parent.mesh->materials.size++] = child.material;
			}
		}

		if (parent.anim_stack) {
			if (child.anim_layer) {
				parent.anim_stack->layers.data[parent.anim_stack->layers.size++] = child.anim_layer;
			}
		}

		// Connect deformers to meshes
		if (child.skin_deformer) {
			ufbx_mesh *mesh = parent.mesh;
			if (mesh == NULL && parent.geometry) {
				mesh = (ufbx_mesh*)parent.geometry->node.parent;
			}
			if (mesh) {
				ufbx_assert(mesh->node.type == UFBX_NODE_MESH);

				// Grow the skin array if there's multiple clusters per mesh
				size_t num_new_skins = child.skin_deformer->num_skins;
				size_t num_old_skins = mesh->skins.size;
				ufbx_skin *skins = ufbxi_push(&uc->result, ufbx_skin, num_old_skins + num_new_skins);
				ufbxi_check(skins);
				memcpy(skins, mesh->skins.data, num_old_skins * sizeof(ufbx_skin));
				mesh->skins.data = skins;

				// Fetch the new skins through the deformer indices. Only
				// add skins that are bound to bones properly.
				for (size_t i = 0; i < num_new_skins; i++) {
					ufbx_skin *skin = &uc->skin_clusters[child.skin_deformer->skin_index[i]];
					if (!skin->bone) continue;

					skins[mesh->skins.size++] = *skin;

					// If there's weights there must be at least one vertex
					// so we can potentially map invalid indices to 0
					if (skin->num_weights > 0) {
						ufbxi_check(mesh->num_vertices > 0);
					}

					// Validate indices
					int32_t invalid_index = uc->opts.allow_nonexistent_indices ? -1 : 0;
					ufbxi_for(int32_t, p_ix, skin->indices, skin->num_weights) {
						int32_t ix = *p_ix;
						if (ix < 0 || (uint32_t)ix >= mesh->num_vertices) {
							*p_ix = invalid_index;
						}
					}
				}
				
				if (num_old_skins == 0 && mesh->skins.size > 0) {
					uc->scene.metadata.num_total_skins += mesh->skins.size - num_old_skins;
					uc->scene.metadata.num_skinned_positions += mesh->num_vertices;
					uc->scene.metadata.num_skinned_indices += mesh->num_indices;
					uc->scene.metadata.max_skinned_positions = ufbxi_max_sz(uc->scene.metadata.max_skinned_positions, mesh->num_vertices);
					uc->scene.metadata.max_skinned_indices = ufbxi_max_sz(uc->scene.metadata.max_skinned_indices, mesh->num_indices);
				}
			}
		}
	}

	// Sort animated properties by target/index/name
	ufbxi_for(ufbx_anim_layer, layer, uc->scene.anim_layers.data, uc->scene.anim_layers.size) {
		qsort(layer->props.data, layer->props.size, sizeof(ufbx_anim_prop), &ufbxi_cmp_anim_prop);

		// Set one past the last anim prop as sentinel INVALID anim prop
		ufbx_anim_prop *sentinel = &layer->props.data[layer->props.size];
		sentinel->name = ufbx_empty_string;
		sentinel->imp_key = 0;
		sentinel->target = UFBX_ANIM_INVALID;
		sentinel->index = 0;
		sentinel->layer = layer;
	}

	uc->scene.root = &uc->scene.models.data[0];
	uc->scene.metadata.ktime_to_sec = uc->ktime_to_sec;

	// Check that the nodes are not too nested
	ufbxi_check(ufbxi_check_node_depth(uc, &uc->scene.root->node, 0));

	return 1;
}

ufbxi_nodiscard static int ufbxi_load_imp(ufbxi_context *uc)
{
	ufbxi_check(ufbxi_load_strings(uc));
	ufbxi_check(ufbxi_load_maps(uc));
	ufbxi_check(ufbxi_load_default_props(uc));
	ufbxi_check(ufbxi_begin_parse(uc));
	ufbxi_check(ufbxi_read_root(uc));
	ufbxi_check(ufbxi_finalize_scene(uc));

	ufbxi_get_properties(&uc->scene);
	ufbxi_update_transform_matrix(&uc->scene.root->node, NULL);

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
	imp->memory_block = NULL;
	imp->memory_block_size = 0;

	imp->scene.metadata.result_memory_used = imp->ator.current_size;
	imp->scene.metadata.temp_memory_used = uc->ator_tmp.current_size;
	imp->scene.metadata.result_allocs = uc->opts.max_result_allocs - imp->ator.allocs_left;
	imp->scene.metadata.temp_allocs = uc->opts.max_temp_allocs - uc->ator_tmp.allocs_left;

	uc->scene_imp = imp;

	return 1;
}

static void ufbxi_free_temp(ufbxi_context *uc)
{
	ufbxi_map_free(&uc->string_map);
	ufbxi_map_free(&uc->prop_type_map);
	ufbxi_map_free(&uc->connectable_map);

	ufbxi_buf_free(&uc->tmp);
	ufbxi_buf_free(&uc->tmp_parse);
	ufbxi_buf_free(&uc->tmp_stack);
	ufbxi_buf_free(&uc->tmp_connection);
	ufbxi_buf_free(&uc->tmp_sort);
	ufbxi_buf_free(&uc->tmp_arr_models);
	ufbxi_buf_free(&uc->tmp_arr_meshes);
	ufbxi_buf_free(&uc->tmp_arr_geometry);
	ufbxi_buf_free(&uc->tmp_arr_materials);
	ufbxi_buf_free(&uc->tmp_arr_lights);
	ufbxi_buf_free(&uc->tmp_arr_bones);
	ufbxi_buf_free(&uc->tmp_arr_anim_stacks);
	ufbxi_buf_free(&uc->tmp_arr_anim_layers);
	ufbxi_buf_free(&uc->tmp_arr_anim_props);
	ufbxi_buf_free(&uc->tmp_arr_anim_curves);
	ufbxi_buf_free(&uc->tmp_arr_attributes);
	ufbxi_buf_free(&uc->tmp_arr_skin_clusters);
	ufbxi_buf_free(&uc->tmp_arr_skin_deformers);

	ufbxi_free(&uc->ator_tmp, ufbxi_node, uc->top_nodes, uc->top_nodes_cap);

	ufbxi_free(&uc->ator_tmp, char, uc->ascii.token.str_data, uc->ascii.token.str_cap);
	ufbxi_free(&uc->ator_tmp, char, uc->ascii.prev_token.str_data, uc->ascii.prev_token.str_cap);

	ufbxi_free(&uc->ator_tmp, char, uc->read_buffer, uc->read_buffer_size);
	ufbxi_free(&uc->ator_tmp, char, uc->convert_buffer, uc->convert_buffer_size);

	ufbx_assert(uc->ator_tmp.current_size == 0);
}

static void ufbxi_free_result(ufbxi_context *uc)
{
	ufbxi_buf_free(&uc->result);
	ufbxi_buf_free(&uc->string_buf);

	ufbx_assert(uc->ator_result.current_size == 0);
}

#define ufbxi_default_opt(name, value) if (!opts->name) opts->name = value

static void ufbxi_expand_defaults(ufbx_load_opts *opts)
{
	ufbxi_default_opt(max_temp_memory, 0xf0000000);
	ufbxi_default_opt(max_result_memory, 0xf0000000);
	ufbxi_default_opt(max_temp_allocs, 0x10000000);
	ufbxi_default_opt(max_result_allocs, 0x10000000);
	ufbxi_default_opt(temp_huge_size, 0x100000);
	ufbxi_default_opt(result_huge_size, 0x100000);
	ufbxi_default_opt(max_ascii_token_length, 0x10000000);
	ufbxi_default_opt(read_buffer_size, 4096);
	ufbxi_default_opt(max_properties, 0x10000000);
	ufbxi_default_opt(max_string_length, 0x10000000);
	ufbxi_default_opt(max_strings, 0x10000000);
	ufbxi_default_opt(max_node_depth, 0x10000000);
	ufbxi_default_opt(max_node_values, 0x10000000);
	ufbxi_default_opt(max_node_children, 0x10000000);
	ufbxi_default_opt(max_array_size, 0x10000000);
	ufbxi_default_opt(max_child_depth, 200);
}

static void ufbxi_expand_evaluate_defaults(ufbx_evaluate_opts *opts)
{
	(void)opts;
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
	uc->ator_tmp.allocs_left = uc->opts.max_temp_allocs;
	uc->ator_tmp.huge_size = uc->opts.temp_huge_size;
	uc->ator_result.error = &uc->error;
	uc->ator_result.ator = uc->opts.result_allocator;
	uc->ator_result.max_size = uc->opts.max_result_memory;
	uc->ator_result.allocs_left = uc->opts.max_result_allocs;
	uc->ator_result.huge_size = uc->opts.result_huge_size;

	uc->string_map.ator = &uc->ator_tmp;
	uc->prop_type_map.ator = &uc->ator_tmp;
	uc->connectable_map.ator = &uc->ator_tmp;

	uc->tmp.ator = &uc->ator_tmp;
	uc->tmp_parse.ator = &uc->ator_tmp;
	uc->tmp_stack.ator = &uc->ator_tmp;
	uc->tmp_connection.ator = &uc->ator_tmp;
	uc->tmp_sort.ator = &uc->ator_tmp;
	uc->tmp_arr_models.ator = &uc->ator_tmp;
	uc->tmp_arr_meshes.ator = &uc->ator_tmp;
	uc->tmp_arr_lights.ator = &uc->ator_tmp;
	uc->tmp_arr_bones.ator = &uc->ator_tmp;
	uc->tmp_arr_geometry.ator = &uc->ator_tmp;
	uc->tmp_arr_materials.ator = &uc->ator_tmp;
	uc->tmp_arr_anim_stacks.ator = &uc->ator_tmp;
	uc->tmp_arr_anim_layers.ator = &uc->ator_tmp;
	uc->tmp_arr_anim_props.ator = &uc->ator_tmp;
	uc->tmp_arr_anim_curves.ator = &uc->ator_tmp;
	uc->tmp_arr_attributes.ator = &uc->ator_tmp;
	uc->tmp_arr_skin_clusters.ator = &uc->ator_tmp;
	uc->tmp_arr_skin_deformers.ator = &uc->ator_tmp;

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

// -- Animation evaluation

static ufbxi_forceinline void ufbxi_evaluate_reserve_size(uint32_t *p_offset, size_t size, size_t num)
{
	uint32_t align_mask = ufbxi_size_align_mask(size);
	*p_offset = ufbxi_align_to_mask(*p_offset, align_mask) + (uint32_t)(size*num);
}

static ufbxi_forceinline void *ufbxi_evaluate_push_size(char *data, uint32_t *p_offset, size_t size, size_t num)
{
	uint32_t align_mask = ufbxi_size_align_mask(size);
	uint32_t pos = ufbxi_align_to_mask(*p_offset, align_mask);
	*p_offset = pos + (uint32_t)(size*num);
	return data + pos;
}

#define ufbxi_evaluate_reserve(p_offset, type, num) ufbxi_evaluate_reserve_size((p_offset), sizeof(type), (num))
#define ufbxi_evaluate_push(data, p_offset, type, num) (type*)ufbxi_evaluate_push_size((data), (p_offset), sizeof(type), (num))

static void ufbxi_evaluate_prop(ufbx_prop *prop, ufbx_anim_prop *anim_prop, double time)
{
	prop->name = anim_prop->name;
	prop->imp_key = anim_prop->imp_key;
	prop->value_str = ufbx_empty_string;

	// TODO: Set this based on something
	prop->type = UFBX_PROP_UNKNOWN;

	prop->value_real_arr[0] = ufbx_evaluate_curve(&anim_prop->curves[0], time);
	prop->value_real_arr[1] = ufbx_evaluate_curve(&anim_prop->curves[1], time);
	prop->value_real_arr[2] = ufbx_evaluate_curve(&anim_prop->curves[2], time);

	prop->value_int = (int64_t)prop->value_real_arr[0];
}

static ufbx_node *ufbxi_translate_node(const ufbx_scene *src, ufbx_scene *dst, ufbx_node *old)
{
	if (old == NULL) return NULL;

	void *src_base, *dst_base;
	switch (old->type) {
	case UFBX_NODE_MODEL: src_base = src->models.data; dst_base = dst->models.data; break;
	case UFBX_NODE_MESH: src_base = src->meshes.data; dst_base = dst->meshes.data; break;
	case UFBX_NODE_LIGHT: src_base = src->lights.data; dst_base = dst->lights.data; break;
	case UFBX_NODE_BONE: src_base = src->bones.data; dst_base = dst->bones.data; break;
	default: ufbx_assert(0 && "Unhandled node type"); return old;
	}

	return (ufbx_node*)((char*)dst_base + ((char*)old - (char*)src_base));
}

static ufbx_material *ufbxi_translate_material(const ufbx_scene *src, ufbx_scene *dst, ufbx_material *old)
{
	if (old == NULL) return NULL;

	void *src_base = src->materials.data, *dst_base = dst->materials.data;
	return (ufbx_material*)((char*)dst_base + ((char*)old - (char*)src_base));
}

static void ufbxi_translate_skin(const ufbx_scene *src, ufbx_scene *dst, ufbx_skin *p_new, ufbx_skin *p_old)
{
	*p_new = *p_old;
	p_new->bone = ufbxi_translate_node(src, dst, p_old->bone);
}

static void ufbxi_translate_node_refs(ufbx_node ***p_child_refs, const ufbx_scene *src, ufbx_scene *dst, ufbx_node *node)
{
	node->parent = ufbxi_translate_node(src, dst, node->parent);

	size_t num_children = node->children.size;
	if (num_children > 0) {
		ufbx_node **src_node = node->children.data;
		ufbx_node **dst_node = *p_child_refs;
		*p_child_refs = dst_node + num_children;
		node->children.data = dst_node;
		for (size_t i = 0; i < num_children; i++) {
			dst_node[i] = ufbxi_translate_node(src, dst, src_node[i]);
		}
	}
}

static ufbx_scene *ufbxi_evaluate_scene(const ufbx_scene *scene, const ufbx_evaluate_opts *user_opts, double time)
{
	ufbx_evaluate_opts opts;
	if (user_opts) {
		opts = *user_opts;
	} else {
		memset(&opts, 0, sizeof(opts));
	}
	ufbxi_expand_evaluate_defaults(&opts);
	if (opts.layer == NULL) {
		if (scene->anim_layers.size == 0) return 0;
		opts.layer = &scene->anim_layers.data[0];
	}

	ufbx_anim_layer layer = *opts.layer;

	uint32_t alloc_size = 0;

	ufbxi_evaluate_reserve(&alloc_size, ufbxi_scene_imp, 1);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_node*, scene->nodes.size);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_model, scene->models.size);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_mesh, scene->meshes.size);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_light, scene->lights.size);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_bone, scene->bones.size);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_material, scene->materials.size);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_prop, layer.props.size);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_node*, scene->metadata.num_total_child_refs);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_material*, scene->metadata.num_total_material_refs);
	ufbxi_evaluate_reserve(&alloc_size, ufbx_skin, scene->metadata.num_total_skins);

	if (opts.evaluate_skinned_vertices) {
		// Reserve one extra NULL -1 vertex position/normal per mesh
		ufbxi_evaluate_reserve(&alloc_size, ufbx_vec3, scene->metadata.num_skinned_positions + scene->meshes.size);
		ufbxi_evaluate_reserve(&alloc_size, ufbx_vec3, scene->metadata.num_skinned_indices + scene->meshes.size);
		ufbxi_evaluate_reserve(&alloc_size, int32_t, scene->metadata.num_skinned_indices);
		ufbxi_evaluate_reserve(&alloc_size, ufbx_real, scene->metadata.max_skinned_positions);
		ufbxi_evaluate_reserve(&alloc_size, ufbx_real, scene->metadata.max_skinned_positions);
	}

	ufbx_error err;
	ufbxi_allocator ator;
	ator.ator = opts.allocator;
	ator.error = &err;
	ator.current_size = 0;
	ator.max_size = 0xf0000000;
	ator.allocs_left = 3;
	ator.huge_size = 0xf0000000;

	ufbxi_scene_imp *old_imp = (ufbxi_scene_imp*)opts.reuse_scene;

	char *data = NULL;
	if (old_imp) {
		ufbx_assert(old_imp->magic == UFBXI_SCENE_IMP_MAGIC);
		if (old_imp->memory_block_size >= alloc_size) {
			data = old_imp->memory_block;
			alloc_size = (uint32_t)old_imp->memory_block_size;
			ator = old_imp->ator;
		} else {
			ufbxi_allocator old_ator = old_imp->ator;
			ufbxi_free(&old_ator, char, old_imp->memory_block, old_imp->memory_block_size);
		}
	}

	if (!data) {
		data = ufbxi_alloc(&ator, char, alloc_size);
		if (!data) return NULL;
	}
	uint32_t offset = 0;

	ufbxi_scene_imp *imp = ufbxi_evaluate_push(data, &offset, ufbxi_scene_imp, 1);
	ufbx_node **nodes = ufbxi_evaluate_push(data, &offset, ufbx_node*, scene->nodes.size);
	ufbx_model *models = ufbxi_evaluate_push(data, &offset, ufbx_model, scene->models.size);
	ufbx_mesh *meshes = ufbxi_evaluate_push(data, &offset, ufbx_mesh, scene->meshes.size);
	ufbx_light *lights = ufbxi_evaluate_push(data, &offset, ufbx_light, scene->lights.size);
	ufbx_bone *bones = ufbxi_evaluate_push(data, &offset, ufbx_bone, scene->bones.size);
	ufbx_material *materials = ufbxi_evaluate_push(data, &offset, ufbx_material, scene->materials.size);
	ufbx_prop *props = ufbxi_evaluate_push(data, &offset, ufbx_prop, layer.props.size);
	ufbx_node **child_refs = ufbxi_evaluate_push(data, &offset, ufbx_node*, scene->metadata.num_total_child_refs);
	ufbx_material **material_refs = ufbxi_evaluate_push(data, &offset, ufbx_material*, scene->metadata.num_total_material_refs);
	ufbx_skin *skins = ufbxi_evaluate_push(data, &offset, ufbx_skin, scene->metadata.num_total_skins);
	ufbx_vec3 *skinned_positions = NULL, *skinned_normals = NULL;
	int32_t *skinned_normal_indices = NULL;
	ufbx_real *skinned_pos_weights = NULL;
	ufbx_real *skinned_pos_total_weight = NULL;
	if (opts.evaluate_skinned_vertices) {
		skinned_positions = ufbxi_evaluate_push(data, &offset, ufbx_vec3, scene->metadata.num_skinned_positions + scene->meshes.size);
		skinned_normals = ufbxi_evaluate_push(data, &offset, ufbx_vec3, scene->metadata.num_skinned_indices + scene->meshes.size);
		skinned_normal_indices = ufbxi_evaluate_push(data, &offset, int32_t, scene->metadata.num_skinned_indices);
		skinned_pos_weights = ufbxi_evaluate_push(data, &offset, ufbx_real, scene->metadata.max_skinned_positions);
		skinned_pos_total_weight = ufbxi_evaluate_push(data, &offset, ufbx_real, scene->metadata.max_skinned_positions);
	}

	ufbx_assert(offset == alloc_size);

	memset((char*)imp + sizeof(ufbx_scene), 0, sizeof(ufbxi_scene_imp) - sizeof(ufbx_scene));
	imp->scene = *scene;
	imp->scene.nodes.data = nodes;
	imp->scene.models.data = models;
	imp->scene.meshes.data = meshes;
	imp->scene.lights.data = lights;
	imp->scene.bones.data = bones;
	imp->scene.materials.data = materials;

	imp->magic = UFBXI_SCENE_IMP_MAGIC;
	imp->ator = ator;
	imp->memory_block = data;
	imp->memory_block_size = alloc_size;

	ufbx_anim_prop *ap = layer.props.data;

	ufbx_prop *prop = props;
	ufbx_node **node = nodes;

	// Skip unknown animation properties
	while (ap->target == UFBX_ANIM_UNKNOWN) {
		continue;
	}

	size_t num_models = scene->models.size;
	for (size_t i = 0; i < num_models; i++) {
		ufbx_model *model = &imp->scene.models.data[i];
		ufbx_model *src = &scene->models.data[i];
		*model = *src;

		ufbxi_translate_node_refs(&child_refs, scene, &imp->scene, &model->node);
		*node++ = &model->node;

		ufbx_prop *props_begin = prop;
		while (ap->target == UFBX_ANIM_MODEL && ap->index == i) {
			ufbxi_evaluate_prop(prop, ap, time);
			prop++;
			ap++;
		}

		if (prop - props_begin > 0) {
			model->node.props.defaults = &src->node.props;
			model->node.props.props = props_begin;
			model->node.props.num_props = prop - props_begin;

			model->node.transform = ufbxi_get_transform(&model->node.props);
		}
	}

	size_t num_meshes = scene->meshes.size;
	for (size_t i = 0; i < num_meshes; i++) {
		ufbx_mesh *mesh = &imp->scene.meshes.data[i];
		ufbx_mesh *src = &scene->meshes.data[i];
		*mesh = *src;

		ufbxi_translate_node_refs(&child_refs, scene, &imp->scene, &mesh->node);
		*node++ = &mesh->node;

		{
			size_t num_materials = src->materials.size;
			ufbx_material **src_mat = src->materials.data;
			ufbx_material **dst_mat = material_refs;
			material_refs += num_materials;
			mesh->materials.data = dst_mat;
			for (size_t i = 0; i < num_materials; i++) {
				dst_mat[i] = ufbxi_translate_material(scene, &imp->scene, src_mat[i]);
			}
		}

		if (src->skins.size) {
			size_t num_skins = src->skins.size;
			ufbx_skin *src_skin = src->skins.data;
			ufbx_skin *dst_skin = skins;
			skins += num_skins;
			mesh->skins.data = dst_skin;
			for (size_t i = 0; i < num_skins; i++) {
				ufbxi_translate_skin(scene, &imp->scene, &dst_skin[i], &src_skin[i]);
			}
		}

		ufbx_prop *props_begin = prop;
		while (ap->target == UFBX_ANIM_MESH && ap->index == i) {
			ufbxi_evaluate_prop(prop, ap, time);
			prop++;
			ap++;
		}

		if (prop - props_begin > 0) {
			mesh->node.props.defaults = &src->node.props;
			mesh->node.props.props = props_begin;
			mesh->node.props.num_props = prop - props_begin;

			mesh->node.transform = ufbxi_get_transform(&mesh->node.props);
		}
	}

	size_t num_lights = scene->lights.size;
	for (size_t i = 0; i < num_lights; i++) {
		ufbx_light *light = &imp->scene.lights.data[i];
		ufbx_light *src = &scene->lights.data[i];
		*light = *src;

		ufbxi_translate_node_refs(&child_refs, scene, &imp->scene, &light->node);
		*node++ = &light->node;

		ufbx_prop *props_begin = prop;
		while (ap->target == UFBX_ANIM_LIGHT && ap->index == i) {
			ufbxi_evaluate_prop(prop, ap, time);
			prop++;
			ap++;
		}

		if (prop - props_begin > 0) {
			light->node.props.defaults = &src->node.props;
			light->node.props.props = props_begin;
			light->node.props.num_props = prop - props_begin;

			light->node.transform = ufbxi_get_transform(&light->node.props);
			ufbxi_get_light_properties(light);
		}
	}

	size_t num_bones = scene->bones.size;
	for (size_t i = 0; i < num_bones; i++) {
		ufbx_bone *bone = &imp->scene.bones.data[i];
		ufbx_bone *src = &scene->bones.data[i];
		*bone = *src;

		ufbxi_translate_node_refs(&child_refs, scene, &imp->scene, &bone->node);
		*node++ = &bone->node;

		ufbx_prop *props_begin = prop;
		while (ap->target == UFBX_ANIM_BONE && ap->index == i) {
			ufbxi_evaluate_prop(prop, ap, time);
			prop++;
			ap++;
		}

		if (prop - props_begin > 0) {
			bone->node.props.defaults = &src->node.props;
			bone->node.props.props = props_begin;
			bone->node.props.num_props = prop - props_begin;

			bone->node.transform = ufbxi_get_transform(&bone->node.props);
			ufbxi_get_bone_properties(bone);
		}
	}

	size_t num_materials = scene->materials.size;
	for (size_t i = 0; i < num_materials; i++) {
		ufbx_material *material = &imp->scene.materials.data[i];
		ufbx_material *src = &scene->materials.data[i];
		*material = *src;

		ufbx_prop *props_begin = prop;
		while (ap->target == UFBX_ANIM_MATERIAL && ap->index == i) {
			ufbxi_evaluate_prop(prop, ap, time);
			prop++;
			ap++;
		}

		if (prop - props_begin > 0) {
			material->props.defaults = &src->props;
			material->props.props = props_begin;
			material->props.num_props = prop - props_begin;

			ufbxi_get_material_properties(material);
		}
	}

	ufbx_assert(ap->target == UFBX_ANIM_INVALID);
	ufbx_assert(node == imp->scene.nodes.data + imp->scene.nodes.size);

	imp->scene.root = &imp->scene.models.data[0];

	ufbxi_update_transform_matrix(&imp->scene.root->node, NULL);

	if (opts.evaluate_skinned_vertices) {
		ufbxi_for(ufbx_mesh, mesh, imp->scene.meshes.data, imp->scene.meshes.size) {
			if (mesh->skins.size == 0) continue;

			ufbx_vec3 *positions = skinned_positions;
			ufbx_vec3 *normals = skinned_normals;
			int32_t *normal_indices = skinned_normal_indices;
			skinned_positions += mesh->num_vertices + 1;
			skinned_normals += mesh->num_indices + 1;
			skinned_normal_indices += mesh->num_indices;

			// Leave zero position/normal at index - 1
			memset(positions, 0, mesh->num_vertices * (sizeof(ufbx_vec3) + 1));
			memset(normals, 0, mesh->num_indices * (sizeof(ufbx_vec3) + 1));
			positions += 1;
			normals += 1;
			for (size_t i = 0 ; i < mesh->num_indices; i++) {
				normal_indices[i] = (int32_t)i;
			}

			memset(skinned_pos_total_weight, 0, mesh->num_vertices * sizeof(ufbx_real));

			ufbxi_for(ufbx_skin, skin, mesh->skins.data, mesh->skins.size) {
				ufbx_matrix mesh_to_root;
				ufbx_matrix_mul(&mesh_to_root, &skin->bone->to_root, &skin->mesh_to_bind);

				memset(skinned_pos_weights, 0, mesh->num_vertices * sizeof(ufbx_real));

				ufbx_vec3 *pos_src = mesh->vertex_position.data;
				ufbx_vec3 *pos_dst = positions;
				for (size_t i = 0; i < skin->num_weights; i++) {
					int32_t index = skin->indices[i];
					ufbx_real weight = skin->weights[i];
					skinned_pos_weights[index] = weight;
					skinned_pos_total_weight[index] += weight;
					ufbx_vec3 p = ufbx_transform_position(&mesh_to_root, pos_src[index]);
					pos_dst[index].x += p.x * weight;
					pos_dst[index].y += p.y * weight;
					pos_dst[index].z += p.z * weight;
				}

				ufbx_vec3 *normal_dst = normals;
				if (mesh->vertex_normal.data) {
					for (size_t i = 0; i < mesh->num_indices; i++) {
						ufbx_real weight = skinned_pos_weights[mesh->vertex_position.indices[i]];
						if (weight <= 0.0) continue;

						ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->vertex_normal, i);
						n = ufbx_transform_direction(&mesh_to_root, n);
						normal_dst[i].x += n.x * weight;
						normal_dst[i].y += n.y * weight;
						normal_dst[i].z += n.z * weight;
					}
				}
			}

			mesh->node.to_parent = ufbx_identity_matrix;
			mesh->node.to_root = ufbx_identity_matrix;

			for (size_t i = 0; i < mesh->num_vertices; i++) {
				ufbx_real rcp_weight = 1.0 / skinned_pos_total_weight[i];
				positions[i].x *= rcp_weight;
				positions[i].y *= rcp_weight;
				positions[i].z *= rcp_weight;
			}
			mesh->vertex_position.data = positions;

			if (mesh->vertex_normal.data) {
				for (size_t i = 0; i < mesh->num_indices; i++) {
					ufbx_vec3 n = normals[i];
					ufbx_real rcp_len = 1.0 / sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
					normals[i].x = n.x * rcp_len;
					normals[i].y = n.y * rcp_len;
					normals[i].z = n.z * rcp_len;
				}

				mesh->vertex_normal.data = normals;
				mesh->vertex_normal.indices = normal_indices;
			}
		}
	}

	return &imp->scene;
}

ufbxi_forceinline static void ufbxi_eval_anim_prop_imp(ufbx_anim_target target, uint32_t index, ufbx_anim_prop **p_ap, double time,
	ufbx_prop **p_prop, const char *name, size_t name_len, uint32_t imp_key)
{
	ufbx_anim_prop *ap = *p_ap;
	for (; ap->target == target && ap->index == index && ap->imp_key <= imp_key; ap++) {
		if (ap->name.data == name) {
			ufbx_prop *prop = (*p_prop)++;
			prop->name.data = name;
			prop->name.length = name_len;
			prop->imp_key = imp_key;
			prop->value_real_arr[0] = ufbx_evaluate_curve(&ap->curves[0], time);
			prop->value_real_arr[1] = ufbx_evaluate_curve(&ap->curves[1], time);
			prop->value_real_arr[2] = ufbx_evaluate_curve(&ap->curves[2], time);
			*p_ap = ap;
			return;
		}
	}

	*p_ap = ap;
}

#define ufbxi_eval_anim_prop(m_target, m_index, m_p_ap, m_time, m_prop, m_name) \
	ufbxi_eval_anim_prop_imp(m_target, m_index, m_p_ap, m_time, m_prop, m_name, sizeof(m_name) - 1, \
	(m_name[0] << 24) | (m_name[1] << 16) | (m_name[2] << 8) | m_name[3])

ufbx_inline ufbx_vec3 ufbxi_add3(ufbx_vec3 a, ufbx_vec3 b) {
	ufbx_vec3 v = { a.x + b.x, a.y + b.y, a.z + b.z };
	return v;
}

ufbx_inline ufbx_vec3 ufbxi_sub3(ufbx_vec3 a, ufbx_vec3 b) {
	ufbx_vec3 v = { a.x - b.x, a.y - b.y, a.z - b.z };
	return v;
}

ufbx_inline ufbx_vec3 ufbxi_mul3(ufbx_vec3 a, ufbx_real b) {
	ufbx_vec3 v = { a.x * b, a.y * b, a.z * b };
	return v;
}

ufbx_inline ufbx_real ufbxi_dot3(ufbx_vec3 a, ufbx_vec3 b) {
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

ufbx_inline ufbx_vec3 ufbxi_cross3(ufbx_vec3 a, ufbx_vec3 b) {
	ufbx_vec3 v = { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
	return v;
}

ufbx_inline ufbx_vec3 ufbxi_normalize(ufbx_vec3 a) {
	ufbx_real len = (ufbx_real)sqrt(ufbxi_dot3(a, a));
	if (len != 0.0) {
		return ufbxi_mul3(a, (ufbx_real)1.0 / len);
	} else {
		ufbx_vec3 zero = { (ufbx_real)0 };
		return zero;
	}
}

// -- API

#ifdef __cplusplus
extern "C" {
#endif

const ufbx_string ufbx_empty_string = { ufbxi_empty_char, 0 };
const ufbx_matrix ufbx_identity_matrix = { 1,0,0, 0,1,0, 0,0,1, 0,0,0 };
const ufbx_transform ufbx_identity_transform = { {0,0,0}, {0,0,0,1}, {1,1,1} };

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

ufbx_scene *ufbx_load_stdio(void *file, const ufbx_load_opts *opts, ufbx_error *error)
{
	ufbxi_context uc = { 0 };
	uc.read_fn = &ufbxi_file_read;
	uc.read_user = file;
	ufbx_scene *scene = ufbxi_load(&uc, opts, error);

	return scene;
}

void ufbx_free_scene(ufbx_scene *scene)
{
	if (!scene) return;

	ufbxi_scene_imp *imp = (ufbxi_scene_imp*)scene;
	ufbx_assert(imp->magic == UFBXI_SCENE_IMP_MAGIC);
	if (imp->magic != UFBXI_SCENE_IMP_MAGIC) return;

	ufbxi_buf_free(&imp->string_buf);

	char *memory_block = imp->memory_block;
	size_t memory_block_size = imp->memory_block_size;

	// We need to free `result_buf` last and be careful to copy it to
	// the stack since the `ufbxi_scene_imp` that contains it is allocated
	// from the same result buffer!
	ufbxi_allocator ator = imp->ator;
	ufbxi_buf result = imp->result_buf;
	result.ator = &ator;
	ufbxi_buf_free(&result);
	ufbxi_free(&ator, char, memory_block, memory_block_size);

	ufbx_assert(ator.current_size == 0);
}

ufbx_node *ufbx_find_node_len(const ufbx_scene *scene, const char *name, size_t name_len)
{
	ufbxi_for_ptr(ufbx_node, p_node, scene->nodes.data, scene->nodes.size) {
		ufbx_node *node = *p_node;
		if (node->name.length == name_len && !memcmp(node->name.data, name, name_len)) {
			return node;
		}
	}
	return NULL;
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

ufbx_material *ufbx_find_material_len(const ufbx_scene *scene, const char *name, size_t name_len)
{
	ufbxi_for(ufbx_material, material, scene->materials.data, scene->materials.size) {
		if (material->name.length == name_len && !memcmp(material->name.data, name, name_len)) {
			return material;
		}
	}
	return NULL;
}

ufbx_light *ufbx_find_light_len(const ufbx_scene *scene, const char *name, size_t name_len)
{
	ufbxi_for(ufbx_light, light, scene->lights.data, scene->lights.size) {
		if (light->node.name.length == name_len && !memcmp(light->node.name.data, name, name_len)) {
			return light;
		}
	}
	return NULL;
}

ufbx_anim_stack *ufbx_find_anim_stack_len(const ufbx_scene *scene, const char *name, size_t name_len)
{
	ufbxi_for(ufbx_anim_stack, stack, scene->anim_stacks.data, scene->anim_stacks.size) {
		if (stack->name.length == name_len && !memcmp(stack->name.data, name, name_len)) {
			return stack;
		}
	}
	return NULL;
}

ufbx_anim_layer *ufbx_find_anim_layer_len(const ufbx_scene *scene, const char *name, size_t name_len)
{
	ufbxi_for(ufbx_anim_layer, layer, scene->anim_layers.data, scene->anim_layers.size) {
		if (layer->name.length == name_len && !memcmp(layer->name.data, name, name_len)) {
			return layer;
		}
	}
	return NULL;
}

ufbx_prop *ufbx_find_prop_len(const ufbx_props *props, const char *name, size_t name_len)
{
	uint32_t key = ufbxi_get_name_key(name, name_len);

	do {
		ufbx_prop *prop_data = props->props;
		size_t begin = 0;
		size_t end = props->num_props;
		while (end - begin >= 16) {
			size_t mid = (begin + end) >> 1;
			const ufbx_prop *p = &prop_data[mid];
			if (p->imp_key < key) {
				begin = mid + 1;
			} else { 
				end = mid;
			}
		}

		end = props->num_props;
		for (; begin < end; begin++) {
			const ufbx_prop *p = &prop_data[begin];
			if (p->imp_key > key) break;
			if (p->imp_key == key && p->name.length == name_len && !memcmp(p->name.data, name, name_len)) {
				return (ufbx_prop*)p;
			}
		}

		props = props->defaults;
	} while (props);

	return NULL;
}

ufbx_anim_prop *ufbx_find_node_anim_prop_begin(const ufbx_scene *scene, const ufbx_anim_layer *layer, const ufbx_node *node)
{
	if (!scene || !layer || !node) return NULL;

	ufbx_anim_target target = UFBX_ANIM_UNKNOWN;
	uint32_t index = ~0u;
	switch (node->type) {
	default: return NULL;
	case UFBX_NODE_MODEL:
		target = UFBX_ANIM_MODEL;
		index = (uint32_t)((ufbx_model*)node - scene->models.data);
		break;
	case UFBX_NODE_MESH:
		target = UFBX_ANIM_MESH;
		index = (uint32_t)((ufbx_mesh*)node - scene->meshes.data);
		break;
	case UFBX_NODE_LIGHT:
		target = UFBX_ANIM_LIGHT;
		index = (uint32_t)((ufbx_light*)node - scene->lights.data);
		break;
	case UFBX_NODE_BONE:
		target = UFBX_ANIM_BONE;
		index = (uint32_t)((ufbx_bone*)node - scene->bones.data);
		break;
	}

	size_t begin = 0;
	size_t end = layer->props.size;
	const ufbx_anim_prop *props = layer->props.data;
	while (end - begin >= 16) {
		size_t mid = (begin + end) >> 1;
		if (ufbxi_cmp_anim_prop_imp(&props[mid], target, index) < 0) {
			begin = mid + 1;
		} else { 
			end = mid;
		}
	}

	end = layer->props.size;
	for (; begin < end; begin++) {
		const ufbx_anim_prop *prop = &props[begin];
		if (prop->target == target && prop->index == index) return (ufbx_anim_prop*)prop;
	}

	return NULL;
}

ufbx_face *ufbx_find_face(const ufbx_mesh *mesh, size_t index)
{
	size_t range_end = mesh->num_faces;
	size_t begin = 0, end = range_end;

	for (;;) {
		const ufbx_face *faces = mesh->faces;
		while (end - begin >= 16) {
			size_t mid = (begin + end) >> 1;
			const ufbx_face *f = &faces[mid];
			if (f->index_begin + f->num_indices < index) {
				begin = mid + 1;
			} else { 
				end = mid;
			}
		}

		end = range_end;
		for (; begin < end; begin++) {
			const ufbx_face *f = &faces[begin];
			if (index - f->index_begin < f->num_indices) return (ufbx_face*)f;
		}

		// Check bad faces if not found in regular ones
		size_t total_faces = mesh->num_faces + mesh->num_bad_faces;
		if (range_end == total_faces) return NULL;
		begin = mesh->num_faces;
		range_end = end = total_faces;
	}
}

ufbx_matrix ufbx_get_transform_matrix(const ufbx_transform *t)
{
	ufbx_vec4 q = t->rotation;
	ufbx_real sx = 2.0 * t->scale.x, sy = 2.0 * t->scale.y, sz = 2.0 * t->scale.z;
	ufbx_real xx = q.x*q.x, xy = q.x*q.y, xz = q.x*q.z, xw = q.x*q.w;
	ufbx_real yy = q.y*q.y, yz = q.y*q.z, yw = q.y*q.w;
	ufbx_real zz = q.z*q.z, zw = q.z*q.w;
	ufbx_matrix m;
	m.m00 = sx * (- yy - zz + 0.5);
	m.m10 = sx * (+ xy + zw);
	m.m20 = sx * (- yw + xz);
	m.m01 = sy * (- zw + xy);
	m.m11 = sy * (- xx - zz + 0.5);
	m.m21 = sy * (+ xw + yz);
	m.m02 = sz * (+ xz + yw);
	m.m12 = sz * (- xw + yz);
	m.m22 = sz * (- xx - yy + 0.5);
	m.m03 = t->translation.x;
	m.m13 = t->translation.y;
	m.m23 = t->translation.z;
	return m;
}

void ufbx_matrix_mul(ufbx_matrix *dst, const ufbx_matrix *p_l, const ufbx_matrix *p_r)
{
	ufbx_matrix l = *p_l;
	ufbx_matrix r = *p_r;

	dst->m03 = l.m00*r.m03 + l.m01*r.m13 + l.m02*r.m23 + l.m03;
	dst->m13 = l.m10*r.m03 + l.m11*r.m13 + l.m12*r.m23 + l.m13;
	dst->m23 = l.m20*r.m03 + l.m21*r.m13 + l.m22*r.m23 + l.m23;

	dst->m00 = l.m00*r.m00 + l.m01*r.m10 + l.m02*r.m20;
	dst->m10 = l.m10*r.m00 + l.m11*r.m10 + l.m12*r.m20;
	dst->m20 = l.m20*r.m00 + l.m21*r.m10 + l.m22*r.m20;

	dst->m01 = l.m00*r.m01 + l.m01*r.m11 + l.m02*r.m21;
	dst->m11 = l.m10*r.m01 + l.m11*r.m11 + l.m12*r.m21;
	dst->m21 = l.m20*r.m01 + l.m21*r.m11 + l.m22*r.m21;

	dst->m02 = l.m00*r.m02 + l.m01*r.m12 + l.m02*r.m22;
	dst->m12 = l.m10*r.m02 + l.m11*r.m12 + l.m12*r.m22;
	dst->m22 = l.m20*r.m02 + l.m21*r.m12 + l.m22*r.m22;
}

ufbx_vec3 ufbx_transform_position(const ufbx_matrix *m, ufbx_vec3 v)
{
	ufbx_vec3 r;
	r.x = m->m00*v.x + m->m01*v.y + m->m02*v.z + m->m03;
	r.y = m->m10*v.x + m->m11*v.y + m->m12*v.z + m->m13;
	r.z = m->m20*v.x + m->m21*v.y + m->m22*v.z + m->m23;
	return r;
}

ufbx_vec3 ufbx_transform_direction(const ufbx_matrix *m, ufbx_vec3 v)
{
	ufbx_vec3 r;
	r.x = m->m00*v.x + m->m01*v.y + m->m02*v.z;
	r.y = m->m10*v.x + m->m11*v.y + m->m12*v.z;
	r.z = m->m20*v.x + m->m21*v.y + m->m22*v.z;
	return r;
}

ufbx_matrix ufbx_get_normal_matrix(const ufbx_matrix *m)
{
	ufbx_real det = 
		- m->m02*m->m11*m->m20 + m->m01*m->m12*m->m20 + m->m02*m->m10*m->m21
		- m->m00*m->m12*m->m21 - m->m01*m->m10*m->m22 + m->m00*m->m11*m->m22;

	ufbx_matrix r;
	if (det == 0.0) {
		memset(&r, 0, sizeof(r));
		return r;
	}

	ufbx_real rcp_det = 1.0 / det;

	r.m00 = ( - m->m12*m->m21 + m->m11*m->m22) * rcp_det;
	r.m01 = ( + m->m12*m->m20 - m->m10*m->m22) * rcp_det;
	r.m02 = ( - m->m11*m->m20 + m->m10*m->m21) * rcp_det;
	r.m10 = ( + m->m02*m->m21 - m->m01*m->m22) * rcp_det;
	r.m11 = ( - m->m02*m->m20 + m->m00*m->m22) * rcp_det;
	r.m12 = ( + m->m01*m->m20 - m->m00*m->m21) * rcp_det;
	r.m20 = ( - m->m02*m->m11 + m->m01*m->m12) * rcp_det;
	r.m21 = ( + m->m02*m->m10 - m->m00*m->m12) * rcp_det;
	r.m22 = ( - m->m01*m->m10 + m->m00*m->m11) * rcp_det;
	r.m03 = r.m13 = r.m23 = 0.0;

	return r;
}

ufbx_matrix ufbx_get_inverse_matrix(const ufbx_matrix *m)
{
	ufbx_real det = 
		- m->m02*m->m11*m->m20 + m->m01*m->m12*m->m20 + m->m02*m->m10*m->m21
		- m->m00*m->m12*m->m21 - m->m01*m->m10*m->m22 + m->m00*m->m11*m->m22;

	ufbx_matrix r;
	if (det == 0.0) {
		memset(&r, 0, sizeof(r));
		return r;
	}

	ufbx_real rcp_det = 1.0 / det;

	r.m00 = ( - m->m12*m->m21 + m->m11*m->m22) * rcp_det;
	r.m10 = ( + m->m12*m->m20 - m->m10*m->m22) * rcp_det;
	r.m20 = ( - m->m11*m->m20 + m->m10*m->m21) * rcp_det;
	r.m01 = ( + m->m02*m->m21 - m->m01*m->m22) * rcp_det;
	r.m11 = ( - m->m02*m->m20 + m->m00*m->m22) * rcp_det;
	r.m21 = ( + m->m01*m->m20 - m->m00*m->m21) * rcp_det;
	r.m02 = ( - m->m02*m->m11 + m->m01*m->m12) * rcp_det;
	r.m12 = ( + m->m02*m->m10 - m->m00*m->m12) * rcp_det;
	r.m22 = ( - m->m01*m->m10 + m->m00*m->m11) * rcp_det;
	r.m03 = (m->m03*m->m12*m->m21 - m->m02*m->m13*m->m21 - m->m03*m->m11*m->m22 + m->m01*m->m13*m->m22 + m->m02*m->m11*m->m23 - m->m01*m->m12*m->m23) * rcp_det;
	r.m13 = (m->m02*m->m13*m->m20 - m->m03*m->m12*m->m20 + m->m03*m->m10*m->m22 - m->m00*m->m13*m->m22 - m->m02*m->m10*m->m23 + m->m00*m->m12*m->m23) * rcp_det;
	r.m23 = (m->m03*m->m11*m->m20 - m->m01*m->m13*m->m20 - m->m03*m->m10*m->m21 + m->m00*m->m13*m->m21 + m->m01*m->m10*m->m23 - m->m00*m->m11*m->m23) * rcp_det;

	return r;
}

ufbx_real ufbx_evaluate_curve(const ufbx_anim_curve *curve, double time)
{
	if (curve->keyframes.size <= 1) {
		if (curve->keyframes.size == 1) {
			return curve->keyframes.data[0].value;
		} else {
			return curve->default_value;
		}
	}

	size_t begin = 0;
	size_t end = curve->keyframes.size;
	const ufbx_keyframe *keys = curve->keyframes.data;
	while (end - begin >= 8) {
		size_t mid = (begin + end) >> 1;
		if (keys[mid].time < time) {
			begin = mid + 1;
		} else { 
			end = mid;
		}
	}

	end = curve->keyframes.size;
	for (; begin < end; begin++) {
		const ufbx_keyframe *next = &keys[begin];
		if (next->time < time) continue;

		// First keyframe
		if (begin == 0) return next->value;

		const ufbx_keyframe *prev = next - 1;

		double rcp_delta = 1.0 / (next->time - prev->time);
		double t = (time - prev->time) * rcp_delta;

		switch (prev->interpolation) {

		case UFBX_INTERPOLATION_CONSTANT_PREV:
			return prev->value;

		case UFBX_INTERPOLATION_CONSTANT_NEXT:
			return next->value;

		case UFBX_INTERPOLATION_LINEAR:
			return prev->value*(1.0 - t) + next->value*t;

		case UFBX_INTERPOLATION_CUBIC:
		{
			double x1 = prev->right.dx * rcp_delta;
			double x2 = 1.0 - next->left.dx * rcp_delta;
			t = ufbxi_find_cubic_bezier_t(x1, x2, t);

			double t2 = t*t, t3 = t2*t;
			double u = 1.0 - t, u2 = u*u, u3 = u2*u;

			double y0 = prev->value;
			double y3 = next->value;
			double y1 = y0 + prev->right.dy;
			double y2 = y3 - next->left.dy;

			return u3*y0 + 3.0 * (u2*t*y1 + u*t2*y2) + t3*y3;
		}

		}
	}

	// Last keyframe
	return curve->keyframes.data[curve->keyframes.size - 1].value;
}

ufbx_transform ufbx_evaluate_transform(const ufbx_scene *scene, const ufbx_node *node, const ufbx_anim_stack *stack, double time)
{
	const ufbx_anim_layer *layer = NULL;
	if (!stack && scene->anim_stacks.size > 0) {
		stack = &scene->anim_stacks.data[0];
	}
	if (stack && stack->layers.size > 0) {
		layer = stack->layers.data[0];
	}
	ufbx_anim_prop *ap = ufbx_find_node_anim_prop_begin(scene, layer, node);
	if (!ap) return node->transform;
	ufbx_anim_target target = ap->target;
	uint32_t index = ap->index;

	ufbx_prop props[10];

	ufbx_prop *pp = props;
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_Lcl_Rotation);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_Lcl_Scaling);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_Lcl_Translation);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_PostRotation);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_PreRotation);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_RotationOffset);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_RotationOrder);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_RotationPivot);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_ScalingOffset);
	ufbxi_eval_anim_prop(target, index, &ap, time, &pp, ufbxi_ScalingPivot);

	ufbx_props eval_props;
	eval_props.props = props;
	eval_props.num_props = pp - props;
	eval_props.defaults = (ufbx_props*)&node->props;
	return ufbxi_get_transform(&eval_props);
}

ufbx_scene *ufbx_evaluate_scene(const ufbx_scene *scene, const ufbx_evaluate_opts *user_opts, double time)
{
	return ufbxi_evaluate_scene(scene, user_opts, time);
}

ufbx_vec3 ufbx_rotate_vector(ufbx_vec4 q, ufbx_vec3 v)
{
	ufbx_real xy = q.x*v.y - q.y*v.x;
	ufbx_real xz = q.x*v.z - q.z*v.x;
	ufbx_real yz = q.y*v.z - q.z*v.y;
	ufbx_vec3 r;
	r.x = 2.0 * (+ q.w*yz + q.y*xy + q.z*xz) + v.x;
	r.y = 2.0 * (- q.x*xy - q.w*xz + q.z*yz) + v.y;
	r.z = 2.0 * (- q.x*xz - q.y*yz + q.w*xy) + v.z;
	return r;
}

bool ufbx_triangulate(uint32_t *indices, size_t num_indices, ufbx_mesh *mesh, ufbx_face face)
{
	if (face.num_indices < 3 || num_indices < ((size_t)face.num_indices - 2) * 3) return false;

	if (face.num_indices == 3) {
		// Fast case: Already a triangle
		indices[0] = face.index_begin + 0;
		indices[1] = face.index_begin + 1;
		indices[2] = face.index_begin + 2;
		return true;
	} else if (face.num_indices == 4) {
		// Quad: Split along the shortest axis unless a vertex crosses the axis
		uint32_t i0 = face.index_begin + 0;
		uint32_t i1 = face.index_begin + 1;
		uint32_t i2 = face.index_begin + 2;
		uint32_t i3 = face.index_begin + 3;
		ufbx_vec3 v0 = mesh->vertex_position.data[mesh->vertex_position.indices[i0]];
		ufbx_vec3 v1 = mesh->vertex_position.data[mesh->vertex_position.indices[i1]];
		ufbx_vec3 v2 = mesh->vertex_position.data[mesh->vertex_position.indices[i2]];
		ufbx_vec3 v3 = mesh->vertex_position.data[mesh->vertex_position.indices[i3]];

		ufbx_vec3 a = ufbxi_sub3(v2, v0);
		ufbx_vec3 b = ufbxi_sub3(v3, v1);

		ufbx_vec3 na1 = ufbxi_normalize(ufbxi_cross3(a, ufbxi_sub3(v1, v0)));
		ufbx_vec3 na3 = ufbxi_normalize(ufbxi_cross3(a, ufbxi_sub3(v0, v3)));
		ufbx_vec3 nb0 = ufbxi_normalize(ufbxi_cross3(b, ufbxi_sub3(v1, v0)));
		ufbx_vec3 nb2 = ufbxi_normalize(ufbxi_cross3(b, ufbxi_sub3(v2, v1)));

		ufbx_real dot_aa = ufbxi_dot3(a, a);
		ufbx_real dot_bb = ufbxi_dot3(b, b);
		ufbx_real dot_na = ufbxi_dot3(na1, na3);
		ufbx_real dot_nb = ufbxi_dot3(nb0, nb2);

		bool split_a = dot_aa <= dot_bb;

		if (dot_na < 0.0f || dot_nb < 0.0f) {
			split_a = dot_na >= dot_nb;
		}

		if (split_a) {
			indices[0] = i0;
			indices[1] = i1;
			indices[2] = i2;
			indices[3] = i2;
			indices[4] = i3;
			indices[5] = i0;
		} else {
			indices[0] = i1;
			indices[1] = i2;
			indices[2] = i3;
			indices[3] = i3;
			indices[4] = i0;
			indices[5] = i1;
		}
	} else {
		// N-Gon: TODO something reasonable
		uint32_t *dst = indices;
		for (uint32_t i = 1; i + 2 <= face.num_indices; i++) {
			dst[0] = face.index_begin;
			dst[1] = face.index_begin + i;
			dst[2] = face.index_begin + i + 1;
			dst += 3;
		}
	}

	return true;
}

#ifdef __cplusplus
}
#endif

#endif
