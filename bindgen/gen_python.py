import os
from typing import NamedTuple, Optional, List
import ufbx_ir as ir
import json

g_indent = 0
g_outfile = None

prologue = """
import ctypes as ct
from typing import Iterator, NamedTuple, Optional, Union, List, Tuple, BinaryIO
from enum import IntEnum, IntFlag
from array import array

p_size_t = ct.POINTER(ct.c_size_t)
p_bool = ct.POINTER(ct.c_bool)
p_int8_t = ct.POINTER(ct.c_int8)
p_uint8_t = ct.POINTER(ct.c_uint8)
p_int16_t = ct.POINTER(ct.c_int16)
p_uint16_t = ct.POINTER(ct.c_uint16)
p_int32_t = ct.POINTER(ct.c_int32)
p_uint32_t = ct.POINTER(ct.c_uint32)
p_int64_t = ct.POINTER(ct.c_int64)
p_uint64_t = ct.POINTER(ct.c_uint64)
p_ptrdiff_t = ct.POINTER(ct.c_ssize_t)
p_size_t = ct.POINTER(ct.c_size_t)
p_float = ct.POINTER(ct.c_float)
p_double = ct.POINTER(ct.c_double)
p_string = ct.c_char_p
p_void = ct.c_void_p
p_ptr = ct.POINTER(p_void)

cast = ct.cast

def _load_string(ptr):
    addr = cast(ptr, p_ptr)[0]
    size = cast(ptr + 8, p_size_t)[0]
    return ct.string_at(addr, size).decode("utf-8")

def _store_string(retain, ptr, val):
    encoded = val.encode("utf-8")
    retain.append(encoded)
    cast(ptr, p_ptr)[0] = cast(ct.create_string_buffer(encoded), p_void)
    cast(ptr + 8, p_size_t)[0] = len(encoded)

def _load_bytes(ptr):
    addr = cast(ptr, p_ptr)[0]
    size = cast(ptr + 8, p_size_t)[0]
    return ct.string_at(addr, size)

def _check_index(index, count):
    if not isinstance(index, int): raise TypeError(f"Index must be int or slice, not {type(index)}")
    if index < 0: index += count
    if index < 0 or index >= count: raise IndexError(f"Index {index} out of range, maximum {count}")

class _ContextFlag:
    def __init__(self):
        self.valid = True

class _Context:
    def __init__(self, parent):
        self.released = False
        self.flag = _ContextFlag()
        self.flags = [self.flag]
        p = parent
        while p:
            p.flags.append(self.flag)
            p = p.parent
        self.parent = parent

    def release(self):
        if self.released: return
        self.released = True
        for flag in self.flags:
            flag.valid = False

    def __del__(self):
        self.release()

class _ErrorContext(_Context):
    def __init__(self):
        super().__init__(None)

class _SceneContext(_Context):
    def __init__(self, parent, cufbx, scene_ptr):
        super().__init__(parent)
        self.cufbx = cufbx
        self.scene_ptr = scene_ptr

    def release(self):
        if self.released: return
        super().release()

        self.cufbx.ufbx_free_scene(self.scene_ptr)

class _MeshContext(_Context):
    def __init__(self, parent, cufbx, mesh_ptr):
        super().__init__(parent)
        self.cufbx = cufbx
        self.mesh_ptr = mesh_ptr

    def release(self):
        if self.released: return
        super().release()

        self.cufbx.ufbx_free_mesh(self.mesh_ptr)

class _Base:
    def __init__(self, ctx):
        if not isinstance(ctx, _Context):
            raise TypeError(f"Expected ctx to be _Context, got {type(ctx)}")
        self._ctx = ctx
    
    def _check(self):
        if not self._ctx.flag.valid:
            raise RuntimeError("Operating on closed file")

class UintArray:
    def __init__(self, arr):
        self._arr = arr

    def __getitem__(self, index: Union[int, slice]):
        if isinstance(index, slice):
            return UintArray(self._arr[index])
        elif isinstance(index, int):
            return self._arr(index)
        else:
            raise TypeError(f"Bad index type {type(index).__name__}")

    def __iter__(self):
        return iter(self._arr)

def _make_UintArray(retain, num):
    # TODO: BAD
    arr = UintArray(array("I", bytearray(4 * num)))
    return arr._arr.buffer_info()[0], arr

class DoubleArray:
    def __init__(self, arr):
        self._arr = arr

    def __getitem__(self, index: Union[int, slice]):
        if isinstance(index, slice):
            return DoubleArray(self._arr[index])
        elif isinstance(index, int):
            return self._arr(index)
        else:
            raise TypeError(f"Bad index type {type(index).__name__}")

def _make_DoubleArray(retain, num):
    # TODO: BAD
    arr = DoubleArray(array("d", bytearray(8 * num)))
    return arr._arr.buffer_info()[0], arr

class Vec3Array:
    def __init__(self, arr):
        self._arr = arr

    def __getitem__(self, index: Union[int, slice]):
        if isinstance(index, slice):
            start = slice.start * 3 if slice.start else None
            stop = slice.stop * 3 if slice.stop else None
            step = slice.step * 3 if slice.step else None
            return Vec3Array(self._arr[slice(start, stop, step)])
        elif isinstance(index, int):
            x = self._arr(index*3 + 0)
            y = self._arr(index*3 + 1)
            z = self._arr(index*3 + 2)
            return Vec3(x, y, z)
        else:
            raise TypeError(f"Bad index type {type(index).__name__}")

def _make_Vec3Array(retain, num):
    # TODO: BAD
    arr = Vec3Array(array("d", bytearray(24 * num)))
    return arr._arr.buffer_info()[0], arr

""".strip()

