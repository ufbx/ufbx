
#if UFBXT_IMPL
typedef struct {
	ufbx_real u;
	ufbx_vec3 position;
	ufbx_vec3 derivative;
} ufbxt_curve_sample;
#endif

UFBXT_FILE_TEST(maya_nurbs_curve_form)
#if UFBXT_IMPL
{
	ufbx_node *node_open = ufbx_find_node(scene, "circleOpen");
	ufbx_node *node_closed = ufbx_find_node(scene, "circleClosed");
	ufbx_node *node_periodic = ufbx_find_node(scene, "circlePeriodic");

	ufbxt_assert(node_open && node_open->attrib_type == UFBX_ELEMENT_NURBS_CURVE);
	ufbxt_assert(node_closed && node_closed->attrib_type == UFBX_ELEMENT_NURBS_CURVE);
	ufbxt_assert(node_periodic && node_periodic->attrib_type == UFBX_ELEMENT_NURBS_CURVE);

	ufbx_nurbs_curve *open = (ufbx_nurbs_curve*)node_open->attrib;
	ufbx_nurbs_curve *closed = (ufbx_nurbs_curve*)node_closed->attrib;
	ufbx_nurbs_curve *periodic = (ufbx_nurbs_curve*)node_periodic->attrib;

	ufbxt_assert(open->basis.valid);
	ufbxt_assert(closed->basis.valid);
	ufbxt_assert(periodic->basis.valid);

	ufbxt_assert(open->basis.topology == UFBX_NURBS_TOPOLOGY_OPEN);
	ufbxt_assert(closed->basis.topology == UFBX_NURBS_TOPOLOGY_CLOSED);
	ufbxt_assert(periodic->basis.topology == UFBX_NURBS_TOPOLOGY_PERIODIC);

	ufbxt_assert(open->basis.order == 4);
	ufbxt_assert(closed->basis.order == 4);
	ufbxt_assert(periodic->basis.order == 4);

	ufbxt_assert(!open->basis.is_2d);
	ufbxt_assert(!closed->basis.is_2d);
	ufbxt_assert(!periodic->basis.is_2d);

	{
		ufbxt_assert_close_real(err, open->basis.t_min, 0.0f);
		ufbxt_assert_close_real(err, open->basis.t_max, 1.0f);
		ufbxt_assert(open->basis.knot_vector.count == 8);
		ufbxt_assert(open->basis.spans.count == 2);
		ufbxt_assert(open->control_points.count == 4);

		{
			ufbx_real weights[16];
			size_t knot = ufbx_evaluate_nurbs_basis(&open->basis, 0.0f, 16, weights, NULL);
			ufbxt_assert(knot == 0);
			ufbxt_assert_close_real(err, weights[0], 1.0f);
			ufbxt_assert_close_real(err, weights[1], 0.0f);
			ufbxt_assert_close_real(err, weights[2], 0.0f);
			ufbxt_assert_close_real(err, weights[3], 0.0f);
		}

		{
			ufbx_real weights[16];
			size_t knot = ufbx_evaluate_nurbs_basis(&open->basis, 0.5f, 16, weights, NULL);
			ufbxt_assert(knot == 0);
			ufbxt_assert_close_real(err, weights[0], 0.125f);
			ufbxt_assert_close_real(err, weights[1], 0.375f);
			ufbxt_assert_close_real(err, weights[2], 0.375f);
			ufbxt_assert_close_real(err, weights[3], 0.125f);
		}

		{
			ufbx_real weights[16];
			size_t knot = ufbx_evaluate_nurbs_basis(&open->basis, 1.0f, 16, weights, NULL);
			ufbxt_assert(knot == 0);
			ufbxt_assert_close_real(err, weights[0], 0.0f);
			ufbxt_assert_close_real(err, weights[1], 0.0f);
			ufbxt_assert_close_real(err, weights[2], 0.0f);
			ufbxt_assert_close_real(err, weights[3], 1.0f);
		}

		{
			ufbxt_curve_sample samples[] = {
				{ 0.00f, { 0.000000f, 0.0f, -1.000000f }, { -1.500000f, 0.0f, 0.000000f } },
				{ 0.10f, { -0.149500f, 0.0f, -0.985500f }, { -1.485000f, 0.0f, 0.285000f } },
				{ 0.20f, { -0.296000f, 0.0f, -0.944000f }, { -1.440000f, 0.0f, 0.540000f } },
				{ 0.30f, { -0.436500f, 0.0f, -0.878500f }, { -1.365000f, 0.0f, 0.765000f } },
				{ 0.40f, { -0.568000f, 0.0f, -0.792000f }, { -1.260000f, 0.0f, 0.960000f } },
				{ 0.50f, { -0.687500f, 0.0f, -0.687500f }, { -1.125000f, 0.0f, 1.125000f } },
				{ 0.60f, { -0.792000f, 0.0f, -0.568000f }, { -0.960000f, 0.0f, 1.260000f } },
				{ 0.70f, { -0.878500f, 0.0f, -0.436500f }, { -0.765000f, 0.0f, 1.365000f } },
				{ 0.80f, { -0.944000f, 0.0f, -0.296000f }, { -0.540000f, 0.0f, 1.440000f } },
				{ 0.90f, { -0.985500f, 0.0f, -0.149500f }, { -0.285000f, 0.0f, 1.485000f } },
				{ 1.00f, { -1.000000f, 0.0f, -0.000000f }, { 0.000000f, 0.0f, 1.500000f } },
			};

			for (size_t i = 0; i < ufbxt_arraycount(samples); i++) {
				ufbxt_hintf("i: %zu", i);
				const ufbxt_curve_sample *sample = &samples[i];
				ufbx_curve_point p = ufbx_evaluate_nurbs_curve_point(open, sample->u);
				ufbxt_assert_close_vec3(err, p.position, sample->position);
				ufbxt_assert_close_vec3(err, p.derivative, sample->derivative);
			}
		}
	}

	{
		ufbxt_assert_close_real(err, closed->basis.t_min, 1.0f);
		ufbxt_assert_close_real(err, closed->basis.t_max, 5.0f);
		ufbxt_assert(closed->basis.knot_vector.count == 11);
		ufbxt_assert(closed->basis.spans.count == 5);
		ufbxt_assert(closed->control_points.count == 6);

		{
			ufbx_real weights[16];
			size_t knot = ufbx_evaluate_nurbs_basis(&closed->basis, 3.14f, 16, weights, NULL);
			ufbxt_assert(knot == 2);
			ufbxt_assert_close_real(err, weights[0], 0.106009f);
			ufbxt_assert_close_real(err, weights[1], 0.648438f);
			ufbxt_assert_close_real(err, weights[2], 0.244866f);
			ufbxt_assert_close_real(err, weights[3], 0.000686f);
		}

		{
			ufbxt_curve_sample samples[] = {
				{ 1.00f, { -1.000000f, 0.0f, 0.000000f }, { 0.000000f, 0.0f, 1.500000f } },
				{ 1.30f, { -0.878500f, 0.0f, 0.436500f }, { 0.765000f, 0.0f, 1.365000f } },
				{ 1.60f, { -0.568000f, 0.0f, 0.792000f }, { 1.260000f, 0.0f, 0.960000f } },
				{ 1.90f, { -0.149500f, 0.0f, 0.985500f }, { 1.485000f, 0.0f, 0.285000f } },
				{ 2.20f, { 0.296000f, 0.0f, 0.944000f }, { 1.440000f, 0.0f, -0.540000f } },
				{ 2.50f, { 0.687500f, 0.0f, 0.687500f }, { 1.125000f, 0.0f, -1.125000f } },
				{ 2.80f, { 0.944000f, 0.0f, 0.296000f }, { 0.540000f, 0.0f, -1.440000f } },
				{ 3.10f, { 0.985500f, 0.0f, -0.149500f }, { -0.285000f, 0.0f, -1.485000f } },
				{ 3.40f, { 0.792000f, 0.0f, -0.568000f }, { -0.960000f, 0.0f, -1.260000f } },
				{ 3.70f, { 0.436500f, 0.0f, -0.878500f }, { -1.365000f, 0.0f, -0.765000f } },
				{ 4.00f, { 0.000000f, 0.0f, -1.000000f }, { -1.500000f, 0.0f, -0.000000f } },
				{ 4.30f, { -0.436500f, 0.0f, -0.878500f }, { -1.365000f, 0.0f, 0.765000f } },
				{ 4.60f, { -0.792000f, 0.0f, -0.568000f }, { -0.960000f, 0.0f, 1.260000f } },
				{ 4.90f, { -0.985500f, 0.0f, -0.149500f }, { -0.285000f, 0.0f, 1.485000f } },
			};

			for (size_t i = 0; i < ufbxt_arraycount(samples); i++) {
				ufbxt_hintf("i: %zu", i);
				const ufbxt_curve_sample *sample = &samples[i];
				ufbx_curve_point p = ufbx_evaluate_nurbs_curve_point(closed, sample->u);
				ufbxt_assert_close_vec3(err, p.position, sample->position);
				ufbxt_assert_close_vec3(err, p.derivative, sample->derivative);
			}
		}
	}

	{
		ufbxt_assert_close_real(err, periodic->basis.t_min, 0.0f);
		ufbxt_assert_close_real(err, periodic->basis.t_max, 4.0f);
		ufbxt_assert(periodic->basis.knot_vector.count == 11);
		ufbxt_assert(periodic->basis.spans.count == 5);
		ufbxt_assert(periodic->control_points.count == 4);

		{
			ufbxt_curve_sample samples[] = {
				{ 0.00f, { 0.000000f, 0.0f, -1.000000f }, { -1.500000f, 0.0f, 0.000000f } },
				{ 0.20f, { -0.296000f, 0.0f, -0.944000f }, { -1.440000f, 0.0f, 0.540000f } },
				{ 0.40f, { -0.568000f, 0.0f, -0.792000f }, { -1.260000f, 0.0f, 0.960000f } },
				{ 0.60f, { -0.792000f, 0.0f, -0.568000f }, { -0.960000f, 0.0f, 1.260000f } },
				{ 0.80f, { -0.944000f, 0.0f, -0.296000f }, { -0.540000f, 0.0f, 1.440000f } },
				{ 1.00f, { -1.000000f, 0.0f, 0.000000f }, { 0.000000f, 0.0f, 1.500000f } },
				{ 1.20f, { -0.944000f, 0.0f, 0.296000f }, { 0.540000f, 0.0f, 1.440000f } },
				{ 1.40f, { -0.792000f, 0.0f, 0.568000f }, { 0.960000f, 0.0f, 1.260000f } },
				{ 1.60f, { -0.568000f, 0.0f, 0.792000f }, { 1.260000f, 0.0f, 0.960000f } },
				{ 1.80f, { -0.296000f, 0.0f, 0.944000f }, { 1.440000f, 0.0f, 0.540000f } },
				{ 2.00f, { -0.000000f, 0.0f, 1.000000f }, { 1.500000f, 0.0f, 0.000000f } },
				{ 2.20f, { 0.296000f, 0.0f, 0.944000f }, { 1.440000f, 0.0f, -0.540000f } },
				{ 2.40f, { 0.568000f, 0.0f, 0.792000f }, { 1.260000f, 0.0f, -0.960000f } },
				{ 2.60f, { 0.792000f, 0.0f, 0.568000f }, { 0.960000f, 0.0f, -1.260000f } },
				{ 2.80f, { 0.944000f, 0.0f, 0.296000f }, { 0.540000f, 0.0f, -1.440000f } },
				{ 3.00f, { 1.000000f, 0.0f, -0.000000f }, { -0.000000f, 0.0f, -1.500000f } },
				{ 3.20f, { 0.944000f, 0.0f, -0.296000f }, { -0.540000f, 0.0f, -1.440000f } },
				{ 3.40f, { 0.792000f, 0.0f, -0.568000f }, { -0.960000f, 0.0f, -1.260000f } },
				{ 3.60f, { 0.568000f, 0.0f, -0.792000f }, { -1.260000f, 0.0f, -0.960000f } },
				{ 3.80f, { 0.296000f, 0.0f, -0.944000f }, { -1.440000f, 0.0f, -0.540000f } },
				{ 4.00f, { -0.000000f, 0.0f, -1.000000f }, { -1.500000f, 0.0f, 0.000000f } },
			};

			for (size_t i = 0; i < ufbxt_arraycount(samples); i++) {
				ufbxt_hintf("i: %zu", i);
				const ufbxt_curve_sample *sample = &samples[i];
				ufbx_curve_point p = ufbx_evaluate_nurbs_curve_point(periodic, sample->u);
				ufbxt_assert_close_vec3(err, p.position, sample->position);
				ufbxt_assert_close_vec3(err, p.derivative, sample->derivative);
			}
		}
	}
}
#endif

