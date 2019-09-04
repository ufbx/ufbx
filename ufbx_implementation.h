#ifndef UFBX_UFBX_H_IMPLEMENTED
#define UFBX_UFBX_H_IMPLEMENTED

#include "ufbx.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


// -- Platform

#if defined(_MSC_VER)
	#define ufbxi_noinline __declspec(noinline)
	#define ufbxi_forceinline __forceinline
#elif defined(__GNUC__) || defined(__clang__)
	#define ufbxi_noinline __attribute__((noinline))
	#define ufbxi_forceinline inline __attribute__((always_inline))
#else
	#define ufbxi_noinline
	#define ufbxi_forceinline
#endif

#define ufbxi_arraycount(arr) (sizeof(arr) / sizeof(*(arr)))

// -- Utility

typedef struct {
	const char *data;
	size_t length;
} ufbxi_string;

static ufbxi_forceinline int
ufbxi_streq(ufbxi_string str, const char *ref)
{
	size_t length = strlen(ref);
	return str.length == length && !memcmp(str.data, ref, length);
}

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
#define ufbxi_write_i64(ptr, val) ufbxi_write_u64(ptr, val)

// -- Binary parsing

typedef struct {
	ufbxi_string name;
	size_t value_begin_pos;
	size_t child_begin_pos;
	size_t end_pos;

	size_t next_value_pos;
	size_t next_child_pos;
} ufbxi_node;

#define UFBXI_NODE_STACK_SIZE 16

typedef struct {
	const char *data;
	size_t size;

	uint32_t version;
	int from_ascii;

	// Currently focused (via find or iteration) node.
	ufbxi_node focused_node;

	// Entered node stack.
	ufbxi_node node_stack[UFBXI_NODE_STACK_SIZE];
	ufbxi_node *node_stack_top;

	// Error status
	int failed;
	ufbx_error *error;
} ufbxi_context;

typedef enum {
	ufbxi_val_int,    // < I,L,Y,C
	ufbxi_val_float,  // < F,D
	ufbxi_val_string, // < S
	ufbxi_val_array,  // < i,l,f,d,b
} ufbxi_val_class;

typedef enum {
	ufbxi_encoding_basic = 0,
	ufbxi_encoding_deflate = 1,

	// Custom array encoding for older FBX compatability. Concatenates multiple
	// individual values into a single array. Magic number 'UFmv' (little endian)
	// chosen to minimize risk of colliding with other custom encodings.
	ufbxi_encoding_multivalue = 0x766d4655,
} ufbxi_array_encoding;

typedef struct {
	size_t num_elements;
	ufbxi_array_encoding encoding;  
	size_t encoded_size;
	size_t data_offset;
	char src_type;
	char dst_type;
} ufbxi_array;

typedef struct {
	char type_code;
	ufbxi_val_class value_class;
	union {
		uint64_t i;
		double f;
		ufbxi_string str;
		ufbxi_array arr;
	} value;
} ufbxi_any_value;

static int ufbxi_do_error(ufbxi_context *uc, uint32_t line, const char *desc)
{
	if (uc->failed) return 0;
	uc->failed = 1;
	size_t length = strlen(desc);
	if (length > UFBX_ERROR_DESC_MAX_LENGTH) length = UFBX_ERROR_DESC_MAX_LENGTH;
	if (uc->error) {
		uc->error->source_line = line;
		memcpy(uc->error->desc, desc, length);
		uc->error->desc[length] = '\0';
	}
	return 0;
}

static int ufbxi_do_errorf(ufbxi_context *uc, uint32_t line, const char *fmt, ...)
{
	if (uc->failed) return 0;
	uc->failed = 1;
	va_list args;
	va_start(args, fmt);
	if (uc->error) {
		uc->error->source_line = line;
		vsnprintf(uc->error->desc, sizeof(uc->error->desc), fmt, args);
	}
	va_end(args);
	return 0;
}

#define ufbxi_error(uc, desc) ufbxi_do_error((uc), __LINE__, (desc))
#define ufbxi_errorf(uc, fmt, ...) ufbxi_do_errorf((uc), __LINE__, (fmt), __VA_ARGS__)

