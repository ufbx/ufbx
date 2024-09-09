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

void *ufbxc_os_allocate(size_t size, size_t *p_allocated_size);
bool ufbxc_os_free(void *pointer, size_t allocated_size);

#define UFMALLOC_FREE 0x1
#define UFMALLOC_USED 0x2
#define UFMALLOC_BLOCK 0x4

typedef struct ufmalloc_node {
	struct ufmalloc_node *prev_all, *next_all;
	struct ufmalloc_node **prev_next_free, *next_free;
	size_t flags;
	size_t size;
} ufmalloc_node;

#define UFMALLOC_NUM_SIZE_CLASSES 32

static ufmalloc_node ufmalloc_root;
static ufmalloc_node *ufmalloc_free_list[UFMALLOC_NUM_SIZE_CLASSES];

static size_t ufmalloc_size_class(size_t size)
{
	size_t sc = 0;
	while (size > 16 && sc + 1 < UFMALLOC_NUM_SIZE_CLASSES) {
		sc++;
		size /= 2;
	}
	return sc;
}

static void ufmalloc_link(ufmalloc_node *prev, ufmalloc_node *next)
{
	if (prev->next_all) prev->next_all->prev_all = next;
	next->next_all = prev->next_all;
	next->prev_all = prev;
	prev->next_all = next;
}

static void ufmalloc_unlink(ufmalloc_node *node)
{
	ufmalloc_node *prev = node->prev_all, *next = node->next_all;
	if (next) next->prev_all = prev;
	prev->next_all = next;
	node->prev_all = node->next_all = NULL;
}

static void ufmalloc_create(ufmalloc_node *prev, ufmalloc_node *node, size_t size, uint32_t flags)
{
	ufmalloc_link(prev, node);
	node->size = size;
	node->flags = flags;
	node->prev_next_free = NULL;
	node->next_free = NULL;
}

static void ufmalloc_link_free(ufmalloc_node *node)
{
	ufbxc_assert(node->flags == UFMALLOC_FREE);

	size_t sc = ufmalloc_size_class(node->size);
	ufmalloc_node **p_free = &ufmalloc_free_list[sc];
	ufmalloc_node *free_node = *p_free;
	if (free_node) free_node->prev_next_free = &node->next_free;
	node->prev_next_free = p_free;
	node->next_free = *p_free;
	*p_free = node;
}

static void ufmalloc_unlink_free(ufmalloc_node *node)
{
	if (node->next_free) node->next_free->prev_next_free = node->prev_next_free;
	ufbxc_assert(node->prev_next_free);
	*node->prev_next_free = node->next_free;
	node->prev_next_free = NULL;
	node->next_free = NULL;
}

static bool ufmalloc_block_end(ufmalloc_node *node)
{
	if (!node) return true;
	if (!node->flags || node->flags == UFMALLOC_BLOCK) return true;
	return false;
}

static void *ufmalloc_alloc(size_t size)
{
	if (size == 0) {
		size = 1;
	}

	size_t align = 2 * sizeof(void*);
	size = (size + align - 1) & ~(align - 1);

	ufmalloc_node *node = NULL;

	size_t search_attempts = 16;
	size_t sc = ufmalloc_size_class(size);
	for (; !node && sc < UFMALLOC_NUM_SIZE_CLASSES; sc++) {
		ufmalloc_node *free_node = ufmalloc_free_list[sc];
		if (!free_node) continue;

		for (size_t i = 0; i < search_attempts; i++) {
			if (!free_node) break;

			if (free_node->size >= size) {
				node = free_node;
				ufmalloc_unlink_free(node);
				ufbxc_assert(node->flags == UFMALLOC_FREE);
				break;
			}

			free_node = free_node->next_free;
		}
	}

	if (node == NULL) {
		size_t allocated_size = 0;
		void *memory = ufbxc_os_allocate(2 * sizeof(ufmalloc_node) + size + align, &allocated_size);
		if (!memory) return NULL;

		// Can't do anything if the memory is too small
		if (allocated_size <= 2 * sizeof(ufmalloc_node) + align) {
			ufbxc_os_free(memory, allocated_size);
			return NULL;
		}

		size_t align_bytes = (align - (uintptr_t)memory % align) % align;
		char *aligned_memory = (char*)memory + align_bytes;
		size_t aligned_size = allocated_size - align_bytes;
		size_t free_space = aligned_size - sizeof(ufmalloc_node) * 2;

		ufmalloc_node *header = (ufmalloc_node*)aligned_memory;
		ufmalloc_node *block = header + 1;

		ufmalloc_create(&ufmalloc_root, header, allocated_size, UFMALLOC_BLOCK);
		header->next_free = memory;

		ufmalloc_create(header, block, free_space, UFMALLOC_FREE);

		if (free_space >= size) {
			node = block;
		} else {
			return NULL;
		}
	}

	if (node == NULL) return NULL;

	char *data = (char*)(node + 1);

	size_t slack = node->size - size;
	if (slack >= 2 * sizeof(ufmalloc_node)) {
		ufmalloc_node *next = (ufmalloc_node*)(data + size);
		ufmalloc_create(node, next, slack - sizeof(ufmalloc_node), UFMALLOC_FREE);
		ufmalloc_link_free(next);
		ufbxc_assert(size + next->size + sizeof(ufmalloc_node) == node->size);
		node->size = size;
	}

	node->flags = UFMALLOC_USED;
	return data;
}

