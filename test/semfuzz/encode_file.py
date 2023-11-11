import common
import argparse
import re
import math
import struct
from collections import namedtuple

string_table = list(common.read_string_table(0))
unknown_string_table = list(common.read_string_table(1))
string_to_index = { s: i for i,s in enumerate(string_table) }
unknown_string_to_index = { s: i for i,s in enumerate(unknown_string_table) }

re_name = re.compile(r"^([A-Za-z0-9_]+):\s*(.*)")
re_str = re.compile(r"^\"([^\"]*)\"$")
re_int = re.compile(r"^[-+]?[0-9]+$")
re_word = re.compile(r"^[A-Za-z]+$")
re_float = re.compile(r"[-+]?[0-9]+(?:\.[0-9]+)?(?:[eE][+\-]?[0-9]+)?")
re_magic = re.compile(r"; FBX (\d).(\d).(\d) project file")

Value = namedtuple("Value", "type value")
VALUE_STRING = 0
VALUE_INTEGER = 1
VALUE_FLOAT = 2
VALUE_CHAR = 3

c_gray = "\033[1;30m"
c_def = "\033[0m"

class Field:
    def __init__(self, name, values, is_array):
        self.name = name
        self.values = values
        self.properties = []
        self.fields = []
        self.is_array = is_array
        self.array_min = None
        self.array_max = None
        self.array_float = False
        self.array_size = 0
        self.array_hash = 0
        self.array_explicit = False

class File:
    def __init__(self):
        self.version = 0
        self.flags = 0
        self.temp_limit = 0
        self.result_limit = 0
        self.top_field = None

def dump_str(index):
    if index < len(string_table):
        return f"{index:04x}{c_gray}[{string_table[index]}]{c_def}"
    else:
        un_index = (index & 0x7fff) % len(unknown_string_table)
        return f"{index:04x}{c_gray}[{unknown_string_table[un_index]}]{c_def}"

def decode_int(index):
    sign = index >> 15
    exp = (index >> 13) & 0x3
    mantissa = index & 0x1fff
    if exp == 0:
        value = mantissa
    elif exp == 1:
        value = 0x2000 + mantissa * 0x3
    elif exp == 2:
        value = 0x8000 + mantissa * 0x40000
    elif exp == 3:
        value = 0x80000000 + mantissa * 0x4000000000000
    return -value if sign else value

def decode_float(index):
    sign = index >> 15
    exp = (((index >> 8) & 0x7f) - 64)
    mantissa = (float(index & 0xff) / 256.0) * 0.5 + 0.5
    value = math.ldexp(mantissa, exp)
    if exp == -64:
        value = 0
    return -value if sign else value

def decode_number(value):
    if value.type == VALUE_INTEGER:
        return decode_int(value.value)
    elif value.type == VALUE_FLOAT:
        return decode_float(value.value)
    else:
        assert False

def dump_int(index):
    return f"{index:04x}{c_gray}[{decode_int(index)}]{c_def}"

def dump_float(index):
    return f"{index:04x}{c_gray}[{decode_float(index)}]{c_def}"

def dump_char(index):
    char = chr(index & 0xff)
    return f"{index:04x}{c_gray}[{char}]{c_def}"

def dump_value(value):
    if value.type == VALUE_STRING:
        return f"{value.type}{c_gray}[string]{c_def}:{dump_str(value.value)}"
    elif value.type == VALUE_INTEGER:
        return f"{value.type}{c_gray}[integer]{c_def}:{dump_int(value.value)}"
    elif value.type == VALUE_FLOAT:
        return f"{value.type}{c_gray}[float]{c_def}:{dump_float(value.value)}"
    elif value.type == VALUE_CHAR:
        return f"{value.type}{c_gray}[char]{c_def}:{dump_char(value.value)}"

def dump_field(field, indent=0):
    if field.is_array:
        if field.array_float:
            min_v = encode_float(field.array_min)
            max_v = encode_float(field.array_max)
            result = f"{'  ' * indent}{dump_str(field.name)}: [array] 2:{c_gray}[float]{c_def} {field.array_size} {field.array_hash & 0xffff} {dump_float(min_v)} {dump_float(max_v)}"
        else:
            min_v = encode_int(int(field.array_min))
            max_v = encode_int(int(field.array_max))
            result = f"{'  ' * indent}{dump_str(field.name)}: [array] 1:{c_gray}[integer]{c_def} {field.array_size} {field.array_hash & 0xffff} {dump_int(min_v)} {dump_int(max_v)}"
        return result
    else:
        values = (dump_value(v) for v in field.values)
        result = f"{'  ' * indent}{dump_str(field.name)}: {' '.join(values)}"
        for child in field.fields:
            result += "\n" + dump_field(child, indent + 1)
        return result