// Prepare `dst` as an multivalue encoded array. Does not parse the data yet,
// only counts the number of elements, checks types, and skips the array data.
static int ufbxi_convert_multivalue_array(ufbxi_context *uc, ufbxi_array *dst, char dst_type)
{
	size_t begin = uc->focused_node.next_value_pos;
	uint32_t num_elements = 0;
	while (uc->focused_node.next_value_pos < uc->focused_node.child_begin_pos) {
		char src_type = uc->data[uc->focused_node.next_value_pos];
		uint32_t size;
		switch (src_type) {
		case 'I': size = 4; break;
		case 'L': size = 8; break;
		case 'Y': size = 2; break;
		case 'C': size = 1; break;
		case 'F': size = 4; break;
		case 'D': size = 8; break;
		default: return ufbxi_errorf(uc, "Bad multivalue array type '%c'", src_type);
		}
		uc->focused_node.next_value_pos += 1 + size;
		num_elements++;
	}
	if (uc->focused_node.next_value_pos != uc->focused_node.child_begin_pos) {
		return ufbxi_error(uc, "Multivalue array overrun");
	}

	dst->num_elements = num_elements;
	dst->data_offset = begin;
	dst->encoding = ufbxi_encoding_multivalue;
	dst->encoded_size = uc->focused_node.next_value_pos - begin;
	// NOTE: Multivalue arrays can be heterogenous, just use `dst_type` as `src_type`
	dst->src_type = dst_type; 
	dst->dst_type = dst_type;
	return 1;
}
// Parse the next node value in the input stream.
// `dst_type` is a FBX-like type string:
//   Numbers: "I" u/int32_t  "L" u/int64_t  "B" char (bool)  "F" float  "D" double
//   Data: "SR" ufbxi_string  "ilfdb" ufbxi_val_array (matching upper-case type)
//   Misc: "." (ignore)  "?" ufbxi_any_value
// Performs implicit conversions:
//   FDILY -> FD, ILYC -> ILB, ILFDYilfd -> fd, ILYCilb -> ilb
static int ufbxi_parse_value(ufbxi_context *uc, char dst_type, void *dst)
{
	// An FBX file must end in a 13 or 25-byte NULL node. Due to this a valid
	// FBX file must always have space for the largest possible property header.
	if (uc->size - uc->focused_node.next_value_pos < 13) {
		return ufbxi_error(uc, "Reading value at the end of file");
	}

	size_t pos = uc->focused_node.next_value_pos;
	char src_type = uc->data[pos];
	const char *src = uc->data + pos + 1;

	// Read the next value locally
	size_t val_size = 1;
	ufbxi_val_class val_class;
	int64_t val_int;
	double val_float;
	size_t val_num_elements;
	uint32_t val_encoding;
	size_t val_encoded_size;
	size_t val_data_offset;
	switch (src_type) {
	case 'I': val_class = ufbxi_val_int; val_int = ufbxi_read_i32(src); val_size += 4; break;
	case 'L': val_class = ufbxi_val_int; val_int = ufbxi_read_i64(src); val_size += 8; break;
	case 'Y': val_class = ufbxi_val_int; val_int = ufbxi_read_i16(src); val_size += 2; break;
	case 'C': val_class = ufbxi_val_int; val_int = (ufbxi_read_u8(src) ? 1 : 0); val_size += 1; break;
	case 'F': val_class = ufbxi_val_float; val_float = ufbxi_read_f32(src); val_size += 4; break;
	case 'D': val_class = ufbxi_val_float; val_float = ufbxi_read_f64(src); val_size += 8; break;
	case 'S': case 'R':
		val_class = ufbxi_val_string;
		val_num_elements = ufbxi_read_u32(src);
		if (val_num_elements > 0x1000000) {
			return ufbxi_errorf(uc, "String is too large: %u bytes", val_num_elements);
		}
		val_size += 4 + val_num_elements;
		val_data_offset = pos + 5;
		break;
	case 'i': case 'l': case 'f': case 'd': case 'b':
		val_class = ufbxi_val_array;
		val_num_elements = ufbxi_read_u32(src + 0);
		val_encoding = ufbxi_read_u32(src + 4);
		val_encoded_size = ufbxi_read_u32(src + 8);
		if (val_encoded_size > 0x1000000) {
			return ufbxi_errorf(uc, "Array is too large: %u bytes", val_encoded_size);
		}
		val_size += 12 + val_encoded_size;
		val_data_offset = pos + 13;
		break;
	default:
		return ufbxi_errorf(uc, "Invalid type code '%c'", src_type);
	}

	size_t val_end = pos + val_size;
	if (val_end < pos || val_end > uc->focused_node.child_begin_pos) {
		return ufbxi_errorf(uc, "Value overflows data block: %u bytes", val_size);
	}
	uc->focused_node.next_value_pos = val_end;

	// Early return: Ignore the data
	if (dst_type == '.') return 1;
	ufbxi_any_value *any = NULL;
	if (dst_type == '?') {
		any = (ufbxi_any_value*)dst;
		any->type_code = src_type;
		any->value_class = val_class;
	}

	// Interpret the read value into the user pointer, potentially applying the
	// implicit conversion rules.
	if (val_class == ufbxi_val_int) {
		switch (dst_type) {
		case 'I': *(int32_t*)dst = (int32_t)val_int; break;
		case 'L': *(int64_t*)dst = val_int; break;
		case 'B': *(char*)dst = val_int ? 1 : 0; break;
		case 'F': *(float*)dst = (float)val_int; break;
		case 'D': *(double*)dst = (double)val_int; break;
		case '?': any->value.i = val_int; break;
		case 'i': case 'l': case 'f': case 'd': case 'b':
			// Early return: Parse as multivalue array. Reset position to beginning and rescan.
			uc->focused_node.next_value_pos = pos;
			return ufbxi_convert_multivalue_array(uc, (ufbxi_array*)dst, dst_type);
		default:
			return ufbxi_errorf(uc, "Cannot convert from int '%c' to '%c'", src_type, dst_type);
		}

	} else if (val_class == ufbxi_val_float) {
		switch (dst_type) {
		case 'F': *(float*)dst = (float)val_float; break;
		case 'D': *(double*)dst = val_float; break;
		case '?': any->value.f = val_float; break;
		case 'f': case 'd':
			// Early return: Parse as multivalue array. Reset position to beginning and rescan.
			uc->focused_node.next_value_pos = pos;
			return ufbxi_convert_multivalue_array(uc, (ufbxi_array*)dst, dst_type);
		default:
			return ufbxi_errorf(uc, "Cannot convert from float '%c' to '%c'", src_type, dst_type);
		}

	} else if (val_class == ufbxi_val_string) {
		ufbxi_string *str;
		switch (dst_type) {
		case 'S': str = (ufbxi_string*)dst; break;
		case '?': str = &any->value.str; break;
		default:
			return ufbxi_errorf(uc, "Cannot convert from string '%c' to '%c'", src_type, dst_type);
		}

		str->data = uc->data + val_data_offset;
		str->length = val_num_elements;
	} else if (val_class == ufbxi_val_array) {
		ufbxi_array *arr;
		switch (dst_type) {
		case 'i': case 'l': case 'f': case 'd': case 'b':
			arr = (ufbxi_array*)dst;
			arr->dst_type = dst_type;
			break;
		case '?':
			arr = &any->value.arr;
			arr->dst_type = src_type;
			break;
		default:
			return ufbxi_errorf(uc, "Cannot convert from array '%c' to '%c'", src_type, dst_type);
		}

		arr->src_type = src_type;
		arr->num_elements = val_num_elements;
		arr->encoding = (ufbxi_array_encoding)val_encoding;
		arr->encoded_size = val_encoded_size;
		arr->data_offset = val_data_offset;
		if (val_encoding != ufbxi_encoding_basic && val_encoding != ufbxi_encoding_deflate) {
			return ufbxi_errorf(uc, "Unknown array encoding: %u", val_encoding);
		}
	}

	return 1;
}

