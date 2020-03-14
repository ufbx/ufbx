#pragma once

#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define UFBXW_MAX_DEPTH 32

#define ufbxw_arraysize(arr) (sizeof(arr)/sizeof(*(arr)))

typedef struct {
	uint32_t version;
	bool ascii;
	char vertex_type;
	char index_type;
	char normal_type;
} ufbxw_opts;

typedef struct {
	size_t offset;
	size_t num_props;
	size_t num_children;
	size_t name_end;
	size_t props_end;
} ufbxw_node;

typedef struct {
	uint64_t id;
	const char *type;
	const char *name;
} ufbxw_id_mapping;

typedef struct {
	char *data;
	size_t size;
	size_t capacity;

	bool is_ascii;
	uint32_t version;
	ufbxw_opts opts;

	ufbxw_node nodes[UFBXW_MAX_DEPTH];
	ufbxw_node *top;

	ufbxw_id_mapping *id_mappings;
	size_t num_id_mappings;

} ufbxw_context;

#define ufbxw_write_u8(ptr, val) (*(uint8_t*)(ptr) = (uint8_t)(val))
#define ufbxw_write_u16(ptr, val) (*(uint16_t*)(ptr) = (uint16_t)(val))
#define ufbxw_write_u32(ptr, val) (*(uint32_t*)(ptr) = (uint32_t)(val))
#define ufbxw_write_u64(ptr, val) (*(uint64_t*)(ptr) = (uint64_t)(val))
#define ufbxw_write_f32(ptr, val) (*(float*)(ptr) = (float)(val))
#define ufbxw_write_f64(ptr, val) (*(double*)(ptr) = (double)(val))
#define ufbxw_write_i8(ptr, val) ufbxw_write_u8(ptr, (int8_t)(val))
#define ufbxw_write_i16(ptr, val) ufbxw_write_u16(ptr, (int16_t)(val))
#define ufbxw_write_i32(ptr, val) ufbxw_write_u32(ptr, (int32_t)(val))
#define ufbxw_write_i64(ptr, val) ufbxw_write_u64(ptr, (int64_t)(val))

static char *ufbxw_push(ufbxw_context *uw, size_t size)
{
	size_t pos = uw->size;
	uw->size += size;
	if (uw->size > uw->capacity) {
		uw->capacity *= 2;
		if (uw->capacity < uw->size) uw->capacity = uw->size;
		uw->data = (char*)realloc(uw->data, uw->capacity);
	}
	return uw->data + pos;
}

static void ufbxw_literal(ufbxw_context *uw, const char *str)
{
	size_t len = strlen(str);
	char *ptr = ufbxw_push(uw, len);
	memcpy(ptr, str, len);
}

static void ufbxw_fmt(ufbxw_context *uw, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char data[512];
	int len = vsnprintf(data, sizeof(data), fmt, args);
	char *dst = ufbxw_push(uw, len);
	memcpy(dst, data, len);

	va_end(args);
}

static void ufbxw_section(ufbxw_context *uw, const char *name)
{
	if (uw->is_ascii) {
		ufbxw_fmt(uw, "\n; %s\n;------------------------------------------------------------------\n", name);
	}
}

static char *ufbxw_push_prop(ufbxw_context *uw, char type, size_t size)
{
	char *ptr = ufbxw_push(uw, 1 + size);
	ptr[0] = type;
	return ptr + 1;
}

static void ufbxw_prop_float(ufbxw_context *uw, char fmt, double v)
{
	if (uw->is_ascii) {
		ufbxw_literal(uw, uw->top->num_props > 0 ? ", " : " ");
		switch (fmt) {
		case 'B': case 'C': ufbxw_fmt(uw, "%c", v != 0 ? 'Y' : 'N'); break;
		case 'Y': case 'I': case 'L': ufbxw_fmt(uw, "%" PRIi64, (int64_t)v); break;
		case 'F': ufbxw_fmt(uw, "%f", (float)v);break;
		case 'D': ufbxw_fmt(uw, "%f", (double)v); break;
		}
	} else {
		switch (fmt) {
		case 'B': ufbxw_write_u8(ufbxw_push_prop(uw, 'B', 1), v != 0); break;
		case 'C': ufbxw_write_u8(ufbxw_push_prop(uw, 'C', 1), v != 0); break;
		case 'Y': ufbxw_write_i16(ufbxw_push_prop(uw, 'Y', 2), v); break;
		case 'I': ufbxw_write_i32(ufbxw_push_prop(uw, 'I', 4), v); break;
		case 'L': ufbxw_write_i64(ufbxw_push_prop(uw, 'L', 8), v); break;
		case 'F': ufbxw_write_f32(ufbxw_push_prop(uw, 'F', 4), v); break;
		case 'D': ufbxw_write_f64(ufbxw_push_prop(uw, 'D', 8), v); break;
		}
	}
	uw->top->num_props++;
	uw->top->props_end = uw->size;
}

static void ufbxw_prop_int(ufbxw_context *uw, char fmt, int64_t v)
{
	if (uw->is_ascii) {
		ufbxw_literal(uw, uw->top->num_props > 0 ? ", " : " ");
		switch (fmt) {
		case 'B': case 'C': ufbxw_fmt(uw, "%c", v != 0 ? 'T' : 'F'); break;
		case 'Y': case 'I': case 'L': ufbxw_fmt(uw, "%" PRIi64, (int64_t)v); break;
		case 'F': ufbxw_fmt(uw, "%f", (float)v);break;
		case 'D': ufbxw_fmt(uw, "%f", (double)v); break;
		}
	} else {
		switch (fmt) {
		case 'B': ufbxw_write_u8(ufbxw_push_prop(uw, 'B', 1), v != 0); break;
		case 'C': ufbxw_write_u8(ufbxw_push_prop(uw, 'C', 1), v != 0); break;
		case 'Y': ufbxw_write_i16(ufbxw_push_prop(uw, 'Y', 2), v); break;
		case 'I': ufbxw_write_i32(ufbxw_push_prop(uw, 'I', 4), v); break;
		case 'L': ufbxw_write_i64(ufbxw_push_prop(uw, 'L', 8), v); break;
		case 'F': ufbxw_write_f32(ufbxw_push_prop(uw, 'F', 4), v); break;
		case 'D': ufbxw_write_f64(ufbxw_push_prop(uw, 'D', 8), v); break;
		}
	}
	uw->top->num_props++;
	uw->top->props_end = uw->size;
}

