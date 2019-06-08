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

// -- Binary parsing

typedef struct {
	const char *data;
	uint32_t size;
	uint32_t pos;

	uint32_t value_end;

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
	ufbxi_encoding_multivalue = 0x80000000,
} ufbxi_array_encoding;

typedef struct {
	uint32_t num_elements;
	ufbxi_array_encoding encoding;  
	uint32_t encoded_size;
	uint32_t data_offset;
	char src_type;
	char dst_type;
} ufbxi_array;

static int ufbxi_do_error(ufbxi_context *uc, uint32_t line, const char *desc)
{
	size_t length = strlen(desc);
	if (length > UFBX_ERROR_DESC_MAX_LENGTH) length = UFBX_ERROR_DESC_MAX_LENGTH;
	if (uc->error) {
		uc->error->byte_offset = uc->pos;
		memcpy(uc->error->desc, desc, length);
		uc->error->desc[length] = '\0';
	}
	return 0;
}

static int ufbxi_do_errorf(ufbxi_context *uc, uint32_t line, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (uc->error) {
		uc->error->byte_offset = uc->pos;
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
	uint32_t begin = uc->pos;
	uint32_t num_elements = 0;
	while (uc->pos < uc->value_end) {
		char src_type = uc->data[uc->pos];
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
		uc->pos += 1 + size;
		num_elements++;
	}
	if (uc->pos != uc->value_end) return ufbxi_error(uc, "Multivalue array overrun");

	dst->num_elements = num_elements;
	dst->data_offset = begin;
	dst->encoding = ufbxi_encoding_multivalue;
	dst->encoded_size = uc->pos - begin;
	// NOTE: Multivalue arrays can be heterogenous, just use `dst_type` as `src_type`
	dst->src_type = dst_type; 
	dst->dst_type = dst_type;
	return 1;
}
// Parse the next node value in the input stream.
// `dst_type` is a FBX-like type string:
//   Numbers: "I" u/int32_t  "L" u/int64_t  "B" char (bool)  "F" float  "D" double
//   Data: "SR" ufbxi_string  "ilfdb" ufbxi_val_array (matching upper-case type)
//   Misc: "." (ignore)
// Performs implicit conversions:
//   FDILY -> FD, ILYC -> ILB, ILFDYilfd -> fd, ILYCilb -> ilb
static int ufbxi_parse_value(ufbxi_context *uc, char dst_type, void *dst)
{
	// An FBX file must end in a 13-byte NULL node. Due to this a valid
	// FBX file must always have space for the largest possible property header.
	if (uc->size - uc->pos < 13) {
		return ufbxi_error(uc, "Reading value at the end of file");
	}

	char src_type = uc->data[uc->pos];
	const char *src = uc->data + uc->pos + 1;

	// Read the next value: Determine the class and size of the data
	uint32_t val_size = 1;
	ufbxi_val_class val_class;
	uint64_t val_int;
	double val_float;
	switch (src_type) {
	case 'I': val_class = ufbxi_val_int; val_int = ufbxi_read_u32(src); val_size += 4; break;
	case 'L': val_class = ufbxi_val_int; val_int = ufbxi_read_u64(src); val_size += 8; break;
	case 'Y': val_class = ufbxi_val_int; val_int = ufbxi_read_u16(src); val_size += 2; break;
	case 'C': val_class = ufbxi_val_int; val_int = ufbxi_read_u8(src);  val_size += 1; break;
	case 'F': val_class = ufbxi_val_float; val_float = ufbxi_read_f32(src); val_size += 4; break;
	case 'D': val_class = ufbxi_val_float; val_float = ufbxi_read_f64(src); val_size += 8; break;
	case 'S': case 'R':
		val_class = ufbxi_val_string;
		val_size += 4;
		break;
	case 'i': case 'l': case 'f': case 'd': case 'b':
		val_class = ufbxi_val_array;
		val_size += 12;
		break;
	default:
		return ufbxi_errorf(uc, "Invalid type code '%c'", src_type);
	}

	// Interpret the read value into the user pointer, potentially applying the
	// implicit conversion rules.
	if (val_class == ufbxi_val_int) {
		switch (dst_type) {
		case 'I': *(uint32_t*)dst = (uint32_t)val_int; break;
		case 'L': *(uint64_t*)dst = val_int; break;
		case 'B': *(char*)dst = val_int ? 1 : 0; break;
		case 'F': *(float*)dst = (float)val_int; break;
		case 'D': *(double*)dst = (double)val_int; break;
		case 'i': case 'l': case 'f': case 'd': case 'b':
			// Early return: Parse as multivalue array.
			return ufbxi_convert_multivalue_array(uc, (ufbxi_array*)dst, dst_type);
		default:
			return ufbxi_errorf(uc, "Cannot convert from int '%c' to '%c'", src_type, dst_type);
		}

	} else if (val_class == ufbxi_val_float) {
		switch (dst_type) {
		case 'F': *(float*)dst = (float)val_float; break;
		case 'D': *(double*)dst = val_float; break;
		case 'f': case 'd':
			// Early return: Parse as multivalue array.
			return ufbxi_convert_multivalue_array(uc, (ufbxi_array*)dst, dst_type);
		default:
			return ufbxi_errorf(uc, "Cannot convert from float '%c' to '%c'", src_type, dst_type);
		}

	} else if (val_class == ufbxi_val_string) {
		if (dst_type != 'S') {
			return ufbxi_errorf(uc, "Cannot convert from string '%c' to '%c'", src_type, dst_type);
		}

		ufbxi_string *str = (ufbxi_string*)dst;
		str->length = ufbxi_read_u32(src);
		str->data = uc->data + uc->pos + 5;
		val_size += str->length;
		if (str->length > 0x1000000) {
			return ufbxi_errorf(uc, "String is too large: %u bytes", str->length);
		}
	} else if (val_class == ufbxi_val_array) {
		switch (dst_type) {
		case 'i': case 'l': case 'f': case 'd': case 'b': break;
		default:
			return ufbxi_errorf(uc, "Cannot convert from array '%c' to '%c'", src_type, dst_type);
		}

		ufbxi_array *arr = (ufbxi_array*)dst;
		arr->num_elements = ufbxi_read_u32(src + 0);
		uint32_t encoding = ufbxi_read_u32(src + 4);
		arr->encoding = (ufbxi_array_encoding)encoding;
		arr->encoded_size = ufbxi_read_u32(src + 8);
		arr->data_offset = uc->pos + 12;
		arr->src_type = src_type;
		arr->dst_type = dst_type;
		val_size += arr->encoded_size;
		if (arr->encoded_size > 0x1000000) {
			return ufbxi_errorf(uc, "Array is too large: %u bytes", arr->encoded_size);
		}
		if (encoding != ufbxi_encoding_basic && encoding != ufbxi_encoding_deflate) {
			return ufbxi_errorf(uc, "Unknown array encoding: %u", encoding);
		}
	}

	if (val_size > uc->value_end - uc->pos) {
		return ufbxi_errorf(uc, "Value overflows data block: %u bytes", val_size);
	}
	uc->pos += val_size;
	return 1;
}

// Parse multiple values, works like `scanf()`.
// See `ufbxi_parse_value()` for more information.
static int ufbxi_parse_values(ufbxi_context *uc, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const char *fmt_ptr = fmt;
	while (*fmt_ptr) {
		void *dst = va_arg(args, void*);
		if (!ufbxi_parse_value(uc, *fmt_ptr, dst)) {
			va_end(args);
			return 0;
		}
		fmt_ptr++;
	}
	va_end(args);
	return 1;
}

// Decode the data of an multivalue array into memory. Requires the array to be
// prepared with `ufbxi_convert_multivalue_array()`.
static int ufbxi_decode_multivalue_array(ufbxi_context *uc, ufbxi_array *arr, void *dst, char dst_type)
{
	if (arr->encoding != ufbxi_encoding_multivalue) {
		return ufbxi_error(uc, "Internal: Bad multivalue encoding");
	}

	// HACK: Set the parsing position to the array temporarily
	uint32_t old_pos = uc->pos;
	uint32_t old_value_end = uc->value_end;
	uc->pos = arr->data_offset;
	uc->value_end = arr->data_offset + arr->encoded_size;

	char dst_elem_type;
	uint32_t dst_elem_size;
	switch (dst_type) {
	case 'i': dst_elem_type = 'I'; dst_elem_size = 4; break;
	case 'l': dst_elem_type = 'L'; dst_elem_size = 8; break;
	case 'f': dst_elem_type = 'F'; dst_elem_size = 4; break;
	case 'd': dst_elem_type = 'D'; dst_elem_size = 8; break;
	case 'b': dst_elem_type = 'B'; dst_elem_size = 1; break;
	}

	char *dst_ptr = (char*)dst;
	while (uc->pos < uc->value_end) {
		if (!ufbxi_parse_value(uc, dst_elem_type, dst_ptr)) return 0;
		dst_ptr += dst_elem_size;
	}
	if (uc->pos != uc->value_end) return ufbxi_error(uc, "Multivalue array overrun");
	if (dst_ptr != (char*)dst + arr->num_elements * dst_elem_size) {
		return ufbxi_error(uc, "Internal: Multivalue array read failed");
	}

	uc->pos = old_pos;
	uc->value_end = old_value_end;
	return 1;
}

// Decode the contents of an array into memory. Performs potential type conversion
// and decoding of the data. Should not fail for a non-decoding failure reason.
static int ufbxi_decode_array(ufbxi_context *uc, ufbxi_array *arr, void *dst, char dst_type)
{
	if (arr->dst_type != dst_type) return ufbxi_error(uc, "Internal: Array type mismatch");
	uint32_t elem_size;
	switch (dst_type) {
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
			return ufbxi_decode_multivalue_array(uc, arr, dst, dst_type);
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
		if (dst_type == 'i') {
			uint32_t *dp = (uint32_t*)dst;
			switch (arr->src_type) {
			case 'l': for (; sp!=ep; sp+=8) *dp++ = (uint32_t)*(uint64_t*)sp; break;
			case 'b': for (; sp!=ep; sp+=1) *dp++ = *sp != 0 ? 1 : 0; break;
			default: failed = 1; break;
			}
		} else if (dst_type == 'l') {
			uint64_t *dp = (uint64_t*)dst;
			switch (arr->src_type) {
			case 'i': for (; sp!=ep; sp+=4) *dp++ = *(uint32_t*)sp; break;
			case 'b': for (; sp!=ep; sp+=1) *dp++ = *sp != 0 ? 1 : 0; break;
			default: failed = 1; break;
			}
		} else if (dst_type == 'f') {
			float *dp = (float*)dst;
			switch (arr->src_type) {
			case 'i': for (; sp!=ep; sp+=4) *dp++ = (float)*(uint32_t*)sp; break;
			case 'l': for (; sp!=ep; sp+=8) *dp++ = (float)*(uint64_t*)sp; break;
			case 'd': for (; sp!=ep; sp+=8) *dp++ = (float)*(double*)sp; break;
			default: failed = 1; break;
			}
		} else if (dst_type == 'd') {
			double *dp = (double*)dst;
			switch (arr->src_type) {
			case 'i': for (; sp!=ep; sp+=4) *dp++ = (double)*(uint32_t*)sp; break;
			case 'l': for (; sp!=ep; sp+=8) *dp++ = (double)*(uint64_t*)sp; break;
			case 'f': for (; sp!=ep; sp+=4) *dp++ = *(float*)sp; break;
			default: failed = 1; break;
			}
		} else if (dst_type == 'b') {
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
				arr->src_type, dst_type);
		}
	}

	return 1;
}

#endif