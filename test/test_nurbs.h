
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

UFBXT_FILE_TEST(maya_nurbs_curve_multiplicity)
#if UFBXT_IMPL
{
	ufbx_node *node = ufbx_find_node(scene, "curve1");
	ufbxt_assert(node && node->attrib_type == UFBX_ELEMENT_NURBS_CURVE);
	ufbx_nurbs_curve *curve = (ufbx_nurbs_curve*)node->attrib;

	{
		ufbxt_assert_close_real(err, curve->basis.t_min, 0.0f);
		ufbxt_assert_close_real(err, curve->basis.t_max, 43.257f);

		for (size_t i = 1; i < curve->basis.spans.count; i++) {
			ufbxt_assert(curve->basis.spans.data[i - 1] < curve->basis.spans.data[i]);
		}

		ufbxt_curve_sample samples[] = {
			{ 0.000000f, { 0.908068f, 0.000000f, -0.405819f }, { 0.253319f, 0.000000f, -0.445464f } },
			{ 1.081421f, { 1.119511f, 0.000000f, -0.868651f }, { 0.205140f, 0.000000f, -0.285504f } },
			{ 2.162843f, { 1.387340f, 0.000000f, -0.963056f }, { 0.256681f, 0.000000f, 0.053404f } },
			{ 3.244264f, { 1.633207f, 0.000000f, -0.849916f }, { 0.200801f, 0.000000f, 0.139006f } },
			{ 4.325685f, { 1.835297f, 0.000000f, -0.683140f }, { 0.178149f, 0.000000f, 0.161468f } },
			{ 5.407107f, { 2.026478f, 0.000000f, -0.516048f }, { 0.177779f, 0.000000f, 0.141353f } },
			{ 6.488528f, { 2.222653f, 0.000000f, -0.388757f }, { 0.185504f, 0.000000f, 0.089817f } },
			{ 7.569949f, { 2.427523f, 0.000000f, -0.328685f }, { 0.192948f, 0.000000f, 0.019186f } },
			{ 8.651371f, { 2.638875f, 0.000000f, -0.349293f }, { 0.197550f, 0.000000f, -0.057045f } },
			{ 9.732792f, { 2.854870f, 0.000000f, -0.448812f }, { 0.202560f, 0.000000f, -0.124212f } },
			{ 10.814214f, { 3.080323f, 0.000000f, -0.608983f }, { 0.217038f, 0.000000f, -0.166482f } },
			{ 11.895635f, { 3.332994f, 0.000000f, -0.793795f }, { 0.255859f, 0.000000f, -0.166851f } },
			{ 12.977056f, { 3.649869f, 0.000000f, -0.948214f }, { 0.339710f, 0.000000f, -0.107146f } },
			{ 14.058478f, { 3.985671f, 0.000000f, 0.194786f }, { -0.143623f, 0.000000f, 5.538350f } },
			{ 15.139899f, { 3.504179f, 0.000000f, 3.786682f }, { -0.683192f, 0.000000f, 1.734052f } },
			{ 16.221320f, { 2.613011f, 0.000000f, 4.871228f }, { -0.929318f, 0.000000f, 0.531104f } },
			{ 17.302742f, { 1.550585f, 0.000000f, 5.237214f }, { -1.016831f, 0.000000f, 0.198109f } },
			{ 18.384163f, { 0.445465f, 0.000000f, 5.328320f }, { -1.014626f, 0.000000f, -0.029385f } },
			{ 19.465584f, { -0.620174f, 0.000000f, 5.174213f }, { -0.946609f, 0.000000f, -0.255004f } },
			{ 20.547006f, { -1.584181f, 0.000000f, 4.779433f }, { -0.829428f, 0.000000f, -0.473124f } },
			{ 21.628427f, { -2.402332f, 0.000000f, 4.157381f }, { -0.679606f, 0.000000f, -0.673164f } },
			{ 22.709848f, { -3.048214f, 0.000000f, 3.335470f }, { -0.513558f, 0.000000f, -0.839798f } },
			{ 23.791270f, { -3.513108f, 0.000000f, 2.360250f }, { -0.347591f, 0.000000f, -0.952957f } },
			{ 24.872691f, { -3.805868f, 0.000000f, 1.302539f }, { -0.197901f, 0.000000f, -0.987827f } },
			{ 25.954113f, { -3.952805f, 0.000000f, 0.262553f }, { -0.080577f, 0.000000f, -0.914853f } },
			{ 27.035534f, { -3.997572f, 0.000000f, -0.624962f }, { -0.011599f, 0.000000f, -0.699733f } },
			{ 28.116955f, { -2.943720f, 0.000000f, -0.502852f }, { 0.978119f, 0.000000f, 1.270640f } },
			{ 29.198377f, { -2.049079f, 0.000000f, -0.429651f }, { 0.608714f, 0.000000f, -0.657963f } },
			{ 30.279798f, { -1.426889f, 0.000000f, -0.907600f }, { 0.591440f, 0.000000f, -0.061570f } },
			{ 31.361219f, { -1.030212f, 0.000000f, -0.664031f }, { 0.116667f, 0.000000f, 0.406368f } },
			{ 32.442641f, { -0.988133f, 0.000000f, -0.182528f }, { 0.031241f, 0.000000f, 0.447589f } },
			{ 33.524062f, { -0.910710f, 0.000000f, 0.269968f }, { 0.117183f, 0.000000f, 0.384560f } },
			{ 34.605483f, { -0.736010f, 0.000000f, 0.651700f }, { 0.200556f, 0.000000f, 0.327364f } },
			{ 35.686905f, { -0.495401f, 0.000000f, 0.997855f }, { 0.233755f, 0.000000f, 0.324195f } },
			{ 36.768326f, { -0.254763f, 0.000000f, 1.378865f }, { 0.200505f, 0.000000f, 0.392064f } },
			{ 37.849747f, { -0.080667f, 0.000000f, 1.866711f }, { 0.115792f, 0.000000f, 0.516827f } },
			{ 38.931169f, { -0.006578f, 0.000000f, 2.501231f }, { 0.025863f, 0.000000f, 0.653179f } },
			{ 40.012590f, { 1.291208f, 0.000000f, 3.143478f }, { 1.958604f, 0.000000f, 0.677384f } },
			{ 41.094012f, { 0.552930f, 0.000000f, 3.783813f }, { -1.959784f, 0.000000f, 0.211471f } },
			{ 42.175433f, { -1.250738f, 0.000000f, 3.642614f }, { -1.058494f, 0.000000f, -0.348306f } },
			{ 43.256854f, { -1.669232f, 0.000000f, 3.251459f }, { 0.154248f, 0.000000f, -0.305748f } },
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
