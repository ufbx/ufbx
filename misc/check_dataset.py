from asyncio.format_helpers import extract_stack
import os
import json
from typing import NamedTuple, Optional
import subprocess
import glob
import re
import shlex

class TestCase(NamedTuple):
    root: str
    fbx_path: str
    obj_path: Optional[str]
    mtl_path: Optional[str]
    title: str
    author: str
    license: str
    url: str
    frame: Optional[int]

def log(message=""):
    print(message, flush=True)

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

            seen_objs = set()
            for obj_ext in (".obj.gz", ".obj"):
                obj_path = path.replace(".json", obj_ext)
                obj_prefix = obj_path[:-len(obj_ext)] + "_"
                obj_glob = f"{obj_prefix}*.{obj_ext}"
                obj_paths = [obj_path] + glob.glob(obj_glob)

                for obj_path in obj_paths:
                    if not os.path.exists(obj_path): continue

                    mtl_path = obj_path.replace(obj_ext, ".mtl")
                    if not os.path.exists(mtl_path):
                        mtl_path = None

                    frame = None

                    base_name = obj_path[:-len(obj_ext)]
                    if base_name in seen_objs:
                        continue
                    seen_objs.add(base_name)

                    flags = obj_path[len(obj_prefix):-len(obj_ext)].split("_")
                    for flag in flags:
                        m = re.match(r"frame(\d+)", flag)
                        if m:
                            frame = int(m.group(1))

                    case = TestCase(
                        root=root_dir,
                        fbx_path=fbx_path,
                        obj_path=obj_path,
                        mtl_path=mtl_path,
                        title=desc["title"],
                        author=desc["author"],
                        license=desc["license"],
                        url=desc["url"],
                        frame=frame,
                    )

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
        extra = []

        args = [argv.exe]
        args.append(case.fbx_path)

        if case.obj_path:
            args += ["--obj", case.obj_path]

        if case.frame is not None:
            extra.append(f"frame {case.frame}")
            args += ["--frame", str(case.frame)]

        extra_str = ""
        if extra:
            extra_str = " [" + ", ".join(extra) + "]"

        log(f"-- '{case.title}' by '{case.author}' ({case.license}){extra_str} --")

        log()
        log(f"  source url: {case.url}")
        if argv.host_url:
            log(f"    .fbx url: {fmt_url(case.fbx_path, case.root)}")
            if case.obj_path:
                log(f"    .obj url: {fmt_url(case.obj_path, case.root)}")
            if case.mtl_path:
                log(f"    .mtl url: {fmt_url(case.mtl_path, case.root)}")

        log()
        log("$ " + " ".join(args))
        log()

        try:
            subprocess.check_call(args)
            log()
            log("-- PASS --")
            ok_count += 1
        except subprocess.CalledProcessError:
            log()
            log("-- FAIL --")
        log()

    log(f"{ok_count}/{len(cases)} tests passed")

    if ok_count < len(cases):
        exit(1)
