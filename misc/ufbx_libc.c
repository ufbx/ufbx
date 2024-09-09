#include "ufbx_libc.h"

#if defined(UFBXC_HAS_EXIT)
	void ufbxc_os_exit(int code);
#endif

static void ufbxc_assert_fail(const char *message)
{
	(void)message;

#if defined(UFBXC_HAS_STDERR)
	ufbxc_fprintf(stderr, "ufbxc_assert() fail: %s\n", message);
#endif

#if defined(UFBXC_HAS_EXIT)
	ufbxc_os_exit(1);
#endif
}

#define ufbxc_assert(cond) do { if (!(cond)) ufbxc_assert_fail(#cond); } while (0)

// -- bigint

#define BIGINT_LIMB_BITS 32

#if BIGINT_LIMB_BITS == 8
	typedef uint8_t bigint_limb;
	typedef unsigned bigint_accum;
#elif BIGINT_LIMB_BITS == 16
	typedef uint16_t bigint_limb;
	typedef unsigned bigint_accum;
#elif BIGINT_LIMB_BITS == 32
	typedef uint32_t bigint_limb;
	typedef uint64_t bigint_accum;
#else
	#error "Bad bigint limb bits"
#endif

#define BIGINT_LIMB_MAX (bigint_limb)(((bigint_accum)1 << BIGINT_LIMB_BITS) - 1)
#define bigint_limbs_for_bits(bits) (((bits) + (BIGINT_LIMB_BITS - 1)) / BIGINT_LIMB_BITS)

typedef struct {
	bigint_limb *limbs;
	uint32_t capacity;
	uint32_t length;
	bool overflow;
} bigint;

static bigint bigint_make(bigint_limb *limbs, size_t capacity)
{
	bigint bi = { limbs, (uint32_t)capacity };
	return bi;
}

#define bigint_array(arr) bigint_make((arr), sizeof(arr) / sizeof(*(arr)))

static void bigint_mad(bigint *b, bigint_limb multiplicand, bigint_limb addend)
{
	if (b->overflow) return;
	bigint_limb carry = addend;
	for (uint32_t i = 0; i < b->length; i++) {
		bigint_accum r = (bigint_accum)b->limbs[i] * multiplicand + carry;
		b->limbs[i] = (bigint_limb)r;
		carry = (bigint_limb)(r >> BIGINT_LIMB_BITS);
	}
	if (carry) {
		b->limbs[b->length++] = carry;
		if (b->length == b->capacity) {
			b->overflow = true;
		}
	}
}

static void bigint_mul(bigint *b, bigint_limb multiplicand)
{
	bigint_mad(b, multiplicand, 0);
}

static void bigint_shift_limbs(bigint *b, uint32_t count)
{
	if (b->overflow || b->length + count > b->capacity) {
		b->overflow = true;
		return;
	}
	for (uint32_t i = b->length; i-- > 0; ) {
		b->limbs[i + count] = b->limbs[i];
	}
	for (uint32_t i = 0; i < count; i++) {
		b->limbs[i] = 0;
	}
	b->length += count;
}

static bigint_limb bigint_div(bigint *b, bigint_limb divisor)
{
	uint32_t new_length = 0;
	bigint_accum accum = 0;
	for (uint32_t i = b->length; i-- > 0; ) {
		accum = (accum << BIGINT_LIMB_BITS) | b->limbs[i];
		if (accum >= divisor) {
			bigint_accum quot = accum / divisor;
			bigint_accum rem = accum % divisor;

			b->limbs[i] = (bigint_limb)quot;
			if (quot > 0 && !new_length) {
				new_length = i + 1;
			}
			accum = rem;
		} else {
			b->limbs[i] = 0;
		}
	}
	b->length = new_length;
	return (bigint_limb)accum;
}

