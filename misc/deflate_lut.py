import itertools

# Generate lookup table for `base + read(bits)` Deflate operands
def deflate_lookup_table(pairs):
    for (base, bits) in pairs:
        mask = (1 << bits) - 1
        assert mask < (1 << 13)
        assert bits < (1 << 4)
        assert base < (1 << 15)
        yield mask | (bits << 13) | (base << 17)

def format_table(data, cols):
    data = list(data)
    for base in range(0, len(data), cols):
        yield ''.join('0x{:08x}, '.format(x) for x in data[base:base+cols])

# Deflate RFC 1951 3.2.5. tables
length_operands = [
    (3,0),(4,0),(5,0),(6,0),(7,0),(8,0),(9,0),(10,0),(11,1),
    (13,1),(15,1),(17,1),(19,2),(23,2),(27,2),(31,2),(35,3),
    (43,3),(51,3),(59,3),(67,4),(83,4),(99,4),(115,4),(131,5),
    (163,5),(195,5),(227,5),(258,0),(0,0),(0,0),
]
dist_operands = [
    (1,0),(2,0),(3,0),(4,0),(5,1),(7,1),(9,2),(13,2),(17,3),
    (25,3),(33,4),(49,4),(65,5),(97,5),(129,6),(193,6),(257,7),
    (385,7),(513,8),(769,8),(1025,9),(1537,9),(2049,10),(3073,10),
    (4097,11),(6145,11),(8193,12),(12289,12),(16385,13),(24577,13),
]

print('static const uint32_t bfbxz_length_lut[] = {')
table = deflate_lookup_table(length_operands)
print('\n'.join('\t' + t for t in format_table(table, 8)))
print('};')

print('static const uint32_t bfbxz_dist_lut[] = {')
table = deflate_lookup_table(dist_operands)
print('\n'.join('\t' + t for t in format_table(table, 8)))
print('};')
