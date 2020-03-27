#include "rtk.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#if 1

// -- Configuration

#define RTK_REGRESSION 0

#define BVH_MAX_DEPTH 32
#define BVH_BUILD_SPLITS 32

#define BVH_LEAF_MIN_ITEMS 4

#define BVH_LEAF_MAX_ITEMS 64
#define BVH_GROUP_MAX_VERTICES 256

#define SAH_BVH_COST 1.0f
#define SAH_ITEM_COST 1.0f

#define REAL_PI  ((rtk_real)3.14159265358979323846)
#define REAL_2PI ((rtk_real)6.28318530717958647693)

#define MAX_CONCURRENT_RAYS 8

#define rtk_assert(cond) assert(cond)

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
	#define RTK_SSE 1
	#include <xmmintrin.h>
	#include <emmintrin.h>
	#include <tmmintrin.h>
	#include <intrin.h>

	#define SSE_CTZ(ix, mask) _BitScanForward((unsigned long*)&(ix), (unsigned long)(mask))

	#define SSE_ALIGN __declspec(align(16))

	// #define SSE_RCP(v) _mm_rcp_ps(v)
	#define SSE_RCP(v) _mm_div_ps(_mm_set1_ps(1.0f), (v))
	#define SSE_ABS(v) _mm_andnot_ps(_mm_set1_ps(-0.0f), (v))

#else
	#define RTK_SSE 0
	#define SSE_ALIGN
#endif

#if defined(_MSC_VER)
	#define rtk_inline static __forceinline

	// Unary minus operator applied to unsigned type, result still unsigned
	#pragma warning(disable:4146)

	// Nonstandard extension used: nameless struct/union
	#pragma warning(disable: 4201) 

	// Structure was padded due to alignment specifier
	#pragma warning(disable: 4324) 

	// Enable /fp:precise as we depend on it for some operations
	#pragma float_control(precise, on)

#elif defined(__GNUC_)
	#define rtk_inline static __attribute__((always_inline))
#else
	#define rtk_inline static
#endif

#define rtk_static_assert(name, expr) typedef char rtk_assert_##name[(expr) != 0]

#if RTK_SSE
	#define rtk_prefetch(ptr) _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
	#define rtk_sqrt(v) _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(v)))
	#define rtk_abs(v) _mm_cvtss_f32(_mm_andnot_ps(_mm_set_ss(-0.0f), _mm_set_ss(v)))

	#define MM_BROADCAST(reg, lane) _mm_shuffle_ps((reg), (reg), _MM_SHUFFLE(lane, lane, lane, lane))
#else
	#define rtk_prefetch(ptr)
	#define rtk_sqrt(v) (rtk_real)sqrt(v)
	#define rtk_abs(v) (rtk_real)fabs(v)
#endif

#define rtk_for(type, name, begin, num) for (type *name = begin, *name##_end = name + (num); name != name##_end; name++)

// -- Math

rtk_inline rtk_real real_min(rtk_real a, rtk_real b)
{
#if RTK_SSE
	return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a), _mm_set_ss(b)));
#else
	return a < b ? a : b;
#endif
}
rtk_inline rtk_real real_max(rtk_real a, rtk_real b)
{
#if RTK_SSE
	return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(b)));
#else
	return b < a ? a : b;
#endif
}

rtk_inline uint32_t align_up_u32(uint32_t v, uint32_t align) {
	return v + ((uint32_t)-v & (align - 1));
}
rtk_inline size_t align_up_sz(size_t v, size_t align) {
	return v + ((size_t)-v & (align - 1));
}

// -- Vector operations

rtk_inline rtk_vec3 v_make(rtk_real x, rtk_real y, rtk_real z)
{
	rtk_vec3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

rtk_inline rtk_vec3 v_add(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
	v.x = a.x + b.x;
	v.y = a.y + b.y;
	v.z = a.z + b.z;
	return v;
}

rtk_inline rtk_vec3 v_sub(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	v.z = a.z - b.z;
	return v;
}

rtk_inline rtk_vec3 v_mul(rtk_vec3 a, rtk_real b) {
	rtk_vec3 v;
	v.x = a.x * b;
	v.y = a.y * b;
	v.z = a.z * b;
	return v;
}

rtk_inline rtk_vec3 v_mad(rtk_vec3 a, rtk_real b, rtk_vec3 c) {
	rtk_vec3 v;
	v.x = a.x * b + c.x;
	v.y = a.y * b + c.y;
	v.z = a.z * b + c.z;
	return v;
}

rtk_inline rtk_vec3 v_mul_comp(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
	v.x = a.x * b.x;
	v.y = a.y * b.y;
	v.z = a.z * b.z;
	return v;
}

rtk_inline rtk_real v_dot(rtk_vec3 a, rtk_vec3 b) {
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

rtk_inline rtk_vec3 v_cross(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
	v.x = a.y*b.z - a.z*b.y;
	v.y = a.z*b.x - a.x*b.z;
	v.z = a.x*b.y - a.y*b.x;
	return v;
}

rtk_inline rtk_vec3 v_normalize(rtk_vec3 a) {
	rtk_real rcp_len = 1.0f / rtk_sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
	rtk_vec3 v;
	v.x = a.x * rcp_len;
	v.y = a.y * rcp_len;
	v.z = a.z * rcp_len;
	return v;
}

rtk_inline rtk_vec3 v_min(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
#if RTK_SSE
	v.x = _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a.x), _mm_set_ss(b.x)));
	v.y = _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a.y), _mm_set_ss(b.y)));
	v.z = _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a.z), _mm_set_ss(b.z)));
#else
	v.x = a.x < b.x ? a.x : b.x;
	v.y = a.y < b.y ? a.y : b.y;
	v.z = a.z < b.z ? a.z : b.z;
#endif
	return v;
}

rtk_inline rtk_vec3 v_max(rtk_vec3 a, rtk_vec3 b) {
	rtk_vec3 v;
#if RTK_SSE
	v.x = _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a.x), _mm_set_ss(b.x)));
	v.y = _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a.y), _mm_set_ss(b.y)));
	v.z = _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a.z), _mm_set_ss(b.z)));
#else
	v.x = b.x < a.x ? a.x : b.x;
	v.y = b.y < a.y ? a.y : b.y;
	v.z = b.z < a.z ? a.z : b.z;
#endif
	return v;
}

rtk_inline rtk_vec3 v_abs(rtk_vec3 a) {
	rtk_vec3 v;
	v.x = rtk_abs(a.x);
	v.y = rtk_abs(a.y);
	v.z = rtk_abs(a.z);
	return v;
}

rtk_inline rtk_vec3 v_zero() {
	rtk_vec3 v = { 0.0f, 0.0f, 0.0f };
	return v;
}

rtk_inline rtk_vec3 v_pos_inf() {
	rtk_vec3 v = { +RTK_INF, +RTK_INF, +RTK_INF };
	return v;
}

rtk_inline rtk_vec3 v_neg_inf() {
	rtk_vec3 v = { -RTK_INF, -RTK_INF, -RTK_INF };
	return v;
}

// -- Matrix operations

rtk_inline rtk_vec3 mat_mul_pos(const rtk_matrix *m, rtk_vec3 v)
{
	rtk_vec3 r;
#if RTK_SSE
	__m128 m0 = _mm_loadu_ps(m->cols[0].v);
	__m128 m1 = _mm_loadu_ps(m->cols[1].v);
	__m128 m2 = _mm_loadu_ps(m->cols[2].v);
	__m128 m3 = _mm_loadu_ps(m->cols[3].v - 1);
	m3 = _mm_shuffle_ps(m3, m3, _MM_SHUFFLE(3,3,2,1));

	__m128 mr = _mm_mul_ps(m0, _mm_set1_ps(v.x));
	mr = _mm_add_ps(mr, _mm_mul_ps(m1, _mm_set1_ps(v.y)));
	mr = _mm_add_ps(mr, _mm_mul_ps(m2, _mm_set1_ps(v.z)));
	mr = _mm_add_ps(mr, m3);

	r.x = _mm_cvtss_f32(mr);
	r.y = _mm_cvtss_f32(_mm_shuffle_ps(mr, mr, _MM_SHUFFLE(3,2,1,1)));
	r.z = _mm_cvtss_f32(_mm_shuffle_ps(mr, mr, _MM_SHUFFLE(3,2,1,2)));
#else
	r.x = v.x*m->m00 + v.y*m->m01 + v.z*m->m02 + m->m03;
	r.y = v.x*m->m10 + v.y*m->m11 + v.z*m->m12 + m->m13;
	r.z = v.x*m->m20 + v.y*m->m21 + v.z*m->m22 + m->m23;
#endif
	return r;
}

rtk_inline rtk_vec3 mat_mul_dir(const rtk_matrix *m, rtk_vec3 v)
{
	rtk_vec3 r;
#if RTK_SSE
	__m128 m0 = _mm_loadu_ps(m->cols[0].v);
	__m128 m1 = _mm_loadu_ps(m->cols[1].v);
	__m128 m2 = _mm_loadu_ps(m->cols[2].v);

	__m128 mr = _mm_mul_ps(m0, _mm_set1_ps(v.x));
	mr = _mm_add_ps(mr, _mm_mul_ps(m1, _mm_set1_ps(v.y)));
	mr = _mm_add_ps(mr, _mm_mul_ps(m2, _mm_set1_ps(v.z)));

	r.x = _mm_cvtss_f32(mr);
	r.y = _mm_cvtss_f32(_mm_shuffle_ps(mr, mr, _MM_SHUFFLE(3,2,1,1)));
	r.z = _mm_cvtss_f32(_mm_shuffle_ps(mr, mr, _MM_SHUFFLE(3,2,1,2)));
#else
	r.x = v.x*m->m00 + v.y*m->m01 + v.z*m->m02;
	r.y = v.x*m->m10 + v.y*m->m11 + v.z*m->m12;
	r.z = v.x*m->m20 + v.y*m->m21 + v.z*m->m22;
#endif
	return r;
}

rtk_inline rtk_vec3 mat_mul_dir_transpose(const rtk_matrix *m, rtk_vec3 v)
{
	rtk_vec3 r;
#if RTK_SSE
	__m128 c0 = _mm_loadu_ps(m->cols[0].v);
	__m128 c1 = _mm_loadu_ps(m->cols[1].v);
	__m128 c2 = _mm_loadu_ps(m->cols[2].v);

	__m128 t0 = _mm_shuffle_ps(c0, c1, _MM_SHUFFLE(1,0,1,0));
	__m128 t1 = _mm_shuffle_ps(c0, c1, _MM_SHUFFLE(3,2,3,2));

	__m128 m0 = _mm_shuffle_ps(t0, c2, _MM_SHUFFLE(0,0,2,0));
	__m128 m1 = _mm_shuffle_ps(t0, c2, _MM_SHUFFLE(1,1,3,1));
	__m128 m2 = _mm_shuffle_ps(t1, c2, _MM_SHUFFLE(2,2,2,0));

	__m128 mr = _mm_mul_ps(m0, _mm_set1_ps(v.x));
	mr = _mm_add_ps(mr, _mm_mul_ps(m1, _mm_set1_ps(v.y)));
	mr = _mm_add_ps(mr, _mm_mul_ps(m2, _mm_set1_ps(v.z)));

	r.x = _mm_cvtss_f32(mr);
	r.y = _mm_cvtss_f32(_mm_shuffle_ps(mr, mr, _MM_SHUFFLE(3,2,1,1)));
	r.z = _mm_cvtss_f32(_mm_shuffle_ps(mr, mr, _MM_SHUFFLE(3,2,1,2)));
#else
	r.x = v.x*m->m00 + v.y*m->m10 + v.z*m->m20;
	r.y = v.x*m->m01 + v.y*m->m11 + v.z*m->m21;
	r.z = v.x*m->m02 + v.y*m->m12 + v.z*m->m22;
#endif
	return r;
}

rtk_inline rtk_vec3 mat_mul_dir_abs(const rtk_matrix *m, rtk_vec3 v)
{
	rtk_vec3 r;
#if RTK_SSE
	__m128 m0 = SSE_ABS(_mm_loadu_ps(m->cols[0].v));
	__m128 m1 = SSE_ABS(_mm_loadu_ps(m->cols[1].v));
	__m128 m2 = SSE_ABS(_mm_loadu_ps(m->cols[2].v));

	// Clamp to zero to flush NANs leading from inf*0 to 0
	__m128 zero = _mm_setzero_ps();
	__m128 mr = _mm_max_ps(_mm_mul_ps(m0, _mm_set1_ps(v.x)), zero);
	mr = _mm_add_ps(mr, _mm_max_ps(_mm_mul_ps(m1, _mm_set1_ps(v.y)), zero));
	mr = _mm_add_ps(mr, _mm_max_ps(_mm_mul_ps(m2, _mm_set1_ps(v.z)), zero));

	r.x = _mm_cvtss_f32(mr);
	r.y = _mm_cvtss_f32(_mm_shuffle_ps(mr, mr, _MM_SHUFFLE(3,2,1,1)));
	r.z = _mm_cvtss_f32(_mm_shuffle_ps(mr, mr, _MM_SHUFFLE(3,2,1,2)));
#else

	// Clamp to zero to flush NANs leading from inf*0 to 0
	r.x  = real_max(v.x*rtk_abs(m->m00), 0.0f);
	r.x += real_max(v.y*rtk_abs(m->m01), 0.0f);
	r.x += real_max(v.z*rtk_abs(m->m02), 0.0f);
	r.y  = real_max(v.x*rtk_abs(m->m10), 0.0f);
	r.y += real_max(v.y*rtk_abs(m->m11), 0.0f);
	r.y += real_max(v.z*rtk_abs(m->m12), 0.0f);
	r.z  = real_max(v.x*rtk_abs(m->m20), 0.0f);
	r.z += real_max(v.y*rtk_abs(m->m21), 0.0f);
	r.z += real_max(v.z*rtk_abs(m->m22), 0.0f);

#endif
	return r;
}

static void mat_mul_left(rtk_matrix *dst, const rtk_matrix *l)
{
#if RTK_SSE
	__m128 d0 = _mm_loadu_ps(dst->cols[0].v);
	__m128 l0 = _mm_loadu_ps(l->cols[0].v);
	__m128 d1 = _mm_loadu_ps(dst->cols[1].v);
	__m128 l1 = _mm_loadu_ps(l->cols[1].v);
	__m128 d2 = _mm_loadu_ps(dst->cols[2].v);
	__m128 l2 = _mm_loadu_ps(l->cols[2].v);
	__m128 d3_off = _mm_loadu_ps(dst->cols[3].v - 1);
	__m128 l3_off = _mm_loadu_ps(l->cols[3].v - 1);

	__m128 a = _mm_mul_ps(l0, _mm_shuffle_ps(d0, d0, _MM_SHUFFLE(0,0,0,0)));
	a = _mm_add_ps(a, _mm_mul_ps(l1, _mm_shuffle_ps(d0, d0, _MM_SHUFFLE(1,1,1,1))));
	a = _mm_add_ps(a, _mm_mul_ps(l2, _mm_shuffle_ps(d0, d0, _MM_SHUFFLE(2,2,2,2))));

	__m128 b = _mm_mul_ps(l0, _mm_shuffle_ps(d1, d1, _MM_SHUFFLE(0,0,0,0)));
	b = _mm_add_ps(b, _mm_mul_ps(l1, _mm_shuffle_ps(d1, d1, _MM_SHUFFLE(1,1,1,1))));
	b = _mm_add_ps(b, _mm_mul_ps(l2, _mm_shuffle_ps(d1, d1, _MM_SHUFFLE(2,2,2,2))));

	__m128 c = _mm_mul_ps(l0, _mm_shuffle_ps(d2, d2, _MM_SHUFFLE(0,0,0,0)));
	c = _mm_add_ps(c, _mm_mul_ps(l1, _mm_shuffle_ps(d2, d2, _MM_SHUFFLE(1,1,1,1))));
	c = _mm_add_ps(c, _mm_mul_ps(l2, _mm_shuffle_ps(d2, d2, _MM_SHUFFLE(2,2,2,2))));

	__m128 d_off = _mm_mul_ps(l0, _mm_shuffle_ps(d3_off, d3_off, _MM_SHUFFLE(1,1,1,1)));
	d_off = _mm_add_ps(d_off, _mm_mul_ps(l1, _mm_shuffle_ps(d3_off, d3_off, _MM_SHUFFLE(2,2,2,2))));
	d_off = _mm_add_ps(d_off, _mm_mul_ps(l2, _mm_shuffle_ps(d3_off, d3_off, _MM_SHUFFLE(3,3,3,3))));
	d_off = _mm_add_ps(d_off, l3_off);

	__m128 t0 = _mm_shuffle_ps(a, b, _MM_SHUFFLE(0,0,2,2));  // < AZ AZ BX BX
	__m128 p0 = _mm_shuffle_ps(a, t0, _MM_SHUFFLE(2,0,1,0)); // < AX AY AZ BX
	_mm_storeu_ps(dst->v + 0, p0);

	__m128 p1 = _mm_shuffle_ps(b, c, _MM_SHUFFLE(1,0,2,1)); // < BY BZ CX CY
	_mm_storeu_ps(dst->v + 4, p1);

	__m128 t1 = _mm_shuffle_ps(c, d_off, _MM_SHUFFLE(1,1,2,2));  // < CZ CZ DX DX
	__m128 p2 = _mm_shuffle_ps(t1, d_off, _MM_SHUFFLE(3,2,2,0)); // < CZ DX DY DZ
	_mm_storeu_ps(dst->v + 8, p2);
#else
	rtk_real m00 = dst->m00, m10 = dst->m10, m20 = dst->m20;
	rtk_real m01 = dst->m01, m11 = dst->m11, m21 = dst->m21;
	rtk_real m02 = dst->m02, m12 = dst->m12, m22 = dst->m22;
	rtk_real m03 = dst->m03, m13 = dst->m13, m23 = dst->m23;
	rtk_real a, b, c;

	a = l->m00; b = l->m01; c = l->m02;
	dst->m00 = a*m00 + b*m10 + c*m20;
	dst->m01 = a*m01 + b*m11 + c*m21;
	dst->m02 = a*m02 + b*m12 + c*m22;
	dst->m03 = a*m03 + b*m13 + c*m23 + l->m03;

	a = l->m10; b = l->m11; c = l->m12;
	dst->m10 = a*m00 + b*m10 + c*m20;
	dst->m11 = a*m01 + b*m11 + c*m21;
	dst->m12 = a*m02 + b*m12 + c*m22;
	dst->m13 = a*m03 + b*m13 + c*m23 + l->m13;

	a = l->m20; b = l->m21; c = l->m22;
	dst->m20 = a*m00 + b*m10 + c*m20;
	dst->m21 = a*m01 + b*m11 + c*m21;
	dst->m22 = a*m02 + b*m12 + c*m22;
	dst->m23 = a*m03 + b*m13 + c*m23 + l->m23;
#endif
}