// VA-list version of `ufbxi_parse_values()`
static int ufbxi_parse_values_va(ufbxi_context *uc, const char *fmt, va_list args)
{
	const char *fmt_ptr = fmt;
	char ch;
	while ((ch = *fmt_ptr) != '\0') {
		void *dst = NULL;
		if (ch != '.') dst = va_arg(args, void*);
		if (!ufbxi_parse_value(uc, ch, dst)) {
			va_end(args);
			return 0;
		}
		fmt_ptr++;
	}
	return 1;
}

// Parse multiple values, works like `scanf()`.
// See `ufbxi_parse_value()` for more information.
static int ufbxi_parse_values(ufbxi_context *uc, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = ufbxi_parse_values_va(uc, fmt, args);
	va_end(args);
	return ret;
}

// Decode the data of an multivalue array into memory. Requires the array to be
// prepared with `ufbxi_convert_multivalue_array()`.
static int ufbxi_decode_multivalue_array(ufbxi_context *uc, ufbxi_array *arr, void *dst)
{
	if (arr->encoding != ufbxi_encoding_multivalue) {
		return ufbxi_error(uc, "Internal: Bad multivalue encoding");
	}

	// HACK: Create a virtual node for the multivalue array and set it as
	// the current one.
	ufbxi_node focused_node = uc->focused_node;
	memset(&uc->focused_node, 0, sizeof(ufbxi_node));

	size_t arr_begin = arr->data_offset;
	size_t arr_end = arr_begin + arr->encoded_size;
	uc->focused_node.value_begin_pos = arr_begin;
	uc->focused_node.next_value_pos = arr_begin;
	uc->focused_node.child_begin_pos = arr_end;
	uc->focused_node.next_child_pos = arr_end;
	uc->focused_node.end_pos = arr_end;

	char dst_elem_type;
	uint32_t dst_elem_size;
	switch (arr->dst_type) {
	case 'i': dst_elem_type = 'I'; dst_elem_size = 4; break;
	case 'l': dst_elem_type = 'L'; dst_elem_size = 8; break;
	case 'f': dst_elem_type = 'F'; dst_elem_size = 4; break;
	case 'd': dst_elem_type = 'D'; dst_elem_size = 8; break;
	case 'b': dst_elem_type = 'B'; dst_elem_size = 1; break;
	}

	char *dst_ptr = (char*)dst;
	while (uc->focused_node.next_value_pos < uc->focused_node.child_begin_pos) {
		if (!ufbxi_parse_value(uc, dst_elem_type, dst_ptr)) return 0;
		dst_ptr += dst_elem_size;
	}
	if (uc->focused_node.next_value_pos != uc->focused_node.child_begin_pos) {
		return ufbxi_error(uc, "Multivalue array overrun");
	}
	if (dst_ptr != (char*)dst + arr->num_elements * dst_elem_size) {
		return ufbxi_error(uc, "Internal: Multivalue array read failed");
	}

	uc->focused_node = focused_node;
	return 1;
}

