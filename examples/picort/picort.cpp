#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

#include "../../ufbx.h"

#ifndef GUI
#define GUI 0
#endif

#ifndef SSE
#define SSE 0
#endif

#if GUI
	#define NOMINMAX
	#include <Windows.h>
#endif

#include <xmmintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <immintrin.h>

#if SSE
	#ifdef _MSC_VER
	#include <intrin.h>
	#else
	#include <x86intrin.h>
	#endif
#endif

#if defined(_MSC_VER)
	#define picort_forceinline inline __forceinline
#elif defined(__GNUC__)
	#define picort_forceinline inline __attribute__((always_inline))
#else
	#define picort_forceinline inline
#endif

// -- Global options

bool g_verbose = false;

static void verbosef(const char *fmt, ...)
{
	if (!g_verbose) return;

	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

// -- Generic math definitions

using std::sin;
using std::cos;
using std::atan2;
using std::acos;
using std::asin;
using std::abs;

using Real = float;
static const constexpr Real Inf = INFINITY;
static const constexpr Real Pi = (Real)3.14159265359;

struct Vec2
{
	Real x = 0.0f, y = 0.0f;

	picort_forceinline Vec2 operator-() const { return { -x, -y }; };
	picort_forceinline Vec2 operator+(const Vec2 &b) const { return { x + b.x, y + b.y }; }
	picort_forceinline Vec2 operator-(const Vec2 &b) const { return { x - b.x, y - b.y }; }
	picort_forceinline Vec2 operator*(const Vec2 &b) const { return { x * b.x, y * b.y }; }
	picort_forceinline Vec2 operator/(const Vec2 &b) const { return { x / b.x, y / b.y }; }
	picort_forceinline Vec2 operator*(Real b) const { return { x * b, y * b }; }
	picort_forceinline Vec2 operator/(Real b) const { return { x / b, y / b }; }

	picort_forceinline Real operator[](int axis) const { return (&x)[axis]; }
};

struct Vec3
{
	Real x = 0.0f, y = 0.0f, z = 0.0f;

	picort_forceinline Vec3 operator-() const { return { -x, -y, -z }; };
	picort_forceinline Vec3 operator+(const Vec3 &b) const { return { x + b.x, y + b.y, z + b.z }; }
	picort_forceinline Vec3 operator-(const Vec3 &b) const { return { x - b.x, y - b.y, z - b.z }; }
	picort_forceinline Vec3 operator*(const Vec3 &b) const { return { x * b.x, y * b.y, z * b.z }; }
	picort_forceinline Vec3 operator/(const Vec3 &b) const { return { x / b.x, y / b.y, z / b.z }; }
	picort_forceinline Vec3 operator*(Real b) const { return { x * b, y * b, z * b }; }
	picort_forceinline Vec3 operator/(Real b) const { return { x / b, y / b, z / b }; }

	picort_forceinline Real operator[](int axis) const { return (&x)[axis]; }

	picort_forceinline bool operator==(const Vec3 &rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
	picort_forceinline bool operator!=(const Vec3 &rhs) const { return x != rhs.x || y != rhs.y || z != rhs.z; }
};

#if SSE
picort_forceinline Real min(Real a, Real b) { return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a), _mm_set_ss(b))); }
picort_forceinline Real max(Real a, Real b) { return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(b))); }
picort_forceinline Real floor_real(Real a) { return _mm_cvtss_f32(_mm_floor_ss(_mm_setzero_ps(), _mm_set_ss(a))); }
picort_forceinline Real ceil_real(Real a) { return _mm_cvtss_f32(_mm_ceil_ss(_mm_setzero_ps(), _mm_set_ss(a))); }
#else
picort_forceinline Real min(Real a, Real b) { return a < b ? a : b; }
picort_forceinline Real max(Real a, Real b) { return b < a ? a : b; }
picort_forceinline Real floor_real(Real a) { return std::floor(a); }
picort_forceinline Real ceil_real(Real a) { return std::ceil(a); }
#endif

picort_forceinline Vec2 min(const Vec2 &a, const Vec2 &b) { return { min(a.x, b.x), min(a.y, b.y) }; }
picort_forceinline Vec2 max(const Vec2 &a, const Vec2 &b) { return { max(a.x, b.x), max(a.y, b.y) }; }

picort_forceinline Vec3 min(const Vec3 &a, const Vec3 &b) { return { min(a.x, b.x), min(a.y, b.y), min(a.z, b.z) }; }
picort_forceinline Vec3 max(const Vec3 &a, const Vec3 &b) { return { max(a.x, b.x), max(a.y, b.y), max(a.z, b.z) }; }

picort_forceinline Real min_component(const Vec2 &a) { return min(min(a.x, a.y), +Inf); }
picort_forceinline Real max_component(const Vec2 &a) { return max(max(a.x, a.y), -Inf); }

picort_forceinline Real min_component(const Vec3 &a) { return min(min(min(a.x, a.y), a.z), +Inf); }
picort_forceinline Real max_component(const Vec3 &a) { return max(max(max(a.x, a.y), a.z), -Inf); }

picort_forceinline Vec2 abs(const Vec2 &a) { return { abs(a.x), abs(a.y) }; }
picort_forceinline Vec3 abs(const Vec3 &a) { return { abs(a.x), abs(a.y), abs(a.z) }; }

picort_forceinline Real dot(const Vec2 &a, const Vec2 &b) { return a.x*b.x + a.y*b.y; }
picort_forceinline Real dot(const Vec3 &a, const Vec3 &b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
picort_forceinline Vec3 cross(const Vec3 &a, const Vec3 &b) {
	return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}

picort_forceinline Real length(const Vec2 &a) { return sqrt(dot(a, a)); }
picort_forceinline Real length(const Vec3 &a) { return sqrt(dot(a, a)); }

picort_forceinline Vec2 normalize(const Vec2 &a) { return a / length(a); }
picort_forceinline Vec3 normalize(const Vec3 &a) { return a / length(a); }

picort_forceinline Real lerp(Real a, Real b, Real t) { return a * (1.0f - t) + b * t; }
picort_forceinline Vec2 lerp(const Vec2 &a, const Vec2 &b, Real t) { return a * (1.0f - t) + b * t; }
picort_forceinline Vec3 lerp(const Vec3 &a, const Vec3 &b, Real t) { return a * (1.0f - t) + b * t; }

picort_forceinline int largest_axis(const Vec3 &v) {
	Real m = max_component(v);
	return v.x == m ? 0 : v.y == m ? 1 : 2;
}

picort_forceinline Vec3 reflect(const Vec3 &n, const Vec3 &v) { return -v + n * (2.0f * dot(n, v)); }

picort_forceinline Real sqrt_safe(Real a) { return std::sqrt(a); }

picort_forceinline Real clamp(Real a, Real min_v, Real max_v) { return min(max(a, min_v), max_v); }

// -- SIMD

#if SSE

struct Real4
{
	__m128 v;

	struct Mask {
		__m128 v;

		Mask() { }
		Mask(__m128 v) : v(v) { }
		Mask(bool x, bool y, bool z, bool w) : v(_mm_castsi128_ps(_mm_setr_epi32(-(int)x, -(int)y, -(int)z, -(int)w))) { }

		uint32_t mask() const { return _mm_movemask_ps(v); }
		uint32_t count() const { return _mm_popcnt_u32((unsigned)_mm_movemask_ps(v)); }
		bool any() const { return !_mm_test_all_zeros(_mm_castps_si128(v), _mm_castps_si128(v)); }
		bool all() const { return _mm_test_all_ones(_mm_castps_si128(v)); }

		Mask operator&(const Mask &rhs) const { return { _mm_and_ps(v, rhs.v) }; }
		Mask operator|(const Mask &rhs) const { return { _mm_or_ps(v, rhs.v) }; }
		Mask operator^(const Mask &rhs) const { return { _mm_xor_ps(v, rhs.v) }; }
		Mask operator~() const { return { _mm_xor_ps(v, _mm_castsi128_ps(_mm_set1_epi32(-1))) }; }
	};

	struct Index {
		__m128i v;

		Index() { }
		Index(uint32_t x, uint32_t y, uint32_t z, uint32_t w) : v(_mm_setr_epi32(
			(int)(x * 0x04040404u + 0x03020100u), (int)(y * 0x04040404u + 0x03020100u),
			(int)(z * 0x04040404u + 0x03020100u), (int)(w * 0x04040404u + 0x03020100u))) { }
	};

	Real4() { }
	Real4(Real v) : v(_mm_set1_ps(v)) { }
	Real4(Real x, Real y, Real z, Real w) : v(_mm_setr_ps(x, y, z, w)) { }
	Real4(__m128 v) : v(v) { }

	picort_forceinline Real4 operator+(const Real4 &rhs) const { return { _mm_add_ps(v, rhs.v) }; }
	picort_forceinline Real4 operator-(const Real4 &rhs) const { return { _mm_sub_ps(v, rhs.v) }; }
	picort_forceinline Real4 operator*(const Real4 &rhs) const { return { _mm_mul_ps(v, rhs.v) }; }

	picort_forceinline Mask operator==(const Real4 &rhs) const { return { _mm_cmpeq_ps(v, rhs.v) }; }
	picort_forceinline Mask operator!=(const Real4 &rhs) const { return { _mm_cmpneq_ps(v, rhs.v) }; }
	picort_forceinline Mask operator<(const Real4 &rhs) const { return { _mm_cmplt_ps(v, rhs.v) }; }
	picort_forceinline Mask operator<=(const Real4 &rhs) const { return { _mm_cmple_ps(v, rhs.v) }; }
	picort_forceinline Mask operator>=(const Real4 &rhs) const { return { _mm_cmpge_ps(v, rhs.v) }; }
	picort_forceinline Mask operator>(const Real4 &rhs) const { return { _mm_cmpgt_ps(v, rhs.v) }; }

	picort_forceinline Real get(uint32_t ix) const { return _mm_cvtss_f32(_mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(v), _mm_cvtsi32_si128((int)(ix * 0x04040404u + 0x03020100u))))); }

	picort_forceinline Real x() const { return _mm_cvtss_f32(v); }
	picort_forceinline Real y() const { return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3,2,1,1))); }
	picort_forceinline Real z() const { return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3,2,1,2))); }
	picort_forceinline Real w() const { return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3,2,1,3))); }

	picort_forceinline Real4 xs() const { return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(0,0,0,0))); }
	picort_forceinline Real4 ys() const { return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1,1,1,1))); }
	picort_forceinline Real4 zs() const { return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2,2,2,2))); }
	picort_forceinline Real4 ws() const { return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3,3,3,3))); }

	picort_forceinline Real min() const {
		__m128 t = _mm_min_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1,0,3,2)));
		return _mm_cvtss_f32(_mm_min_ps(t, _mm_shuffle_ps(t, t, _MM_SHUFFLE(2,3,0,1))));
	}
	picort_forceinline Real max() const {
		__m128 t = _mm_max_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1,0,3,2)));
		return _mm_cvtss_f32(_mm_max_ps(t, _mm_shuffle_ps(t, t, _MM_SHUFFLE(2,3,0,1))));
	}

	static picort_forceinline Real4 min(const Real4 &a, const Real4 &b) { return { _mm_min_ps(a.v, b.v) }; }
	static picort_forceinline Real4 max(const Real4 &a, const Real4 &b) { return { _mm_max_ps(a.v, b.v) }; }

	static picort_forceinline Real4 load(const Real *ptr) { return { _mm_loadu_ps(ptr) }; }
	static picort_forceinline Real4 load_shuffle(const Real *ptr, const Index &index) { return { _mm_castsi128_ps(_mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)ptr), index.v)) }; }
	static picort_forceinline Real4 load_u16(const uint16_t *ptr) { return _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_loadl_epi64((const __m128i*)ptr))); }
	static picort_forceinline Real4 load_i16(const int16_t *ptr) { return _mm_cvtepi32_ps(_mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i*)ptr))); }
	static picort_forceinline void store(Real *ptr, const Real4 &a) { _mm_storeu_ps(ptr, a.v); }

	static picort_forceinline Real4 select(const Mask &mask, const Real4 &a, const Real4 &b) { return { _mm_blendv_ps(b.v, a.v, mask.v) }; }
	static picort_forceinline Real4 shuffle(const Real4 &a, const Index &index) { return { _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(a.v), index.v)) }; }
	static picort_forceinline void transpose(Real4 &a, Real4 &b, Real4 &c, Real4 &d) { _MM_TRANSPOSE4_PS(a.v, b.v, c.v, d.v); }
};

#else

struct Real4
{
	Real x_, y_, z_, w_;

	struct Mask {
		int x_, y_, z_, w_;

		int mask() const { return (int)x_ | (int)y_<<1 | (int)z_<<2 | (int)w_<<3; }
		int count() const { return (int)x_ + (int)y_ + (int)z_ + (int)w_; }
		bool any() const { return x_ | y_ | z_ | w_; }
		bool all() const { return x_ & y_ & z_ & w_; }

		Mask operator&(const Mask &rhs) const { return { x_&rhs.x_, y_&rhs.y_, z_&rhs.z_, w_&rhs.w_ }; }
		Mask operator|(const Mask &rhs) const { return { x_|rhs.x_, y_|rhs.y_, z_|rhs.z_, w_|rhs.w_}; }
		Mask operator^(const Mask &rhs) const { return { x_^rhs.x_, y_^rhs.y_, z_^rhs.z_, w_^rhs.w_ }; }
		Mask operator~() const { return { !x_, !y_, !z_, !w_ }; }
	};

	struct Index {
		uint8_t x_, y_, z_, w_;

		Index() { }
		Index(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
			: x_((uint8_t)x), y_((uint8_t)y), z_((uint8_t)z), w_((uint8_t)w) { }
	};

	Real4() { }
	Real4(Real v) : x_(v), y_(v), z_(v), w_(v) { }
	Real4(Real x, Real y, Real z, Real w) : x_(x), y_(y), z_(z), w_(w) { }

	picort_forceinline Real4 operator+(const Real4 &rhs) const { return { x_+rhs.x_, y_+rhs.y_, z_+rhs.z_, w_+rhs.w_ }; }
	picort_forceinline Real4 operator-(const Real4 &rhs) const { return { x_-rhs.x_, y_-rhs.y_, z_-rhs.z_, w_-rhs.w_ }; }
	picort_forceinline Real4 operator*(const Real4 &rhs) const { return { x_*rhs.x_, y_*rhs.y_, z_*rhs.z_, w_*rhs.w_ }; }

	picort_forceinline Mask operator==(const Real4 &rhs) const { return { x_==rhs.x_, y_==rhs.y_, z_==rhs.z_, w_==rhs.w_ }; }
	picort_forceinline Mask operator!=(const Real4 &rhs) const { return { x_!=rhs.x_, y_!=rhs.y_, z_!=rhs.z_, w_!=rhs.w_ }; }
	picort_forceinline Mask operator<(const Real4 &rhs) const { return { x_<rhs.x_, y_<rhs.y_, z_<rhs.z_, w_<rhs.w_ }; }
	picort_forceinline Mask operator<=(const Real4 &rhs) const { return { x_<=rhs.x_, y_<=rhs.y_, z_<=rhs.z_, w_<=rhs.w_ }; }
	picort_forceinline Mask operator>=(const Real4 &rhs) const { return { x_>=rhs.x_, y_>=rhs.y_, z_>=rhs.z_, w_>=rhs.w_ }; }
	picort_forceinline Mask operator>(const Real4 &rhs) const { return { x_>rhs.x_, y_>rhs.y_, z_>rhs.z_, w_>rhs.w_ }; }

	picort_forceinline Real get(uint32_t ix) const {
		Real v[] = { x_, y_, z_, w_ };
		return v[ix];
	};

	picort_forceinline Real x() const { return x_; }
	picort_forceinline Real y() const { return y_; }
	picort_forceinline Real z() const { return z_; }
	picort_forceinline Real w() const { return w_; }

	picort_forceinline Real4 xs() const { return { x_, x_, x_, x_ }; }
	picort_forceinline Real4 ys() const { return { y_, y_, y_, y_ }; }
	picort_forceinline Real4 zs() const { return { z_, z_, z_, z_ }; }
	picort_forceinline Real4 ws() const { return { w_, w_, w_, w_ }; }

	picort_forceinline Real min() const { return ::min(::min(x_, y_), ::min(z_, w_)); }
	picort_forceinline Real max() const { return ::max(::max(x_, y_), ::max(z_, w_)); }

	static picort_forceinline Real4 min(const Real4 &a, const Real4 &b) { return { ::min(a.x_, b.x_), ::min(a.y_, b.y_), ::min(a.z_, b.z_), ::min(a.w_, b.w_) }; }
	static picort_forceinline Real4 max(const Real4 &a, const Real4 &b) { return { ::max(a.x_, b.x_), ::max(a.y_, b.y_), ::max(a.z_, b.z_), ::max(a.w_, b.w_) }; }

	static picort_forceinline Real4 load(const Real *ptr) { return { ptr[0], ptr[1], ptr[2], ptr[3] }; }
	static picort_forceinline Real4 load_u16(const uint16_t *ptr) { return { (Real)ptr[0], (Real)ptr[1], (Real)ptr[2], (Real)ptr[3] }; }
	static picort_forceinline Real4 load_i16(const int16_t *ptr) { return { (Real)ptr[0], (Real)ptr[1], (Real)ptr[2], (Real)ptr[3] }; }
	static picort_forceinline Real4 load_shuffle(const Real *ptr, const Index &index) { return { ptr[index.x_], ptr[index.y_], ptr[index.z_], ptr[index.w_] }; }
	static picort_forceinline void store(Real *ptr, const Real4 &a) { ptr[0] = a.x_; ptr[1] = a.y_; ptr[2] = a.z_; ptr[3] = a.w_; }

	static picort_forceinline Real4 select(const Mask &mask, const Real4 &a, const Real4 &b) {
		return { mask.x_ ? a.x_ : b.x_, mask.y_ ? a.y_ : b.y_, mask.z_ ? a.z_ : b.z_, mask.w_ ? a.w_ : b.w_ };
	}