static int mat_is_identity(const rtk_matrix *m)
{
#if RTK_SSE
	__m128 a = _mm_cmpeq_ps(_mm_loadu_ps(rtk_identity.v + 0), _mm_loadu_ps(m->v + 0));
	__m128 b = _mm_cmpeq_ps(_mm_loadu_ps(rtk_identity.v + 4), _mm_loadu_ps(m->v + 4));
	__m128 c = _mm_cmpeq_ps(_mm_loadu_ps(rtk_identity.v + 8), _mm_loadu_ps(m->v + 8));
	__m128 eq = _mm_and_ps(_mm_and_ps(a, b), c);
	return _mm_movemask_ps(eq) == 0x7 ? 1 : 0;
#else
	for (int i = 0; i < 12; i++) {
		if (m->v[i] != rtk_identity.v[i]) return 0;
	}
	return 1;
#endif
}

static void mat_inverse(rtk_matrix *dst, const rtk_matrix *src)
{
	rtk_matrix s = *src;
	rtk_real det =
		- s.m02*s.m11*s.m20 + s.m01*s.m12*s.m20 + s.m02*s.m10*s.m21
		- s.m00*s.m12*s.m21 - s.m01*s.m10*s.m22 + s.m00*s.m11*s.m22;
	rtk_real rcp_det = 1.0f / det;

	dst->m00 = ( - s.m12*s.m21 + s.m11*s.m22) * rcp_det;
	dst->m10 = ( + s.m12*s.m20 - s.m10*s.m22) * rcp_det;
	dst->m20 = ( - s.m11*s.m20 + s.m10*s.m21) * rcp_det;
	dst->m01 = ( + s.m02*s.m21 - s.m01*s.m22) * rcp_det;
	dst->m11 = ( - s.m02*s.m20 + s.m00*s.m22) * rcp_det;
	dst->m21 = ( + s.m01*s.m20 - s.m00*s.m21) * rcp_det;
	dst->m02 = ( - s.m02*s.m11 + s.m01*s.m12) * rcp_det;
	dst->m12 = ( + s.m02*s.m10 - s.m00*s.m12) * rcp_det;
	dst->m22 = ( - s.m01*s.m10 + s.m00*s.m11) * rcp_det;
	dst->m03 = (s.m03*s.m12*s.m21 - s.m02*s.m13*s.m21 - s.m03*s.m11*s.m22 + s.m01*s.m13*s.m22 + s.m02*s.m11*s.m23 - s.m01*s.m12*s.m23) * rcp_det;
	dst->m13 = (s.m02*s.m13*s.m20 - s.m03*s.m12*s.m20 + s.m03*s.m10*s.m22 - s.m00*s.m13*s.m22 - s.m02*s.m10*s.m23 + s.m00*s.m12*s.m23) * rcp_det;
	dst->m23 = (s.m03*s.m11*s.m20 - s.m01*s.m13*s.m20 - s.m03*s.m10*s.m21 + s.m00*s.m13*s.m21 + s.m01*s.m10*s.m23 - s.m00*s.m11*s.m23) * rcp_det;
}

// -- Misc utility

static void buf_realloc(void **data, uint32_t *p_cap, uint32_t num, size_t size)
{
	uint32_t cap = *p_cap;
	cap = cap ? cap * 2 : 4096 / (uint32_t)size;
	if (cap < num) cap = num;
	*p_cap = cap;
	*data = realloc(*data, cap * size);
}

rtk_inline void buf_grow_size(void **p_data, uint32_t *p_cap, uint32_t num, size_t size) {
	if (num <= *p_cap) return;
	buf_realloc(p_data, p_cap, num, size);
}

#define buf_grow(p_buf, p_cap, num) buf_grow_size((void**)(p_buf), (p_cap), (num), sizeof(**(p_buf)))

static size_t push_offset(size_t *p_pos, size_t size, size_t align)
{
	size_t pos = *p_pos;
	pos += (-pos & align);
	*p_pos = pos + size;
	return pos;
}

rtk_inline void bounds_reset(rtk_bounds *bounds)
{
	bounds->min = v_pos_inf();
	bounds->max = v_neg_inf();
}

rtk_inline void bounds_add(rtk_bounds *bounds, const rtk_bounds *other)
{
	bounds->min = v_min(bounds->min, other->min);
	bounds->max = v_max(bounds->max, other->max);
}

static rtk_real bounds_area(const rtk_bounds *bounds)
{
	rtk_vec3 d = v_sub(bounds->max, bounds->min);
	return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
}

static rtk_bounds transform_bounds(const rtk_bounds *bounds, const rtk_matrix *m)
{
	rtk_vec3 center = v_mul(v_add(bounds->min, bounds->max), 0.5f);
	rtk_vec3 extent = v_mul(v_sub(bounds->max, bounds->min), 0.5f);

	// Flush center +-inf/nan to zero
	center.x = rtk_abs(center.x) < RTK_INF ? center.x : 0.0f;
	center.y = rtk_abs(center.y) < RTK_INF ? center.y : 0.0f;
	center.z = rtk_abs(center.z) < RTK_INF ? center.z : 0.0f;

	center = mat_mul_pos(m, center);
	extent = mat_mul_dir_abs(m, extent);

	rtk_bounds r;
	r.min = v_sub(center, extent);
	r.max = v_add(center, extent);
	return r;
}

// -- Lookup

typedef uint32_t bit4_info;
#define b4_make(num, a, b, c, d) ((num) | (a) << 3 | (b) << 5 | (c) << 7 | (d) << 9)
#define b4_bitcount(info) ((info) & 0x7)
#define b4_bit(info, index) ((info) >> ((index)*2+3) & 0x3)

static const uint16_t bit4_tab[16] = {
	b4_make(0,0,0,0,0), b4_make(1,0,0,0,0), b4_make(1,1,0,0,0), b4_make(2,0,1,0,0),
	b4_make(1,2,0,0,0), b4_make(2,0,2,0,0), b4_make(2,1,2,0,0), b4_make(3,0,1,2,0),
	b4_make(1,3,0,0,0), b4_make(2,0,3,0,0), b4_make(2,1,3,0,0), b4_make(3,0,1,3,0),
	b4_make(2,2,3,0,0), b4_make(3,0,2,3,0), b4_make(3,1,2,3,0), b4_make(4,0,1,2,3),
};

static const uint32_t shuf_u32_tab[4] = {
	0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c,
};

// -- Types

// Pair of [0:32] vertex index [32:64] mesh index
typedef uint64_t vertex_id;
#define vi_make(mesh_ix, vert_ix) ((uint64_t)(mesh_ix) << 32u | (vert_ix))
#define vi_mesh(id) (uint32_t)((id) >> 32u)
#define vi_vertex(id) (uint32_t)(id)

// Set of unique vertices
typedef struct {
	vertex_id entries[BVH_GROUP_MAX_VERTICES];
	size_t size;
} vertex_set;

// Tagged pointer to either `bvh_node` (lsb=0) or `bvh_leaf` (lsb=1)
typedef uintptr_t bvh_ptr;
#define bp_is_node(ptr) (((ptr) & 1) == 0)
#define bp_make_node(node) ((uintptr_t)(node))
#define bp_make_leaf(leaf) ((uintptr_t)(leaf) | 1)
#define bp_node(ptr) (bvh_node*)(ptr)
#define bp_leaf(ptr) (bvh_leaf*)((ptr) ^ 1)

// Internal BVH node, aligned to 64 byte cache lines
typedef union {
	struct {
		rtk_real bounds_x[2][4];
		rtk_real bounds_y[2][4];
		rtk_real bounds_z[2][4];
		bvh_ptr ptr[4];
	};
	char pad_align[128];
} bvh_node;

rtk_static_assert(bvh_node_size, sizeof(bvh_node) == 128);

typedef struct {
	rtk_vec3 pos;   // < World space position
	uint32_t index; // < Index in the mesh
} leaf_vert;

// Triangle inside a leaf node
typedef struct {
	// NOTE: v[1] and v[2] are reversed since v[2] is needed first
	uint8_t v[3];    // < !!! Vertex indices into the current vertex group 
	uint8_t obj_ix;  // < Object index into the leaf object list
} leaf_tri;

// Triangle inside a leaf node
typedef struct {
	rtk_primitive prim;
	rtk_matrix inv_mat;
} leaf_prim;

// Mesh data ie. UVs and normals
typedef struct {
	rtk_object object;
	rtk_vec2 *uvs;
	rtk_vec3 *normals;
} mesh_data;

// Object that owns a triangle in a leaf node
typedef struct {
	uint32_t mesh_ix;
} leaf_tri_obj;

// BVH leaf node
// Memory layout:
// bvh_leaf leaf_head;
// leaf_tri tris[align_up(num_tris, 4)];
// leaf_prim prims[num_prims];
// leaf_tri_obj tri_objects[num_unique_meshes];
typedef struct {
	leaf_vert *vertices;
	uint32_t num_tris, num_prims;
} bvh_leaf;

// BVH stack frame used during traversal, contains the pointer to the
// further away node and intersection `t` value.
typedef struct {
	rtk_real t;
	bvh_ptr ptr;
} bvh_frame;

// Main raytracing context struct used for all traversal
typedef struct {
	rtk_ray ray;
	rtk_hit hit;
	uint32_t mesh_ix;

	#if RTK_SSE
		__m128 sse_origin;
		__m128 sse_rcp_dir;
		__m128i sse_shear_shuf;
	#else
		rtk_vec3 rcp_dir;
	#endif

	int shear_axis[3];
	rtk_vec3 shear;
	rtk_vec3 shear_origin;
	uint32_t sign_mask;

	uint32_t depth;
	float t_stack[BVH_MAX_DEPTH + 1];
	bvh_ptr node_stack[BVH_MAX_DEPTH + 1];

} raytrace;

#define MESH_IX_TRI (uint32_t)-2
#define MESH_IX_PRIM (uint32_t)-1

typedef struct {
	rtk_bounds bounds; // < Bounds of the item
	uint32_t mesh_ix;  // < Index into `desc.meshes`, MESH_IX_TRI if triangle, MESH_IX_PRIM if primitive
	union {
		// NOTE: Before the item is assigned into a vertex group (ie. the vertex set
		// of its parent node is closed) `vertex_ix` refers to `mesh.vertices`, afterwards
		// it refers to the local index in the group.
		uint32_t vertex_ix[3]; // < Vertex indices, see above
		uint32_t prim_ix;      // < Index into `desc.primitives`
	};
} build_item;

typedef struct {
	rtk_bounds bounds;          // < Bounds of the node
	uint32_t begin, num;        // < Range in `build.items`
	uint32_t child_ix;          // < Index of pair in `build.nodes`, ~0u if leaf
	uint32_t vertex_offset;     // < Offset into `build.vertices`, ~0u if unassigned
	uint32_t num_tris;          // < Number of triangles if leaf
} build_node;

typedef struct {
	rtk_bounds bounds;       // < Bounds of all the items in this bucket
	rtk_bounds bounds_right; // < Cumulative bounds for all the buckets from the right
	uint32_t num;            // < Number of items in the bucket
} build_bucket;

typedef enum {
	build_attrib_uv,
	build_attrib_normal,
} build_attrib_type;

typedef struct {
	const rtk_real *data;
	uint32_t num_data;
	uint32_t num_components;
	uint32_t mesh_ix;
	size_t final_offset;
	size_t stride;
	int largest_duplicate;
	build_attrib_type type;
} build_attrib;

// Main build context
typedef struct {
	rtk_scene_desc desc;

	build_item *items;

	build_node *nodes;
	uint32_t num_nodes, cap_nodes;

	leaf_vert *verts;
	uint32_t num_verts, cap_verts;

	build_attrib *attribs;
	uint32_t num_attribs, cap_attribs;

	size_t leaf_memory_top;

	uint32_t depth_num_nodes[BVH_MAX_DEPTH];
	uint32_t depth_node_offset[BVH_MAX_DEPTH];

	bvh_ptr null_leaf;

	bvh_node *final_nodes;
	char *final_leaves;
	size_t final_leaf_offset;
	leaf_vert *final_verts;
} build;

// Retained scene
struct rtk_scene {
	bvh_node root;     // < Root BVH node
	rtk_bounds bounds; // < Bounds of the whole scene

	mesh_data *meshes; // < Per mesh data

	void *memory;       // < Combined allocation
	size_t memory_size; // < Size of `memory` in bytes
};

// -- BVH traversal

// Intersect the ray with an AABB, returns `t` of the intersection
// or `RTK_RAY_INF` if the ray missed the AABB.
rtk_inline rtk_real intersect_aabb(raytrace *rt, const rtk_bounds *a)
{
#if RTK_SSE

	const __m128 origin = rt->sse_origin;
	const __m128 rcp_dir = rt->sse_rcp_dir;

	__m128 min = _mm_loadu_ps(a->min.v);
	__m128 max = _mm_loadu_ps(a->max.v);
	max = _mm_shuffle_ps(max, max, _MM_SHUFFLE(2,2,1,0));

	const __m128 t_lo = _mm_mul_ps(_mm_sub_ps(min, origin), rcp_dir);
	const __m128 t_hi = _mm_mul_ps(_mm_sub_ps(max, origin), rcp_dir);

	__m128 min_t = _mm_min_ps(t_lo, t_hi);
	__m128 max_t = _mm_max_ps(t_lo, t_hi);

	min_t = _mm_max_ps(min_t, _mm_set1_ps(rt->ray.min_t));

	min_t = _mm_max_ps(min_t, _mm_shuffle_ps(min_t, min_t, _MM_SHUFFLE(3,2,1,1)));
	min_t = _mm_max_ps(min_t, _mm_shuffle_ps(min_t, min_t, _MM_SHUFFLE(3,2,1,2)));
	max_t = _mm_min_ps(max_t, _mm_shuffle_ps(max_t, max_t, _MM_SHUFFLE(3,2,1,1)));
	max_t = _mm_min_ps(max_t, _mm_shuffle_ps(max_t, max_t, _MM_SHUFFLE(3,2,1,2)));

	__m128 mask = _mm_cmple_ss(min_t, max_t);
	__m128 result = _mm_or_ps(_mm_and_ps(mask, min_t), _mm_andnot_ps(mask, _mm_set1_ps(RTK_RAY_INF)));
	return _mm_cvtss_f32(result);

#else

	rtk_vec3 origin = rt->ray.origin;
	rtk_vec3 rcp_dir = rt->rcp_dir;

	rtk_vec3 min = a->min;
	rtk_vec3 max = a->max;

	rtk_vec3 t_lo = v_mul_comp(v_sub(min, origin), rcp_dir);
	rtk_vec3 t_hi = v_mul_comp(v_sub(max, origin), rcp_dir);

	rtk_vec3 min_t = v_min(t_lo, t_hi);
	rtk_vec3 max_t = v_max(t_lo, t_hi);

	rtk_real t_min = real_max(real_max(min_t.x, min_t.y), min_t.z);
	rtk_real t_max = real_min(real_min(max_t.x, max_t.y), max_t.z);

	return t_min <= t_max && t_max >= rt->ray.min_t ? t_min : RTK_RAY_INF;

#endif
}

