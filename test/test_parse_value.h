
UFBXT_TEST(parse_single_value)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03\x04"
	);

	uint32_t value;
	ufbxt_assert(ufbxi_parse_value(uc, 'I', &value));
	ufbxt_assert(value == 0x04030201);
}
#endif

UFBXT_TEST(parse_multiple_values)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03\x04"
		"Y\xAA\xBB"
	);

	uint32_t a, b;
	ufbxt_assert(ufbxi_parse_values(uc, "II", &a, &b));
	ufbxt_assert(a == 0x04030201);
	ufbxt_assert(b == 0xBBAA);
}
#endif

UFBXT_TEST(parse_value_truncated)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03"
	);

	uint32_t value;
	ufbxt_assert(!ufbxi_parse_value(uc, 'I', &value));
	ufbxt_log_error(uc);
}
#endif

UFBXT_TEST(parse_value_bad_type)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"X\x01\x02\x03\x04"
	);

	uint32_t value;
	ufbxt_assert(!ufbxi_parse_value(uc, 'I', &value));
	ufbxt_log_error(uc);
}
#endif

UFBXT_TEST(parse_value_fail_position)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03\x04"
	);

	uint32_t a, b;
	ufbxt_assert(!ufbxi_parse_values(uc, "II", &a, &b));
	ufbxt_assert(uc->error->byte_offset == 5);
	ufbxt_log_error(uc);
}
#endif

UFBXT_TEST(parse_value_basic_types)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03\x04"
		"L\x11\x12\x13\x14\x15\x16\x17\x18"
		"Y\x21\x22"
		"C\x01"
		"F\x00\x00\x80\x3f"
		"D\x00\x00\x00\x00\x00\x00\xf0\x3f"
	);

	uint64_t i, l, y, c;
	float f;
	double d;
	ufbxt_assert(ufbxi_parse_values(uc, "LLLLFD", &i, &l, &y, &c, &f, &d));
	ufbxt_assert(i == 0x04030201);
	ufbxt_assert(l == UINT64_C(0x1817161514131211));
	ufbxt_assert(y == 0x2221);
	ufbxt_assert(c == 1);
	ufbxt_assert(f == 1.0f);
	ufbxt_assert(d == 1.0);
}
#endif
