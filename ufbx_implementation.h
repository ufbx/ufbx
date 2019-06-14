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
	#define ufbxi_forceinline __attribute__((always_inline))
#else
	#define ufbxi_noinline
	#define ufbxi_forceinline
#endif

#define ufbxi_arraycount(arr) (sizeof(arr) / sizeof(*(arr)))

// -- Utility

typedef struct {
	const char *data;
	uint32_t length;
} ufbxi_string;

static ufbxi_forceinline int
ufbxi_streq(ufbxi_string str, const char *ref)
{
	uint32_t length = (uint32_t)strlen(ref);
	return str.length == length && !memcmp(str.data, ref, length);
}

// TODO: Unaligned loads for some platforms
#define ufbxi_read_u8(ptr) (*(uint8_t*)(ptr))
#define ufbxi_read_u16(ptr) (*(uint16_t*)(ptr))
#define ufbxi_read_u32(ptr) (*(uint32_t*)(ptr))
#define ufbxi_read_u64(ptr) (*(uint64_t*)(ptr))
#define ufbxi_read_f32(ptr) (*(float*)(ptr))
#define ufbxi_read_f64(ptr) (*(double*)(ptr))

#define ufbxi_read_i8(ptr) (int8_t)(ufbxi_read_u8(ptr))
#define ufbxi_read_i16(ptr) (int16_t)(ufbxi_read_u16(ptr))
#define ufbxi_read_i32(ptr) (int32_t)(ufbxi_read_u32(ptr))
#define ufbxi_read_i64(ptr) (int64_t)(ufbxi_read_u64(ptr))

// -- Binary parsing

typedef struct {
	uint32_t value_begin_pos;
	uint32_t child_begin_pos;
	uint32_t end_pos;
	ufbxi_string name;
	uint32_t next_child_pos;
	uint32_t next_value_pos;
} ufbxi_node;

#define UFBXI_NODE_STACK_SIZE 16

typedef struct {
	const char *data;
	uint32_t size;

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
	uint32_t num_elements;
	ufbxi_array_encoding encoding;  
	uint32_t encoded_size;
	uint32_t data_offset;
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
	uint32_t begin = uc->focused_node.next_value_pos;
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
	// An FBX file must end in a 13-byte NULL node. Due to this a valid
	// FBX file must always have space for the largest possible property header.
	if (uc->size - uc->focused_node.next_value_pos < 13) {
		return ufbxi_error(uc, "Reading value at the end of file");
	}

	uint32_t pos = uc->focused_node.next_value_pos;
	char src_type = uc->data[pos];
	const char *src = uc->data + pos + 1;

	// Read the next value locally
	uint32_t val_size = 1;
	ufbxi_val_class val_class;
	int64_t val_int;
	double val_float;
	uint32_t val_num_elements;
	uint32_t val_encoding;
	uint32_t val_encoded_size;
	uint32_t val_data_offset;
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

	uint32_t val_end = pos + val_size;
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

	uint32_t arr_begin = arr->data_offset;
	uint32_t arr_end = arr_begin + arr->encoded_size;
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

	uint32_t arr_size = arr->num_elements * elem_size;

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
static int ufbxi_parse_node(ufbxi_context *uc, uint32_t pos, ufbxi_node *node)
{
	if (pos > uc->size - 13) {
		return ufbxi_error(uc, "Internall: Trying to read node out of bounds");
	}

	uint32_t end_pos = ufbxi_read_u32(uc->data + pos + 0);
	uint32_t values_len = ufbxi_read_u32(uc->data + pos + 8);
	uint8_t name_len = ufbxi_read_u8(uc->data + pos + 12);

	if (end_pos == 0) {
		// NULL-record: Return without failure
		return 0;
	}

	uint32_t name_pos = pos + 13;
	uint32_t value_pos = name_pos + name_len;
	uint32_t child_pos = value_pos + values_len;

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

	node->name.data = uc->data + name_pos;
	node->name.length = name_len;
	node->value_begin_pos = value_pos;
	node->next_value_pos = value_pos;
	node->child_begin_pos = child_pos;
	node->end_pos = end_pos;
	node->next_child_pos = child_pos;
	return 1;
}

// Move the focus to the next child of the currently entered node.
static int ufbxi_next_child(ufbxi_context *uc, ufbxi_string *name)
{
	ufbxi_node *top = uc->node_stack_top;
	uint32_t pos = top->next_child_pos;
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

#endif