def encode_string(value):
    content = value.replace("&quot;", "\"")
    ix = string_to_index.get(content)
    if ix is not None:
        return ix
    else:
        ix = unknown_string_to_index.get(content)
        if not ix:
            ix = hash(content) % len(unknown_string_table)
        return 0x8000 | ix

seen_int_mapping = [{}, {}, {}, {}]

def add_int(exp, value):
    existing = seen_int_mapping[exp].get(value)
    if existing is not None:
        return existing
    index = len(seen_int_mapping[exp])
    seen_int_mapping[exp][value] = index
    return index

def encode_int(value):
    sign = 1 if value < 0 else 0
    if sign:
        value = -value
    if value < 0x2000:
        # Factor 0x1
        exp = 0
    elif value < 0x8000:
        # Factor 0x3
        exp = 1
        value = add_int(exp, value) & 0x1fff
    elif value < 0x80000000:
        # Factor 0x40000
        exp = 2
        value = add_int(exp, value) & 0x1fff
    else:
        # Factor 0x4000000000000
        exp = 3
        value = add_int(exp, value) & 0x1fff
    return sign << 15 | exp << 13 | value

def encode_float(value):
    sign = 1 if value < 0 else 0
    if sign:
        value = -value
    if value != 0:
        m, exp = math.frexp(value)
    else:
        m, exp = 0.5, -64
    exp = min(max(exp + 64, 0), 127)
    m = min(max(int((m - 0.5) * 2 * 256), 0), 255)
    return sign << 15 | exp << 8 | m

def encode_value(s):
    m = re_str.match(s)
    if m:
        return Value(VALUE_STRING, encode_string(m.group(1)))
    m = re_int.match(s)
    if m:
        return Value(VALUE_INTEGER, encode_int(int(m.group(0))))
    m = re_word.match(s)
    if m:
        return Value(VALUE_CHAR, ord(s[0]))
    try:
        value = float(s)
        return Value(VALUE_FLOAT, encode_float(value))
    except:
        pass
    return None

def encode_file(src):
    file = File()
    file.top_field = top_field = Field(0, [], False)
    stack = [top_field]

    def iter_lines():
        buffer = ""
        for line in src:
            line = line.strip()
            if buffer.endswith(",") or buffer.endswith(":") or line.startswith(","):
                buffer += line
            else:
                yield buffer
                buffer = line
        yield buffer

    for line in iter_lines():
        if line == "}":
            field = stack.pop()
            continue

        m = re_magic.match(line)
        if m:
            major, minor, patch = m.groups()
            file.version = int(major) * 1000 + int(minor) * 100 + int(patch) * 10
            continue

        target_array = None
        if stack[-1] and stack[-1].is_array:
            target_array = stack[-1]

        if target_array:
            for v in re_float.findall(line):
                is_int = re_int.match(v)
                value = int(v) if is_int else float(v)
                if not target_array.is_array or target_array.array_explicit:
                    if is_int:
                        target_array.values.append(Value(VALUE_INTEGER, encode_int(value)))
                    else:
                        target_array.values.append(Value(VALUE_FLOAT, encode_float(value)))
                else:
                    if not is_int:
                        target_array.array_float = True
                    if target_array.array_min is None or value < target_array.array_min:
                        target_array.array_min = value
                    if target_array.array_max is None or value > target_array.array_max:
                        target_array.array_max = value
                    target_array.array_hash = hash((target_array.array_hash, hash(value)))
            continue

        m = re_name.match(line)
        if m:
            name, rest = m.groups()
            opens_scope = False
            if rest.endswith("{"):
                opens_scope = True
                rest = rest[:-1]
            values = [v.strip() for v in rest.split(",")]

            is_array = False
            array_size = 0
            if len(values) == 1 and values[0].startswith("*"):
                is_array = True
                array_size = int(values[0][1:])

            name_ix = string_to_index.get(name)
            if not name_ix:
                if opens_scope:
                    stack.append(None)
                continue

            values = [encode_value(v) for v in values]
            values = [v for v in values if v]
            field = Field(name_ix, values, is_array)

            if is_array and name == "KeyAttrRefCount":
                field.array_explicit = True
            field.array_size = min(array_size, 0xffff)
            if stack[-1]:
                stack[-1].fields.append(field)
            if opens_scope:
                stack.append(field)

    if file.version < 7000:
        def transform_to_arrays(field):
            if not field.is_array and len(field.values) > 8 and all(v.type in (VALUE_INTEGER, VALUE_FLOAT) for v in field.values):
                values = [decode_number(v) for v in field.values]
                field.is_array = True
                field.array_float = any(v.type == VALUE_FLOAT for v in field.values)
                field.array_min = min(values)
                field.array_max = max(values)
                field.array_size = min(len(values), 0xffff)
                field.array_hash = hash(tuple(values))
                field.values = []
            for child in field.fields:
                transform_to_arrays(child)
        transform_to_arrays(file.top_field)

    return file

