from collections import namedtuple

NameEnum = namedtuple("NameEnum", "name hash enum")

def str_hash(s):
    h = 0x811c9dc5
    for c in s:
        h = ((h ^ ord(c)) * 0x01000193) & 0xffffffff
    if h == 0: h = 1
    return h

def nameEnum(name, enum):
    return NameEnum(name, str_hash(name), enum)

prop_types = [
    nameEnum("Boolean", "BOOLEAN"),
    nameEnum("bool", "BOOLEAN"),
    nameEnum("Integer", "INTEGER"),
    nameEnum("int", "INTEGER"),
    nameEnum("enum", "INTEGER"),
    nameEnum("Number", "NUMBER"),
    nameEnum("double", "NUMBER"),
    nameEnum("Vector", "VECTOR"),
    nameEnum("Vector3D", "VECTOR"),
    nameEnum("Color", "COLOR"),
    nameEnum("ColorRGB", "COLOR"),
    nameEnum("String", "STRING"),
    nameEnum("KString", "STRING"),
    nameEnum("DateTime", "DATE_TIME"),
    nameEnum("Lcl Translation", "TRANSLATION"),
    nameEnum("Lcl Rotation", "ROTATION"),
    nameEnum("Lcl Scaling", "SCALING"),
]

node_types = [
    nameEnum("Model", "MODEL"),
    nameEnum("Geometry", "MESH"),
    nameEnum("Material", "MATERIAL"),
    nameEnum("Texture", "TEXTURE"),
    nameEnum("AnimationCurveNode", "ANIMATION"),
    nameEnum("AnimationCurve", "ANIMATION_CURVE"),
    nameEnum("AnimationLayer", "ANIMATION_LAYER"),
    nameEnum("NodeAttribute", "ATTRIBUTE"),
]

def find_params(names, map_size, max_k, max_s):
    arr = [None] * map_size
    for k in range(max_k):
        for s in range(0, max_s):
            for i in range(map_size):
                arr[i] = None
            for n in names:
                ix = (n.hash * k >> s) % map_size
                if arr[ix]:
                    break
                else:
                    arr[ix] = n
            else:
                return k, s, arr
    raise ValueError("Could not find params")

decl = []
test = []

def gen_table_imp(names, map_size, type_name, enum_name):
    global decl
    global test

    k, s, arr = find_params(names, map_size, 10000, 24)
 
    decl.append("#define ufbxi_{0}_permute_hash(h) ((((h) * {1}) >> {2}) % {3})".format(type_name, k, s, map_size))
    decl.append("static const ufbxi_{0}_map_entry ufbxi_{0}_map[{1}] = {{".format(type_name, map_size))
    for n in arr:
        if not n:
            decl.append("\t{{ 0u, {{ 0,0 }}, UFBX_{0}_UNKNOWN }},".format(enum_name))
        else:
            decl.append("\t{{ 0x{0:08x}u, {{ \"{1}\", {2} }}, UFBX_{3}_{4} }},".format(n.hash, n.name, len(n.name), enum_name, n.enum))
    decl.append("};")

    test.append("")
    test.append("UFBXT_TEST(table_{0}_map_values)".format(type_name))
    test.append("#if UFBXT_IMPL")
    test.append("{")
    for n in names:
        test.append("\tufbxt_assert(ufbxi_get_{0}(make_str(\"{1}\")) == UFBX_{2}_{3});".format(type_name, n.name, enum_name, n.enum))
    test.append("}")
    test.append("#endif")

    return decl, test

def gen_table(names, type_name, enum_name):
    map_size = 1
    while map_size < len(names):
        map_size *= 2

    while True:
        try:
            return gen_table_imp(names, map_size, type_name, enum_name)
        except:
            map_size *= 2

gen_table(prop_types, "prop_type", "PROP")
gen_table(node_types, "node_type", "NODE")

print("\n".join(decl))
print()
print()
print("\n".join(test))
print()
