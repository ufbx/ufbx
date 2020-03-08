
UFBXT_TEST(prop_names)
#if UFBXT_IMPL
{
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_UNKNOWN), "unknown"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_BOOLEAN), "boolean"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_INTEGER), "integer"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_NUMBER), "number"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_VECTOR), "vector"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_COLOR), "color"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_STRING), "string"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_DATE_TIME), "date_time"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_TRANSLATION), "translation"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_ROTATION), "rotation"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_SCALING), "scaling"));
}
#endif

UFBXT_TEST(node_names)
#if UFBXT_IMPL
{
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_UNKNOWN), "unknown"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_MODEL), "model"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_MESH), "mesh"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_MATERIAL), "material"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_TEXTURE), "texture"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_BONE), "bone"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_SKIN), "skin"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_ANIMATION), "animation"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_ANIMATION_CURVE), "animation_curve"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_ANIMATION_LAYER), "animation_layer"));
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_ATTRIBUTE), "attribute"));
}
#endif

UFBXT_TEST(prop_names_not_found)
#if UFBXT_IMPL
{
	ufbxt_assert(!strcmp(ufbx_prop_type_name(UFBX_PROP_SCALING + 1), "(bad prop type)"));
	ufbxt_assert(!strcmp(ufbx_prop_type_name((ufbx_prop_type)-1), "(bad prop type)"));
}
#endif

UFBXT_TEST(node_names_not_found)
#if UFBXT_IMPL
{
	ufbxt_assert(!strcmp(ufbx_node_type_name(UFBX_NODE_ATTRIBUTE + 1), "(bad node type)"));
	ufbxt_assert(!strcmp(ufbx_node_type_name((ufbx_node_type)-1), "(bad node type)"));
}
#endif
