
UFBXT_TEST(bits_simple)
#if UFBXT_IMPL
{
	char data[3] = "\xab\xcd\xef";
	ufbxi_bit_stream s;
	ufbxi_bit_init(&s, data, sizeof(data));

	uint64_t bits = ufbxi_bit_read(&s, 0);
	ufbxt_assert(bits == UINT64_C(0x00000000EFCDAB));
	bits = ufbxi_bit_read(&s, 1);
	ufbxt_assert(bits == UINT64_C(0x00000000EFCDAB) >> 1);
}
#endif

UFBXT_TEST(bits_small)
#if UFBXT_IMPL
{
	char data[] = "Hello world!";
	ufbxi_bit_stream s;
	ufbxi_bit_init(&s, data, sizeof(data) - 1);
	uint64_t lo = UINT64_C(0x6f77206f6c6c6548);
	uint64_t hi = UINT64_C(0x0000000021646c72);

	for (uint64_t pos = 0; pos < 256; pos++) {
		uint64_t bits = ufbxi_bit_read(&s, pos);
		uint64_t ref;
		if (pos == 0) ref = lo;
		else if (pos < 64) ref = lo >> pos | hi << (64 - pos);
		else if (pos < 128) ref = hi >> pos;
		else ref = 0;
		ufbxt_assert(bits == ref);
	}
}
#endif

UFBXT_TEST(bits_long_bytes)
#if UFBXT_IMPL
{
	char data[1024];
	for (size_t i = 0; i < sizeof(data); i++) {
		data[i] = (char)i;
	}

	for (size_t align = 0; align < 8; align++) {
		ufbxi_bit_stream s;
		ufbxi_bit_init(&s, data + align, sizeof(data) - 8);

		uint64_t bits = 0;
		uint64_t pos = 0;
		for (size_t i = 0; i < sizeof(data) - 8; i++) {
			bits = ufbxi_bit_read(&s, pos);
			ufbxt_assert((uint8_t)(bits & 0xff) == (uint8_t)data[i + align]);
			bits >>= 8;
			pos += 8;
		}

		for (size_t i = 0; i < 128; i++) {
			bits = ufbxi_bit_read(&s, pos);
			ufbxt_assert(bits == 0);
			bits >>= 8;
			pos += 8;
		}
	}
}
#endif

UFBXT_TEST(bits_long_bits)
#if UFBXT_IMPL
{
	char data[1024];
	for (size_t i = 0; i < sizeof(data); i++) {
		data[i] = (char)i;
	}

	for (size_t align = 0; align < 8; align++) {
		ufbxi_bit_stream s;
		ufbxi_bit_init(&s, data + align, sizeof(data) - 8);

		uint64_t bits = 0;
		uint64_t pos = 0;
		for (size_t i = 0; i < sizeof(data) - 8; i++) {
			for (size_t bit = 0; bit < 8; bit++) {
				bits = ufbxi_bit_read(&s, pos);
				uint8_t byte = (uint8_t)data[i + align];
				ufbxt_assert((bits & 1) == ((byte >> bit) & 1));
				bits >>= 1;
				pos += 1;
			}
		}

		for (size_t i = 0; i < 128; i++) {
			bits = ufbxi_bit_read(&s, pos);
			ufbxt_assert(bits == 0);
			bits >>= 1;
			pos += 1;
		}
	}
}
#endif

UFBXT_TEST(bits_empty)
#if UFBXT_IMPL
{
	ufbxi_bit_stream s;
	ufbxi_bit_init(&s, NULL, 0);

	uint64_t bits = ufbxi_bit_read(&s, 0);
	ufbxt_assert(bits == 0);
}
#endif

#if UFBXT_IMPL
static void
test_huff_range(ufbxi_huff_tree *tree, uint32_t begin, uint32_t end, uint32_t num_bits, uint32_t code_begin)
{
	for (uint32_t i = 0; i <= end - begin; i++) {
		uint64_t code = code_begin + i;
		uint64_t rev_code = 0;
		for (uint32_t bit = 0; bit < num_bits; bit++) {
			if (code & (1 << bit)) rev_code |= 1 << (num_bits - bit - 1);
		}

		uint32_t hi_max = 1 << (12 - num_bits);
		for (uint32_t hi = 0; hi < hi_max; hi++) {
			uint64_t bits = rev_code | (hi << num_bits);
			uint64_t pos = 0;
			uint32_t value = ufbxi_huff_decode_bits(tree, &bits, &pos);
			ufbxt_assert(pos == num_bits);
			ufbxt_assert(bits == hi);
			ufbxt_assert(value == begin + i);
		}
	}
}
#endif

