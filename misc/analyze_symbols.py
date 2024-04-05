import argparse
import re
from typing import NamedTuple, List

class Symbol(NamedTuple):
    address: int
    flags: str
    segment: str
    size: int
    name: str

RE_SYMBOL = re.compile(r"^([0-9A-Fa-f]+)\s+([a-zA-Z ]{7})\s+(\S+)\s+([0-9A-Fa-f]+)\s+(\S+)$")

def parse_symbols(f):
    for line in f:
        line = line.strip()
        m = RE_SYMBOL.match(line)
        if m:
            address, flags, segment, size, name = m.groups()
            yield Symbol(
                address=int(address, base=16),
                flags=flags.replace(" ", ""),
                segment=segment,
                size=int(size, base=16),
                name=name)

errors = []

def error(symbol: Symbol, message: str):
    errors.append((symbol, message))

def check_symbols(symbols: List[Symbol], argv):
    for s in symbols:
        if s.name.startswith("."):
            continue
        if "ufbxi_volatile_eps" in s.name:
            continue
        if s.segment in ("*ABS*", "*UND*"):
            continue
        segments = [".text", ".rodata", ".data.rel.ro.local", ".data.rel.ro"]
        if s.name == "ufbxi_zero_element":
            segments.append(".bss")
        if s.segment not in segments and not s.segment.startswith(".rodata."):
            error(s, f"bad segment: {s.segment}")
        if s.name.startswith("ufbxi_") and "l" not in s.flags:
            error(s, "internal symbol with global visibility")
        if argv.all_local and "l" not in s.flags:
            error(s, "symbol with global visibility")

if __name__ == "__main__":
    parser = argparse.ArgumentParser("analyze_symbols.py")
    parser.add_argument("symbols", help="Output from objdump -t")
    parser.add_argument("--all-local", action="store_true", help="Require all symbols to be local")
    argv = parser.parse_args()

    with open(argv.symbols, "rt") as f:
        symbols = list(parse_symbols(f))

    check_symbols(symbols, argv)

    if errors:
        name_width = max(len(e[0].name) for e in errors)
        for symbol, message in errors:
            print(f"{symbol.name:<{name_width}}  {message}")
        if errors:
            exit(1)
