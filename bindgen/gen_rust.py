import json
import argparse
import os
import itertools

ichain = itertools.chain.from_iterable

prelude = """
pub mod ufbx {

use std::{ptr, str, slice};
use std::marker::PhantomData;
use std::ops::{Index};
use std::os::raw::{c_char, c_void};
use std::fmt;
use std::ffi::CString;

pub const UFBX_ERROR_STACK_MAX_DEPTH: usize = 8;

pub type Real = f64;

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct Vec2 {
    pub x: Real,
    pub y: Real,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct Vec3 {
    pub x: Real,
    pub y: Real,
    pub z: Real,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct Vec4 {
    pub x: Real,
    pub y: Real,
    pub z: Real,
    pub w: Real,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct Quat {
    pub x: Real,
    pub y: Real,
    pub z: Real,
    pub w: Real,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct Matrix {
    pub m00: Real,
    pub m10: Real,
    pub m20: Real,
    pub m01: Real,
    pub m11: Real,
    pub m21: Real,
    pub m02: Real,
    pub m12: Real,
    pub m22: Real,
    pub m03: Real,
    pub m13: Real,
    pub m23: Real,
}

#[repr(C)]
pub struct List<'a, T> {
    data: *const T,
    pub count: usize,
    _marker: PhantomData<&'a T>,
}

impl<'a, T> AsRef<[T]> for List<'a, T> {
    fn as_ref(&self) -> &[T] {
        unsafe { slice::from_raw_parts(self.data, self.count) }
    }
}

impl<'a, T> Index<usize> for List<'a, T> {
    type Output = T;
    fn index(&self, i: usize) -> &T {
        self.as_ref().index(i)
    }
}

impl<'a, T> List<'a, T> {
    pub fn iter(&self) -> slice::Iter<T> {
        self.as_ref().iter()
    }
}

pub struct RefIter<'a, T> {
    inner: slice::Iter<'a, Ref<'a, T>>,
}

impl<'a, T> Iterator for RefIter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(|v| v.as_ref())
    }
}

#[repr(C)]
pub struct RefList<'a, T> {
    data: *const Ref<'a, T>,
    pub count: usize,
    _marker: PhantomData<&'a T>,
}

impl<'a, T> AsRef<[Ref<'a, T>]> for RefList<'a, T> {
    fn as_ref(&self) -> &[Ref<'a, T>] {
        unsafe { slice::from_raw_parts(self.data, self.count) }
    }
}

impl<'a, T> Index<usize> for RefList<'a, T> {
    type Output = T;
    fn index(&self, i: usize) -> &T {
        self.as_ref().index(i).as_ref()
    }
}

impl<'a, T> RefList<'a, T> {
    pub fn iter(&self) -> RefIter<'_, T> {
        RefIter::<'_, T> { inner: self.as_ref().iter() }
    }
}

#[repr(C)]
pub struct Ref<'a, T> {
    ptr: *const T,
    _marker: PhantomData<&'a T>,
}

impl<'a, T> AsRef<T> for Ref<'a, T> {
    fn as_ref(&self) -> &T {
        unsafe { &*self.ptr }
    }
}

#[repr(C)]
pub struct OptionRef<'a, T> {
    ptr: *const T,
    _marker: PhantomData<&'a T>,
}

impl<'a, T> OptionRef<'a, T> {
    pub fn is_some(&self) -> bool { self.ptr.is_null() }
    pub fn is_none(&self) -> bool { !self.ptr.is_null() }

    pub fn as_ref(&self) -> Option<&T> {
        unsafe { self.ptr.as_ref() }
    }
}

#[repr(C)]
pub struct String<'a> {
    data: *const u8,
    pub length: usize,
    _marker: PhantomData<&'a u8>,
}

impl<'a> AsRef<str> for String<'a> {
    fn as_ref(&self) -> &str {
        unsafe { str::from_utf8_unchecked(slice::from_raw_parts(self.data, self.length)) }
    }
}

impl<'a> fmt::Display for String<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_ref().fmt(f)
    }
}

#[repr(C)]
pub struct Blob<'a> {
    data: *const u8,
    pub size: usize,
    _marker: PhantomData<&'a u8>,
}

impl<'a> AsRef<[u8]> for Blob<'a> {
    fn as_ref(&self) -> &[u8] {
        unsafe { slice::from_raw_parts(self.data, self.size) }
    }
}

""".strip()