static void ufmalloc_free(void *ptr)
{
	ufmalloc_node *node = (ufmalloc_node*)ptr - 1;
	ufbxc_assert(node->flags == UFMALLOC_USED);
	node->flags = UFMALLOC_FREE;

	while (node->prev_all->flags == UFMALLOC_FREE) {
		node = node->prev_all;
		node->size += node->next_all->size + sizeof(ufmalloc_node);
		ufmalloc_unlink_free(node);
		ufmalloc_unlink(node->next_all);
	}
	while (node->next_all && node->next_all->flags == UFMALLOC_FREE) {
		node->size += node->next_all->size + sizeof(ufmalloc_node);
		ufmalloc_unlink_free(node->next_all);
		ufmalloc_unlink(node->next_all);
	}

	if (node->prev_all->flags == UFMALLOC_BLOCK && ufmalloc_block_end(node->next_all)) {
		ufmalloc_node *header = node->prev_all;
		ufmalloc_unlink(header);
		ufmalloc_unlink(node);
		if (!ufbxc_os_free(header->next_free, header->size)) {
			ufmalloc_link(&ufmalloc_root, header);
			ufmalloc_link(header, node);
		} else {
			node = NULL;
		}
	}

	if (node) {
		ufmalloc_link_free(node);
	}
}

static size_t ufmalloc_size(void *ptr)
{
	ufmalloc_node *node = (ufmalloc_node*)ptr - 1;
	ufbxc_assert(node->flags == UFMALLOC_USED);
	return node->size;
}

#endif

// -- print

typedef struct {
	char *dst;
	size_t length;
	size_t pos;
} print_buffer;

// Flags
#define PRINT_ALIGN_LEFT 0x1
#define PRINT_PAD_ZERO 0x2
#define PRINT_SIGN_PLUS 0x4
#define PRINT_SIGN_SPACE 0x8
#define PRINT_ALT 0x10
#define PRINT_64BIT 0x20
// Formatting flags
#define PRINT_SIGNED 0x100
#define PRINT_UPPERCASE 0x200
// Type
#define PRINT_INT 0x100
#define PRINT_CHAR 0x200
#define PRINT_STRING 0x400
// Radix
#define PRINT_RADIX_BIN 0x020000
#define PRINT_RADIX_OCT 0x080000
#define PRINT_RADIX_DEC 0x0a0000
#define PRINT_RADIX_HEX 0x100000

#define print_radix(flags) ((flags) >> 16)

static void print_pad(print_buffer *buf, uint32_t flags, size_t count)
{
	char pad_char = (flags & (PRINT_ALIGN_LEFT|PRINT_PAD_ZERO)) == PRINT_PAD_ZERO ? '0' : ' ';
	for (size_t i = 0; i < count; i++) {
		if (buf->dst && buf->pos < buf->length) buf->dst[buf->pos] = pad_char;
		buf->pos++;
	}
}

static void print_append(print_buffer *buf, size_t min_width, size_t max_width, uint32_t flags, const char *str)
{
	size_t width = 0;
	for (width = 0; width < max_width; width++) {
		if (!str[width]) break;
	}

	size_t pad = min_width > width ? min_width - width : 0;
	if (pad > 0 && (flags & PRINT_ALIGN_LEFT) == 0) {
		print_pad(buf, flags, pad);
	}

	for (size_t i = 0; i < width; i++) {
		if (buf->dst && buf->pos < buf->length) buf->dst[buf->pos] = str[i];
		buf->pos++;
	}

	if (pad > 0 && (flags & PRINT_ALIGN_LEFT) != 0) {
		print_pad(buf, flags, pad);
	}
}