// b * divisor^power
static void bigint_mul_pow(bigint *b, bigint_limb multiplicand, uint32_t power)
{
	const bigint_limb max_mul = BIGINT_LIMB_MAX / multiplicand;
	bigint_limb mul = 1;

	for (uint32_t i = 0; i < power; i++) {
		mul *= multiplicand;
		if (mul >= max_mul) {
			bigint_mul(b, mul);
			if (b->overflow) return;
			mul = 1;
		}
	}
	if (mul > 1) {
		bigint_mul(b, mul);
	}
}

// b / divisor^power -> any remainder
static bool bigint_div_pow(bigint *b, bigint_limb divisor, uint32_t power)
{
	const bigint_limb max_div = BIGINT_LIMB_MAX / divisor;
	bool any_rem = false;
	bigint_limb div = 1;

	for (uint32_t i = 0; i < power; i++) {
		div *= divisor;
		if (div >= max_div) {
			any_rem |= bigint_div(b, div) != 0;
			div = 1;
		}
	}
	if (div > 1) {
		any_rem |= bigint_div(b, div) != 0;
	}

	return any_rem;
}

static bigint_limb bigint_top_limb(const bigint b, uint32_t index) {
	return index < b.length ? b.limbs[b.length - 1 - index] : 0;
}

static uint64_t bigint_extract_high(const bigint b, int32_t *p_exponent, bool *p_tail)
{
	if (b.length == 0) {
		*p_exponent = 0;
		return 0;
	}

	const uint32_t limb_count = 64 / BIGINT_LIMB_BITS;

	uint64_t result = 0;
	for (uint32_t i = 0; i < limb_count; i++) {
		result = (result << BIGINT_LIMB_BITS) | bigint_top_limb(b, i);
	}

	bool tail = false;
	uint32_t shift = 0;
	while ((result >> 63) == 0) {
		result <<= 1;
		shift += 1;
	}

	bigint_limb lo = bigint_top_limb(b, limb_count);
	if (shift > 0) {
		result |= lo >> (BIGINT_LIMB_BITS - shift);
	}
	tail |= (bigint_limb)(lo << shift) != 0;
	for (uint32_t i = limb_count + 1; i < b.length; i++) {
		tail |= bigint_top_limb(b, i) != 0;
	}

	*p_exponent = b.length * BIGINT_LIMB_BITS - shift;
	if (tail) *p_tail = true;
	return result;
}

// -- bigfloat

typedef struct {
	bigint mantissa;
	char **p_end;
	uint64_t mantissa_bits;
	int32_t exponent;
	bool tail;
	bool negative;
} bigfloat;

static void bigfloat_parse(bigfloat *bf, const char *str)
{
	int32_t dec_exponent = 0;
	const char *p = str;

	const uint32_t max_limbs = bf->mantissa.capacity - (bigint_limbs_for_bits(64) + 1) - 1;

	if (*p == '+' || *p == '-') {
		bf->negative = *p == '-';
		p++;
	}

	for (;;) {
		char c = *p;
		if (c >= '0' && c <= '9') {
			p++;
			if (bf->mantissa.length < max_limbs) {
				bigint_mad(&bf->mantissa, 10, c - '0');
			} else {
				dec_exponent++;
			}
		} else {
			break;
		}
	}

	if (*p == '.') {
		p++;
		for (;;) {
			char c = *p;
			if (c >= '0' && c <= '9') {
				p++;
				if (bf->mantissa.length < max_limbs) {
					dec_exponent--;
					bigint_mad(&bf->mantissa, 10, c - '0');
				}
			} else {
				break;
			}
		}
	}

	if (*p == 'e' || *p == 'E') {
		p++;
		bool exp_negative = false;
		if (*p == '+' || *p == '-') {
			exp_negative = *p == '-';
			p++;
		}
		int32_t exp = 0;
		for (;;) {
			char c = *p;
			if (c >= '0' && c <= '9') {
				p++;
				exp = exp * 10 + (c - '0');
				if (exp >= INT32_MAX / 2 / 10) break;
			} else {
				break;
			}
		}
		dec_exponent += exp_negative ? -exp : exp;
	}

	if (bf->p_end) {
		*bf->p_end = (char*)p;
	}

	if (dec_exponent < 0) {
		uint32_t limb_shift = bigint_limbs_for_bits(4 * -dec_exponent + 64) + 1;
		if (limb_shift > bf->mantissa.capacity) {
			limb_shift = bf->mantissa.capacity;
		}

		if (limb_shift > bf->mantissa.length) {
			uint32_t shift = limb_shift - bf->mantissa.length;
			bigint_shift_limbs(&bf->mantissa, shift);
			bf->exponent = -(int32_t)shift * BIGINT_LIMB_BITS;
		}

		bf->tail = bigint_div_pow(&bf->mantissa, 10, (uint32_t)-dec_exponent);
	} else if (dec_exponent > 0) {
		bigint_mul_pow(&bf->mantissa, 10, dec_exponent);
	}

	int32_t mantissa_exp = 0;
	bf->mantissa_bits = bigint_extract_high(bf->mantissa, &mantissa_exp, &bf->tail);
	bf->exponent += mantissa_exp - 1;
}