epilogue = """

#[link(name="ufbx")]
extern "C" {
    fn ufbx_load_file<'a>(path: *const c_char, opts: *const c_void, error: *const c_void) -> *mut Scene<'a>;
    fn ufbx_free_scene(scene: *mut Scene);
}

pub struct SceneRef<'a> {
    scene: *mut Scene<'a>,
}

impl<'a> AsRef<Scene<'a>> for SceneRef<'a> {
    fn as_ref(&self) -> &Scene<'a> {
        unsafe { &*self.scene }
    }
}

impl<'a> Drop for SceneRef<'a> {
    fn drop(&mut self) {
        unsafe { ufbx_free_scene(self.scene) }
    }
}

pub fn load_file(path: &str) -> SceneRef {
    let path_c = CString::new(path).unwrap(); // TODO: Allocation failure
    SceneRef {
        scene: unsafe { ufbx_load_file(path_c.as_ptr(), ptr::null(), ptr::null()) },
    }
}

impl<'a> Index<usize> for VertexReal<'a> {
    type Output = Real;
    fn index(&self, i: usize) -> &Real {
        &self.values[self.indices[i] as usize]
    }
}

impl<'a> Index<usize> for VertexVec2<'a> {
    type Output = Vec2;
    fn index(&self, i: usize) -> &Vec2 {
        &self.values[self.indices[i] as usize]
    }
}

impl<'a> Index<usize> for VertexVec3<'a> {
    type Output = Vec3;
    fn index(&self, i: usize) -> &Vec3 {
        &self.values[self.indices[i] as usize]
    }
}

impl<'a> Index<usize> for VertexVec4<'a> {
    type Output = Vec4;
    fn index(&self, i: usize) -> &Vec4 {
        &self.values[self.indices[i] as usize]
    }
}

impl<'a> Prop<'a> {
    pub fn value_real(&self) -> Real {
        self.value_vec3.x
    }

    pub fn value_vec2(&self) -> Vec2 {
        let v = self.value_vec3;
        Vec2 { x: v.x, y: v.y }
    }
}


}
""".strip()

builtin_types = {
    "char": "u8",
    "uint8_t": "i32",
    "int8_t": "u32",
    "int32_t": "i32",
    "uint32_t": "u32",
    "int64_t": "i64",
    "uint64_t": "u64",
    "float": "f32",
    "double": "f64",
    "size_t": "usize",
    "bool": "bool",
    "ufbx_real": "Real",
    "ufbx_vec2": "Vec2",
    "ufbx_vec3": "Vec3",
    "ufbx_vec4": "Vec4",
    "ufbx_quat": "Quat",
    "ufbx_matrix": "Matrix",
    "ufbx_blob": "Blob<'a>",
}

ignore_types = {
    "ufbx_string",
    "ufbx_vertex_attrib",
    "ufbx_void_list",
}

list_types = { }
name_types = { }
lifetime_types = { "ufbx_string", "ufbx_blob" }

def filter_name(name):
    if name == "type":
        return "type_"
    else:
        return name

outfile = None

def emit(text):
    print(text, file=outfile)

def fmt_type_base(type):
    name = type["name"]
    if name in builtin_types:
        return builtin_types[name]
    if name == "ufbx_string":
        return "String<'a>"
    elif name in list_types:
        elem_type = list_types[name]
        mods = elem_type["mods"]
        if mods == [{ "type": "pointer" }] or mods == [{ "type": "const" }, { "type": "pointer" }]:
            inner = fmt_type_base(elem_type)
            return f"RefList<'a, {inner}>"
        else:
            inner = fmt_type(elem_type)
            return f"List<'a, {inner}>"
    elif name in name_types:
        if name in lifetime_types:
            return f"{name_types[name]}<'a>"
        else:
            return name_types[name]
    else:
        return name

