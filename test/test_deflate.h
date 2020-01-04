
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
	int ok = ufbxi_inflate(dst, 1, src, 0, NULL);
	ufbxt_assert(ok == 0);
}
#endif

UFBXT_TEST(deflate_simple)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello!\x07\xa2\x02\x16";
	char dst[6];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "Hello!", 6));
}
#endif

UFBXT_TEST(deflate_simple_chunks)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x00\x06\x00\xf9\xffHello \x01\x06\x00\xf9\xffworld!\x1d\x09\x04\x5e";
	char dst[12];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "Hello world!", 12));
}
#endif

UFBXT_TEST(deflate_bad_chunk_type)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x06";
	char dst[1];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok == 0);
}
#endif

UFBXT_TEST(deflate_static)
#if UFBXT_IMPL
{
	const char src[] = "x\xda\xf3H\xcd\xc9\xc9W(\xcf/\xcaIQ\x04\x00\x1d\t\x04^";
	char dst[12];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "Hello world!", 12));
}
#endif

UFBXT_TEST(deflate_static_match)
#if UFBXT_IMPL
{
	const char src[] = "x\xda\xf3H\xcd\xc9\xc9W\xf0\x00\x91\x8a\x00\x1b\xbb\x04*";
	char dst[12];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "Hello Hello!", 12));
}
#endif

UFBXT_TEST(deflate_static_rle)
#if UFBXT_IMPL
{
	const char src[] = "x\xdastD\x00\x00\x13\xda\x03\r";
	char dst[12];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "AAAAAAAAAAAA", 12));
}
#endif

UFBXT_TEST(deflate_dynamic)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x1d\xc4\x31\x0d\x00\x00\x0c\x02\x41\x2b\xad"
		"\x1b\x8c\xb0\x7d\x82\xff\x8d\x84\xe5\x64\xc8\xcd\x2f\x1b\xbb\x04\x2a";
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
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
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
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
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
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
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, NULL);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "Test Part Data Data Test Data Part New Test Data", 48));
}
#endif

UFBXT_TEST(deflate_fail_cfm)
#if UFBXT_IMPL
{
	const char src[] = "\x79\x9c", *error = NULL;
	char dst[4];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_fdict)
#if UFBXT_IMPL
{
	const char src[] = "\x78\xbc", *error = NULL;
	char dst[4];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_fcheck)
#if UFBXT_IMPL
{
	const char src[] = "\x78\0x9d", *error = NULL;
	char dst[4];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_nlen)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf8\xffHello!\x07\xa2\x02\x16", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_dst_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello!\x07\xa2\x02\x16", *error;
	char dst[5];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_src_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_bad_block)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x07\x08\x00\xf8\xff", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_bad_truncated_checksum)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello!\x07\xa2\x02", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_bad_checksum)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x01\x06\x00\xf9\xffHello!\x07\xa2\x02\xff", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_codelen_16_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\x80\x85\x0c\x00\x00\x00\xc0\xfc\xa1\x5f\xc3\x06\x05\xf5\x02\xfb", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_codelen_17_overflow)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\xc0\x81\x08\x00\x00\x00\x00\x20\x7f\xdf\x09\x4e\x00\xf5\x00\xf5", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_codelen_bad_huffman)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\x80\x31\x11\x01\x00\x00\x01\xc3\xa9\xe2\x37\x47\xff\xcd\x69\x26\xf4\x0a\x7a\x02\xbb", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_litlen_bad_huffman)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x05\x40\x81\x09\x00\x20\x08\x7b\xa5\x0f\x7a\xa4\x27\xa2"
		"\x46\x0a\xa2\xa0\xfb\x1f\x11\x23\xea\xf8\x16\xc4\xa7\xae\x9b\x0f\x3d\x4e\xe4\x07\x8d", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif

UFBXT_TEST(deflate_fail_distance_bad_huffman)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9c\x1d\xc5\x31\x0d\x00\x00\x0c\x02\x41\x2b\x55\x80\x8a\x9a"
		"\x61\x06\xff\x21\xf9\xe5\xfe\x9d\x1e\x48\x3c\x31\xba\x05\x79", *error;
	char dst[64];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1, &error);
	ufbxt_assert(ok == 0);
	ufbxt_logf("Error: %s", error);
}
#endif
