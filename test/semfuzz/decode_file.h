#ifndef SEMFUZZ_DECODE_FILE_H
#define SEMFUZZ_DECODE_FILE_H

#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>

#include "string_table.h"

namespace semfuzz {

typedef uint16_t string_id;

enum {
    INST_FIELD = 0x0,
    INST_ARRAY_INT = 0x1,
    INST_ARRAY_FLOAT = 0x2,
    INST_VALUE_STRING = 0x3,
    INST_VALUE_INT = 0x4,
    INST_VALUE_FLOAT = 0x5,
    INST_VALUE_CHAR = 0x6,
};

#define MAX_VALUES 8

struct Value {
    uint16_t type;
    uint16_t value;
    uint32_t next_sibling;
};

struct Field {
    string_id name;

    uint32_t first_child;
    uint32_t last_child;
    uint32_t next_sibling;

    uint32_t first_value;
    uint32_t last_value;

    uint16_t array_type;
    uint16_t array_size;
    uint16_t array_hash;
};

struct File {
    Field *fields = nullptr;
    size_t max_fields = 0;
    Value *values = nullptr;
    size_t max_values = 0;

    uint32_t num_fields = 0;
    uint32_t num_values = 0;

    uint16_t version = 0;
    uint32_t flags = 0;
    uint32_t temp_limit = 0;
    uint32_t result_limit = 0;

	uint32_t add_child(uint32_t parent_index, string_id name)
	{
		Field field = { };
		field.name = name;

        if (num_fields == max_fields) return ~0u;
		uint32_t index = (uint32_t)num_fields;
        fields[index] = field;
        num_fields++;

		Field &parent = fields[parent_index];
		if (parent.first_child == 0) {
			parent.first_child = index;
			parent.last_child = index;
		} else {
			Field &prev = fields[parent.last_child];
			prev.next_sibling = index;
			parent.last_child = index;
		}
		return index;
	}

	uint32_t add_value(uint32_t parent_index, uint16_t type, uint16_t value)
	{
        if (num_values == max_fields) return ~0u;
		uint32_t index = (uint32_t)num_values;
        values[index] = Value{ type, value };
        num_values++;

		Field &parent = fields[parent_index];
		if (parent.first_value == 0) {
			parent.first_value = index;
			parent.last_value = index;
		} else {
			Value &prev = values[parent.last_value];
			prev.next_sibling = index;
			parent.last_value = index;
		}
		return index;
	}
};

static uint16_t read_u16(const uint8_t *ptr)
{
    return (uint16_t)((uint32_t)ptr[0] | (uint32_t)ptr[1] << 8);
}

static uint32_t read_u32(const uint8_t *ptr)
{
    return ((uint32_t)ptr[0]
        | (uint32_t)ptr[1] << 8
        | (uint32_t)ptr[2] << 16
        | (uint32_t)ptr[3] << 24);
}

static bool read_fbb(File &file, const void *data, size_t size)
{
    file.num_values = 1;
    file.num_fields = 1;
    file.fields[0] = Field{};
    file.values[0] = Value{};

    uint32_t stack[17] = { 0 };

    const uint8_t *bytecode = (const uint8_t*)data;

    if (size < 14) return false;

    file.version = read_u16(bytecode + 0);
    file.flags = read_u32(bytecode + 2);
    file.temp_limit = read_u32(bytecode + 6);
    file.result_limit = read_u32(bytecode + 10);
    size_t pc = 14;

    while (pc + 2 <= size) {
        uint32_t inst_word = read_u16(bytecode + pc);
        if ((inst_word & 0xff) != ((inst_word >> 8) ^ 0xff)) {
            pc += 1;
            continue;
        }

        uint32_t inst = inst_word & 0xf;
        uint32_t level = (inst_word >> 4) & 0xf;

        if (inst == INST_FIELD) {
            if (pc + 4 > size) break;
			if (level != 0 || stack[level] == 0) {
                uint32_t index = file.add_child(stack[level], read_u16(bytecode + pc + 1*2));
                if (index == ~0u) return false;
                stack[level + 1] = index;
            }
			pc += 4;
        } else if (inst >= INST_ARRAY_INT && inst <= INST_ARRAY_FLOAT) {
            if (pc + 12 > size) break;
			if (level == 0 || stack[level] != 0) {
                uint32_t index = file.add_child(stack[level], read_u16(bytecode + pc + 1*2));
                if (index == ~0u) return false;
                Field &field = file.fields[index];
                field.array_size = read_u16(bytecode + pc + 2*2);
                field.array_hash = read_u16(bytecode + pc + 3*2);
                field.array_type = inst;

                uint32_t index_min = file.add_value(index, 0, read_u16(bytecode + pc + 4*2));
                uint32_t index_max = file.add_value(index, 0, read_u16(bytecode + pc + 5*2));
                if (index_min == ~0u || index_max == ~0u) return false;
            }
			pc += 12;
        } else if (inst >= INST_VALUE_STRING && inst <= INST_VALUE_CHAR) {
            if (pc + 4 > size) break;
			if (stack[level] != 0) {
                Field &field = file.fields[stack[level]];
                uint32_t index = file.add_value(stack[level], inst, read_u16(bytecode + pc + 1*2));
                if (index == ~0u) return false;
            }
			pc += 4;
        } else {
            pc += 2;
        }
    }

    return true;
}

struct Stream {
    char *start, *dst, *end;
    bool failed = false;

