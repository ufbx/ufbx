import os
from re import I
from typing import NamedTuple, Optional, List
import ufbx_ir as ir
import json

g_indent = 0
g_outfile = None

prologue = """
import ctypes as ct
from typing import Iterator, NamedTuple, Optional, Union, List, Tuple
from enum import IntEnum, IntFlag

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

def load_string(ptr):
    addr = cast(ptr, p_ptr)[0]
    size = cast(ptr + 8, p_size_t)[0]
    return ct.string_at(addr, size).decode("utf-8")

def store_string(retain, ptr, val):
    encoded = val.encode("utf-8")
    retain.append(encoded)
    cast(ptr, p_ptr)[0] = cast(ct.create_string_buffer(encoded), p_void)
    cast(ptr + 8, p_size_t)[0] = len(encoded)

def load_bytes(ptr):
    addr = cast(ptr, p_ptr)[0]
    size = cast(ptr + 8, p_size_t)[0]
    return ct.string_at(addr, size)

def check_index(index, count):
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

""".strip()

epilogue = """

cufbx = ct.cdll.LoadLibrary("ufbx.dll")

setup_ffi(cufbx)

def _alloc(retain, size):
    uints = (size + 7) // 8
    arr = (ct.c_uint64 * uints)()
    retain.append(arr)
    return cast(ct.pointer(arr), p_void)

def _alloc_str(retain, val, name=None):
    if isinstance(val, str):
        buf = val.encode("utf-8")
    elif isinstance(val, bytes):
        buf = val
    else:
        ctx = f"for '{name}'" if name else None
        raise TypeError(f"Expected str or bytes{ctx}, got {type(val).__name__}")
    val = ct.create_string_buffer(buf)
    retain.append(val)
    return val, len(buf)

def _alloc_obj(retain, typ, val=None, name=None):
    data = _alloc(retain, typ._size)
    if val:
        if not isinstance(val, typ):
            ctx = f"for '{name}'" if name else None
            raise TypeError(f"Expected {typ.__name__}{ctx}, got {type(val).__name__}")
        val._store(retain, data.value)
    return data

def _raise_error(err):
    raise error_types[err.type](err.description)

def _raise_error_ptr(p_error):
    err_ctx = _ErrorContext()
    err = _Error(err_ctx, p_error.value)
    _raise_error(err)

def _check_type(obj, typ, name):
    if not isinstance(obj, typ):
        raise TypeError(f"Expected {typ.__name__} for '{name}', got {type(obj).__name__}")

def load_file(path: Union[str, bytes], opts: Optional[LoadOpts]=None) -> Scene:
    r = []

    p_path, p_path_len = _alloc_str(r, path, "path")
    p_opts = _alloc_obj(r, LoadOpts, opts, "opts")
    p_error = _alloc_obj(r, _Error)

    ptr = cufbx.ufbx_load_file_len(p_path, p_path_len, p_opts, p_error)
    if not ptr: _raise_error_ptr(p_error)

    ctx = _SceneContext(None, cufbx, ptr)
    return Scene(ctx, ptr)

def evaluate_scene(scene: Scene, anim: Anim, time: float, opts: Optional[EvaluateOpts]=None) -> Scene:
    r = []

    _check_type(scene, Scene, "scene")
    _check_type(anim, Anim, "anim")
    p_opts = _alloc_obj(r, EvaluateOpts, opts, "opts")
    p_error = _alloc_obj(r, _Error)

    ptr = cufbx.ufbx_evaluate_scene(p_void(scene.ptr), p_void(anim.ptr), time, p_opts, p_error)
    if not ptr: _raise_error_ptr(p_error)

    ctx = _SceneContext(scene._ctx, cufbx, ptr)
    return Scene(ctx, ptr)

def find_node(scene: Scene, name: Union[str, bytes]) -> Optional[Node]:
    r = []

    _check_type(scene, Scene, "scene")
    p_name, p_name_len = _alloc_str(r, name, "name")

    p_node = scene._ctx.cufbx.ufbx_find_node_len(p_void(scene.ptr), p_name, p_name_len)
    if not p_node: return None

    return Node(scene._ctx, p_node)

def triangulate_face(mesh: Mesh, face: Face) -> List[Tuple[int, int, int]]:
    num_tris = face.num_indices - 2
    if num_tris <= 0: return []

    _check_type(mesh, Mesh, "mesh")

    indices = (ct.c_uint32 * (num_tris * 3))()
    p_indices = cast(ct.pointer(indices), p_void)

    num_tris = cufbx.ufbx_ffi_triangulate_face(p_indices, num_tris * 3, p_void(mesh.ptr), face.index_begin, face.num_indices)

    result = [None] * num_tris
    for n in range(num_tris):
        result[n] = (indices[3*n+0], indices[3*n+1], indices[3*n+2])
    return result

def subdivide_mesh(mesh: Mesh, level: int = 1, opts: Optional[SubdivideOpts] = None) -> Mesh:
    r = []

    _check_type(mesh, Mesh, "mesh")
    p_opts = _alloc_obj(r, SubdivideOpts, opts, "opts")
    p_error = _alloc_obj(r, _Error)

    ptr = cufbx.ufbx_subdivide_mesh(p_void(mesh.ptr), level, p_opts, p_error)
    if not ptr: _raise_error_ptr(p_error)

    ctx = _MeshContext(mesh._ctx, cufbx, ptr)
    return Mesh(ctx, ptr)

def evaluate_curve(curve: AnimCurve, time: float, default: float=0.0) -> float:
    _check_type(curve, AnimCurve, "curve")
    _check_type(time, float, "time")
    _check_type(default, float, "default")
    return cufbx.ufbx_evaluate_curve(p_void(curve.ptr), time, default)

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

def evaluate(self, anim: Anim, time: float, opts: Optional["EvaluateOpts"]=None) -> "Scene":
    return evaluate_scene(self, anim, time, opts)

def find_node(self, name: Union[str, bytes]) -> Optional[Node]:
    return find_node(self, name)

""".strip()

extras["ufbx_mesh"] = """

def triangulate_face(self, face: Face) -> List[Tuple[int, int, int]]:
    return triangulate_face(self, face)

def subdivide(self, level: int = 1, opts: Optional["SubdivideOpts"] = None) -> "Mesh":
    return subdivide_mesh(self, level, opts)

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
    "str": "load_string",
    "bytes": "load_bytes",
}

store_funcs = {
    "str": "store_string",
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
            emit(f"{prefix}load_element(self._ctx, {addr}){suffix}")
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
    emit("check_index(index, self.count)")
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
    emit(f"def load_{name}(addr):")
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
    emit(f"def store_{name}(retain, addr, val):")
    indent()

    for uf in used_fields:
        typ = file.types[uf.field.type]
        res = resolve_py_type(file, typ)
        assert uf.field.name not in ("ptr", "addr")
        addr = f"addr + {uf.offset}"
        val = f"val.{uf.field.name}"
        emit_store(addr, res, val)

    outdent()

    load_funcs[name] = f"load_{name}"
    store_funcs[name] = f"store_{name}"
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
    emit("def load_element(ctx, ptr):")
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
