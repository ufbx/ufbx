#define ufbx_assert(cond) do { \
		if (!(cond)) exit(1); \
	} while (0)
#include "../ufbx_implementation.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
	#include <fcntl.h>
	#include <io.h>
#else
	// TODO: Implement binary stdin/stdout
#endif

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: fuzz_test <src-size> <dst-size>\n");
		return 1;
	}

	#ifdef _WIN32
		_setmode(_fileno(stdin), _O_BINARY);
		_setmode(_fileno(stdout), _O_BINARY);
	#endif

	size_t src_size = atoi(argv[1]);
	size_t dst_size = atoi(argv[2]);

	char *src = malloc(src_size);
	char *dst = malloc(dst_size);

	size_t num_read = fread(src, 1, src_size, stdin);
	if (num_read != src_size) {
		fprintf(stderr, "Failed to read input\n");
		return 1;
	}

	ptrdiff_t result = ufbxi_inflate(dst, dst_size, src, src_size);
	if (result != dst_size) {
		fprintf(stderr, "Failed to decompress: %d\n", (int)result);
		return 1;
	}

	fwrite(dst, 1, dst_size, stdout);

	free(src);
	free(dst);

	return 0;
}