rtk_inline rtk_real intersect_aabb_ray(const rtk_bounds *a, const rtk_ray *ray)
{
#if RTK_SSE

	const __m128 origin = _mm_loadu_ps(ray->origin.v);
	const __m128 rcp_dir = SSE_RCP(_mm_loadu_ps(ray->direction.v));

	__m128 min = _mm_loadu_ps(a->min.v);
	__m128 max = _mm_loadu_ps(a->max.v);
	max = _mm_shuffle_ps(max, max, _MM_SHUFFLE(2,2,1,0));

	const __m128 t_lo = _mm_mul_ps(_mm_sub_ps(min, origin), rcp_dir);
	const __m128 t_hi = _mm_mul_ps(_mm_sub_ps(max, origin), rcp_dir);

	__m128 min_t = _mm_min_ps(t_lo, t_hi);
	__m128 max_t = _mm_max_ps(t_lo, t_hi);

	min_t = _mm_max_ps(min_t, _mm_set1_ps(ray->min_t));

	min_t = _mm_max_ps(min_t, _mm_shuffle_ps(min_t, min_t, _MM_SHUFFLE(3,2,1,1)));
	min_t = _mm_max_ps(min_t, _mm_shuffle_ps(min_t, min_t, _MM_SHUFFLE(3,2,1,2)));
	max_t = _mm_min_ps(max_t, _mm_shuffle_ps(max_t, max_t, _MM_SHUFFLE(3,2,1,1)));
	max_t = _mm_min_ps(max_t, _mm_shuffle_ps(max_t, max_t, _MM_SHUFFLE(3,2,1,2)));

	__m128 mask = _mm_cmple_ss(min_t, max_t);
	__m128 result = _mm_or_ps(_mm_and_ps(mask, min_t), _mm_andnot_ps(mask, _mm_set1_ps(RTK_RAY_INF)));
	return _mm_cvtss_f32(result);

#else

	rtk_vec3 origin = ray->origin;
	rtk_vec3 rcp_dir;
	rcp_dir.x = 1.0f / ray->direction.x;
	rcp_dir.y = 1.0f / ray->direction.y;
	rcp_dir.z = 1.0f / ray->direction.z;

	rtk_vec3 min = a->min;
	rtk_vec3 max = a->max;

	rtk_vec3 t_lo = v_mul_comp(v_sub(min, origin), rcp_dir);
	rtk_vec3 t_hi = v_mul_comp(v_sub(max, origin), rcp_dir);

	rtk_vec3 min_t = v_min(t_lo, t_hi);
	rtk_vec3 max_t = v_max(t_lo, t_hi);

	rtk_real t_min = real_max(real_max(min_t.x, min_t.y), min_t.z);
	rtk_real t_max = real_min(real_min(max_t.x, max_t.y), max_t.z);

	return t_min <= t_max && t_max >= ray->min_t ? t_min : RTK_RAY_INF;

#endif
}

static void bvh_leaf_traverse(raytrace *rt, const bvh_leaf *leaf)
{
	// Resolve implicit pointers, see comment above `bvh_leaf`
	uint32_t num_tris_aligned = align_up_u32(leaf->num_tris, 4);
	const leaf_tri *tris = (const leaf_tri*)(leaf + 1);
	const leaf_prim *prims = (const leaf_prim*)(tris + num_tris_aligned);
	const leaf_prim *prims_end = prims + leaf->num_prims;
	const leaf_tri_obj *tri_objs = (const leaf_tri_obj*)prims_end;
	const leaf_vert *verts = leaf->vertices;


#if RTK_SSE

	__m128i shear_shuf = rt->sse_shear_shuf;

	// Setup SOA ray data
	__m128 min_t = _mm_set1_ps(rt->ray.min_t);
	__m128 max_t = _mm_set1_ps(rt->hit.t);

	__m128 shear_origin_x = _mm_set1_ps(rt->shear_origin.x);
	__m128 shear_origin_y = _mm_set1_ps(rt->shear_origin.y);
	__m128 shear_origin_z = _mm_set1_ps(rt->shear_origin.z);

	__m128 shear_x = _mm_set1_ps(rt->shear.x);
	__m128 shear_y = _mm_set1_ps(rt->shear.y);
	__m128 shear_z = _mm_set1_ps(rt->shear.z);

	const leaf_tri *tris_end = tris + num_tris_aligned;
	for (; tris != tris_end; tris += 4) {
		// Load AOS triangle data (XYZ + vertex index)

		__m128i ai0 = _mm_load_si128((const __m128i*)(verts + tris[0].v[0]));
		__m128i bi0 = _mm_load_si128((const __m128i*)(verts + tris[1].v[0]));
		__m128i ci0 = _mm_load_si128((const __m128i*)(verts + tris[2].v[0]));
		__m128i di0 = _mm_load_si128((const __m128i*)(verts + tris[3].v[0]));

		__m128i ai1 = _mm_load_si128((const __m128i*)(verts + tris[0].v[1]));
		__m128i bi1 = _mm_load_si128((const __m128i*)(verts + tris[1].v[1]));
		__m128i ci1 = _mm_load_si128((const __m128i*)(verts + tris[2].v[1]));
		__m128i di1 = _mm_load_si128((const __m128i*)(verts + tris[3].v[1]));

		__m128i ai2 = _mm_load_si128((const __m128i*)(verts + tris[0].v[2]));
		__m128i bi2 = _mm_load_si128((const __m128i*)(verts + tris[1].v[2]));
		__m128i ci2 = _mm_load_si128((const __m128i*)(verts + tris[2].v[2]));
		__m128i di2 = _mm_load_si128((const __m128i*)(verts + tris[3].v[2]));

		// Permute coordinates to shear space

		__m128 a0 = _mm_castsi128_ps(_mm_shuffle_epi8(ai0, shear_shuf));
		__m128 b0 = _mm_castsi128_ps(_mm_shuffle_epi8(bi0, shear_shuf));
		__m128 c0 = _mm_castsi128_ps(_mm_shuffle_epi8(ci0, shear_shuf));
		__m128 d0 = _mm_castsi128_ps(_mm_shuffle_epi8(di0, shear_shuf));
		__m128 a1 = _mm_castsi128_ps(_mm_shuffle_epi8(ai1, shear_shuf));
		__m128 b1 = _mm_castsi128_ps(_mm_shuffle_epi8(bi1, shear_shuf));
		__m128 c1 = _mm_castsi128_ps(_mm_shuffle_epi8(ci1, shear_shuf));
		__m128 d1 = _mm_castsi128_ps(_mm_shuffle_epi8(di1, shear_shuf));
		__m128 a2 = _mm_castsi128_ps(_mm_shuffle_epi8(ai2, shear_shuf));
		__m128 b2 = _mm_castsi128_ps(_mm_shuffle_epi8(bi2, shear_shuf));
		__m128 c2 = _mm_castsi128_ps(_mm_shuffle_epi8(ci2, shear_shuf));
		__m128 d2 = _mm_castsi128_ps(_mm_shuffle_epi8(di2, shear_shuf));

#if 0
		const leaf_tri *pre = tris + 4 != tris_end ? tris + 4 : tris;
		rtk_prefetch(verts + pre[0].v[0]);
		rtk_prefetch(verts + pre[1].v[0]);
		rtk_prefetch(verts + pre[2].v[0]);
		rtk_prefetch(verts + pre[3].v[0]);
		rtk_prefetch(verts + pre[0].v[1]);
		rtk_prefetch(verts + pre[1].v[1]);
		rtk_prefetch(verts + pre[2].v[1]);
		rtk_prefetch(verts + pre[3].v[1]);
		rtk_prefetch(verts + pre[0].v[2]);
		rtk_prefetch(verts + pre[1].v[2]);
		rtk_prefetch(verts + pre[2].v[2]);
		rtk_prefetch(verts + pre[3].v[2]);
#endif

		// Tranpose to SOA
		__m128 t0, t1, vx, vy, vz;

		t0 = _mm_unpacklo_ps(a0, b0); // XX YY
		t1 = _mm_unpacklo_ps(c0, d0); // XX YY
		vx = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(1,0,1,0));
		vy = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(3,2,3,2));
		t0 = _mm_unpackhi_ps(a0, b0); // ZZ II
		t1 = _mm_unpackhi_ps(c0, d0); // ZZ II
		vz = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(1,0,1,0));
		__m128 v0x = _mm_sub_ps(vx, shear_origin_x);
		__m128 v0y = _mm_sub_ps(vy, shear_origin_y);
		__m128 v0z = _mm_sub_ps(vz, shear_origin_z);

		t0 = _mm_unpacklo_ps(a1, b1); // XX YY
		t1 = _mm_unpacklo_ps(c1, d1); // XX YY
		vx = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(1,0,1,0));
		vy = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(3,2,3,2));
		t0 = _mm_unpackhi_ps(a1, b1); // ZZ II
		t1 = _mm_unpackhi_ps(c1, d1); // ZZ II
		vz = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(1,0,1,0));
		__m128 v1x = _mm_sub_ps(vx, shear_origin_x);
		__m128 v1y = _mm_sub_ps(vy, shear_origin_y);
		__m128 v1z = _mm_sub_ps(vz, shear_origin_z);

		t0 = _mm_unpacklo_ps(a2, b2); // XX YY
		t1 = _mm_unpacklo_ps(c2, d2); // XX YY
		vx = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(1,0,1,0));
		vy = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(3,2,3,2));
		t0 = _mm_unpackhi_ps(a2, b2); // ZZ II
		t1 = _mm_unpackhi_ps(c2, d2); // ZZ II
		vz = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(1,0,1,0));
		__m128 v2x = _mm_sub_ps(vx, shear_origin_x);
		__m128 v2y = _mm_sub_ps(vy, shear_origin_y);
		__m128 v2z = _mm_sub_ps(vz, shear_origin_z);

		// Shear to ray space

		__m128 x0 = _mm_add_ps(v0x, _mm_mul_ps(shear_x, v0z));
		__m128 y0 = _mm_add_ps(v0y, _mm_mul_ps(shear_y, v0z));
		__m128 z0 = _mm_mul_ps(shear_z, v0z);
		__m128 x1 = _mm_add_ps(v1x, _mm_mul_ps(shear_x, v1z));
		__m128 y1 = _mm_add_ps(v1y, _mm_mul_ps(shear_y, v1z));
		__m128 z1 = _mm_mul_ps(shear_z, v1z);
		__m128 x2 = _mm_add_ps(v2x, _mm_mul_ps(shear_x, v2z));
		__m128 y2 = _mm_add_ps(v2y, _mm_mul_ps(shear_y, v2z));
		__m128 z2 = _mm_mul_ps(shear_z, v2z);

		// Edge functions

		__m128 u = _mm_sub_ps(_mm_mul_ps(x1, y2), _mm_mul_ps(y1, x2));
		__m128 v = _mm_sub_ps(_mm_mul_ps(x2, y0), _mm_mul_ps(y2, x0));
		__m128 w = _mm_sub_ps(_mm_mul_ps(x0, y1), _mm_mul_ps(y0, x1));

		__m128 zero = _mm_setzero_ps();
		__m128 zero_u = _mm_cmpeq_ps(u, zero);
		__m128 zero_v = _mm_cmpeq_ps(v, zero);
		__m128 zero_w = _mm_cmpeq_ps(w, zero);
		__m128 any_zero = _mm_or_ps(_mm_or_ps(zero_u, zero_v), zero_w);

		// Recalculate edge functions in double precision if necessary
		if (_mm_movemask_ps(any_zero) != 0) {
			__m128d xd0 = _mm_cvtps_pd(x0);
			__m128d yd0 = _mm_cvtps_pd(y0);
			__m128d xd1 = _mm_cvtps_pd(x1);
			__m128d yd1 = _mm_cvtps_pd(y1);
			__m128d xd2 = _mm_cvtps_pd(x2);
			__m128d yd2 = _mm_cvtps_pd(y2);

			__m128d ud = _mm_sub_pd(_mm_mul_pd(xd1, yd2), _mm_mul_pd(yd1, xd2));
			__m128d vd = _mm_sub_pd(_mm_mul_pd(xd2, yd0), _mm_mul_pd(yd2, xd0));
			__m128d wd = _mm_sub_pd(_mm_mul_pd(xd0, yd1), _mm_mul_pd(yd0, xd1));

			u = _mm_cvtpd_ps(ud);
			v = _mm_cvtpd_ps(vd);
			w = _mm_cvtpd_ps(wd);

			xd0 = _mm_cvtps_pd(_mm_movehl_ps(x0, x0));
			yd0 = _mm_cvtps_pd(_mm_movehl_ps(y0, y0));
			xd1 = _mm_cvtps_pd(_mm_movehl_ps(x1, x1));
			yd1 = _mm_cvtps_pd(_mm_movehl_ps(y1, y1));
			xd2 = _mm_cvtps_pd(_mm_movehl_ps(x2, x2));
			yd2 = _mm_cvtps_pd(_mm_movehl_ps(y2, y2));

			ud = _mm_sub_pd(_mm_mul_pd(xd1, yd2), _mm_mul_pd(yd1, xd2));
			vd = _mm_sub_pd(_mm_mul_pd(xd2, yd0), _mm_mul_pd(yd2, xd0));
			wd = _mm_sub_pd(_mm_mul_pd(xd0, yd1), _mm_mul_pd(yd0, xd1));

			u = _mm_movelh_ps(u, _mm_cvtpd_ps(ud));
			v = _mm_movelh_ps(v, _mm_cvtpd_ps(vd));
			w = _mm_movelh_ps(w, _mm_cvtpd_ps(wd));
		}

		__m128 neg = _mm_cmplt_ps(_mm_min_ps(_mm_min_ps(u, v), w), zero);
		__m128 pos = _mm_cmpgt_ps(_mm_max_ps(_mm_max_ps(u, v), w), zero);
		__m128 bad_sign = _mm_and_ps(neg, pos);
		int bad_sign_mask = _mm_movemask_ps(bad_sign);
		if (bad_sign_mask == 0xf) continue;

		__m128 det = _mm_add_ps(_mm_add_ps(u, v), w);
		__m128 rcp_det = SSE_RCP(det);

		__m128 z = _mm_mul_ps(u, z0);
		z = _mm_add_ps(z, _mm_mul_ps(v, z1));
		z = _mm_add_ps(z, _mm_mul_ps(w, z2));

		__m128 t = _mm_mul_ps(z, rcp_det);
		__m128 good = _mm_and_ps(_mm_cmpgt_ps(t, min_t), _mm_cmplt_ps(t, max_t));

		// Scalar loop over hit triangles

		int good_mask = _mm_movemask_ps(good);
		good_mask &= ~bad_sign_mask;
		if (good_mask != 0) {
			SSE_ALIGN float ts[4], us[4], vs[4];

			_mm_store_ps(ts, t);
			_mm_store_ps(us, _mm_mul_ps(u, rcp_det));
			_mm_store_ps(vs, _mm_mul_ps(v, rcp_det));

			do {
				uint32_t lane_ix;
				SSE_CTZ(lane_ix, good_mask);

				float lane_t = ts[lane_ix];
				if (lane_t < rt->hit.t) {
					const leaf_vert *pv0 = &verts[tris[lane_ix].v[0]];
					const leaf_vert *pv1 = &verts[tris[lane_ix].v[1]];
					const leaf_vert *pv2 = &verts[tris[lane_ix].v[2]];
					const leaf_tri_obj *obj = &tri_objs[tris[lane_ix].obj_ix];

					rtk_vec3 e0 = v_sub(pv1->pos, pv0->pos);
					rtk_vec3 e1 = v_sub(pv2->pos, pv0->pos);

					rt->mesh_ix = obj->mesh_ix;
					rt->hit.t = lane_t;
					rt->hit.geom.u = us[lane_ix];
					rt->hit.geom.v = vs[lane_ix];
					rt->hit.geom.normal = v_cross(e0, e1);
					rt->hit.geom.dp_du = e0;
					rt->hit.geom.dp_dv = e1;
					rt->hit.user = NULL;
					rt->hit.vertex_index[0] = pv0->index;
					rt->hit.vertex_index[1] = pv1->index;
					rt->hit.vertex_index[2] = pv2->index;
					rt->hit.vertex_pos[0] = pv0->pos;
					rt->hit.vertex_pos[1] = pv1->pos;
					rt->hit.vertex_pos[2] = pv2->pos;
					rt->hit.num_parents = 0;
					rt->hit.geometry_type = rtk_hit_triangle;
					max_t = _mm_set1_ps(lane_t);
				}

				good_mask &= good_mask - 1;
			} while (good_mask != 0);
		}
	}

#else

	int shear_x = rt->shear_axis[0];
	int shear_y = rt->shear_axis[1];
	int shear_z = rt->shear_axis[2];

	rtk_vec3 shear_origin = rt->shear_origin;
	const leaf_tri *tris_end = tris + leaf->num_tris;
	for (; tris != tris_end; tris++) {
		const leaf_vert *pv0 = &verts[tris->v[0]];
		const leaf_vert *pv1 = &verts[tris->v[1]];
		const leaf_vert *pv2 = &verts[tris->v[2]];

		// Permute coordinates to shear space

		rtk_vec3 v0, v1, v2;
		v0.x = pv0->pos.v[shear_x] - shear_origin.x;
		v0.y = pv0->pos.v[shear_y] - shear_origin.y;
		v0.z = pv0->pos.v[shear_z] - shear_origin.z;
		v1.x = pv1->pos.v[shear_x] - shear_origin.x;
		v1.y = pv1->pos.v[shear_y] - shear_origin.y;
		v1.z = pv1->pos.v[shear_z] - shear_origin.z;
		v2.x = pv2->pos.v[shear_x] - shear_origin.x;
		v2.y = pv2->pos.v[shear_y] - shear_origin.y;
		v2.z = pv2->pos.v[shear_z] - shear_origin.z;

		// Shear to ray space

		rtk_vec3 shear = rt->shear;	
		rtk_real x0 = v0.x + shear.x * v0.z;
		rtk_real y0 = v0.y + shear.y * v0.z;
		rtk_real z0 = shear.z * v0.z;
		rtk_real x1 = v1.x + shear.x * v1.z;
		rtk_real y1 = v1.y + shear.y * v1.z;
		rtk_real z1 = shear.z * v1.z;
		rtk_real x2 = v2.x + shear.x * v2.z;
		rtk_real y2 = v2.y + shear.y * v2.z;
		rtk_real z2 = shear.z * v2.z;

		rtk_real u = x1*y2 - y1*x2;
		rtk_real v = x2*y0 - y2*x0;
		rtk_real w = x0*y1 - y0*x1;

		// Recalculate edge functions in double precision if necessary
		// TODO: RTK_DOUBLE define

		if ((u == 0.0) | (v == 0.0) | (w == 0.0)) {
			u = (rtk_real)((double)x1*(double)y2 - (double)y1*(double)x2);
			v = (rtk_real)((double)x2*(double)y0 - (double)y2*(double)x0);
			w = (rtk_real)((double)x0*(double)y1 - (double)y0*(double)x1);
		}

		if (((u<0.0) | (v<0.0) | (w<0.0)) & (((u>0.0) | (v>0.0) | (w>0.0)))) continue;
		rtk_real det = u + v + w;
		rtk_real rcp_det = 1.0f / det;

		rtk_real z = u*z0 + v*z1 + w*z2;
		rtk_real t = z * rcp_det;
		if ((t <= rt->ray.min_t) || (t >= rt->hit.t)) continue;

		rtk_vec3 e0 = v_sub(pv1->pos, pv0->pos);
		rtk_vec3 e1 = v_sub(pv2->pos, pv0->pos);

		rt->hit.t = t;
		rt->hit.u = u * rcp_det;
		rt->hit.v = v * rcp_det;
		rt->hit.normal = v_cross(e0, e1);
		rt->hit.dp_du = e0;
		rt->hit.dp_dv = e1;
		rt->hit.object = tri_objs[tris->obj_ix];
		rt->hit.user = NULL;
		rt->hit.vertex_index[0] = pv0->index;
		rt->hit.vertex_index[1] = pv1->index;
		rt->hit.vertex_index[2] = pv2->index;
		rt->hit.vertex_pos[0] = pv0->pos;
		rt->hit.vertex_pos[1] = pv1->pos;
		rt->hit.vertex_pos[2] = pv2->pos;
		rt->hit.num_parents = 0;
		rt->hit.geom = rtk_hit_triangle;
	}

