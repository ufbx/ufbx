# ufbx [![CI](https://github.com/bqqbarbhg/ufbx/actions/workflows/ci.yml/badge.svg)](https://github.com/bqqbarbhg/ufbx/actions/workflows/ci.yml) [![codecov](https://codecov.io/gh/bqqbarbhg/ufbx/branch/master/graph/badge.svg)](https://codecov.io/gh/bqqbarbhg/ufbx)

Single source file FBX reader.

## Usage

```c
ufbx_load_opts opts = { 0 }; // Optional, pass NULL for defaults
ufbx_error error; // Optional, pass NULL if you don't care about errors
ufbx_scene *scene = ufbx_load_file("thing.fbx", &opts, &error);
if (!scene) {
    fprintf(stderr, "Failed to load: %s\n", error.description.data);
    exit(1);
}

// Use and inspect `scene`, it's just plain data!

// Geometry is always stored in a consistent indexed format:
ufbx_node *cube = ufbx_find_node(scene, "Cube");
ufbx_mesh *mesh = cube->mesh;
for (size_t face_ix = 0; face_ix < mesh->num_faces; face_ix++) {
    ufbx_face face = mesh->faces.data[face_ix];
    for (size_t vertex_ix = 0; vertex_ix < face.num_indices; vertex_ix++) {
        size_t index = face.index_begin + vertex_ix;
        ufbx_vec3 position = mesh->vertex_position.values.data[mesh->vertex_position.indices.data[index]];
        ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index); // Equivalent utility function
        push_vertex(&position, &normal);
    }
}

// There are helper functions for evaluating animations:
for (double time = 0.0; time <= 1.0; time += 1.0/60.0) {
    ufbx_transform transform = ufbx_evaluate_transform(&scene->anim, cube, time);
    ufbx_matrix matrix = ufbx_transform_to_matrix(&transform);
    push_pose(&matrix);
}

// Don't forget to free the allocation!
ufbx_free_scene(scene);
```

## WIP

The library is nearing first proper version so most of the API should be relatively
stable now for `1.0`. The header file contains some documentation but proper guide
is still on the way.

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