epilogue = """

cufbx = ct.cdll.LoadLibrary(".\\\\ufbx.dll")

setup_ffi(cufbx)

def _alloc(retain, size):
    uints = (size + 7) // 8
    arr = (ct.c_uint64 * uints)()
    retain.append(arr)
    return cast(ct.pointer(arr), p_void)

def _raise_error(err):
    raise error_types[err.type](err.description)

def _raise_error_ptr(p_error):
    err_ctx = _ErrorContext()
    err = _Error(err_ctx, p_error.value)
    _raise_error(err)

def _make_pod(retain, typ):
    return _alloc(retain, typ._size)

def _check_ref(val, typ, name):
    if not isinstance(val, typ):
        raise TypeError(f"Expected '{name}' to be {typ.__name__}, got {type(val).__name__}")
    return val.ptr

def _check_string(retain, val, name):
    if isinstance(val, str):
        buf = val.encode("utf-8")
    elif isinstance(val, bytes):
        buf = val
    else:
        raise TypeError(f"Expected '{name}' to be str or bytes, got {type(val).__name__}")
    ptr = ct.create_string_buffer(buf)
    retain.append(ptr)
    return ptr, len(buf)

def _check_pod(retain, val, typ, name):
    if not isinstance(val, typ):
        raise TypeError(f"Expected {name} to be {typ.__name__}, got {type(val).__name__}")
    data = _alloc(retain, typ._size)
    typ._store(retain, data.value, val)
    return data

def _check_input(retain, val, typ, name):
    data = _alloc(retain, typ._size)
    if val:
        if not isinstance(val, typ):
            raise TypeError(f"Expected {name} to be {typ.__name__}, got {type(val).__name__}")
        typ._store(retain, data.value, val)
    return data

def _check_enum(val, typ, name):
    if not isinstance(val, typ):
        raise TypeError(f"Expected '{name}' to be {typ.__name__}, got {type(val).__name__}")
    return val.value

def _check_int(val, name):
    if not isinstance(val, int):
        raise TypeError(f"Expected {name} to be int, got {type(val).__name__}")
    return val

def _check_float(val, name):
    if not isinstance(val, float):
        raise TypeError(f"Expected {name} to be float, got {type(val).__name__}")
    return val

def _check_bool(val, name):
    if not isinstance(val, bool):
        raise TypeError(f"Expected {name} to be bool, got {type(val).__name__}")
    return bool(val)

""".strip()

extras = { }

extras["ufbx_scene"] = """

def __enter__(self):
    return self

def __exit__(self, *exc):
    self.release()
    return False

def release(self):
    self._ctx.release()

""".strip()

