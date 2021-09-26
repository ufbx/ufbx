
UFBXT_FILE_TEST(maya_cache_sine)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, false, "maya_cache_sine_12", NULL, 12.0/24.0);
	ufbxt_check_frame(scene, err, false, "maya_cache_sine_18", NULL, 18.0/24.0);
}
#endif
