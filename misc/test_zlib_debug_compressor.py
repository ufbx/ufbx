import zlib_debug_compressor as zz
import zlib
import sys

def test_dynamic():
    """Simple dynamic Huffman tree compressed block"""
    opts = zz.Options(force_block_types=[2])
    data = b"Hello Hello!"
    return data, zz.deflate(data, opts)

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

def test_fail_codelen_16_overflow():
    """Test oveflow of codelen symbol 16"""
    data = b"\xfd\xfe\xff"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Patch Litlen 254-256 repeat extra N to 4
    buf.data |= 1 << 0x66

    return data, buf

def test_fail_codelen_17_overflow():
    """Test oveflow of codelen symbol 17"""
    data = b"\xfc"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Patch Litlen 254-256 zero extra N to 5
    buf.data |= 2 << 0x6c

    return data, buf

def test_fail_codelen_18_overflow():
    """Test oveflow of codelen symbol 18"""
    data = b"\xf4"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Patch Litlen 254-256 extra N to 13
    buf.data |= 2 << 0x6a

    return data, buf

def test_fail_codelen_overfull():
    """Test bad codelen Huffman tree with too many symbols"""
    data = b"Codelen"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Over-filled Huffman tree
    buf.data |= 1 << 0x30

    return data, buf

def test_fail_codelen_underfull():
    """Test bad codelen Huffman tree too few symbols"""
    data = b"Codelen"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Under-filled Huffman tree
    buf.data &= ~(0b111 << 0x4e)
    buf.data |= 5 << 0x4e
    
    return data, buf

def test_fail_litlen_bad_huffman():
    """Test bad lit/len Huffman tree"""
    data = b"Literal/Length codes"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Under-filled Huffman tree
    buf.data &= ~(0b11 << 0x6d)
    buf.data |= 0b01 << 0x6d

    return data, buf

def test_fail_distance_bad_huffman():
    """Test bad distance Huffman tree"""
    data = b"Dist Dist .. Dist"
    opts = zz.Options(force_block_types=[2])
    buf = zz.deflate(data, opts)
    
    # Under-filled Huffman tree
    buf.data &= ~(0b1111 << 0xb1)
    buf.data |= 0b1111 << 0xb1

    return data, buf
test_cases = [
    test_dynamic,
    test_repeat_length,
    test_huff_lengths,
    test_multi_part_matches,
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

_, buf = test_fail_codelen_underfull()
zz.print_buf(buf)
zz.print_bytes(buf.to_bytes())
zlib.decompress(buf.to_bytes())

sys.exit(0 if good else 1)
        