static void ufbxw_prop_str(ufbxw_context *uw, char fmt, const char *v)
{
	if (uw->is_ascii) {
		ufbxw_literal(uw, uw->top->num_props > 0 ? ", " : " ");
		ufbxw_fmt(uw, "\"%s\"", v);
	} else {
		size_t len = strlen(v);
		char *ptr = ufbxw_push(uw, 5 + len);
		ptr[0] = fmt;
		ufbxw_write_u32(ptr + 1, len);
		memcpy(ptr + 5, v, len);
	}
	uw->top->num_props++;
	uw->top->props_end = uw->size;
}

static void ufbxw_prop_str_len(ufbxw_context *uw, char fmt, const char *v, size_t len)
{
	if (uw->is_ascii) {
		ufbxw_literal(uw, uw->top->num_props > 0 ? ", " : " ");
		ufbxw_fmt(uw, "\"%.*s\"", (int)len, v);
	} else {
		char *ptr = ufbxw_push(uw, 5 + len);
		ptr[0] = fmt;
		ufbxw_write_u32(ptr + 1, len);
		memcpy(ptr + 5, v, len);
	}
	uw->top->num_props++;
	uw->top->props_end = uw->size;
}

static const char *ufbxw_indent_str = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

static void ufbxw_prop_float_array(ufbxw_context *uw, char fmt, const double *vs, size_t size)
{
	if (uw->version < 7000) {
		char val_type = (char)toupper(fmt);
		for (size_t i = 0; i < size; i++) {
			ufbxw_prop_float(uw, val_type, vs[i]);
		}
	} else if (uw->is_ascii) {
		int num_indent = (int)(uw->top - uw->nodes);
		ufbxw_fmt(uw, " *%zu {\n%.*sa: ", size, num_indent, ufbxw_indent_str);
		for (size_t i = 0; i < size; i++) {
			if (i > 0) ufbxw_literal(uw, ",");
			switch (fmt) {
			case 'b': ufbxw_fmt(uw, "%d", vs[i] != 0); break;
			case 'i': case 'l': ufbxw_fmt(uw, "%" PRIi64, (int64_t)vs[i]); break;
			case 'f': ufbxw_fmt(uw, "%f", (float)vs[i]); break;
			case 'd': ufbxw_fmt(uw, "%f", (float)vs[i]); break;
			}
		}
		ufbxw_fmt(uw, "\n%.*s}", num_indent-1, ufbxw_indent_str);
		uw->top->num_props++;
		uw->top->props_end = uw->size;
	} else {
		size_t elem_size = 0;
		switch (fmt) {
		case 'b': elem_size = 1; break;
		case 'i': case 'f': elem_size = 4; break;
		case 'l': case 'd': elem_size = 8; break;
		}
		char *ptr = ufbxw_push(uw, 13 + size * elem_size);
		ptr[0] = fmt;
		ufbxw_write_u32(ptr + 1, size);
		ufbxw_write_u32(ptr + 5, 0);
		ufbxw_write_u32(ptr + 9, size * elem_size);
		ptr += 13;

		for (size_t i = 0; i < size; i++) {
			switch (fmt) {
			case 'b': ufbxw_write_u8(ptr, vs[i] != 0); break;
			case 'i': ufbxw_write_i32(ptr, vs[i]); break;
			case 'l': ufbxw_write_i64(ptr, vs[i]); break;
			case 'f': ufbxw_write_f32(ptr, vs[i]); break;
			case 'd': ufbxw_write_f64(ptr, vs[i]); break;
			}
			ptr += elem_size;
		}

		uw->top->num_props++;
		uw->top->props_end = uw->size;
	}
}

static void ufbxw_prop_int_array(ufbxw_context *uw, char fmt, const int64_t *vs, size_t size)
{
	if (uw->version < 7000) {
		char val_type = (char)toupper(fmt);
		for (size_t i = 0; i < size; i++) {
			ufbxw_prop_int(uw, val_type, vs[i]);
		}
	} else if (uw->is_ascii) {
		int num_indent = (int)(uw->top - uw->nodes);
		ufbxw_fmt(uw, " *%zu {\n%.*sa: ", size, num_indent, ufbxw_indent_str);
		for (size_t i = 0; i < size; i++) {
			if (i > 0) ufbxw_literal(uw, ",");
			switch (fmt) {
			case 'b': ufbxw_fmt(uw, "%d", vs[i] != 0); break;
			case 'i': case 'l': ufbxw_fmt(uw, "%" PRIi64, (int64_t)vs[i]); break;
			case 'f': ufbxw_fmt(uw, "%f", (float)vs[i]); break;
			case 'd': ufbxw_fmt(uw, "%f", (float)vs[i]); break;
			}
		}
		ufbxw_fmt(uw, "\n%.*s}", num_indent-1, ufbxw_indent_str);
		uw->top->num_props++;
		uw->top->props_end = uw->size;
	} else {
		size_t elem_size = 0;
		switch (fmt) {
		case 'b': elem_size = 1; break;
		case 'i': case 'f': elem_size = 4; break;
		case 'l': case 'd': elem_size = 8; break;
		}
		char *ptr = ufbxw_push(uw, 13 + size * elem_size);
		ptr[0] = fmt;
		ufbxw_write_u32(ptr + 1, size);
		ufbxw_write_u32(ptr + 5, 0);
		ufbxw_write_u32(ptr + 9, size * elem_size);
		ptr += 13;

		for (size_t i = 0; i < size; i++) {
			switch (fmt) {
			case 'b': ufbxw_write_u8(ptr, vs[i] != 0); break;
			case 'i': ufbxw_write_i32(ptr, vs[i]); break;
			case 'l': ufbxw_write_i64(ptr, vs[i]); break;
			case 'f': ufbxw_write_f32(ptr, vs[i]); break;
			case 'd': ufbxw_write_f64(ptr, vs[i]); break;
			}
			ptr += elem_size;
		}

		uw->top->num_props++;
		uw->top->props_end = uw->size;
	}
}

static void ufbxw_begin_node(ufbxw_context *uw, const char *name)
{
	uw->top->num_children++;
	ufbxw_node *node = ++uw->top;
	node->offset = uw->size;
	node->num_props = 0;
	node->num_children = 0;
	if (uw->is_ascii) {
		int num_indent = (int)(uw->top - uw->nodes - 1);
		if (node[-1].num_children == 1) {
			if (num_indent > 0) ufbxw_literal(uw, " {\n");
		} else {
			ufbxw_literal(uw, "\n");
		}
		ufbxw_fmt(uw, "%.*s%s:", num_indent, ufbxw_indent_str, name);
	} else if (uw->version >= 7500) {
		size_t name_len = strlen(name);
		char *ptr = ufbxw_push(uw, 25 + name_len);
		ufbxw_write_u8(ptr + 24, name_len);
		memcpy(ptr + 25, name, name_len);
		node->name_end = node->props_end = uw->size;
	} else {
		size_t name_len = strlen(name);
		char *ptr = ufbxw_push(uw, 13 + name_len);
		ufbxw_write_u8(ptr + 12, name_len);
		memcpy(ptr + 13, name, name_len);
		node->name_end = node->props_end = uw->size;
	}
}