static uint64_t bigfloat_shift_right_round(uint64_t value, uint32_t shift, bool tail)
{
	if (shift == 0) return value;
	if (shift > 64) return 0;
	uint64_t result = value >> (shift - 1);
	uint64_t tail_mask = (UINT64_C(1) << (shift - 1)) - 1;

	bool r_odd = (result & 0x2) != 0;
	bool r_round = (result & 0x1) != 0;
	bool r_tail = tail || (value & tail_mask) != 0;
	uint64_t round_bit = (r_round && (r_odd || r_tail)) ? 1u : 0u;

	return (result >> 1u) + round_bit;
}

static uint64_t bigfloat_assemble(bigfloat *bf, uint32_t mantissa_bits, int32_t min_exp, int32_t max_exp, uint32_t sign_shift)
{
	uint64_t mantissa = bf->mantissa_bits;
	int32_t exponent = bf->exponent;
	uint64_t sign_bit = (uint64_t)(bf->negative ? 1u : 0u) << sign_shift;

	uint32_t mantissa_shift = 64 - mantissa_bits;
	if (exponent > max_exp) {
		return sign_bit | (uint64_t)(max_exp - min_exp + 1) << (mantissa_bits - 1);
	} else if (exponent <= min_exp) {
		mantissa_shift += (uint32_t)(min_exp + 1 - exponent);
		exponent = min_exp + 1;
	}

	mantissa = bigfloat_shift_right_round(mantissa, mantissa_shift, bf->tail);
	if (mantissa == 0) return 0;

	uint64_t bits = mantissa;
	bits += (uint64_t)(exponent - min_exp - 1) << (mantissa_bits - 1);
	return sign_bit | bits;
}

static float bigfloat_parse_float(const char *str, char **p_end)
{
	bigint_limb limbs[bigint_limbs_for_bits(3000)];
	bigfloat bf = { bigint_array(limbs), p_end };
	bigfloat_parse(&bf, str);

	uint32_t bits = (uint32_t)bigfloat_assemble(&bf, 24, -127, 127, 31);

	float d;
	memcpy(&d, &bits, sizeof(float));
	return d;
}

static double bigfloat_parse_double(const char *str, char **p_end)
{
	bigint_limb limbs[bigint_limbs_for_bits(3000)];
	bigfloat bf = { bigint_array(limbs), p_end };
	bigfloat_parse(&bf, str);

	uint64_t bits = bigfloat_assemble(&bf, 53, -1023, 1023, 63);

	double d;
	memcpy(&d, &bits, sizeof(double));
	return d;
}

// -- ufmalloc

#if defined(UFBXC_HAS_MALLOC)

// Small allocator interface, NOT thread safe!

void *ufbxc_os_allocate(size_t size, size_t *p_allocated_size);
bool ufbxc_os_free(void *pointer, size_t allocated_size);

#define UFMALLOC_FREE 0x1
#define UFMALLOC_USED 0x2
#define UFMALLOC_BLOCK 0x4

typedef struct ufmalloc_node {
	struct ufmalloc_node *prev, *next;
	size_t flags;
	size_t size;
} ufmalloc_node;

