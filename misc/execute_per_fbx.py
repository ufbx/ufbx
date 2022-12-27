#!/usr/bin/env python3
import argparse
import os
import time
import subprocess

parser = argparse.ArgumentParser(usage="execute_per_fbx.py --exe loader --root .")
parser.add_argument("--exe", help="Executable to run")
parser.add_argument("--root", default=".", help="Root path to search from")
parser.add_argument("--start", default="", help="Top-level file to start from")
parser.add_argument("--verbose", action="store_true", help="Verbose information")
parser.add_argument('remainder', nargs="...")
argv = parser.parse_args()

begin = time.time()

num_tested = 0
num_fail = 0
total_size = 0

for root, dirs, files in os.walk(argv.root):
    for file in files:
        if not file.lower().endswith(".fbx"): continue
        if file.lower().endswith(".ufbx-fail.fbx"):
            num_fail += 1
            continue
        path = os.path.join(root, file)
        size = os.stat(path).st_size
        display = os.path.relpath(path, argv.root)

        if argv.start and display < argv.start:
            continue

        print(f"-- {display}", flush=True)

        total_size += size
        if argv.exe:
            args = [argv.exe, path.encode("utf-8")] + argv.remainder[1:]

            if argv.verbose:
                cmdline = subprocess.list2cmdline(args)
                print(f"$ {cmdline}")

            subprocess.check_call(args)
        num_tested += 1

end = time.time()
dur = end - begin
print()
print("Success!")
print(f"Loaded {num_tested} files in {int(dur//60)}min {int(dur%60)}s.")
print(f"Processed {total_size/1e9:.2f}GB at {total_size/1e6/dur:.2f}MB/s.")
print(f"Ignored {num_fail} invalid files.")