INST_FIELD = 0x0
INST_ARRAY_INT = 0x1
INST_ARRAY_FLOAT = 0x2
INST_VALUE_STRING = 0x3
INST_VALUE_INT = 0x4
INST_VALUE_FLOAT = 0x5
INST_VALUE_CHAR = 0x6

def push_instruction(code, inst, level, *words):
    inst_word = level << 4 | inst
    inst_word |= (inst_word ^ 0xff) << 8
    code += struct.pack("<H", inst_word)
    for w in words:
        code += struct.pack("<H", w)

def encode_instructions(code, field, level):
    if field.is_array:
        hash = field.array_hash & 0xffff
        if field.array_explicit:
            push_instruction(code, INST_FIELD, level, field.name)
            for value in field.values:
                push_instruction(code, INST_VALUE_STRING + value.type, level + 1, value.value)
        elif field.array_float:
            min_v = encode_float(field.array_min)
            max_v = encode_float(field.array_max)
            push_instruction(code, INST_ARRAY_FLOAT, level, field.name, field.array_size, hash, min_v, max_v)
        else:
            min_v = encode_int(int(field.array_min))
            max_v = encode_int(int(field.array_max))
            push_instruction(code, INST_ARRAY_INT, level, field.name, field.array_size, hash, min_v, max_v)
    else:
        push_instruction(code, INST_FIELD, level, field.name)
        for value in field.values:
            push_instruction(code, INST_VALUE_STRING + value.type, level + 1, value.value)
        for child in field.fields:
            encode_instructions(code, child, level + 1)

flag_descs = [
    (0x00000003, 0x00000001, "UFBX_SPACE_CONVERSION_TRANSFORM_ROOT"),
    (0x00000003, 0x00000002, "UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS"),
    (0x00000003, 0x00000003, "UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY"),
    (0x0000000c, 0x00000004, "UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES"),
    (0x0000000c, 0x00000008, "UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY"),
    (0x0000000c, 0x0000000c, "UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY_NO_FALLBACK"),
    (0x00000030, 0x00000010, "UFBX_INHERIT_MODE_HANDLING_HELPER_NODES"),
    (0x00000030, 0x00000020, "UFBX_INHERIT_MODE_HANDLING_COMPENSATE"),
    (0x00000030, 0x00000030, "UFBX_INHERIT_MODE_HANDLING_IGNORE"),
    (0x000000c0, 0x00000040, "UFBX_MIRROR_AXIS_X"),
    (0x000000c0, 0x00000080, "UFBX_MIRROR_AXIS_Y"),
    (0x000000c0, 0x000000c0, "UFBX_MIRROR_AXIS_Z"),
    (0x00000100, 0x00000100, "ignore_geometry"),
    (0x00000200, 0x00000200, "ignore_animation"),
    (0x00000400, 0x00000400, "ignore_embedded"),
    (0x00000800, 0x00000800, "disable_quirks"),
    (0x00001000, 0x00001000, "strict"),
    (0x00002000, 0x00002000, "connect_broken_elements"),
    (0x00004000, 0x00004000, "allow_nodes_out_of_root"),
    (0x00008000, 0x00008000, "allow_missing_vertex_position"),
    (0x00010000, 0x00010000, "allow_empty_faces"),
    (0x00020000, 0x00020000, "generate_missing_normals"),
    (0x00040000, 0x00040000, "reverse_winding"),
    (0x00080000, 0x00080000, "normalize_normals"),
    (0x00100000, 0x00100000, "normalize_tangents"),
    (0x00200000, 0x00200000, "retain_dom"),
    (0x00c00000, 0x00400000, "UFBX_INDEX_ERROR_HANDLING_NO_INDEX"),
    (0x00c00000, 0x00800000, "UFBX_INDEX_ERROR_HANDLING_ABORT_LOADING"),
    (0x10000000, 0x10000000, "lefthanded"),
    (0x20000000, 0x20000000, "z_up"),
]
flag_values = { name: value for _, value, name in flag_descs }