static void ufbxw_end_node(ufbxw_context *uw)
{
	ufbxw_node *node = uw->top--;
	if (uw->is_ascii) {
		int num_indent = (int)(uw->top - uw->nodes);
		if (node->num_children == 0 && node->num_props == 0) {
			ufbxw_fmt(uw, " {\n%.*s}", num_indent, ufbxw_indent_str);
		}
		if (node->num_children > 0) {
			ufbxw_fmt(uw, "\n%.*s}", num_indent, ufbxw_indent_str);
		}
		if (num_indent == 0) ufbxw_literal(uw, "\n");
	} else if (uw->version >= 7500) {
		char *ptr;
		if (node->num_children > 0) {
			ptr = ufbxw_push(uw, 25);
			memset(ptr, 0, 25);
		}
		ptr = uw->data + node->offset;
		ufbxw_write_u64(ptr + 0, uw->size);
		ufbxw_write_u64(ptr + 8, node->num_props);
		ufbxw_write_u64(ptr + 16, node->props_end - node->name_end);
	} else {
		char *ptr;
		if (node->num_children > 0) {
			ptr = ufbxw_push(uw, 13);
			memset(ptr, 0, 13);
		}
		ptr = uw->data + node->offset;
		ufbxw_write_u32(ptr + 0, uw->size);
		ufbxw_write_u32(ptr + 4, node->num_props);
		ufbxw_write_u32(ptr + 8, node->props_end - node->name_end);
	}
}

static void ufbxw_write_values_va(ufbxw_context *uw, const char *fmt, va_list args)
{
	char f;
	while ((f = *fmt++) != 0) {
		switch (f) {
		case 'B': case 'C': case 'I': ufbxw_prop_int(uw, f, va_arg(args, int32_t)); break;
		case 'L': ufbxw_prop_int(uw, f, va_arg(args, int64_t)); break;
		case 'F': case 'D': ufbxw_prop_float(uw, f, va_arg(args, double)); break;
		case 'S': case 'R': ufbxw_prop_str(uw, f, va_arg(args, const char*)); break;
		}
	}
}

static void ufbxw_write_values(ufbxw_context *uw, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ufbxw_write_values_va(uw, fmt, args);
	va_end(args);
}

static void ufbxw_value_node(ufbxw_context *uw, const char *name, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ufbxw_begin_node(uw, name);
	ufbxw_write_values_va(uw, fmt, args);
	ufbxw_end_node(uw);
	va_end(args);
}

static void ufbxw_float_array_node(ufbxw_context *uw, const char *name, char fmt, const double *vs, size_t size)
{
	ufbxw_begin_node(uw, name);
	ufbxw_prop_float_array(uw, fmt, vs, size);
	ufbxw_end_node(uw);
}

static void ufbxw_int_array_node(ufbxw_context *uw, const char *name, char fmt, const int64_t *vs, size_t size)
{
	ufbxw_begin_node(uw, name);
	ufbxw_prop_int_array(uw, fmt, vs, size);
	ufbxw_end_node(uw);
}

static void ufbxw_init(ufbxw_context *uw, const ufbxw_opts *opts)
{
	memset(uw, 0, sizeof(ufbxw_context));
	uw->is_ascii = opts->ascii;
	uw->version = opts->version;
	uw->opts = *opts;
	uw->top = uw->nodes;

	if (uw->is_ascii) {
		ufbxw_fmt(uw, "; FBX %d.%d.%d project file\n", uw->version/1000%10, uw->version/100%10, uw->version/10%10);
		ufbxw_literal(uw, "; ----------------------------------------------------\n\n");
	} else {
		char *ptr = ufbxw_push(uw, 27);
		memcpy(ptr, "Kaydara FBX Binary  \x00\x1a\x00", 23);
		ufbxw_write_u32(ptr + 23, uw->version);
	}
}

static void ufbxw_finish(ufbxw_context *uw)
{
	if (uw->is_ascii) {

	} else if (uw->version >= 7500) {
		char *ptr = ufbxw_push(uw, 25);
		memset(ptr, 0, 25);
	} else {
		char *ptr = ufbxw_push(uw, 13);
		memset(ptr, 0, 13);
	}
}

typedef enum ufbxw_prop_type {
	UFBXW_PROP_UNKNOWN,
	UFBXW_PROP_BOOLEAN,
	UFBXW_PROP_INTEGER,
	UFBXW_PROP_LONG,
	UFBXW_PROP_ENUM,
	UFBXW_PROP_OBJECT,
	UFBXW_PROP_NUMBER,
	UFBXW_PROP_TIME,
	UFBXW_PROP_VECTOR,
	UFBXW_PROP_COLOR,
	UFBXW_PROP_STRING,
	UFBXW_PROP_DATE_TIME,
	UFBXW_PROP_TRANSLATION,
	UFBXW_PROP_ROTATION,
	UFBXW_PROP_SCALING,

	UFBXW_NUM_PROP_TYPES,
} ufbxw_prop_type;

typedef struct {
	const char *name;
	ufbxw_prop_type type;
	const char *flags;

	int64_t value_int;
	double value_float[3];
	const char *value_str;

	const char *type_override;
} ufbxw_prop;

typedef struct {
	uint64_t id;
	const char *name;
	uint64_t parent_id;
	ufbxw_prop *props;
	size_t num_props;

	uint64_t material_id;

} ufbxw_model;

typedef struct {
	uint64_t id;
	const char *name;
	uint64_t parent_id;
	ufbxw_prop *props;
	size_t num_props;

	const double *vertices;
	size_t num_vertices;

	const double *normals;
	size_t num_normals;

	const int32_t *indices;
	size_t num_indices;

	const uint32_t *face_sizes;
	size_t num_faces;

} ufbxw_mesh;

typedef struct {
	uint64_t id;
	const char *name;
	uint64_t parent_id;
	ufbxw_prop *props;
	size_t num_props;
} ufbxw_material;

typedef struct {

	const ufbxw_model *models;
	size_t num_models;

	const ufbxw_mesh *meshes;
	size_t num_meshes;

	const ufbxw_material *materials;
	size_t num_materials;

} ufbxw_scene;