static uint32_t print_fmt_flags(const char **p_fmt)
{
	const char *fmt = *p_fmt;
	uint32_t flags = 0;
	for (;;) {
		char c = *fmt;
		switch (c) {
		case '-': flags |= PRINT_ALIGN_LEFT; break;
		case '+': flags |= PRINT_SIGN_PLUS; break;
		case ' ': flags |= PRINT_SIGN_SPACE; break;
		case '0': flags |= PRINT_PAD_ZERO; break;
		case '#': flags |= PRINT_ALT; break;
		default:
			*p_fmt = fmt;
			return flags;
		}
		fmt++;
	}
	*p_fmt = fmt;
}

static size_t print_fmt_count(const char **p_fmt, size_t def, int count_arg)
{
	const char *fmt = *p_fmt;
	int count = -1;
	if (*fmt >= '0' && *fmt <= '9') {
		count = 0;
		while (*fmt >= '0' && *fmt <= '9') {
			count = count * 10 + (*fmt - '0');
			fmt++;
		}
	} else if (*fmt == '*') {
		fmt++;
		count = count_arg;
	}
	*p_fmt = fmt;
	return count >= 0 ? (size_t)count : def;
}

static uint32_t print_fmt_type(const char **p_fmt)
{
	const char *fmt = *p_fmt;
	size_t size = sizeof(int);
	switch (*fmt) {
	case 'l':
		size = sizeof(long);
		if (*++fmt == 'l') {
			size = sizeof(long long);
			fmt++;
		}
		break;
	case 'I':
		fmt++;
		size = sizeof(size_t);
		if (fmt[1] == '3' && fmt[2] == '2') {
			fmt += 2;
			size = sizeof(uint32_t);
		} else if (fmt[1] == '6' && fmt[2] == '4') {
			fmt += 2;
			size = sizeof(uint64_t);
		}
		break;
	case 'h': fmt++; if (*fmt == 'h') fmt++; break;
	case 'z': fmt++; size = sizeof(size_t); break;
	case 'j': fmt++; size = sizeof(intmax_t); break;
	case 't': fmt++; size = sizeof(ptrdiff_t); 
	}

	uint32_t flags = 0;
	switch (*fmt++) {
	case 'd': case 'i': flags = PRINT_INT | PRINT_SIGNED | PRINT_RADIX_DEC; break;
	case 'u': flags = PRINT_INT | PRINT_RADIX_DEC; break;
	case 'x': flags = PRINT_INT | PRINT_RADIX_HEX; break;
	case 'X': flags = PRINT_INT | PRINT_UPPERCASE | PRINT_RADIX_HEX; break;
	case 'o': flags = PRINT_INT | PRINT_RADIX_OCT; break;
	case 'b': flags = PRINT_INT | PRINT_RADIX_BIN; break;
	case 's': flags = PRINT_STRING; break;
	case 'c': flags = PRINT_CHAR; break;
	case 'p': flags = PRINT_INT | PRINT_RADIX_HEX; size = sizeof(void*); break;
	}

	if (size == 8) flags |= PRINT_64BIT;
	*p_fmt = fmt;

	return flags;
}

static char *print_format_int(char *buffer, uint32_t flags, uint64_t value)
{
	bool sign = false;
	if (flags & PRINT_64BIT) {
		if ((flags & PRINT_SIGNED) != 0 && (int64_t)value < 0) {
			value = -(int64_t)value;
			sign = true;
		}
	} else {
		if ((flags & PRINT_SIGNED) != 0 && (int32_t)value < 0) {
			value = -(int32_t)value;
			sign = true;
		}
	}

	const char *chars = (flags & PRINT_UPPERCASE) != 0 ? "0123456789ABCDEFX" : "0123456789abcdefx";
	uint32_t radix = print_radix(flags);
	*--buffer = '\0';
	do {
		uint64_t digit = (uint32_t)(value % radix);
		value = value / radix;
		*--buffer = chars[digit];
	} while (value > 0);

	if (radix == 16 && (flags & PRINT_ALT) != 0) {
		*--buffer = chars[16];
		*--buffer = '0';
	}

	if (sign) {
		*--buffer = '-';
	} else if (flags & (PRINT_SIGN_PLUS|PRINT_SIGN_SPACE)) {
		*--buffer = (flags & PRINT_SIGN_PLUS) != 0 ? '+' : ' ';
	}

	return buffer;
}

