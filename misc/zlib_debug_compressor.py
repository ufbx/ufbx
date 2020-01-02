import zlib
from collections import namedtuple
import itertools

Code = namedtuple("Code", "code bits")
IntCoding = namedtuple("IntCoding", "symbol base bits")
BinDesc = namedtuple("BinDesc", "offset value bits desc")
SymExtra = namedtuple("Code", "symbol extra bits")

null_code = Code(0,0)

def make_int_coding(first_symbol, first_value, bit_sizes):
    symbol = first_symbol
    value = first_value
    codings = []
    for bits in bit_sizes:
        codings.append(IntCoding(symbol, value, bits))
        value += 1 << bits
        symbol += 1
    return codings

length_coding = make_int_coding(257, 3, [
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,
])

distance_coding = make_int_coding(0, 1, [
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,
])

def find_int_coding(codes, value):
    for coding in codes:
        if value < coding.base + (1 << coding.bits):
            return coding

class BitBuf:
    def __init__(self):
        self.pos = 0
        self.data = 0
        self.desc = []

    def push(self, val, bits, desc=""):
        assert val < 1 << bits
        val = int(val)
        self.desc.append(BinDesc(self.pos, val, bits, desc))
        self.data |= val << self.pos
        self.pos += bits

    def push_rev(self, val, bits, desc=""):
        assert val < 1 << bits
        rev = 0
        for n in range(bits):
            rev |= ((val >> n) & 1) << bits-n-1
        self.push(rev, bits, desc)

    def push_code(self, code, desc=""):
        self.push(code.code, code.bits, desc)
    def push_rev_code(self, code, desc=""):
        self.push_rev(code.code, code.bits, desc)

    def to_bytes(self):
        return bytes((self.data>>p&0xff) for p in range(0, self.pos, 8))

class Literal:
    def __init__(self, data):
        self.data = data

    def count_codes(self, litlen_count, dist_count):
        for c in self.data:
            litlen_count[c] += 1

    def encode(self, buf, litlen_syms, dist_syms):
        for c in self.data:
            sym = litlen_syms[c]
            buf.push_rev_code(sym, "Literal '{}'".format(chr(c)))

    def decode(self, history):
        return self.data

class Match:
    def __init__(self, length, distance):
        self.length = length
        self.distance = distance
        self.lcode = find_int_coding(length_coding, length)
        self.dcode = find_int_coding(distance_coding, distance)

    def count_codes(self, litlen_count, dist_count):
        litlen_count[self.lcode.symbol] += 1
        dist_count[self.dcode.symbol] += 1

    def encode(self, buf, litlen_syms, dist_syms):
        lsym = litlen_syms[self.lcode.symbol]
        dsym = dist_syms[self.dcode.symbol]
        buf.push_rev_code(lsym, "Length")
        if self.lcode.bits > 0:
            buf.push(self.length - self.lcode.base, self.lcode.bits, "Length extra")
        buf.push_rev_code(dsym, "Distance")
        if self.dcode.bits > 0:
            buf.push(self.distance - self.dcode.base, self.dcode.bits, "Distance extra")
    
    def decode(self, history):
        # TODO: RLE!
        return history[-self.distance:-self.distance + self.length]


def make_huffman(syms, max_code_length):
    """Build a canonical Huffman tree containing `syms`

    `syms` must be a dict mapping from symbol to probability.
    `max_code_length` is the maximum length of the resulting Huffman codes.
    The function will return a dict mapping from symbol to its code.
    """

    if len(syms) == 0:
        return { }
    if len(syms) == 1:
        return { next(iter(syms)): Code(0, 1) }

    sym_groups = ((prob, (sym,)) for sym,prob in syms.items())
    initial_groups = list(sorted(sym_groups))
    groups = initial_groups

    for n in range(max_code_length-1):
        packaged = [(a[0]+b[0], a[1]+b[1]) for a,b in zip(groups[0::2], groups[1::2])]
        groups = list(sorted(packaged + initial_groups))

    sym_bits = { }
    for g in groups[:(len(syms) - 1) * 2]:
        for sym in g[1]:
            sym_bits[sym] = sym_bits.get(sym, 0) + 1

    bl_count = [0] * (max_code_length + 1)
    next_code = [0] * (max_code_length + 1)
    for bits in sym_bits.values():
        bl_count[bits] += 1
    code = 0
    for n in range(1, max_code_length + 1):
        code = (code + bl_count[n - 1]) << 1
        next_code[n] = code

    codes = { }
    for sym,bits in sorted(sym_bits.items()):
        codes[sym] = Code(next_code[bits], bits)
        next_code[bits] += 1

    return codes

def print_huffman(tree):
    width = max(len(str(s)) for s in tree.keys())
    for sym,code in tree.items():
        print("{:{}} {:0{}b}".format(sym, width, code.code, code.bits))

def decode(message):
    result = b""
    for m in message:
        result += m.decode(result)
    return result

def encode_huff_bits(bits):
    encoded = []
    for value,copies in itertools.groupby(bits):
        num = len(list(copies))
        if value == 0:
            while num >= 11:
                amount = min(num, 138)
                encoded.append(SymExtra(18, amount-11, 7))
                num -= amount
            while num >= 3:
                amount = min(num, 10)
                encoded.append(SymExtra(17, amount-3, 3))
                num -= amount
            while num >= 1:
                encoded.append(SymExtra(0, 0, 0))
                num -= 1
        else:
            encoded.append(SymExtra(value, 0, 0))
            num -= 1
            while num >= 3:
                amount = min(num, 3)
                encoded.append(SymExtra(16, amount-3, 2))
                num -= amount
            while num >= 1:
                encoded.append(SymExtra(value, 0, 0))
                num -= 1
    return encoded