#endif

	for (; prims != prims_end; prims++) {
		rtk_ray new_ray;
		new_ray.origin = mat_mul_pos(&prims->inv_mat, rt->ray.origin);
		new_ray.direction = mat_mul_dir(&prims->inv_mat, rt->ray.direction);
		new_ray.min_t = rt->ray.min_t;

		rtk_real t = intersect_aabb_ray(&prims->prim.bounds, &new_ray);
		if (t < rt->hit.t) {
			if (prims->prim.intersect_fn(&prims->prim, &new_ray, &rt->hit)) {
				// TODO: Cache this?
				if (!mat_is_identity(&prims->prim.transform)) {
					rt->hit.geom.normal = mat_mul_dir_transpose(&prims->inv_mat, rt->hit.geom.normal);
					rt->hit.interp.normal = mat_mul_dir_transpose(&prims->inv_mat, rt->hit.interp.normal);
					rt->hit.geom.dp_du = mat_mul_dir(&prims->prim.transform, rt->hit.geom.dp_du);
					rt->hit.geom.dp_dv = mat_mul_dir(&prims->prim.transform, rt->hit.geom.dp_dv);
					rt->hit.interp.dp_du = mat_mul_dir(&prims->prim.transform, rt->hit.interp.dp_du);
					rt->hit.interp.dp_dv = mat_mul_dir(&prims->prim.transform, rt->hit.interp.dp_dv);
				}
				rt->mesh_ix = ~0u;
			}
		}
	}
}

#if 0
static void bvh_traverse(raytrace *rt, const bvh_node *node)
{
	bvh_frame stack[BVH_MAX_DEPTH];
	bvh_frame *top = stack;

#if RTK_SSE
	const __m128 origin = rt->sse_origin;
	const __m128 rcp_dir = rt->sse_rcp_dir;
#else
	const rtk_vec3 origin = rt->ray.origin;
	const rtk_vec3 rcp_dir = rt->rcp_dir;
#endif

	for (;;) {
		// Prefetch both child pointers, no need to remove tag bit
		// since prefetch works at a cache-line granularity.
		// TODO: Profile this
		rtk_prefetch((const void*)node->ptr[0]);
		rtk_prefetch((const void*)node->ptr[1]);

		// Intersect the ray with both children
#if RTK_SSE

		// L.min.x, L.max.x, R.min.X, R.max.x
		__m128 bx = _mm_load_ps(node->bounds[0]);
		__m128 by = _mm_load_ps(node->bounds[1]);
		__m128 bz = _mm_load_ps(node->bounds[2]);

		// L.min.x - O.x, L.max.x - O.x, R.min.x - O.x, R.max.x - O.x
		__m128 dx = _mm_sub_ps(bx, _mm_shuffle_ps(origin, origin, _MM_SHUFFLE(0,0,0,0)));
		__m128 dy = _mm_sub_ps(by, _mm_shuffle_ps(origin, origin, _MM_SHUFFLE(1,1,1,1)));
		__m128 dz = _mm_sub_ps(bz, _mm_shuffle_ps(origin, origin, _MM_SHUFFLE(2,2,2,2)));

		// l_tx0, l_tx1, r_tx0, r_tx1
		__m128 tx = _mm_mul_ps(dx, _mm_shuffle_ps(rcp_dir, rcp_dir, _MM_SHUFFLE(0,0,0,0)));
		__m128 ty = _mm_mul_ps(dy, _mm_shuffle_ps(rcp_dir, rcp_dir, _MM_SHUFFLE(1,1,1,1)));
		__m128 tz = _mm_mul_ps(dz, _mm_shuffle_ps(rcp_dir, rcp_dir, _MM_SHUFFLE(2,2,2,2)));

		// l_tx_min, ?, r_tx_min, ?
		// l_tx_max, ?, r_tx_max, ?
		__m128 temp;
		temp = _mm_shuffle_ps(tx, tx, _MM_SHUFFLE(2,3,0,1));
		__m128 min_tx = _mm_min_ps(tx, temp);
		__m128 max_tx = _mm_max_ps(tx, temp);
		temp = _mm_shuffle_ps(ty, ty, _MM_SHUFFLE(2,3,0,1));
		__m128 min_ty = _mm_min_ps(ty, temp);
		__m128 max_ty = _mm_max_ps(ty, temp);
		temp = _mm_shuffle_ps(tz, tz, _MM_SHUFFLE(2,3,0,1));
		__m128 min_tz = _mm_min_ps(tz, temp);
		__m128 max_tz = _mm_max_ps(tz, temp);

		// l_t_min, ?, r_t_min, ?
		// l_t_max, ?, r_t_max, ?
		__m128 min_t = _mm_max_ps(_mm_max_ps(min_tx, min_ty), min_tz);
		__m128 max_t = _mm_min_ps(_mm_min_ps(max_tx, max_ty), max_tz);

		__m128 mask = _mm_and_ps(_mm_cmple_ps(min_t, max_t), _mm_cmpge_ps(max_t, _mm_set1_ps(rt->ray.min_t)));
		__m128 result = _mm_or_ps(_mm_and_ps(mask, min_t), _mm_andnot_ps(mask, _mm_set1_ps(RTK_RAY_INF)));
		temp = _mm_movehl_ps(result, result);
		__m128 near_t = _mm_min_ps(result, temp);
		__m128 far_t = _mm_max_ps(result, temp);

		// Output
		int ix_near = _mm_ucomineq_ss(near_t, result);
		rtk_real t_near = _mm_cvtss_f32(near_t);
		rtk_real t_far = _mm_cvtss_f32(far_t);

#else
		rtk_real l_t0 = (node->bounds[0][0] - origin.x) * rcp_dir.x;
		rtk_real l_t1 = (node->bounds[0][1] - origin.x) * rcp_dir.x;
		rtk_real r_t0 = (node->bounds[0][2] - origin.x) * rcp_dir.x;
		rtk_real r_t1 = (node->bounds[0][3] - origin.x) * rcp_dir.x;
		rtk_real l_min_t = real_min(l_t0, l_t1);
		rtk_real l_max_t = real_max(l_t0, l_t1);
		rtk_real r_min_t = real_min(r_t0, r_t1);
		rtk_real r_max_t = real_max(r_t0, r_t1);

		l_t0 = (node->bounds[1][0] - origin.y) * rcp_dir.y;
		l_t1 = (node->bounds[1][1] - origin.y) * rcp_dir.y;
		r_t0 = (node->bounds[1][2] - origin.y) * rcp_dir.y;
		r_t1 = (node->bounds[1][3] - origin.y) * rcp_dir.y;
		l_min_t = real_max(l_min_t, real_min(l_t0, l_t1));
		l_max_t = real_min(l_max_t, real_max(l_t0, l_t1));
		r_min_t = real_max(r_min_t, real_min(r_t0, r_t1));
		r_max_t = real_min(r_max_t, real_max(r_t0, r_t1));

		l_t0 = (node->bounds[2][0] - origin.z) * rcp_dir.z;
		l_t1 = (node->bounds[2][1] - origin.z) * rcp_dir.z;
		r_t0 = (node->bounds[2][2] - origin.z) * rcp_dir.z;
		r_t1 = (node->bounds[2][3] - origin.z) * rcp_dir.z;
		l_min_t = real_max(l_min_t, real_min(l_t0, l_t1));
		l_max_t = real_min(l_max_t, real_max(l_t0, l_t1));
		r_min_t = real_max(r_min_t, real_min(r_t0, r_t1));
		r_max_t = real_min(r_max_t, real_max(r_t0, r_t1));

		rtk_real l_t = l_min_t <= l_max_t && l_max_t >= rt->ray.min_t ? l_min_t : RTK_RAY_INF;
		rtk_real r_t = r_min_t <= r_max_t && r_max_t >= rt->ray.min_t ? r_min_t : RTK_RAY_INF;

		// Output
		rtk_real t_near = real_min(l_t, r_t);
		rtk_real t_far = real_max(l_t, r_t);
		int ix_near = l_t != t_near;
#endif

		// Sort the intersection distances
		int ix_far = ix_near ^ 1;
		if (t_near < rt->hit.t) {
			bvh_ptr ptr = node->ptr[ix_near];
			if (bp_is_node(ptr)) {
				// Near child is a node, push far child to stack and traverse in
				rtk_assert(top - stack < BVH_MAX_DEPTH);
				top->t = t_far;
				top->ptr = node->ptr[ix_far];
				top += (t_far < rt->hit.t ? 1 : 0);
				node = bp_node(ptr);
				continue;
			} else {
				// Near child is a leaf, traverse it and continue with the far
				// child immediately afterwards
				bvh_leaf_traverse(rt, bp_leaf(ptr));
				if (t_far < rt->hit.t) {
					ptr = node->ptr[ix_far];
					if (bp_is_node(ptr)) {
						node = bp_node(ptr);
						continue;
					} else {
						bvh_leaf_traverse(rt, bp_leaf(ptr));
					}
				}
			}
		}

		// Pop nodes from stack and traverse to them
		for (;;) {
			if (top == stack) return;

			--top;
			if (top->t < rt->hit.t) {
				bvh_ptr ptr = top->ptr;
				if (bp_is_node(ptr)) {
					node = bp_node(ptr);
					break;
				} else {
					bvh_leaf_traverse(rt, bp_leaf(ptr));
				}
			}
		}
	}
}
#else

static void bvh_traverse(raytrace *rt, const bvh_node *root_node)
{
	float t_stack[BVH_MAX_DEPTH + 1];
	bvh_ptr node_stack[BVH_MAX_DEPTH + 1];
	uint32_t depth = 1;
	t_stack[0] = RTK_INF;
	node_stack[0] = 0;
	t_stack[1] = RTK_INF;
	node_stack[1] = 0;

	float top_t = -RTK_INF;
	bvh_ptr top_node = bp_make_node(root_node);

	const __m128 pos_inf = _mm_set1_ps(3.402823e+38f);
	const __m128 ray_min_t = _mm_load1_ps(&rt->ray.min_t);
	const __m128 ray_origin = rt->sse_origin;
	const __m128 ray_rcp_dir = rt->sse_rcp_dir;

	const __m128 origin_x = MM_BROADCAST(ray_origin, 0);
	const __m128 origin_y = MM_BROADCAST(ray_origin, 1);
	const __m128 origin_z = MM_BROADCAST(ray_origin, 2);
	const __m128 rcp_dir_x = MM_BROADCAST(ray_rcp_dir, 0);
	const __m128 rcp_dir_y = MM_BROADCAST(ray_rcp_dir, 1);
	const __m128 rcp_dir_z = MM_BROADCAST(ray_rcp_dir, 2);

	const __m128 tag_mask = _mm_castsi128_ps(_mm_set1_epi32(0x3));
	const __m128 tag = _mm_castsi128_ps(_mm_setr_epi32(0, 1, 2, 3));

	for (;;) {
		float hit_t = rt->hit.t;
		__m128 ray_max_t = _mm_set1_ps(hit_t);

		bvh_ptr next_node = node_stack[depth];
		float next_t = t_stack[depth];
		while (top_t >= hit_t) {
			if (depth == 0) return;
			depth--;
			top_t = next_t;
			top_node = next_node;
			next_t = t_stack[depth];
			next_node = node_stack[depth];
		}

		if (!bp_is_node(top_node)) {
			bvh_leaf_traverse(rt, bp_leaf(top_node));
			depth--;
			top_node = next_node;
			top_t = next_t;
			continue;
		}

		bvh_node *node = bp_node(top_node);

		const uint32_t sign_mask = rt->sign_mask;
		const uint32_t sign_x = sign_mask & 1;
		const uint32_t sign_y = sign_mask >> 1 & 1;
		const uint32_t sign_z = sign_mask >> 2;
		__m128 lo_x = _mm_load_ps(node->bounds_x[sign_x]);
		__m128 hi_x = _mm_load_ps(node->bounds_x[sign_x ^ 1]);
		__m128 lo_y = _mm_load_ps(node->bounds_y[sign_y]);
		__m128 hi_y = _mm_load_ps(node->bounds_y[sign_y ^ 1]);
		__m128 lo_z = _mm_load_ps(node->bounds_z[sign_z]);
		__m128 hi_z = _mm_load_ps(node->bounds_z[sign_z ^ 1]);

		__m128 min_x = _mm_mul_ps(_mm_sub_ps(lo_x, origin_x), rcp_dir_x);
		__m128 max_x = _mm_mul_ps(_mm_sub_ps(hi_x, origin_x), rcp_dir_x);
		__m128 min_y = _mm_mul_ps(_mm_sub_ps(lo_y, origin_y), rcp_dir_y);
		__m128 max_y = _mm_mul_ps(_mm_sub_ps(hi_y, origin_y), rcp_dir_y);
		__m128 min_z = _mm_mul_ps(_mm_sub_ps(lo_z, origin_z), rcp_dir_z);
		__m128 max_z = _mm_mul_ps(_mm_sub_ps(hi_z, origin_z), rcp_dir_z);

		__m128 min_t = _mm_max_ps(_mm_max_ps(min_x, min_y), _mm_max_ps(min_z, ray_min_t));
		__m128 max_t = _mm_min_ps(_mm_min_ps(max_x, max_y), _mm_min_ps(max_z, ray_max_t));

		__m128 mask = _mm_cmple_ps(min_t, max_t);
		uint32_t mask_bits = _mm_movemask_ps(mask);
		bit4_info mask_info = bit4_tab[mask_bits];
		uint32_t num = b4_bitcount(mask_info);

		if (num == 0) {
			depth--;
			top_node = next_node;
			top_t = next_t;
		} else if (num == 1) {
			top_node = node->ptr[b4_bit(mask_info, 0)];
			rtk_prefetch((char*)top_node);
			rtk_prefetch((char*)top_node + 64);

			__m128 ts = _mm_or_ps(_mm_and_ps(mask, min_t), _mm_andnot_ps(mask, pos_inf));
			__m128 near_t = _mm_min_ps(ts, _mm_shuffle_ps(ts, ts, _MM_SHUFFLE(2,3,0,1)));
			near_t = _mm_min_ps(near_t, _mm_movehl_ps(near_t, near_t));
			top_t = _mm_cvtss_f32(near_t);
		} else {
			__m128 ts = _mm_or_ps(_mm_and_ps(mask, min_t), _mm_andnot_ps(mask, pos_inf));
			__m128 t_tag = _mm_or_ps(_mm_andnot_ps(tag_mask, ts), tag);

			// t_tag[0] --v--- t_lo[01] - t_1[0] --v--- t_lo[01] - t_2[0] ----- t_lo/hi[0] - t_tag_sorted[0]
			// t_tag[1] --^--- t_hi[01] - t_1[2] --|v-- t_lo[23] - t_2[1] --v--  t_lo[12]  - t_tag_sorted[1]
			// t_tag[2] ---v-- t_lo[23] - t_1[1] --^|-- t_hi[01] - t_2[2] --^--  t_hi[12]  - t_tag_sorted[2]
			// t_tag[3] ---^-- t_hi[23] - t_1[3] ---^-- t_hi[23] - t_2[3] ----- t_lo/hi[3] - t_tag_sorted[3]

			__m128 t_swap = _mm_shuffle_ps(t_tag, t_tag, _MM_SHUFFLE(2,3,0,1));
			__m128 t_lo = _mm_min_ps(t_tag, t_swap);
			__m128 t_hi = _mm_max_ps(t_tag, t_swap);
			__m128 t_1 = _mm_shuffle_ps(t_lo, t_hi, _MM_SHUFFLE(2,0,2,0));

			t_swap = _mm_shuffle_ps(t_1, t_1, _MM_SHUFFLE(2,3,0,1));
			t_lo = _mm_min_ps(t_1, t_swap);
			t_hi = _mm_max_ps(t_1, t_swap);
			__m128 t_2 = _mm_shuffle_ps(t_lo, t_hi, _MM_SHUFFLE(2,0,2,0));

			t_swap = _mm_shuffle_ps(t_2, t_2, _MM_SHUFFLE(3,1,2,0));
			t_lo = _mm_min_ps(t_2, t_swap);
			t_hi = _mm_max_ps(t_2, t_swap);

			__m128 t_tag_sorted = _mm_shuffle_ps(t_lo, t_hi, _MM_SHUFFLE(3,2,1,0));

			__m128 t_sorted = _mm_andnot_ps(tag_mask, t_tag_sorted);
			__m128 tag_sorted = _mm_and_ps(tag_mask, t_tag_sorted);

			depth += num - 1;

			float *t_top = t_stack + depth;
			uintptr_t *node_top = node_stack + depth;
			top_t = _mm_cvtss_f32(MM_BROADCAST(t_sorted, 0));
			top_node = node->ptr[_mm_cvtsi128_si32(_mm_castps_si128(MM_BROADCAST(tag_sorted, 0)))];

			rtk_prefetch((char*)top_node);
			rtk_prefetch((char*)top_node + 64);

			t_top[0] = _mm_cvtss_f32(MM_BROADCAST(t_sorted, 1));
			node_top[0] = node->ptr[_mm_cvtsi128_si32(_mm_castps_si128(MM_BROADCAST(tag_sorted, 1)))];
			if (num >= 3) {
				t_top[-1] = _mm_cvtss_f32(MM_BROADCAST(t_sorted, 2));
				node_top[-1] = node->ptr[_mm_cvtsi128_si32(_mm_castps_si128(MM_BROADCAST(tag_sorted, 2)))];
				if (num == 4) {
					t_top[-2] = _mm_cvtss_f32(MM_BROADCAST(t_sorted, 3));
					node_top[-2] = node->ptr[_mm_cvtsi128_si32(_mm_castps_si128(MM_BROADCAST(tag_sorted, 3)))];
				}
			}
		}
	}
}