    void indent(uint32_t count)
    {
        for (uint32_t i = 0; i < count; i++) {
            if (dst == end) failed = true;
            if (failed) return;
            *dst++ = ' ';
        }
    }

    void write(const char *str)
    {
        if (failed) return;
        size_t length = strlen(str);
        if (length >= (size_t)(end - dst)) {
            failed = true;
            return;
        }
        memcpy(dst, str, length);
        dst += length;
    }

    void format(const char *fmt, ...)
    {
        if (failed) return;

        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(dst, end - dst, fmt, args);
        va_end(args);
        if (written < 0 || written >= end - dst) failed = true;
        if (!failed) {
			dst += written;
        }
    }
};

static int64_t decode_int(uint32_t index)
{
    uint32_t sign = index >> 15;
    uint32_t exp = (index >> 13) & 0x3;
    int64_t mantissa = index & 0x1fff;
    int64_t value = 0;
    if (exp == 0) {
        value = mantissa;
    } else if (exp == 1) {
        value = 0x2000 + mantissa * 0x3;
	} else if (exp == 2) {
        value = 0x8000 + mantissa * 0x40000;
	} else {
        value = INT64_C(0x80000000) + mantissa * INT64_C(0x4000000000000);
	}
    return sign ? -value : value;
}

static double decode_float(uint32_t index)
{
    uint32_t sign = index >> 15;
    int32_t exp = ((int32_t)((index >> 8) & 0x7f) - 64);
    if (exp == -64) return sign ? -0.0 : 0.0;
    double mantissa = (double(index & 0xff) / 256.0) * 0.5 + 0.5;
    double value = ldexp(mantissa, exp);
    return sign ? -value : value;
}

struct rng_state {
    uint64_t x[2];
};

uint64_t rng_u64(rng_state &state)
{
	uint64_t t = state.x[0];
	uint64_t const s = state.x[1];
	state.x[0] = s;
	t ^= t << 23;
	t ^= t >> 18;
	t ^= s ^ (s >> 5);
	state.x[1] = t;
	return t + s;
}

int64_t rng_i64_range(rng_state &state, int64_t min_v, int64_t max_v)
{
    if (min_v == max_v) return min_v;
    uint64_t range = (uint64_t)(max_v - min_v + 1);
    return (int64_t)(min_v + rng_u64(state) % range);
}

double rng_f64(rng_state &state)
{
    return (double)rng_u64(state) * 5.421010862427522e-20;
}

double rng_f64_range(rng_state &state, double min_v, double max_v)
{
    double t = rng_f64(state);
    return min_v * (1.0 - t) + max_v * t;
}

struct NumberCache {
    static constexpr uint32_t size = 0x10000;

    int64_t int_value[size];
    char int_str[size][64];

    uint64_t float_bits[size];
    char float_str[size][64];

    static uint32_t hash_u64(uint64_t value)
    {
        uint64_t x = value;
		x ^= x >> 32;
		x *= 0xd6e8feb86659fd93U;
		x ^= x >> 32;
		x *= 0xd6e8feb86659fd93U;
		x ^= x >> 32;
		return (uint32_t)x;
    }

    void init()
    {
        {
			uint32_t index = hash_u64(0) & (size - 1);
			char *dst = int_str[index];
			snprintf(dst, 64, "%lld", (long long)0);
        }

        {
			uint32_t index = hash_u64(0) & (size - 1);
			char *dst = float_str[index];
			snprintf(dst, 64, "%g", 0.0);
        }
    }

    char *format_int(int64_t value)
    {
        uint32_t index = hash_u64((uint64_t)value) & (size - 1);
        char *dst = int_str[index];
        if (int_value[index] != value) {
            snprintf(dst, 64, "%lld", (long long)value);
            int_value[index] = value;
        }
        return dst;
    }

    char *format_float(double value)
    {
        uint64_t bits = 0;
        memcpy(&bits, &value, sizeof(double));
        uint32_t index = hash_u64(bits) & (size - 1);
        char *dst = float_str[index];
        if (float_bits[index] != bits) {
            snprintf(dst, 64, "%g", value);
            float_bits[index] = bits;
        }
        return dst;
    }
};

struct Writer {
    Stream stream;
    const File &file;
    NumberCache *number_cache;