extras["ufbx_mesh"] = """

def __enter__(self):
    return self

def __exit__(self, *exc):
    self.release()
    return False

def release(self):
    if self._ctx is MeshContext:
        self._ctx.release()

""".strip()

extras["ufbx_vertex_real"] = """
def __getitem__(self, index: int) -> float:
    return self.values[self.indices[index]]
"""

extras["ufbx_vertex_vec2"] = """
def __getitem__(self, index: int) -> Vec2:
    return self.values[self.indices[index]]
"""

extras["ufbx_vertex_vec3"] = """
def __getitem__(self, index: int) -> Vec3:
    return self.values[self.indices[index]]
"""

extras["ufbx_vertex_vec4"] = """
def __getitem__(self, index: int) -> Vec4:
    return self.values[self.indices[index]]
"""

extras["ufbx_anim_curve"] = """
def evaluate(self, time: float, default: float = 0.0):
    return evaluate_curve(self, time, default)
"""

py_type = {
    "ufbx_string": "str",
    "ufbx_blob": "bytes",
    "bool": "bool",
    "int8_t": "int",
    "uint8_t": "int",
    "int16_t": "int",
    "uint16_t": "int",
    "int32_t": "int",
    "uint32_t": "int",
    "int64_t": "int",
    "uint64_t": "int",
    "ptrdiff_t": "int",
    "size_t": "int",
    "float": "float",
    "double": "float",
}

def emit(line=""):
    global g_indent
    global g_outfile
    if line:
        print("    " * g_indent + line, file=g_outfile)
    else:
        print("", file=g_outfile)

def indent(delta=1):
    global g_indent
    g_indent += delta

def outdent(delta=1):
    global g_indent
    g_indent -= delta

ignore_decls = {
    "ufbx_string",
    "ufbx_blob",
}

load_funcs = {
    "str": "_load_string",
    "bytes": "_load_bytes",
}

store_funcs = {
    "str": "_store_string",
}

array_types = {
    "uint32_t": "UintArray",
    "double": "DoubleArray",
    "ufbx_vec3": "Vec3Array",
}

py_defined = set()

class ArrayInfo(NamedTuple):
    length: int
    stride: int
    elem_py_type: str

class ResolvedType(NamedTuple):
    type: ir.Type
    py_type: str
    array_info: Optional[ArrayInfo]
    pointers: int

def emit_lines(extra: str):
    for line in extra.splitlines():
        emit(line)

def emit_load(addr: str, res: ResolvedType, prefix="return ", check_null=False):
    if res.array_info:
        arr = res.array_info
        in_res = res._replace(py_type=arr.elem_py_type, array_info=None)
        emit(f"arr = [None] * {arr.length}")
        emit(f"arr_addr = {addr}")
        emit(f"for n in range({arr.length}):")
        indent()
        emit_load("arr_addr", in_res, prefix="arr[n] = ")
        emit(f"arr_addr += {arr.stride}")
        outdent()
        emit(f"{prefix}arr")
        return

    base = res.type.base_name

    for _ in range(res.pointers):
        if check_null:
            emit(f"ptr = cast({addr}, p_ptr)[0] if {addr} else None")
        else:
            emit(f"ptr = cast({addr}, p_ptr)[0]")
        addr = "ptr"
        check_null = True

    suffix = ""
    if check_null:
        suffix = f" if {addr} else None"

    if res.py_type in load_funcs:
        func = load_funcs[res.py_type]
        emit(f"{prefix}{func}({addr}){suffix}")
    elif res.type.kind == "enum":
            emit(f"{prefix}{res.py_type}(cast({addr}, p_uint32_t)[0]){suffix}")
    elif res.type.kind == "struct":
        if res.type.base_name == "ufbx_element":
            emit(f"{prefix}_load_element(self._ctx, {addr}){suffix}")
        else:
            emit(f"{prefix}{res.py_type}(self._ctx, {addr}){suffix}")
    else:
        emit(f"{prefix}cast({addr}, p_{base})[0]{suffix}")