	static picort_forceinline Real4 shuffle(const Real4 &a, const Index &index) {
		Real arr[4] = { a.x_, a.y_, a.z_, a.w_ };
		return { arr[index.x_], arr[index.y_], arr[index.z_], arr[index.w_] };
	}

	static picort_forceinline void transpose(Real4 &a, Real4 &b, Real4 &c, Real4 &d) {
		Real t;
		t = a.y_; a.y_ = b.x_; b.x_ = t;
		t = a.z_; a.z_ = c.x_; c.x_ = t;
		t = a.w_; a.w_ = d.x_; d.x_ = t;
		t = b.z_; b.z_ = c.y_; c.y_ = t;
		t = b.w_; b.w_ = d.y_; d.y_ = t;
		t = c.w_; c.w_ = d.z_; d.z_ = t;
	}

	static picort_forceinline int min_component_index(Real4 a) {
		if (::min(a.x_, a.y_) < ::min(a.z_, a.w_)) {
			return a.x_ < a.y_ ? 0 : 1;
		} else {
			return 2 + (a.z_ < a.w_ ? 0 : 1);
		}
	}
};

#endif

// -- Some mask utilities

#if SSE
picort_forceinline uint32_t popcount4(uint32_t v) { return _mm_popcnt_u32(v); }
picort_forceinline uint32_t first_set4(uint32_t v) { return _tzcnt_u32(v); }
#else
static uint8_t pop4_table[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
static uint8_t ffs4_table[16] = { 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 };
picort_forceinline uint32_t popcount4(uint32_t v) { return pop4_table[v]; }
picort_forceinline uint32_t first_set4(uint32_t v) { return ffs4_table[v]; }
#endif

// -- Prefetch

#if SSE
	#define picort_prefetch(ptr) _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
#else
	#define picort_prefetch(ptr) (void)0
#endif

// -- Random series

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
struct Random
{
	uint64_t state = 1, inc = 1;

	uint32_t next() {
		uint64_t oldstate = state;
		// Advance internal state
		state = oldstate * 6364136223846793005ULL + inc;
		// Calculate output function (XSH RR), uses old state for max ILP
		uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
		uint32_t rot = oldstate >> 59u;
		return (xorshifted >> rot) | (xorshifted << ((0-rot) & 31));
	}
};

Real uniform_real(Random &rng) { return (Real)rng.next() * (Real)2.3283064e-10; }
Vec2 uniform_vec2(Random &rng) { return { uniform_real(rng), uniform_real(rng) }; }
Vec3 uniform_vec3(Random &rng) { return { uniform_real(rng), uniform_real(rng), uniform_real(rng) }; }

// -- Images

struct alignas(4) Pixel {
	uint8_t r=0, g=0, b=0, a=0xff;
};

struct alignas(8) Pixel16 {
	uint16_t r=0, g=0, b=0, a=0xffff;

	bool operator==(const Pixel16 &p) const { return r==p.r && g==p.g && b==p.b && a==p.a; }
	bool operator!=(const Pixel16 &p) const { return !(*this == p); }
};

struct Image
{
	const char *error = nullptr;
	uint32_t width = 0, height = 0;
	std::vector<Pixel16> pixels;
	bool srgb = false;
};

picort_forceinline Real4 lerp(const Real4 &a, const Real4 &b, Real t)
{
	return a * (1.0f - t) + b * t;
}

// Fetch a single pixel from `image` at pixel coordinate `{x, y}`
picort_forceinline Real4 fetch_image(const Image &image, int32_t x, int32_t y)
{
	if (((image.width & (image.width - 1)) | (image.height & (image.height - 1))) == 0) {
		x &= image.width - 1;
		y &= image.height - 1;
	} else {
		x = ((x % image.width) + image.width) % image.width;
		y = ((y % image.height) + image.height) % image.height;
	}
	Pixel16 p = image.pixels[y * image.width + x];
	Real4 v = Real4::load_u16(&image.pixels[y * image.width + x].r);
	v = v * (Real)(1.0 / 65535.0);
	if (image.srgb) v = Real4::select(Real4::Mask{true, true, true, false}, v * v, v);
	return v;
}

// Bilinearly sample `image` at normalized coordinate `uv`
Real4 sample_image(const Image &image, const Vec2 &uv)
{
	if (image.width == 0 || image.height == 0) return { };
	Vec2 px = uv * Vec2{ (Real)image.width, -(Real)image.height };
	Real x = floor_real(px.x), y = floor_real(px.y), dx = px.x - x, dy = px.y - y;
	Real4 p00 = fetch_image(image, (int32_t)x + 0, (int32_t)y + 0);
	Real4 p10 = fetch_image(image, (int32_t)x + 1, (int32_t)y + 0);
	Real4 p01 = fetch_image(image, (int32_t)x + 0, (int32_t)y + 1);
	Real4 p11 = fetch_image(image, (int32_t)x + 1, (int32_t)y + 1);
	Real4 v = lerp(lerp(p00, p10, dx), lerp(p01, p11, dx), dy);
	return v;
}

// Load a PNG image file
// http://www.libpng.org/pub/png/spec/1.2/PNG-Contents.html
Image read_png(const void *data, size_t size)
{
	Image image;

	const uint8_t *ptr = (const uint8_t*)data, *end = ptr + size;
	uint8_t bit_depth = 0, pixel_values = 0, pixel_format;
	std::vector<uint8_t> deflate_data, src, dst;
	std::vector<Pixel16> palette;
	static const uint8_t lace_none[] = { 0,0,1,1, 0,0,0,0 };
	static const uint8_t lace_adam7[] = {
		0,0,8,8, 4,0,8,8, 0,4,4,8, 2,0,4,4, 0,2,2,4, 1,0,2,2, 0,1,1,2, 0,0,0,0, };
	uint32_t scale = 1;
	const uint8_t *lace = lace_none; // Interlacing pattern (x0,y0,dx,dy)
	Pixel16 trns = { 0, 0, 0, 0 }; // Transparent pixel value for RGB

	if (end - ptr < 8) return { "file header truncated" };
	if (memcmp(ptr, "\x89PNG\r\n\x1a\n", 8)) return { "bad file header" };
	ptr += 8;

	// Iterate chunks: gather IDAT into a single buffer
	for (;;) {
		if (end - ptr < 8) return { "chunk header truncated" };
		uint32_t chunk_len = ptr[0]<<24 | ptr[1]<<16 | ptr[2]<<8 | ptr[3];
		const uint8_t *tag = ptr + 4;
		ptr += 8;
		if ((uint32_t)(end - ptr) < chunk_len + 4) return { "chunk data truncated" };
		if (!memcmp(tag, "IHDR", 4) && chunk_len >= 13) {
			image.width  = ptr[0]<<24 | ptr[1]<<16 | ptr[2]<<8 | ptr[3];
			image.height = ptr[4]<<24 | ptr[5]<<16 | ptr[6]<<8 | ptr[7];
			bit_depth = ptr[8];
			pixel_format = ptr[9];
			switch (pixel_format) {
			case 0: pixel_values = 1; break;
			case 2: pixel_values = 3; break;
			case 3: pixel_values = 1; break;
			case 4: pixel_values = 2; break;
			case 6: pixel_values = 4; break;
			default: return { "unknown pixel format" };
			}
			for (uint32_t i = 0; i < 16 && pixel_format != 3; i += bit_depth) scale |= 1u << i;
			if (ptr[12] != 0) lace = lace_adam7;
			if (ptr[10] != 0 || ptr[11] != 0) return { "unknown settings" };
		} else if (!memcmp(tag, "IDAT", 4)) {
			deflate_data.insert(deflate_data.end(), ptr, ptr + chunk_len);
		} else if (!memcmp(tag, "PLTE", 4)) {
			for (size_t i = 0; i < chunk_len; i += 3) {
				palette.push_back({ (uint16_t)(ptr[i]*0x101), (uint16_t)(ptr[i+1]*0x101), (uint16_t)(ptr[i+2]*0x101) });
			}
		} else if (!memcmp(tag, "IEND", 4)) {
			break;
		} else if (!memcmp(tag, "tRNS", 4)) {
			if (pixel_format == 2 && chunk_len >= 6) {
				trns = {
					(uint16_t)((ptr[0]<<8 | ptr[1]) * scale),
					(uint16_t)((ptr[2]<<8 | ptr[3]) * scale),
					(uint16_t)((ptr[4]<<8 | ptr[5]) * scale) };
			} else if (pixel_format == 0 && chunk_len >= 2) {
				uint16_t v = (uint16_t)(ptr[0]<<8 | ptr[1]) * scale;
				trns = { v, v, v };
			} else if (pixel_format == 3) {
				for (size_t i = 0; i < chunk_len; i++) {
					if (i < palette.size()) palette[i].a = ptr[i] * 0x101;
				}
			}
		}
		ptr += chunk_len + 4; // Skip data and CRC
	}

	size_t bpp = (pixel_values * bit_depth + 7) / 8;
	size_t stride = (image.width * pixel_values * bit_depth + 7) / 8;
	if (image.width == 0 || image.height == 0) return { "bad image size" };
	src.resize(image.height * stride + image.height * (lace == lace_adam7 ? 14 : 1));
	dst.resize((image.height + 1) * (stride + bpp));
	image.pixels.resize(image.width * image.height);

	// Decompress the combined IDAT chunks
	ufbx_inflate_retain retain;
	retain.initialized = false;

	ufbx_inflate_input input = { 0 };
	input.total_size = deflate_data.size();
	input.data_size = deflate_data.size();
	input.data = deflate_data.data();

	ptrdiff_t res = ufbx_inflate(src.data(), src.size(), &input, &retain);
	if (res < 0) return { "deflate error" };
	uint8_t *sp = src.data(), *sp_end = sp + src.size();

	for (; lace[2]; lace += 4) {
		int32_t width = ((int32_t)image.width - lace[0] + lace[2] - 1) / lace[2];
		int32_t height = ((int32_t)image.height - lace[1] + lace[3] - 1) / lace[3];
		if (width <= 0 || height <= 0) continue;
		size_t lace_stride = (width * pixel_values * bit_depth + 7) / 8;
		if ((size_t)(sp_end - sp) < height * (1 + lace_stride)) return { "data truncated" };

		// Unfilter the scanlines
		ptrdiff_t dx = bpp, dy = stride + bpp;
		for (int32_t y = 0; y < height; y++) {
			uint8_t *dp = dst.data() + (stride + bpp) * (1 + y) + bpp;
			uint8_t filter = *sp++;
			for (int32_t x = 0; x < lace_stride; x++) {
				uint8_t s = *sp++, *d = dp++;
				switch (filter) {
				case 0: d[0] = s; break; // 6.2: No filter
				case 1: d[0] = d[-dx] + s; break; // 6.3: Sub (predict left)
				case 2: d[0] = d[-dy] + s; break; // 6.4: Up (predict top)
				case 3: d[0] = (d[-dx] + d[-dy]) / 2 + s; break; // 6.5: Average (top+left)
				case 4: { // 6.6: Paeth (choose closest of 3 to estimate)
					int32_t a = d[-dx], b = d[-dy], c = d[-dx - dy], p = a+b-c;
					int32_t pa = abs(p-a), pb = abs(p-b), pc = abs(p-c);
					if (pa <= pb && pa <= pc) d[0] = (uint8_t)(a + s);
					else if (pb <= pc) d[0] = (uint8_t)(b + s);
					else d[0] = (uint8_t)(c + s);
				} break;
				default: return { "unknown filter" };
				}
			}
		}

		// Expand to RGBA pixels
		for (int32_t y = 0; y < height; y++) {
			uint8_t *dr = dst.data() + (stride + bpp) * (y + 1) + bpp;
			for (int32_t x = 0; x < width; x++) {
				uint16_t v[4];
				for (uint32_t c = 0; c < pixel_values; c++) {
					ptrdiff_t bit = (x * pixel_values + c + 1) * bit_depth;
					uint32_t raw = (dr[(bit - 9) >> 3] << 8 | dr[(bit - 1) >> 3]) >> ((8 - bit) & 7);
					v[c] = (raw & ((1 << bit_depth) - 1)) * scale;
				}

				Pixel16 px;
				switch (pixel_format) {
				case 0: px = { v[0], v[0], v[0], 0xffff }; break;
				case 2: px = { v[0], v[1], v[2], 0xffff }; break;
				case 3: px = v[0] < palette.size() ? palette[v[0]] : Pixel16{ }; break;
				case 4: px = { v[0], v[0], v[0], v[1] }; break;
				case 6: px = { v[0], v[1], v[2], v[3] }; break;
				}
				if (px == trns) px = { 0, 0, 0, 0 };
				image.pixels[(lace[1]+y*lace[3]) * image.width + (lace[0]+x*lace[2])] = px;
			}
		}

	}

	return image;
}

std::vector<char> load_file(const char *filename)
{
	std::vector<char> data;
	FILE *f = fopen(filename, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		size_t size = ftell(f);
		fseek(f, 0, SEEK_SET);
		data.resize(size);
		size_t result = fread(data.data(), 1, size, f);
		ufbx_assert(result == size);
		fclose(f);
	}
	return data;
}

Image load_png(const char *filename)
{
	std::vector<char> data = load_file(filename);
	return read_png(data.data(), data.size());
}

struct HuffSymbol {
	uint16_t length, bits;
};

uint32_t bit_reverse(uint32_t mask, uint32_t num_bits)
{
	ufbx_assert(num_bits <= 16);
	uint32_t x = mask;
	x = (((x & 0xaaaa) >> 1) | ((x & 0x5555) << 1));
	x = (((x & 0xcccc) >> 2) | ((x & 0x3333) << 2));
	x = (((x & 0xf0f0) >> 4) | ((x & 0x0f0f) << 4));
	x = (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));
	return x >> (16 - num_bits);
}

void build_huffman(HuffSymbol *symbols, uint32_t *counts, uint32_t num_symbols, uint32_t max_bits)
{
	struct HuffNode {
		uint16_t index; uint16_t parent; uint32_t count;
		bool operator<(const HuffNode &rhs) const {
			return count != rhs.count ? count < rhs.count : index > rhs.index;
		}
	};
	HuffNode nodes[1024];

	uint32_t bias = 0;
	for (;;) {
		for (uint32_t i = 0; i < num_symbols; i++) {
			nodes[i] = { (uint16_t)i, UINT16_MAX, counts[i] ? counts[i] + bias : 0 };
		}
		std::sort(nodes, nodes + num_symbols);
		uint32_t cs = 0, ce = num_symbols, qs = 512, qe = qs;
		while (cs + 2 < ce && nodes[cs].count == 0) cs++;
		while (ce-cs + qe-qs > 1) {
			uint32_t a = ce-cs > 0 && (qe-qs == 0 || nodes[cs].count < nodes[qs].count) ? cs++ : qs++;
			uint32_t b = ce-cs > 0 && (qe-qs == 0 || nodes[cs].count < nodes[qs].count) ? cs++ : qs++;
			nodes[a].parent = nodes[b].parent = (uint16_t)qe;
			nodes[qe++] = { UINT16_MAX, UINT16_MAX, nodes[a].count + nodes[b].count };
		}

		bool fail = false;
		uint32_t length_counts[16] = { }, length_codes[16] = { };
		for (uint32_t i = 0; i < num_symbols; i++) {
			uint32_t len = 0;
			for (uint32_t a = nodes[i].parent; a != UINT16_MAX; a = nodes[a].parent) len++;
			if (len > max_bits) { fail = true; break; }
			length_counts[len]++;
			symbols[nodes[i].index].length = (uint16_t)len;
		}
		if (fail) {
			bias = bias ? bias << 1 : 1;
			continue;
		}

		uint32_t code = 0, prev_count = 0;
		for (uint32_t bits = 1; bits < 16; bits++) {
			uint32_t count = length_counts[bits];
			code = (code + prev_count) << 1;
			prev_count = count;
			length_codes[bits] = code;
		}

		for (uint32_t i = 0; i < num_symbols; i++) {
			uint32_t len = symbols[i].length;
			symbols[i].bits = len ? bit_reverse(length_codes[len]++, len) : 0;
		}

		break;
	}
}

uint32_t match_len(const unsigned char *a, const unsigned char *b, uint32_t max_length)
{
	if (max_length > 258) max_length = 258;
	for (uint32_t len = 0; len < max_length; len++) {
		if (*a++ != *b++) return len;
	}
	return max_length;
}

uint16_t encode_lit(uint32_t value, uint32_t bits)
{
	return (uint16_t)(value | (0xfffeu << bits));
}

uint32_t sym_offset_bits(HuffSymbol *syms, uint32_t code, uint32_t base, const uint16_t *counts, uint32_t value)
{
	for (uint32_t bits = 0; counts[bits]; bits++) {
		uint32_t num = counts[bits] << bits, delta = value - base;
		if (delta < num) {
			return syms[code + (delta >> bits)].length + bits;
		}
		code += counts[bits];
		base += num;
	}
	return syms[code].bits;
}

size_t encode_sym_offset(uint16_t *dst, uint16_t code, uint32_t base, const uint16_t *counts, uint32_t value)
{
	for (uint32_t bits = 0; counts[bits]; bits++) {
		uint32_t num = counts[bits] << bits, delta = value - base;
		if (delta < num) {
			dst[0] = code + (delta >> bits);
			if (bits > 0) {
				dst[1] = encode_lit(delta & ((1 << bits) - 1), bits);
				return 2;
			} else {
				return 1;
			}
		}
		code += counts[bits];
		base += num;
	}
	dst[0] = code;
	return 1;
}

uint32_t lz_hash(const unsigned char *d)
{
	uint32_t x = (uint32_t)d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16;
	x ^= x >> 16;
	x *= UINT32_C(0x7feb352d);
	x ^= x >> 15;
	x *= UINT32_C(0x846ca68b);
	x ^= x >> 16;
	return x ? x : 1;
}

void init_tri_dist(uint16_t *tri_dist, const void *data, uint32_t length)
{
	const uint32_t max_scan = 32;
	const unsigned char *d = (const unsigned char*)data;
	struct Match {
		uint32_t hash = 0;
		uint32_t offset = 0x80000000;
	};
	std::vector<Match> match_table;
	uint32_t mask = 0x1ffff;
	match_table.resize(mask + 1);
	for (uint32_t i = 0; i < length; i++) {
		if (length - i >= 3) {
			uint32_t hash = lz_hash(d + i), replace_ix = 0, replace_score = 0;
			for (uint32_t scan = 0; scan < max_scan; scan++) {
				uint32_t ix = (hash + scan) & mask;
				uint32_t delta = (uint32_t)(i - match_table[ix].offset);
				if (match_table[ix].hash == hash && delta <= 32768) {
					tri_dist[i] = (uint16_t)delta;
					match_table[ix].offset = i;
					replace_ix = UINT32_MAX;
					break;
				} else {
					uint32_t score = delta <= 32768 ? delta : 32768 + max_scan - scan;
					if (score > replace_score) {
						replace_score = score;
						replace_ix = ix;
						if (score > 32768) break;
					}
				}
			}
			if (replace_ix != UINT32_MAX) {
				match_table[replace_ix].hash = hash;
				match_table[replace_ix].offset = i;
				tri_dist[i] = UINT16_MAX;
			}
		} else {
			tri_dist[i] = UINT16_MAX;
		}
	}
}

static picort_forceinline bool cmp3(const void *a, const void *b)
{
	const char *ca = (const char*)a, *cb = (const char*)b;
	return ca[0] == cb[0] && ca[1] == cb[1] && ca[2] == cb[2];
}

struct LzMatch
{
	uint32_t len = 0, dist = 0, bits = 0;
};

static const uint16_t len_counts[] = { 8, 4, 4, 4, 4, 4, 0 };
static const uint16_t dist_counts[] = { 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0 };

LzMatch find_match(HuffSymbol *syms, const uint16_t *tri_dist, const void *data, uint32_t begin, uint32_t end)
{
	const unsigned char *d = (const unsigned char*)data;
	int32_t max_len = (int32_t)end - begin - 1;
	if (max_len > 258) max_len = 258;
	if (max_len < 3) return { };
	uint32_t best_len = 0, best_dist = 0;
	int32_t m_off = begin - tri_dist[begin], m_end = m_off, m_delta = 0;
	uint32_t best_bits = syms[d[begin + 0]].length + syms[d[begin + 1]].length;
	for (size_t scan = 0; scan < 1000; scan++) {
		if (begin - m_off > 32768 || m_off < 0 || m_end < 0 || m_delta > max_len - 3) break;
		int32_t delta = m_end - m_off - 3;

		bool ok = true;
		if (m_off >= 0 && (m_end - m_off < m_delta || !cmp3(d + m_off + m_delta, d + begin + m_delta))) {
			m_off -= tri_dist[m_off];
			if (m_off >= 0 && m_end - m_off > m_delta && cmp3(d + m_off + m_delta, d + begin + m_delta)) {
				m_end = m_off + m_delta;
			}
			ok = false;
		}
		if (m_end >= m_delta && (m_end - m_off < m_delta || !cmp3(d + m_end - m_delta, d + begin))) {
			m_end -= tri_dist[m_end];
			if (m_end >= m_delta && m_end - m_off < m_delta && cmp3(d + m_end - m_delta, d + begin)) {
				m_off = m_end - m_delta;
			}
			ok = false;
		}
		if (!ok) continue;

		if (m_delta <= 3 || !memcmp(d + begin, d + m_off, m_delta + 3)) {
			do {
				uint32_t m_len = m_delta + 3, m_dist = begin - m_off;
				uint32_t m_bits = sym_offset_bits(syms, 257, 3, len_counts, m_len)
					+ sym_offset_bits(syms, 286, 1, dist_counts, m_dist);

				best_bits += syms[d[begin + m_len - 1]].length;
				if (m_bits < best_bits) {
					best_bits = m_bits;
					best_len = m_len;
					best_dist = m_dist;
				}

				m_end++;
				m_delta++;
			} while (m_delta <= max_len - 3 && d[begin + m_delta + 2] == d[m_off + m_delta + 2]);
		} else {
			m_off--;
		}
	}
	return { best_len, best_dist, best_bits };
}

uint32_t encode_lz(HuffSymbol *syms, uint16_t *dst, const uint16_t *tri_dist, const void *data, uint32_t begin, uint32_t end, uint32_t *p_bits)
{
	const unsigned char *d = (const unsigned char*)data;
	uint16_t *p = dst;
	uint32_t bits = 0;
	for (int32_t i = (int32_t)begin; i < (int32_t)end; i++) {

		LzMatch match = find_match(syms, tri_dist, data, i, end);
		while (match.len > 0) {
			LzMatch next = find_match(syms, tri_dist, data, i + 1, end);
			if (next.len == 0) break;

			uint32_t match_bits = match.bits, next_bits = next.bits + syms[d[i]].length;
			for (uint32_t j = i + match.len; j < i + 1 + next.len; j++) match_bits += syms[d[j]].length;
			for (uint32_t j = i + 1 + next.len; j < i + match.len; j++) next_bits += syms[d[j]].length;
			if (next_bits >= match_bits) break;

			bits += syms[d[i]].length;
			*p++ = d[i++];
			match = next;
		}

		if (match.len > 0) {
			assert(!memcmp(d + i, d + i - match.dist, match.len));
			p += encode_sym_offset(p, 257, 3, len_counts, match.len);
			p += encode_sym_offset(p, 286, 1, dist_counts, match.dist);
			i += match.len - 1;
		} else {
			*p++ = d[i];
		}
	}
	return (uint32_t)(p - dst);
}

size_t encode_lengths(uint16_t *dst, HuffSymbol *syms, uint32_t *p_count, uint32_t min_count)
{
	uint32_t count = *p_count;
	while (count > min_count && syms[count - 1].length == 0) count--;
	*p_count = count;

	uint16_t *p = dst;
	for (uint32_t begin = 0; begin < count; ) {
		uint16_t len = syms[begin].length;
		uint32_t end = begin + 1;
		while (end < count && syms[end].length == len) {
			end++;
		}

		uint32_t span_begin = begin;
		while (begin < end) {
			uint32_t num = end - begin;
			if (num < 3 || (len > 0 && begin == span_begin)) {
				num = 1;
				*p++ = len;
			} else if (len == 0 && end - begin < 11) {
				*p++ = 17;
				*p++ = encode_lit(num - 3, 3);
			} else if (len == 0) {
				if (num > 138) num = 138;
				*p++ = 18;
				*p++ = encode_lit(num - 11, 7);
			} else { 
				if (num > 6) num = 6;
				*p++ = 16;
				*p++ = encode_lit(num - 3, 2);
			}
			begin += num;
		}
	}
	return (size_t)(p - dst);
}

size_t flush_bits(char **p_dst, size_t reg, size_t num)
{
	char *dst = *p_dst;
	for (; num >= 8; num -= 8) {
		*dst++ = (uint8_t)reg;
		reg >>= 8;
	}
	*p_dst = dst;
	return reg;
}

picort_forceinline void push_bits(char **p_dst, size_t *p_reg, size_t *p_num, uint32_t value, uint32_t bits)
{
	if (*p_num + bits > sizeof(size_t) * 8) {
		*p_reg = flush_bits(p_dst, *p_reg, *p_num);
		*p_num &= 0x7;
	}
	*p_reg |= (size_t)value << *p_num;
	*p_num += bits;
}

void encode_syms(char **p_dst, size_t *p_reg, size_t *p_num, HuffSymbol *table, const uint16_t *syms, size_t num_syms)
{
	size_t reg = *p_reg, num = *p_num;
	for (size_t i = 0; i < num_syms; i++) {
		uint32_t sym = syms[i];
		if (sym & 0x8000) {
			// TODO: BSR?
			uint32_t bits = 15;
			while (sym & (1 << bits)) bits--;
			push_bits(p_dst, &reg, &num, sym & ((1 << bits) - 1), bits);
		} else {
			HuffSymbol hs = table[sym];
			push_bits(p_dst, &reg, &num, hs.bits, hs.length);
		}
	}
	*p_reg = reg;
	*p_num = num;
}

uint32_t adler32(const void *data, size_t size)
{
	size_t a = 1, b = 0;
	const char *p = (const char*)data;
	const size_t num_before_wrap = sizeof(size_t) == 8 ? 380368439u : 5552u;
	size_t size_left = size;
	while (size_left > 0) {
		size_t num = size_left <= num_before_wrap ? size_left : num_before_wrap;
		size_left -= num;
		const char *end = p + num;
		while (p != end) {
			a += (size_t)(uint8_t)*p++; b += a;
		}
		a %= 65521u;
		b %= 65521u;
	}
	return (uint32_t)((b << 16) | (a & 0xffff));
}

size_t deflate(void *dst, const void *data, size_t length)
{
	char *dp = (char*)dst;

	static const uint32_t lz_block_size = 8*1024*1024;
	static const uint32_t huff_block_size = 64*1024;

	std::vector<uint16_t> sym_buf, tri_buf;
	sym_buf.resize(std::min((size_t)huff_block_size, length));
	tri_buf.resize(std::min((size_t)lz_block_size, length));

	size_t reg = 0, num = 0;

	push_bits(&dp, &reg, &num, 0x78, 8);
	push_bits(&dp, &reg, &num, 0x9c, 8);

	for (size_t lz_base = 0; lz_base < length; lz_base += lz_block_size) {
		size_t lz_length = length - lz_base;
		if (lz_length > lz_block_size) lz_length = lz_block_size;

		init_tri_dist(tri_buf.data(), (const char*)data + lz_base, (uint32_t)lz_length);

		for (size_t huff_base = 0; huff_base < lz_length; huff_base += huff_block_size) {
			size_t huff_length = lz_length - huff_base;
			if (huff_length > huff_block_size) huff_length = huff_block_size;

			uint16_t *syms = sym_buf.data();
			size_t num_syms = 0;

			HuffSymbol sym_huffs[316];
			for (uint32_t i = 0; i < 316; i++) {
				sym_huffs[i].length = i >= 256 ? 6 : 8;
			}

			for (uint32_t i = 0; i < 2; i++) {
				uint32_t bits = 0;
				num_syms = encode_lz(sym_huffs, syms, tri_buf.data(), (const char*)data + lz_base, (uint32_t)huff_base, (uint32_t)(huff_base + huff_length), &bits);

				uint32_t sym_counts[316] = { };
				for (size_t i = 0; i < num_syms; i++) {
					uint32_t sym = syms[i];
					if (sym < 316) sym_counts[sym]++;
				}
				sym_counts[256] = 1;

				build_huffman(sym_huffs + 0, sym_counts + 0, 286, 15);
				build_huffman(sym_huffs + 286, sym_counts + 286, 30, 15);
			}

			uint32_t hlit = 286, hdist = 30;
			uint16_t header_syms[316], *header_dst = header_syms;
			header_dst += encode_lengths(header_dst, sym_huffs + 0, &hlit, 257);
			header_dst += encode_lengths(header_dst, sym_huffs + 286, &hdist, 1);
			size_t header_len = (size_t)(header_dst - header_syms);

			uint32_t header_counts[19] = { };
			HuffSymbol header_huffs[19];
			for (size_t i = 0; i < header_len; i++) {
				uint32_t sym = header_syms[i];
				if (sym < 19) header_counts[sym]++;
			}

			build_huffman(header_huffs, header_counts, 19, 7);
			static const uint8_t lens[] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
			uint32_t hclen = 19;
			while (hclen > 4 && header_huffs[lens[hclen - 1]].length == 0) hclen--;

			bool end = huff_base + huff_length == lz_length && lz_base + lz_length == length;
			push_bits(&dp, &reg, &num, end ? 0x5 : 0x4, 3);

			push_bits(&dp, &reg, &num, hlit - 257, 5);
			push_bits(&dp, &reg, &num, hdist - 1, 5);
			push_bits(&dp, &reg, &num, hclen - 4, 4);
			for (uint32_t i = 0; i < hclen; i++) {
				push_bits(&dp, &reg, &num, header_huffs[lens[i]].length, 3);
			}

			encode_syms(&dp, &reg, &num, header_huffs, header_syms, header_len);
			encode_syms(&dp, &reg, &num, sym_huffs, syms, num_syms);

			HuffSymbol end_hs = sym_huffs[256];
			push_bits(&dp, &reg, &num, end_hs.bits, end_hs.length);
		}
	}

	if (num % 8 != 0) push_bits(&dp, &reg, &num, 0, 8 - (num % 8));
	uint32_t checksum = adler32(data, length);
	for (size_t i = 0; i < 4; i++) {
		push_bits(&dp, &reg, &num, (checksum >> (24 - i * 8)) & 0xff, 8);
	}

	flush_bits(&dp, reg, num);

	return (size_t)(dp - (char*)dst);
}

void png_filter_row(uint32_t method, uint8_t *dst, const uint8_t *line, const uint8_t *prev, uint32_t width, uint32_t num_channels)
{
	int32_t dx = (int32_t)num_channels, pitch = (int32_t)(width * num_channels);
	if (method == 0) {
		for (int32_t i = 0; i < pitch; i++) dst[i] = line[i];
	} else if (method == 1) {
		for (int32_t i = 0; i < pitch; i++) dst[i] = line[i] - line[i-dx];
	} else if (method == 2) {
		for (int32_t i = 0; i < pitch; i++) dst[i] = line[i] - prev[i];
	} else if (method == 3) {
		for (int32_t i = 0; i < pitch; i++) dst[i] = line[i] - (line[i-dx] + prev[i]) / 2;
	} else if (method == 4) {
		for (int32_t i = 0; i < pitch; i++) {
			int32_t s = line[i], a = line[i-dx], b = prev[i], c = prev[i-dx], p = a+b-c;
			int32_t pa = abs(p-a), pb = abs(p-b), pc = abs(p-c);
			if (pa <= pb && pa <= pc) dst[i] = (uint8_t)(s - a);
			else if (pb <= pc) dst[i] = (uint8_t)(s - b);
			else dst[i] = (uint8_t)(s - c);
		}
	}
}

void crc32_init(uint32_t *crc_table)
{
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t r = i;
		for(uint32_t k = 0; k < 8; ++k) {
			r = ((r & 1) ? UINT32_C(0xEDB88320) : 0) ^ (r >> 1);
		}
		crc_table[i] = r;
	}
}

