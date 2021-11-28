#define SOKOL_IMPL

#if defined(__APPLE__)
	#define SOKOL_METAL
#elif defined(_WIN32)
	#define SOKOL_D3D11
#elif defined(__EMSCRIPTEN__)
	#define SOKOL_GLES2
#else
	#define SOKOL_GLCORE33
#endif

#define UMATH_IMPLEMENTATION

#include "external/sokol_app.h"
#include "external/sokol_gfx.h"
#include "external/sokol_time.h"
#include "external/sokol_glue.h"
#include "external/umath.h"