UFBXT_TEST(huff_static_lit_length)
#if UFBXT_IMPL
{
	ufbxi_deflate_context dc;
	ufbxi_init_static_huff(&dc);

	test_huff_range(&dc.huff_lit_length, 0, 143, 8, 0x30);
	test_huff_range(&dc.huff_lit_length, 144, 255, 9, 0x190);
	test_huff_range(&dc.huff_lit_length, 256, 279, 7, 0x0);
	test_huff_range(&dc.huff_lit_length, 280, 287, 8, 0xc0);
}
#endif

UFBXT_TEST(huff_static_dist)
#if UFBXT_IMPL
{
	ufbxi_deflate_context dc;
	ufbxi_init_static_huff(&dc);

	test_huff_range(&dc.huff_dist, 0, 31, 5, 0x0);
}
#endif


UFBXT_TEST(deflate_empty)
#if UFBXT_IMPL
{
	char src[1], dst[1];
	ptrdiff_t res = ufbxi_inflate(dst, 1, src, 0);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res != 0);
}
#endif

UFBXT_TEST(deflate_simple)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello!\x07\xa2\x02\x16";
	char dst[6];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 6);
	ufbxt_assert(!memcmp(dst, "Hello!", 6));
}
#endif

UFBXT_TEST(deflate_simple_chunks)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x00\x06\x00\xf9\xffHello \x01\x06\x00\xf9\xffworld!\x1d\x09\x04\x5e";
	char dst[12];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 12);
	ufbxt_assert(!memcmp(dst, "Hello world!", 12));
}
#endif

UFBXT_TEST(deflate_static)
#if UFBXT_IMPL
{
	const char src[] = "x\xda\xf3H\xcd\xc9\xc9W(\xcf/\xcaIQ\x04\x00\x1d\t\x04^";
	char dst[12];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 12);
	ufbxt_assert(!memcmp(dst, "Hello world!", 12));
}
#endif

UFBXT_TEST(deflate_static_match)
#if UFBXT_IMPL
{
	const char src[] = "x\xda\xf3H\xcd\xc9\xc9W\xf0\x00\x91\x8a\x00\x1b\xbb\x04*";
	char dst[12];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 12);
	ufbxt_assert(!memcmp(dst, "Hello Hello!", 12));
}
#endif

UFBXT_TEST(deflate_static_rle)
#if UFBXT_IMPL
{
	const char src[] = "x\xdastD\x00\x00\x13\xda\x03\r";
	char dst[12];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 12);
	ufbxt_assert(!memcmp(dst, "AAAAAAAAAAAA", 12));
}
#endif

UFBXT_TEST(deflate_dynamic)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x1d\xc4\x31\x0d\x00\x00\x0c\x02\x41\x2b\xad"
		"\x1b\x8c\xb0\x7d\x82\xff\x8d\x84\xe5\x64\xc8\xcd\x2f\x1b\xbb\x04\x2a";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 12);
	ufbxt_assert(!memcmp(dst, "Hello Hello!", 12));
}
#endif

UFBXT_TEST(deflate_repeat_length)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\x00\x05\x0d\x00\x20\x2c\x1b\xee\x0e\xb7"
		"\xfe\x41\x98\xd2\xc6\x3a\x1f\x62\xca\xa5\xb6\x3e\xe6\xda\xe7\x3e\x40"
		"\x62\x11\x26\x84\x77\xcf\x5e\x73\xf4\x56\x4b\x4e\x31\x78\x67\x8d\x56\x1f\xa1\x6e\x0f\xbf";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 52);
	ufbxt_assert(!memcmp(dst, "ABCDEFGHIJKLMNOPQRSTUVWXYZZYXWVUTSRQPONMLKJIHGFEDCBA", 52));
}
#endif

UFBXT_TEST(deflate_huff_lengths)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\xe0\xc1\x95\x65\x59\x96\x65\xd9\xb1\x84"
		"\xca\x70\x53\xf9\xaf\x79\xcf\x5e\x93\x7f\x96\x30\xfe\x7f\xff\xdf\xff"
		"\xfb\xbf\xff\xfd\xf7\xef\xef\xf7\xbd\x5b\xfe\xff\x19\x28\x03\x5d";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 15);
	ufbxt_assert(!memcmp(dst, "0123456789ABCDE", 15));
}
#endif