static void bvh_traverse_many(raytrace *rts, size_t num, const bvh_node *root_node)
{
#if 0
	uint32_t queue[MAX_CONCURRENT_RAYS];
	uint32_t queue_begin = 0;
	uint32_t queue_end = 0;

	rtk_for(raytrace, rt, rts, num) {
		rt->depth = 2;
		rt->t_stack[0] = -RTK_INF;
		rt->node_stack[0] = bp_make_node(root_node);
		rt->t_stack[1] = -RTK_INF;
		rt->node_stack[1] = bp_make_node(root_node);
		queue[queue_end] = queue_end;
		queue_end++;
	}

	const __m128 neg_inf = _mm_set1_ps(-3.402823e+38f);
	const __m128 tag_mask = _mm_castsi128_ps(_mm_set1_epi32(0x3));
	const __m128 tag = _mm_castsi128_ps(_mm_setr_epi32(0, 1, 2, 3));

	while (queue_begin != queue_end) {
		uint32_t ix = queue[queue_begin & (MAX_CONCURRENT_RAYS - 1)];
		raytrace *rt = &rts[ix];

		queue_begin++;

		const __m128 ray_min_t = _mm_load1_ps(&rt->ray.min_t);
		const __m128 ray_origin = rt->sse_origin;
		const __m128 ray_rcp_dir = rt->sse_rcp_dir;

		const __m128 origin_x = MM_BROADCAST(ray_origin, 0);
		const __m128 origin_y = MM_BROADCAST(ray_origin, 1);
		const __m128 origin_z = MM_BROADCAST(ray_origin, 2);
		const __m128 rcp_dir_x = MM_BROADCAST(ray_rcp_dir, 0);
		const __m128 rcp_dir_y = MM_BROADCAST(ray_rcp_dir, 1);
		const __m128 rcp_dir_z = MM_BROADCAST(ray_rcp_dir, 2);

		uint32_t depth = rt->depth;
		depth--;
		float t = rt->t_stack[depth];
		while (t >= rt->hit.t) {
			depth--;
			t = rt->t_stack[depth];
		}
		if (depth == 0) {
			continue;
		}

		bvh_ptr ptr = rt->node_stack[depth];
		if (!bp_is_node(ptr)) {
			bvh_leaf_traverse(rt, bp_leaf(ptr));
		} else {
			bvh_node *node = bp_node(ptr);

			const uint32_t sign_mask = rt->sign_mask;
			const uint32_t sign_x = sign_mask & 1;
			const uint32_t sign_y = sign_mask >> 1 & 1;
			const uint32_t sign_z = sign_mask >> 2;
			__m128 lo_x = _mm_load_ps(node->bounds_x[sign_x]);
			__m128 hi_x = _mm_load_ps(node->bounds_x[sign_x ^ 1]);
			__m128 lo_y = _mm_load_ps(node->bounds_y[sign_y]);
			__m128 hi_y = _mm_load_ps(node->bounds_y[sign_y ^ 1]);
			__m128 lo_z = _mm_load_ps(node->bounds_z[sign_z]);
			__m128 hi_z = _mm_load_ps(node->bounds_z[sign_z ^ 1]);

			__m128 ray_max_t = _mm_load1_ps(&rt->hit.t);

			__m128 lo_t, hi_t, min_t, max_t;
			lo_t = _mm_sub_ps(lo_x, origin_x);
			hi_t = _mm_sub_ps(hi_x, origin_x);
			min_t = _mm_mul_ps(lo_t, rcp_dir_x);
			max_t = _mm_mul_ps(hi_t, rcp_dir_x);

			lo_t = _mm_sub_ps(lo_y, origin_y);
			hi_t = _mm_sub_ps(hi_y, origin_y);
			min_t = _mm_max_ps(min_t, _mm_mul_ps(lo_t, rcp_dir_y));
			max_t = _mm_min_ps(max_t, _mm_mul_ps(hi_t, rcp_dir_y));

			lo_t = _mm_sub_ps(lo_z, origin_z);
			hi_t = _mm_sub_ps(hi_z, origin_z);
			min_t = _mm_max_ps(min_t, _mm_mul_ps(lo_t, rcp_dir_z));
			max_t = _mm_min_ps(max_t, _mm_mul_ps(hi_t, rcp_dir_z));

			__m128 mask = _mm_and_ps(_mm_cmple_ps(min_t, max_t), _mm_cmpge_ps(max_t, ray_min_t));
			mask = _mm_and_ps(mask, _mm_cmplt_ps(min_t, ray_max_t));
			uint32_t mask_bits = _mm_movemask_ps(mask);
			uint32_t num = bitcount4[mask_bits];

			__m128 ts = _mm_or_ps(_mm_and_ps(mask, min_t), _mm_andnot_ps(mask, neg_inf));
			__m128 t_tag = _mm_or_ps(_mm_andnot_ps(tag_mask, ts), tag);

			// t_tag[0] --v--- t_lo[01] - t_1[0] --v--- t_lo[01] - t_2[0] ----- t_lo/hi[0] - t_tag_sorted[0]
			// t_tag[1] --^--- t_hi[01] - t_1[2] --|v-- t_lo[23] - t_2[1] --v--  t_lo[12]  - t_tag_sorted[1]
			// t_tag[2] ---v-- t_lo[23] - t_1[1] --^|-- t_hi[01] - t_2[2] --^--  t_hi[12]  - t_tag_sorted[2]
			// t_tag[3] ---^-- t_hi[23] - t_1[3] ---^-- t_hi[23] - t_2[3] ----- t_lo/hi[3] - t_tag_sorted[3]

			__m128 t_swap = _mm_shuffle_ps(t_tag, t_tag, _MM_SHUFFLE(2,3,0,1));
			__m128 t_lo = _mm_max_ps(t_tag, t_swap);
			__m128 t_hi = _mm_min_ps(t_tag, t_swap);
			__m128 t_1 = _mm_shuffle_ps(t_lo, t_hi, _MM_SHUFFLE(2,0,2,0));

			t_swap = _mm_shuffle_ps(t_1, t_1, _MM_SHUFFLE(2,3,0,1));
			t_lo = _mm_max_ps(t_1, t_swap);
			t_hi = _mm_min_ps(t_1, t_swap);
			__m128 t_2 = _mm_shuffle_ps(t_lo, t_hi, _MM_SHUFFLE(2,0,2,0));

			t_swap = _mm_shuffle_ps(t_2, t_2, _MM_SHUFFLE(3,1,2,0));
			t_lo = _mm_max_ps(t_2, t_swap);
			t_hi = _mm_min_ps(t_2, t_swap);

			__m128 t_tag_sorted = _mm_shuffle_ps(t_lo, t_hi, _MM_SHUFFLE(3,2,1,0));

			__m128 t_sorted = _mm_andnot_ps(tag_mask, t_tag_sorted);
			float t0 = _mm_cvtss_f32(MM_BROADCAST(t_sorted, 0));
			float t1 = _mm_cvtss_f32(MM_BROADCAST(t_sorted, 1));
			float t2 = _mm_cvtss_f32(MM_BROADCAST(t_sorted, 2));
			float t3 = _mm_cvtss_f32(MM_BROADCAST(t_sorted, 3));
			float *t_top = rt->t_stack + depth;
			t_top[0] = t0;
			t_top[1] = t1;
			t_top[2] = t2;
			t_top[3] = t3;

			__m128 tag_sorted = _mm_and_ps(tag_mask, t_tag_sorted);
			uint32_t c0 = _mm_cvtsi128_si32(_mm_castps_si128(MM_BROADCAST(tag_sorted, 0)));
			uint32_t c1 = _mm_cvtsi128_si32(_mm_castps_si128(MM_BROADCAST(tag_sorted, 1)));
			uint32_t c2 = _mm_cvtsi128_si32(_mm_castps_si128(MM_BROADCAST(tag_sorted, 2)));
			uint32_t c3 = _mm_cvtsi128_si32(_mm_castps_si128(MM_BROADCAST(tag_sorted, 3)));

			uintptr_t *node_top = rt->node_stack + depth;
			node_top[0] = node->ptr[c0];
			node_top[1] = node->ptr[c1];
			node_top[2] = node->ptr[c2];
			node_top[3] = node->ptr[c3];

			depth += num;
		}

		char *next = (char*)rt->node_stack[depth - 1];
		rtk_prefetch(next);
		rtk_prefetch(next + 64);

		rt->depth = depth;

		queue[queue_end & (MAX_CONCURRENT_RAYS - 1)] = ix;
		queue_end++;
	}
#endif
}

#endif

// -- BVH building

typedef int (*sort_fn)(const void *a, const void *b);

static int item_sort_axis_x(const void *a, const void *b) {
	rtk_real mid_a = ((const build_item*)a)->bounds.min.x + ((const build_item*)a)->bounds.max.x;
	rtk_real mid_b = ((const build_item*)b)->bounds.min.x + ((const build_item*)b)->bounds.max.x;
	return mid_a < mid_b ? -1 : +1;
}

static int item_sort_axis_y(const void *a, const void *b) {
	rtk_real mid_a = ((const build_item*)a)->bounds.min.y + ((const build_item*)a)->bounds.max.y;
	rtk_real mid_b = ((const build_item*)b)->bounds.min.y + ((const build_item*)b)->bounds.max.y;
	return mid_a < mid_b ? -1 : +1;
}

static int item_sort_axis_z(const void *a, const void *b) {
	rtk_real mid_a = ((const build_item*)a)->bounds.min.z + ((const build_item*)a)->bounds.max.z;
	rtk_real mid_b = ((const build_item*)b)->bounds.min.z + ((const build_item*)b)->bounds.max.z;
	return mid_a < mid_b ? -1 : +1;
}

static sort_fn item_sort_axis_fn[3] = {
	&item_sort_axis_x,
	&item_sort_axis_y,
	&item_sort_axis_z,
};

static void build_node_rec(build *b, build_node *node, int depth);

static void build_node_leaf(build *b, build_node *node)
{
	rtk_assert(node->num <= BVH_LEAF_MAX_ITEMS);

	rtk_vec3 size = v_sub(node->bounds.max, node->bounds.min);
	rtk_real max_comp = real_max(real_max(size.x, size.y), size.z);
	int axis = size.x == max_comp ? 0 : size.y == max_comp ? 1 : 2;

	// Sort items by the largest axis
	build_item *items = b->items + node->begin;
	qsort(items, node->num, sizeof(build_item), item_sort_axis_fn[axis]);

	uint32_t unique_meshes[BVH_LEAF_MAX_ITEMS];
	uint32_t num_unique_meshes = 0;

	uint32_t num_tris = 0;
	rtk_for(build_item, item, b->items + node->begin, node->num) {
		uint32_t mesh_ix = item->mesh_ix;
		if (mesh_ix == MESH_IX_PRIM) continue;
		num_tris++;

		if (mesh_ix == MESH_IX_TRI) {
			// Every triangle has an unique mesh
			num_unique_meshes++;
		} else {
			// O(n^2) but n is bounded by `BVH_LEAF_MAX_ITEMS` and probably _very_ small
			uint32_t i;
			for (i = 0; i != num_unique_meshes; i++) {
				if (unique_meshes[i] == mesh_ix) break;
			}
			if (i == num_unique_meshes) {
				unique_meshes[i] = mesh_ix;
				num_unique_meshes++;
			}
		}
	}
	uint32_t num_prims = node->num - num_tris;
	uint32_t num_tris_aligned = align_up_u32(num_tris, 4);

	node->num_tris = num_tris;

	// Count used memory

	b->leaf_memory_top += sizeof(bvh_leaf);
	b->leaf_memory_top += num_tris_aligned * sizeof(leaf_tri);
	b->leaf_memory_top += num_prims * sizeof(leaf_prim);
	b->leaf_memory_top += num_unique_meshes * sizeof(leaf_tri_obj);

	// Align to cache lines
	b->leaf_memory_top = align_up_sz(b->leaf_memory_top, 64);

	node->child_ix = ~0u;
	node->vertex_offset = ~0u;
}

// Build node using the surface area heuristic (SAH)
static void build_node_sah(build *b, build_node *node, int depth)
{
	build_bucket buckets[BVH_BUILD_SPLITS];

	build_node left, right;
	rtk_real best_cost = RTK_INF;
	int best_axis = -1;
	int best_bucket = -1;
	rtk_real rcp_parent_area = 1.0f / bounds_area(&node->bounds);

	for (int axis = 0; axis < 3; axis++) {
		// Reset all buckets for the axis
		for (int i = 0; i < BVH_BUILD_SPLITS; i++) {
			bounds_reset(&buckets[i].bounds);
			buckets[i].num = 0;
		}

		rtk_real min = node->bounds.min.v[axis];
		rtk_real max = node->bounds.max.v[axis];
		rtk_real min_2x = 2.0f * min;
		rtk_real rcp_scale_2x = (0.5f * (rtk_real)BVH_BUILD_SPLITS) / (max - min);

		// Insert items into the buckets
		rtk_for(build_item, item, b->items + node->begin, node->num) {
			rtk_real mid_2x = item->bounds.min.v[axis] + item->bounds.max.v[axis];
			int bucket = (int)((mid_2x - min_2x) * rcp_scale_2x);
			if (bucket < 0) bucket = 0;
			if (bucket >= BVH_BUILD_SPLITS - 1) bucket = BVH_BUILD_SPLITS - 1;
			bounds_add(&buckets[bucket].bounds, &item->bounds);
			buckets[bucket].num++;
		}

		// Scan backwards to get `bounds_right`
		buckets[BVH_BUILD_SPLITS - 1].bounds_right = buckets[BVH_BUILD_SPLITS - 1].bounds;
		for (int i = BVH_BUILD_SPLITS - 2; i >= 0; i--) {
			buckets[i].bounds_right.min = v_min(buckets[i].bounds.min, buckets[i + 1].bounds_right.min);
			buckets[i].bounds_right.max = v_max(buckets[i].bounds.max, buckets[i + 1].bounds_right.max);
		}

		// Scan forwards to find the best split
		rtk_bounds bounds_left;
		bounds_reset(&bounds_left);
		uint32_t num_left = 0;

		for (int i = 0; i < BVH_BUILD_SPLITS - 1; i++) {
			build_bucket *bucket = &buckets[i];
			build_bucket *bucket_right = &buckets[i + 1];
			bounds_add(&bounds_left, &bucket->bounds);

			num_left += bucket->num;
			uint32_t num_right = node->num - num_left;
			if (num_left == 0 || num_right == 0) continue;

			rtk_real area_left = bounds_area(&bounds_left);
			rtk_real area_right = bounds_area(&bucket_right->bounds_right);

			rtk_real cost_left = (rtk_real)((num_left + 3) / 4) * SAH_ITEM_COST;
			rtk_real cost_right = (rtk_real)((num_right + 3) / 4) * SAH_ITEM_COST;
			rtk_real split_cost = SAH_BVH_COST + (area_left*cost_left + area_right*cost_right) * rcp_parent_area;

			if (split_cost < best_cost) {
				left.bounds = bounds_left;
				right.bounds = bucket_right->bounds_right;
				best_cost = split_cost;
				best_axis = axis;
				best_bucket = i;
			}
		}
	}

	rtk_real leaf_cost = (rtk_real)node->num * SAH_ITEM_COST;
	if (best_cost < leaf_cost || node->num > BVH_LEAF_MAX_ITEMS) {
		// Split the node using the best axis and bucket, we need to replicate the
		// exact split criteria (bucket index) to make the heuristic accurate.
		rtk_assert(best_axis >= 0);

		rtk_real min = node->bounds.min.v[best_axis];
		rtk_real max = node->bounds.max.v[best_axis];
		rtk_real min_2x = 2.0f * min;
		rtk_real rcp_scale_2x = (0.5f * (rtk_real)BVH_BUILD_SPLITS) / (max - min);

		// Split the items
		build_item *items = b->items + node->begin;
		build_item *first = items;
		build_item *last = first + node->num;
		while (first != last) {
			rtk_real mid_2x = first->bounds.min.v[best_axis] + first->bounds.max.v[best_axis];
			int bucket = (int)((mid_2x - min_2x) * rcp_scale_2x);
			if (bucket < 0) bucket = 0;
			if (bucket >= BVH_BUILD_SPLITS - 1) bucket = BVH_BUILD_SPLITS - 1;
			if (bucket <= best_bucket) {
				first++;
			} else {
				last--;
				build_item temp = *first;
				*first = *last;
				*last = temp;
			}
		}

		uint32_t num_left = (uint32_t)(first - items);
		uint32_t num_right = node->num - num_left;
		rtk_assert(num_left > 0 && num_right > 0);

		left.begin = node->begin;
		left.num = num_left;
		right.begin = node->begin + num_left;
		right.num = num_right;

		build_node_rec(b, &left, depth + 1);
		build_node_rec(b, &right, depth + 1);

		uint32_t child_ix = b->num_nodes;
		b->num_nodes += 2;
		buf_grow(&b->nodes, &b->cap_nodes, b->num_nodes);
		b->nodes[child_ix + 0] = left;
		b->nodes[child_ix + 1] = right;
		node->child_ix = child_ix;
		node->vertex_offset = ~0u;
		b->depth_num_nodes[depth]++;
	} else {
		build_node_leaf(b, node);
	}
}

