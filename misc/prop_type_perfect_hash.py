from collections import namedtuple

PropType = namedtuple("PropType", "name hash enum")

def str_hash(s):
    h = 0x811c9dc5
    for c in s:
        h = ((h ^ ord(c)) * 0x01000193) & 0xffffffff
    if h == 0: h = 1
    return h

def propType(name, enum):
    return PropType(name, str_hash(name), enum)

types = [
    propType("Boolean", "BOOLEAN"),
    propType("bool", "BOOLEAN"),
    propType("Integer", "INTEGER"),
    propType("int", "INTEGER"),
    propType("enum", "INTEGER"),
    propType("Number", "NUMBER"),
    propType("double", "NUMBER"),
    propType("Vector", "VECTOR"),
    propType("Vector3D", "VECTOR"),
    propType("Color", "COLOR"),
    propType("ColorRGB", "COLOR"),
    propType("String", "STRING"),
    propType("KString", "STRING"),
    propType("DateTime", "DATE_TIME"),
    propType("Lcl Translation", "TRANSLATION"),
    propType("Lcl Rotation", "ROTATION"),
    propType("Lcl Scaling", "SCALING"),
]

MAP_SIZE = 32

arr = [None] * MAP_SIZE

def find_params(max_k, max_s):
    for k in range(max_k):
        for s in range(0, max_s):
            for n in range(MAP_SIZE):
                arr[n] = None
            for t in types:
                ix = (t.hash * k >> s) % MAP_SIZE
                if arr[ix]:
                    break
                else:
                    arr[ix] = t
            else:
                return k, s
    raise ValueError("Could not find params")

k, s = find_params(100000, 24)

print("#define ufbxi_proptype_permute_hash(h) ((((h) * {}) >> {}) % {})".format(k, s, MAP_SIZE))
print("static const ufbxi_proptype_map_entry ufbxi_proptype_map[{}] = {{".format(MAP_SIZE))
for t in arr:
    if not t:
        print("\t{ 0u, { 0,0 }, UFBX_PROP_UNKNOWN },")
    else:
        print("\t{{ 0x{:08x}u, {{ \"{}\", {} }}, UFBX_PROP_{} }},".format(t.hash, t.name, len(t.name), t.enum))
print("};")

print()
print()

for t in types:
	print("\tufbxt_assert(ufbxi_get_prop_type(make_str(\"{}\")) == UFBX_PROP_{});".format(t.name, t.enum))