typedef struct {
	void *data;
	size_t size;
} ufbxw_result;

static const char *ufbxw_prop_type_str[] = {
	"Unknown",
	"bool",
	"int",
	"ULongLong",
	"enum",
	"object",
	"double",
	"KTime",
	"Vector3D",
	"Color",
	"KString",
	"DateTime",
	"Lcl Translation",
	"Lcl Rotation",
	"Lcl Scaling",
};

static const char *ufbxw_prop_subtype_str[] = {
	"",
	"",
	"Integer",
	"",
	"",
	"",
	"Number",
	"Time",
	"Vector",
	"",
	"",
	"",
	"",
	"",
	"",
};


static const ufbxw_prop ufbxw_anim_stack_defaults[] = {
	{ "Description", UFBXW_PROP_STRING, "", 0, 0,0,0, "" },
	{ "LocalStart", UFBXW_PROP_TIME, "", 0, 0,0,0, NULL },
	{ "LocalStop", UFBXW_PROP_TIME, "", 0, 0,0,0, NULL },
	{ "ReferenceStart", UFBXW_PROP_TIME, "", 0, 0,0,0, NULL },
	{ "ReferenceStop", UFBXW_PROP_TIME, "", 0, 0,0,0, NULL },
};

static const ufbxw_prop ufbxw_anim_layer_defaults[] = {
	{ "Weight", UFBXW_PROP_NUMBER, "A", 0, 100,0,0, NULL },
	{ "Mute", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "Solo", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "Lock", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "Color", UFBXW_PROP_COLOR, "", 0, 0.8,0.8,0.8, NULL, },
	{ "BlendMode", UFBXW_PROP_ENUM, "", 0, 0,0,0, NULL },
	{ "RotationAccumulationMode", UFBXW_PROP_ENUM, "", 0, 0,0,0, NULL },
	{ "ScaleAccumulationMode", UFBXW_PROP_ENUM, "", 0, 0,0,0, NULL },
	{ "BlendModeBypass",UFBXW_PROP_LONG,"",0 },
};

static const ufbxw_prop ufbxw_mesh_defaults[] = {
	{ "Color", UFBXW_PROP_COLOR, "", 0, 0.8,0.8,0.8, NULL },
	{ "BBoxMin", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "BBoxMax", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "Primary Visibility", UFBXW_PROP_BOOLEAN, "", 1, 0,0,0, NULL },
	{ "Casts Shadows", UFBXW_PROP_BOOLEAN, "", 1, 0,0,0, NULL },
	{ "Receive Shadows", UFBXW_PROP_BOOLEAN, "", 1, 0,0,0, NULL },
};

static const ufbxw_prop ufbxw_material_defaults[] = {
	{ "ShadingModel", UFBXW_PROP_STRING, "", 0, 0,0,0, "Lambert" },
	{ "MultiLayer", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "EmissiveColor", UFBXW_PROP_COLOR, "A", 0, 0,0,0, NULL },
	{ "EmissiveFactor", UFBXW_PROP_NUMBER, "A", 0, 1,0,0, NULL },
	{ "AmbientColor", UFBXW_PROP_COLOR, "A", 0, 0.2,0.2,0.2, NULL },
	{ "AmbientFactor", UFBXW_PROP_NUMBER, "A", 0, 1,0,0, NULL },
	{ "DiffuseColor", UFBXW_PROP_COLOR, "A", 0, 0.8,0.8,0.8, NULL },
	{ "DiffuseFactor", UFBXW_PROP_NUMBER, "A", 0, 1,0,0, NULL },
	{ "Bump", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "NormalMap", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "BumpFactor", UFBXW_PROP_NUMBER, "", 0, 1,0,0, NULL },
	{ "TransparentColor", UFBXW_PROP_COLOR, "A", 0, 0,0,0, NULL },
	{ "TransparencyFactor", UFBXW_PROP_NUMBER, "A", 0, 0,0,0, NULL },
	{ "DisplacementColor", UFBXW_PROP_COLOR, "", 0, 0,0,0, NULL },
	{ "DisplacementFactor", UFBXW_PROP_NUMBER, "", 0, 1,0,0, NULL },
	{ "VectorDisplacementColor", UFBXW_PROP_COLOR, "", 0, 0,0,0, NULL },
	{ "VectorDisplacementFactor", UFBXW_PROP_NUMBER, "", 0, 1,0,0, NULL },
};

static const ufbxw_prop ufbxw_model_defaults[] = {
	{ "QuaternionInterpolate", UFBXW_PROP_ENUM, "", 0, 0,0,0, NULL },
	{ "RotationOffset", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "RotationPivot", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "ScalingOffset", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "ScalingPivot", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "TranslationActive", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "TranslationMin", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "TranslationMax", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "TranslationMinX", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "TranslationMinY", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "TranslationMinZ", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "TranslationMaxX", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "TranslationMaxY", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "TranslationMaxZ", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "RotationOrder", UFBXW_PROP_ENUM, "", 0, 0,0,0, NULL },
	{ "RotationSpaceForLimitOnly", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "RotationStiffnessX", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "RotationStiffnessY", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "RotationStiffnessZ", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "AxisLen", UFBXW_PROP_NUMBER, "", 0, 10,0,0, NULL },
	{ "PreRotation", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "PostRotation", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "RotationActive", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "RotationMin", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "RotationMax", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "RotationMinX", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "RotationMinY", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "RotationMinZ", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "RotationMaxX", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "RotationMaxY", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "RotationMaxZ", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "InheritType", UFBXW_PROP_ENUM, "", 0, 0,0,0, NULL },
	{ "ScalingActive", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "ScalingMin", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "ScalingMax", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "ScalingMinX", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "ScalingMinY", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "ScalingMinZ", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "ScalingMaxX", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "ScalingMaxY", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "ScalingMaxZ", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "GeometricTranslation", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "GeometricRotation", UFBXW_PROP_VECTOR, "", 0, 0,0,0, NULL },
	{ "GeometricScaling", UFBXW_PROP_VECTOR, "", 0, 1,1,1, NULL },
	{ "MinDampRangeX", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MinDampRangeY", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MinDampRangeZ", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MaxDampRangeX", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MaxDampRangeY", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MaxDampRangeZ", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MinDampStrengthX", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MinDampStrengthY", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MinDampStrengthZ", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MaxDampStrengthX", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MaxDampStrengthY", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "MaxDampStrengthZ", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "PreferedAngleX", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "PreferedAngleY", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "PreferedAngleZ", UFBXW_PROP_NUMBER, "", 0, 0,0,0, NULL },
	{ "LookAtProperty", UFBXW_PROP_OBJECT, "", 0, 0,0,0, NULL },
	{ "UpVectorProperty", UFBXW_PROP_OBJECT, "", 0, 0,0,0, NULL },
	{ "Show", UFBXW_PROP_BOOLEAN, "", 1, 0,0,0, NULL },
	{ "NegativePercentShapeSupport", UFBXW_PROP_BOOLEAN, "", 1, 0,0,0, NULL },
	{ "DefaultAttributeIndex", UFBXW_PROP_INTEGER, "", 0, 0,0,0, NULL },
	{ "Freeze", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "LODBox", UFBXW_PROP_BOOLEAN, "", 0, 0,0,0, NULL },
	{ "Lcl Translation", UFBXW_PROP_TRANSLATION, "A", 0, 0,0,0, NULL },
	{ "Lcl Rotation", UFBXW_PROP_ROTATION, "A", 0, 0,0,0, NULL },
	{ "Lcl Scaling", UFBXW_PROP_SCALING, "A", 0, 1,1,1, NULL },
	{ "Visibility", UFBXW_PROP_NUMBER, "A", 0, 1,0,0, NULL, "Visibility" },
	{ "Visibility Inheritance", UFBXW_PROP_NUMBER, "", 0, 1,0,0, NULL, "Visibility Inheritance" },
};