uint32_t crc32(uint32_t *crc_table, const void *data, size_t size, uint32_t seed)
{
	uint32_t crc = ~seed;
	const uint8_t *src = (const uint8_t*)data;
	for (size_t i = 0; i < size; i++) {
		crc = crc_table[(crc ^ src[i]) & 0xff] ^ (crc >> 8);
	}
	return ~crc;
}

void png_add_chunk(uint32_t *crc_table, std::vector<uint8_t> &dst, const char *tag, const void *data, size_t size)
{
	uint8_t be_size[] = { (uint8_t)(size>>24), (uint8_t)(size>>16), (uint8_t)(size>>8), (uint8_t)size };
	dst.insert(dst.end(), be_size, be_size + 4);
	dst.insert(dst.end(), (const uint8_t*)tag, (const uint8_t*)tag + 4);
	dst.insert(dst.end(), (const uint8_t*)data, (const uint8_t*)data + size);
	uint32_t crc = crc32(crc_table, dst.data() + dst.size() - (size + 4), size + 4, 0);
	uint8_t be_crc[] = { (uint8_t)(crc>>24), (uint8_t)(crc>>16), (uint8_t)(crc>>8), (uint8_t)crc };
	dst.insert(dst.end(), be_crc, be_crc + 4);
}

std::vector<uint8_t> write_png(const void *data, uint32_t width, uint32_t height)
{
	std::vector<uint8_t> result;

	const uint8_t *pixels = (const uint8_t*)data;
	size_t num_pixels = (size_t)width * (size_t)height;

	uint32_t num_channels = 3;
	for (size_t i = 0; i < num_pixels; i++) {
		if (pixels[i * 4 + 3] < 255) {
			num_channels = 4;
			break;
		}
	}

	std::vector<uint8_t> lines[2];
	lines[0].resize((width + 1) * num_channels);
	lines[1].resize((width + 1) * num_channels);
	uint8_t *prev = lines[0].data() + num_channels;
	uint8_t *line = lines[1].data() + num_channels;

	std::vector<uint8_t> idat, idat_deflate;
	idat.resize((width * num_channels + 1) * height);
	// TODO: Proper bound...
	idat_deflate.resize(idat.size() + idat.size() / 10);

	uint32_t pitch = width * num_channels;
	uint8_t *dst = idat.data();
	for (uint32_t y = 0; y < height; y++) {
		const uint8_t *src = pixels + y * width * 4;
		for (uint32_t c = 0; c < num_channels; c++) {
			for (uint32_t x = 0; x < width; x++) {
				line[x*num_channels + c] = src[x*4 + c];
			}
		}

		uint32_t best_filter = 0, best_entropy = UINT32_MAX;
		for (uint32_t f = 0; f <= 4; f++) {
			png_filter_row(f, dst + 1, line, prev, width, num_channels);
			uint32_t entropy = 0;
			for (uint32_t x = 0; x < pitch; x++) {
				entropy += abs((int8_t)dst[1 + x]);
			}
			if (entropy < best_entropy) {
				best_filter = f;
				best_entropy = entropy;
			}
		}
		if (best_filter != 4) {
			png_filter_row(best_filter, dst + 1, line, prev, width, num_channels);
		}
		dst[0] = (uint8_t)best_filter;

		dst += width * num_channels + 1;
		std::swap(prev, line);
	}

	size_t idat_length = deflate(idat_deflate.data(), idat.data(), idat.size());

	uint8_t ihdr[] = {
		(uint8_t)(width>>24), (uint8_t)(width>>16), (uint8_t)(width>>8), (uint8_t)width,
		(uint8_t)(height>>24), (uint8_t)(height>>16), (uint8_t)(height>>8), (uint8_t)height,
		8, (uint8_t)(num_channels == 4 ? 6 : 2), 0, 0, 0,
	};

	uint32_t crc_table[256];
	crc32_init(crc_table);

	const char magic[] = "\x89PNG\r\n\x1a\n";
	result.insert(result.end(), magic, magic + 8);
	png_add_chunk(crc_table, result, "IHDR", ihdr, sizeof(ihdr));
	png_add_chunk(crc_table, result, "IDAT", idat_deflate.data(), idat_length);
	png_add_chunk(crc_table, result, "IEND", NULL, 0);
	return result;
}