    void write_string(string_id id)
    {
        const char *str;
        if (id & 0x8000) {
            str = unknown_string_table[(id & 0x7fff) % UNKNOWN_STRING_TABLE_SIZE];
        } else {
            str = string_table[id % STRING_TABLE_SIZE];
        }
        stream.write(str);
    }

    void write_escaped_string(string_id id)
    {
        const char *str;
        if (id & 0x8000) {
            str = unknown_string_table[(id & 0x7fff) % UNKNOWN_STRING_TABLE_SIZE];
        } else {
            str = string_table[id % STRING_TABLE_SIZE];
        }

        stream.write("\"");
        while (*str) {
            char c = *str++;
            if (stream.dst == stream.end) stream.failed = true;
            if (stream.failed) return;
            if (c == '"') {
                stream.write("&amp;");
            } else {
                *stream.dst++ = c;
            }
        }
        stream.write("\"");
    }

    void write_field(uint32_t index, uint32_t indent)
    {
        const Field &field = file.fields[index];

        stream.indent(indent * 4);
		write_string(field.name);
        stream.write(": ");
        if (field.array_type != 0) {
			uint32_t size = field.array_size;
            if (size >= 4096) size = 4096;

            stream.format("*%u {\n", size);
            stream.indent((indent + 1) * 4);
            stream.write("a: ");

            rng_state rng = { (uint64_t)field.array_hash + 1, 1 };

            Value min_val = file.values[field.first_value];
            Value max_val = file.values[field.last_value];

            if (field.array_type == INST_ARRAY_INT) {
                int64_t min_v = decode_int(min_val.value);
                int64_t max_v = decode_int(max_val.value);
                if (min_v > max_v) {
                    std::swap(min_v, max_v);
                }
                for (uint32_t i = 0; i < size; i++) {
                    if (i != 0) stream.write(", ");
                    stream.write(number_cache->format_int(rng_i64_range(rng, min_v, max_v)));
                    // stream.format("%lld", rng_i64_range(rng, min_v, max_v));
                }
            } else if (field.array_type == INST_ARRAY_FLOAT) {
                double min_v = decode_float(min_val.value);
                double max_v = decode_float(max_val.value);
                if (min_v > max_v) {
                    std::swap(min_v, max_v);
                }
                for (uint32_t i = 0; i < size; i++) {
                    if (i != 0) stream.write(", ");
                    stream.write(number_cache->format_float(rng_f64_range(rng, min_v, max_v)));
                    // stream.format("%g", rng_f64_range(rng, min_v, max_v));
                }
            }

            stream.write("\n");
			stream.indent(indent * 4);
            stream.write("}\n");
        } else {
            uint32_t value_ix = field.first_value;
            while (value_ix) {
                const Value &value = file.values[value_ix];
                if (value_ix != field.first_value) stream.write(", ");

                if (value.type == INST_VALUE_STRING) {
                    write_escaped_string(value.value);
                } else if (value.type == INST_VALUE_INT) {
                    stream.write(number_cache->format_int(decode_int(value.value)));
                    // stream.format("%lld", decode_int(value.value));
                } else if (value.type == INST_VALUE_FLOAT) {
                    stream.write(number_cache->format_float(decode_float(value.value)));
                    // stream.format("%g", decode_float(value.value));
                } else if (value.type == INST_VALUE_CHAR) {
                    stream.format("%c", (char)(value.value & 0x7f));
                }

                value_ix = value.next_sibling;
            }

            if (field.first_child != 0) {
                stream.write(" {\n");
                uint32_t child = field.first_child;
                while (child) {
                    write_field(child, indent + 1);
                    child = file.fields[child].next_sibling;
                }
				stream.indent(indent * 4);
				stream.write("}\n");
            } else {
				stream.write("\n");
            }
        }

        if (indent == 0) {
            stream.write("\n");
        }
    }
};

static size_t write_ascii(char *dst, size_t dst_size, const File &file, NumberCache *number_cache)
{
    Writer w = { { dst, dst, dst + dst_size }, file, number_cache };
    w.stream.format("; FBX %u.%u.0 project file\n", file.version / 1000 % 10, file.version / 100 % 10);
    w.stream.write("; ----------------------------------------------------\n\n");

    const Field &top = file.fields[0];
	uint32_t child = top.first_child;
	while (child) {
		w.write_field(child, 0);
		child = file.fields[child].next_sibling;
	}

    if (!w.stream.failed && w.stream.dst != w.stream.end) {
        *w.stream.dst = '\0';
        return w.stream.dst - w.stream.start;
    } else {
        w.stream.failed = true;
        return 0;
    }
}

}

#endif
