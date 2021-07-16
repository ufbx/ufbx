import asyncio
import itertools
from platform import platform
import subprocess
import time
import re
import os
import sys
import shutil

color_out = sys.stdout

has_color = False
if sys.platform == "win32":
    try:
        import colorama
        colorama.init(wrap=False)
        color_out = colorama.AnsiToWin32(color_out).stream
        has_color = True
    except ImportError:
        print("# Run 'pip install colorama' for colorful output")
else:
    has_color = sys.stdout.isatty()

STYLE_FAIL = "\x1b[31m"
STYLE_CMD = "\x1b[36m"

def log(line, *, style=""):
    if style and has_color:
        line = style + line + "\x1b[39m"
    print(line, file=color_out, flush=True)

def log_cmd(line):
    log(line, style=STYLE_CMD)

def log_mkdir(path):
    log_cmd("mkdir " + path)

def log_comment(line, fail=False):
    style = STYLE_FAIL if fail else ""
    if sys.platform == "win32":
        log("rem " + line, style=style)
    else:
        log("# " + line, style=style)

loop = asyncio.ProactorEventLoop()

def flatten_str_list(str_list):
    """Flatten arbitrarily nested str list `item` to `dst`"""
    def inner(result, str_list):
        if isinstance(str_list, str):
            result.append(str_list)
        else:
            for s in str_list:
                inner(result, s)
    result = []
    inner(result, str_list)
    return result

cmd_sema = asyncio.Semaphore(8, loop=loop)

async def run_cmd(cmd, *args, realtime_output=False):
    """Asynchronously run a command"""

    await cmd_sema.acquire()

    cmd_args = flatten_str_list(args)

    pipe = None if realtime_output else asyncio.subprocess.PIPE
    cmdline = subprocess.list2cmdline([cmd] + cmd_args)

    out = err = ""
    ok = False

    log_cmd(cmdline)

    begin = time.time()

    try:
        proc = await asyncio.create_subprocess_exec(cmd, *cmd_args,
            stdout=pipe, stderr=pipe)

        if not realtime_output:
            out, err = await proc.communicate()
            out = out.decode("utf-8", errors="ignore").strip()
            err = err.decode("utf-8", errors="ignore").strip()

        ok = proc.returncode == 0
    except FileNotFoundError:
        err = f"{cmd} not found"

    end = time.time()

    cmd_sema.release()

    return ok, out, err, cmdline, end - begin

class Compiler:
    def __init__(self, name, exe):
        self.name = name
        self.exe = exe

    def run(self, *args, **kwargs):
        return run_cmd(self.exe, args, **kwargs)

class CLCompiler(Compiler):
    def __init__(self, name, exe):
        super().__init__(name, exe)
        self.has_c = True
        self.has_cpp = True

    async def check_version(self):
        _, out, err, _, _ = await self.run()
        m = re.search(r"Version ([.0-9]+) for (\w+)", out + err, re.M)
        if not m: return False
        self.arch = m.group(2).lower()
        self.version = m.group(1)
        return True
    
    def supported_archs(self):
        if self.arch == "x86":
            return ["x86"]
        if self.arch == "x64":
            return ["x64"]
        return []
    
    def compile(self, config):
        sources = config["sources"]
        output = config["output"]

        args = []
        args += sources
        args += ["/MT", "/nologo"]

        obj_dir = os.path.dirname(output)
        args.append(f"/Fo{obj_dir}\\")

        if config.get("warnings", False):
            args.append("/W4")
            args.append("/WX")
        else:
            args.append("/W3")

        if config.get("optimize", False):
            args.append("/Ox")

        if config.get("openmp", False):
            args.append("/openmp")

        args.append("/link")

        args += ["/opt:ref"]
        args.append(f"-out:{output}")

        return self.run(args)

class GCCCompiler(Compiler):
    def __init__(self, name, exe, cpp):
        super().__init__(name, exe)
        self.has_c = not cpp
        self.has_cpp = cpp

    async def check_version(self):
        _, out, err, _, _ = await self.run("--version")
        mv = re.search(r"version ([.0-9]+)", out + err, re.M)
        ma = re.search(r"Target: ([a-zA-Z0-9_-]+)", out + err, re.M)
        if not (ma and mv): return False
        self.arch = ma.group(1).lower()
        self.version = mv.group(1)
        return True

    def supported_archs(self):
        if "x86_64" in self.arch:
            return ["x86", "x64"]
        if "i686" in self.arch:
            return ["x86"]
        return []
    
    def compile(self, config):
        sources = config["sources"]
        output = config["output"]

        args = []

        if config.get("warnings", False):
            args.append("-Wall -Wextra -Werror")

        if config.get("optimize", False):
            args.append("-O2")

        if config.get("openmp", False):
            args.append("-openmp")
        
        args += sources
        args += ["-o", output]

        return self.run(args)

class ClangCompiler(GCCCompiler):
    def __init__(self, name, exe, cpp):
        super().__init__(name, exe, cpp)