def emit_store(addr: str, res: ResolvedType, value, retain="retain"):
    base = res.type.base_name

    emit(f"if {value} is not None:")
    indent()

    if res.py_type in store_funcs:
        func = store_funcs[res.py_type]
        emit(f"{func}({retain}, {addr}, {value})")
    elif res.type.kind == "enum":
        emit(f"cast({addr}, p_uint32_t)[0] = {value}.value")
    elif res.type.kind == "struct":
        emit(f"{value}._store({retain}, {addr})")
    else:
        emit(f"cast({addr}, p_{base})[0] = {value}")

    outdent()

def emit_load_ptr(offset: int, res: ResolvedType):
    emit_load(f"self.ptr + {offset}", res)

def resolve_py_type(file: ir.File, typ: ir.Type) -> Optional[ResolvedType]:
    py = py_type.get(typ.base_name)
    if py: return ResolvedType(typ, py, None, 0)

    if typ.kind == "struct":
        st = file.structs[typ.base_name]
        return ResolvedType(typ, ir.to_pascal(st.short_name), None, 0)

    if typ.kind == "enum":
        en = file.enums[typ.base_name]
        return ResolvedType(typ, ir.to_pascal(en.short_name), None, 0)

    if typ.kind == "pointer":
        inner = file.types[typ.inner]
        res = resolve_py_type(file, inner)
        if not res: return None
        return res._replace(pointers=res.pointers+1)

    if typ.kind == "array":
        inner = file.types[typ.inner]
        res = resolve_py_type(file, inner)
        if not res: return None
        py = f"List[{res.py_type}]"
        info = ArrayInfo(typ.array_length, inner.size["x64"], res.py_type)
        return res._replace(py_type=py, array_info=info)

    if typ.kind == "typedef":
        it = file.types[typ.inner]
        return resolve_py_type(file, it)

def fmt_type(py):
    if py in ("str", "bytes", "float", "int", "bool"):
        return py
    if py in py_defined:
        return py
    return f"\"{py}\""

def emit_field(file: ir.File, field: ir.Field, base: int = 0):
    if field.name == "":
        ist = file.structs[field.type]
        for ifield in ist.fields:
            emit_field(file, ifield, base + field.offset["x64"])
        return

    if field.private:
        return

    typ = file.types[field.type]
    res = resolve_py_type(file, typ)
    if not res:
        return

    emit()
    emit("@property")
    emit(f"def {field.name}(self) -> {fmt_type(res.py_type)}:")
    indent()
    emit("self._check()")
    emit_load_ptr(base + field.offset["x64"], res)
    outdent()

def emit_list(file: ir.File, st: ir.Struct):
    name = ir.to_pascal(st.short_name)
    emit()
    emit(f"class {name}(_Base):")
    indent()
    emit("def __init__(self, ctx, ptr):")
    indent()
    emit("super().__init__(ctx)")
    emit("self.ptr = ptr")
    emit(f"self.data = cast(self.ptr, p_ptr)[0]")
    emit(f"self.count = cast(self.ptr + 8, p_size_t)[0]")
    outdent()

    elem_type = file.types[file.types[st.fields[0].type].inner]
    res = resolve_py_type(file, elem_type)
    assert res

    elem_sz = elem_type.size["x64"]

    emit("def __len__(self) -> int:")
    indent()
    emit("return self.count")
    outdent()

    emit(f"def __getitem__(self, index: int) -> {fmt_type(res.py_type)}:")
    indent()
    emit("self._check()")
    emit("_check_index(index, self.count)")
    addr = f"self.data + index * {elem_sz}"
    emit_load(addr, res)
    outdent()

    emit(f"def __iter__(self) -> Iterator[{fmt_type(res.py_type)}]:")
    indent()
    emit("index = 0")
    emit("while index < self.count:")
    indent()
    emit("self._check()")
    addr = f"self.data + index * {elem_sz}"
    emit_load(addr, res, prefix="yield ")
    emit("index += 1")
    outdent()
    outdent()

    outdent()
    py_defined.add(name)

class UsedField(NamedTuple):
    field: ir.Field
    offset: int

def emit_pod_field(file: ir.File, field: ir.Field, used_fields: List[UsedField], base: int = 0):
    if field.name == "":
        ist = file.structs[field.type]
        for ifield in ist.fields:
            if ist.is_union and not ifield.union_preferred: continue
            emit_pod_field(file, ifield, used_fields, base + field.offset["x64"])
        return

    typ = file.types[field.type]
    res = resolve_py_type(file, typ)
    assert res

    offset = base + field.offset["x64"]
    used_fields.append(UsedField(field, offset))
    emit(f"{field.name}: {fmt_type(res.py_type)}")