// Decode the contents of an array into memory. Performs potential type conversion
// and decoding of the data. Should not fail for a non-decoding failure reason.
static int ufbxi_decode_array(ufbxi_context *uc, ufbxi_array *arr, void *dst)
{
	uint32_t elem_size;
	switch (arr->dst_type) {
	case 'i': case 'f': elem_size = 4; break;
	case 'l': case 'd': elem_size = 8; break;
	case 'b': elem_size = 1; break;
	default: return ufbxi_error(uc, "Internal: Invalid array type");
	}

	size_t arr_size = arr->num_elements * elem_size;

	if (arr->dst_type == arr->src_type) {
		const void *src = uc->data + arr->data_offset;

		// Fast path: Source is memory-compatible with destination, decode directly to
		// the destination memory.
		switch (arr->encoding) {
		case ufbxi_encoding_basic:
			if (arr->encoded_size != arr_size) {
				return ufbxi_errorf(uc, "Array size mismatch, encoded %u bytes, decoded %u bytes",
					arr->encoded_size, arr_size);
			}
			memcpy(dst, src, arr_size);
			break;
		case ufbxi_encoding_deflate:
			return ufbxi_error(uc, "Internal: Unimplemented.");
		case ufbxi_encoding_multivalue:
			// Early return: Defer to multivalue implementation.
			return ufbxi_decode_multivalue_array(uc, arr, dst);
		default: return ufbxi_error(uc, "Internal: Bad array encoding");
		}
	} else {
		const char *src_ptr;

		// Slow path: Need to do conversion, allocate temporary buffer if necessary
		switch (arr->encoding) {
		case ufbxi_encoding_basic:
			src_ptr = uc->data + arr->data_offset;
			break;
		case ufbxi_encoding_deflate:
			return ufbxi_error(uc, "Internal: Unimplemented.");
			break;
		case ufbxi_encoding_multivalue:
			// Multivalue arrays should always have `src_type == dst_type`.
			return ufbxi_error(uc, "Internal: Multivalue array has invalid type");
		default: return ufbxi_error(uc, "Internal: Bad array encoding");
		}

		uint32_t src_elem_size;
		switch (arr->src_type) {
		case 'i': case 'f': src_elem_size = 4; break;
		case 'l': case 'd': src_elem_size = 8; break;
		case 'b': src_elem_size = 1; break;
		default: return ufbxi_error(uc, "Internal: Invalid array type");
		}

		if (arr->num_elements * src_elem_size != arr->encoded_size) {
			return ufbxi_errorf(uc, "Array size mismatch, encoded %u bytes, decoded %u bytes",
				arr->encoded_size, arr->num_elements * src_elem_size);
		}
		const char *src_end = src_ptr + arr->encoded_size;

		// Try to perform type conversion.
		const char *sp = src_ptr, *ep = src_end;
		int failed = 0;
		if (arr->dst_type == 'i') {
			int32_t *dp = (int32_t*)dst;
			switch (arr->src_type) {
			case 'l': for (; sp!=ep; sp+=8) *dp++ = (int32_t)*(uint64_t*)sp; break;
			case 'b': for (; sp!=ep; sp+=1) *dp++ = *sp != 0 ? 1 : 0; break;
			default: failed = 1; break;
			}
		} else if (arr->dst_type == 'l') {
			int64_t *dp = (int64_t*)dst;
			switch (arr->src_type) {
			case 'i': for (; sp!=ep; sp+=4) *dp++ = *(int32_t*)sp; break;
			case 'b': for (; sp!=ep; sp+=1) *dp++ = *sp != 0 ? 1 : 0; break;
			default: failed = 1; break;
			}
		} else if (arr->dst_type == 'f') {
			float *dp = (float*)dst;
			switch (arr->src_type) {
			case 'i': for (; sp!=ep; sp+=4) *dp++ = (float)*(int32_t*)sp; break;
			case 'l': for (; sp!=ep; sp+=8) *dp++ = (float)*(int64_t*)sp; break;
			case 'd': for (; sp!=ep; sp+=8) *dp++ = (float)*(double*)sp; break;
			default: failed = 1; break;
			}
		} else if (arr->dst_type == 'd') {
			double *dp = (double*)dst;
			switch (arr->src_type) {
			case 'i': for (; sp!=ep; sp+=4) *dp++ = (double)*(int32_t*)sp; break;
			case 'l': for (; sp!=ep; sp+=8) *dp++ = (double)*(int64_t*)sp; break;
			case 'f': for (; sp!=ep; sp+=4) *dp++ = *(float*)sp; break;
			default: failed = 1; break;
			}
		} else if (arr->dst_type == 'b') {
			char *dp = (char*)dst;
			switch (arr->src_type) {
			case 'i': for (; sp!=ep; sp+=4) *dp++ = *(uint32_t*)sp != 0 ? 1 : 0; break;
			case 'l': for (; sp!=ep; sp+=8) *dp++ = *(uint64_t*)sp != 0 ? 1 : 0; break;
			default: failed = 1; break;
			}
		} else {
			return ufbxi_error(uc, "Internal: Unexpected array type");
		}

		if (failed) {
			return ufbxi_errorf(uc, "Bad array conversion: '%c' -> '%c'",
				arr->src_type, arr->dst_type);
		}
	}

	return 1;
}