class EmscriptenCompiler(ClangCompiler):
    def __init__(self, name, exe, cpp):
        super().__init__(name, exe, cpp)

all_compilers = [
    CLCompiler("cl", "cl.exe"),
    GCCCompiler("gcc", "gcc", False),
    GCCCompiler("gcc", "g++", True),
    ClangCompiler("clang", "clang", False),
    ClangCompiler("clang", "clang++", True),
    # EmscriptenCompiler("emcc", emcc", False),
    # EmscriptenCompiler("emcc", emcc++", True),
]

def gather(aws):
    return asyncio.gather(*aws)

ichain = itertools.chain.from_iterable

async def check_compiler(compiler):
    if await compiler.check_version():
        return [compiler]
    else:
        return []

async def find_compilers():
    return list(ichain(await gather(check_compiler(c) for c in all_compilers)))

class Target:
    def __init__(self, name, compiler, config):
        self.name = name
        self.compiler = compiler
        self.config = config
        self.skipped = False
        self.compiled = False
        self.ok = True
        self.log = []

async def compile_target(t):
    if t.config["arch"] not in t.compiler.supported_archs():
        t.skipped = True
        return

    ok, out, err, cmdline, time = await t.compiler.compile(t.config)

    t.log.append("$ " + cmdline)
    t.log.append(out)
    t.log.append(err)

    if ok:
        t.compiled = True
    else:
        t.ok = False

    head = f"Compile {t.name}"
    tail = f"[{time:.1f}s OK]" if ok else "[FAIL]"
    log_comment(f"{tail} {head}", fail=not ok)
    return t

async def run_target(t, args):
    if not t.compiled: return

    ok, out, err, cmdline, time = await run_cmd(t.config["output"], args)

    t.log.append("$ " + cmdline)
    t.log.append(out)
    t.log.append(err)

    if not ok:
        t.ok = False

    head = f"Run {t.name}"
    tail = f"[{time:.1f}s OK]" if ok else "[FAIL]"
    log_comment(f"{tail} {head}", fail=not ok)
    return t

async def compile_and_run_target(t, args):
    await compile_target(t)
    await run_target(t, args)
    return t

def copy_file(src, dst):
    shutil.copy(src, dst)
    if sys.platform == "win32":
        log_cmd(f"copy {src} {dst}")
    else:
        log_cmd(f"cp {src} {dst}")

exit_code = 0

async def main():
    log_comment("-- Searching for compilers --")
    compilers = await find_compilers()
    for compiler in compilers:
        archs = ", ".join(compiler.supported_archs())
        log_comment(f"{compiler.exe}: {compiler.arch} {compiler.version} [{archs}]")

    all_configs = {
        "optimize": {
            "debug": { "optimize": False },
            "release": { "optimize": True },
        },
        "arch": {
            "x86": { "arch": "x86" },
            "x64": { "arch": "x64" },
        },
    }

    arch_configs = { "arch": all_configs["arch"] }

    log_comment("-- Compiling and running tests --")

    build_path = "build"
    if not os.path.exists(build_path):
        os.makedirs(build_path, exist_ok=True)
        log_mkdir(build_path)

    def compile_permutations(prefix, config, config_options):
        opt_combos = [[(name, opt) for opt in opts] for name, opts in config_options.items()]
        opt_combos = list(itertools.product(*opt_combos))

        is_cpp = config.get("cpp", False)
        is_c = not is_cpp

        for compiler in compilers:
            if is_c and not compiler.has_c: continue
            if is_cpp and not compiler.has_cpp: continue

            for opts in opt_combos:
                optstring = "_".join(opt for _,opt in opts)
                name = f"{prefix}_{compiler.name}_{optstring}"

                path = os.path.join(build_path, name)
                if not os.path.exists(path):
                    os.makedirs(path, exist_ok=True)
                    log_mkdir(path)

                conf = dict(config)
                conf["output"] = os.path.join(path, config.get("output", "a.exe"))
                for opt_name, opt in opts:
                    conf.update(config_options[opt_name][opt])

                target = Target(name, compiler, conf)
                yield compile_and_run_target(target, ["-d", "data"])

    target_tasks = []

    runner_config = {
        "sources": ["test/runner.c", "ufbx.c"],
        "output": "runner.exe",
    }
    target_tasks += compile_permutations("runner", runner_config, all_configs)

    cpp_config = {
        "sources": ["misc/test_build.cpp"],
        "output": "cpp.exe",
        "cpp": True,
        "warnings": True,
    }
    target_tasks += compile_permutations("cpp", cpp_config, arch_configs)

    targets = await gather(target_tasks)

    num_fail = 0
    for target in targets:
        if target.ok: continue
        print()
        log(f"-- FAIL: {target.name}", style=STYLE_FAIL)
        print("\n".join(target.log))
        num_fail += 1
    
    print()
    print(f"{len(targets) - num_fail}/{len(targets)} targets succeeded")
    if num_fail > 0:
        exit_code = 1

if __name__ == "__main__":
    loop.run_until_complete(main())
    loop.close()
    sys.exit(exit_code)