// Build node by splitting it into equal parts by the largest axis
static void build_node_equal(build *b, build_node *node, int depth)
{
	rtk_vec3 size = v_sub(node->bounds.max, node->bounds.min);
	rtk_real max_comp = real_max(real_max(size.x, size.y), size.z);
	int axis = size.x == max_comp ? 0 : size.y == max_comp ? 1 : 2;

	build_item *items = b->items + node->begin;
	qsort(items, node->num, sizeof(build_item), item_sort_axis_fn[axis]);

	uint32_t num_left = node->num / 2;

	build_node left, right;
	bounds_reset(&left.bounds);
	bounds_reset(&right.bounds);
	left.begin = node->begin;
	left.num = num_left;
	right.begin = node->begin + num_left;
	right.num = node->num - num_left;

	// Calculate child bounds
	rtk_for(build_item, item, items, num_left) {
		bounds_add(&left.bounds, &item->bounds);
	}
	rtk_for(build_item, item, items + num_left, node->num) {
		bounds_add(&right.bounds, &item->bounds);
	}

	build_node_rec(b, &left, depth + 1);
	build_node_rec(b, &right, depth + 1);

	uint32_t child_ix = b->num_nodes;
	b->num_nodes += 2;
	buf_grow(&b->nodes, &b->cap_nodes, b->num_nodes);
	b->nodes[child_ix + 0] = left;
	b->nodes[child_ix + 1] = right;
	node->child_ix = child_ix;
	node->vertex_offset = ~0u;
	b->depth_num_nodes[depth]++;
}

// Recursive node build, selects the optimal implementation to call
static void build_node_rec(build *b, build_node *node, int depth)
{
	#if RTK_REGRESSION
	{
		// Verify that all items are contained within the bounds and
		// that the union bounds of all items matches with the node.
		rtk_bounds debug_bounds;
		bounds_reset(&debug_bounds);
		rtk_for(build_item, item, b->items + node->begin, node->num) {
			rtk_assert(item->bounds.min.x >= node->bounds.min.x);
			rtk_assert(item->bounds.min.y >= node->bounds.min.y);
			rtk_assert(item->bounds.min.z >= node->bounds.min.z);
			rtk_assert(item->bounds.max.x <= node->bounds.max.x);
			rtk_assert(item->bounds.max.y <= node->bounds.max.y);
			rtk_assert(item->bounds.max.z <= node->bounds.max.z);
			bounds_add(&debug_bounds, &item->bounds);
		}
		rtk_assert(debug_bounds.min.x == node->bounds.min.x);
		rtk_assert(debug_bounds.min.y == node->bounds.min.y);
		rtk_assert(debug_bounds.min.z == node->bounds.min.z);
		rtk_assert(debug_bounds.max.x == node->bounds.max.x);
		rtk_assert(debug_bounds.max.y == node->bounds.max.y);
		rtk_assert(debug_bounds.max.z == node->bounds.max.z);
	}
	#endif

	// Reached the end, must do a leaf.
	if (depth == BVH_MAX_DEPTH) {
		build_node_leaf(b, node);
		return;
	}

	// Calculate the amount of triangles/primitives per node if we _don't_
	// start splitting them evenly at this level. If these exceed the limits
	// then we must split evenly.
	int splits_left = BVH_MAX_DEPTH - depth - 1;
	if (splits_left < 0) splits_left = 0;
	if (splits_left > 31) splits_left = 31;
	size_t split_items = node->num >> splits_left;
	if (split_items > BVH_LEAF_MAX_ITEMS) {
		build_node_equal(b, node, depth);
		return;
	}

	// Create a leaf node if there's not many triangles or primitives
	if (node->num <= BVH_LEAF_MIN_ITEMS) {
		build_node_leaf(b, node);
		return;
	}

	// Default: Use SAH
	build_node_sah(b, node, depth);
}

// -- Vertex group building

static void vertex_set_insert(vertex_set *dst, vertex_id entry)
{
	// Find the correct position to insert `entry`
	vertex_id *entries = dst->entries;
	size_t size = dst->size;
	assert(size < BVH_GROUP_MAX_VERTICES);
	for (size_t i = 0; i < size; i++) {
		if (entries[i] >= entry) {
			if (entries[i] == entry) return; // Found!
			// Push back all the entries after `entry`
			for (; i < size; i++) {
				vertex_id tmp = entries[i];
				entries[i] = entry;
				entry = tmp;
			}
			break;
		}
	}
	entries[size] = entry;
	dst->size = size + 1;
}

static int vertex_set_merge(vertex_set *dst, const vertex_set *a, const vertex_set *b)
{
	const vertex_id *ap = a->entries, *aend = ap + a->size;
	const vertex_id *bp = b->entries, *bend = bp + b->size;
	vertex_id *dp = dst->entries, *dend = dp + BVH_GROUP_MAX_VERTICES;

	// Branchlessly merge A and B in the correct order
	while (ap != aend && bp != bend) {
		if (dp == dend) return 0;
		vertex_id av = *ap, bv = *bp;
		int ax = av <= bv ? 1 : 0;
		int bx = bv <= av ? 1 : 0;
		ap += ax;
		bp += bx;
		*dp++ = ax ? av : bv;
	}

	// Copy the rest from A and B
	while (ap != aend) {
		if (dp == dend) return 0;
		*dp++ = *ap++;
	}
	while (bp != bend) {
		if (dp == dend) return 0;
		*dp++ = *bp++;
	}

	dst->size = dp - dst->entries;

	return 1;
}

rtk_inline void vertex_set_copy(vertex_set *dst, const vertex_set *src)
{
	dst->size = src->size;
	memcpy(dst->entries, src->entries, src->size * sizeof(vertex_id));
}

// Find `entry` in the set, returns ~0u if not found
rtk_inline uint32_t vertex_set_find(const vertex_set *set, vertex_id entry)
{
	const vertex_id *lo = set->entries;
	const vertex_id *hi = lo + set->size;

	while (hi - lo > 8) {
		size_t dist = hi - lo;
		const vertex_id *mid = lo + (dist >> 1);
		if (*mid > entry) {
			hi = mid;
		} else {
			lo = mid;
		}
	}

	for (; lo != hi; lo++) {
		if (*lo == entry) {
			return (uint32_t)(lo - set->entries);
		}
	}

	return ~0u;
}

static void build_assign_vertices(build *b, build_node *node, const vertex_set *set, uint32_t vertex_offset)
{
	// Return if this node has a vertex group already
	if (node->vertex_offset != ~0u) return;
	node->vertex_offset = vertex_offset;
	if (node->child_ix != ~0u) {
		build_node *child = b->nodes + node->child_ix;
		build_assign_vertices(b, &child[0], set, vertex_offset);
		build_assign_vertices(b, &child[1], set, vertex_offset);
	} else {
		rtk_for(build_item, item, b->items + node->begin, node->num) {
			uint32_t mesh_ix = item->mesh_ix;
			if (mesh_ix == ~0u) continue;
			for (int i = 0; i < 3; i++) {
				uint32_t ix = vertex_set_find(set, vi_make(mesh_ix, item->vertex_ix[i]));
				rtk_assert(ix != ~0u);
				item->vertex_ix[i] = ix;
			}
		}
	}
}

// Close the vertex set `set` copying the data into the contiguous array and
// assigning vertices in related nodes.
static void build_close_vertices(build *b, build_node *node, const vertex_set *set)
{
	uint32_t vertex_offset = b->num_verts;
	b->num_verts += (uint32_t)set->size;
	buf_grow(&b->verts, &b->cap_verts, b->num_verts);

	// Copy mesh vertices into the group
	leaf_vert *dst = b->verts + vertex_offset;
	rtk_for(const vertex_id, id, set->entries, set->size) {
		uint32_t mesh_ix = vi_mesh(*id);
		uint32_t vert_ix = vi_vertex(*id);
		if (mesh_ix == MESH_IX_TRI) {
			uint32_t tri_ix = vert_ix / 3u;
			uint32_t corner_ix = vert_ix % 3u;
			dst->pos = b->desc.triangles[tri_ix].v[corner_ix];
			dst->index = vert_ix;
		} else {
			const rtk_mesh *mesh = &b->desc.meshes[mesh_ix];
			size_t stride = mesh->vertices_stride ? mesh->vertices_stride : sizeof(rtk_vec3);
			dst->pos = *(const rtk_vec3*)((char*)mesh->vertices + vert_ix * stride);
			dst->index = vert_ix;
			// TODO: Branch this? Profile?
			dst->pos = mat_mul_pos(&mesh->transform, dst->pos);
		}
		dst++;
	}

	build_assign_vertices(b, node, set, vertex_offset);
}

// Gather vertices from `node_ptr` into `parent_set`. Returns non-zero if
// the vertex group is still open ie. it can be appended into.
static int build_gather_vertices(build *b, build_node *node, vertex_set *parent_set)
{
	if (node->child_ix != ~0u) {
		build_node *child = b->nodes + node->child_ix;

		vertex_set set[2];
		set[0].size = set[1].size = 0;
		int open[2];
		open[0] = build_gather_vertices(b, &child[0], &set[0]);
		open[1] = build_gather_vertices(b, &child[1], &set[1]);

		// If both sets are closed we're done with this node
		if (!open[0] && !open[1]) return 0;

		// If both sets are open try to merge them
		if (open[0] && open[1]) {
			if (vertex_set_merge(parent_set, &set[0], &set[1])) {
				return 1;
			}

			// Close the larger child and pass along the smaller one
			int close_ix = set[1].size > set[0].size ? 1 : 0;
			build_close_vertices(b, &child[close_ix], &set[close_ix]);
			vertex_set_copy(parent_set, &set[close_ix ^ 1]);
			return 1;
		}

		// Pass along the currently open set
		int copy_ix = open[1] ? 1 : 0;
		vertex_set_copy(parent_set, &set[copy_ix]);
		return 1;

	} else {

		// Insert vertices of all the triangles in this node
		rtk_for(build_item, item, b->items + node->begin, node->num) {
			uint32_t mesh_ix = item->mesh_ix;
			if (mesh_ix == ~0u) continue;
			for (int i = 0; i < 3; i++) {
				vertex_set_insert(parent_set, vi_make(mesh_ix, item->vertex_ix[i]));
			}
		}

		return 1;
	}
}

// -- Attribute deduplication

static int attrib_sort_ptr(const void *a, const void *b) {
	const build_attrib *aa = (const build_attrib*)a;
	const build_attrib *ab = (const build_attrib*)b;
	if (aa->data != ab->data) return aa->data < ab->data ? -1 : +1;
	return aa->num_data > ab->num_data ? -1 : +1;
}

// -- BVH finalization

static void bvh_linearize_leaf(build *b, bvh_leaf *dst, build_node *src)
{
	char *begin = (char*)dst;

	dst->num_tris = src->num_tris;
	dst->num_prims = src->num - src->num_tris;
	dst->vertices = b->final_verts + src->vertex_offset;

	uint32_t num_tris_aligned = align_up_u32(src->num_tris, 4);
	leaf_tri *tris_begin = (leaf_tri*)(dst + 1);
	leaf_tri *tri = tris_begin;
	leaf_tri *tris_end = (leaf_tri*)(tris_begin + num_tris_aligned);
	leaf_prim *prim = (leaf_prim*)tris_end;
	leaf_prim *prims_end = prim + dst->num_prims;
	leaf_tri_obj *tri_obj = (leaf_tri_obj*)prims_end;

	uint32_t unique_meshes[BVH_LEAF_MAX_ITEMS];
	uint32_t num_unique_meshes = 0;

	rtk_for(build_item, item, b->items + src->begin, src->num) {
		uint32_t mesh_ix = item->mesh_ix;
		if (mesh_ix != MESH_IX_PRIM) {
			uint32_t obj_ix;
			if (mesh_ix == MESH_IX_TRI) {
				// Every triangle has an unique mesh
				uint32_t tri_ix = item->vertex_ix[0] / 3;
				obj_ix = num_unique_meshes++;
				tri_obj->mesh_ix = (uint32_t)b->desc.num_meshes + tri_ix;
				tri_obj++;
			} else {
				// O(n^2) but n is bounded by `BVH_LEAF_MAX_ITEMS` and probably _very_ small
				for (obj_ix = 0; obj_ix != num_unique_meshes; obj_ix++) {
					if (unique_meshes[obj_ix] == mesh_ix) break;
				}
				if (obj_ix == num_unique_meshes) {
					unique_meshes[obj_ix] = mesh_ix;
					num_unique_meshes++;

					tri_obj->mesh_ix = mesh_ix;
					tri_obj++;
				}
			}

			tri->v[0] = (uint8_t)item->vertex_ix[0];
			tri->v[1] = (uint8_t)item->vertex_ix[1];
			tri->v[2] = (uint8_t)item->vertex_ix[2];
			tri->obj_ix = (uint8_t)obj_ix;
			tri++;
		} else {
			const rtk_primitive *prim_src = &b->desc.primitives[item->prim_ix];
			prim->prim = *prim_src;
			mat_inverse(&prim->inv_mat, &prim_src->transform);
			prim++;
		}
	}

	rtk_assert(tri <= tris_end);
	// Copy the first triangle over the end, in SSE we want to avoid intersecting
	// with the dummy lanes, duplicating the last element would cause false intersections.
	while (tri != tris_end) {
		*tri++ = *tris_begin;
	}

	rtk_assert(prim == prims_end);

	char *end = (char*)tri_obj;
	size_t size = end - begin;
	b->final_leaf_offset += size;
	b->final_leaf_offset = align_up_sz(b->final_leaf_offset, 64);
}

static void bvh_linearize_node(build *b, bvh_node *dst, build_node *src, int depth)
{
	for (unsigned i = 0; i < 4; i++) {
		uint32_t mid_i = i >> 1;
		uint32_t child_i = i & 1;

		build_node *mid_src = &b->nodes[src->child_ix + mid_i];
		build_node *child_src;
		if (mid_src->child_ix != ~0u) {
			child_src = &b->nodes[mid_src->child_ix + child_i];
		} else {
			if (child_i == 0) {
				child_src = mid_src;
			} else {
				child_src = NULL;
			}
		}

		// Replace empty nodes with NULL leaves
		if (child_src != NULL && child_src->num == 0) {
			child_src = NULL;
		}

		if (child_src) {
			dst->bounds_x[0][i] = child_src->bounds.min.x;
			dst->bounds_x[1][i] = child_src->bounds.max.x;
			dst->bounds_y[0][i] = child_src->bounds.min.y;
			dst->bounds_y[1][i] = child_src->bounds.max.y;
			dst->bounds_z[0][i] = child_src->bounds.min.z;
			dst->bounds_z[1][i] = child_src->bounds.max.z;

			if (child_src->child_ix != ~0u) {
				uint32_t offset = b->depth_node_offset[depth]++;
				bvh_node *child_dst = b->final_nodes + offset;
				dst->ptr[i] = bp_make_node(child_dst);
				bvh_linearize_node(b, child_dst, child_src, depth + 2);
			} else {
				bvh_leaf *leaf_dst = (bvh_leaf*)(b->final_leaves + b->final_leaf_offset);
				dst->ptr[i] = bp_make_leaf(leaf_dst);
				bvh_linearize_leaf(b, leaf_dst, child_src);
			}
		} else {
			dst->bounds_x[0][i] = 0.0f;
			dst->bounds_x[1][i] = 0.0f;
			dst->bounds_y[0][i] = 0.0f;
			dst->bounds_y[1][i] = 0.0f;
			dst->bounds_z[0][i] = 0.0f;
			dst->bounds_z[1][i] = 0.0f;
			dst->ptr[i] = b->null_leaf;
		}
	}
}

#if RTK_REGRESSION

static void regression_verify_build_node_rec(build *b, const build_node *node, const rtk_bounds *parent_bounds)
{
	rtk_assert(node->bounds.min.x >= parent_bounds->min.x);
	rtk_assert(node->bounds.min.y >= parent_bounds->min.y);
	rtk_assert(node->bounds.min.z >= parent_bounds->min.z);
	rtk_assert(node->bounds.max.x <= parent_bounds->max.x);
	rtk_assert(node->bounds.max.y <= parent_bounds->max.y);
	rtk_assert(node->bounds.max.z <= parent_bounds->max.z);
	if (node->child_ix != ~0u) {
		const build_node *child_src = &b->nodes[node->child_ix];
		for (int i = 0; i < 2; i++) {
			regression_verify_build_node_rec(b, &child_src[i], &node->bounds);
		}
	}
}