// -- BVH construction

struct Bounds
{
	Vec3 min = { +Inf, +Inf, +Inf };
	Vec3 max = { -Inf, -Inf, -Inf };

	Vec3 center() const { return (min + max) * 0.5f; }
	Vec3 diagonal() const { return max - min; }
	Real area() const {
		Vec3 d = max - min;
		return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
	}

	void add(const Bounds &b) {
		min = ::min(min, b.min);
		max = ::max(max, b.max);
	}
};

struct Triangle
{
	Vec3 v[3];
	size_t index = 0;

	Bounds bounds() const { return { min(min(v[0], v[1]), v[2]), max(max(v[0], v[1]), v[2]) }; }
};

struct BVH
{
	Triangle *triangles = nullptr;
	size_t num_triangles = 0;
	Bounds child_bounds[2];
	Real spacer = 0.0f;
	std::unique_ptr<BVH[]> child_nodes;
};

struct BVHBucket
{
	Bounds bounds;
	size_t count = 0;

	inline void add(const Bounds &b) { bounds.add(b); count++; }
	inline void add(const BVHBucket &b) { bounds.add(b.bounds); count += b.count; }
	Real cost(Real parent_area) {
		return bounds.area() / parent_area * (Real)((count + 3) / 4);
	}
};

struct BVHSplit
{
	Real cost = Inf;
	int axis = -1;
	size_t bucket = 0;
	BVHBucket left, right;
};

Bounds merge_triangle_bounds(const Triangle *triangles, size_t count)
{
	Bounds bounds;
	for (size_t i = 0; i < count; i++) bounds.add(triangles[i].bounds());
	return bounds;
}

BVHBucket merge_buckets(const BVHBucket *buckets, size_t count)
{
	BVHBucket bucket;
	for (size_t i = 0; i < count; i++) bucket.add(buckets[i]);
	return bucket;
}

void bvh_recurse(BVH &bvh, Triangle *triangles, size_t count, const Bounds &bounds, int depth);

// Split a BVH node to two containing equal amount of triangles on the largest axis
void bvh_split_equal(BVH &bvh, Triangle *triangles, size_t count, const Bounds &bounds, int depth)
{
	int axis = largest_axis(bounds.diagonal());
	std::sort(triangles, triangles + count, [axis](const Triangle &a, const Triangle &b) {
		return a.bounds().center()[axis] < b.bounds().center()[axis]; });

	size_t num_left = count / 2, num_right = count - num_left;
	Triangle *right = triangles + num_left;
	bvh.child_bounds[0] = merge_triangle_bounds(triangles, num_left);
	bvh.child_bounds[1] = merge_triangle_bounds(right, num_right);

	bvh.child_nodes = std::make_unique<BVH[]>(2);
	bvh_recurse(bvh.child_nodes[0], triangles, num_left, bvh.child_bounds[0], depth + 1);
	bvh_recurse(bvh.child_nodes[1], right, num_right, bvh.child_bounds[1], depth + 1);
}

// Split a BVH node based on the SAH heuristic
// https://pbr-book.org/3ed-2018/Primitives_and_Intersection_Acceleration/Bounding_Volume_Hierarchies#TheSurfaceAreaHeuristic
void bvh_split_bucketed(BVH &bvh, Triangle *triangles, size_t count, const Bounds &bounds, int depth)
{
	constexpr size_t num_buckets = 8;
	BVHBucket buckets[3][num_buckets];

	// Initialized buckets with triangles
	Real nb = (Real)num_buckets - 0.0001f;
	Vec3 to_bucket = Vec3{ nb, nb, nb } / (bounds.max - bounds.min);
	for (size_t i = 0; i < count; i++) {
		Triangle &tri = triangles[i];
		Bounds tri_bounds = tri.bounds();
		Vec3 t = (tri_bounds.center() - bounds.min) * to_bucket;
		t = min(max(t, Vec3{}), Vec3{ nb, nb, nb });
		buckets[0][(size_t)t.x].add(tri_bounds);
		buckets[1][(size_t)t.y].add(tri_bounds);
		buckets[2][(size_t)t.z].add(tri_bounds);
	}

	// Find the best split that minimizes the estimated SAH cost
	BVHSplit split;
	Real parent_area = bounds.area();
	for (int axis = 0; axis < 3; axis++) {
		for (size_t bucket = 1; bucket < num_buckets - 1; bucket++) {
			BVHBucket left = merge_buckets(buckets[axis], bucket);
			BVHBucket right = merge_buckets(buckets[axis] + bucket, num_buckets - bucket);
			if (left.count == 0 || right.count == 0) continue;

			Real cost = 0.5f + left.cost(parent_area) + right.cost(parent_area);
			if (cost < split.cost) {
				split = { cost, axis, bucket, left, right };
			}
		}
	}

	Real leaf_cost = (Real)((count + 3) / 4);

	// Partition the triangles to the left and right halves and recurse if we found
	// a split better than a leaf node, otherwise create a leaf or an equal split BVH.
	if (split.axis >= 0 && (split.cost < leaf_cost || count > 16)) {
		Triangle *first = triangles, *last = triangles + count;
		while (first != last) {
			Bounds tri_bounds = first->bounds();
			Vec3 t = (tri_bounds.center() - bounds.min) * to_bucket;
			t = min(max(t, Vec3{}), Vec3{ nb, nb, nb });
			if ((size_t)t[split.axis] < split.bucket) {
				++first;
			} else {
				std::swap(*first, *--last);
			}
		}

		size_t num_left = first - triangles, num_right = count - num_left;
		assert(num_left == split.left.count && num_right == split.right.count);
		bvh.child_bounds[0] = split.left.bounds;
		bvh.child_bounds[1] = split.right.bounds;
		bvh.child_nodes = std::make_unique<BVH[]>(2);

		// TODO: Thread limit
		if (depth < 4 && (num_left > count / 8 || num_right > count / 8)) {
			std::thread a { bvh_recurse, std::ref(bvh.child_nodes[0]), triangles, num_left, bvh.child_bounds[0], depth + 1 };
			std::thread b { bvh_recurse, std::ref(bvh.child_nodes[1]), first, num_right, bvh.child_bounds[1], depth + 1 };
			a.join();
			b.join();
		} else {
			bvh_recurse(bvh.child_nodes[0], triangles, num_left, bvh.child_bounds[0], depth + 1);
			bvh_recurse(bvh.child_nodes[1], first, num_right, bvh.child_bounds[1], depth + 1);
		}

	} else if (count > 16) {
		bvh_split_equal(bvh, triangles, count, bounds, depth);
	} else {
		bvh.triangles = triangles;
		bvh.num_triangles = count;
	}
}

// Choose a split method for the current BVH node
void bvh_recurse(BVH &bvh, Triangle *triangles, size_t count, const Bounds &bounds, int depth)
{
	if (count <= 4) {
		bvh.triangles = triangles;
		bvh.num_triangles = count;
	} else if (depth > 32) {
		bvh_split_equal(bvh, triangles, count, bounds, depth);
	} else {
		bvh_split_bucketed(bvh, triangles, count, bounds, depth);
	}
}

// Build a BVH structure from `count` `triangles`.
BVH build_bvh(Triangle *triangles, size_t count)
{
	BVH bvh;
	Bounds bounds = merge_triangle_bounds(triangles, count);
	bvh_recurse(bvh, triangles, count, bounds, 0);
	return bvh;
}

struct Ray
{
	Vec3 origin;
	Vec3 direction;
};

struct RayHit { Real t = Inf, u = 0.0f, v = 0.0f; size_t index = SIZE_MAX; size_t steps = 0; };

struct RayTrace
{
	struct Axes { int x = -1, y = -1, z = -1; };

	Ray ray;
	Vec3 rcp_direction;
	Real spacer = 0.0f;

	Axes axes;
	Vec3 shear;

	Real max_t = Inf;
	RayHit hit;
};

// Precompute some values to trace with for `ray`
RayTrace setup_trace(const Ray &ray, Real max_t=Inf)
{
	int kz = largest_axis(abs(ray.direction));
	int kx = (kz + 1) % 3, ky = (kz + 2) % 3;
	if (ray.direction[kz] < 0.0f) std::swap(kx, ky);

	RayTrace trace;
	trace.ray = ray;
	trace.rcp_direction = Vec3{1.0f, 1.0f, 1.0f} / ray.direction;
	trace.axes = { kx, ky, kz };
	trace.shear = Vec3{ray.direction[kx], ray.direction[ky], 1.0f} / ray.direction[kz];
	trace.max_t = max_t;
	return trace;
}

// Test if `trace` intersects `bounds`, returns front intersection t value
// https://pbr-book.org/3ed-2018/Shapes/Basic_Shape_Interface#RayndashBoundsIntersections
picort_forceinline Real intersect_bounds(const RayTrace &trace, const Bounds &bounds)
{
	Vec3 ts_min = (bounds.min - trace.ray.origin) * trace.rcp_direction;
	Vec3 ts_max = (bounds.max - trace.ray.origin) * trace.rcp_direction;
	Vec3 ts_near = min(ts_min, ts_max);
	Vec3 ts_far = max(ts_min, ts_max);
	Real t_near = max(max_component(ts_near), 0.0f);
	Real t_far = min_component(ts_far);
	return t_near <= t_far ? t_near : Inf;
}

// Check if `trace` intersects `triangle` in a watertight manner, updates `trace.hit`
// http://jcgt.org/published/0002/01/05/
// https://pbr-book.org/3ed-2018/Shapes/Triangle_Meshes#TriangleIntersection
void intersect_triangle(RayTrace &trace, const Triangle &triangle)
{
	int kx = trace.axes.x, ky = trace.axes.y, kz = trace.axes.z;
	Vec3 s = trace.shear;

	Vec3 a = triangle.v[0] - trace.ray.origin;
	Vec3 b = triangle.v[1] - trace.ray.origin;
	Vec3 c = triangle.v[2] - trace.ray.origin;

	Real ax = a[kx] - s.x*a[kz], ay = a[ky] - s.y*a[kz];
	Real bx = b[kx] - s.x*b[kz], by = b[ky] - s.y*b[kz];
	Real cx = c[kx] - s.x*c[kz], cy = c[ky] - s.y*c[kz];

	Real u = cx*by - cy*bx;
	Real v = ax*cy - ay*cx;
	Real w = bx*ay - by*ax;
	if (sizeof(Real) < 8 && (u == 0.0f || v == 0.0f || w == 0.0f)) {
		using D = double;
		u = (Real)((D)cx*(D)by - (D)cy*(D)bx);
		v = (Real)((D)ax*(D)cy - (D)ay*(D)cx);
		w = (Real)((D)bx*(D)ay - (D)by*(D)ax);
	}

	if ((u<0.0f || v<0.0f || w<0.0f) && (u>0.0f || v>0.0f || w>0.0f)) return;

	Real det = u + v + w;
	Real t = u*s.z*a[kz] + v*s.z*b[kz] + w*s.z*c[kz];

	if (det == 0.0f) return;
	if (det < 0.0f && (t >= 0.0f || t < trace.max_t * det)) return;
	if (det > 0.0f && (t <= 0.0f || t > trace.max_t * det)) return;

	Real rcp_det = 1.0f / det;
	trace.max_t = t * rcp_det;
	trace.hit = { t * rcp_det, u * rcp_det, v * rcp_det, triangle.index, trace.hit.steps };
}

struct BVHHit
{
	const BVH *bvh;
	Real t;
};

// Check if `trace` intersects anything within `bvh`, updates `trace.hit`
void intersect_bvh(RayTrace &trace, const BVH &root)
{
	BVHHit stack[64];
	uint32_t depth = 1;
	stack[0].bvh = &root;
	stack[0].t = 0.0f;

	while (depth > 0) {
		depth--;
		if (stack[depth].t >= trace.max_t) continue;
		const BVH *bvh = stack[depth].bvh;

		while (bvh && bvh->num_triangles == 0) {
			trace.hit.steps++;
			const BVH *child0 = &bvh->child_nodes[0], *child1 = &bvh->child_nodes[1];
			picort_prefetch(&child0->child_bounds);
			picort_prefetch(&child1->child_bounds);
			Real t0 = intersect_bounds(trace, bvh->child_bounds[0]);
			Real t1 = intersect_bounds(trace, bvh->child_bounds[1]);
			bvh = NULL;
			if (t0 < t1) {
				if (t1 < trace.max_t) {
					stack[depth].bvh = child1;
					stack[depth].t = t1;
					depth++;
				}
				if (t0 < trace.max_t) {
					bvh = child0;
				}
			} else {
				if (t0 < trace.max_t) {
					stack[depth].bvh = child0;
					stack[depth].t = t0;
					depth++;
				}
				if (t1 < trace.max_t) {
					bvh = child1;
				}
			}
		}

		if (bvh && bvh->num_triangles > 0) {
			trace.hit.steps++;
			for (size_t i = 0; i < bvh->num_triangles; i++) {
				intersect_triangle(trace, bvh->triangles[i]);
			}
		}
	}
}

#if SSE

picort_forceinline Real scale_to_real(uint8_t scale)
{
	return _mm_cvtss_f32(_mm_castsi128_ps(_mm_set1_epi32((int)scale << 23)));
}

uint8_t real_to_scale(Real v)
{
	uint32_t exp = ((uint32_t)_mm_cvtsi128_si32(_mm_castps_si128(_mm_set_ss(v * 2.125f))) >> 23) & 0xff;
	if (exp < 1) exp = 1;
	if (exp > 254) exp = 254;
	return exp;
}

#else

struct ScaleTable
{
	Real v[256];

	ScaleTable() {
		for (int i = 0; i < 256; i++) {
			v[i] = ldexp((Real)1.0, i - 127);
		}
	}
};

ScaleTable g_scale_table;

picort_forceinline Real scale_to_real(uint8_t scale)
{
	return g_scale_table.v[(uint32_t)scale];
}

uint8_t real_to_scale(Real v)
{
	int exp;
	float frac = frexp(v * 2.125f, &exp);
	exp += 127;
	if (exp < 1) exp = 1;
	if (exp > 254) exp = 254;
	return (uint8_t)exp;
}


#endif

picort_forceinline uint8_t rcp_scale(uint8_t v)
{
	return 254 - v;
}

union alignas(64) BVH4
{
	struct {
		const void *data;
		uint8_t tag_node;
	} common;
	struct {
		const void *children;
		uint8_t origin_scale;
		uint8_t bounds_scale;
		int16_t origin[3];
		int16_t bounds[3][2][4];
	} node;
	struct {
		const void *vertices;
		uint8_t tag_zero;
		uint8_t num_triangles;
		uint32_t triangle_offset;
		uint8_t triangles[4][3][4];
	} leaf;
	struct {
		uintptr_t data_index;
	} build;
};

static_assert(sizeof(uintptr_t) == sizeof(void*), "Aliasing breaks BVH4");
static_assert(sizeof(BVH4) == 64, "BVH4 is too big");

struct Trace4
{
	int sx, sy, sz;
	Real max_t;
	const BVH4 *bvhs;

	Real4 ray_origin;
	Real4 ray_rcp_dir;

	Real4::Index triangle_shuffle;
	Real4 shear;

	RayHit hit;

	bool shadow;
};

static const Real4::Index sort_indices[] = { 
	{ 1, 2, 3, 1 }, { 2, 3, 0, 2 }, { 3, 0, 1, 3 }, { 0, 1, 2, 0 },
};

Real4 to_real4(const Vec3 &v)
{
	return { v.x, v.y, v.z, 0.0f };
}

struct Scene4
{
	std::vector<BVH4> bvhs;
	std::vector<Vec3> vertices;
	std::vector<uint32_t> triangle_indices;
};

static std::atomic_uint64_t a_count[5];