UFBXT_TEST(deflate_multi_part_matches)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x00\x04\x00\xfb\xff\x54\x65\x73\x74\x52\x08"
		"\x48\x2c\x02\x10\x00\x06\x32\x00\x00\x00\x0c\x52\x39\xcc\x45\x72\xc8"
		"\x7f\xcd\x9d\x00\x08\x00\xf7\xff\x74\x61\x20\x44\x61\x74\x61\x20\x02"
		"\x8b\x01\x38\x8c\x43\x12\x00\x00\x00\x00\x40\xff\x5f\x0b\x36\x8b\xc0"
		"\x12\x80\xf9\xa5\x96\x23\x84\x00\x8e\x36\x10\x41";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == 48);
	ufbxt_assert(!memcmp(dst, "Test Part Data Data Test Data Part New Test Data", 48));
}
#endif

UFBXT_TEST(deflate_fail_cfm)
#if UFBXT_IMPL
{
	const char src[] = "\x79\x9c";
	char dst[4];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -1);
}
#endif

UFBXT_TEST(deflate_fail_fdict)
#if UFBXT_IMPL
{
	const char src[] = "\x78\xbc";
	char dst[4];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -2);
}
#endif

UFBXT_TEST(deflate_fail_fcheck)
#if UFBXT_IMPL
{
	const char src[] = "\x78\0x9d";
	char dst[4];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -3);
}
#endif

UFBXT_TEST(deflate_fail_nlen)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf8\xffHello!\x07\xa2\x02\x16";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -4);
}
#endif

UFBXT_TEST(deflate_fail_dst_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello!\x07\xa2\x02\x16";
	char dst[5];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -6);
}
#endif

UFBXT_TEST(deflate_fail_src_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -5);
}
#endif

UFBXT_TEST(deflate_fail_bad_block)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x07\x08\x00\xf8\xff";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -7);
}
#endif

UFBXT_TEST(deflate_fail_bad_truncated_checksum)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello!\x07\xa2\x02";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -8);
}
#endif

UFBXT_TEST(deflate_fail_bad_checksum)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello!\x07\xa2\x02\xff";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -9);
}
#endif

UFBXT_TEST(deflate_fail_codelen_16_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\x80\x85\x0c\x00\x00\x00\xc0\xfc\xa1\x5f\xc3\x06\x05\xf5\x02\xfb";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -18);
}
#endif

UFBXT_TEST(deflate_fail_codelen_17_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\xc0\xb1\x0c\x00\x00\x00\x00\x20\x7f\xe7\xae\x26\x00\xfd\x00\xfd";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -19);
}
#endif

UFBXT_TEST(deflate_fail_codelen_18_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\xc0\x81\x08\x00\x00\x00\x00\x20\x7f\xdf\x09\x4e\x00\xf5\x00\xf5";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -20);
}
#endif

UFBXT_TEST(deflate_fail_codelen_overfull)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\x80\x31\x11\x01\x00\x00\x01\xc3\xa9\xe2\x37\x47\xff\xcd\x69\x26\xf4\x0a\x7a\x02\xbb";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -14);
}
#endif

UFBXT_TEST(deflate_fail_codelen_underfull)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\x80\x31\x11\x00\x00\x00\x41\xc3\xa9\xe2\x37\x47\xff\xcd\x69\x26\xf4\x0a\x7a\x02\xbb";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -15);
}
#endif

UFBXT_TEST(deflate_fail_litlen_bad_huffman)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\x40\x81\x09\x00\x20\x08\x7b\xa5\x0f\x7a\xa4\x27\xa2"
		"\x46\x0a\xa2\xa0\xfb\x1f\x11\x23\xea\xf8\x16\xc4\xa7\xae\x9b\x0f\x3d\x4e\xe4\x07\x8d";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -17);
}
#endif

UFBXT_TEST(deflate_fail_distance_bad_huffman)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x1d\xc5\x31\x0d\x00\x00\x0c\x02\x41\x2b\x55\x80\x8a\x9a"
		"\x61\x06\xff\x21\xf9\xe5\xfe\x9d\x1e\x48\x3c\x31\xba\x05\x79";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -23);
}
#endif

UFBXT_TEST(deflate_fail_bad_distance)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x73\xc9\x2c\x2e\x51\x00\x3d\x00\x0f\xd7\x03\x49";
	char dst[64];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -11);
}
#endif

UFBXT_TEST(deflate_fail_literal_overflow)
#if UFBXT_IMPL
{
	const char src[] = "x\xda\xf3H\xcd\xc9\xc9W(\xcf/\xcaIQ\x04\x00\x1d\t\x04^";
	char dst[8];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -10);
}
#endif

