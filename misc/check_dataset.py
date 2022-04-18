import os
import json
from typing import NamedTuple, Optional
import subprocess

class TestCase(NamedTuple):
    root: str
    fbx_path: str
    obj_path: Optional[str]
    mtl_path: Optional[str]
    title: str
    author: str
    license: str
    url: str

def gather_dataset_tasks(root_dir):
    for root, _, files in os.walk(root_dir):
        for filename in files:
            if not filename.endswith(".json"):
                continue

            path = os.path.join(root, filename)
            with open(path, "rt", encoding="utf-8") as f:
                desc = json.load(f)
            
            fbx_path = path.replace(".json", ".fbx")
            assert os.path.exists(fbx_path)

            obj_path = path.replace(".json", ".obj")
            if not os.path.exists(obj_path):
                obj_path = None

            mtl_path = path.replace(".json", ".mtl")
            if not os.path.exists(mtl_path):
                mtl_path = None

            case = TestCase(
                root=root_dir,
                fbx_path=fbx_path,
                obj_path=obj_path,
                mtl_path=mtl_path,
                title=desc["title"],
                author=desc["author"],
                license=desc["license"],
                url=desc["url"])

            yield case

if __name__ == "__main__":
    from argparse import ArgumentParser

    parser = ArgumentParser("check_dataset.py --root <root>")
    parser.add_argument("--root", help="Root directory to search for .json files")
    parser.add_argument("--host-url", help="URL where the files are hosted")
    parser.add_argument("--exe", help="check_fbx.c executable")
    parser.add_argument("--verbose", action="store_true", help="Print verbose information")
    argv = parser.parse_args()

    cases = list(gather_dataset_tasks(root_dir=argv.root))

    def fmt_url(path, root=""):
        if root:
            path = os.path.relpath(path, root)
        path = path.replace("\\", "/")
        return f"{argv.host_url}/{path}"

    ok_count = 0
    for case in cases:
        print(f"-- '{case.title}' by '{case.author}' ({case.license}) --")
        print(f"  source url: {case.url}")
        if argv.host_url:
            print(f"    .fbx url: {fmt_url(case.fbx_path, case.root)}")
            if case.obj_path:
                print(f"    .obj url: {fmt_url(case.obj_path, case.root)}")
            if case.mtl_path:
                print(f"    .mtl url: {fmt_url(case.mtl_path, case.root)}")

        args = [argv.exe]
        args.append(case.fbx_path)
        if case.obj_path:
            args += ["--obj", case.obj_path]
        if argv.verbose:
            print("$ " + " ".join(args))
        print()

        try:
            subprocess.check_call(args)
            print()
            print("-- SUCCESS --")
            ok_count += 1
        except subprocess.CalledProcessError:
            print()
            print("-- FAIL --")
        print()
    print(f"{ok_count}/{len(cases)}")

    if ok_count < len(cases):
        exit(1)