RayHit intersect4(const Scene4 &scene, const Ray &ray, Real ray_max_t=Inf, bool shadow=false)
{
	RayHit hit;

	int kz = largest_axis(abs(ray.direction));
	int kx = (kz + 1) % 3, ky = (kz + 2) % 3;
	if (ray.direction[kz] < 0.0f) std::swap(kx, ky);

	Real4::Index triangle_shuffle = { (uint8_t)kx, (uint8_t)ky, (uint8_t)kz, 3 };

	Real4 ray_origin = to_real4(ray.origin);
	Real4 ray_rcp_dir = to_real4(Vec3{1.0f, 1.0f, 1.0f} / ray.direction);
	Real4 ray_shear = to_real4(Vec3{ray.direction[kx], ray.direction[ky], 1.0f} / ray.direction[kz]);

	// TODO: These get reloaded by MSVC....
	int sx = ray.direction.x < 0.0f ? 8 : 0;
	int sy = ray.direction.y < 0.0f ? 8 : 0;
	int sz = ray.direction.z < 0.0f ? 8 : 0;

	const BVH4 *bvh = scene.bvhs.data();
	picort_prefetch((const char*)bvh->common.data + 0*64);
	picort_prefetch((const char*)bvh->common.data + 1*64);
	picort_prefetch((const char*)bvh->common.data + 2*64);
	picort_prefetch((const char*)bvh->common.data + 3*64);

	const BVH4 *stack_bvh[64];
	Real stack_t[64];
	int stack_top = 0;

	for (;;) {
		hit.steps++;

		if (bvh->common.tag_node) {
			Real origin_scale = scale_to_real(bvh->node.origin_scale);
			Real bounds_scale = scale_to_real(bvh->node.bounds_scale);
			Real rcp_bounds_scale = scale_to_real(rcp_scale(bvh->node.bounds_scale));
			Real4 bvh_origin = Real4::load_i16(bvh->node.origin) * origin_scale;

			Real4 origin = (ray_origin - bvh_origin) * rcp_bounds_scale;
			Real4 rcp_dir = ray_rcp_dir * bounds_scale;

			Real4 min_x = (Real4::load_i16((const int16_t*)((const char*)bvh->node.bounds[0] + (sx^0))) - origin.xs()) * rcp_dir.xs();
			Real4 max_x = (Real4::load_i16((const int16_t*)((const char*)bvh->node.bounds[0] + (sx^8))) - origin.xs()) * rcp_dir.xs();
			Real4 min_y = (Real4::load_i16((const int16_t*)((const char*)bvh->node.bounds[1] + (sy^0))) - origin.ys()) * rcp_dir.ys();
			Real4 max_y = (Real4::load_i16((const int16_t*)((const char*)bvh->node.bounds[1] + (sy^8))) - origin.ys()) * rcp_dir.ys();
			Real4 min_z = (Real4::load_i16((const int16_t*)((const char*)bvh->node.bounds[2] + (sz^0))) - origin.zs()) * rcp_dir.zs();
			Real4 max_z = (Real4::load_i16((const int16_t*)((const char*)bvh->node.bounds[2] + (sz^8))) - origin.zs()) * rcp_dir.zs();
			Real4 min_t = Real4::max(Real4::max(min_x, min_y), Real4::max(min_z, 0.0f));
			Real4 max_t = Real4::min(Real4::min(max_x, max_y), max_z);
			Real4 t = Real4::select(min_t <= max_t, min_t, Inf);
			Real4::Mask hit_mask = t < ray_max_t;
			if (hit_mask.any()) {
				int mask = hit_mask.mask();
				int count = (int)popcount4((unsigned)mask);
				const BVH4 *children = (const BVH4*)bvh->node.children;

				if (count > 1) {
					int closest = (int)first_set4((unsigned)(t == t.min()).mask());
					bvh = children + closest;
					picort_prefetch((const char*)bvh->common.data + 0*64);
					picort_prefetch((const char*)bvh->common.data + 1*64);
					picort_prefetch((const char*)bvh->common.data + 2*64);
					picort_prefetch((const char*)bvh->common.data + 3*64);

					Real4 rest = Real4::shuffle(t, sort_indices[closest]);

					Real va = rest.x(), vb = rest.y(), vc = rest.z();
					int mab = va < vb ? 0 : 1;
					int mbc = 1 + (vb < vc ? 0 : 1);
					int mac = (va < vc ? 0 : 1) << 1;
					int tmp = va < vc ? mbc : mab;

					int i0 = mab == mbc ? 1 : mac;
					int i1 = mab == mbc ? mac : tmp;
					int i2 = i0 ^ i1 ^ 3;

					const BVH4 *c0 = children + ((closest + 1 + i0) & 3);
					const BVH4 *c1 = children + ((closest + 1 + i1) & 3);
					const BVH4 *c2 = children + ((closest + 1 + i2) & 3);

					stack_bvh[stack_top + ((count - 2) & 3)] = c0;
					stack_t[stack_top + ((count - 2) & 3)] = rest.get(i0);
					stack_bvh[stack_top + ((count - 3) & 3)] = c1;
					stack_t[stack_top + ((count - 3) & 3)] = rest.get(i1);
					stack_bvh[stack_top + ((count - 4) & 3)] = c2;
					stack_t[stack_top + ((count - 4) & 3)] = rest.get(i2);
					stack_top += count - 1;

					continue;
				} else {
					int closest = (int)first_set4((unsigned)(mask));
					bvh = children + closest;
					picort_prefetch((const char*)bvh->common.data + 0*64);
					picort_prefetch((const char*)bvh->common.data + 1*64);
					picort_prefetch((const char*)bvh->common.data + 2*64);
					picort_prefetch((const char*)bvh->common.data + 3*64);
					continue;
				}

			}
		} else {
			Real4::Index shuf = triangle_shuffle;
			Real4 sx = ray_shear.xs();
			Real4 sy = ray_shear.ys();
			Real4 sz = ray_shear.zs();
			Real4 shuf_origin = Real4::shuffle(ray_origin, shuf);
			uint32_t num_tris = bvh->leaf.num_triangles;
			uint32_t num_blocks = (uint32_t)(num_tris + 3) / 4;
			uint32_t valid_mask = (1u << num_tris) - 1;
			for (uint32_t base = 0; base < num_blocks; base++) {
				const Vec3 *vertices = (const Vec3*)bvh->leaf.vertices;

				Real4 v0x = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][0][0]), shuf) - shuf_origin;
				Real4 v0y = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][0][1]), shuf) - shuf_origin;
				Real4 v0z = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][0][2]), shuf) - shuf_origin;
				Real4 v0w = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][0][3]), shuf) - shuf_origin;
				Real4::transpose(v0x, v0y, v0z, v0w);
				Real4 ax = v0x - sx*v0z, ay = v0y - sy*v0z;

				Real4 v1x = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][1][0]), shuf) - shuf_origin;
				Real4 v1y = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][1][1]), shuf) - shuf_origin;
				Real4 v1z = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][1][2]), shuf) - shuf_origin;
				Real4 v1w = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][1][3]), shuf) - shuf_origin;
				Real4::transpose(v1x, v1y, v1z, v1w);
				Real4 bx = v1x - sx*v1z, by = v1y - sy*v1z;

				Real4 v2x = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][2][0]), shuf) - shuf_origin;
				Real4 v2y = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][2][1]), shuf) - shuf_origin;
				Real4 v2z = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][2][2]), shuf) - shuf_origin;
				Real4 v2w = Real4::load_shuffle((const Real*)(vertices + bvh->leaf.triangles[base][2][3]), shuf) - shuf_origin;
				Real4::transpose(v2x, v2y, v2z, v2w);
				Real4 cx = v2x - sx*v2z, cy = v2y - sy*v2z;

				Real4 u = cx*by - cy*bx;
				Real4 v = ax*cy - ay*cx;
				Real4 w = bx*ay - by*ax;

				if (sizeof(Real) < 8 && ((u == 0.0f) | (v == 0.0f) | (w == 0.0f)).any()) {
					using D = double;
					u = Real4 {
						(Real)((D)cx.x()*(D)by.x() - (D)cy.x()*(D)bx.x()),
						(Real)((D)cx.y()*(D)by.y() - (D)cy.y()*(D)bx.y()),
						(Real)((D)cx.z()*(D)by.z() - (D)cy.z()*(D)bx.z()),
						(Real)((D)cx.w()*(D)by.w() - (D)cy.w()*(D)bx.w()), };
					v = Real4 {
						(Real)((D)ax.x()*(D)cy.x() - (D)ay.x()*(D)cx.x()),
						(Real)((D)ax.y()*(D)cy.y() - (D)ay.y()*(D)cx.y()),
						(Real)((D)ax.z()*(D)cy.z() - (D)ay.z()*(D)cx.z()),
						(Real)((D)ax.w()*(D)cy.w() - (D)ay.w()*(D)cx.w()), };
					w = Real4 {
						(Real)((D)bx.x()*(D)ay.x() - (D)by.x()*(D)ax.x()),
						(Real)((D)bx.y()*(D)ay.y() - (D)by.y()*(D)ax.y()),
						(Real)((D)bx.z()*(D)ay.z() - (D)by.z()*(D)ax.z()),
						(Real)((D)bx.w()*(D)ay.w() - (D)by.w()*(D)ax.w()), };
				}

				Real4::Mask bad = ((u<0.0f) | (v<0.0f) | (w<0.0f)) & ((u>0.0f) | (v>0.0f) | (w>0.0f));
				if (!bad.all()) {
					uint32_t bad_mask = bad.mask();

					Real4 t = u*sz*v0z + v*sz*v1z + w*sz*v2z;

					uint32_t good_mask = (bad_mask ^ 0xf) & valid_mask;
					do {
						assert(good_mask);
						uint32_t i = first_set4(good_mask);
						good_mask &= good_mask - 1;

						Real ui = u.get(i), vi = v.get(i), wi = w.get(i), ti = t.get(i);

						Real det = ui + vi + wi;
						if (det == 0.0f) continue;
						if (det < 0.0f && (ti >= 0.0f || ti < ray_max_t * det)) continue;
						if (det > 0.0f && (ti <= 0.0f || ti > ray_max_t * det)) continue;

						uint32_t index = scene.triangle_indices[bvh->leaf.triangle_offset + (uint32_t)(base * 4 + i)];

						Real rcp_det = 1.0f / det;
						ray_max_t = ti * rcp_det;
						hit = { ti * rcp_det, ui * rcp_det, vi * rcp_det, index, hit.steps };

						if (shadow) return hit;
					} while (good_mask);
				}

				valid_mask >>= 4;
			}
		}

		for (;;) {
			if (stack_top == 0) return hit;
			stack_top--;
			if (stack_t[stack_top] < ray_max_t) {
				bvh = stack_bvh[stack_top];
				picort_prefetch((const char*)bvh->common.data + 0*64);
				picort_prefetch((const char*)bvh->common.data + 1*64);
				picort_prefetch((const char*)bvh->common.data + 2*64);
				picort_prefetch((const char*)bvh->common.data + 3*64);
				break;
			}
		}
	}
}

void create_node4(Scene4 &scene, size_t dst_ix, const BVH &src)
{
	BVH4 dst = { 0 };
	if (src.num_triangles > 0) {
		size_t base = scene.vertices.size() > 12 ? scene.vertices.size() - 12 : 0;
		dst.leaf.tag_zero = 0;
		uint8_t min_offset = UINT8_MAX;
		dst.leaf.triangle_offset = (uint32_t)scene.triangle_indices.size();
		dst.leaf.num_triangles = (uint8_t)src.num_triangles;

		for (uint32_t tri_ix = 0; tri_ix < src.num_triangles; tri_ix++) {
			scene.triangle_indices.push_back((uint32_t)src.triangles[tri_ix].index);

			for (uint32_t vert_ix = 0; vert_ix < 3; vert_ix++) {
				Vec3 v = src.triangles[tri_ix].v[vert_ix];
				size_t i, count = scene.vertices.size();
				for (i = base; i < count; i++) {
					if (scene.vertices[i] == v) break;
				}
				i = count;
				if (i == count) scene.vertices.push_back(v);
				uint8_t offset = (uint8_t)(i - base);
				if (offset < min_offset) min_offset = offset;
				dst.leaf.triangles[tri_ix/4][vert_ix][tri_ix%4] = offset;
			}
		}

		for (uint32_t tri_ix = 0; tri_ix < src.num_triangles; tri_ix++) {
			for (uint32_t vert_ix = 0; vert_ix < 3; vert_ix++) {
				dst.leaf.triangles[tri_ix/4][vert_ix][tri_ix%4] -= min_offset;
			}
		}

		uint32_t last_ix = (uint32_t)src.num_triangles - 1;
		for (uint32_t tri_ix = (uint32_t)src.num_triangles; tri_ix < 16; tri_ix++) {
			for (uint32_t vert_ix = 0; vert_ix < 3; vert_ix++) {
				uint8_t v = dst.leaf.triangles[last_ix/4][vert_ix][last_ix%4];
				dst.leaf.triangles[tri_ix/4][vert_ix][tri_ix%4] = v;
			}
		}

		dst.build.data_index = base + min_offset;
	} else {
		struct BVHChild
		{
			const BVH *bvh;
			Bounds bounds;
		};

		size_t child_base = scene.bvhs.size();
		dst.build.data_index = (uint32_t)child_base;
		scene.bvhs.insert(scene.bvhs.end(), 4, BVH4{});

		BVHChild children[4];
		uint32_t num_children = 0;

		for (uint32_t i = 0; i < 2; i++) {
			const BVH &child = src.child_nodes[i];
			if (child.num_triangles > 0) {
				children[num_children++] = { &child, src.child_bounds[i] };
			} else {
				children[num_children++] = { &child.child_nodes[0], child.child_bounds[0] };
				children[num_children++] = { &child.child_nodes[1], child.child_bounds[1] };
			}
		}

		Bounds bounds;
		for (uint32_t i = 0; i < num_children; i++) {
			bounds.add(children[i].bounds);
		}


		Vec3 origin = bounds.center();
		uint8_t origin_scale_ix = real_to_scale(max_component(abs(origin)) / INT16_MAX);
		Real origin_scale = scale_to_real(origin_scale_ix);

		dst.node.origin_scale = origin_scale_ix;

		dst.node.origin[0] = (int16_t)(origin.x / origin_scale);
		dst.node.origin[1] = (int16_t)(origin.y / origin_scale);
		dst.node.origin[2] = (int16_t)(origin.z / origin_scale);

		origin = Vec3{ (Real)dst.node.origin[0], (Real)dst.node.origin[1], (Real)dst.node.origin[2] } * origin_scale;

		Vec3 max_delta;
		for (uint32_t i = 0; i < num_children; i++) {
			Vec3 v = max(abs(children[i].bounds.min - origin), abs(children[i].bounds.max - origin));
			max_delta = max(max_delta, v);
		}
		uint8_t bounds_scale_ix = real_to_scale(max_component(max_delta) / INT16_MAX);
		Real bounds_scale = scale_to_real(bounds_scale_ix);

		dst.node.bounds_scale = bounds_scale_ix;

		for (uint32_t i = 0; i < num_children; i++) {
			Vec3 min = children[i].bounds.min - origin;
			Vec3 max = children[i].bounds.max - origin;
			for (uint32_t c = 0; c < 3; c++) {
				dst.node.bounds[c][0][i] = (int16_t)floor_real(min[c] / bounds_scale - 0.03125f);
				dst.node.bounds[c][1][i] = (int16_t)ceil_real(max[c] / bounds_scale + 0.03125f);
			}
		}
		for (uint32_t i = num_children; i < 4; i++) {
			for (uint32_t c = 0; c < 3; c++) {
				dst.node.bounds[c][0][i] = INT16_MAX;
				dst.node.bounds[c][1][i] = INT16_MIN;
			}
		}

		for (uint32_t i = 0; i < num_children; i++) {
			create_node4(scene, child_base + i, *children[i].bvh);
		}
	}

	scene.bvhs[dst_ix] = dst;
}

Scene4 create_scene4(const BVH &root)
{
	Scene4 scene;
	scene.bvhs.emplace_back();
	create_node4(scene, 0, root);

	for (BVH4 &bvh : scene.bvhs) {
		if (bvh.common.tag_node) {
			bvh.node.children = scene.bvhs.data() + bvh.build.data_index;
		} else {
			bvh.leaf.vertices = scene.vertices.data() + bvh.build.data_index;
		}
	}

	return scene;
}

// -- Sampling

template <int Base>
inline Real radical_inverse(uint64_t a, Real offset) {
    const Real inv_base = 1.0f / (Real)Base;
    uint64_t rev_digits = 0;
    Real inv_base_n = 1.0f;
    while (a) {
        uint64_t next  = a / Base;
        uint64_t digit = a - next * Base;
        rev_digits = rev_digits * Base + digit;
        inv_base_n *= inv_base;
        a = next;
    }
    return fmod((Real)(rev_digits * inv_base_n + offset), 1.0f);
}

Vec2 halton_vec2(uint64_t a, const Vec2 &offset=Vec2{})
{
	return {
		radical_inverse<2>(a, offset.x),
		radical_inverse<3>(a, offset.y),
	};
}

Vec3 halton_vec3(uint64_t a, const Vec3 &offset=Vec3{})
{
	return {
		radical_inverse<2>(a, offset.x),
		radical_inverse<3>(a, offset.y),
		radical_inverse<5>(a, offset.z),
	};
}

Vec3 cosine_hemisphere_sample(const Vec2 &uv)
{
	Real r = sqrt(uv.x);
	Real t = 2.0f*Pi*uv.y;
	return { r * cos(t), r * sin(t), sqrt_safe(1.0f - r) };
}

// Probability distribution for `cosine_hemisphere_sample()`
Real cosine_hemisphere_pdf(const Vec3 &wi)
{
	return wi.z / Pi;
}

Vec3 uniform_sphere_sample(const Vec2 &uv)
{
	Real t = 2.0f*Pi*uv.x;
	Real p = 2.0f*uv.y - 1.0f, q = sqrt_safe(1.0f - p*p);
	return { q * cos(t), q * sin(t), p };
}

struct Basis
{
	Vec3 x, y, z;

	Vec3 to_basis(const Vec3 &v) const { return { dot(x, v), dot(y, v), dot(z, v) }; }
	Vec3 to_world(const Vec3 &v) const { return x*v.x + y*v.y + z*v.z; }
};

