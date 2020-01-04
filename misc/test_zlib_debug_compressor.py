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

sys.exit(0 if good else 1)
        