def emit_pod(file: ir.File, st: ir.Struct):
    name = ir.to_pascal(st.short_name)
    emit()
    emit(f"class {name}(NamedTuple):")
    indent()

    used_fields = []
    for field in st.fields:
        emit_pod_field(file, field, used_fields)

    outdent()

    emit()
    emit(f"def _load_{name}(addr):")
    indent()

    for uf in used_fields:
        typ = file.types[uf.field.type]
        res = resolve_py_type(file, typ)
        assert uf.field.name not in ("ptr", "addr")
        addr = f"addr + {uf.offset}"
        prefix = f"{uf.field.name} = "
        emit_load(addr, res, prefix=prefix)

    args = ", ".join(uf.field.name for uf in used_fields)
    emit(f"return {name}({args})")

    outdent()

    emit()
    emit(f"def _store_{name}(retain, addr, val):")
    indent()

    for uf in used_fields:
        typ = file.types[uf.field.type]
        res = resolve_py_type(file, typ)
        assert uf.field.name not in ("ptr", "addr")
        addr = f"addr + {uf.offset}"
        val = f"val.{uf.field.name}"
        emit_store(addr, res, val)

    outdent()

    typ = file.types[st.name]
    size = typ.size["x64"]
    emit()
    emit(f"{name}._size = {size}")
    emit(f"{name}._load = _load_{name}")
    emit(f"{name}._store = _store_{name}")

    load_funcs[name] = f"_load_{name}"
    store_funcs[name] = f"_store_{name}"
    py_defined.add(name)

def emit_input_field(file: ir.File, field: ir.Field, used_fields: List[UsedField], base: int = 0):
    if field.private: return
    assert field.name

    typ = file.types[field.type]
    res = resolve_py_type(file, typ)
    if not res: return

    offset = base + field.offset["x64"]
    used_fields.append(UsedField(field, offset))

    emit(f"{field.name}: {fmt_type(res.py_type)}")

def emit_input(file: ir.File, st: ir.Struct):
    name = ir.to_pascal(st.short_name)
    emit()
    emit(f"class {name}:")
    indent()

    size = file.types[st.name].size["x64"]
    emit(f"_size = {size}")
    emit()

    used_fields = []
    for field in st.fields:
        emit_input_field(file, field, used_fields)

    emit()
    
    emit(f"def __init__(self, *,")
    indent(2)
    for ix, uf in enumerate(used_fields):
        suffix = ","
        if ix + 1 == len(used_fields):
            suffix = "):"
        
        field = uf.field
        default = None
        if field.type == "ufbx_allocator":
            default = "Allocator()"

        emit(f"{uf.field.name}={default}{suffix}")

    outdent(2)

    indent()
    for uf in used_fields:
        emit(f"self.{uf.field.name} = {uf.field.name}")
    outdent()

    emit()
    emit("def _store(self, retain, addr):")
    indent()
    for uf in used_fields:
        addr = f"addr + {uf.offset}"
        val = f"self.{uf.field.name}"
        typ = file.types[uf.field.type]
        res = resolve_py_type(file, typ)
        emit_store(addr, res, val)
        
    outdent()
    
    outdent()

    py_defined.add(name)

def emit_struct(file: ir.File, st: ir.Struct):
    if st.is_list: return emit_list(file, st)
    if st.is_pod: return emit_pod(file, st)
    if st.is_input: return emit_input(file, st)

    base = "_Base"
    if st.is_element:
        base = "Element"

    name = ir.to_pascal(st.short_name)
    if name == "Error":
        name = "_Error"

    emit()
    emit(f"class {name}({base}):")
    indent()

    size = file.types[st.name].size["x64"]
    emit(f"_size = {size}")
    emit()

    emit("def __init__(self, ctx, ptr):")
    indent()
    if st.is_element:
        emit("super().__init__(ctx, ptr)")
    else:
        emit("super().__init__(ctx)")
        emit("self.ptr = ptr")
    outdent()

    for field in st.fields:
        emit_field(file, field)

    if st.name in extras:
        emit()
        emit_lines(extras[st.name])

    outdent()
    py_defined.add(name)

