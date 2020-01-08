import zlib_debug_compressor as zz
import zlib
import sys
import itertools

def test_dynamic():
    """Simple dynamic Huffman tree compressed block"""
    opts = zz.Options(force_block_types=[2])
    data = b"Hello Hello!"
    return data, zz.deflate(data, opts)

def test_dynamic_no_match():
    """Simple dynamic Huffman tree without matches"""
    opts = zz.Options(force_block_types=[2])
    data = b"Hello World!"
    return data, zz.deflate(data, opts)

def test_dynamic_rle():
    """Simple dynamic Huffman with a single repeating match"""
    opts = zz.Options(force_block_types=[2])
    data = b"AAAAAAAAAAAAAAAAA"
    message = [zz.Literal(b"A"), zz.Match(16, 1)]
    return data, zz.compress_message(message, opts)

def test_repeat_length():
    """Dynamic Huffman compressed block with repeat lengths"""
    data = b"ABCDEFGHIJKLMNOPQRSTUVWXYZZYXWVUTSRQPONMLKJIHGFEDCBA"
    return data, zz.deflate(data)

def test_huff_lengths():
    """Test all possible lit/len code lengths"""
    data = b"0123456789ABCDE"
    freq = 1
    probs = { }
    for c in data:
        probs[c] = freq
        freq *= 2
    opts = zz.Options(force_block_types=[2], override_litlen_counts=probs)
    return data, zz.deflate(data, opts)

def test_multi_part_matches():
    """Matches that refer to earlier compression blocks"""
    data = b"Test Part Data Data Test Data Part New Test Data"
    opts = zz.Options(block_size=4, force_block_types=[0,1,2,0,1,2])
    return data, zz.deflate(data, opts)

def create_match_distances_and_lengths_message():
    lens = [3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,
            23,24,25,26,27,28,29,30,31,32,33,34,35,39,42,43,48,50,51,
            55,58,59,63,66,67,70,82,83,90,98,99,105,114,115,120,130,
            131,140,150,162,163,170,180,194,195,200,210,226,227,230,
            240,250,257,258]
    dists = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,20,24,25,28,
             32,33,40,48,49,50,64,65,75,96,97,110,128,129,160,192,
             193,230,256,257,330,384,385,400,512,513,600,768,769,
             900,1024,1025,1250,1536,1537,1800,2048,2049,2500,3072,
             3073,3500,4096,4097,5000,6144,6145,7000,8192,8193,10000,
             12288,12289,14000,16384,16385,20000,24576,24577,25000,
             26000, 27000, 28000, 29000, 30000, 31000, 32768, 32768+300]

    message = []
    
    l_iter = itertools.chain(lens, itertools.repeat(lens[-1]))
    lit_iter = itertools.cycle(range(0,256))
    pos = 0
    prev_d = 1
    for d in dists:
        while pos < d:
            l = next(l_iter)
            pos += l
            message.append(zz.Literal(bytes([next(lit_iter), next(lit_iter)])))
            message.append(zz.Match(l, prev_d))
        prev_d = d
    return message

def test_static_distances_and_lengths():
    """Test all possible match length and distance buckets (Static)"""
    message = create_match_distances_and_lengths_message()
    opts = zz.Options(block_size=4294967296, force_block_types=[1])
    data = zz.decode(message)
    return data, zz.compress_message(message, opts)

def test_dynamic_distances_and_lengths():
    """Test all possible match length and distance buckets (Dynamic)"""
    message = create_match_distances_and_lengths_message()
    opts = zz.Options(block_size=4294967296, force_block_types=[2])
    data = zz.decode(message)
    return data, zz.compress_message(message, opts)

def test_long_codes():
    """Test longest possible bit-codes for symbols"""
    message = [zz.Literal(b"test")]
    pos = 0
    matches = [(140,10000),(180,14000),(210,20000),(230,30000)]
    while pos < 30000:
        message.append(zz.Match(258, 4))
        next_pos = pos + 258
        for l,o in matches:
            if pos < o and next_pos >= o:
                for n in range(5):
                    for m in range(n - 1):
                        message.append(zz.Literal(bytes([ord("A") + m])))
                    message.append(zz.Match(l, o))
                    next_pos += l
                    l += 1
        pos = next_pos

    ll_override = { }
    count = 1000000000
    for ll in itertools.chain([285], b"Test", range(260,284)):
        ll_override[ll] = count
        count /= 2

    dist_override = { }
    count = 1000000000
    for dist in itertools.chain([3], range(10,28)):
        dist_override[dist] = count
        count /= 2

    opts = zz.Options(block_size=4294967296, force_block_types=[2],
                      override_litlen_counts=ll_override,
                      override_dist_counts=dist_override)
    data = zz.decode(message)
    return data, zz.compress_message(message, opts)

