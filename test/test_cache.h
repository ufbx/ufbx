
#if UFBXT_IMPL
static void ufbxt_test_sine_cache(ufbxt_diff_error *err, const char *path, double begin, double end, double err_threshold)
{
	char buf[512];
	snprintf(buf, sizeof(buf), "%s%s", data_root, path);

	ufbx_geometry_cache *cache = ufbx_load_geometry_cache(buf, NULL, NULL);
	ufbxt_assert(cache);
	ufbxt_assert(cache->channels.count == 2);

	bool found_cube1 = false;
	for (size_t i = 0; i < cache->channels.count; i++) {
		ufbx_cache_channel *channel = &cache->channels.data[i];
		ufbxt_assert(channel->interpretation == UFBX_CACHE_INTERPRETATION_VERTEX_POSITION);

		if (!strcmp(channel->name.data, "pCubeShape1")) {
			found_cube1 = true;

			ufbx_vec3 pos[64];
			for (double time = begin; time <= end + 0.0001; time += 0.1/24.0) {
				size_t num_verts = ufbx_sample_geometry_cache_vec3(channel, time, pos, ufbxt_arraycount(pos), NULL);
				ufbxt_assert(num_verts == 36);

				double t = (time - 1.0/24.0) / (29.0/24.0) * 4.0;
				double pi2 = 3.141592653589793*2.0;
				double err_scale = 0.001 / err_threshold;

				for (size_t i = 0; i < num_verts; i++) {
					ufbx_vec3 v = pos[i];
					double sx = sin((v.y + t * 0.5f)*pi2) * 0.25;
					double vx = v.x;
					vx += vx > 0.0 ? -0.5 : 0.5;
					ufbxt_assert_close_real(err, vx*err_scale, sx*err_scale);
				}
			}
		}
	}

	ufbxt_assert(found_cube1);

	ufbx_free_geometry_cache(cache);
}
#endif

UFBXT_FILE_TEST(maya_cache_sine)
#if UFBXT_IMPL
{
	ufbxt_check_frame(scene, err, false, "maya_cache_sine_12", NULL, 12.0/24.0);
	ufbxt_check_frame(scene, err, false, "maya_cache_sine_18", NULL, 18.0/24.0);
}
#endif

UFBXT_TEST(maya_cache_sine_caches)
#if UFBXT_IMPL
{
	ufbxt_diff_error err = { 0 };

	ufbxt_test_sine_cache(&err, "caches/sine_mcmf_undersample/cache.xml", 1.0/24.0, 29.0/24.0, 0.04);
	ufbxt_test_sine_cache(&err, "caches/sine_mcsd_oversample/cache.xml", 1.0/24.0, 29.0/24.0, 0.003);
	ufbxt_test_sine_cache(&err, "caches/sine_mxmd_oversample/cache.xml", 11.0/24.0, 19.0/24.0, 0.003);
	ufbxt_test_sine_cache(&err, "caches/sine_mxsf_regular/cache.xml", 1.0/24.0, 29.0/24.0, 0.008);

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
}
#endif