def fmt_type(type):
    mods = type["mods"]
    if mods == []:
        return fmt_type_base(type)
    elif mods == [{ "type": "pointer" }] or mods == [{ "type": "const" }, { "type": "pointer" }]:
        inner = fmt_type_base(type)
        return f"Ref<'a, {inner}>"
    elif mods == [{ "type": "nullable" }, { "type": "pointer" }]:
        inner = fmt_type_base(type)
        return f"OptionRef<'a, {inner}>"
    elif mods == [{ "type": "nullable" }, { "type": "array", "length": "3" }, { "type": "pointer" }]:
        inner = fmt_type_base(type)
        return f"[OptionRef<'a, {inner}>;3]"
    elif mods and mods[0]["type"] == "array":
        new_type = dict(type)
        new_type["mods"] = mods[1:]
        num = mods[0]["length"]
        inner = fmt_type(new_type)
        return f"[{inner};{num}]"
    else:
        raise ValueError(f"Unhandled mods: {mods}\n")

def type_needs_lifetime(type):
    mods = type["mods"]
    if mods == []:
        name = type["name"]
        if name in list_types:
            return True
        elif name in lifetime_types:
            return True
        else:
            return False
    elif mods == [{ "type": "pointer" }] or mods == [{ "type": "const" }, { "type": "pointer" }]:
        return True
    elif mods == [{ "type": "nullable" }, { "type": "pointer" }]:
        return True
    elif mods == [{ "type": "nullable" }, { "type": "array", "length": "3" }, { "type": "pointer" }]:
        return True
    elif mods and mods[0]["type"] == "array":
        new_type = dict(type)
        new_type["mods"] = mods[1:]
        return type_needs_lifetime(new_type)
    else:
        raise ValueError(f"Unhandled mods: {mods}\n")

def emit_comment(comment, indent):
    if not comment: return
    for line in comment:
        emit(f"{indent}// {line}")

def emit_field(decl):
    kind = decl["kind"]
    if kind == "group":
        emit_comment(decl.get("comment"), "\t")
        for inner in decl["decls"]:
            emit_field(inner)
    elif kind == "struct":
        decl_kind = decl["structKind"]
        if decl_kind == "union":
            emit_field(choose_variant(decl))
        elif decl_kind:
            for inner in decl["decls"]:
                emit_field(inner)
    elif kind == "decl":
        name = filter_name(decl["name"])
        type = fmt_type(decl["type"])
        emit(f"\tpub {name}: {type},")

element_types = []

def emit_enum(decl, parent_name):
    kind = decl["kind"]
    if kind == "group":
        emit_comment(decl.get("comment"), "\t")
        for inner in decl["decls"]:
            emit_enum(inner, parent_name)
    elif kind == "decl":
        name = decl["name"]
        for seg in parent_name.split("_"):
            if name.startswith(f"{seg.upper()}_"):
                name = name[len(seg) + 1:]
        new_name = "".join(word.title() for word in name.split('_'))
        if new_name[0].isnumeric():
            new_name = "E" + new_name
        if parent_name == "ufbx_element_type":
            if new_name == "NumElementTypes" or new_name == "FirstAttrib" or new_name == "LastAttrib":
                return
            element_types.append(new_name)
        emit(f"\t{new_name},")

def flatten(decl):
    kind = decl["kind"]
    if kind == "group":
        for inner in decl["decls"]:
            yield from flatten(inner)
    else:
        yield decl

def choose_variant(union):
    decls = list(ichain((flatten(inner) for inner in union["decls"])))
    first = decls[0]
    last = decls[-1]
    if first["name"] == "element" or last["name"] == "elements_by_type":
        return first
    else:
        return last

def gather_fields(decl, parent):
    kind = decl["kind"]
    if kind == "group":
        for inner in decl["decls"]:
            gather_fields(inner, parent)
    elif kind == "struct":
        decl_kind = decl["structKind"]
        if decl_kind == "union":
            gather_fields(choose_variant(decl), parent)
        elif decl_kind:
            for inner in decl["decls"]:
                gather_fields(inner, parent)
    elif kind == "decl":
        type = decl["type"]
        if type_needs_lifetime(type):
            lifetime_types.add(parent)