static const ufbxw_prop ufbxw_global_settings_defaults[] = {
	{ "UpAxis", UFBXW_PROP_INTEGER, "", 1, 0,0,0, NULL },
	{ "UpAxisSign", UFBXW_PROP_INTEGER, "", 1, 0,0,0, NULL },
	{ "FrontAxis", UFBXW_PROP_INTEGER, "", 2, 0,0,0, NULL },
	{ "FrontAxisSign", UFBXW_PROP_INTEGER, "", 1, 0,0,0, NULL },
	{ "CoordAxis", UFBXW_PROP_INTEGER, "", 0, 0,0,0, NULL },
	{ "CoordAxisSign", UFBXW_PROP_INTEGER, "", 1, 0,0,0, NULL },
	{ "OriginalUpAxis", UFBXW_PROP_INTEGER, "", 1, 0,0,0, NULL },
	{ "OriginalUpAxisSign", UFBXW_PROP_INTEGER, "", 1, 0,0,0, NULL },
	{ "UnitScaleFactor", UFBXW_PROP_NUMBER, "", 0, 1,0,0, NULL },
	{ "OriginalUnitScaleFactor", UFBXW_PROP_NUMBER, "", 0, 1,0,0, NULL },
	{ "AmbientColor", UFBXW_PROP_COLOR, "", 0, 0,0,0, NULL },
	{ "DefaultCamera", UFBXW_PROP_STRING, "", 0, 0,0,0, "Producer Perspective" },
	{ "TimeMode", UFBXW_PROP_ENUM, "", 11, 0,0,0, NULL },
	{ "TimeProtocol", UFBXW_PROP_ENUM, "", 2, 0,0,0, NULL },
	{ "SnapOnFrameMode", UFBXW_PROP_ENUM, "", 0, 0,0,0, NULL },
	{ "TimeSpanStart", UFBXW_PROP_TIME, "", 1924423250, 0,0,0, NULL },
	{ "TimeSpanStop", UFBXW_PROP_TIME, "", 384884650000, 0,0,0, NULL },
	{ "CustomFrameRate", UFBXW_PROP_NUMBER, "", 0, -1,0,0, NULL },
	{ "TimeMarker", UFBXW_PROP_STRING, "", 0, 0,0,0, NULL, "Compound" },
	{ "CurrentTimeMarker", UFBXW_PROP_INTEGER, "", -1, 0,0,0, NULL },
};

static void ufbxw_node_header_extension(ufbxw_context *uw)
{
	ufbxw_begin_node(uw, "FBXHeaderExtension");
	ufbxw_value_node(uw, "FBXHeaderVersion", "I", 1003);
	ufbxw_value_node(uw, "FBXVersion", "I", uw->version);
	ufbxw_value_node(uw, "Creator", "S", "ufbx_write");

	{
		ufbxw_begin_node(uw, "CurrentCameraResolution");
		ufbxw_value_node(uw, "CameraName", "S", "Producer Perspective");
		ufbxw_value_node(uw, "CameraResolutionMode", "S", "Fixed Resolution");
		ufbxw_value_node(uw, "CameraResolutionW", "I", 960);
		ufbxw_value_node(uw, "CameraResolutionH", "I", 540);
		ufbxw_end_node(uw);
	}

	{
		ufbxw_begin_node(uw, "CreationTimeStamp");
		ufbxw_value_node(uw, "Version", "I", 1000);
		ufbxw_value_node(uw, "Year", "I", 2020);
		ufbxw_value_node(uw, "Month", "I", 3);
		ufbxw_value_node(uw, "Day", "I", 14);
		ufbxw_value_node(uw, "Hour", "I", 1);
		ufbxw_value_node(uw, "Minute", "I", 1);
		ufbxw_value_node(uw, "Second", "I", 1);
		ufbxw_value_node(uw, "Millisecond", "I", 1);
		ufbxw_end_node(uw);
	}

	ufbxw_end_node(uw);
}

static void ufbxw_node_property(ufbxw_context *uw, const ufbxw_prop *prop)
{
	ufbxw_begin_node(uw, uw->version >= 7000 ? "P" : "Property");
	ufbxw_prop_str(uw, 'S', prop->name);
	if (prop->type_override) {
		ufbxw_prop_str(uw, 'S', prop->type_override);
		if (uw->version >= 7000) {
			ufbxw_prop_str(uw, 'S', "");
		}
	} else {
		ufbxw_prop_str(uw, 'S', ufbxw_prop_type_str[prop->type]);
		if (uw->version >= 7000) {
			ufbxw_prop_str(uw, 'S', ufbxw_prop_subtype_str[prop->type]);
		}
	}
	ufbxw_prop_str(uw, 'S', prop->flags ? prop->flags : "");

	switch (prop->type) {
	case UFBXW_PROP_UNKNOWN: break;
	case UFBXW_PROP_BOOLEAN:
	case UFBXW_PROP_INTEGER:
	case UFBXW_PROP_ENUM:
		ufbxw_prop_int(uw, 'I', prop->value_int);
		break;
	case UFBXW_PROP_TIME:
	case UFBXW_PROP_LONG:
		ufbxw_prop_int(uw, 'L', prop->value_int);
		break;
	case UFBXW_PROP_NUMBER:
		ufbxw_prop_float(uw, 'D', prop->value_float[0]);
		break;
	case UFBXW_PROP_VECTOR:
	case UFBXW_PROP_COLOR:
	case UFBXW_PROP_TRANSLATION:
	case UFBXW_PROP_ROTATION:
	case UFBXW_PROP_SCALING:
		ufbxw_prop_float(uw, 'D', prop->value_float[0]);
		ufbxw_prop_float(uw, 'D', prop->value_float[1]);
		ufbxw_prop_float(uw, 'D', prop->value_float[2]);
		break;
	case UFBXW_PROP_STRING:
	case UFBXW_PROP_OBJECT:
	case UFBXW_PROP_DATE_TIME:
		if (prop->value_str) {
			ufbxw_prop_str(uw, 'S', prop->value_str);
		}
		break;
	}

	ufbxw_end_node(uw);
}