static void regression_verify_bvh_node_rec(build *b, const bvh_node *node, const rtk_bounds *parent_bounds)
{
	for (int i = 0; i < 2; i++) {
		rtk_assert(node->bounds[i].min.x >= parent_bounds->min.x);
		rtk_assert(node->bounds[i].min.y >= parent_bounds->min.y);
		rtk_assert(node->bounds[i].min.z >= parent_bounds->min.z);
		rtk_assert(node->bounds[i].max.x <= parent_bounds->max.x);
		rtk_assert(node->bounds[i].max.y <= parent_bounds->max.y);
		rtk_assert(node->bounds[i].max.z <= parent_bounds->max.z);
		if (bp_is_node(node->ptr[i])) {
			regression_verify_bvh_node_rec(b, bp_node(node->ptr[i]), &node->bounds[i]);
		}
	}
}

#endif

static rtk_scene *build_root(build *b)
{
	build_node root;
	bounds_reset(&root.bounds);

	// Count the number of items
	size_t num_items = b->desc.num_primitives + b->desc.num_triangles;
	rtk_for(const rtk_mesh, mesh, b->desc.meshes, b->desc.num_meshes) {
		num_items += mesh->num_triangles;
	}
	size_t num_total_meshes = b->desc.num_meshes + b->desc.num_triangles;

	root.begin = 0;
	root.num = (uint32_t)num_items;
	b->items = malloc(sizeof(build_item) * num_items);

	build_item *item = b->items;

	// Collect triangles from meshes
	rtk_for(const rtk_mesh, mesh, b->desc.meshes, b->desc.num_meshes) {
		const rtk_vec3 *verts = mesh->vertices;
		uint32_t mesh_ix = (uint32_t)(mesh - b->desc.meshes);
		int has_mat = !mat_is_identity(&mesh->transform);
		rtk_matrix mat = mesh->transform;
		uint32_t max_ix = 0;
		size_t vert_stride = mesh->vertices_stride ? mesh->vertices_stride : sizeof(rtk_vec3);
		for (const uint32_t *ix = mesh->indices, *ix_end = ix + mesh->num_triangles * 3; ix != ix_end; ix += 3) {
			rtk_vec3 v[3];
			v[0] = *(const rtk_vec3*)((const char*)verts + ix[0]*vert_stride);
			v[1] = *(const rtk_vec3*)((const char*)verts + ix[1]*vert_stride);
			v[2] = *(const rtk_vec3*)((const char*)verts + ix[2]*vert_stride);
			if (ix[0] > max_ix) max_ix = ix[0];
			if (ix[1] > max_ix) max_ix = ix[1];
			if (ix[2] > max_ix) max_ix = ix[2];
			if (has_mat) {
				for (int i = 0; i < 3; i++) {
					v[i] = mat_mul_pos(&mat, v[i]);
				}
			}

			rtk_vec3 min = v_min(v_min(v[0], v[1]), v[2]);
			rtk_vec3 max = v_max(v_max(v[0], v[1]), v[2]);
			item->bounds.min = min;
			item->bounds.max = max;
			item->mesh_ix = mesh_ix;
			item->vertex_ix[0] = ix[0];
			item->vertex_ix[1] = ix[1];
			item->vertex_ix[2] = ix[2];
			item++;
			root.bounds.min = v_min(root.bounds.min, min);
			root.bounds.max = v_max(root.bounds.max, max);
		}

		if (mesh->num_triangles > 0) {
			uint32_t num_verts = max_ix + 1;
			if (mesh->uvs) {
				buf_grow(&b->attribs, &b->cap_attribs, ++b->num_attribs);
				build_attrib *attrib = &b->attribs[b->num_attribs - 1];
				attrib->data = (rtk_real*)mesh->uvs;
				attrib->num_data = num_verts * 2;
				attrib->num_components = 2;
				attrib->mesh_ix = (uint32_t)(mesh - b->desc.meshes);
				attrib->type = build_attrib_uv;
				attrib->stride = mesh->uvs_stride ? mesh->uvs_stride : sizeof(rtk_vec2);
			}
			if (mesh->normals) {
				buf_grow(&b->attribs, &b->cap_attribs, ++b->num_attribs);
				build_attrib *attrib = &b->attribs[b->num_attribs - 1];
				attrib->data = (rtk_real*)mesh->normals;
				attrib->num_data = num_verts * 3;
				attrib->num_components = 3;
				attrib->mesh_ix = (uint32_t)(mesh - b->desc.meshes);
				attrib->type = build_attrib_normal;
				attrib->stride = mesh->normals_stride ? mesh->normals_stride : sizeof(rtk_vec3);
			}
		}
	}

	// Collect standalone triangles
	uint32_t tri_vert_ix = 0;
	rtk_for(const rtk_triangle, tri, b->desc.triangles, b->desc.num_triangles) {
		rtk_vec3 min = v_min(v_min(tri->v[0], tri->v[1]), tri->v[2]);
		rtk_vec3 max = v_max(v_max(tri->v[0], tri->v[1]), tri->v[2]);
		item->bounds.min = min;
		item->bounds.max = max;
		item->mesh_ix = MESH_IX_TRI;
		item->vertex_ix[0] = tri_vert_ix + 0;
		item->vertex_ix[1] = tri_vert_ix + 1;
		item->vertex_ix[2] = tri_vert_ix + 2;
		item++;
		root.bounds.min = v_min(root.bounds.min, min);
		root.bounds.max = v_max(root.bounds.max, max);
		tri_vert_ix += 3;
	}

	// Collect primitives
	uint32_t prim_ix = 0;
	rtk_for(const rtk_primitive, prim, b->desc.primitives, b->desc.num_primitives) {
		rtk_bounds bounds = transform_bounds(&prim->bounds, &prim->transform);
		bounds_add(&root.bounds, &bounds);
		item->bounds = bounds;
		item->mesh_ix = MESH_IX_PRIM;
		item->prim_ix = prim_ix++;
		item++;
	}

	rtk_assert(item == b->items + num_items);

	// Recursive BVH build
	build_node_rec(b, &root, 0);

	// Root node is always inlined
	b->depth_num_nodes[0] = 0;

	// Reserve NULL leaf node
	b->leaf_memory_top += sizeof(bvh_leaf);
	b->leaf_memory_top = align_up_sz(b->leaf_memory_top, 64);

	// If the root node is a leaf generate a virtual root
	if (root.child_ix == ~0u) {
		build_node right;
		right.bounds.min = right.bounds.max = root.bounds.min;
		right.begin = right.num_tris = right.num = 0;
		right.child_ix = ~0u;
		right.vertex_offset = ~0u;

		uint32_t child_ix = b->num_nodes;
		b->num_nodes += 2;
		buf_grow(&b->nodes, &b->cap_nodes, b->num_nodes);
		b->nodes[child_ix + 0] = root;
		b->nodes[child_ix + 1] = right;
		root.child_ix = child_ix;
		root.vertex_offset = ~0u;
	}

	// Assign vertices
	{
		vertex_set root_set;
		root_set.size = 0;
		if (build_gather_vertices(b, &root, &root_set)) {
			build_close_vertices(b, &root, &root_set);
		}
	}

	// Count nodes and resolve offsets
	uint32_t num_nodes = 0;
	for (size_t i = 0; i < BVH_MAX_DEPTH; i++) {
		b->depth_node_offset[i] = num_nodes;
		num_nodes += b->depth_num_nodes[i];
	}

	// Deduplicate and count attributes
	size_t num_attrib_data = 0;
	{
		// Sort by pointer and size, only copy the largest instance of each buffer
		qsort(b->attribs, b->num_attribs, sizeof(build_attrib), &attrib_sort_ptr);
		const void *prev_data = NULL;
		size_t offset = 0;
		size_t prev_end = 0;
		rtk_for(build_attrib, attrib, b->attribs, b->num_attribs) {
			if (attrib->data != prev_data) {
				prev_data = attrib->data;
				offset = prev_end;
				prev_end += attrib->num_data;
				attrib->largest_duplicate = 1;
			} else {
				attrib->largest_duplicate = 0;
			}
			attrib->final_offset = offset;
		}
		num_attrib_data = prev_end;
	}

	// Setup contiguous allocation
	size_t m = 0;
	size_t off_scene = push_offset(&m, sizeof(rtk_scene), 64);
	size_t off_nodes = push_offset(&m, sizeof(bvh_node) * num_nodes, 64);
	size_t off_leaves = push_offset(&m, b->leaf_memory_top, 64);
	size_t off_verts = push_offset(&m, sizeof(leaf_vert) * b->num_verts, 64);
	size_t off_meshes = push_offset(&m, sizeof(mesh_data) * num_total_meshes, 64);
	size_t off_attrib_data = push_offset(&m, sizeof(rtk_real) * num_attrib_data, 64);

	// Allocate and align to 64 bytes
	size_t alloc_size = m + 64;
	char *memory = (char*)malloc(alloc_size);
	char *ptr = memory + (-(uintptr_t)memory & 63);

	rtk_scene *scene = (rtk_scene*)(ptr + off_scene);
	scene->bounds = root.bounds;
	scene->memory = memory;
	scene->memory_size = alloc_size;

	b->final_nodes = (bvh_node*)(ptr + off_nodes);
	b->final_leaves = (char*)(ptr + off_leaves);
	b->final_verts = (leaf_vert*)(ptr + off_verts);

	memcpy(b->final_verts, b->verts, sizeof(leaf_vert) * b->num_verts);
	free(b->verts);

	// Build NULL leaf
	{
		bvh_leaf *null_leaf = (bvh_leaf*)(b->final_leaves + b->final_leaf_offset);
		build_node null_leaf_node = { 0 };
		null_leaf_node.child_ix = ~0u;
		bvh_linearize_leaf(b, null_leaf, &null_leaf_node);
		b->null_leaf = bp_make_leaf(null_leaf);
	}

	// Linearize the nodes recursively
	bvh_linearize_node(b, &scene->root, &root, 1);

	// Initialize mesh data
	mesh_data *final_meshes = (mesh_data*)(ptr + off_meshes);
	scene->meshes = final_meshes;
	mesh_data *dst_mesh = final_meshes;
	rtk_for(const rtk_mesh, mesh, b->desc.meshes, b->desc.num_meshes) {
		dst_mesh->object = mesh->object;
		dst_mesh->normals = NULL;
		dst_mesh->uvs = NULL;
		dst_mesh++;
	}
	rtk_for(const rtk_triangle, tri, b->desc.triangles, b->desc.num_triangles) {
		dst_mesh->object = tri->object;
		dst_mesh->normals = NULL;
		dst_mesh->uvs = NULL;
		dst_mesh++;
	}
	rtk_assert(dst_mesh == final_meshes + num_total_meshes);

	// Copy and assign attribute data
	rtk_real *attrib_data = (rtk_real*)(ptr + off_attrib_data);
	rtk_for(build_attrib, attrib, b->attribs, b->num_attribs) {
		mesh_data *mesh = &final_meshes[attrib->mesh_ix];
		rtk_real *data = attrib_data + attrib->final_offset;
		switch (attrib->type) {
		case build_attrib_uv: mesh->uvs = (rtk_vec2*)data; break;
		case build_attrib_normal: mesh->normals = (rtk_vec3*)data; break;
		default: rtk_assert(0 && "Unexpected build_attrib_type"); break;
		}
		if (attrib->largest_duplicate) {
			if (attrib->stride == 0) {
				memcpy(data, attrib->data, attrib->num_data * sizeof(rtk_real));
			} else {
				size_t num_components = attrib->num_components;
				rtk_real *dst = data, *end = data + attrib->num_data;
				const char *src = (const char*)attrib->data;
				size_t stride = attrib->stride;
				while (dst != end) {
					memcpy(dst, src, num_components * sizeof(rtk_real));
					dst += num_components;
					src += stride;
				}
			}
		}
	}

	#if RTK_REGRESSION
	{
		// Check that nodes were allocated correctly
		size_t offset = 0;
		for (size_t i = 0; i < BVH_MAX_DEPTH; i++) {
			size_t num = b->depth_node_offset[i] - offset;
			rtk_assert(num == b->depth_num_nodes[i]);
			offset = b->depth_node_offset[i];
		}

		// Recusively verify build and BVH trees
		regression_verify_build_node_rec(b, &root, &root.bounds);
		regression_verify_bvh_node_rec(b, &scene->root, &root.bounds);
	}
	#endif

	rtk_assert(b->final_leaf_offset == b->leaf_memory_top);

	free(b->items);
	free(b->nodes);
	free(b->attribs);

	return scene;
}

// -- Primitive intersection functions

static int isect_prim_scene(const rtk_primitive *p, const rtk_ray *ray, rtk_hit *hit)
{
	const rtk_scene *scene = (const rtk_scene*)p->user;
	if (rtk_raytrace(scene, ray, hit, hit->t)) {
		if (hit->num_parents < RTK_HIT_MAX_PARENTS) {
			hit->parent_objects[hit->num_parents] = p->object;
			hit->num_parents++;
		}
		return 1;
	} else {
		return 0;
	}
}

static int isect_prim_sphere(const rtk_primitive *p, const rtk_ray *ray, rtk_hit *hit)
{
	rtk_real a = v_dot(ray->direction, ray->direction);
	rtk_real b = 2.0f * v_dot(ray->origin, ray->direction);
	rtk_real c = v_dot(ray->origin, ray->origin) - 1.0f;
	rtk_real root = b*b - 4.0f*a*c;
	if (root < 0.0f) return 0;
	root = rtk_sqrt(root);
	rtk_real denom = 0.5f / a;
	rtk_real t0 = (-b - root) * denom;
	rtk_real t1 = (-b + root) * denom;
	rtk_real t = t0 > ray->min_t ? t0 : t1;
	if (t > ray->min_t && t < hit->t) {
		rtk_vec3 n = v_mad(ray->direction, t, ray->origin);
		rtk_real rcp_phi_radius = rtk_sqrt(n.x*n.x + n.y*n.y);
		rtk_real phi = (rtk_real)atan2(n.y, n.x);
		rtk_real theta = (rtk_real)acos(n.z);
		rtk_real cos_phi = n.x * rcp_phi_radius;
		rtk_real sin_phi = n.y * rcp_phi_radius;
		rtk_real sin_theta = rtk_sqrt(real_max(0.0f, 1.0f - n.z*n.z));
		hit->t = t;
		hit->geom.u = phi * (0.5f/REAL_PI) + 0.5f;
		hit->geom.v = theta * (1.0f/REAL_PI) + 0.5f;
		hit->geom.dp_du = v_make(-n.y*REAL_2PI, n.x*REAL_2PI, 0.0f);
		hit->geom.dp_dv = v_make(n.z*cos_phi*REAL_PI, n.z*sin_phi*REAL_PI, -sin_theta*REAL_PI);
		hit->geom.normal = n;
		hit->interp = hit->geom;
		hit->user = NULL;
		hit->object = p->object;
		hit->num_parents = 0;
		hit->geometry_type = rtk_hit_sphere;
		return 1;
	} else {
		return 0;
	}
}

static int isect_prim_plane(const rtk_primitive *p, const rtk_ray *ray, rtk_hit *hit)
{
	rtk_real t = -ray->origin.x / ray->direction.x;
	if (t > ray->min_t && t < hit->t) {
		hit->t = t;
		hit->geom.u = ray->origin.y + ray->direction.y * t;
		hit->geom.v = ray->origin.z + ray->direction.z * t;
		hit->geom.normal = v_make(1.0f, 0.0f, 0.0f);
		hit->geom.dp_du = v_make(0.0f, 1.0f, 0.0f);
		hit->geom.dp_dv = v_make(0.0f, 0.0f, 1.0f);
		hit->interp = hit->geom;
		hit->user = NULL;
		hit->object = p->object;
		hit->num_parents = 0;
		hit->geometry_type = rtk_hit_plane;
		return 1;
	} else {
		return 0;
	}
}

// -- API

extern const rtk_matrix rtk_identity = {
	1.0f,0.0f,0.0f, 0.0f,1.0f,0.0f, 0.0f,0.0f,1.0f, 0.0f,0.0f,0.0f,
};

rtk_scene *rtk_create_scene(const rtk_scene_desc *desc)
{
	build b;
	memset(&b, 0, sizeof(b));
	b.desc = *desc;
	return build_root(&b);
}

void rtk_free_scene(rtk_scene *s)
{
	if (!s) return;
	free(s->memory);
}

