#undef UFBXT_TEST_GROUP
#define UFBXT_TEST_GROUP "math"

#if UFBXT_IMPL

static ufbx_real ufbxt_quat_error(ufbx_quat a, ufbx_quat b)
{
	double pos = fabs(a.x-b.x) + fabs(a.y-b.y) + fabs(a.z-b.z) + fabs(a.w-b.w);
	double neg = fabs(a.x+b.x) + fabs(a.y+b.y) + fabs(a.z+b.z) + fabs(a.w+b.w);
	return (ufbx_real)(pos < neg ? pos : neg);
}

static uint32_t ufbxt_xorshift32(uint32_t *state)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return *state = x;
}

static ufbx_real ufbxt_xorshift32_real(uint32_t *state)
{
	uint32_t u = ufbxt_xorshift32(state);
	return (ufbx_real)u * (ufbx_real)2.3283064365386962890625e-10;
}

#endif

UFBXT_TEST(quat_to_euler_structured)
#if UFBXT_IMPL
{
	ufbxt_diff_error err = { 0 };

	for (int iorder = 0; iorder < 6; iorder++) {
		ufbx_rotation_order order = (ufbx_rotation_order)iorder;

		for (int x = -360; x <= 360; x += 45)
		for (int y = -360; y <= 360; y += 45)
		for (int z = -360; z <= 360; z += 45) {
			ufbx_vec3 v = { (ufbx_real)x, (ufbx_real)y, (ufbx_real)z };

			ufbx_quat q = ufbx_euler_to_quat(v, order);
			ufbx_vec3 v2 = ufbx_quat_to_euler(q, order);
			ufbx_quat q2 = ufbx_euler_to_quat(v2, order);

			ufbxt_assert_close_real(&err, ufbxt_quat_error(q, q2), 0.0f);
			ufbxt_assert_close_real(&err, ufbxt_quat_error(q, q2), 0.0f);
		}
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
}
#endif

UFBXT_TEST(quat_to_euler_random)
#if UFBXT_IMPL
{
	size_t steps = ufbxt_begin_fuzz() ? 10000000 : 100000;
	ufbxt_diff_error err = { 0 };

	for (int iorder = 0; iorder < 6; iorder++) {
		ufbx_rotation_order order = (ufbx_rotation_order)iorder;

		uint32_t state = 1;

		for (size_t i = 0; i < steps; i++) {
			if (g_fuzz && ufbxt_fuzz_should_skip((int)i >> 8)) continue;

			ufbx_quat q;
			q.x = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
			q.y = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
			q.z = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
			q.w = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
			ufbx_real qm = (ufbx_real)sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
			q.x /= qm;
			q.y /= qm;
			q.z /= qm;
			q.w /= qm;

			ufbx_vec3 v = ufbx_quat_to_euler(q, order);
			ufbx_quat q2 = ufbx_euler_to_quat(v, order);

			ufbxt_assert_close_real(&err, ufbxt_quat_error(q, q2), 0.0f);
		}
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
}
#endif

UFBXT_TEST(matrix_to_transform_structured)
#if UFBXT_IMPL
{
	ufbxt_diff_error err = { 0 };

	for (int sx = -2; sx <= 2; sx++)
	for (int sy = -2; sy <= 2; sy++)
	for (int sz = -2; sz <= 2; sz++)
	for (int rx = -2; rx <= 2; rx++)
	for (int ry = -2; ry <= 2; ry++)
	for (int rz = -2; rz <= 2; rz++)
	for (int rw = -2; rw <= 2; rw++)
	{
		// TODO: Support single axis squish?
		if (sx == 0 || sy == 0 || sz == 0) continue;
		if (rx == 0 && ry == 0 && rz == 0 && rw == 0) continue;

		ufbx_transform t;

		ufbx_quat q = { (ufbx_real)rx / 3.0f, (ufbx_real)ry / 3.0f, (ufbx_real)rz / 3.0f, (ufbx_real)rw / 3.0f };
		ufbx_real qm = (ufbx_real)sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
		t.rotation.x = q.x / qm;
		t.rotation.y = q.y / qm;
		t.rotation.z = q.z / qm;
		t.rotation.w = q.w / qm;

		t.translation.x = 1.0f;
		t.translation.y = 2.0f;
		t.translation.z = 3.0f;
		t.scale.x = (ufbx_real)sx / 2.0f;
		t.scale.y = (ufbx_real)sy / 2.0f;
		t.scale.z = (ufbx_real)sz / 2.0f;

		ufbx_matrix m = ufbx_transform_to_matrix(&t);
		ufbx_transform t2 = ufbx_matrix_to_transform(&m);

		if (sx < 0 || sy < 0 || sz < 0) {
			// Flipped signs cannot be uniquely recovered, check that the transforms are identical
			ufbx_matrix m2 = ufbx_transform_to_matrix(&t2);
			ufbxt_assert_close_matrix(&err, m, m2);
		} else {
			ufbxt_assert_close_vec3(&err, t.translation, t2.translation);
			ufbxt_assert_close_vec3(&err, t.scale, t2.scale);
			ufbxt_assert_close_real(&err, ufbxt_quat_error(t.rotation, t2.rotation), 0.0f);
		}
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
}
#endif

UFBXT_TEST(matrix_to_transform_random)
#if UFBXT_IMPL
{
	ufbxt_diff_error err = { 0 };

	uint32_t state = 1;
	size_t steps = ufbxt_begin_fuzz() ? 1000000 : 100000;

	for (size_t i = 0; i < steps; i++) {
		if (g_fuzz && ufbxt_fuzz_should_skip((int)i >> 4)) continue;

		ufbx_transform t;

		ufbx_quat q;
		q.x = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
		q.y = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
		q.z = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
		q.w = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
		ufbx_real qm = (ufbx_real)sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
		t.rotation.x = q.x / qm;
		t.rotation.y = q.y / qm;
		t.rotation.z = q.z / qm;
		t.rotation.w = q.w / qm;

		t.translation.x = ufbxt_xorshift32_real(&state) * 20.0f - 10.0f;
		t.translation.y = ufbxt_xorshift32_real(&state) * 20.0f - 10.0f;
		t.translation.z = ufbxt_xorshift32_real(&state) * 20.0f - 10.0f;
		t.scale.x = ufbxt_xorshift32_real(&state) * 10.0f + 0.01f;
		t.scale.y = ufbxt_xorshift32_real(&state) * 10.0f + 0.01f;
		t.scale.z = ufbxt_xorshift32_real(&state) * 10.0f + 0.01f;

		uint32_t flip = ufbxt_xorshift32(&state);

		// Prevent most of the inputs being flips
		if (flip & 8) flip = 0;

		if (flip & 1) t.scale.x *= -1.0f;
		if (flip & 2) t.scale.y *= -1.0f;
		if (flip & 4) t.scale.z *= -1.0f;

		ufbx_matrix m = ufbx_transform_to_matrix(&t);
		ufbx_transform t2 = ufbx_matrix_to_transform(&m);

		if (flip) {
			// Flipped signs cannot be uniquely recovered, check that the transforms are identical
			ufbx_matrix m2 = ufbx_transform_to_matrix(&t2);
			ufbxt_assert_close_matrix(&err, m, m2);
		} else {
			ufbxt_assert_close_vec3(&err, t.translation, t2.translation);
			ufbxt_assert_close_vec3(&err, t.scale, t2.scale);
			ufbxt_assert_close_real(&err, ufbxt_quat_error(t.rotation, t2.rotation), 0.0f);
		}
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
}
#endif

UFBXT_TEST(matrix_inverse_simple)
#if UFBXT_IMPL
{
	ufbxt_diff_error err = { 0 };

	{
		ufbx_matrix m = { 0 };
		m.m00 = 2.0f;
		m.m11 = 0.5f;
		m.m22 = 0.25f;
		m.m03 = 1.0f;
		m.m13 = 2.0f;
		m.m23 = 3.0f;

		ufbx_matrix im = ufbx_matrix_invert(&m);
		ufbxt_assert_close_real(&err, im.m00, 0.5f);
		ufbxt_assert_close_real(&err, im.m10, 0.0f);
		ufbxt_assert_close_real(&err, im.m20, 0.0f);
		ufbxt_assert_close_real(&err, im.m01, 0.0f);
		ufbxt_assert_close_real(&err, im.m11, 2.0f);
		ufbxt_assert_close_real(&err, im.m21, 0.0f);
		ufbxt_assert_close_real(&err, im.m02, 0.0f);
		ufbxt_assert_close_real(&err, im.m12, 0.0f);
		ufbxt_assert_close_real(&err, im.m22, 4.0f);
		ufbxt_assert_close_real(&err, im.m03, -0.5f);
		ufbxt_assert_close_real(&err, im.m13, -4.0f);
		ufbxt_assert_close_real(&err, im.m23, -12.0f);
	}

	{
		ufbx_matrix m = { 0 };
		m.m00 = 1.0f;
		m.m12 = -1.0f;
		m.m21 = 1.0f;
		m.m13 = 1.0f;
		m.m23 = 2.0f;

		ufbx_matrix im = ufbx_matrix_invert(&m);
		ufbxt_assert_close_real(&err, im.m00, 1.0f);
		ufbxt_assert_close_real(&err, im.m10, 0.0f);
		ufbxt_assert_close_real(&err, im.m20, 0.0f);
		ufbxt_assert_close_real(&err, im.m01, 0.0f);
		ufbxt_assert_close_real(&err, im.m11, 0.0f);
		ufbxt_assert_close_real(&err, im.m21, -1.0f);
		ufbxt_assert_close_real(&err, im.m02, 0.0f);
		ufbxt_assert_close_real(&err, im.m12, 1.0f);
		ufbxt_assert_close_real(&err, im.m22, 0.0f);
		ufbxt_assert_close_real(&err, im.m03, 0.0f);
		ufbxt_assert_close_real(&err, im.m13, -2.0f);
		ufbxt_assert_close_real(&err, im.m23, 1.0f);
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
}
#endif

UFBXT_TEST(matrix_inverse_random)
#if UFBXT_IMPL
{
	ufbxt_diff_error err = { 0 };

	size_t steps = ufbxt_begin_fuzz() ? 1000000 : 10000;

	uint32_t state = 1;

	for (size_t i = 0; i < steps; i++) {
		if (g_fuzz && ufbxt_fuzz_should_skip((int)i >> 4)) continue;

		ufbx_transform t;

		ufbx_quat q;
		q.x = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
		q.y = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
		q.z = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
		q.w = ufbxt_xorshift32_real(&state) * 2.0f - 1.0f;
		ufbx_real qm = (ufbx_real)sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
		t.rotation.x = q.x / qm;
		t.rotation.y = q.y / qm;
		t.rotation.z = q.z / qm;
		t.rotation.w = q.w / qm;

		t.translation.x = ufbxt_xorshift32_real(&state) * 20.0f - 10.0f;
		t.translation.y = ufbxt_xorshift32_real(&state) * 20.0f - 10.0f;
		t.translation.z = ufbxt_xorshift32_real(&state) * 20.0f - 10.0f;
		t.scale.x = ufbxt_xorshift32_real(&state) * 10.0f + 0.1f;
		t.scale.y = ufbxt_xorshift32_real(&state) * 10.0f + 0.1f;
		t.scale.z = ufbxt_xorshift32_real(&state) * 10.0f + 0.1f;

		uint32_t flip = ufbxt_xorshift32(&state);

		// Prevent most of the inputs being flips
		if (flip & 8) flip = 0;

		if (flip & 1) t.scale.x *= -1.0f;
		if (flip & 2) t.scale.y *= -1.0f;
		if (flip & 4) t.scale.z *= -1.0f;

		ufbx_matrix m = ufbx_transform_to_matrix(&t);
		ufbx_matrix im = ufbx_matrix_invert(&m);
		ufbx_matrix identity = ufbx_matrix_mul(&m, &im);

		ufbxt_assert_close_real(&err, identity.m00, 1.0f);
		ufbxt_assert_close_real(&err, identity.m10, 0.0f);
		ufbxt_assert_close_real(&err, identity.m20, 0.0f);
		ufbxt_assert_close_real(&err, identity.m01, 0.0f);
		ufbxt_assert_close_real(&err, identity.m11, 1.0f);
		ufbxt_assert_close_real(&err, identity.m21, 0.0f);
		ufbxt_assert_close_real(&err, identity.m02, 0.0f);
		ufbxt_assert_close_real(&err, identity.m12, 0.0f);
		ufbxt_assert_close_real(&err, identity.m22, 1.0f);
		ufbxt_assert_close_real(&err, identity.m03, 0.0f);
		ufbxt_assert_close_real(&err, identity.m13, 0.0f);
		ufbxt_assert_close_real(&err, identity.m23, 0.0f);
	}

	ufbxt_logf(".. Absolute diff: avg %.3g, max %.3g (%zu tests)", err.sum / (ufbx_real)err.num, err.max, err.num);
}
#endif