// Enter the currently focused node. Pushes the node to stack and allows inspecting its
// children. Does not fail if the node is empty.
static int ufbxi_enter_node(ufbxi_context *uc)
{
	if (uc->node_stack_top == uc->node_stack + UFBXI_NODE_STACK_SIZE) {
		return ufbxi_error(uc, "Node stack overflow: Too many nested nodes");
	}

	uc->node_stack_top++;
	*uc->node_stack_top = uc->focused_node;
	return 1;
}

// Exit a node previously entered using `ufbxi_enter_node()`. Future child node
// iteration and find queries will be done to the parent node.
static int ufbxi_exit_node(ufbxi_context *uc)
{
	if (uc->node_stack_top == uc->node_stack) {
		return ufbxi_error(uc, "Internal: Trying to pop root node");
	}

	uc->focused_node = *uc->node_stack_top;
	uc->node_stack_top--;
	return 1;
}

// Parse the node starting from `pos` to `node`. Does not modify `node` if the
// function fails. Returns zero when parsing a NULL-record without failure.
static int ufbxi_parse_node(ufbxi_context *uc, uint64_t pos, ufbxi_node *node)
{
	uint64_t end_pos, values_len, name_pos;
	uint8_t name_len;

	if (uc->version >= 7500 || uc->from_ascii) {
		if (pos > uc->size - 25) {
			return ufbxi_error(uc, "Internal: Trying to read node out of bounds");
		}
		end_pos = ufbxi_read_u64(uc->data + pos + 0);
		values_len = ufbxi_read_u64(uc->data + pos + 16);
		name_len = ufbxi_read_u8(uc->data + pos + 24);
		name_pos = pos + 25;
	} else {
		if (pos > uc->size - 13) {
			return ufbxi_error(uc, "Internal: Trying to read node out of bounds");
		}
		end_pos = ufbxi_read_u32(uc->data + pos + 0);
		values_len = ufbxi_read_u32(uc->data + pos + 8);
		name_len = ufbxi_read_u8(uc->data + pos + 12);
		name_pos = pos + 13;
	}

	if (end_pos == 0) {
		// NULL-record: Return without failure
		return 0;
	}

	uint64_t value_pos = name_pos + name_len;
	uint64_t child_pos = value_pos + values_len;

	// Check for integer overflow and out-of-bounds at the same time. `name_pos`
	// cannot overflow due to the precondition check.
	if (value_pos < name_pos || value_pos > uc->size) {
		return ufbxi_error(uc, "Node name out of bounds");
	}
	if (child_pos < value_pos || child_pos > uc->size) {
		return ufbxi_error(uc, "Node values out of bounds");
	}
	if (end_pos < child_pos || end_pos > uc->size) {
		return ufbxi_error(uc, "Node children out of bounds");
	}

	if (value_pos > (uint64_t)SIZE_MAX || child_pos > (uint64_t)SIZE_MAX
		|| end_pos > (uint64_t)SIZE_MAX) {
		return ufbxi_error(uc, "The file requires 64-bit build");
	}

	node->name.data = uc->data + name_pos;
	node->name.length = name_len;
	node->value_begin_pos = (size_t)value_pos;
	node->next_value_pos = (size_t)value_pos;
	node->child_begin_pos = (size_t)child_pos;
	node->end_pos = (size_t)end_pos;
	node->next_child_pos = (size_t)child_pos;
	return 1;
}

// Move the focus to the next child of the currently entered node.
static int ufbxi_next_child(ufbxi_context *uc, ufbxi_string *name)
{
	ufbxi_node *top = uc->node_stack_top;
	uint64_t pos = top->next_child_pos;
	if (pos == top->end_pos) return 0;

	// Parse the node to be focused. If we encounter a NULL-record here
	// it will be reported as not having found the next child without error.
	if (!ufbxi_parse_node(uc, pos, &uc->focused_node)) return 0;

	top->next_child_pos = uc->focused_node.end_pos;
	if (name) *name = uc->focused_node.name;
	return 1;
}

// Move the focus to the first node matching a name in the currently entered node.
// Does not affect iteration with `ufbxi_next_child()`
static int ufbxi_find_node_str(ufbxi_context *uc, ufbxi_string str)
{
	// TODO
	return 0;
}

// Move to the nth node matching a name in the currently entered node.
// In/out `index`, finds the next node following `index`. Specify `-1` to
// find the first node. Call with the same variable pointer to step through
// all the found matching nodes.
// Does not affect iteration with `ufbxi_next_child()`
static int ufbxi_find_node_str_nth(ufbxi_context *uc, ufbxi_string str, int32_t *index)
{
	// TODO
	return 0;
}

// Convenient shorthands for the above functions.