def emit_enum(file: ir.File, en: ir.Enum):
    base = "IntFlag" if en.flag else "IntEnum"
    name = ir.to_pascal(en.short_name)
    emit()
    emit(f"class {name}({base}):")
    indent()

    for val in en.values:
        ev = file.enum_values[val]
        if en.flag:
            emit(f"{ev.short_name} = 0x{ev.value:x}")
        else:
            emit(f"{ev.short_name} = {ev.value}")

    outdent()

def ffi_type(file: ir.File, type: ir.Type):
    if type.kind == "pointer":
        inner = file.types[type.inner]
        if inner.kind == "" and inner.base_name == "char":
            return "p_string"
        else:
            return "p_void"
    elif type.kind == "typedef":
        inner = file.types[type.inner]
        return ffi_type(file, inner)
    elif type.kind == "":
        types = {
            "void": "None",
            "size_t": "ct.c_size_t",
            "bool": "ct.c_bool",
            "int8_t": "ct.c_int8",
            "uint8_t": "ct.c_uint8",
            "int16_t": "ct.c_int16",
            "uint16_t": "ct.c_uint16",
            "int32_t": "ct.c_int32",
            "uint32_t": "ct.c_uint32",
            "int64_t": "ct.c_int64",
            "uint64_t": "ct.c_uint64",
            "ptrdiff_t": "ct.c_ssize_t",
            "size_t": "ct.c_size_t",
            "float": "ct.c_float",
            "double": "ct.c_double",
        }
        return types.get(type.base_name)

def emit_ffi_prototype(file: ir.File, func: ir.Function):
    if func.is_inline: return

    ret = ffi_type(file, file.types[func.return_type])
    args = [ffi_type(file, file.types[arg.type]) for arg in func.arguments]
    if ret is None or any(a is None for a in args): return

    args_str = ", ".join(args)

    emit()
    emit(f"cufbx.{func.name}.argtypes = [{args_str}]")
    emit(f"cufbx.{func.name}.restype = {ret}")

class FuncArg(NamedTuple):
    name: str
    py_name: str
    kind: str
    py_type: str
    py_hint: str
    py_default: Optional[str]

def safe_name(name: str):
    if name == "def":
        return "default"
    else:
        return name

non_ffi_functions = {
    "ufbx_find_int_len",
}

def get_function_args(file: ir.File, func: ir.Function):
    args = []
    ret_py = "None"

    typ = file.types[func.return_type]
    ret_res = resolve_py_type(file, typ)
    if ret_res:
        ret_py = ret_res.py_type
    has_retval = False

    for arg in func.arguments:
        py_name = safe_name(arg.name)
        if arg.is_return:
            inner = file.types[file.types[arg.type].inner]
            ret_res = resolve_py_type(file, inner)
            if ret_res:
                ret_py = ret_res.py_type
            has_retval = True
        else:
            atyp = file.types[arg.type]
            if arg.by_ref:
                atyp = file.types[atyp.inner]
            ares = resolve_py_type(file, atyp)
            if arg.kind == "stringPointer":
                args.append(FuncArg(arg.name, py_name, "string", "str", "Union[str, bytes]", None))
            elif arg.kind == "stringLength":
                pass
            elif arg.kind == "arrayPointer":
                pass
            elif arg.kind == "arrayLength":
                pass
            elif arg.kind == "blobPointer":
                args.append(FuncArg(arg.name, py_name, "blob", "bytes", "bytes", None))
                pass
            elif arg.kind == "blobSize":
                pass
            elif arg.kind == "error":
                pass
            elif arg.kind == "stream":
                args.append(FuncArg(arg.name, py_name, "stream", "BinaryIO", "BinaryIO", None))
            elif arg.kind == "pod":
                args.append(FuncArg(arg.name, py_name, "pod", ares.py_type, ares.py_type, None))
            elif arg.kind == "input":
                args.append(FuncArg(arg.name, py_name, "input", ares.py_type, f"Optional[{ares.py_type}]", "None"))
            elif arg.kind == "prim":
                args.append(FuncArg(arg.name, py_name, "prim", ares.py_type, ares.py_type, None))
            elif arg.kind == "ref":
                args.append(FuncArg(arg.name, py_name, "ref", ares.py_type, ares.py_type, None))
            elif arg.kind == "enum":
                args.append(FuncArg(arg.name, py_name, "enum", ares.py_type, ares.py_type, None))
            else:
                raise RuntimeError(f"Unhandled arg kind {arg.kind}")

    return args, ret_py, has_retval