static ufmalloc_node ufmalloc_root;

static void ufmalloc_link(ufmalloc_node *prev, ufmalloc_node *next, size_t size, size_t flags)
{
	if (prev->next) prev->next->prev = next;

	next->next = prev->next;
	next->prev = prev;
	next->size = size;
	next->flags = flags;

	prev->next = next;
}

static void ufmalloc_unlink(ufmalloc_node *node)
{
	ufmalloc_node *prev = node->prev, *next = node->next;
	if (next) next->prev = prev;
	prev->next = next;
	node->prev = node->next = NULL;
}

static bool ufmalloc_block_end(ufmalloc_node *node)
{
	if (!node) return true;
	if (!node->flags || node->flags == UFMALLOC_BLOCK) return true;
	return false;
}

static int serial = 0;
static int valid_serial = 0;

static void ufmalloc_validate()
{
	valid_serial++;

	ufmalloc_node *node = ufmalloc_root.next;

	if (node) {
		ufbxc_assert(node->flags == UFMALLOC_BLOCK);
	}

	while (node) {
		ufbxc_assert(node->flags <= 4);

		if (node->flags == UFMALLOC_FREE || node->flags == UFMALLOC_USED) {
			ufmalloc_node *next = node->next;
			if (next && (next->flags == UFMALLOC_FREE || next->flags == UFMALLOC_USED)) {
				size_t expected_size = (size_t)((char*)next - (char*)(node + 1));
				ufbxc_assert(node->size == expected_size);
			}
		}

		node = node->next;
	}
}

static void *ufmalloc_alloc(size_t size)
{
	if (size == 0) {
		size = 1;
	}

	size_t align = 2 * sizeof(void*);
	size = (size + align - 1) & ~(align - 1);

	serial++;

	ufmalloc_validate();

	ufmalloc_node *node = ufmalloc_root.next;
	while (node) {
		if (node->flags & UFMALLOC_FREE) {
			if (size <= node->size) {
				char *data = (char*)(node + 1);

				size_t slack = node->size - size;
				if (slack >= 2 * sizeof(ufmalloc_node)) {
					ufmalloc_node *next = (ufmalloc_node*)(data + size);
					ufmalloc_link(node, next, slack - sizeof(ufmalloc_node), UFMALLOC_FREE);
					ufbxc_assert(size + next->size + sizeof(ufmalloc_node) == node->size);
					node->size = size;
				}

				node->flags = UFMALLOC_USED;
				ufmalloc_validate();

				return data;
			}
		}

		node = node->next;
	}

	size_t allocated_size = 0;
	void *memory = ufbxc_os_allocate(2 * sizeof(ufmalloc_node) + size, &allocated_size);
	if (!memory) return NULL;

	// Can't do anything if the memory is too small
	if (allocated_size <= 2 * sizeof(ufmalloc_node)) {
		ufbxc_os_free(memory, allocated_size);
		return NULL;
	}

	ufmalloc_node *header = (ufmalloc_node*)memory;
	ufmalloc_node *block = header + 1;

	size_t free_space = allocated_size - sizeof(ufmalloc_node) * 2;
	ufmalloc_link(&ufmalloc_root, header, allocated_size, UFMALLOC_BLOCK);
	ufmalloc_link(header, block, free_space, UFMALLOC_FREE);

	ufmalloc_validate();

	if (free_space >= size) {
		return ufmalloc_alloc(size);
	} else {
		return NULL;
	}
}

static void ufmalloc_free(void *ptr)
{
	ufmalloc_node *node = (ufmalloc_node*)ptr - 1;
	ufbxc_assert(node->flags == UFMALLOC_USED);
	node->flags = UFMALLOC_FREE;

	serial++;

	while (node->prev->flags == UFMALLOC_FREE) {
		node = node->prev;
		node->size += node->next->size + sizeof(ufmalloc_node);
		ufmalloc_unlink(node->next);
	}
	while (node->next && node->next->flags == UFMALLOC_FREE) {
		node->size += node->next->size + sizeof(ufmalloc_node);
		ufmalloc_unlink(node->next);
	}

	if (node->prev->flags == UFMALLOC_BLOCK && ufmalloc_block_end(node->next)) {
		ufmalloc_node *header = node->prev;
		ufmalloc_unlink(header);
		ufmalloc_unlink(node);
		if (!ufbxc_os_free(header, header->size)) {
			ufmalloc_link(&ufmalloc_root, header, header->size, header->flags);
			ufmalloc_link(header, node, node->size, node->flags);
		}
	}

	ufmalloc_validate();
}