static void ufbxw_node_properties(ufbxw_context *uw, const ufbxw_prop *props, size_t num_props, const ufbxw_prop *defaults, size_t num_defaults)
{
	ufbxw_begin_node(uw, uw->version >= 7000 ? "Properties70" : "Properties60");

	if (uw->version < 7000) {
		for (size_t i = 0; i < num_defaults; i++) {
			const ufbxw_prop *prop = &defaults[i];
			bool found = false;
			for (size_t fi = 0; fi < num_props; fi++) {
				if (!strcmp(props[fi].name, prop->name)) {
					found = true;
					break;
				}
			}
			if (!found) {
				ufbxw_node_property(uw, prop);
			}
		}
	}

	for (size_t i = 0; i < num_props; i++) {
		ufbxw_node_property(uw, &props[i]);
	}

	ufbxw_end_node(uw);
}

static void ufbxw_node_definition(ufbxw_context *uw, const char *name, const char *type, size_t count, bool do_props, const ufbxw_prop *props, size_t num_props)
{
	ufbxw_begin_node(uw, "ObjectType");
	ufbxw_prop_str(uw, 'S', name);
	ufbxw_value_node(uw, "Count", "L", count);

	if (uw->version >= 7000 && do_props) {
		ufbxw_begin_node(uw, "PropertyTemplate");
		ufbxw_prop_str(uw, 'S', type);
		ufbxw_node_properties(uw, props, num_props, NULL, 0);
		ufbxw_end_node(uw);
	}

	ufbxw_end_node(uw);
}

static size_t ufbxw_format_type_and_name(ufbxw_context *uw, char *type_and_name, size_t size, const char *name, const char *type)
{
	size_t len;
	if (uw->is_ascii) {
		len = snprintf(type_and_name, size, "%s::%s", type, name);
	} else {
		size_t name_len = strlen(name);
		size_t type_len = strlen(type);
		memcpy(type_and_name, name, name_len);
		type_and_name[name_len + 0] = '\x00';
		type_and_name[name_len + 1] = '\x01';
		memcpy(type_and_name + name_len + 2, type, type_len);
		len = name_len + 2 + type_len;
	}
	return len;
}

static void ufbxw_node_begin_object(ufbxw_context *uw, const char *name, const char *type, const char *sub_type, uint64_t id)
{
	char type_and_name[256];
	size_t name_len = ufbxw_format_type_and_name(uw, type_and_name, sizeof(type_and_name), name, type);

	if (uw->id_mappings) {
		ufbxw_id_mapping *map = &uw->id_mappings[uw->num_id_mappings++];
		map->id = id;
		map->type = type;
		map->name = name;
	}

	ufbxw_begin_node(uw, type);
	if (uw->version >= 7000) {
		ufbxw_prop_int(uw, 'L', id);
	}
	ufbxw_prop_str_len(uw, 'S', type_and_name, name_len);
	ufbxw_prop_str(uw, 'S', sub_type);
}

static ufbxw_id_mapping ufbxw_root_mapping = { 0, "Model", "Scene" };

static const ufbxw_id_mapping *ufbxw_find_id_mapping(ufbxw_context *uw, uint64_t id)
{
	if (id == 0) {
		return &ufbxw_root_mapping;
	}

	for (size_t i = 0; i < uw->num_id_mappings; i++) {
		ufbxw_id_mapping *map = &uw->id_mappings[i];
		if (map->id == id) {
			return map;
		}
	}
	return NULL;
}

static void ufbxw_node_connection(ufbxw_context *uw, uint64_t parent, uint64_t child)
{
	if (uw->version < 7000) {
		const ufbxw_id_mapping *parent_map = ufbxw_find_id_mapping(uw, parent);
		const ufbxw_id_mapping *child_map = ufbxw_find_id_mapping(uw, child);

		char child_tn[256], parent_tn[256];
		size_t parent_len = ufbxw_format_type_and_name(uw, parent_tn, sizeof(parent_tn), parent_map->name, parent_map->type);
		size_t child_len = ufbxw_format_type_and_name(uw, child_tn, sizeof(child_tn), child_map->name, child_map->type);

		ufbxw_begin_node(uw, "Connect");
		ufbxw_prop_str(uw, 'S', "OO");
		ufbxw_prop_str_len(uw, 'S', child_tn, child_len);
		ufbxw_prop_str_len(uw, 'S', parent_tn, parent_len);
		ufbxw_end_node(uw);
	} else {
		ufbxw_value_node(uw, "C", "SLL", "OO", child, parent);
	}
}

static void ufbxw_node_model_inline(ufbxw_context *uw, const ufbxw_model *model)
{
	ufbxw_value_node(uw, "Version", "I", 232);
	ufbxw_value_node(uw, "Shading", "C", 1);
	ufbxw_value_node(uw, "Culling", "S", "CullingOff");

	if (uw->version < 7000) {
		ufbxw_value_node(uw, "MultiLayer", "I", 0);
		ufbxw_value_node(uw, "MultiTake", "I", 0);
	}
}