static void vprint(print_buffer *buf, const char *fmt, va_list args)
{
	char buffer[96];
	for (const char *p = fmt; *p;) {
		if (*p == '%' && *++p != '%') {
			uint32_t flags = print_fmt_flags(&p);
			size_t min_width = print_fmt_count(&p, 0, *p == '*' ? va_arg(args, int) : -1);
			size_t max_width = SIZE_MAX;
			if (*p == '.') {
				p++;
				max_width = print_fmt_count(&p, max_width, *p == '*' ? va_arg(args, int) : -1);
			}
			flags |= print_fmt_type(&p);

			if (flags & PRINT_STRING) {
				const char *str = va_arg(args, const char*);
				print_append(buf, min_width, max_width, flags, str);
			} else if (flags & PRINT_INT) {
				uint64_t value = (flags & PRINT_64BIT) != 0 ? va_arg(args, uint64_t) : va_arg(args, uint32_t);
				char *str = print_format_int(buffer + sizeof(buffer), flags, value);
				print_append(buf, min_width, max_width, flags, str);
			} else if (flags & PRINT_CHAR) {
				char ch = (char)va_arg(args, int);
				print_append(buf, min_width, max_width, 0, &ch);
			}
		} else {
			if (buf->dst && buf->pos < buf->length) buf->dst[buf->pos] = *p;
			buf->pos++;
			p++;
		}
	}
	if (buf->length && buf->dst) {
		size_t end = buf->pos <= buf->length - 1 ? buf->pos : buf->length - 1;
		buf->dst[end] = '\0';
	}
}

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
	print_buffer buf = { buffer, count };
	vprint(&buf, format, args);
	return (int)buf.pos;
}

#if defined(UFBXC_HAS_STDIO)

bool ufbxc_os_open_file(size_t index, const char *filename, size_t *p_file_size);
size_t ufbxc_os_read_file(size_t index, void *dst, size_t offset, size_t count);
void ufbxc_os_close_file(size_t index);

struct ufbxc_file {
	bool used;
	bool read;
	bool error;
	size_t position;
	size_t size;
};

#define UFBXC_MAX_FILES 256
static ufbxc_file ufbxc_files[UFBXC_MAX_FILES];

FILE *ufbxc_fopen(const char *filename, const char *mode)
{
	for (uint32_t i = 0; i < UFBXC_MAX_FILES; i++) {
		ufbxc_file *file = &ufbxc_files[i];
		if (!file->used) {
			size_t file_size = SIZE_MAX;
			bool ok = ufbxc_os_open_file(i, filename, &file_size);
			if (!ok) return NULL;

			file->size = file_size;
			file->used = true;
			return file;
		}
	}

	return NULL;
}

size_t ufbxc_fread(void *buffer, size_t size, size_t count, FILE *f)
{
	ufbxc_assert(f && f->used);
	if (f->error) return 0;

	f->read = true;
	size_t to_read = size * count;
	size_t left = f->size - f->position;
	if (to_read > left) {
		to_read = left;
	}

	size_t index = f - ufbxc_files;
	size_t num_read = ufbxc_os_read_file(index, buffer, f->position, to_read);
	if (num_read == SIZE_MAX) {
		f->error = true;
		num_read = 0;
	}

	f->position += num_read;
	return num_read / size;
}

void ufbxc_fclose(FILE *f)
{
	if (!f) return;
	size_t index = f - ufbxc_files;
	ufbxc_assert(index < UFBXC_MAX_FILES);
	ufbxc_assert(f->used);
	ufbxc_os_close_file(index);
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
	if (f->size == SIZE_MAX) return 1;
	if (origin == SEEK_END) {
		ufbxc_assert(!f->read);
		f->position = ufbxci_clamp_pos(f->size, (long)f->size + offset);
	} else if (origin == SEEK_CUR) {
		ufbxc_assert(offset >= 0);
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
	ufbxc_assert(!f->read);
	*pos = (int64_t)f->position;
	return 0;
}

int ufbxc_fsetpos(FILE *f, const fpos_t *pos)
{
	ufbxc_assert(f && f->used);
	ufbxc_assert(!f->read);
	if (f->size == SIZE_MAX) return 1;
	f->position = (size_t)*pos;
	return 0;
}

void ufbxc_rewind(FILE *f)
{
	ufbxc_assert(f && f->used);
	ufbxc_assert(!f->read);
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
