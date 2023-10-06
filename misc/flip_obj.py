import argparse
import os
import re

parser = argparse.ArgumentParser()
parser.add_argument("input", help="Input .obj file")
parser.add_argument("-o", help="Output file, [input]_lefthanded.py by default")
argv = parser.parse_args()

output = argv.o
if not output:
    base = os.path.basename(argv.input)
    name, _ = os.path.splitext(base)
    output = os.path.join(os.path.dirname(argv.input), f"{name}_lefthanded.obj")

def flip(c):
    if c.startswith("-"):
        return c[1:]
    else:
        return f"-{c}"

with open(argv.input, "rt", encoding="utf-8") as inf:
    with open(output, "wt", encoding="utf-8") as outf:
        for line in inf:
            line = line.rstrip()
            m = re.match(r"\s*(vn?)\s+(\S+)\s+(\S+)\s+(\S+)\s*", line)
            if m:
                v, x, y, z = m.groups()
                print(f"{v} {x} {y} {flip(z)}", file=outf)
            else:
                print(line, file=outf)