static size_t ufmalloc_size(void *ptr)
{
	ufmalloc_node *node = (ufmalloc_node*)ptr - 1;
	ufbxc_assert(node->flags == UFMALLOC_USED);
	return node->size;
}

#endif

// -- utility

static void ufbxci_swap(void *a, void *b, size_t size)
{
	char *ca = (char*)a, *cb = (char*)b;
	for (size_t i = 0; i < size; i++) {
		char t = ca[i];
		ca[i] = cb[i];
		cb[i] = t;
	}
}

static size_t ufbxci_clamp_pos(size_t size, long offset)
{
	if (offset < 0) {
		return 0;
	}
	if (offset >= (long)size || (size_t)offset >= size) {
		return size;
	}
	return (size_t) offset;
}

// -- string.h

size_t ufbxc_strlen(const char *str)
{
	size_t length = 0;
	while (str[length]) {
		length++;
	}
	return length;
}

void *ufbxc_memcpy(void *dst, const void *src, size_t count)
{
	char *d = (char*)dst;
	const char *s = (const char*)src;
	for (size_t i = 0; i < count; i++) {
		d[i] = s[i];
	}
	return dst;
}

void *ufbxc_memmove(void *dst, const void *src, size_t count)
{
	char *d = (char*)dst;
	const char *s = (const char*)src;
	if ((uintptr_t)d < (uintptr_t)s) {
		for (size_t i = 0; i < count; i++) {
			d[i] = s[i];
		}
	} else {
		for (size_t i = count; i-- > 0; ) {
			d[i] = s[i];
		}
	}
	return dst;
}

void *ufbxc_memset(void *dst, int ch, size_t count)
{
	char *d = (char*)dst;
	char c = (char)ch;
	for (size_t i = 0; i < count; i++) {
		d[i] = c;
	}
	return dst;
}

const void *ufbxc_memchr(const void *ptr, int value, size_t num)
{
	const char *p = (const char*)ptr;
	char c = (char)value;
	for (size_t i = 0; i < num; i++) {
		if (p[i] == c) {
			return p + i;
		}
	}
	return NULL;
}

int ufbxc_memcmp(const void *a, const void *b, size_t count)
{
	const char *pa = (const char*)a;
	const char *pb = (const char*)b;
	for (size_t i = 0; i < count; i++) {
		if (pa[i] != pb[i]) {
			return (unsigned char)pa[i] < (unsigned char)pb[i] ? -1 : 1;
		}
	}
	return 0;
}

int ufbxc_strcmp(const char *a, const char *b)
{
	const char *pa = (const char*)a;
	const char *pb = (const char*)b;
	for (size_t i = 0; ; i++) {
		if (pa[i] != pb[i]) {
			return (unsigned char)pa[i] < (unsigned char)pb[i] ? -1 : 1;
		} else if (pa[i] == 0) {
			return 0;
		}
	}
}

int ufbxc_strncmp(const char *a, const char *b, size_t count)
{
	const char *pa = (const char*)a;
	const char *pb = (const char*)b;
	for (size_t i = 0; i < count; i++) {
		if (pa[i] != pb[i]) {
			return (unsigned char)pa[i] < (unsigned char)pb[i] ? -1 : 1;
		} else if (pa[i] == 0) {
			return 0;
		}
	}
	return 0;
}

// -- stdlib.h

float ufbxc_strtof(const char *str, char **end)
{
	return bigfloat_parse_float(str, end);
}

double ufbxc_strtod(const char *str, char **end)
{
	return bigfloat_parse_double(str, end);
}