def disassemble(code):
    n = 0

    print(f"{c_gray}[header]{c_def}")

    version = struct.unpack("<H", code[n:n+2])[0]
    print(f"{version:04x}{c_gray}[version: {version}]{c_def}")
    n += 2

    flags = struct.unpack("<I", code[n:n+4])[0]
    flag_str = ", ".join(name for mask, value, name in flag_descs if (flags & mask) == value)
    if not flag_str:
        flag_str = "0"
    print(f"{flags:08x}{c_gray}[flags: {flag_str}]{c_def}")
    n += 4

    temp_limit = struct.unpack("<I", code[n:n+4])[0]
    print(f"{temp_limit:08x}{c_gray}[temp_limit: {temp_limit}]{c_def}")
    n += 4

    result_limit = struct.unpack("<I", code[n:n+4])[0]
    print(f"{result_limit:08x}{c_gray}[result_limit: {result_limit}]{c_def}")
    n += 4

    print()
    print(f"{c_gray}[bytecode]{c_def}")

    while n < len(code):
        inst_word = struct.unpack("<H", code[n:n+2])[0]
        level = (inst_word >> 4) & 0xf
        inst = inst_word & 0xf
        if inst == INST_FIELD:
            name = struct.unpack("<H", code[n+2:n+4])[0]
            inst_str = f"{inst_word:04x}{c_gray}[{level} INST_FIELD]{c_def}"
            print(f"{inst_str:<38} {dump_str(name)}")
            n += 4
        elif INST_ARRAY_INT <= inst <= INST_ARRAY_FLOAT:
            name, size, hash, min_v, max_v = struct.unpack("<HHHHH", code[n+2:n+12])
            if inst == INST_ARRAY_INT:
                inst_str = f"{inst_word:04x}{c_gray}[{level} INST_ARRAY_INT]{c_def}"
                print(f"{inst_str:<38} {dump_str(name)} {size:04x}{c_gray}[{size}]{c_def} {hash:04x}{c_gray}[{hash}]{c_def} {dump_int(min_v)} {dump_int(max_v)}")
            elif inst == INST_ARRAY_FLOAT:
                inst_str = f"{inst_word:04x}{c_gray}[{level} INST_ARRAY_FLOAT]{c_def}"
                print(f"{inst_str:<38} {dump_str(name)} {size:04x}{c_gray}[{size}]{c_def} {hash:04x}{c_gray}[{hash}]{c_def} {dump_float(min_v)} {dump_float(max_v)}")
            n += 12
        elif INST_VALUE_STRING <= inst <= INST_VALUE_CHAR:
            value = struct.unpack("<H", code[n+2:n+4])[0]
            if inst == INST_VALUE_STRING:
                inst_str = f"{inst_word:04x}{c_gray}[{level} INST_VALUE_STRING]{c_def}"
                print(f"{inst_str:<38} {dump_str(value)}")
            elif inst == INST_VALUE_INT:
                inst_str = f"{inst_word:04x}{c_gray}[{level} INST_VALUE_INT]{c_def}"
                print(f"{inst_str:<38} {dump_int(value)}")
            elif inst == INST_VALUE_FLOAT:
                inst_str = f"{inst_word:04x}{c_gray}[{level} INST_VALUE_FLOAT]{c_def}"
                print(f"{inst_str:<38} {dump_float(value)}")
            elif inst == INST_VALUE_CHAR:
                inst_str = f"{inst_word:04x}{c_gray}[{level} INST_VALUE_CHAR]{c_def}"
                print(f"{inst_str:<38} {dump_char(value)}")
            n += 4
        else:
            assert False

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Input filename")
    parser.add_argument("-o", type=str, help="Output path")
    parser.add_argument("-f", default=[], action="append", help="Specify flags")
    parser.add_argument("--temp-limit", type=int, help="Temporary allocation limit")
    parser.add_argument("--result-limit", type=int, help="Result allocation limit")
    parser.add_argument("--dump", action="store_true", help="Dump output")
    parser.add_argument("--disassemble", action="store_true", help="Disassemble output")
    argv = parser.parse_args()

    with open(argv.input, "rt", encoding="utf-8") as f:
        result = encode_file(f)

        for flag in argv.f:
            result.flags |= flag_values[flag]
        result.temp_limit = min(max(argv.temp_limit or 0, 0), 0xffff)
        result.result_limit = min(max(argv.result_limit or 0, 0), 0xffff)

        if argv.dump:
            print(dump_field(result.top_field))
        code = bytearray()
        code += struct.pack("<HIII", result.version, result.flags, result.temp_limit, result.result_limit)
        for field in result.top_field.fields:
            encode_instructions(code, field, 0)
        code = bytes(code)
        if argv.disassemble:
            disassemble(code)
        if argv.o:
            with open(argv.o, "wb") as wf:
                wf.write(code)
