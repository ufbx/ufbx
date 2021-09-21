
UFBXT_FILE_TEST(maya_cache_jiggle)
#if UFBXT_IMPL
{
	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s", data_root, "maya_cache_jiggle.mcx");

	ufbx_geometry_cache *cache = ufbx_load_geometry_cache(bbuf, NULL, NULL);
	ufbxt_assert(cache);
}
#endif