unsigned long ufbxc_strtoul(const char *str, char **end, int base)
{
	unsigned long value = 0;
	ufbxc_assert(base == 10 || base == 16);

	while (*str) {
		char c = *str;
		unsigned long digit = 0;

		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (base == 16 && c >= 'A' && c <= 'F') {
			digit = 10 + (c - 'A');
		} else if (base == 16 && c >= 'a' && c <= 'f') {
			digit = 10 + (c - 'a');
		} else {
			break;
		}

		value = value * (unsigned long)base + digit;
		str++;
	}

	if (end) {
		*end = (char*)str;
	}

	return value;
}

void ufbxc_qsort(void *ptr, size_t count, size_t size, int (*cmp_fn)(const void*, const void*))
{
	if (count <= 1) return;
	char *data = (char*)ptr;
	size_t start = (count - 1) >> 1;
	size_t end = count - 1;
	for (;;) {
		size_t root = start;
		size_t child;
		while ((child = root*2 + 1) <= end) {
			size_t next = cmp_fn(data + child * size, data + root * size) < 0 ? root : child;
			if (child + 1 <= end && cmp_fn(data + next * size, data + (child + 1) * size) < 0) {
				next = child + 1;
			}
			if (next == root) break;
			ufbxci_swap(data + root * size, data + next * size, size);
			root = next;
		}

		if (start > 0) {
			start--;
		} else if (end > 0) {
			ufbxci_swap(data + end * size, data, size);
			end--;
		} else {
			break;
		}
	}
}

#if defined(UFBXC_HAS_MALLOC)

void *ufbxc_malloc(size_t size)
{
	return ufmalloc_alloc(size);
}

void *ufbxc_realloc(void *ptr, size_t new_size)
{
	void *new_ptr = ufbxc_malloc(new_size);
	if (!new_ptr) return NULL;

	size_t size = ufmalloc_size(ptr);
	ufbxc_memcpy(new_ptr, ptr, size < new_size ? size : new_size);
	ufmalloc_free(ptr);

	return new_ptr;
}

void ufbxc_free(void *ptr)
{
	ufmalloc_free(ptr);
}

#endif

// -- stdio.h

int ufbxc_vsnprintf(char *buffer, size_t count, const char *format, va_list args)
{
	if (count == 0) return 0;

	char *dst = buffer, *cap = buffer + count - 1;
	for (const char *p = format; *p; ) {
		if (*p == '%') {
			size_t max_len = SIZE_MAX;
			if (!ufbxc_strncmp(p, "%s", 2)) {
				p += 2;
				const char *str = va_arg(args, const char*);
				for (const char *s = str; *s; s++) {
					*dst = *s;
					if (dst < cap) dst++;
				}
			} else if (!ufbxc_strncmp(p, "%.*s", 4)) {
				p += 4;
				int len = va_arg(args, int);
				const char *str = va_arg(args, const char*);
				for (const char *s = str; *s; s++) {
					if (len-- <= 0) break;
					*dst = *s;
					if (dst < cap) dst++;
				}
			} else if (!ufbxc_strncmp(p, "%u", 2)) {
				p += 2;
				unsigned value = va_arg(args, unsigned);
				char buffer[sizeof(unsigned) * 8 / 2];
				size_t len = 0;
				do {
					buffer[len] = (char)('0' + value % 10);
					value /= 10;
					len++;
				} while (value != 0);
				for (size_t i = 0; i < len; i++) {
					*dst = buffer[len - i - 1];
					if (dst < cap) dst++;
				}
			} else if (!ufbxc_strncmp(p, "%6u", 3)) {
				p += 3;
				unsigned value = va_arg(args, unsigned);
				char buffer[sizeof(unsigned) * 8 / 2];
				size_t len = 0;
				do {
					buffer[len] = (char)('0' + value % 10);
					value /= 10;
					len++;
				} while (value != 0);
				for (size_t i = len; i < 6; i++ ){
					*dst = ' ';
					if (dst < cap) dst++;
				}
				for (size_t i = 0; i < len; i++) {
					*dst = buffer[len - i - 1];
					if (dst < cap) dst++;
				}
			} else if (!ufbxc_strncmp(p, "%zu", 2)) {
				p += 3;
				size_t value = va_arg(args, size_t);
				char buffer[sizeof(size_t) * 8 / 2];
				size_t len = 0;
				do {
					buffer[len] = (char)('0' + value % 10);
					value /= 10;
					len++;
				} while (value != 0);
				for (size_t i = 0; i < len; i++) {
					*dst = buffer[len - i - 1];
					if (dst < cap) dst++;
				}
			} else {
				ufbxc_assert(0 && "Bad format specifier");
				return -1;
			}
		} else {
			*dst = *p++;
			if (dst < cap) dst++;
		}
	}

	*dst = '\0';
	return (int)(dst - buffer);
}