def emit_function(file: ir.File, func: ir.Function):
    name = func.pretty_name
    if func.is_inline: return
    if func.is_ffi: return
    if any(not a.kind for a in func.arguments): return
    if func.ffi_name and func.name not in non_ffi_functions:
        func = file.functions[func.ffi_name]
    if not func.return_kind: return

    args, ret_py, has_retval = get_function_args(file, func)

    emit()

    def fmt_arg(arg: FuncArg):
        if arg.py_default:
            return f"{arg.py_name}: {arg.py_hint} = {arg.py_default}"
        else:
            return f"{arg.py_name}: {arg.py_hint}"

    arg_str = ", ".join(fmt_arg(a) for a in args)

    ret_py_hint = ret_py
    arr_py_type = None

    if func.return_array_scale:
        ptr_arg = func.arguments[func.array_arguments[0].pointer_index]
        arr_type = file.types[file.types[ptr_arg.type].inner]
        if arr_type.kind == "typedef":
            arr_type = file.types[arr_type.inner]
        arr_py_type = array_types[arr_type.key]
        ret_py_hint = arr_py_type
    elif func.return_kind == "ref":
        ret_py_hint = f"Optional[{ret_py}]"

    emit(f"def {name}({arg_str}) -> {ret_py_hint}:")
    indent()
    emit("retain = []")
    for arg in args:
        if arg.kind == "string":
            emit(f"c_{arg.name}, c_{arg.name}_len = _check_string(retain, {arg.py_name}, \"{arg.py_name}\")")
        elif arg.kind == "pod":
            emit(f"c_{arg.name} = _check_pod(retain, {arg.py_name}, {arg.py_type}, \"{arg.py_name}\")")
        elif arg.kind == "input":
            emit(f"c_{arg.name} = _check_input(retain, {arg.py_name}, {arg.py_type}, \"{arg.py_name}\")")
        elif arg.kind == "ref":
            emit(f"c_{arg.name} = _check_ref({arg.py_name}, {arg.py_type}, \"{arg.py_name}\")")
        elif arg.kind == "enum":
            emit(f"c_{arg.name} = _check_enum({arg.py_name}, {arg.py_type}, \"{arg.py_name}\")")
        elif arg.kind == "prim":
            emit(f"c_{arg.name} = _check_{arg.py_type}({arg.py_name}, \"{arg.py_name}\")")
    if has_retval:
        if func.return_kind == "pod":
            emit(f"c_retval = _make_pod(retain, {ret_py})")
    if func.has_error:
        emit(f"c_error = _make_pod(retain, _Error)")

    for aa in func.array_arguments:
        arg_ptr = func.arguments[aa.pointer_index]
        arg_num = func.arguments[aa.num_index]
        
        len_getter_name = func.name.replace("_ffi_", "_").replace("ufbx_", "ufbx_get_") + "_" + arg_num.name
        len_getter = file.functions.get(len_getter_name)
        if len_getter:
            if func.is_ffi and len_getter.ffi_name:
                len_getter = file.functions[len_getter.ffi_name]
            len_c_args = ", ".join(f"c_{a.name}" for a in len_getter.arguments)
            emit(f"c_{arg_num.name} = cufbx.{len_getter.name}({len_c_args})")
            emit(f"c_{arg_ptr.name}, ret_arr = _make_{arr_py_type}(retain, c_{arg_num.name})")

    for ba in func.blob_arguments:
        arg_ptr = func.arguments[ba.pointer_index]
        arg_size = func.arguments[ba.size_index]
        emit(f"c_{arg_size.name} = len({ba.name})")
        emit(f"c_{arg_ptr.name} = ct.create_string_buffer({ba.name})")

    c_args = ", ".join(f"c_{a.name}" for a in func.arguments)
    if has_retval:
        emit(f"cufbx.{func.name}({c_args})")
    else:
        emit(f"ret = cufbx.{func.name}({c_args})")

    if func.return_array_scale:
        if func.return_array_scale != 1:
            emit(f"return ret_arr[:ret * {func.return_array_scale}]")
        else:
            emit(f"return ret_arr[:ret]")
    elif func.return_kind == "prim":
        assert not has_retval
        emit(f"return ret")
    elif func.return_kind == "pod":
        assert has_retval
        emit(f"return _load_{ret_py}(c_retval)")
    elif func.return_kind == "ref":
        emit("if not ret:")
        indent()
        if func.has_error:
            emit("_raise_error_ptr(c_error)")
        else:
            emit("return None")
        outdent()
        ctx_name = "None"
        for arg in args:
            if arg.kind == "ref":
                ctx_name = f"{arg.py_name}._ctx"
                break
        if func.alloc_type:
            if func.alloc_type == "scene":
                emit(f"ctx = _SceneContext({ctx_name}, cufbx, ret)")
            elif func.alloc_type == "mesh":
                emit(f"ctx = _MeshContext({ctx_name}, cufbx, ret)")
            else:
                raise RuntimeError(f"Unhandled alloc_type {func.alloc_type}")
            ctx_name = "ctx"
        assert not has_retval
        emit(f"return {ret_py}({ctx_name}, ret)")
    elif func.return_kind == "enum":
        assert not has_retval
        emit(f"return {ret_py}(ret)")
    else:
        raise RuntimeError(f"Unhandled return_kind {func.return_kind}")

    outdent()