static ufbxi_forceinline int
ufbxi_find_node(ufbxi_context *uc, const char *name)
{
	ufbxi_string str = { name, strlen(name) };
	return ufbxi_find_node_str(uc, str);
}

static ufbxi_forceinline int
ufbxi_find_node_nth(ufbxi_context *uc, const char *name, int32_t *index)
{
	ufbxi_string str = { name, strlen(name) };
	return ufbxi_find_node_str_nth(uc, str, index);
}

static ufbxi_forceinline int
ufbxi_find_value_str(ufbxi_context *uc, ufbxi_string str, char dst_type, void *dst)
{
	if (!ufbxi_find_node_str(uc, str)) return 0;
	return ufbxi_parse_value(uc, dst_type, dst);
}

static ufbxi_forceinline int
ufbxi_find_value(ufbxi_context *uc, const char *name, char dst_type, void *dst)
{
	ufbxi_string str = { name, strlen(name) };
	if (!ufbxi_find_node_str(uc, str)) return 0;
	return ufbxi_parse_value(uc, dst_type, dst);
}

static ufbxi_forceinline int
ufbxi_find_values_str(ufbxi_context *uc, ufbxi_string str, const char *fmt, ...)
{
	if (!ufbxi_find_node_str(uc, str)) return 0;
	va_list args;
	va_start(args, fmt);
	int ret = ufbxi_parse_values_va(uc, fmt, args);
	va_end(args);
	return ret;
}

static ufbxi_forceinline int
ufbxi_find_values(ufbxi_context *uc, const char *name, const char *fmt, ...)
{
	ufbxi_string str = { name, strlen(name) };
	if (!ufbxi_find_node_str(uc, str)) return 0;
	va_list args;
	va_start(args, fmt);
	int ret = ufbxi_parse_values_va(uc, fmt, args);
	va_end(args);
	return ret;
}

// ASCII format parsing

#define UFBXI_ASCII_END '\0'
#define UFBXI_ASCII_NAME 'N'
#define UFBXI_ASCII_BARE_WORD 'B'
#define UFBXI_ASCII_INT 'I'
#define UFBXI_ASCII_FLOAT 'F'
#define UFBXI_ASCII_STRING 'S'

#define UFBXI_ASCII_MAX_STACK_SIZE 64

typedef struct {
	ufbxi_string str;
	char type;
	union {
		double f64;
		int64_t i64;
		uint32_t name_len;
	} value;
} ufbxi_ascii_token;

typedef struct {
	const char *src;
	const char *src_end;

	char *dst;
	size_t dst_pos;
	size_t dst_size;

	ufbxi_ascii_token prev_token;
	ufbxi_ascii_token token;

	ufbxi_string node_stack[UFBXI_ASCII_MAX_STACK_SIZE];
	uint32_t node_stack_size;

	uint32_t version;

	int failed;
	ufbx_error *error;
} ufbxi_ascii;

static int ufbxi_ascii_do_error(ufbxi_ascii *ua, uint32_t line, const char *desc)
{
	if (ua->failed) return 0;
	ua->failed = 1;
	size_t length = strlen(desc);
	if (length > UFBX_ERROR_DESC_MAX_LENGTH) length = UFBX_ERROR_DESC_MAX_LENGTH;
	if (ua->error) {
		ua->error->source_line = line;
		memcpy(ua->error->desc, desc, length);
		ua->error->desc[length] = '\0';
	}
	return 0;
}

#define ufbxi_ascii_error(ua, desc) ufbxi_ascii_do_error((ua), __LINE__, (desc))

static char ufbxi_ascii_peek(ufbxi_ascii *ua)
{
	return ua->src != ua->src_end ? *ua->src : '\0';
}

static char ufbxi_ascii_next(ufbxi_ascii *ua)
{
	if (ua->src != ua->src_end) ua->src++;
	return ua->src != ua->src_end ? *ua->src : '\0';
}

static char ufbxi_ascii_skip_whitespace(ufbxi_ascii *ua)
{
	// Ignore whitespace
	char c = ufbxi_ascii_peek(ua);
	for (;;) {
		while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			c = ufbxi_ascii_next(ua);
		}
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

static int ufbxi_ascii_next_token(ufbxi_ascii *ua, ufbxi_ascii_token *token)
{
	char c = ufbxi_ascii_skip_whitespace(ua);
	token->str.data = ua->src;

	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
		token->type = UFBXI_ASCII_BARE_WORD;
		uint32_t len = 0;
		while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_') {
			len++;
			c = ufbxi_ascii_next(ua);
		}

		// Skip whitespace to find if there's a following ':'
		c = ufbxi_ascii_skip_whitespace(ua);
		if (c == ':') {
			token->value.name_len = len;
			token->type = UFBXI_ASCII_NAME;
			ufbxi_ascii_next(ua);
		}
	} else if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
		token->type = UFBXI_ASCII_INT;

		char buf[128];
		uint32_t len = 0;
		while ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
			if (c == '.' || c == 'e' || c == 'E') {
				token->type = UFBXI_ASCII_FLOAT;
			}
			if (len == sizeof(buf) - 1) {
				return ufbxi_ascii_error(ua, "Number is too long");
			}
			buf[len++] = c;
			c = ufbxi_ascii_next(ua);
		}
		buf[len] = '\0';

		char *end;
		if (token->type == UFBXI_ASCII_INT) {
			token->value.i64 = strtoll(buf, &end, 10);
			if (end != buf + len) {
				return ufbxi_ascii_error(ua, "Bad integer constant");
			}
		} else if (token->type == UFBXI_ASCII_FLOAT) {
			token->value.f64 = strtod(buf, &end);
			if (end != buf + len) {
				return ufbxi_ascii_error(ua, "Bad float constant");
			}
		}
	} else if (c == '"') {
		token->type = UFBXI_ASCII_STRING;
		c = ufbxi_ascii_next(ua);
		while (c != '"') {
			c = ufbxi_ascii_next(ua);
			if (c == '\0') {
				return ufbxi_ascii_error(ua, "Unclosed string");
			}
		}
		// Skip closing quote
		ufbxi_ascii_next(ua);
	} else {
		token->type = c;
		ufbxi_ascii_next(ua);
	}

	token->str.length = ua->src - token->str.data;
	return 1;
}