#if defined(UFBXC_HAS_STDIO)

void *ufbxc_os_read_file(size_t index, const char *filename, size_t *p_size);
void ufbxc_os_free_file(size_t index, void *data);

struct ufbxc_file {
	size_t used;
	size_t position;
	size_t size;
	void *data;
};

#define UFBXC_MAX_FILES 128
static ufbxc_file ufbxc_files[UFBXC_MAX_FILES];

FILE *ufbxc_fopen(const char *filename, const char *mode)
{
	for (uint32_t i = 0; i < UFBXC_MAX_FILES; i++) {
		ufbxc_file *file = &ufbxc_files[i];
		if (!file->used) {
			size_t size = 0;
			void *data = ufbxc_os_read_file(i, filename, &size);
			if (!data) return NULL;

			file->position = 0;
			file->data = data;
			file->size = size;
			file->used = 1;
			return file;
		}
	}

	return NULL;
}

size_t ufbxc_fread(void *buffer, size_t size, size_t count, FILE *f)
{
	ufbxc_assert(f && f->used);

	size_t to_read = size * count;
	size_t left = f->size - f->position;
	if (to_read > left) {
		to_read = left;
	}

	ufbxc_memcpy(buffer, (const char*)f->data + f->position, to_read);
	f->position += to_read;
	return to_read / size;
}

void ufbxc_fclose(FILE *f)
{
	if (!f) return;
	size_t index = f - ufbxc_files;
	ufbxc_assert(index < UFBXC_MAX_FILES);
	ufbxc_assert(f->used);
	ufbxc_os_free_file(index, f->data);
	ufbxc_memset(f, 0, sizeof(ufbxc_file));
}

long ufbxc_ftell(FILE *f)
{
	ufbxc_assert(f && f->used);
	return (long)f->position;
}

int ufbxc_ferror(FILE *f)
{
	ufbxc_assert(f && f->used);
	return 0;
}

int ufbxc_fseek(FILE *f, long offset, int origin)
{
	ufbxc_assert(f && f->used);
	if (origin == SEEK_END) {
		f->position = ufbxci_clamp_pos(f->size, (long)f->size + offset);
	} else if (origin == SEEK_CUR) {
		f->position = ufbxci_clamp_pos(f->size, (long)f->position + offset);
	} else {
		ufbxc_assert(0 && "Bad seek mode");
		return 1;
	}
	return 0;
}

int ufbxc_fgetpos(FILE *f, fpos_t *pos)
{
	ufbxc_assert(f && f->used);
	*pos = (int64_t)f->position;
	return 0;
}

int ufbxc_fsetpos(FILE *f, const fpos_t *pos)
{
	ufbxc_assert(f && f->used);
	f->position = (size_t)*pos;
	return 0;
}

void ufbxc_rewind(FILE *f)
{
	ufbxc_assert(f && f->used);
	f->position = 0;
}

#endif

#if defined(UFBXC_HAS_STDERR)

void ufbxc_os_print_error(const char *message, size_t length);

void ufbxc_fprintf(FILE *file, const char *fmt, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, fmt);
	int length = ufbxc_vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (length > 0) {
		ufbxc_os_print_error(buffer, (size_t)length);
	}
}

#endif
