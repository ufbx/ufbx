# ufbx

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

The library is relatively heavily tested and fuzzed so it should be reasonably stable.
The animation stack API is still in progress but otherwise the library is starting to mature.
