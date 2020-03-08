
UFBXT_TEST(parse_single_node)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x12\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "Hello"
	);

	ufbxi_node node;
	memset(&node, 0, sizeof(ufbxi_node));
	ufbxt_assert(ufbxi_parse_node(uc, 0, &node));

	ufbxt_assert(ufbxi_streq(node.name, "Hello"));
	ufbxt_assert(node.value_begin_pos == 18);
	ufbxt_assert(node.child_begin_pos == 18);
	ufbxt_assert(node.end_pos == 18);
}
#endif

UFBXT_TEST(parse_single_node_7500)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x1e\x00\x00\x00\x00\x00\x00\x00" "\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x05" "Hello"
	);
	uc->version = 7500;

	ufbxi_node node;
	memset(&node, 0, sizeof(ufbxi_node));
	ufbxt_assert(ufbxi_parse_node(uc, 0, &node));

	ufbxt_assert(ufbxi_streq(node.name, "Hello"));
	ufbxt_assert(node.value_begin_pos == 30);
	ufbxt_assert(node.child_begin_pos == 30);
	ufbxt_assert(node.end_pos == 30);
}
#endif

UFBXT_TEST(iter_two_nodes)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x12\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "Hello"
		"\x24\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "World"
	);

	ufbx_string name;
	ufbxt_assert(ufbxi_next_child(uc, &name));
	ufbxt_assert(ufbxi_streq(name, "Hello"));
	ufbxt_assert(uc->focused_node.value_begin_pos == 18);
	ufbxt_assert(uc->focused_node.child_begin_pos == 18);
	ufbxt_assert(uc->focused_node.end_pos == 18);

	ufbxt_assert(ufbxi_next_child(uc, &name));
	ufbxt_assert(ufbxi_streq(name, "World"));
	ufbxt_assert(uc->focused_node.value_begin_pos == 36);
	ufbxt_assert(uc->focused_node.child_begin_pos == 36);
	ufbxt_assert(uc->focused_node.end_pos == 36);

	ufbxt_assert(!ufbxi_next_child(uc, &name));
	ufbxt_assert(!uc->base.failed);
}
#endif

UFBXT_TEST(parse_single_node_value)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x1c\x00\x00\x00" "\x02\x00\x00\x00" "\x0a\x00\x00\x00"
		"\x05" "Hello"
		"I\x03\x00\x00\x00" "F\x00\x00\x00\x40"
	);

	ufbxi_node node;
	memset(&node, 0, sizeof(ufbxi_node));
	ufbxt_assert(ufbxi_parse_node(uc, 0, &node));

	ufbxt_assert(ufbxi_streq(node.name, "Hello"));
	ufbxt_assert(node.value_begin_pos == 18);
	ufbxt_assert(node.child_begin_pos == 28);
	ufbxt_assert(node.end_pos == 28);
	uc->focused_node = node;

	int32_t i;
	float f;
	ufbxt_assert(ufbxi_parse_values(uc, "IF", &i, &f));
	ufbxt_assert(i == 3);
	ufbxt_assert(f == 2.0f);
}
#endif

UFBXT_TEST(parse_single_node_child)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x31\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "Hello"
		"\x24\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "World"
		"\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00"
	);

	ufbxi_node node;
	memset(&node, 0, sizeof(ufbxi_node));
	ufbxt_assert(ufbxi_parse_node(uc, 0, &node));

	ufbxt_assert(ufbxi_streq(node.name, "Hello"));
	ufbxt_assert(node.value_begin_pos == 18);
	ufbxt_assert(node.child_begin_pos == 18);
	ufbxt_assert(node.end_pos == 49);
	uc->focused_node = node;

	ufbxi_node child;
	memset(&child, 0, sizeof(ufbxi_node));
	ufbxt_assert(ufbxi_parse_node(uc, node.child_begin_pos, &child));

	ufbxt_assert(ufbxi_streq(child.name, "World"));
	ufbxt_assert(child.value_begin_pos == 36);
	ufbxt_assert(child.child_begin_pos == 36);
	ufbxt_assert(child.end_pos == 36);
}
#endif

UFBXT_TEST(enter_single_node)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x31\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "Hello"
		"\x24\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "World"
		"\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00"
	);

	ufbx_string name;
	ufbxt_assert(ufbxi_next_child(uc, &name));
	ufbxt_assert(ufbxi_streq(name, "Hello"));
	ufbxt_assert(uc->focused_node.value_begin_pos == 18);
	ufbxt_assert(uc->focused_node.child_begin_pos == 18);
	ufbxt_assert(uc->focused_node.end_pos == 49);

	ufbxt_assert(ufbxi_enter_node(uc));
	ufbxt_assert(ufbxi_next_child(uc, &name));
	ufbxt_assert(ufbxi_streq(name, "World"));
	ufbxt_assert(uc->focused_node.value_begin_pos == 36);
	ufbxt_assert(uc->focused_node.child_begin_pos == 36);
	
	ufbxt_assert(!ufbxi_next_child(uc, &name));
	ufbxi_exit_node(uc);
	ufbxt_assert(!ufbxi_next_child(uc, &name));
	ufbxt_assert(!uc->base.failed);
}
#endif

UFBXT_TEST(node_stack_error)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x31\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "Hello"
		"\x24\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "World"
		"\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00"
	);

	int32_t dummy;
	ufbx_string name;
	ufbxt_assert(ufbxi_next_child(uc, &name));
	ufbxt_assert(ufbxi_enter_node(uc));
	ufbxt_assert(ufbxi_next_child(uc, &name));
	ufbxt_assert(ufbxi_enter_node(uc));
	ufbxt_assert(!ufbxi_parse_value(uc, 'I', &dummy));
	ufbxt_log_error(uc);
	ufbxt_assert(uc->base.error->stack_size == 2);
	ufbxt_assert(!strcmp(uc->base.error->stack[0], "Hello"));
	ufbxt_assert(!strcmp(uc->base.error->stack[1], "World"));
}
#endif

UFBXT_TEST(find_two_nodes)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x12\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "Hello"
		"\x24\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "World"
	);

	ufbxt_assert(ufbxi_find_node(uc, "Hello"));
	ufbxt_assert(ufbxi_streq(uc->focused_node.name, "Hello"));
	ufbxt_assert(uc->focused_node.value_begin_pos == 18);
	ufbxt_assert(uc->focused_node.child_begin_pos == 18);
	ufbxt_assert(uc->focused_node.end_pos == 18);

	ufbxt_assert(ufbxi_find_node(uc, "World"));
	ufbxt_assert(ufbxi_streq(uc->focused_node.name, "World"));
	ufbxt_assert(uc->focused_node.value_begin_pos == 36);
	ufbxt_assert(uc->focused_node.child_begin_pos == 36);
	ufbxt_assert(uc->focused_node.end_pos == 36);

	ufbxt_assert(!ufbxi_find_node(uc, "Other"));
	ufbxt_assert(!uc->base.failed);
}
#endif

UFBXT_TEST(fail_parse_node_backwards)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x01\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "Hello"
	);

	ufbxi_node node;
	ufbxt_assert(!ufbxi_parse_node(uc, 0, &node));
	ufbxt_log_error(uc);
}
#endif

UFBXT_TEST(fail_next_node_backwards)
#if UFBXT_IMPL
{
	ufbxi_context *uc = ufbxt_memory_context(
		"\x01\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\x00\x00"
		"\x05" "Hello"
	);

	uint64_t next_pos;
	ufbxt_assert(!ufbxi_next_node_pos(uc, 0, &next_pos));
	ufbxt_log_error(uc);
}
#endif
