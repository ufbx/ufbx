
UFBXT_FILE_TEST(maya_slime)
#if UFBXT_IMPL
{
	ufbx_node *node_high = ufbx_find_node(scene, "Slime_002:Slime_Body_high");
	ufbxt_assert(node_high);
	ufbxt_assert(!node_high->visible);
}
#endif

UFBXT_FILE_TEST(blender_293_barbarian)
#if UFBXT_IMPL
{
}
#endif

UFBXT_FILE_TEST_ALT(evaluate_alloc_fail, blender_293_barbarian)
#if UFBXT_IMPL
{
	for (size_t max_temp = 1; max_temp < 10000; max_temp++) {
		ufbx_evaluate_opts opts = { 0 };
		opts.temp_allocator.huge_threshold = 1;
		opts.temp_allocator.allocation_limit = max_temp;
		opts.evaluate_skinning = true;

		ufbxt_hintf("Temp limit: %zu", max_temp);

		ufbx_error error;
		ufbx_scene *eval_scene = ufbx_evaluate_scene(scene, NULL, 0.2, &opts, &error);
		if (eval_scene) {
			ufbxt_logf(".. Tested up to %zu temporary allocations", max_temp);
			ufbx_free_scene(eval_scene);
			break;
		}
		ufbxt_assert(error.type == UFBX_ERROR_ALLOCATION_LIMIT);
	}

	for (size_t max_result = 1; max_result < 10000; max_result++) {
		ufbx_evaluate_opts opts = { 0 };
		opts.result_allocator.huge_threshold = 1;
		opts.result_allocator.allocation_limit = max_result;
		opts.evaluate_skinning = true;

		ufbxt_hintf("Result limit: %zu", max_result);

		ufbx_error error;
		ufbx_scene *eval_scene = ufbx_evaluate_scene(scene, NULL, 0.2, &opts, &error);
		if (eval_scene) {
			ufbxt_logf(".. Tested up to %zu result allocations", max_result);
			ufbx_free_scene(eval_scene);
			break;
		}
		ufbxt_assert(error.type == UFBX_ERROR_ALLOCATION_LIMIT);
	}
}
#endif