def gather_types(decl):
    kind = decl["kind"]
    if kind == "group":
        for inner in decl["decls"]:
            gather_types(inner)
    elif kind == "struct":
        name = decl.get("name")
        if not name or name in builtin_types:
            return
        if decl["isList"]:
            new_type = dict(decl["decls"][0]["decls"][0]["type"])
            new_type["mods"] = new_type["mods"][1:]
            list_types[name] = new_type
        else:
            if name.startswith("ufbx_"):
                new_name = "".join(word.title() for word in name[5:].split('_'))
                name_types[name] = new_name
            for field in decl["decls"]:
                gather_fields(field, name)

    elif kind == "enum":
        name = decl.get("name")
        if not name or name in builtin_types:
            return
        if name.startswith("ufbx_"):
            new_name = "".join(word.title() for word in name[5:].split('_'))
            name_types[name] = new_name

hack_emit_scene = False

def emit_decl(decl, api):
    global hack_emit_scene

    kind = decl["kind"]
    if kind == "group":
        emit("")
        emit_comment(decl.get("comment"), "")
        for inner in decl["decls"]:
            emit_decl(inner, api)
    elif kind == "struct":
        name = decl.get("name")
        if not name or name in builtin_types:
            return
        if decl["isList"]:
            return
        if name in ignore_types:
            return
        new_name = name_types.get(name)
        if not new_name: return

        emit("")
        emit("#[repr(C)]")
        if name in lifetime_types:
            emit(f"pub struct {new_name}<'a> {{")
        else:
            emit(f"pub struct {new_name} {{")
        for field in decl["decls"]:
            emit_field(field)
        emit(f"}}")

        if name == "ufbx_scene":
            hack_emit_scene = True
    elif kind == "enum":
        name = decl.get("name")
        new_name = name_types.get(name)
        if not new_name: return

        emit("")
        emit("#[repr(i32)]")
        emit(f"pub enum {new_name} {{")
        for field in decl["decls"]:
            emit_enum(field, name)
        emit(f"}}")

def generate(ufbx):
    global hack_emit_scene
    global element_types

    emit(prelude)
    for decl in ufbx:
        gather_types(decl)
    for decl in ufbx:
        emit_decl(decl, True)
        if hack_emit_scene:
            break

    emit("")
    emit("pub enum ElementData<'a> {")
    for et in element_types:
        emit(f"\t{et}(&'a {et}<'a>),")
    emit("}")

    emit("")
    emit("impl<'a> Element<'a> {")
    emit("\tpub fn as_data(&self) -> ElementData<'a> {")
    emit("\t\tunsafe {")
    emit("\t\t\tmatch self.type_ {")
    for et in element_types:
        emit(f"\t\t\t\tElementType::{et} => ElementData::{et}(&*((self as *const Element) as *const {et})),")
    emit("\t\t\t}")
    emit("\t\t}")
    emit("\t}")
    emit("}")

if __name__ == "__main__":

    parser = argparse.ArgumentParser("gen_rust.py")
    parser.add_argument("-i", help="Input ufbx.json file")
    parser.add_argument("-o", help="Output path")
    argv = parser.parse_args()

    src_path = os.path.dirname(os.path.realpath(__file__))

    input_file = argv.i
    if not input_file:
        input_file = os.path.join(src_path, "build", "ufbx.json")

    output_path = argv.o
    if not output_path:
        output_path = os.path.join(src_path, "build", "rust")

    with open(input_file, "rt") as f:
        ufbx = json.load(f)

    if not os.path.exists(output_path):
        os.makedirs(output_path, exist_ok=True)

    with open(os.path.join(output_path, "ufbx", "src", "lib.rs"), "wt", encoding="utf-8") as f:
        outfile = f
        generate(ufbx)

        emit("")
        emit(epilogue)