Basis basis_normal(const Vec3 &axis_z)
{
	Vec3 z = normalize(axis_z);
	Real t = sqrt(z.x*z.x + z.y*z.y);
	Vec3 x = t > 0.0f ? Vec3{-z.y/t, z.x/t, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
	Vec3 y = normalize(cross(z, x));
	return { x, y, z };
}

// -- Material

Vec3 schlick_f(const Vec3 &f0, const Vec3 &wo)
{
	Real x = 1.0f - wo.z;
	return f0 + (Vec3{1.0f,1.0f,1.0f} - f0) * (x*x*x*x*x);
}

Real ggx_d(const Vec3 &wm, Real a2)
{
	Real x = wm.z * wm.z * (a2 - 1.0f) + 1.0f;
	return a2 / (Pi * x * x);
}

Real ggx_g1(const Vec3 &wo, Real a2)
{
	Real x = sqrt_safe(a2 + (1.0f - a2) * wo.z * wo.z) + wo.z;
	return (2.0f * wo.z) / x;
}

Real ggx_g2(const Vec3 &wo, const Vec3 &wi, Real a2)
{
	Real x = wo.z * sqrt_safe(a2 + (1.0f - a2) * wi.z * wi.z);
	Real y = wi.z * sqrt_safe(a2 + (1.0f - a2) * wo.z * wo.z);
	return (2.0f * wo.z * wi.z) / (x + y);
}

// Sample a visible normal vector for `uv`
// http://www.jcgt.org/published/0007/04/01/paper.pdf
Vec3 ggx_vndf_wm_sample(const Vec3 &wo, Real a, const Vec2 &uv)
{
	Basis basis = basis_normal(Vec3{a*wo.x, a*wo.y, wo.z});
	Real x = 1.0f / (1.0f + basis.z.z);
	Real r = sqrt_safe(uv.x);
	Real t = uv.y < x ? uv.y/x * Pi : Pi + (uv.y-x) / (1.0f-x) * Pi;
	Vec2 p { r*cos(t), r*sin(t)*(uv.y < x ? 1.0f : basis.z.z) };
	Vec3 n = basis.to_world(Vec3{ p.x, p.y, sqrt_safe(1.0f - p.x*p.x - p.y*p.y) });
	return normalize(Vec3{a*n.x, a*n.y, max(n.z, 0.0f)});
}

// Probability distribution for `ggx_vndf_wm_sample()`
// http://www.jcgt.org/published/0007/04/01/paper.pdf eq. (17) and (3)
Real ggx_vndf_wm_pdf(const Vec3 &wo, const Vec3 &wm, Real a2)
{
	// TODO: dot(wo, wm) / wo.z^2?
	return (ggx_g1(wo, a2) * ggx_d(wm, a2)) / (wo.z * 4.0f);
}

struct Texture
{
	Vec3 value;
	Image image;
};

struct Material
{
	bool has_alpha = false;
	Texture base_factor;
	Texture base_color;
	Texture roughness;
	Texture metallic;
	Texture emission_factor;
	Texture emission_color;
};

struct Light
{
	Vec3 position;
	Vec3 color;
	Real radius = 0.0f;
	bool directional = false;
};

struct Surface
{
	Vec3 diffuse;
	Vec3 specular;
	Vec3 emission;
	Real alpha = 1.0f;
	Real roughness = 0.0f;
};

Vec3 eval_texture(const Texture &texture, const Vec2 &uv)
{
	if (texture.image.width > 0) {
		Real4 v = sample_image(texture.image, uv);
		return { v.x(), v.y(), v.z() };
	}
	return texture.value;
}

Surface eval_surface(const Material &material, const Vec2 &uv)
{
	Vec3 base_color = eval_texture(material.base_color, uv) * eval_texture(material.base_factor, uv).x;
	Vec3 emission = eval_texture(material.emission_color, uv) * eval_texture(material.emission_factor, uv).x;
	Real metallic = eval_texture(material.metallic, uv).x;
	Surface s;
	s.diffuse = lerp(base_color, Vec3{}, metallic);
	s.specular = lerp(Vec3{0.04f,0.04f,0.04f}, base_color, metallic);
	s.emission = emission;
	s.roughness = max(0.05f, eval_texture(material.roughness, uv).x); // TODO??
	return s;
}

Vec3 surface_wi_sample(const Vec3 &uvw, const Surface &surface, const Vec3 &wo, Real *out_pdf)
{
	Real a = surface.roughness * surface.roughness, a2 = a * a;
	Vec3 f = schlick_f(surface.specular, wo);

	Real kd = length(surface.diffuse), ks = length(f);
	Real pd = kd / (kd + ks), ps = 1.0f - pd;

	Vec3 wm, wi;
	if (uvw.z <= pd) {
		wi = cosine_hemisphere_sample(Vec2{ uvw.x, uvw.y });
		wm = normalize(wi + wo);
	} else {
		wm = ggx_vndf_wm_sample(wo, a, Vec2{ uvw.x, uvw.y });
		wi = reflect(wm, wo);
	}
	if (wi.z <= 0.0f) return Vec3{};

	*out_pdf = pd * cosine_hemisphere_pdf(wi) + ps * ggx_vndf_wm_pdf(wo, wm, a2);
	return wi;
}

Real surface_wi_pdf(const Surface &surface, const Vec3 &wo, const Vec3 &wi)
{
	// TODO: Cache these
	if (wi.z <= 0.0f) return 0.0f;
	Real a = surface.roughness * surface.roughness, a2 = a * a;
	Vec3 f = schlick_f(surface.specular, wo);
	Vec3 wm = normalize(wi + wo);
	Real kd = length(surface.diffuse), ks = length(f);
	Real pd = kd / (kd + ks), ps = 1.0f - pd;
	return pd * cosine_hemisphere_pdf(wi) + ps * ggx_vndf_wm_pdf(wo, wm, a2);
}

Vec3 surface_brdf(const Surface &surface, const Vec3 &wo, const Vec3 &wi)
{
	Real a = surface.roughness * surface.roughness, a2 = a * a;
	Vec3 wm = normalize(wi + wo);
	Vec3 f = schlick_f(surface.specular, wo);

	Vec3 dif = surface.diffuse * (wi.z / Pi);

	Real g = ggx_g2(wo, wi, a2);
	Real d = ggx_d(wm, a2);
	Vec3 spec = f * (d * g) / (4.0f * wo.z);

	return dif*(Vec3{1.0f,1.0f,1.0f} - f) + spec;
}

struct TriangleInfo
{
	Vec3 v[3];
	Vec2 uv[3];
	Vec3 normal[3];
	uint32_t material = 0;
};

struct Camera
{
	Vec3 origin;
	Vec3 forward;
	Vec3 planeX;
	Vec3 planeY;
};

static const uint32_t SkyGridX = 128;
static const uint32_t SkyGridY = 64;
static const Vec2 SkyGridCell = { 2.0f * Pi / (Real)SkyGridX, Pi / (Real)SkyGridY };

struct Scene
{
	Camera camera;
	BVH *root = nullptr;
	TriangleInfo *triangles = nullptr;
	Material *materials = nullptr;

	Image sky;
	Real sky_factor = 0.0f;
	Real sky_rotation = 0.0f;
	std::vector<Real> sky_grid;

	Light *lights = nullptr;
	size_t num_lights = 0;

	Real exposure;
	Vec3 indirect_clamp;
	int indirect_depth;
	bool bvh_heatmap;

	Scene4 scene4;
};

Vec3 shade_sky(Scene &scene, const Ray &ray)
{
	Vec3 sky;
	if (scene.sky.width > 0) {
		Vec3 dir = normalize(ray.direction);
		Vec2 uv = {
			(atan2(dir.z, dir.x) + scene.sky_rotation) * (0.5f/Pi),
			0.99f - acos(clamp(dir.y, -1.0f, 1.0f)) * (0.98f/Pi),
		};
		Real4 v = sample_image(scene.sky, uv);
		sky = Vec3{ v.x(), v.y(), v.z() };
	} else {
		sky = (Vec3{ 0.05f, 0.05f, 0.05f }
			+ Vec3{ 1.0f, 1.0f, 1.3f } * max(ray.direction.y, 0.0f)
			+ Vec3{ 0.12f, 0.15f, 0.0f } * max(-ray.direction.y, 0.0f)) * 0.03f;
	}
	return sky * scene.sky_factor;
}

void init_sky_grid(Scene &scene)
{
	constexpr size_t num_samples = 128;

	Real total_weight = 0.0f;
	scene.sky_grid.resize(1 + SkyGridX * SkyGridY);
	Real *sky_grid = scene.sky_grid.data();

	for (size_t i = 0; i < SkyGridX * SkyGridY; i++) {
		Vec2 base = { (Real)(i % SkyGridX), (Real)(i / SkyGridX) };

		Real weight = 0.0f;
		for (size_t j = 0; j < num_samples; j++) {
			Vec2 uv = (base + halton_vec2(j)) * SkyGridCell;
			Real sin_y = sin(uv.y);
			Vec3 dir = { sin_y * cos(uv.x), cos(uv.y), sin_y * sin(uv.x) };
			Ray ray = { { }, dir };
			Vec3 sky = shade_sky(scene, ray);
			weight += (0.001f + sin_y) * (0.001f + sqrt(length(sky)));
		}

		weight /= (Real)num_samples;
		total_weight += weight;
		sky_grid[1 + i] = weight;
	}

	for (size_t i = 0; i < SkyGridX * SkyGridY; i++) {
		sky_grid[1 + i] /= total_weight;
	}

	for (size_t i = 0; i < SkyGridX * SkyGridY; i++) {
		sky_grid[1 + i] += sky_grid[i];
	}
}

Vec3 sky_grid_sample(Scene &scene, Vec3 uvw, Real *out_pdf)
{
	Real w = uvw.z;
	Real *weight = std::lower_bound(scene.sky_grid.data(), scene.sky_grid.data() + scene.sky_grid.size(), w);
	uint32_t ix = (uint32_t)(weight - scene.sky_grid.data());
	if (ix == 0) ix = 1;
	if (ix >= scene.sky_grid.size()) ix = (uint32_t)scene.sky_grid.size() - 1;
	Vec2 base = { (Real)(ix % SkyGridX), (Real)(ix / SkyGridX) };
	Vec2 uv = (base + Vec2{ uvw.x, uvw.y }) * SkyGridCell;
	Real sin_y = sin(uv.y);
	Vec3 dir = { sin_y * cos(uv.x), cos(uv.y), sin_y * sin(uv.x) };
	*out_pdf = (weight[0] - weight[-1]) * (Real)(SkyGridX * SkyGridY) / (sin_y * (2.0f * Pi * Pi));
	return dir;
}

Real sky_grid_pdf(Scene &scene, Vec3 dir)
{
	Vec2 uv = {
		(atan2(dir.z, dir.x)) * (0.5f/Pi),
		acos(clamp(dir.y, -1.0f, 1.0f)) * (1.0f/Pi),
	};

	uint32_t x = (uint32_t)((uv.x + 1.0f) * (Real)SkyGridX) % SkyGridX;
	uint32_t y = std::min((uint32_t)(uv.y * (Real)SkyGridY), SkyGridY);
	uint32_t ix = y * SkyGridX + x;
	Real *weight = scene.sky_grid.data() + ix;
	Real sin_y = sqrt_safe(1.0f - dir.y * dir.y);
	return (weight[0] - weight[-1]) * (Real)(SkyGridX * SkyGridY) / (sin_y * (2.0f * Pi * Pi));
}

RayHit trace_ray_imp(Scene &scene, const Ray &ray, Real max_t=Inf, bool shadow=false)
{
#if 0
	RayTrace trace = setup_trace(ray);
	intersect_bvh(trace, *scene.root);
	return trace.hit;
#else
	return intersect4(scene.scene4, ray, max_t, shadow);
#endif
}

RayHit trace_ray(Random &rng, Scene &scene, const Ray &ray, Real max_t=Inf, bool shadow=false)
{
	RayHit hit = { };
	Real t_offset = 0.0f;
	uint32_t depth = 0;
	for (;;) {
		depth++;
		Ray test_ray = { ray.origin + ray.direction * t_offset, ray.direction };
		hit = trace_ray_imp(scene, test_ray, max_t, shadow && depth == 1);
		if (hit.index == SIZE_MAX || depth > 64) break;

		const TriangleInfo &tri = scene.triangles[hit.index];
		const Material &material = scene.materials[tri.material];
		if (!material.has_alpha) break;

		if (depth == 1 && shadow) continue;

		Real u = hit.u, v = hit.v, w = 1.0f - u - v;
		Vec2 uv = tri.uv[0]*u + tri.uv[1]*v + tri.uv[2]*w;
		Real alpha = sample_image(material.base_color.image, uv).w();
		if (uniform_real(rng) < alpha) break;

		t_offset += hit.t * 1.001f + 0.00001f;
	}

	hit.t += t_offset;
	return hit;
}

Vec3 trace_path(Random &rng, Scene &scene, const Ray &ray, const Vec3 &uvw, int depth=0)
{
	if (depth > scene.indirect_depth) return { };

	RayHit hit = trace_ray(rng, scene, ray);
	if (scene.bvh_heatmap) {
		return Vec3{ 0.005f, 0.003f, 0.1f } * (Real)hit.steps / scene.exposure;
	}

	if (hit.t >= Inf) {
		return depth == 0 ? shade_sky(scene, ray) : Vec3{ };
	}

	Real u = hit.u, v = hit.v, w = 1.0f - u - v;
	const TriangleInfo &tri = scene.triangles[hit.index];

	Vec3 pos = ray.origin + ray.direction * (hit.t * 0.9999f);
	Vec2 uv = tri.uv[0]*u + tri.uv[1]*v + tri.uv[2]*w;
	Vec3 normal = tri.normal[0]*u + tri.normal[1]*v + tri.normal[2]*w;

	Basis basis = basis_normal(normal);

	Vec3 wo = basis.to_basis(-ray.direction);
	if (wo.z <= 0.001f) {
		wo.z = 0.001f;
		wo = normalize(wo);
	}

	const Material &material = scene.materials[tri.material];
	Surface surface = eval_surface(material, uv);

	Vec3 li = surface.emission;

	for (size_t i = 0; i < scene.num_lights; i++) {
		const Light &light = scene.lights[i];

		Real max_t = 1.0f;
		Vec3 delta;
		if (light.directional) {
			max_t = Inf;
			delta = light.position;
		} else {
			Vec3 p = light.position + uniform_sphere_sample(uniform_vec2(rng)) * light.radius;
			delta = p - pos;
			max_t = length(delta);
		}

		Vec3 dir = normalize(delta);
		Vec3 wi = basis.to_basis(dir);
		if (wi.z <= 0.0f) continue;

		Ray shadow_ray = { pos, dir };
		RayHit shadow_hit = trace_ray(rng, scene, shadow_ray, max_t, true);
		if (shadow_hit.t < max_t) continue;

		Real att = light.directional ? 1.0f : max(dot(delta, delta), 0.01f);
		li = li + light.color * surface_brdf(surface, wo, wi) / att;
	}

	{
		Real grid_pdf = 0.0f;
		Vec3 grid_dir = sky_grid_sample(scene, uvw, &grid_pdf);
		Vec3 grid_wi = basis.to_basis(grid_dir);
		Vec3 grid_li;

		Real brdf_pdf = 0.0f;
		Vec3 brdf_wi = surface_wi_sample(uvw, surface, wo, &brdf_pdf);
		Vec3 brdf_dir = basis.to_world(brdf_wi);
		Vec3 brdf_li;

		Real grid_brdf_pdf = sky_grid_pdf(scene, brdf_dir);
		Real brdf_grid_pdf = surface_wi_pdf(surface, wo, grid_wi);

		if (grid_wi.z >= 0.0f) {
			Ray shadow_ray = { pos, grid_dir };
			RayHit shadow_hit = trace_ray(rng, scene, shadow_ray, Inf, true);
			if (shadow_hit.t >= Inf) {
				Vec3 sky = shade_sky(scene, shadow_ray);
				grid_li = surface_brdf(surface, wo, grid_wi) * sky;
			}
		}

		if (brdf_wi.z >= 0.0f && brdf_pdf > 0.0f) {
			Ray shadow_ray = { pos, brdf_dir };
			RayHit shadow_hit = trace_ray(rng, scene, shadow_ray, Inf, true);
			if (shadow_hit.t >= Inf) {
				Vec3 sky = shade_sky(scene, shadow_ray);
				brdf_li = surface_brdf(surface, wo, brdf_wi) * sky;
			}
		}

		Vec3 result = grid_li / (grid_pdf + brdf_grid_pdf) + brdf_li / (brdf_pdf + grid_brdf_pdf);
		li = li + result;
	}

	{
		Real pdf = 0.0f;
		Vec3 wi = surface_wi_sample(uniform_vec3(rng), surface, wo, &pdf);
		if (pdf == 0.0f) return li;

		Vec3 next_dir = basis.to_world(wi);
		Ray next_ray = { pos - ray.direction * 0.0001f, next_dir };
		Vec3 uvw = uniform_vec3(rng);
		Vec3 l = trace_path(rng, scene, next_ray, uvw, depth + 1);
		l = surface_brdf(surface, wo, wi) * l / pdf;
		l = min(l, scene.indirect_clamp);
		return li + l;
	}
}

struct Framebuffer
{
	std::vector<Pixel> pixels;
	uint32_t width = 0, height = 0;

	Framebuffer() { }
	Framebuffer(uint32_t width, uint32_t height)
		: pixels(width * height), width(width), height(height) { }
};

struct ImagerTracer
{
	Scene scene;
	Pixel *pixels;
	uint32_t width, height;
	size_t num_samples;
	uint64_t seed;
	std::atomic_uint32_t a_counter { 0 };
	std::atomic_uint32_t a_workers { 0 };
};

Ray camera_ray(Camera &camera, Vec2 uv)
{
	Ray ray;
	ray.origin = camera.origin;
	ray.direction = normalize(camera.forward
		+ camera.planeX*(uv.x*2.0f - 1.0f)
		+ camera.planeY*(uv.y*-2.0f + 1.0f));
	return ray;
}

void trace_image(ImagerTracer &tracer, bool print_status)
{
	static const size_t tile_size = 16;
	uint32_t num_tiles_x = (tracer.width + tile_size - 1) / tile_size;
	uint32_t num_tiles_y = (tracer.height + tile_size - 1) / tile_size;
	Vec2 resolution = { (Real)tracer.width, (Real)tracer.height };

	tracer.a_workers.fetch_add(1u, std::memory_order_relaxed);

	for (;;) {
		uint32_t tile_ix = tracer.a_counter.fetch_add(1u, std::memory_order_relaxed);
		if (tile_ix >= num_tiles_x * num_tiles_y) break;

		uint32_t tile_x = tile_ix % num_tiles_x;
		uint32_t tile_y = tile_ix / num_tiles_x;

		Random rng { (tracer.seed << 32u) + tile_ix };
		rng.next();

		for (uint32_t dy = 0; dy < tile_size; dy++)
		for (uint32_t dx = 0; dx < tile_size; dx++) {
			uint32_t x = tile_x * tile_size + dx, y = tile_y * tile_size + dy;
			if (x >= tracer.width || y >= tracer.height) continue;

			static constexpr uint32_t num_buckets = 6;
			Vec3 totals[num_buckets];
			Real weights[num_buckets] = { };

			Real exposure = tracer.scene.exposure;
			Vec3 offset = uniform_vec3(rng);

			for (size_t i = 0; i < tracer.num_samples; i++) {
				Vec2 aa = uniform_vec2(rng) - Vec2{ 0.5f, 0.5f };
				Vec2 uv = Vec2 { (Real)x + aa.x, (Real)y + aa.y } / resolution;

				Ray ray = camera_ray(tracer.scene.camera, uv);

				Vec3 uvw = halton_vec3(i, offset);
				Vec3 l = trace_path(rng, tracer.scene, ray, uvw) * exposure;
				l = min(l, Vec3{ 100.0f, 100.0f, 100.0f });

				uint32_t bucket = rng.next() % num_buckets;
				totals[bucket] = totals[bucket] + l;
				weights[bucket] += 1.0f;

#if 0
				Vec3 mean;
				for (uint32_t i = 0; i < num_buckets; i++) {
					mean = mean + totals[i] / (weights[i] * num_buckets);
				}
				Real variance = 0.0f;
				for (uint32_t i = 0; i < num_buckets; i++) {
					Vec3 delta = (mean - (totals[i] / weights[i])) / mean;
					variance += dot(delta, delta);
				}
				if (i > 64 && variance < 0.04f) {
					break;
				}
#endif

			}

			Vec3 color;
			Real weight = 0.0f;
			for (uint32_t i = 0; i < num_buckets; i++) {
				color = color + totals[i];
				weight = weight + weights[i];
			}
			color = color / weight;

			// Reinhard tonemap
			color = color / (Vec3{1.0f, 1.0f, 1.0f} + color);

			// TODO: sRGB
			color.x = sqrt(color.x);
			color.y = sqrt(color.y);
			color.z = sqrt(color.z);
			color = min(max(color, Vec3{0.0f,0.0f,0.0f}), Vec3{1.0f,1.0f,1.0f}) * 255.9f;

			size_t ix = y * tracer.width + x;
			tracer.pixels[ix] = { (uint8_t)color.x, (uint8_t)color.y, (uint8_t)color.z };
		}

		if (print_status) {
			verbosef("%u/%u\n", tile_ix+1, num_tiles_x*num_tiles_y);
		}
	}

	tracer.a_workers.fetch_sub(1u, std::memory_order_relaxed);
}

struct BmpHeaderRgba
{
	char data[122] =
		// magic  size    unused   data offset    DIB size   width  height
		"" "BM"  "????" "\0\0\0\0" "\x7a\0\0\0" "\x6c\0\0\0" "????" "????" 
		// 1-plane 32-bits  bitfields    data-size     print resolution
		"" "\x1\0" "\x20\0" "\x3\0\0\0"   "????"   "\x13\xb\0\0\x13\xb\0\0"
		//   palette counts            channel masks for RGBA            colors
		"" "\0\0\0\0\0\0\0\0" "\xff\0\0\0\0\xff\0\0\0\0\xff\0\0\0\0\xff" "sRGB";
			
	BmpHeaderRgba(size_t width=0, size_t height=0) {
		size_t size = width * height * 4;
		patch(0x02, 122 + size); // total size
		patch(0x12, width);      // width (left-to-right)
		patch(0x16, 0 - height); // height (top-to-bottom)
		patch(0x22, size);       // data size
	}

	void patch(size_t offset, size_t value) {
		data[offset+0] = (char)((value >>  0) & 0xff);
		data[offset+1] = (char)((value >>  8) & 0xff);
		data[offset+2] = (char)((value >> 16) & 0xff);
		data[offset+3] = (char)((value >> 24) & 0xff);
	}
};

#if GUI

struct GuiStateWin32
{
	HANDLE init_event = NULL;
	std::thread thread;
	HWND hwnd;

	UINT_PTR timer;
	std::recursive_mutex mutex;
	Framebuffer internal_framebuffer;
	const Framebuffer *framebuffer;
};

static GuiStateWin32 g_gui;

LRESULT CALLBACK win32_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	std::lock_guard<std::recursive_mutex> lg { g_gui.mutex };
	const Framebuffer *fb = g_gui.framebuffer;

	switch (uMsg)
	{
	case WM_CLOSE: DestroyWindow(hwnd); return 0;
	case WM_DESTROY: PostQuitMessage(0); return 0;
	case WM_TIMER: {
		if (fb) {
			RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_INVALIDATE);
		}
	} return 0;
	default: return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		if (fb) {
			RECT rect;
			GetClientRect(hwnd, &rect);
			int width = rect.right - rect.left, height = rect.bottom - rect.top;

			BmpHeaderRgba header { fb->width, fb->height };
			StretchDIBits(hdc, 0, 0, width, height, 0, 0, (int)fb->width, (int)fb->height,
				fb->pixels.data(), (BITMAPINFO*)(header.data + 0xe), DIB_RGB_COLORS, SRCCOPY);
		}

		EndPaint(hwnd, &ps);
	} return 0;
	}
}

