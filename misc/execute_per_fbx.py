#!/usr/bin/env python3
import argparse
import os
import subprocess

parser = argparse.ArgumentParser(usage="execute_per_fbx.py --exe loader --root .")
parser.add_argument("--exe", help="Executable to run")
parser.add_argument("--root", default=".", help="Root path to search from")
argv = parser.parse_args()

for root, dirs, files in os.walk(argv.root):
    for file in files:
        if not file.lower().endswith(".fbx"): continue
        if file.lower().endswith(".ufbx-fail.fbx"): continue
        if file.lower().endswith(".ufbx-old.fbx"): continue
        path = os.path.join(root, file)
        display = os.path.relpath(path, argv.root)
        print(f"-- {display}")

        if argv.exe:
            args = [argv.exe, path.encode("utf-8")]
            subprocess.check_call(args)
