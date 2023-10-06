import argparse
import os
import re

parser = argparse.ArgumentParser()
parser.add_argument("input", help="Input .obj file")
parser.add_argument("--axis", default="z", help="Axis to flip")
parser.add_argument("-o", help="Output file, [input_N]_lefthanded.py by default")
argv = parser.parse_args()

output = argv.o
if not output:
    base = os.path.basename(argv.input)
    m = re.match("(.*?)(_\d+)?(\.obj)", base, re.IGNORECASE)
    if m:
        name, frame, ext = m.groups()
        frame = frame or ""
        output = os.path.join(os.path.dirname(argv.input), f"{name}_lefthanded{frame}.obj")

def flip(c):
    if c.startswith("-"):
        return c[1:]
    else:
        return f"-{c}"

axis = argv.axis.lower()

with open(argv.input, "rt", encoding="utf-8") as inf:
    with open(output, "wt", encoding="utf-8") as outf:
        for line in inf:
            line = line.rstrip()
            m = re.match(r"\s*(vn?)\s+(\S+)\s+(\S+)\s+(\S+)\s*", line)
            if m:
                v, x, y, z = m.groups()
                if axis == "x":
                    x = flip(x)
                elif axis == "y":
                    y = flip(y)
                elif axis == "z":
                    z = flip(z)
                print(f"{v} {x} {y} {z}", file=outf)
            else:
                print(line, file=outf)
