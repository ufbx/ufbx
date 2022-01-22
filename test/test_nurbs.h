
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

UFBXT_FILE_TEST(max_nurbs_curve_rational)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "Curve001");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_NURBS_CURVE);
	ufbx_nurbs_curve *curve = (ufbx_nurbs_curve*)node->attrib;

	{
		ufbxt_curve_sample samples[] = {
			{ 0.000000f, { 0.000000f, -40.000000f, 0.000000f }, { -24.322954f, 0.000000f, 6.080737f } },
			{ 0.974593f, { -17.005555f, -37.570554f, 4.572092f }, { -12.337594f, 4.358807f, 3.678146f } },
			{ 1.949186f, { -26.036811f, -32.223433f, 7.596081f }, { -6.784099f, 6.393973f, 2.654863f } },
			{ 2.923779f, { -30.970646f, -25.399582f, 9.909292f }, { -3.585288f, 7.504839f, 2.146619f } },
			{ 3.898372f, { -33.353768f, -17.752430f, 11.854892f }, { -1.420225f, 8.123470f, 1.872876f } },
			{ 4.872965f, { -33.899033f, -9.674799f, 13.598350f }, { 0.243486f, 8.402346f, 1.719326f } },
			{ 5.847558f, { -32.960866f, -1.468619f, 15.227145f }, { 1.651919f, 8.390724f, 1.631066f } },
			{ 6.822152f, { -30.721680f, 6.587631f, 16.788850f }, { 2.927439f, 8.093263f, 1.577779f } },
			{ 7.796745f, { -27.278008f, 14.208835f, 18.307632f }, { 4.130179f, 7.494472f, 1.540372f } },
			{ 8.771338f, { -22.686868f, 21.090577f, 19.792049f }, { 5.283867f, 6.572031f, 1.505301f } },
			{ 9.745931f, { -16.994772f, 26.907367f, 21.239065f }, { 6.387689f, 5.306512f, 1.462049f } },
			{ 10.720524f, { -10.270443f, 31.351748f, 22.650823f }, { 7.377118f, 3.800029f, 1.451715f } },
			{ 11.695117f, { -2.706724f, 34.283196f, 24.096090f }, { 8.085573f, 2.198608f, 1.524685f } },
			{ 12.669710f, { 5.351791f, 35.610305f, 25.636226f }, { 8.370286f, 0.517035f, 1.639198f } },
			{ 13.644303f, { 13.435696f, 35.294565f, 27.290675f }, { 8.125935f, -1.151753f, 1.752252f } },
			{ 14.618896f, { 21.010971f, 33.416195f, 29.038561f }, { 7.331955f, -2.663870f, 1.825309f } },
			{ 15.593489f, { 27.575750f, 30.201053f, 30.826822f }, { 6.075349f, -3.873362f, 1.832138f } },
			{ 16.568082f, { 32.759100f, 25.997218f, 32.585338f }, { 4.530977f, -4.682518f, 1.764466f } },
			{ 17.542675f, { 36.382081f, 21.211799f, 34.244771f }, { 2.907510f, -5.069834f, 1.631536f } },
			{ 18.517268f, { 38.464958f, 16.297312f, 35.744787f }, { 1.405886f, -4.869993f, 1.433972f } },
			{ 19.491861f, { 39.216555f, 11.905556f, 37.036473f }, { 0.183372f, -4.095993f, 1.223988f } },
			{ 20.466455f, { 38.892809f, 8.347449f, 38.154114f }, { -0.825634f, -3.206871f, 1.084857f } },
			{ 21.441048f, { 37.616454f, 5.638129f, 39.185141f }, { -1.808639f, -2.363861f, 1.050754f } },
			{ 22.415641f, { 35.306462f, 3.716150f, 40.244592f }, { -2.930163f, -1.617486f, 1.138404f } },
			{ 23.390234f, { 31.980875f, 2.380957f, 41.416165f }, { -3.862865f, -1.167548f, 1.271465f } },
			{ 24.364827f, { 27.819473f, 1.365943f, 42.736393f }, { -4.662313f, -0.950026f, 1.446458f } },
			{ 25.339420f, { 22.913277f, 0.459596f, 44.257763f }, { -5.398160f, -0.950224f, 1.690832f } },
			{ 26.314013f, { 17.312988f, -0.588856f, 46.074971f }, { -6.083534f, -1.274792f, 2.070482f } },
			{ 27.288606f, { 11.096546f, -2.257805f, 48.396616f }, { -6.632300f, -2.349350f, 2.781984f } },
			{ 28.263199f, { 4.579470f, -5.973973f, 51.849971f }, { -6.516068f, -6.140789f, 4.679450f } },
			{ 29.237792f, { -0.000000f, -20.000002f, 60.000000f }, { -0.000000f, -32.801115f, 16.400558f } },
		};

		for (size_t i = 0; i < ufbxt_arraycount(samples); i++) {
			ufbxt_hintf("i: %zu", i);
			const ufbxt_curve_sample *sample = &samples[i];
			ufbx_curve_point p = ufbx_evaluate_nurbs_curve_point(curve, sample->u);
			ufbxt_assert_close_vec3(err, p.position, sample->position);
			ufbxt_assert_close_vec3(err, p.derivative, sample->derivative);
		}
	}
}
#endif
