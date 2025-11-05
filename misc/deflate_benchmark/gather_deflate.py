import argparse
import os
import subprocess
import json

parser = argparse.ArgumentParser(description="Gather DEFLATE compressed streams from .fbx files")
parser.add_argument("--exe", help="Executable path for per-file gathering, see `gather_deflate_main.cpp`")
parser.add_argument("-o", help="Output file")
parser.add_argument("--root", help="Root path to look for .fbx files")
argv = parser.parse_args()

data = bytearray()

for root,_,files in os.walk(argv.root):
    for file in files:
        if not file.endswith(".fbx"): continue
        path = os.path.join(root, file)
        path = path.replace("\\", "/")
        print(path)

        rel_base = os.path.splitext(path)[0]
        rel_base = os.path.relpath(rel_base, argv.root)
        dst_path = os.path.join(argv.o, rel_base)
        dst_path = dst_path.replace("\\", "/")

        os.makedirs(os.path.dirname(dst_path), exist_ok=True)

        try:
            subprocess.check_output([argv.exe, path, dst_path])
        except Exception as e:
            print(f"FAIL: {e}")

total_json = []

for root,_,files in os.walk(argv.o):
    for file in files:
        if not file.endswith(".json"): continue
        if file == "index.json": continue
        path = os.path.join(root, file)
        with open(path, "rt", encoding="utf-8") as f:
            data = json.load(f)
            if data["arrays"]:
                total_json.append(data)

with open(os.path.join(argv.o, "index.json"), "wt", encoding="utf-8") as f:
    json.dump({
        "data": total_json,
    }, f, indent=4)