static void ufbxw_node_mesh_inline(ufbxw_context *uw, const ufbxw_mesh *mesh)
{
	int64_t *indices = malloc(sizeof(int64_t) * mesh->num_indices);
	size_t offset = 0;
	for (size_t fi = 0; fi < mesh->num_faces; fi++) {
		uint32_t size = mesh->face_sizes[fi];
		for (uint32_t i = 0; i < size; i++) {
			if (i + 1 == size) {
				indices[offset] = ~(int64_t)mesh->indices[offset];
			} else {
				indices[offset] = mesh->indices[offset];
			}
			offset++;
		}
	}

	ufbxw_value_node(uw, "GeometryVersion", "I", 124);
	ufbxw_float_array_node(uw, "Vertices", uw->opts.vertex_type, mesh->vertices, mesh->num_vertices * 3);
	ufbxw_int_array_node(uw, "PolygonVertexIndex", uw->opts.index_type, indices, mesh->num_indices);

	bool has_normal = false;

	if (mesh->normals) {
		has_normal = true;
		ufbxw_begin_node(uw, "LayerElementNormal");
		ufbxw_prop_int(uw, 'I', 0);
		ufbxw_value_node(uw, "Version", "I", 101);
		ufbxw_value_node(uw, "Name", "S", "");
		ufbxw_value_node(uw, "MappingInformationType", "S", "ByPolygonVertex");
		ufbxw_value_node(uw, "ReferenceInformationType", "S", "Direct");
		ufbxw_float_array_node(uw, "Normals", uw->opts.normal_type, mesh->normals, mesh->num_normals * 3);
		if (uw->version >= 7400) {
			double *normals_w = malloc(sizeof(double) * mesh->num_normals);
			for (size_t i = 0; i < mesh->num_normals; i++) {
				normals_w[i] = 1.0;
			}
			ufbxw_float_array_node(uw, "NormalsW", uw->opts.normal_type, normals_w, mesh->num_normals);
			free(normals_w);
		}
		ufbxw_end_node(uw);
	}

	{
		ufbxw_begin_node(uw, "LayerElementMaterial");
		ufbxw_prop_int(uw, 'I', 0);
		ufbxw_value_node(uw, "Version", "I", 101);
		ufbxw_value_node(uw, "Name", "S", "");
		ufbxw_value_node(uw, "MappingInformationType", "S", "AllSame");
		ufbxw_value_node(uw, "ReferenceInformationType", "S", "IndexToDirect");
		int64_t materials[] = { 0 };
		ufbxw_int_array_node(uw, "Materials", 'i', materials, 1);
		ufbxw_end_node(uw);
	}

	{
		ufbxw_begin_node(uw, "Layer");
		ufbxw_prop_int(uw, 'I', 0);
		ufbxw_value_node(uw, "Version", "I", 100);

		if (has_normal) {
			ufbxw_begin_node(uw, "LayerElement");
			ufbxw_value_node(uw, "Type", "S", "LayerElementNormal");
			ufbxw_value_node(uw, "TypedIndex", "I", 0);
			ufbxw_end_node(uw);
		}

		{
			ufbxw_begin_node(uw, "LayerElement");
			ufbxw_value_node(uw, "Type", "S", "LayerElementMaterial");
			ufbxw_value_node(uw, "TypedIndex", "I", 0);
			ufbxw_end_node(uw);
		}

		ufbxw_end_node(uw);
	}

	if (uw->version < 7000) {
		char type_and_name[256];
		size_t name_len = ufbxw_format_type_and_name(uw, type_and_name, sizeof(type_and_name), mesh->name, "Geometry");

		ufbxw_begin_node(uw, "NodeAttributeName");
		ufbxw_prop_str_len(uw, 'S', type_and_name, name_len);
		ufbxw_end_node(uw);
	}

	free(indices);
}

static void ufbxw_node_material_inline(ufbxw_context *uw, const ufbxw_material *material)
{
	ufbxw_value_node(uw, "Version", "I", 102);
	ufbxw_value_node(uw, "ShadingModel", "S", "lambert");
	ufbxw_value_node(uw, "MultiLayer", "I", 0);
}


