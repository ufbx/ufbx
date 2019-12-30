
UFBXT_TEST(bits_simple)
#if UFBXT_IMPL
{
	char data[3] = "\xAB\xCD\xEF";
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
	ufbxi_init_static(&dc);

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
	ufbxi_init_static(&dc);

	test_huff_range(&dc.huff_dist, 0, 31, 5, 0x0);
}
#endif


UFBXT_TEST(deflate_empty)
#if UFBXT_IMPL
{
	char src[1], dst[1];
	int ok = ufbxi_inflate(dst, 1, src, 0);
	ufbxt_assert(ok == 0);
}
#endif

UFBXT_TEST(deflate_simple)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9C\x01\x06\x00\xf9\xffHello!";
	char dst[6];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "Hello!", 6));
}
#endif

UFBXT_TEST(deflate_simple_chunks)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9C\x00\x06\x00\xf9\xffHello \x01\x06\x00\xf9\xffworld!";
	char dst[12];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "Hello world!", 12));
}
#endif

UFBXT_TEST(deflate_bad_chunk_type)
#if UFBXT_IMPL
{
	const char src[] = "\x78\x9C\x06";
	char dst[1];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_assert(ok == 0);
}
#endif

UFBXT_TEST(deflate_static)
#if UFBXT_IMPL
{
	const char src[] = "x\xda\xf3H\xcd\xc9\xc9W(\xcf/\xcaIQ\x04\x00\x1d\t\x04^";
	char dst[12];
	int ok = ufbxi_inflate(dst, sizeof(dst), src, sizeof(src) - 1);
	ufbxt_assert(ok != 0);
	ufbxt_assert(!memcmp(dst, "Hello world!", 12));
}
#endif
