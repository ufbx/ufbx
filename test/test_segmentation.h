
UFBXT_FILE_TEST_ALT(blender_279_ball_segmentation, blender_279_ball)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Icosphere");
	ufbxt_assert(node && node->mesh);
	ufbx_mesh *mesh = node->mesh;

	ufbx_segment_opts opts = { 0 };
	ufbxt_init_allocator(&opts.result_allocator);
	ufbxt_init_allocator(&opts.temp_allocator);
	opts.max_materials = 1;
	opts.max_triangles = 20;

	ufbx_error error;
	ufbx_segmented_mesh *seg_mesh = ufbx_segment_mesh(mesh, &opts, &error);
	if (!seg_mesh) ufbxt_log_error(&error);
	ufbxt_assert(seg_mesh);

	ufbx_free_segmented_mesh(seg_mesh);
}
#endif