UFBXT_TEST(deflate_fail_match_overflow)
#if UFBXT_IMPL
{
	const char src[] = "x\xda\xf3H\xcd\xc9\xc9W\xf0\x00\x91\x8a\x00\x1b\xbb\x04*";
	char dst[8];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -12);
}
#endif

UFBXT_TEST(deflate_fail_bad_distance_bit)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x0d\xc3\x41\x09\x00\x00\x00\xc2\xc0\x2a\x56\x13"
		"\x6c\x60\x7f\xd8\x1e\xd7\x2f\x06\x0a\x41\x02\x91";
	char dst[8];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -11);
}
#endif

UFBXT_TEST(deflate_fail_bad_lit_length)
#if UFBXT_IMPL
{
	char src[] =
		"\x78\x9c\x05\xc0\x81\x08\x00\x00\x00\x00\x20\x7f\xeb\x0b\x00\x00\x00\x01";
	char dst[8];
	ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == -13);
}
#endif

UFBXT_TEST(deflate_bit_flip)
#if UFBXT_IMPL
{
	char src[] = "\x78\x9c\x00\x04\x00\xfb\xff\x54\x65\x73\x74\x52\x08"
		"\x48\x2c\x02\x10\x00\x06\x32\x00\x00\x00\x0c\x52\x39\xcc\x45\x72\xc8"
		"\x7f\xcd\x9d\x00\x08\x00\xf7\xff\x74\x61\x20\x44\x61\x74\x61\x20\x02"
		"\x8b\x01\x38\x8c\x43\x12\x00\x00\x00\x00\x40\xff\x5f\x0b\x36\x8b\xc0"
		"\x12\x80\xf9\xa5\x96\x23\x84\x00\x8e\x36\x10\x41";

	char dst[64];
	int num_res[64] = { 0 };

	for (size_t byte_ix = 0; byte_ix < sizeof(src) - 1; byte_ix++) {
		for (size_t bit_ix = 0; bit_ix < 8; bit_ix++) {
			size_t bit = 1 << bit_ix;

			ufbxt_hintf("byte_ix==%u && bit_ix==%u", (unsigned)byte_ix, (unsigned)bit_ix);

			src[byte_ix] ^= bit;
			ptrdiff_t res = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
			src[byte_ix] ^= bit;

			res = -res;
			if (res < 0) res = 0;
			if (res > ufbxi_arraycount(num_res)) res = ufbxi_arraycount(num_res);
			num_res[res]++;
		}
	}

	char line[128], *ptr = line, *end = line + sizeof(line);
	for (size_t i = 0; i < ufbxi_arraycount(num_res); i++) {
		if (num_res[i] > 0) {
			ptr += snprintf(ptr, end - ptr, "%3d:%3d    ", -(int)i, num_res[i]);
			if (ptr - line > 70) {
				ufbxt_logf("%s", line);
				ptr = line;
			}
		}
	}
}
#endif