void win32_gui_thread(const Framebuffer *fb)
{
	WNDCLASSW wc = { };
	wc.lpfnWndProc = &win32_wndproc;
	wc.hInstance = GetModuleHandleW(NULL);
	wc.lpszClassName = L"picort";
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	ATOM atom = RegisterClassW(&wc);

	DWORD style = WS_VISIBLE|WS_OVERLAPPEDWINDOW;
	RECT rect = { 0, 0, (int)fb->width, (int)fb->height };
	AdjustWindowRectEx(&rect, style, FALSE, 0);
	g_gui.hwnd = CreateWindowExW(0, MAKEINTATOM(atom), L"picort", style,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right-rect.left, rect.bottom-rect.top,
		NULL, NULL, wc.hInstance, NULL);

	SetEvent(g_gui.init_event);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0) != 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

void enable_gui(const Framebuffer *fb)
{
	if (!g_gui.init_event) {
		g_gui.internal_framebuffer = Framebuffer{ 1, 1 };
		g_gui.framebuffer = &g_gui.internal_framebuffer;
		g_gui.init_event = CreateEventW(NULL, FALSE, FALSE, NULL);
		g_gui.thread = std::thread{ win32_gui_thread, fb };
		WaitForSingleObject(g_gui.init_event, INFINITE);
	}

	std::lock_guard<std::recursive_mutex> lg { g_gui.mutex };
	g_gui.framebuffer = fb;
	if (!g_gui.timer) {
		g_gui.timer = SetTimer(g_gui.hwnd, NULL, 100, NULL);
	}
}

void disable_gui(Framebuffer &&fb)
{
	{
		std::lock_guard<std::recursive_mutex> lg { g_gui.mutex };
		g_gui.internal_framebuffer = std::move(fb);
		g_gui.framebuffer = &g_gui.internal_framebuffer;
		KillTimer(g_gui.hwnd, g_gui.timer);
		g_gui.timer = 0;
	}

	RedrawWindow(g_gui.hwnd, NULL, NULL, RDW_INVALIDATE | RDW_INVALIDATE);
}

void close_gui()
{
	PostMessageW(g_gui.hwnd, WM_CLOSE, 0, 0);
}

void wait_gui()
{
	g_gui.thread.join();
}

#else

void enable_gui(const Framebuffer *fb) { }
void disable_gui(Framebuffer &&fb) { }
void close_gui() { }
void wait_gui() { }

#endif

Vec3 from_ufbx(const ufbx_vec3 &v) { return { (Real)v.x, (Real)v.y, (Real)v.z }; }

bool ends_with(const std::string &str, const char *suffix)
{
	size_t len = strlen(suffix);
	return str.size() >= len && !str.compare(str.size() - len, len, suffix);
}

void setup_texture(Texture &texture, const ufbx_material_map &map)
{
	if (map.has_value) {
		texture.value = from_ufbx(map.value_vec3);
	}
	if (map.texture && map.texture->content.size > 0) {
		verbosef("Loading texture: %s\n", map.texture->relative_filename.data);

		texture.image = read_png(map.texture->content.data, map.texture->content.size);
		if (!texture.image.error) return;
		fprintf(stderr, "Failed to load %s: %s\n",
			map.texture->relative_filename.data, texture.image.error);
	}
	
	if (map.texture) {
		std::string path { map.texture->filename.data, map.texture->filename.length };
		if (!ends_with(path, ".png")) {
			size_t dot = path.rfind('.');
			if (dot != std::string::npos) {
				path = path.substr(0, dot) + ".png";
			}
			texture.image = load_png(path.c_str());
		}
	}
}

struct ProgressState
{
};

ufbx_progress_result progress(void *user, const ufbx_progress *progress)
{
	ProgressState *state = (ProgressState*)user;
	static int timer;
	if ((timer++ & 0xfff) == 0) {
		verbosef("%.1f/%.1f MB\n", (double)progress->bytes_read/1e6, (double)progress->bytes_total/1e6);
	}
	return UFBX_PROGRESS_CONTINUE;
}

struct alignas(8) OptBase
{
	const char *name, *desc, *alias;
	uint32_t num_args, size;
	bool defined = false;
	bool from_arg = false;

	OptBase(const char *name, const char *desc, const char *alias, uint32_t num_args, uint32_t size)
		: name(name), desc(desc), alias(alias), num_args(num_args), size(size) { }

	virtual void parse(const char **args) = 0;
	virtual int print(char *buf, size_t size) = 0;
};

template <typename T>
struct OptTraits { };

template <> struct OptTraits<bool> {
	enum { num_args = 0 };
	static bool parse(const char **args) { return true; }
	static int print(bool v, char *buf, size_t size) { return snprintf(buf, size, "%s", v ? "true" : "false"); }
};

template <> struct OptTraits<Real> {
	enum { num_args = 1 };
	static Real parse(const char **args) { return (Real)strtod(args[0], NULL); }
	static int print(Real v, char *buf, size_t size) { return snprintf(buf, size, "%g", v); }
};

template <> struct OptTraits<int> {
	enum { num_args = 1 };
	static int parse(const char **args) { return atoi(args[0]); }
	static int print(int v, char *buf, size_t size) { return snprintf(buf, size, "%d", v); }
};

template <> struct OptTraits<double> {
	enum { num_args = 1 };
	static double parse(const char **args) { return (double)strtod(args[0], NULL); }
	static int print(double v, char *buf, size_t size) { return snprintf(buf, size, "%g", v); }
};

template <> struct OptTraits<Vec2> {
	enum { num_args = 2 };
	static Vec2 parse(const char **args) {
		return { (Real)strtod(args[0], NULL), (Real)strtod(args[1], NULL) };
	}
	static int print(const Vec2 &v, char *buf, size_t size) { return snprintf(buf, size, "(%g, %g)", v.x, v.y); }
};

template <> struct OptTraits<Vec3> {
	enum { num_args = 3 };
	static Vec3 parse(const char **args) {
		return { (Real)strtod(args[0], NULL), (Real)strtod(args[1], NULL), (Real)strtod(args[2], NULL) };
	}
	static int print(const Vec3 &v, char *buf, size_t size) { return snprintf(buf, size, "(%g, %g, %g)", v.x, v.y, v.z); }
};

template <> struct OptTraits<std::string> {
	enum { num_args = 1 };
	static std::string parse(const char **args) { return { args[0] }; }
	static int print(const std::string &v, char *buf, size_t size) { return snprintf(buf, size, "%s", v.c_str()); }
};

struct StringPair { std::string v[2]; };

template <> struct OptTraits<StringPair> {
	enum { num_args = 2 };
	static StringPair parse(const char **args) { return { { { args[0] }, { args[1] } } }; }
	static int print(const StringPair &v, char *buf, size_t size) { return snprintf(buf, size, "(%s, %s)", v.v[0].c_str(), v.v[1].c_str()); }
};

template <typename T, typename Traits=OptTraits<T>>
struct Opt : OptBase
{
	T value;
	Opt(const char *name, const char *desc, T def = T{}, const char *alias=nullptr) : OptBase(name, desc, alias, Traits::num_args, sizeof(*this)), value(def) { }
	virtual void parse(const char **args) override {
		value = Traits::parse(args);
	}
	virtual int print(char *buf, size_t size) override {
		return Traits::print(value, buf, size);
	}
};

struct OptIter
{
	OptBase *ptr;
	OptIter(void *ptr) : ptr((OptBase*)ptr) { }
	OptIter &operator++() { ptr = (OptBase*)((char*)ptr + ptr->size); return *this; }
	OptIter operator++(int) { OptIter it = *this; ptr = (OptBase*)((char*)ptr + ptr->size); return it; }
	OptBase &operator*() const { return *ptr; }
	OptBase *operator->() const { return ptr; }
	bool operator==(const OptIter &rhs) const { return ptr == rhs.ptr; }
	bool operator!=(const OptIter &rhs) const { return ptr != rhs.ptr; }
};

struct Opts
{
	Opt<bool> help { "help", "Display this help", false, "h" };
	Opt<std::string> input { "input", "Input .fbx file (does not need -i if the path doesn't start with a '-')", "", "i" };
	Opt<std::string> output { "output", "Output .bmp file (defaults to 'picort-output.bmp')", "picort-output.bmp", "o" };
	Opt<bool> verbose { "verbose", "Enable verbose output", false, "v" };
	Opt<std::string> camera { "camera", "Camera name (defaults to 'picort_camera')", "picort_camera" };
	Opt<int> samples { "samples", "Samples to use while rendering", 64 };
	Opt<int> bounces { "bounces", "Number of indirect bounces", 1 };
	Opt<int> threads { "threads", "Threads to use while rendering (0 for auto-detect)" };
	Opt<std::string> animation { "animation", "Name of the animation to use" };
	Opt<double> frame { "frame", "Time in frames (can be fractional)", 1 };
	Opt<int> num_frames { "num-frames", "Number of frames to render (starting from --frame)", 1 };
	Opt<int> frame_offset { "frame-offset", "Frame number to start rendering from", 0 };
	Opt<double> fps { "fps", "Frames per second for --num-frames (overrides one set in FBX file)" };
	Opt<Vec2> resolution { "resolution", "Resolution", { 512.0f, 512.0f } };
	Opt<double> resolution_scale { "resolution-scale", "Scale factor to resolution", 1.0 };
	Opt<bool> gui { "gui", "Show a GUI window" };
	Opt<bool> keep_open { "keep-open", "Keep the GUI open" };
	Opt<double> time { "time", "Time in seconds" };
	Opt<Real> scene_scale { "scene-scale", "Global scene scale", 1.0f };
	Opt<Vec3> camera_position { "camera-position", "World space camera position", { 0.0f, 0.0f, 10.0f } };
	Opt<Vec3> camera_direction { "camera-direction", "World space camera direction", { 0.0f, 0.0f, -1.0f } };
	Opt<Vec3> camera_target { "camera-target", "World space camera target", { 0.0f, 0.0f, 0.0f } };
	Opt<Vec3> camera_up { "camera-direction", "World space camera up direction", { 0.0f, 1.0f, 0.0f } };
	Opt<Real> camera_fov { "camera-fov", "Camera field of view in degrees", 60.0f };
	Opt<std::string> sky { "sky", "Sky .png file" };
	Opt<Real> sky_exposure { "sky-exposure", "Exposure of the sky", 5.0f };
	Opt<Real> sky_rotation { "sky-rotation", "Rotation of the sky in degrees" };
	Opt<Real> exposure { "exposure", "Exposure for the camera", 3.0f };
	Opt<Real> indirect_clamp { "indirect-clamp", "Clamping for indirect samples", 10.0f };
	Opt<std::string> base_path { "base-path", "Base directory to look up files from" };
	Opt<StringPair> compare { "compare", "Compare two result images" };
	Opt<std::string> reference { "reference", "Compare the resulting image to a reference" };
	Opt<double> error_threshold { "error-threshold", "Error threshold (MSE) for comparison", 0.05 };
	Opt<bool> bvh_heatmap { "bvh-heatmap", "Visualize BVH heatmap" };

	OptIter begin() { return this; }
	OptIter end() { return this + 1; }
};

std::string get_path(const Opts &opts, const Opt<std::string> &opt)
{
	if (opts.base_path.defined && !opt.from_arg) {
		return opts.base_path.value + opt.value;
	} else {
		return opt.value;
	}
}