int rtk_raytrace(const rtk_scene *s, const rtk_ray *ray, rtk_hit *hit, rtk_real max_t)
{
	SSE_ALIGN raytrace rt;
	rt.ray = *ray;
	rt.mesh_ix = ~0u;

	rt.hit.t = max_t;
	rt.hit.user = NULL;

	rtk_vec3 abs = v_abs(ray->direction);
	rtk_real max_comp = real_max(real_max(abs.x, abs.y), abs.z);
	unsigned shear_z = abs.x == max_comp ? 0 : abs.y == max_comp ? 1 : 2;
	unsigned shear_x = (shear_z + 1) % 3u;
	unsigned shear_y = (shear_z + 2) % 3u;
	uint32_t sign_mask = 0;

#if RTK_SSE
	__m128 zero = _mm_setzero_ps();
	sign_mask |= _mm_comilt_ss(_mm_set_ss(ray->direction.x), zero);
	sign_mask |= _mm_comilt_ss(_mm_set_ss(ray->direction.y), zero) << 1;
	sign_mask |= _mm_comilt_ss(_mm_set_ss(ray->direction.z), zero) << 2;
#else
	sign_mask |= ray->direction.x < 0.0f ? 1 : 0;
	sign_mask |= ray->direction.y < 0.0f ? 2 : 0;
	sign_mask |= ray->direction.z < 0.0f ? 4 : 0;
#endif

	rt.shear_axis[0] = (int)shear_x;
	rt.shear_axis[1] = (int)shear_y;
	rt.shear_axis[2] = (int)shear_z;
	rt.shear.x = -ray->direction.v[shear_x] / ray->direction.v[shear_z];
	rt.shear.y = -ray->direction.v[shear_y] / ray->direction.v[shear_z];
	rt.shear.z = 1.0f / ray->direction.v[shear_z];
	rt.shear_origin.x = ray->origin.v[shear_x];
	rt.shear_origin.y = ray->origin.v[shear_y];
	rt.shear_origin.z = ray->origin.v[shear_z];
	rt.sign_mask = sign_mask;

#if RTK_SSE
	rt.sse_origin = _mm_loadu_ps(ray->origin.v);
	__m128 sse_dir = _mm_loadu_ps(ray->direction.v);
	// Remove the last lane in case `min_t` is 0 to avoid unnecessary `inf` in `sse_rcp_dir`.
	sse_dir = _mm_shuffle_ps(sse_dir, sse_dir, _MM_SHUFFLE(2,2,1,0));
	rt.sse_rcp_dir = SSE_RCP(sse_dir);

	rt.sse_shear_shuf = _mm_setr_epi32((int)shuf_u32_tab[shear_x], (int)shuf_u32_tab[shear_y], (int)shuf_u32_tab[shear_z], -1);

#else
	rt.rcp_dir.x = 1.0f / ray->direction.x;
	rt.rcp_dir.y = 1.0f / ray->direction.y;
	rt.rcp_dir.z = 1.0f / ray->direction.z;
#endif

	bvh_traverse(&rt, &s->root);

	if (rt.hit.t < max_t) {
		rt.hit.geom.normal = v_normalize(rt.hit.geom.normal);

		if (rt.mesh_ix != ~0u) {
			const mesh_data *mesh = &s->meshes[rt.mesh_ix];
			rt.hit.object = mesh->object;
			uint32_t i0 = rt.hit.vertex_index[0];
			uint32_t i1 = rt.hit.vertex_index[1];
			uint32_t i2 = rt.hit.vertex_index[2];
			rtk_real u = rt.hit.geom.u, v = rt.hit.geom.v;
			rtk_real w = 1.0f - u - v;

			// Calculate interpolated UV coordinates and derivatives if necessary
			if (mesh->uvs) {
				const rtk_vec2 *uvs = mesh->uvs;
				rtk_vec2 t0 = uvs[i0], t1 = uvs[i1], t2 = uvs[i2];
				rtk_vec3 e0 = rt.hit.geom.dp_du;
				rtk_vec3 e1 = rt.hit.geom.dp_dv;

				// Invert the UV edge matrix
				rtk_real e0u = t1.x - t0.x, e0v = t1.y - t0.y;
				rtk_real e1u = t2.x - t0.x, e1v = t2.y - t0.y;
				rtk_real rcp_det = 1.0f / (e0u*e1v - e0v*e1u);
				rtk_real m00 = +e1v*rcp_det, m01 = -e0v*rcp_det;
				rtk_real m10 = -e1u*rcp_det, m11 = +e0u*rcp_det;

				rtk_real abs_rcp_det = rtk_abs(rcp_det);
				if (abs_rcp_det > 1e-18f && abs_rcp_det < RTK_INF) {
					rt.hit.interp.u = u*t0.x + v*t1.x + w*t2.x;
					rt.hit.interp.v = u*t0.y + v*t1.y + w*t2.y;
					rt.hit.interp.dp_du.x = e0.x*m00 + e1.x*m01;
					rt.hit.interp.dp_du.y = e0.y*m00 + e1.y*m01;
					rt.hit.interp.dp_du.z = e0.z*m00 + e1.z*m01;
					rt.hit.interp.dp_dv.x = e0.x*m10 + e1.x*m11;
					rt.hit.interp.dp_dv.y = e0.y*m10 + e1.y*m11;
					rt.hit.interp.dp_dv.z = e0.z*m10 + e1.z*m11;
				} else {
					rt.hit.interp.u = rt.hit.geom.u;
					rt.hit.interp.v = rt.hit.geom.v;
					rt.hit.interp.dp_du = rt.hit.geom.dp_du;
					rt.hit.interp.dp_dv = rt.hit.geom.dp_dv;
				}
			} else {
				rt.hit.interp.u = rt.hit.geom.u;
				rt.hit.interp.v = rt.hit.geom.v;
				rt.hit.interp.dp_du = rt.hit.geom.dp_du;
				rt.hit.interp.dp_dv = rt.hit.geom.dp_dv;
			}

			// Calculate interpolated normal if necessary
			if (mesh->normals) {
				const rtk_vec3 *normals = mesh->normals;
				const rtk_vec3 *n0 = &normals[i0], *n1 = &normals[i1], *n2 = &normals[i2];
				rtk_vec3 n;
				n.x = n0->x*u + n1->x*v + n2->x*w;
				n.y = n0->y*u + n1->y*v + n2->y*w;
				n.z = n0->z*u + n1->z*v + n2->z*w;
				rt.hit.interp.normal = v_normalize(n);
			} else {
				rt.hit.interp.normal = rt.hit.geom.normal;
			}
		} else {
			rt.hit.interp.normal = v_normalize(rt.hit.interp.normal);
		}

		*hit = rt.hit;
		return 1;
	} else {
		return 0;
	}
}

void rtk_raytrace_many(const rtk_scene *s, const rtk_ray *rays, rtk_hit *hits, size_t num)
{
#if 1
	for (size_t i = 0; i < num; i++) {
		rtk_raytrace(s, &rays[i], &hits[i], hits[i].t);
	}
#else
	SSE_ALIGN raytrace rts[MAX_CONCURRENT_RAYS];
	for (size_t begin = 0; begin < num; begin += MAX_CONCURRENT_RAYS) {
		size_t left = num - begin;
		if (left > MAX_CONCURRENT_RAYS) left = MAX_CONCURRENT_RAYS;

		for (size_t i = 0; i < left; i++) {
			size_t ix = begin + i;
			raytrace *rt = &rts[i];
			const rtk_ray *ray = &rays[ix];
			rt->ray = *ray;
			rt->mesh_ix = ~0u;

			rt->hit.t = hits[ix].t;

			rtk_vec3 abs = v_abs(ray->direction);
			rtk_real max_comp = real_max(real_max(abs.x, abs.y), abs.z);
			unsigned shear_z = abs.x == max_comp ? 0 : abs.y == max_comp ? 1 : 2;
			unsigned shear_x = (shear_z + 1) % 3u;
			unsigned shear_y = (shear_z + 2) % 3u;
			uint32_t sign_mask = 0;

		#if RTK_SSE
			__m128 zero = _mm_setzero_ps();
			sign_mask |= _mm_comilt_ss(_mm_set_ss(ray->direction.x), zero);
			sign_mask |= _mm_comilt_ss(_mm_set_ss(ray->direction.y), zero) << 1;
			sign_mask |= _mm_comilt_ss(_mm_set_ss(ray->direction.z), zero) << 2;
		#else
			sign_mask |= ray->direction.x < 0.0f ? 1 : 0;
			sign_mask |= ray->direction.y < 0.0f ? 2 : 0;
			sign_mask |= ray->direction.z < 0.0f ? 4 : 0;
		#endif

			rt->shear_axis[0] = (int)shear_x;
			rt->shear_axis[1] = (int)shear_y;
			rt->shear_axis[2] = (int)shear_z;
			rt->shear.x = -ray->direction.v[shear_x] / ray->direction.v[shear_z];
			rt->shear.y = -ray->direction.v[shear_y] / ray->direction.v[shear_z];
			rt->shear.z = 1.0f / ray->direction.v[shear_z];
			rt->shear_origin.x = ray->origin.v[shear_x];
			rt->shear_origin.y = ray->origin.v[shear_y];
			rt->shear_origin.z = ray->origin.v[shear_z];
			rt->sign_mask = sign_mask;

		#if RTK_SSE
			rt->sse_origin = _mm_loadu_ps(ray->origin.v);
			__m128 sse_dir = _mm_loadu_ps(ray->direction.v);
			// Remove the last lane in case `min_t` is 0 to avoid unnecessary `inf` in `sse_rcp_dir`.
			sse_dir = _mm_shuffle_ps(sse_dir, sse_dir, _MM_SHUFFLE(2,2,1,0));
			rt->sse_rcp_dir = SSE_RCP(sse_dir);
		#else
			rt->rcp_dir.x = 1.0f / ray->direction.x;
			rt->rcp_dir.y = 1.0f / ray->direction.y;
			rt->rcp_dir.z = 1.0f / ray->direction.z;
		#endif
		}

		bvh_traverse_many(rts, left, &s->root);

		for (size_t i = 0; i < left; i++) {
			uint32_t ix = begin + i;
			raytrace *rt = &rts[i];
			if (rt->hit.t >= hits[ix].t) continue;

			rt->hit.geom.normal = v_normalize(rt->hit.geom.normal);

			if (rt->mesh_ix != ~0u) {
				const mesh_data *mesh = &s->meshes[rt->mesh_ix];
				rt->hit.object = mesh->object;
				uint32_t i0 = rt->hit.vertex_index[0];
				uint32_t i1 = rt->hit.vertex_index[1];
				uint32_t i2 = rt->hit.vertex_index[2];
				rtk_real u = rt->hit.geom.u, v = rt->hit.geom.v;
				rtk_real w = 1.0f - u - v;

				// Calculate interpolated UV coordinates and derivatives if necessary
				if (mesh->uvs) {
					const rtk_vec2 *uvs = mesh->uvs;
					rtk_vec2 t0 = uvs[i0], t1 = uvs[i1], t2 = uvs[i2];
					rtk_vec3 e0 = rt->hit.geom.dp_du;
					rtk_vec3 e1 = rt->hit.geom.dp_dv;

					// Invert the UV edge matrix
					rtk_real e0u = t1.x - t0.x, e0v = t1.y - t0.y;
					rtk_real e1u = t2.x - t0.x, e1v = t2.y - t0.y;
					rtk_real rcp_det = 1.0f / (e0u*e1v - e0v*e1u);
					rtk_real m00 = +e1v*rcp_det, m01 = -e0v*rcp_det;
					rtk_real m10 = -e1u*rcp_det, m11 = +e0u*rcp_det;

					rtk_real abs_rcp_det = rtk_abs(rcp_det);
					if (abs_rcp_det > 1e-18f && abs_rcp_det < RTK_INF) {
						rt->hit.interp.u = u*t0.x + v*t1.x + w*t2.x;
						rt->hit.interp.v = u*t0.y + v*t1.y + w*t2.y;
						rt->hit.interp.dp_du.x = e0.x*m00 + e1.x*m01;
						rt->hit.interp.dp_du.y = e0.y*m00 + e1.y*m01;
						rt->hit.interp.dp_du.z = e0.z*m00 + e1.z*m01;
						rt->hit.interp.dp_dv.x = e0.x*m10 + e1.x*m11;
						rt->hit.interp.dp_dv.y = e0.y*m10 + e1.y*m11;
						rt->hit.interp.dp_dv.z = e0.z*m10 + e1.z*m11;
					} else {
						rt->hit.interp.u = rt->hit.geom.u;
						rt->hit.interp.v = rt->hit.geom.v;
						rt->hit.interp.dp_du = rt->hit.geom.dp_du;
						rt->hit.interp.dp_dv = rt->hit.geom.dp_dv;
					}
				} else {
					rt->hit.interp.u = rt->hit.geom.u;
					rt->hit.interp.v = rt->hit.geom.v;
					rt->hit.interp.dp_du = rt->hit.geom.dp_du;
					rt->hit.interp.dp_dv = rt->hit.geom.dp_dv;
				}

				// Calculate interpolated normal if necessary
				if (mesh->normals) {
					const rtk_vec3 *normals = mesh->normals;
					const rtk_vec3 *n0 = &normals[i0], *n1 = &normals[i1], *n2 = &normals[i2];
					rtk_vec3 n;
					n.x = n0->x*u + n1->x*v + n2->x*w;
					n.y = n0->y*u + n1->y*v + n2->y*w;
					n.z = n0->z*u + n1->z*v + n2->z*w;
					rt->hit.interp.normal = v_normalize(n);
				} else {
					rt->hit.interp.normal = rt->hit.geom.normal;
				}
			} else {
				rt->hit.interp.normal = v_normalize(rt->hit.interp.normal);
			}

			hits[ix] = rt->hit;
		}
	}
#endif
}

size_t rtk_used_memory(const rtk_scene *s)
{
	return s->memory_size;
}

rtk_bounds rtk_scene_bounds(const rtk_scene *s)
{
	return s->bounds;
}

rtk_bvh rtk_get_bvh(const rtk_scene *s, uintptr_t index)
{
	rtk_assert(index != UINTPTR_MAX);
	rtk_bvh result;
	if (index == 0) {
		uintptr_t ptr = (uintptr_t)&s->root;
		result.bounds = s->bounds;
		result.child[0] = ptr | 0;
		result.child[1] = ptr | 1;
		result.child[2] = ptr | 2;
		result.child[3] = ptr | 3;
		result.leaf = 0;
	} else {
		unsigned side = (unsigned)(index & 3);
		bvh_node *node = (bvh_node*)(index ^ side);
		result.bounds.min.x = node->bounds_x[0][side];
		result.bounds.min.y = node->bounds_y[0][side];
		result.bounds.min.z = node->bounds_z[0][side];
		result.bounds.max.x = node->bounds_x[1][side];
		result.bounds.max.y = node->bounds_y[1][side];
		result.bounds.max.z = node->bounds_z[1][side];
		bvh_ptr child_ptr = node->ptr[side];
		if (bp_is_node(child_ptr)) {
			uintptr_t ptr = (uintptr_t)bp_node(child_ptr);
			result.child[0] = ptr;
			result.child[1] = ptr | 1;
			result.child[2] = ptr | 2;
			result.child[3] = ptr | 3;
			result.leaf = 0;
		} else {
			result.child[0] = UINTPTR_MAX;
			result.child[1] = UINTPTR_MAX;
			result.child[2] = UINTPTR_MAX;
			result.child[3] = UINTPTR_MAX;
			result.leaf = (uintptr_t)bp_leaf(child_ptr);
		}
	}
	return result;
}

void rtk_get_leaf(const rtk_scene *s, uintptr_t index, rtk_leaf *dst_leaf)
{
	(void)s;
	const bvh_leaf *leaf = (const bvh_leaf*)index;
	uint32_t num_tris_aligned = align_up_u32(leaf->num_tris, 4);
	const leaf_tri *tris = (const leaf_tri*)(leaf + 1);
	const leaf_prim *prims = (const leaf_prim*)(tris + num_tris_aligned);
	const rtk_object *tri_objs = (const rtk_object*)prims + leaf->num_prims;
	const leaf_vert *verts = leaf->vertices;

	dst_leaf->num_triangles = leaf->num_tris;
	dst_leaf->num_primitives = leaf->num_prims;

	rtk_leaf_triangle *dst_tri = dst_leaf->triangles;
	rtk_for(const leaf_tri, tri, tris, leaf->num_tris) {
		const leaf_vert *a = &verts[tri->v[0]], *b = &verts[tri->v[1]], *c = &verts[tri->v[2]];
		dst_tri->v[0] = a->pos;
		dst_tri->v[1] = b->pos;
		dst_tri->v[2] = c->pos;
		dst_tri->index[0] = a->index;
		dst_tri->index[1] = b->index;
		dst_tri->index[2] = c->index;
		dst_tri->object = tri_objs[tri->obj_ix];
		dst_tri++;
	}

	rtk_primitive *dst_prim = dst_leaf->primitives;
	rtk_for(const leaf_prim, prim, prims, leaf->num_prims) {
		*dst_prim++ = prim->prim;
	}
}

void rtk_init_subscene(rtk_primitive *p, const rtk_scene *scene, const rtk_matrix *transform)
{
	p->bounds = scene->bounds;
	p->intersect_fn = &isect_prim_scene;
	p->user = (void*)scene;

	if (transform) {
		p->transform = *transform;
	} else {
		p->transform = rtk_identity;
	}
}

void rtk_init_sphere(rtk_primitive *p, rtk_vec3 origin, rtk_real radius, const rtk_matrix *transform)
{
	p->intersect_fn = &isect_prim_sphere;

	p->bounds.min = v_make(-1.0f, -1.0f, -1.0f);
	p->bounds.max = v_make(+1.0f, +1.0f, +1.0f);

	p->transform.cols[0] = v_make(radius, 0.0f, 0.0f);
	p->transform.cols[1] = v_make(0.0f, radius, 0.0f);
	p->transform.cols[2] = v_make(0.0f, 0.0f, radius);
	p->transform.cols[3] = origin;

	if (transform) {
		mat_mul_left(&p->transform, transform);
	}
}

void rtk_init_plane(rtk_primitive *p, rtk_vec3 normal, rtk_real d, const rtk_matrix *transform)
{
	p->intersect_fn = &isect_prim_plane;

	p->bounds.min = v_make(0.0f, -RTK_INF, -RTK_INF);
	p->bounds.max = v_make(0.0f, +RTK_INF, +RTK_INF);

	normal = v_normalize(normal);
	rtk_vec3 right = fabs(normal.x) < 0.5f ? v_make(1.0f, 0.0f, 0.0f) : v_make(0.0f, 1.0f, 0.0f);
	rtk_vec3 up = v_normalize(v_cross(normal, right));
	right = v_normalize(v_cross(normal, up));

	p->transform.cols[0] = normal;
	p->transform.cols[1] = up;
	p->transform.cols[2] = right;
	p->transform.cols[3] = v_mul(normal, d);

	if (transform) {
		mat_mul_left(&p->transform, transform);
	}
}

#endif