UFBXT_TEST(deflate_match_distances_and_lengths)
#if UFBXT_IMPL
{
	const char src[] =
		"\x78\x9c\xed\x9d\x47\x6e\x2b\x31\x10\x44\xaf\x42\xde\x86\x07\x21\x77\x05\x14\xc0"
		"\x4d\x5f\xff\x7f\x67\x85\x09\xca\x9a\xf0\x0c\x03\x36\x6c\x49\x33\xc3\x4c\x76\x57"
		"\xbd\xf2\xff\xab\x7d\x7c\xeb\xf3\x87\xbe\x7e\xf7\xf7\x1f\xd2\xcf\x3f\xe2\xf7\x05"
		"\xfe\x7b\x5d\x3f\x78\x87\x0e\xdf\x51\x8f\x3e\x2a\x1f\x7f\x54\x39\xb9\x40\x3b\xbd"
		"\x9a\xce\x6e\x40\xe7\x37\xe0\x81\xfb\x4b\x43\xf7\x17\x83\x0f\xe0\xe1\x07\xe8\x23"
		"\x4f\xa9\xb1\x02\xa8\xa3\x05\x90\xc7\x0b\xa0\x4c\x94\x64\x9b\x2a\x49\x4d\xd6\x87"
		"\xa6\xeb\xc3\x33\xf5\x91\xe6\xea\x23\x66\x1b\x80\xe7\x1b\x40\xbf\xa0\x01\xe8\xc2"
		"\x76\x55\x2f\x6f\x57\xf9\x8a\x76\x55\xae\x6d\x57\xed\xfa\x0e\xa0\x1b\x3a\x80\x6e"
		"\xec\x83\xbe\xbd\x0f\xa6\x3b\xfa\x60\xdc\x39\x00\xf8\xfe\x01\xa0\x3f\x60\x00\xd0"
		"\x63\x46\xb8\xfa\xf8\x11\x2e\x3f\x61\x84\x2b\xcf\x1b\xe1\xda\xf3\x27\x00\xbd\x60"
		"\x02\xd0\xcb\x26\x00\xbf\x7e\x1e\x4c\x6f\x98\x07\xe3\x5d\x0b\x00\xbf\x7f\x01\xd0"
		"\x17\xb0\x00\xd0\x92\xd6\x70\x75\x99\x6b\xb8\xbc\xfc\x35\x5c\x59\xc1\x1a\xae\xad"
		"\x67\x03\xa0\x35\x6e\x00\xb4\xfe\x0d\x80\x37\xb0\x57\x4b\x5b\xd9\xab\xc5\xf6\xf6"
		"\x6a\xde\xfe\x01\x40\xdf\xc1\x01\x80\xf6\x71\x00\x50\xf7\x76\x00\x90\xf7\x7b\x0e"
		"\x57\x76\x7f\x0e\xd7\x38\x87\x13\xe7\x70\x22\x00\x60\x02\x00\x89\x00\x40\x10\x00"
		"\x30\x71\xb5\x4e\x5c\x4d\xc4\xd5\x2a\x71\xb5\x4c\x02\x40\x21\x01\xa0\x91\x00\x20"
		"\x12\x00\x44\x02\x80\x49\x00\x48\x24\x00\x04\xf9\x70\x26\x1f\xae\x93\x0f\x27\xf2"
		"\xe1\x2a\xf9\x70\x99\x7c\xb8\x42\x3e\x5c\x23\x1f\x4e\xe4\xc3\x09\x01\x80\x11\x00"
		"\x24\x04\x00\x81\x00\xc0\x08\x00\x3a\x02\x00\x21\x00\xa8\xe8\xd5\x32\x7a\xb5\x82"
		"\x5e\xad\xa1\x57\x13\x7a\x35\xa1\x57\x33\x7a\xb5\x84\x5e\x2d\xd0\xab\x19\x03\x80"
		"\x8e\x01\x80\x30\x00\xa8\x18\x00\x64\x0c\x00\x0a\x06\x00\x0d\x03\x00\x61\x00\x20"
		"\x0c\x00\x8c\x01\x40\xc2\x00\x20\x30\x00\x30\x06\x00\x1d\x03\x00\xe1\x0f\x57\xf1"
		"\x87\xcb\xf8\xc3\x15\xfc\xe1\x1a\xfe\x70\xc2\x1f\x4e\xf8\xc3\x19\x7f\xb8\x84\x3f"
		"\x5c\xe0\x0f\x67\xfc\xe1\x3a\xfe\x70\xc2\x1f\xae\xe2\x0f\x97\xf1\x87\x2b\xf8\xc3"
		"\x35\xfc\xe1\x84\x3f\x9c\x00\x00\x18\x4e\x43\x82\xd3\x10\x70\x1a\x0c\xa7\xa1\xc3"
		"\x63\x11\x3c\x96\x0a\x8f\x25\xc3\x63\x29\x70\x97\x1a\xdc\x25\xc1\x5d\x12\xdc\x25"
		"\xc3\x57\x4b\xf0\xd5\x02\xbe\x9a\xe1\xab\x75\x38\x8a\x82\xa3\x58\xe1\x28\x66\x38"
		"\x8a\x05\x5e\x6a\x83\x97\x2a\x78\xa9\x82\x97\x6a\xb8\xc8\x09\x2e\x72\xc0\x45\x36"
		"\x5c\xe4\x0e\x17\x59\x70\x91\x2b\xfc\xff\x0c\xff\xff\x1f\x0c\xd0\x7c\x6e";

	size_t dst_size = 33485;
	char *dst = malloc(dst_size);
	ptrdiff_t res = ufbxi_inflate(dst, dst_size, src, sizeof(src) - 1);
	ufbxt_hintf("res = %d", (int)res);
	ufbxt_assert(res == dst_size);

	// Double check with FNV-1a hash
	{
		uint32_t h = 0x811c9dc5u;
		for (size_t i = 0; i < dst_size; i++) {
			h = (h ^ dst[i]) * 0x01000193;
		}
		ufbxt_assert(h == 0x3599af58);
	}
}
#endif