static ufbxw_result ufbxw_write_scene(const ufbxw_scene *scene, const ufbxw_opts *user_opts)
{
	ufbxw_opts opts = *user_opts;

	if (!opts.version) opts.version = 7400;
	if (!opts.vertex_type) opts.vertex_type = 'd';
	if (!opts.index_type) opts.index_type = 'i';
	if (!opts.normal_type) opts.normal_type = 'd';

	ufbxw_context context, *uw = &context;
	ufbxw_init(uw, &opts);

	ufbxw_node_header_extension(uw);

	if (uw->version < 7000) {
		size_t num_objs = scene->num_models + scene->num_meshes;
		uw->id_mappings = (ufbxw_id_mapping*)malloc(sizeof(ufbxw_id_mapping) * num_objs);
	}

	// GlobalSettings
	if (uw->version >= 7000) {
		ufbxw_begin_node(uw, "GlobalSettings");
		ufbxw_value_node(uw, "Version", "I", 1000);
		ufbxw_node_properties(uw, ufbxw_global_settings_defaults, ufbxw_arraysize(ufbxw_global_settings_defaults), NULL, 0);
		ufbxw_end_node(uw);
	}

	// Document
	if (uw->version < 7000) {
		ufbxw_section(uw, "Document Description");

		ufbxw_begin_node(uw, "Document");
		ufbxw_value_node(uw, "Name", "S", "");
		ufbxw_end_node(uw);
	}


	// Documents
	if (uw->version >= 7000) {
		ufbxw_section(uw, "Documents Description");

		ufbxw_begin_node(uw, "Documents");
		ufbxw_value_node(uw, "Count", "I", 1);

		{
			ufbxw_begin_node(uw, "Document");
			ufbxw_prop_int(uw, 'L', 1);
			ufbxw_prop_str(uw, 'S', "");
			ufbxw_prop_str(uw, 'S', "Scene");

			ufbxw_prop props[2] = { 0 };
			props[0].name = "SourceObject";
			props[0].type = UFBXW_PROP_OBJECT;
			props[0].value_str = NULL;
			props[1].name = "ActiveAnimStackName";
			props[1].type = UFBXW_PROP_STRING;
			props[1].value_str = "Take 001";
			ufbxw_node_properties(uw, props, 2, NULL, 0);

			ufbxw_value_node(uw, "RootNode", "I", 0);

			ufbxw_end_node(uw);
		}

		ufbxw_end_node(uw);
	}

	ufbxw_section(uw, "Document References");

	// References
	{
		ufbxw_begin_node(uw, "References");
		ufbxw_end_node(uw);
	}

	ufbxw_section(uw, "Object definitions");

	// Definitions
	{
		int num_defs = 1;
		if (uw->version < 7000) num_defs++;
		if (scene->num_models > 0) num_defs++;
		if (scene->num_meshes > 0 && uw->version >= 7000) num_defs++;
		if (scene->num_materials > 0) num_defs++;

		ufbxw_begin_node(uw, "Definitions");
		ufbxw_value_node(uw, "Version", "I", 100);
		ufbxw_value_node(uw, "Count", "I", num_defs);

		ufbxw_node_definition(uw, "GlobalSettings", "", 1, false, NULL, 0);

#if 0
		if (uw->version < 7000) {
			ufbxw_node_definition(uw, "SceneInfo", "", 1, false, NULL, 0);
		}
#endif

		if (scene->num_models) {
			ufbxw_node_definition(uw, "Model", "FbxNode", scene->num_models, true, ufbxw_model_defaults, ufbxw_arraysize(ufbxw_model_defaults));
		}
		if (scene->num_meshes > 0 && uw->version >= 7000) {
			ufbxw_node_definition(uw, "Geometry", "FbxMesh", scene->num_meshes, true, ufbxw_mesh_defaults, ufbxw_arraysize(ufbxw_mesh_defaults));
		}
		if (scene->num_materials > 0) {
			ufbxw_node_definition(uw, "Material", "FbxSurfaceLambert", scene->num_materials, true, ufbxw_material_defaults, ufbxw_arraysize(ufbxw_material_defaults));
		}

		ufbxw_end_node(uw);
	}

	ufbxw_section(uw, "Object properties");

	// Objects
	{
		ufbxw_begin_node(uw, "Objects");

		for (size_t i = 0; i < scene->num_models; i++) {
			const ufbxw_model *model = &scene->models[i];
			ufbxw_node_begin_object(uw, model->name, "Model", "", model->id);
			ufbxw_node_properties(uw, model->props, model->num_props, ufbxw_model_defaults, ufbxw_arraysize(ufbxw_model_defaults));

			ufbxw_node_model_inline(uw, model);

			if (uw->version < 7000) {
				for (size_t mi = 0; mi < scene->num_meshes; mi++) {
					if (scene->meshes[mi].parent_id == model->id) {
						ufbxw_node_mesh_inline(uw, &scene->meshes[mi]);
						break;
					}
				}
			}

			ufbxw_end_node(uw);
		}

		if (uw->version >= 7000) {
			for (size_t i = 0; i < scene->num_meshes; i++) {
				const ufbxw_mesh *mesh = &scene->meshes[i];
				ufbxw_node_begin_object(uw, mesh->name, "Geometry", "Mesh", mesh->id);
				ufbxw_node_properties(uw, mesh->props, mesh->num_props, ufbxw_mesh_defaults, ufbxw_arraysize(ufbxw_mesh_defaults));

				ufbxw_node_mesh_inline(uw, mesh);

				ufbxw_end_node(uw);
			}
		}

		for (size_t i = 0; i < scene->num_materials; i++) {
			const ufbxw_material *material = &scene->materials[i];
			ufbxw_node_begin_object(uw, material->name, "Material", "", material->id);
			ufbxw_node_properties(uw, material->props, material->num_props, ufbxw_material_defaults, ufbxw_arraysize(ufbxw_material_defaults));

			ufbxw_node_material_inline(uw, material);

			ufbxw_end_node(uw);
		}

		// GlobalSettings
		if (uw->version < 7000) {
			ufbxw_begin_node(uw, "GlobalSettings");
			ufbxw_value_node(uw, "Version", "I", 1000);
			ufbxw_node_properties(uw, ufbxw_global_settings_defaults, ufbxw_arraysize(ufbxw_global_settings_defaults), NULL, 0);
			ufbxw_end_node(uw);
		}

		ufbxw_end_node(uw);
	}

	ufbxw_section(uw, "Object connections");

	// Connections
	{
		ufbxw_begin_node(uw, "Connections");

		for (size_t i = 0; i < scene->num_models; i++) {
			const ufbxw_model *model = &scene->models[i];
			if (model->material_id) {
				ufbxw_node_connection(uw, model->id, model->material_id);
			}
			ufbxw_node_connection(uw, model->parent_id, model->id);
		}

		if (uw->version >= 7000) {
			for (size_t i = 0; i < scene->num_meshes; i++) {
				const ufbxw_mesh *mesh = &scene->meshes[i];
				ufbxw_node_connection(uw, mesh->parent_id, mesh->id);
			}
		}

		ufbxw_end_node(uw);
	}

	ufbxw_section(uw, "Takes section");

	// Takes
	{
		ufbxw_begin_node(uw, "Takes");

		ufbxw_value_node(uw, "Current", "S", "Take 001");

		{
			ufbxw_begin_node(uw, "Take");
			ufbxw_prop_str(uw, 'S', "Take 001");

			ufbxw_value_node(uw, "FileName", "S", "Take_001.tak");
			ufbxw_value_node(uw, "LocalTime", "LL", INT64_C(1924423250), INT64_C(230930790000));
			ufbxw_value_node(uw, "ReferenceTime", "LL", INT64_C(1924423250), INT64_C(230930790000));

			ufbxw_end_node(uw);
		}

		ufbxw_end_node(uw);
	}

	// Version5
	if (uw->version < 7000) {
		ufbxw_begin_node(uw, "Version5");

		{
			ufbxw_begin_node(uw, "AmbientRenderSettings");
			ufbxw_value_node(uw, "Version", "I", 101);
			ufbxw_value_node(uw, "AmbientLightColor", "FFFF", 0,0,0,1);
			ufbxw_end_node(uw);
		}

		{
			ufbxw_begin_node(uw, "FogOptions");
			ufbxw_value_node(uw, "FlogEnable", "I", 0);
			ufbxw_value_node(uw, "FogMode", "I", 0);
			ufbxw_value_node(uw, "FogDensity", "D", 0.002);
			ufbxw_value_node(uw, "FogStart", "D", 0.3);
			ufbxw_value_node(uw, "FogEnd", "D", 1000);
			ufbxw_value_node(uw, "FogColor", "FFFF", 1,1,1,1);
			ufbxw_end_node(uw);
		}

		{
			ufbxw_begin_node(uw, "Settings");
			ufbxw_value_node(uw, "FrameRate", "S", "24");
			ufbxw_value_node(uw, "TimeFormat", "I", 1);
			ufbxw_value_node(uw, "SnapOnFrames", "I", 0);
			ufbxw_value_node(uw, "ReferenceTimeIndex", "I", -1);
			ufbxw_value_node(uw, "TimeLineStartTime", "L", 1924423250);
			ufbxw_value_node(uw, "TimeLineStopTime", "L", 384884650000);
			ufbxw_end_node(uw);
		}

		{
			ufbxw_begin_node(uw, "RendererSetting");
			ufbxw_value_node(uw, "DefaultCamera", "S", "Producer Perspective");
			ufbxw_value_node(uw, "DefaultViewingMode", "I", 0);
			ufbxw_end_node(uw);
		}

		ufbxw_end_node(uw);
	}

	free(uw->id_mappings);

	ufbxw_finish(uw);

	ufbxw_result result;
	result.data = uw->data;
	result.size = uw->size;
	return result;
}
