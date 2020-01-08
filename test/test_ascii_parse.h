
UFBXT_TEST(parse_ascii_empty)
#if UFBXT_IMPL
{
	const char *header = "Kaydara FBX Binary  \x00\x1a\x00\x4c\x1d\x00\x00";
	const char *null_node = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

	ufbxi_ascii *ua = ufbxt_ascii_context("");
	ufbxt_assert(ufbxi_ascii_parse(ua));
	ufbxt_assert(ua->dst_pos == 52);
	ufbxt_assert_eq(ua->dst, header, 27);
	ufbxt_assert_eq(ua->dst + 27, null_node, 25);
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
		/*  27 */ "Kaydara FBX Binary  \x00\x1a\x00\x4c\x1d\x00\x00"
		/*  51 */ "\x76\0\0\0\0\0\0\0" "\x01\0\0\0\0\0\0\0" "\x03\0\0\0\0\0\0\0"
		/*  59 */ "\x04" "Node" "Y\x01\x00"
		/*  83 */ "\x5d\0\0\0\0\0\0\0" "\x02\0\0\0\0\0\0\0" "\x06\0\0\0\0\0\0\0"
		/*  93 */ "\x03" "Sub" "Y\x02\x00" "Y\x03\x00"
		/* 118 */  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
		/* 143 */  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
	const uint32_t dst_len = 143;
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
		/*  51 */ "\x88\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0"
		/*  79 */ "\x12" "FBXHeaderExtension"
		/*  94 */ "\x6f\0\0\0\0\0\0\0" "\x02\0\0\0\0\0\0\0" "\x06\0\0\0\0\0\0\0"
		/* 111 */ "\x0a" "FBXVersion" "Y\xd4\x17" "Y\xff\x00"
		/* 136 */  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
		/* 161 */  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
	const uint32_t dst_len = 161;
	ufbxt_assert(sizeof(dst) == dst_len + 1);
	test_ascii_to_binary(src, dst, dst_len);
}
#endif

#if UFBXT_IMPL
static void test_ascii_to_binary_value(const char *src, const char *dst, uint32_t dst_arr_size, uint32_t num)
{
	uint32_t dst_len = dst_arr_size - 1;
	const char *header = "Kaydara FBX Binary  \x00\x1a\x00\x4c\x1d\x00\x00";
	const char *null_node = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

	ufbxi_ascii *ua = ufbxt_ascii_context(src);
	ufbxt_assert(ufbxi_ascii_parse(ua));

	uint64_t node_pos = 27;
	uint64_t value_pos = node_pos + 25 + 1;
	uint64_t null_pos = ua->dst_pos - 25;
	uint64_t end_pos = null_pos + 25;
	ufbxt_assert(null_pos - value_pos == dst_len);

	ufbxt_assert_eq(ua->dst, header, 27);
	ufbxt_assert_eq(ua->dst + null_pos, null_node, 25);

	size_t node_end = (size_t)ufbxi_read_u64(ua->dst + node_pos + 0);
	size_t num_values = (size_t)ufbxi_read_u64(ua->dst + node_pos + 8);
	size_t value_size = (size_t)ufbxi_read_u64(ua->dst + node_pos + 16);
	ufbxt_assert(node_end == null_pos);
	ufbxt_assert(value_size == dst_len);
	ufbxt_assert(num_values == num);
	ufbxt_assert_eq(ua->dst + value_pos, dst, dst_len);
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

UFBXT_TEST(parse_ascii_array_syntax)
#if UFBXT_IMPL
{
	const char *src = "V: *4 { a: 1,2,3,-5 }";
	const char dst[] = "Y\x01\x00Y\x02\x00Y\x03\x00Y\xfb\xff";
	test_ascii_to_binary_value(src, dst, sizeof(dst), 4);
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

UFBXT_TEST(parse_ascii_nested_error)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context("First: { Second: { Third: { ? } } }");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_assert(ua->error->stack_size == 3);
	ufbxt_assert(!strcmp(ua->error->stack[0], "First"));
	ufbxt_assert(!strcmp(ua->error->stack[1], "Second"));
	ufbxt_assert(!strcmp(ua->error->stack[2], "Third"));
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_stack_truncation)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context(
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA: { ? }");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_assert(ua->error->stack_size == 1);
	ufbxt_assert(strlen(ua->error->stack[0]) == UFBX_ERROR_STACK_NAME_MAX_LENGTH);
	ufbxt_log_ascii_error(ua);
}
#endif

UFBXT_TEST(parse_ascii_stack_max_depth)
#if UFBXT_IMPL
{
	ufbxi_ascii *ua = ufbxt_ascii_context(
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: {"
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: {"
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: {"
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: {"
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: {"
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: {"
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: {"
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: {"
		"A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { A: { ? ");
	ufbxt_assert(!ufbxi_ascii_parse(ua));
	ufbxt_assert(ua->error->stack_size == UFBX_ERROR_STACK_MAX_DEPTH);
	ufbxt_log_ascii_error(ua);
}
#endif