static int ufbxi_ascii_accept(ufbxi_ascii *ua, char type)
{
	if (ua->token.type == type) {
		ua->prev_token = ua->token;
		if (!ufbxi_ascii_next_token(ua, &ua->token)) return 0;
		return 1;
	} else {
		return 0;
	}
}

static size_t ufbxi_ascii_push_output(ufbxi_ascii *ua, size_t size)
{
	if (ua->dst_size - ua->dst_pos < size) {
		size_t new_size = 2 * ua->dst_size;
		if (new_size < 1024) new_size = 1024;
		if (new_size < size) new_size = size;
		char *new_dst = (char*)realloc(ua->dst, new_size);
		if (!new_dst) return ~0u;
		ua->dst = new_dst;
		ua->dst_size = new_size;
	}
	size_t pos = ua->dst_pos;
	ua->dst_pos += size;
	return pos;
}

static int ufbxi_ascii_parse_node(ufbxi_ascii *ua)
{
	if (ua->node_stack_size >= UFBXI_ASCII_MAX_STACK_SIZE) {
		return ufbxi_ascii_error(ua, "Too many nested nodes");
	}

	if (!ufbxi_ascii_accept(ua, UFBXI_ASCII_NAME)) {
		return ufbxi_ascii_error(ua, "Expected node name");
	}

	uint32_t name_len = ua->prev_token.value.name_len;
	if (name_len > 0xff) {
		return ufbxi_ascii_error(ua, "Node name is too long");
	}

	ua->node_stack[ua->node_stack_size].data = ua->prev_token.str.data;
	ua->node_stack[ua->node_stack_size].length = name_len;
	ua->node_stack_size++;

	size_t node_pos = ufbxi_ascii_push_output(ua, 25 + name_len);
	if (node_pos == ~0u) return 0;
	ufbxi_write_u8(ua->dst + node_pos + 24, name_len);
	memcpy(ua->dst + node_pos + 25, ua->prev_token.str.data, name_len);

	size_t value_begin_pos = ua->dst_pos;

	int in_array = 0;
	size_t num_values = 0;

	// NOTE: Infinite loop to allow skipping the comma parsing via `continue`.
	for (;;) {
		ufbxi_ascii_token *tok = &ua->prev_token;
		if (ufbxi_ascii_accept(ua, UFBXI_ASCII_STRING)) {
			// The ASCII format supports escaping quotes via "&quot;". There seems
			// to be no way to escape "&quot;" itself. Exporting and importing converts
			// strings with "&quot;" to "\"". Append worst-case data and rewind the
			// write position in case we find "&quot;" escapes.
			size_t string_len = tok->str.length - 2;
			size_t pos = ufbxi_ascii_push_output(ua, 5 + string_len);
			if (pos == ~0u) return 0;
			char *dst = ua->dst + pos + 5;
			size_t bytes_escaped = 0;
			for (size_t i = 0; i < string_len; i++) {
				char c = tok->str.data[1 + i];
				if (c == '&' && i + 6 <= string_len) {
					if (!memcmp(tok->str.data + 1 + i, "&quot;", 6)) {
						bytes_escaped += 5;
						i += 5;
						c = '\"';
					}
				}
				*dst++ = c;
			}

			ua->dst_pos -= bytes_escaped;
			string_len -= bytes_escaped;
			ua->dst[pos + 0] = 'S';
			ufbxi_write_u32(ua->dst + pos + 1, string_len);

		} else if (ufbxi_ascii_accept(ua, UFBXI_ASCII_INT)) {
			int64_t val = tok->value.i64;
			if (val >= INT16_MIN && val <= INT16_MAX) {
				size_t pos = ufbxi_ascii_push_output(ua, 3);
				if (pos == ~0u) return 0;
				ua->dst[pos] = 'Y';
				ufbxi_write_i16(ua->dst + pos + 1, val);
			} else if (val >= INT32_MIN && val <= INT32_MAX) {
				size_t pos = ufbxi_ascii_push_output(ua, 5);
				if (pos == ~0u) return 0;
				ua->dst[pos] = 'I';
				ufbxi_write_i32(ua->dst + pos + 1, val);
			} else {
				size_t pos = ufbxi_ascii_push_output(ua, 9);
				if (pos == ~0u) return 0;
				ua->dst[pos] = 'L';
				ufbxi_write_i64(ua->dst + pos + 1, val);
			}

			// Try to guesstimate the FBX version
			if (!ua->version
				&& ua->node_stack_size == 2
				&& val > 0 && val < INT32_MAX
				&& ufbxi_streq(ua->node_stack[0], "FBXHeaderExtension")
				&& ufbxi_streq(ua->node_stack[1], "FBXVersion")) {
				ua->version = (uint32_t)val;
			}

		} else if (ufbxi_ascii_accept(ua, UFBXI_ASCII_FLOAT)) {
			double val = tok->value.f64;
			if ((double)(float)val == val) {
				size_t pos = ufbxi_ascii_push_output(ua, 5);
				if (pos == ~0u) return 0;
				ua->dst[pos] = 'F';
				ufbxi_write_f32(ua->dst + pos + 1, val);
			} else {
				size_t pos = ufbxi_ascii_push_output(ua, 9);
				if (pos == ~0u) return 0;
				ua->dst[pos] = 'D';
				ufbxi_write_f64(ua->dst + pos + 1, val);
			}

		} else if (ufbxi_ascii_accept(ua, UFBXI_ASCII_BARE_WORD)) {
			if (ufbxi_streq(tok->str, "Y") || ufbxi_streq(tok->str, "T")) {
				size_t pos = ufbxi_ascii_push_output(ua, 2);
				if (pos == ~0u) return 0;
				ua->dst[pos] = 'C';
				ua->dst[pos + 1] = 1;
			} else if (ufbxi_streq(tok->str, "N") || ufbxi_streq(tok->str, "F")) {
				size_t pos = ufbxi_ascii_push_output(ua, 2);
				if (pos == ~0u) return 0;
				ua->dst[pos] = 'C';
				ua->dst[pos + 1] = 0;
			}

		} else if (ufbxi_ascii_accept(ua, '*')) {
			if (in_array) {
				return ufbxi_ascii_error(ua, "Nested array values");
			}
			if (!ufbxi_ascii_accept(ua, UFBXI_ASCII_INT)) {
				return ufbxi_ascii_error(ua, "Expected array size");
			}
			if (ufbxi_ascii_accept(ua, '{')) {
				if (!ufbxi_ascii_accept(ua, UFBXI_ASCII_NAME)) {
					return ufbxi_ascii_error(ua, "Expected array content name");
				}

				// NOTE: This `continue` skips incrementing `num_values` and parsing
				// a comma, continuing to parse the values in the array.
				in_array = 1;
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

	if (in_array) {
		if (!ufbxi_ascii_accept(ua, '}')) {
			return ufbxi_ascii_error(ua, "Unclosed value array");
		}
	}

	size_t value_end_pos = ua->dst_pos;

	if (ufbxi_ascii_accept(ua, '{')) {
		while (!ufbxi_ascii_accept(ua, '}')) {
			if (ua->failed) return 0;
			ufbxi_ascii_parse_node(ua);
		}

		size_t null_pos = ufbxi_ascii_push_output(ua, 25);
		if (null_pos == ~0u) return 0;
		memset(ua->dst + null_pos, 0, 25);
	}

	size_t child_end_pos = ua->dst_pos;
	ufbxi_write_u64(ua->dst + node_pos + 0, child_end_pos);
	ufbxi_write_u64(ua->dst + node_pos + 8, num_values);
	ufbxi_write_u64(ua->dst + node_pos + 16, value_end_pos - value_begin_pos);

	ua->node_stack_size--;
	return !ua->failed;
}

#define UFBXI_BINARY_MAGIC_SIZE 23
static const char ufbxi_binary_magic[] = "Kaydara FBX Binary  \x00\x1a\x00";

static int ufbxi_ascii_parse(ufbxi_ascii *ua)
{
	size_t magic_pos = ufbxi_ascii_push_output(ua, UFBXI_BINARY_MAGIC_SIZE + 4);

	ufbxi_ascii_next_token(ua, &ua->token);
	while (!ufbxi_ascii_accept(ua, UFBXI_ASCII_END)) {
		if (ua->failed) return 0;
		if (!ufbxi_ascii_parse_node(ua)) return 0;
	}
	size_t null_pos = ufbxi_ascii_push_output(ua, 25);
	if (null_pos == ~0u) return 0;
	memset(ua->dst + null_pos, 0, 25);

	size_t version = ua->version ? ua->version : 7500;
	memcpy(ua->dst + magic_pos, ufbxi_binary_magic, UFBXI_BINARY_MAGIC_SIZE);
	ufbxi_write_u32(ua->dst + UFBXI_BINARY_MAGIC_SIZE, version);

	return 1;
}

#endif
