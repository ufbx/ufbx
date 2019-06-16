
UFBXT_TEST(parse_ascii_empty)
#if UFBXT_IMPL
{
	const char *header = "Kaydara FBX Binary  \x00\x1a\x00\xe8\x1c\x00\x00";
	const char *null_node = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

	ufbxi_ascii *ua = ufbxt_ascii_context("");
	ufbxt_assert(ufbxi_ascii_parse(ua));
	ufbxt_assert(ua->dst_pos == 40);
	ufbxt_assert_eq(ua->dst, header, 27);
	ufbxt_assert_eq(ua->dst + 27, null_node, 13);
}
#endif

#if UFBXT_IMPL
static void test_ascii_to_binary(const char *src, const char *dst, uint32_t dst_len)
{
	ufbxi_ascii *ua = ufbxt_ascii_context(src);
	ufbxt_assert(ufbxi_ascii_parse(ua));
	ufbxt_assert(ua->dst_pos == dst_len);
	ufbxt_assert_eq(ua->dst, dst, dst_len);
}
#endif

UFBXT_TEST(parse_ascii_simple)
#if UFBXT_IMPL
{
	const char *src = "Node: 1 {Sub:2,3} ; Comment";
	const char dst[] =
		/* 27 */ "Kaydara FBX Binary  \x00\x1a\x00\xe8\x1c\x00\x00"
		/* 39 */ "\x52\x00\x00\x00" "\x01\x00\x00\x00" "\x03\x00\x00\x00"
		/* 47 */ "\x04" "Node" "Y\x01\x00"
		/* 59 */ "\x45\x00\x00\x00" "\x02\x00\x00\x00" "\x06\x00\x00\x00"
		/* 69 */ "\x03" "Sub" "Y\x02\x00" "Y\x03\x00"
		/* 82 */  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		/* 95 */  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
	const uint32_t dst_len = 95;
	ufbxt_assert(sizeof(dst) == dst_len + 1);
	test_ascii_to_binary(src, dst, dst_len);
}
#endif

UFBXT_TEST(parse_ascii_version_from_header)
#if UFBXT_IMPL
{
	const char *src =
		"FBXHeaderExtension: {\r\n"
		"\tFBXVersion: 6100, 255, ; Note: Dangling comma is OK \r\n"
		"}";
	const char dst[] =
		/*  27 */ "Kaydara FBX Binary  \x00\x1a\x00\xd4\x17\x00\x00"
		/*  39 */ "\x64\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		/*  58 */ "\x12" "FBXHeaderExtension"
		/*  70 */ "\x57\x00\x00\x00" "\x02\x00\x00\x00" "\x06\x00\x00\x00"
		/*  87 */ "\x0a" "FBXVersion" "Y\xd4\x17" "Y\xff\x00"
		/* 100 */  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		/* 113 */  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
	const uint32_t dst_len = 113;
	ufbxt_assert(sizeof(dst) == dst_len + 1);
	test_ascii_to_binary(src, dst, dst_len);
}
#endif

#if UFBXT_IMPL
static void test_ascii_to_binary_value(const char *src, const char *dst, uint32_t dst_arr_size, uint32_t num)
{
	uint32_t dst_len = dst_arr_size - 1;
	const char *header = "Kaydara FBX Binary  \x00\x1a\x00\xe8\x1c\x00\x00";
	const char *null_node = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

	ufbxi_ascii *ua = ufbxt_ascii_context(src);
	ufbxt_assert(ufbxi_ascii_parse(ua));
	ufbxt_assert(ua->dst_pos - 13 - 41 == dst_len);

	ufbxt_assert_eq(ua->dst, header, 27);
	ufbxt_assert_eq(ua->dst + ua->dst_pos - 13, null_node, 13);

	uint32_t node_end = ufbxi_read_u32(ua->dst + 27 + 0);
	uint32_t num_values = ufbxi_read_u32(ua->dst + 27 + 4);
	uint32_t value_size = ufbxi_read_u32(ua->dst + 27 + 8);
	ufbxt_assert(node_end == 41 + dst_len);
	ufbxt_assert(value_size == dst_len);
	ufbxt_assert(num_values == num);
	ufbxt_assert_eq(ua->dst + 41, dst, dst_len);
}
#endif

UFBXT_TEST(parse_ascii_string)
#if UFBXT_IMPL
{
	const char *src = "V: \"Hello world!\"";
	const char dst[] = "S\x0c\x00\x00\x00" "Hello world!";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 1);
}
#endif

UFBXT_TEST(parse_ascii_string_escape)
#if UFBXT_IMPL
{
	const char *src = "V: \"&quot;&quot;\"";
	const char dst[] = "S\x02\x00\x00\x00" "\"\"";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 1);
}
#endif

UFBXT_TEST(parse_ascii_string_almost_escape)
#if UFBXT_IMPL
{
	const char *src = "V: \"&&q&qu&quo&quot&quot;&\"";
	const char dst[] = "S\x11\x00\x00\x00" "&&q&qu&quo&quot\"&";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 1);
}
#endif

UFBXT_TEST(parse_ascii_32bit_int)
#if UFBXT_IMPL
{
	const char *src = "V: 100000";
	const char dst[] = "I\xa0\x86\x01\x00";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 1);
}
#endif

UFBXT_TEST(parse_ascii_64bit_int)
#if UFBXT_IMPL
{
	const char *src = "V: 100000000000";
	const char dst[] = "L\x00\xe8\x76\x48\x17\x00\x00\x00";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 1);
}
#endif

UFBXT_TEST(parse_ascii_bool)
#if UFBXT_IMPL
{
	const char *src = "V: T, F, Y, N";
	const char dst[] = "C\1C\0C\1C\0";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 4);
}
#endif

UFBXT_TEST(parse_ascii_float)
#if UFBXT_IMPL
{
	const char *src = "V: 2.5";
	const char dst[] = "F\x00\x00\x20\x40";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 1);
}
#endif

UFBXT_TEST(parse_ascii_double)
#if UFBXT_IMPL
{
	const char *src = "V: 1.0000000000000002";
	const char dst[] = "D\x01\x00\x00\x00\x00\x00\xf0\x3f";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 1);
}
#endif

UFBXT_TEST(parse_ascii_invalid_character_name)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context("@");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_invalid_character_value)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context("Node: @");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_unclosed_node)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context("Node: {");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_long_number)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context(
		"Val: 1000000000000000000000000000000000000000000000000"
		"0000000000000000000000000000000000000000000000000"
		"0000000000000000000000000000000000000000000000000"
	);
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_bad_int)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context("Val: 1+1");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_bad_float)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context("Val: 1..0");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_bad_string)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context("Val: \"Asd");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_bad_node_name)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context(
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA:");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_log_ascii_error(ua);
}
#endif
