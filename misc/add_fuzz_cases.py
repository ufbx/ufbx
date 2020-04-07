import os
import re

self_path = os.path.dirname(os.path.abspath(__file__))
fuzz_path = os.path.join(self_path, "..", "data", "fuzz")

fuzz_files = { }
file_queue = []

RE_FUZZ = re.compile(r"fuzz_(\d+).fbx")

for name in os.listdir(fuzz_path):
    path = os.path.join(fuzz_path, name)
    with open(path, 'rb') as f:
        content = f.read()
    m = RE_FUZZ.match(name)
    if m:
        fuzz_files[content] = name
    else:
        file_queue.append((name, content))

for name, content in file_queue:
    existing = fuzz_files.get(content)
    if existing:
        print("{}: Exists as {}".format(name, existing))
    else:
        new_name = "fuzz_{:04}.fbx".format(len(fuzz_files))
        print("{}: Renaming to {}".format(name, new_name))
        fuzz_files[content] = new_name
        path = os.path.join(fuzz_path, name)
        new_path = os.path.join(fuzz_path, new_name)
        os.rename(path, new_path)