def emit_file(file: ir.File):
    emit(prologue)
    emit()

    for decl in file.declarations:
        if decl.name in ignore_decls: continue
        if decl.kind == "struct":
            st = file.structs[decl.name]
            emit_struct(file, st)
        elif decl.kind == "enum":
            en = file.enums[decl.name]
            emit_enum(file, en)

    emit()
    emit("element_types = [")
    indent()
    for ename in file.element_types:
        st = file.structs[ename]
        name = ir.to_pascal(st.short_name)
        emit(f"{name},")
    outdent()
    emit("]")

    error_en = file.enums["ufbx_error_type"]

    emit()
    emit(f"class Error(Exception):")
    indent()
    emit("pass")
    outdent()

    emit()
    for vname in error_en.values:
        ev = file.enum_values[vname]
        if ev.name in ("UFBX_ERROR_NONE"):
            continue
        name = ir.to_pascal(ev.short_name) + "Error"
        emit(f"class {name}(Error):")
        indent()
        emit("pass")
        outdent()

    emit()
    emit("error_types = [")
    indent()
    for vname in error_en.values:
        ev = file.enum_values[vname]
        name = ir.to_pascal(ev.short_name) + "Error"
        if name == "NoneError":
            name = "UnknownError"
        emit(f"{name},")
    outdent()
    emit("]")

    element_st = file.structs["ufbx_element"]
    type_field = next(f for f in element_st.fields if f.name == "type")
    type_offset = type_field.offset["x64"]

    emit()
    emit("def _load_element(ctx, ptr):")
    indent()
    emit(f"etype = cast(ptr + {type_offset}, p_uint32_t)[0]")
    emit(f"return element_types[etype](ctx, ptr)")
    outdent()

    emit()
    emit("def setup_ffi(cufbx):")
    indent()
    for func in file.functions.values():
        emit_ffi_prototype(file, func)

    outdent()

    emit()
    emit(epilogue)
    emit()

    emit()
    for decl in file.declarations:
        if decl.kind != "function": continue
        func = file.functions[decl.name]
        emit_function(file, func)


if __name__ == "__main__":
    src_path = os.path.dirname(os.path.realpath(__file__))
    path = os.path.join(src_path, "build", "ufbx_typed.json")
    with open(path, "rt") as f:
        js = json.load(f)
    file = ir.from_json(ir.File, js)

    src_path = os.path.dirname(os.path.realpath(__file__))
    dst_path = os.path.join(src_path, "build", "ufbx_ffi.py")
    with open(dst_path, "wt") as f:
        g_outfile = f
        emit_file(file)
