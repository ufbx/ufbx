# ufbx [![Build Status](https://travis-ci.org/bqqbarbhg/ufbx.svg?branch=master)](https://travis-ci.org/bqqbarbhg/ufbx) [![codecov](https://codecov.io/gh/bqqbarbhg/ufbx/branch/master/graph/badge.svg)](https://codecov.io/gh/bqqbarbhg/ufbx)

Single source file FBX reader. Supports both ASCII and binary files starting from version 6100.

## Usage

```c
ufbx_load_opts opts = { }; // Optional, pass NULL for defaults
ufbx_error error; // Optional, pass NULL if you don't care about errors
ufbx_scene *scene = ufbx_load_file("thing.fbx", &opts, &error);
if (!scene) { do_fail(&error); exit(1); }

// Use and inspect `scene`, it's just plain data!

// Geometry is always stored in a consistent indexed format:
ufbx_mesh *cube = ufbx_find_mesh(scene, "Cube");
for (size_t face_ix = 0; face_ix < cube->num_faces; face_ix++) {
    ufbx_face face = cube->faces[face_ix];
    for (size_t vertex_ix = 0; vertex_ix < face.num_indices; vertex_ix++) {
        size_t index = face.index_begin + vertex_ix;
        ufbx_vec3 position = cube->vertex_position.data[cube->vertex_position.indices[index]];
        ufbx_vec3 normal = ufbx_get_vertex_vec3(&cube->vertex_normal, index); // Equivalent utility function
        push_vertex(&position, &normal);
    }
}

// There's also helper functions for evaluating animations:
ufbx_anim_stack *anim = ufbx_find_anim_stack(scene, "Animation");
for (double time = 0.0; time <= 1.0; time += 1.0/60.0) {
    ufbx_transform transform = ufbx_evaluate_transform(scene, cube, anim, time);
    ufbx_matrix matrix = ufbx_get_transform_matrix(&transform);
    push_pose(&matrix);
}

// Don't forget to free the allocation!
ufbx_free_scene(scene);
```

## WIP

This library is still a work in progress, but most of the implemented features are quite
usable as the library is heavily tested and fuzzed.

The API might change in the future, especially materials and evaluating animations.

## License

```
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2020 Samuli Raivio
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
----------------------------------------
```
