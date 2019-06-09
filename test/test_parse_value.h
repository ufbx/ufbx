
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

UFBXT_TEST(parse_value_eof)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context("");

	uint32_t value;
	ufbxt_assert(!ufbxi_parse_value(uc, 'I', &value));
	ufbxt_log_error(uc);
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

UFBXT_TEST(parse_value_bad_src_type)
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

UFBXT_TEST(parse_value_bad_dst_type)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03\x04"
	);

	uint32_t value;
	ufbxt_assert(!ufbxi_parse_value(uc, 'X', &value));
	ufbxt_log_error(uc);
}
#endif

UFBXT_TEST(parse_value_bad_conversion)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"F\x01\x02\x03\x04"
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

UFBXT_TEST(parse_int_to_float)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x08\x00\x00\x00"
		"Y\x04\x00"
	);

	float f;
	double d;
	ufbxt_assert(ufbxi_parse_values(uc, "FD", &f, &d));
	ufbxt_assert(f == 8.0f);
	ufbxt_assert(d == 4.0);
}
#endif

UFBXT_TEST(parse_string_value)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"S\x05\x00\x00\x00Hello"
		"R\x05\x00\x00\x00world"
	);

	ufbxi_string a, b;
	ufbxt_assert(ufbxi_parse_values(uc, "SS", &a, &b));
	ufbxt_assert(ufbxi_streq(a, "Hello"));
	ufbxt_assert(ufbxi_streq(b, "world"));
}
#endif

UFBXT_TEST(parse_string_long)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"S\x40\x00\x00\x00Hello"
	);

	ufbxi_string str;
	ufbxt_assert(!ufbxi_parse_value(uc, 'S', &str));
	ufbxt_log_error(uc);
}
#endif

UFBXT_TEST(parse_string_huge)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"S\xff\xff\xff\xffHello"
	);

	ufbxi_string str;
	ufbxt_assert(!ufbxi_parse_value(uc, 'S', &str));
	ufbxt_log_error(uc);
}
#endif

UFBXT_TEST(parse_string_bad_dst)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"S\x05\x00\x00\x00Hello"
	);

	uint32_t value;
	ufbxt_assert(!ufbxi_parse_values(uc, "I", &value));
	ufbxt_log_error(uc);
}
#endif

UFBXT_TEST(parse_boolean_binary)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"C\x10"
		"C\x20"
		"I\x20\x20\x20\x20"
	);

	uint32_t a;
	char b, c;
	ufbxt_assert(ufbxi_parse_values(uc, "IBB", &a, &b, &c));
	ufbxt_assert(a == 1);
	ufbxt_assert(b == 1);
	ufbxt_assert(c == 1);
}
#endif

UFBXT_TEST(parse_ignore_value)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03\x04"
	);

	uint32_t value;
	ufbxt_assert(ufbxi_parse_value(uc, '.', &value));
	ufbxt_assert(uc->pos == 5);
}
#endif

UFBXT_TEST(parse_skip_value)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03\x04"
		"I\x11\x12\x13\x14"
	);

	uint32_t value;
	ufbxt_assert(ufbxi_parse_values(uc, ".I", &value));
	ufbxt_assert(value == 0x14131211);
}
#endif

UFBXT_TEST(parse_any_values)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context_values(
		"I\x01\x02\x03\x04"
		"L\x11\x12\x13\x14\x15\x16\x17\x18"
		"Y\x21\x22"
		"C\x01"
		"F\x00\x00\x80\x3f"
		"D\x00\x00\x00\x00\x00\x00\xf0\x3f"
		"S\x05\x00\x00\x00Hello"
	);

	ufbxi_any_value i, l, y, c, f, d, s;
	ufbxt_assert(ufbxi_parse_values(uc, "???????", &i, &l, &y, &c, &f, &d, &s));
	ufbxt_assert(i.type_code == 'I');
	ufbxt_assert(i.value_class == ufbxi_val_int);
	ufbxt_assert(i.value.i == 0x04030201);
	ufbxt_assert(l.type_code == 'L');
	ufbxt_assert(l.value_class == ufbxi_val_int);
	ufbxt_assert(l.value.i == UINT64_C(0x1817161514131211));
	ufbxt_assert(y.type_code == 'Y');
	ufbxt_assert(y.value_class == ufbxi_val_int);
	ufbxt_assert(y.value.i == 0x2221);
	ufbxt_assert(c.type_code == 'C');
	ufbxt_assert(c.value_class == ufbxi_val_int);
	ufbxt_assert(c.value.i == 1);
	ufbxt_assert(f.type_code == 'F');
	ufbxt_assert(f.value_class == ufbxi_val_float);
	ufbxt_assert(f.value.f == 1.0f);
	ufbxt_assert(d.type_code == 'D');
	ufbxt_assert(d.value_class == ufbxi_val_float);
	ufbxt_assert(d.value.f == 1.0);
	ufbxt_assert(s.type_code == 'S');
	ufbxt_assert(s.value_class == ufbxi_val_string);
	ufbxt_assert(ufbxi_streq(s.value.str, "Hello"));
}
#endif
