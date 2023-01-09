import argparse
import re
import sys

def strip_comments(line):
    if "//" in line:
        return line[:line.index("//")]
    else:
        return line

def forbid(r, line, err):
    m = re.search(r, line)
    if m:
        return (err, m.start(1), m.end(1))
    else:
        return None

def no_trailing_whitespace(line):
    return forbid(r"(\s+)$", line, "trailing whitespace is forbidden")

def indent_tabs(line):
    return forbid(r"^\s*?( +)\s*", line, "tabs should be used for indentation")

def no_trailing_tabs(line):
    return forbid(r"\S.*(\t+)", line, "tabs should only appear in the beginning of a line")

def keyword_spacing(line):
    line = strip_comments(line)
    return forbid(r"\b(?:for|if|while)(\()", line, "expected space after keyword")

def pointer_alignment(line):
    line = strip_comments(line)
    return forbid(r"\w(\* )\w", line, "pointers should be aligned to the right")

checks = [
    no_trailing_whitespace,
    indent_tabs,
    no_trailing_tabs,
    keyword_spacing,
    pointer_alignment,
]

def check_file(path, colors):
    failed = False
    if colors:
        c_gray = "\033[1;30m"
        c_green = "\033[1;32m"
        c_red = "\033[1;31m"
        c_white = "\033[1;97m"
        c_def = "\033[0m"
    else:
        c_gray = ""
        c_green = ""
        c_red = ""
        c_white = ""
        c_def = ""
    with open(path, "rt") as f:
        for ix, line in enumerate(f):
            line = line.rstrip("\r\n")
            for check in checks:
                err = check(line)
                if err:
                    err_desc, err_begin, err_end = err
                    l = f"{c_white}{path}:{ix + 1}:{err_begin + 1}: {c_red}error:{c_white} {err_desc} [{check.__name__}]{c_def}"
                    s = line
                    s = s.replace("\t", f"{c_gray}\u2192{c_def}")
                    s = s.replace(" ", f"{c_gray}\u00B7{c_def}")
                    e = " " * err_begin + c_green + "^" * (err_end - err_begin) + c_def
                    print(f"{l}\n  {s}\n  {e}")
                    failed = True
    return failed


if __name__ == "__main__":
    p = argparse.ArgumentParser("check_formatting.py")
    p.add_argument("files", nargs="*")
    p.add_argument("--no-color", action="store_true")
    argv = p.parse_args()

    failed = False
    colors = not argv.no_color and sys.stdout.isatty()
    for f in argv.files:
        if check_file(f, colors):
            failed = True

    if failed:
        sys.exit(1)