def test_fail_codelen_16_overflow():
    """Test oveflow of codelen symbol 16"""
    data = b"\xfd\xfe\xff"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Patch Litlen 254-256 repeat extra N to 4
    buf.patch(0x66, 1, 2)

    return data, buf

def test_fail_codelen_17_overflow():
    """Test oveflow of codelen symbol 17"""
    data = b"\xfc"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Patch Litlen 254-256 zero extra N to 5
    buf.patch(0x6c, 2, 3)

    return data, buf

def test_fail_codelen_18_overflow():
    """Test oveflow of codelen symbol 18"""
    data = b"\xf4"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Patch Litlen 254-256 extra N to 13
    buf.patch(0x6a, 2, 7)

    return data, buf

def test_fail_codelen_overfull():
    """Test bad codelen Huffman tree with too many symbols"""
    data = b"Codelen"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Over-filled Huffman tree
    buf.patch(0x30, 1, 3)

    return data, buf

def test_fail_codelen_underfull():
    """Test bad codelen Huffman tree too few symbols"""
    data = b"Codelen"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Under-filled Huffman tree
    buf.patch(0x4e, 5, 3)
    
    return data, buf

def test_fail_litlen_bad_huffman():
    """Test bad lit/len Huffman tree"""
    data = b"Literal/Length codes"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Under-filled Huffman tree
    buf.patch(0x6d, 1, 2)

    return data, buf

def test_fail_distance_bad_huffman():
    """Test bad distance Huffman tree"""
    data = b"Dist Dist .. Dist"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Under-filled Huffman tree
    buf.patch(0xb1, 0b1111, 4)

    return data, buf

def test_fail_bad_distance():
    """Test bad distance symbol (30..31)"""
    data = b"Dist Dist"
    opts = zz.Options(force_block_types=[1])
    buf = zz.deflate(data, opts)
    
    # Distance symbol 30
    buf.patch(0x42, 0b01111, 5)
    
    return data, buf

def test_fail_bad_distance_bit():
    """Test bad distance symbol in one symbol alphabet"""
    data = b"asd asd"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Distance code 1
    buf.patch(0xaa, 0b1, 1)
    
    return data, buf

def test_fail_bad_distance_empty():
    """Test using distance code from an empty tree"""
    data = b"asd asd"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)

    # Add another distance code and replace distance 3 code for 1 (0111)
    # with the code for 0 (00) for distances 3 and 4
    buf.patch(0x18, 4, 5)
    buf.patch(0x98, 0b0000, 4)
    
    return data, buf

def test_fail_bad_lit_length():
    """Test bad lit/length symbol"""
    data = b""
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)

    # Patch end-of-block 0 to 1
    buf.patch(0x6b, 0b1, 1)

    return data, buf

def fmt_bytes(data, cols=20):
    lines = []
    for begin in range(0, len(data), cols):
        chunk = data[begin:begin+cols]
        lines.append("\"" + "".join("\\x%02x" % c for c in chunk) + "\"")
    return "\n".join(lines)

def fnv1a(data):
    h = 0x811c9dc5
    for d in data:
        h = ((h ^ (d&0xff)) * 0x01000193) & 0xffffffff
    return h

test_cases = [
    test_dynamic,
    test_dynamic_no_match,
    test_dynamic_rle,
    test_repeat_length,
    test_huff_lengths,
    test_multi_part_matches,
    test_static_distances_and_lengths,
    test_dynamic_distances_and_lengths,
    test_long_codes,
]

good = True
for case in test_cases:
    try:
        data, buf = case()
        result = zlib.decompress(buf.to_bytes())
        if data != result:
            raise ValueError("Round trip failed")
        print("{}: OK".format(case.__name__))
    except Exception as e:
        print("{}: FAIL ({})".format(case.__name__, e))
        good = False

sys.exit(0 if good else 1)