void render_frame(ufbx_scene *original_scene, const Opts &opts, int frame_offset=0)
{
	std::vector<Triangle> triangles;
	std::vector<TriangleInfo> triangle_infos;
	std::vector<Material> materials;
	std::vector<Light> lights;
	Camera camera;

	bool has_time = opts.time.defined;
	double time = opts.time.value;
	if (opts.frame.defined) {
		time = opts.frame.value / original_scene->settings.frames_per_second;
		has_time = true;
	}

	if (frame_offset > 0) {
		double fps = opts.fps.defined ? opts.fps.value : original_scene->settings.frames_per_second;
		time += frame_offset / fps;
	}

	ufbx_scene *scene;
	if (has_time) {
		ufbx_evaluate_opts eval_opts = { };
		eval_opts.evaluate_skinning = true;
		eval_opts.evaluate_caches = true;
		eval_opts.load_external_files = true;

		ufbx_anim anim = original_scene->anim;
		if (opts.animation.defined) {
			ufbx_anim_stack *stack = (ufbx_anim_stack*)ufbx_find_element(original_scene, UFBX_ELEMENT_ANIM_STACK, opts.animation.value.c_str());
			if (stack) {
				anim = stack->anim;
			}
		}

		ufbx_error error;
		scene = ufbx_evaluate_scene(original_scene, &anim, time, &eval_opts, &error);
		if (!scene) {
			char buf[4096];
			ufbx_format_error(buf, sizeof(buf), &error);
			fprintf(stderr, "%s\n", buf);
			exit(1);
		}
	} else {
		scene = original_scene;
	}

	materials.resize(scene->materials.count + 1);

	// Reserve one undefined material
	materials[0].base_factor.value.x = 1.0f;
	materials[0].base_color.value = Vec3{ 0.8f, 0.8f, 0.8f };
	materials[0].roughness.value.x = 0.5f;
	materials[0].metallic.value.x = 0.0f;

	verbosef("Processing materials: %zu\n", scene->materials.count);

	for (size_t i = 0; i < scene->materials.count; i++) {
		ufbx_material *mat = scene->materials.data[i];
		Material &dst = materials[i + 1];
		dst.base_factor.value.x = 1.0f;
		setup_texture(dst.base_factor, mat->pbr.base_factor);
		setup_texture(dst.base_color, mat->pbr.base_color);
		setup_texture(dst.roughness, mat->pbr.roughness);
		setup_texture(dst.metallic, mat->pbr.metalness);
		setup_texture(dst.emission_factor, mat->pbr.emission_factor);
		setup_texture(dst.emission_color, mat->pbr.emission_color);
		dst.base_color.image.srgb = true;
		dst.emission_color.image.srgb = true;

		if (dst.base_color.image.width > 0) {
			uint32_t num_pixels = dst.base_color.image.width * dst.base_color.image.height;
			const Pixel16 *pixels = dst.base_color.image.pixels.data();
			for (uint32_t i = 0; i < num_pixels; i++) {
				if (pixels[i].a < 0xffff) {
					dst.has_alpha = true;
					break;
				}
			}
		}
	}

	verbosef("Processing meshes: %zu\n", scene->meshes.count);

	uint32_t indices[128];
	for (ufbx_mesh *original_mesh : scene->meshes) {
		if (original_mesh->instances.count == 0) continue;
		ufbx_mesh *mesh = ufbx_subdivide_mesh(original_mesh, 0, NULL, NULL);

		// Iterate over all instances of the mesh
		for (ufbx_node *node : mesh->instances) {
			if (!node->visible) continue;

			verbosef("%s: %zu triangles\n", node->name.data, mesh->num_triangles);

			ufbx_matrix normal_to_world = ufbx_matrix_for_normals(&node->geometry_to_world);

			// Iterate over all the N-gon faces of the mesh
			for (size_t face_ix = 0; face_ix < mesh->num_faces; face_ix++) {

				// Split each face into triangles
				size_t num_tris = ufbx_triangulate_face(indices, 128, mesh, mesh->faces[face_ix]);

				// Iterate over reach split triangle
				for (size_t tri_ix = 0; tri_ix < num_tris; tri_ix++) {
					Triangle tri;
					TriangleInfo info;
					tri.index = triangles.size();

					if (mesh->face_material.count > 0) {
						ufbx_material *mat = mesh->materials.data[mesh->face_material[face_ix]].material;
						info.material = mat->element.typed_id + 1;
					}

					for (size_t corner_ix = 0; corner_ix < 3; corner_ix++) {
						uint32_t index = indices[tri_ix*3 + corner_ix];

						// Load the skinned vertex position at `index`
						ufbx_vec3 v = ufbx_get_vertex_vec3(&mesh->skinned_position, index);

						ufbx_vec2 uv = mesh->vertex_uv.exists ? ufbx_get_vertex_vec2(&mesh->vertex_uv, index) : ufbx_vec2{};
						ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->skinned_normal, index);

						// If the skinned positions are local we must apply `to_root` to get
						// to world coordinates
						if (mesh->skinned_is_local) {
							v = ufbx_transform_position(&node->geometry_to_world, v);
							n = ufbx_transform_direction(&normal_to_world, n);
						}

						info.v[corner_ix] = tri.v[corner_ix] = { (Real)v.x, (Real)v.y, (Real)v.z };
						info.uv[corner_ix] = { (Real)uv.x, (Real)uv.y };
						info.normal[corner_ix] = normalize({ (Real)n.x, (Real)n.y, (Real)n.z });
					}

					triangles.push_back(tri);
					triangle_infos.push_back(info);
				}
			}
		}

		// Free the potentially subdivided mesh
		ufbx_free_mesh(mesh);
	}

	for (ufbx_light *light : scene->lights) {

		// Iterate over all instances of the light
		for (ufbx_node *node : light->instances) {
			Light l;

			ufbx_prop *radius = ufbx_find_prop(&light->props, "Radius");
			if (!radius) radius = ufbx_find_prop(&node->props, "Radius");

			if (light->type == UFBX_LIGHT_DIRECTIONAL) {
				ufbx_vec3 dir = ufbx_transform_direction(&node->node_to_world, light->local_direction);
				l.position = normalize(-from_ufbx(dir));
				l.directional = true;
			} else {
				l.position = from_ufbx(node->world_transform.translation);
			}

			l.color = from_ufbx(light->color) * (Real)light->intensity;
			if (radius) l.radius = (Real)radius->value_real;

			lights.push_back(l);
		}
	}

	uint32_t width = (uint32_t)opts.resolution.value.x, height = (uint32_t)opts.resolution.value.y;

	{
		ufbx_node *camera_node = ufbx_find_node(scene, opts.camera.value.c_str());
		if (!camera_node && scene->cameras.count > 0) {
			camera_node = scene->cameras[0]->instances.data[0];
		}

		if (camera_node && camera_node->camera && !opts.camera_position.defined) {
			ufbx_camera *cam = camera_node->camera;

			Vec3 m0 = normalize(from_ufbx(camera_node->node_to_world.cols[0]));
			Vec3 m1 = normalize(from_ufbx(camera_node->node_to_world.cols[1]));
			Vec3 m2 = normalize(from_ufbx(camera_node->node_to_world.cols[2]));
			Vec3 m3 = from_ufbx(camera_node->node_to_world.cols[3]);

			if (!opts.resolution.defined) {
				if (cam->resolution_is_pixels) {
					width = (uint32_t)round(cam->resolution.x);
					height = (uint32_t)round(cam->resolution.y);
				} else if (cam->aspect_mode != UFBX_ASPECT_MODE_WINDOW_SIZE) {
					width = (uint32_t)((double)width * cam->resolution.x / cam->resolution.y);
				}
			}

			camera.planeX = m2 * (Real)cam->field_of_view_tan.x;
			camera.planeY = m1 * (Real)cam->field_of_view_tan.y;
			camera.forward = m0;
			camera.origin = m3;
		} else {
			Vec3 forward = normalize(opts.camera_direction.value);
			if (opts.camera_target.defined) {
				forward = normalize(opts.camera_target.value - opts.camera_position.value);
			}
			Vec3 right = normalize(cross(forward, opts.camera_up.value));
			Vec3 up = normalize(cross(right, forward));
			Real aspect = (Real)width / (Real)height;
			Real tan_fov = tan(opts.camera_fov.value * 0.5f * (Pi / 180.0f));

			camera.planeX = right * tan_fov * aspect;
			camera.planeY = up * tan_fov;
			camera.forward = forward;
			camera.origin = opts.camera_position.value;
		}
	}

	double res_scale = opts.resolution_scale.value;
	if (res_scale != 1.0) {
		width = (uint32_t)(width * res_scale);
		height = (uint32_t)(height * res_scale);
	}

	verbosef("Building BVH: %zu triangles\n", triangles.size());

	BVH bvh = build_bvh(triangles.data(), triangles.size());

	Framebuffer framebuffer { width, height };

	ImagerTracer tracer;
	tracer.scene.root = &bvh;
	tracer.scene.triangles = triangle_infos.data();
	tracer.scene.materials = materials.data();
	tracer.scene.camera = camera;
	tracer.scene.lights = lights.data();
	tracer.scene.num_lights = lights.size();
	if (opts.sky.defined) {
		std::string path = get_path(opts, opts.sky);
		tracer.scene.sky = load_png(path.c_str());
		tracer.scene.sky.srgb = true;
	}
	tracer.scene.sky_factor = pow((Real)2.0, opts.sky_exposure.value);
	tracer.scene.sky_rotation = opts.sky_rotation.value * (Pi/180.0f);
	tracer.scene.exposure = pow((Real)2.0, opts.exposure.value);
	tracer.scene.indirect_clamp = Vec3{ 1.0f, 1.0f, 1.0f } * opts.indirect_clamp.value / tracer.scene.exposure;
	tracer.scene.indirect_depth = opts.bounces.value;
	tracer.scene.bvh_heatmap = opts.bvh_heatmap.value;
	tracer.width = width;
	tracer.height = height;
	tracer.pixels = framebuffer.pixels.data();
	tracer.num_samples = (size_t)opts.samples.value;
	tracer.seed = (uint64_t)frame_offset;

	tracer.scene.scene4 = create_scene4(*tracer.scene.root);

	init_sky_grid(tracer.scene);

	size_t num_threads = std::thread::hardware_concurrency() - 1;
	if (opts.threads.value > 0) num_threads = (size_t)opts.threads.value - 1;
	std::unique_ptr<std::thread[]> threads = std::make_unique<std::thread[]>(num_threads);

	verbosef("Using %zu threads\n", num_threads + 1);

	auto time_begin = std::chrono::high_resolution_clock::now();

	for (size_t i = 0; i < num_threads; i++) {
		threads[i] = std::thread { trace_image, std::ref(tracer), false };
	}

	if (opts.gui.value) {
		enable_gui(&framebuffer);
	}

	trace_image(tracer, true);

	auto time_end = std::chrono::high_resolution_clock::now();
	printf("Done in %.2fs\n", (double)std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_begin).count() * 1e-9);

	for (size_t i = 0; i < num_threads; i++) {
		threads[i].join();
	}

	std::string output_path = get_path(opts, opts.output);

	std::vector<char> name;
	for (const char *c = output_path.c_str(); *c;) {
		if (*c != '#') {
			name.push_back(*c++);
		} else {
			int width = 0;
			while (*c == '#') {
				width++;
				c++;
			}

			if (width > 0) {
				char tmp[64];
				int len = snprintf(tmp, sizeof(tmp), "%0*d", width, frame_offset);
				name.insert(name.end(), tmp, tmp + len);
			}
		}
	}
	name.push_back('\0');

	std::vector<uint8_t> png = write_png(framebuffer.pixels.data(), framebuffer.width, framebuffer.height);

	Image image = read_png(png.data(), png.size());

	bool write_fail = false;
	FILE *f = fopen(name.data(), "wb");
	if (f) {
		if (fwrite(png.data(), 1, png.size(), f) != png.size()) write_fail = true;
		if (fclose(f) != 0) write_fail = true;
	} else {
		write_fail = true;
	}

	if (write_fail) {
		fprintf(stderr, "Failed to save result file: %s\n", name.data());
		exit(1);
	}

	if (opts.gui.value) {
		disable_gui(std::move(framebuffer));
	}

	if (scene != original_scene) {
		ufbx_free_scene(scene);
	}
}

void render_file(const Opts &opts)
{
	verbosef("Loading scene: %s\n", opts.input.value.c_str());

	ProgressState progress_state = { };

	ufbx_error error;

	ufbx_load_opts load_opts = { };

	load_opts.evaluate_skinning = true;

	load_opts.progress_cb.fn = &progress;
	load_opts.progress_cb.user = &progress_state;

	ufbx_real scale = (ufbx_real)opts.scene_scale.value;

	load_opts.use_root_transform = true;
	load_opts.root_transform.rotation = ufbx_identity_quat;
	load_opts.root_transform.scale = ufbx_vec3{ scale, scale, scale };

	std::string path = get_path(opts, opts.input);
	ufbx_scene *scene = ufbx_load_file(path.c_str(), &load_opts, &error);

	if (!scene) {
		char buf[4096];
		ufbx_format_error(buf, sizeof(buf), &error);
		fprintf(stderr, "%s\n", buf);
		exit(1);
	}

	for (int i = opts.frame_offset.value; i < opts.num_frames.value; i++) {
		render_frame(scene, opts, i);
	}

	ufbx_free_scene(scene);

	if (opts.gui.value) {
		if (!opts.keep_open.value) close_gui();
		wait_gui();
	}
}

void parse_args(Opts &opts, int argc, char **argv, bool ignore_input)
{
	for (int argi = 1; argi < argc; ) {
		const char *arg = argv[argi++];
		if (arg[0] == '-') {
			arg++;
			if (arg[0] == '-') arg++;
			for (OptBase &opt : opts) {
				if (!ignore_input && !strcmp(opt.name, "input")) continue;
				if (!strcmp(opt.name, arg) || (opt.alias && !strcmp(opt.alias, arg))) {
					if ((uint32_t)(argc - argi) >= opt.num_args) {
						opt.parse((const char**)argv + argi);
						opt.defined = true;
						opt.from_arg = true;
						argi += opt.num_args;
						break;
					}
				}
			}
		} else {
			if (!ignore_input) {
				opts.input.value = arg;
				opts.input.defined = true;
				opts.input.from_arg = true;
			}
		}
	}
}

size_t parse_line(const char **tokens, char *line, size_t max_tokens)
{
	size_t num_tokens = 0;
	char *src = line, *dst = line;
	const char *token = dst;
	while (num_tokens < max_tokens) {
		while (*src == ' ' || *src == '\r' || *src == '\t' || *src == '\n') {
			src++;
		}
		if (*src == '\0' || *src == '#') break;
		if (*src == '"') {
			src++;
			while (*src != '\0' && *src != '"') {
				if (*src == '\\') src++;
				*dst++ = *src++;
			}
		} else {
			while (*src != '\0' && *src != ' ' && *src != '\t' && *src != '\r' && *src != '\n') {
				*dst++ = *src++;
			}
		}
		if (*src != '\0') src++;
		*dst++ = '\0';
		tokens[num_tokens++] = token;
		token = dst;
	}
	return num_tokens;
}

void parse_file(Opts &opts, const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "Failed to open: %s\n", filename);
		exit(1);
	}

	{
		size_t len = strlen(filename);
		while (len > 0 && filename[len - 1] != '\\' && filename[len - 1] != '/') {
			len--;
		}
		opts.base_path.value = std::string{ filename, len };
		opts.base_path.defined = true;
	}

	char line[1024];
	const char *tokens[16];
	while (fgets(line, sizeof(line), f)) {
		size_t num_tokens = parse_line(tokens, line, 16);
		if (num_tokens < 1) continue;
		for (OptBase &opt : opts) {
			if (!strcmp(opt.name, tokens[0]) && num_tokens - 1 >= opt.num_args) {
				opt.parse(tokens + 1);
				opt.defined = true;
				break;
			}
		}
	}

	fclose(f);
}

static void compare_images(Opts &opts, const char *path_a, const char *path_b)
{
	const char *paths[] = { path_a, path_b };
	Image img[2];

	for (uint32_t i = 0; i < 2; i++) {
		const char *path = paths[i];
		img[i] = load_png(path);
		if (img[i].error) {
			fprintf(stderr, "Failed to load %s: %s\n", path, img[i].error);
			exit(1);
		}
	}

	if (img[0].width != img[1].width || img[0].height != img[1].height) {
		fprintf(stderr, "Resolution mismatch: %ux%u vs %ux%u\n",
			img[0].width, img[0].height, img[1].width, img[1].height);
		exit(1);
	}

	double error = 0.0;
	for (uint32_t y = 0; y < img[0].height; y++) {
		for (uint32_t x = 0; x < img[0].width; x++) {
			Pixel16 a = img[0].pixels[y * img[0].width + x];
			Pixel16 b = img[1].pixels[y * img[1].width + x];
			for (uint32_t c = 0; c < 4; c++) {
				uint16_t ca = (&a.r)[c], cb = (&b.r)[c];
				double va = (double)ca * (1.0/65535.0), vb = (double)cb * (1.0/65535.0);
				error += (va - vb) * (va - vb);
			}
		}
	}
	error /= (double)(img[0].width * img[0].height);

	printf("Difference (MSE): %.4f\n", error);
	if (error > opts.error_threshold.value) {
		printf("ERROR: Over threshold of %.4f\n", opts.error_threshold.value);
		exit(1);
	}
}

int main(int argc, char **argv)
{
	Opts opts;
	parse_args(opts, argc, argv, false);

	if (opts.help.value) {
		fprintf(stderr, "Usage: picort input.fbx <opts> (--help)\n");
		for (OptBase &opt : opts) {
			char name[64];
			if (opt.alias) {
				snprintf(name, sizeof(name), "--%s (-%s)", opt.name, opt.alias);
			} else {
				snprintf(name, sizeof(name), "--%s", opt.name);
			}
			fprintf(stderr, "  %-20s %s\n", name, opt.desc);
		}
		return 0;
	}

	if (opts.verbose.value) {
		g_verbose = true;
	}

	if (opts.compare.defined) {
		compare_images(opts, opts.compare.value.v[0].c_str(), opts.compare.value.v[1].c_str());
		return 0;
	}

	if (!opts.input.defined) {
		fprintf(stderr, "Usage: picort input.fbx/.picort.txt <opts> (--help)\n");
		return 0;
	}

	if (ends_with(opts.input.value, ".txt")) {
		std::string path = std::move(opts.input.value);
		opts = Opts{};
		parse_file(opts, path.c_str());
		parse_args(opts, argc, argv, true);
	}

	if (opts.verbose.defined) {
		char buf[512];
		for (OptBase &opt : opts) {
			if (opt.defined) {
				opt.print(buf, sizeof(buf));
				if (opt.from_arg) {
					printf("%s: %s (--)\n", opt.name, buf);
				} else {
					printf("%s: %s\n", opt.name, buf);
				}
			}
		}
	}

	if (opts.num_frames.defined) {
		if (!opts.time.defined) {
			opts.frame.defined = true;
		}
	}

	render_file(opts);

	if (opts.reference.defined) {
		std::string output = get_path(opts, opts.output);
		std::string reference = get_path(opts, opts.reference);
		compare_images(opts, output.c_str(), reference.c_str());
	}

	return 0;
}