def write_encoded_huff_bits(buf, codes, syms, desc):
    value = 0
    prev = 0
    for code in codes:
        sym = code.symbol
        num = 1
        if sym <= 15:
            buf.push_rev_code(syms[sym], "{} {} bits: {}".format(desc, value, sym))
            prev = sym
        elif sym == 16:
            num = code.extra + 3
            buf.push_rev_code(syms[sym], "{} {}-{} bits: {}".format(desc, value, value+num-1, prev))
        elif sym == 17:
            num = code.extra + 3
            buf.push_rev_code(syms[sym], "{} {}-{} bits: {}".format(desc, value, value+num-1, 0))
        elif sym == 18:
            num = code.extra + 11
            buf.push_rev_code(syms[sym], "{} {}-{} bits: {}".format(desc, value, value+num-1, 0))
        value += num
        if code.bits > 0:
            buf.push(code.extra, code.bits, "{} N={}".format(desc, num))

def compress_block(buf, message, final, override_litlen_counts={}):
    litlen_count = [0] * 286
    distance_count = [0] * 30

    # There's always one end-of-block
    litlen_count[256] = 1

    for m in message:
        m.count_codes(litlen_count, distance_count)

    for sym,count in override_litlen_counts.items():
        litlen_count[sym] = count

    litlen_map = { sym: count for sym,count in enumerate(litlen_count) if count > 0 }
    distance_map = { sym: count for sym,count in enumerate(distance_count) if count > 0 }

    litlen_syms = make_huffman(litlen_map, 16)
    distance_syms = make_huffman(distance_map, 16)

    num_litlens = max(itertools.chain((k for k in litlen_map.keys()), (256,))) + 1
    num_distances = max(itertools.chain((k for k in distance_map.keys()), (0,))) + 1

    litlen_bits = [litlen_syms.get(s, null_code).bits for s in range(num_litlens)]
    distance_bits = [distance_syms.get(s, null_code).bits for s in range(num_distances)]

    litlen_bit_codes = encode_huff_bits(litlen_bits)
    distance_bit_codes = encode_huff_bits(distance_bits)

    codelen_count = [0] * 20
    for code in itertools.chain(litlen_bit_codes, distance_bit_codes):
        codelen_count[code.symbol] += 1

    codelen_map = { sym: count for sym,count in enumerate(codelen_count) if count > 0 }
    codelen_syms = make_huffman(codelen_map, 8)

    codelen_permutation = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]

    num_codelens = 0
    for i, p in enumerate(codelen_permutation):
        if codelen_count[p] > 0:
            num_codelens = i + 1
    num_codelens = max(num_codelens, 4)

    buf.push(final, 1, "BFINAL")
    buf.push(0b10, 2, "BTYPE")

    buf.push(num_litlens - 257, 5, "HLIT")
    buf.push(num_distances - 1, 5, "HDIST")
    buf.push(num_codelens - 4, 4, "HCLEN")

    for p in codelen_permutation[:num_codelens]:
        bits = 0
        if p in codelen_syms:
            bits = codelen_syms[p].bits
        buf.push(bits, 3, "Codelen {} bits".format(p))

    write_encoded_huff_bits(buf, litlen_bit_codes, codelen_syms, "Litlen")
    write_encoded_huff_bits(buf, distance_bit_codes, codelen_syms, "Distance")

    for m in message:
        m.encode(buf, litlen_syms, distance_syms)

    # End-of-block
    buf.push_rev_code(litlen_syms[256], "End-of-block")

def compress_message(message):
    buf = BitBuf()

    # ZLIB CFM byte
    buf.push(8, 4, "CM")    # CM=8     Compression method: DEFLATE
    buf.push(7, 4, "CINFO") # CINFO=7  Compression info: 32kB window size

    # ZLIB FLG byte
    buf.push(28, 5, "FCHECK") # FCHECK    (CMF*256+FLG) % 31 == 0
    buf.push(0, 1, "FDICT")   # FDICT=0   Preset dictionary: No
    buf.push(2, 2, "FLEVEL")  # FLEVEL=2  Compression level: Default

    compress_block(buf, message, True)

    # Pad to byte
    buf.push(0, -buf.pos & 7, "Pad to byte")

    print(decode(message))
    adler_hash = zlib.adler32(decode(message))

    buf.push((adler_hash >> 24) & 0xff, 8, "Adler[24:32]")
    buf.push((adler_hash >> 16) & 0xff, 8, "Adler[16:24]")
    buf.push((adler_hash >>  8) & 0xff, 8, "Adler[8:16]")
    buf.push((adler_hash >>  0) & 0xff, 8, "Adler[0:8]")

    return buf

message = [Literal(b"Hello World!"), Match(6, 7), Literal(b"?")]
buf = compress_message(message)
encoded = buf.to_bytes()

for d in buf.desc:
    val = "({0}) {0:0{1}b}".format(d.value, d.bits)
    print("{:>8x} {:>4} | {:>16} | {}".format(d.offset, d.bits, val, d.desc))

print("".join("\\x%02x" % b for b in encoded))
print(zlib.decompress(encoded))

