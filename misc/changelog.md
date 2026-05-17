### v0.22.0 (2026-05-18)
> `ufbx-rust 0.11.1`, `ufbx-python 0.0.7`

- Add `UFBX_EXPORTER_UFBX_WRITE` exporter

### v0.21.5 (2026-05-16)
> `ufbx-rust 0.11.0`, `ufbx-python 0.0.6`

- Documentation improvements
- Automatic release process for documentation/Rust/Python

### v0.21.4 (2026-05-16)

- Treat `map_Bump` as bump/normal map in `.mtl` files

### v0.21.3 (2026-02-09)

- Fixed constant rotation in animation baking

### v0.21.2 (2025-11-05)

- Fix GCC warning when using UBSAN without asserts

### v0.21.1 (2025-11-05)

- More DEFLATE fixes

### v0.21.0 (2025-11-02)

- Fix decompressing DEFLATE data written by `libdeflate`
- Rename `ufbx_evaluate_prop_flags_len()` for consistency

### v0.20.1 (2025-09-19)

- WASM64 compile support

### v0.20.0 (2025-06-16)

- Fix `ufbx_pack_version()` not working in preprocessor
- Ignore GCC `-Warray-bounds` warning on pre-GCC-14

### v0.19.0 (2025-06-15)

- Add `UFBX_PIVOT_HANDLING_ADJUST_TO_ROTATION_PIVOT`
- Fixed parsing empty `usemtl` in `.obj` files
- Improved DOM array API
- Blender FullWeights support

### v0.18.2 (2025-04-14)

- Reduced binary size in error handling (-8kB from full features)
- Minor cleanup

### v0.18.1 (2025-04-12)

- Added support more ASCII NAN/infinity formats
- Added support NAN/infinity in threaded ASCII parsing
- Fixed files containing negative object IDs failing to load
- Better standard conforming `uintptr_t` hashing and aliasing blocked copying

### v0.18.0 (2025-03-29)

- Animation curve extrapolation
- Support for new `TCDefinition` time codes
- Better support for user defined properties
- Added support for OpenUSD materials exported from 3ds Max
- Added warning/error when loading unsupported FBX versions (<3000 or >7700)
- Fixed strict warnings when compiling in C

### v0.17.1 (2025-02-25)

- Fixed warnings in newer compilers

### v0.17.0 (2025-02-22)

- Decode ASCII base64 embedded data during parsing
- Any embedded content with invalid base64 is now ignored
- Added a warning for broken base64 data during import
- Base64-encoded embedded data now is decoded if you request the DOM

### v0.16.1 (2025-02-21)

- Fixed parsing multi-part base64 ASCII embedded data (thanks to @drywolf)

### v0.16.0 (2025-01-03)

- Breaking: Change `ufbx_thread_pool_[run/wait]_fn()` return type to `void`
- Add thread pool documentation

### v0.15.1 (2024-12-16)

- Fix detection of SSE/NEON extensions

### v0.15.0 (2024-09-29)

- Support building without `libc`
- Allow overriding `malloc`/`stdio`
- Add `extra/ufbx_math.c`
- Add `extra/ufbx_libc.c`
- Remove `<string.h>` from header
- Add `ufbx_open_memory_ctx()` and `ufbx_open_file_ctx()`

### v0.14.4 (2024-09-26)

- Support building on Arm64EC

### v0.14.3 (2024-08-03)

- Fix transform evaluation rotation fast path mirroring
- Fixes some broken cases of baking animations when converting handedness

### v0.14.2 (2024-05-24)

- Fix `UFBX_VERSION` not matching tagged version

### v0.14.1 (2024-05-24)

- Documentation: Specify result of `ufbx_evaluate_transform()`

### v0.14.0 (2024-05-06)

NOTE: Changes behavior for Blender material import!
Metalness values and textures are not imported by default anymore to
increase compatibility with other importers. Specify the load option
`ufbx_load_opts.use_blender_pbr_material` for enabling the behavior.

- Add `use_blender_pbr_material`
- Re-introduce the convenience `ufbx_node.bone`
- Compute material part usage order for compatibility
- Fixed orthographic camera scaling
- Added `UFBX_INHERIT_MODE_HANDLING_COMPENSATE_NO_FALLBACK`
- Return memory metadata for baked animationsfrom baked
- Fix `.obj` units and add `ufbx_load_opts.obj_unit_meters`/`obj_axes`
- Sort bake nodes by `typed_id` and elements by `element_id`
- Add `ufbx_find_baked_node()`/`ufbx_find_baked_element()`
- Add `ufbx_vec3_normalize()`

### v0.13.0 (2024-05-03)

- Do not fail loading files with bad/missing `MappingInformationType`
- Add a new warning for above

### v0.12.0 (2024-04-07)

- Improve stepped tangent baking
- Add FBX audio support
- Add support for vertex element `W` components
- Add `UFBX_STATIC` user define to fix constants not being static
- Fix failing to parse `'Z'` type
- Fix threaded parsing of `'b'` arrays
- Clean up unused internals
- Remove FFI functions
- Remove `UFBX_EXPORTER_UNITY_BC`
- Minor fixes from new tests
- Fix performance regression of little-endian reads
- Optimized binary size ~30kB down
- More header documentation

### v0.11.1 (2024-02-18)

- Load vertices for meshes with missing index array

### v0.11.0 (2024-02-17)

- Add support for pivot conversion
- Add `ufbx_bake_opts.maximum_sample_rate`
- Improve bind pose support
- Do not concatenate absolute paths for filename
- Fix some rarely used functions

### v0.10.0 (2024-01-05)

- Add `ufbx_load_opts.node_depth_limit`
- Scale camera units by geometry scale

### v0.9.0 (2024-01-02)

- Add scale helpers to emulate inheritance modes
- Add support for loading using multiple threads
- Add API for baking animations into linearly interpolated keyframes
- Space conversion option using modified geometry
- Add handedness conversion via mirroring
- Optimize ACSII double parsing
- Implement advanced tangent features: clamp, velocity, auto, TCB
- Experimental OS-dependent utility in `extra/ufbx_os`
- Add support for embedded thumbnails
- Improved C++ API
- Various fixes found via semantic fuzzing

### v0.6.1 (2023-09-01)

- Add support for textures (FBX Video) in legacy (pre-7000) files

### v0.6.0 (2023-09-01)

- Do not use explicit width enums by default
- Adds `_FORCE_32BIT` to all enums by default
- Adds user define `UFBX_USE_EXPLICIT_ENUM` for old behavior

### v0.5.2 (2023-08-31)

- Ignore/fix most warnings in GCC and MSVC
- Fix issue with pre-6000 ASCII files with unquoted entries in `Children`

### v0.5.1 (2023-08-31)

- Ignore/fix most of warnings in Clang `-Weverything`

### v0.5.0 (2023-05-10)

- Add support for 32-bit float `ufbx_real` by defining `UFBX_REAL_IS_FLOAT`

### v0.4.0 (2023-05-07)

- Add `UFBX_Prop_Name` defines
- Add option for cleaning skin weights

### v0.3.1 (2023-01-21)

- Fixed `ufbx_evaluate_curve()` to return keyframe values at exact keyframe
  times, this is required for correct handling of constant interpolation.

### v0.3.0 (2023-01-20)

- Fixed `.obj` PBR detection with only textures (#55)
- Implemented `p`/`l` support for `.obj` (#56)
- Moved `"notes"` property under `ufbx_node` in FBX version 6100

### v0.2.3 (2023-01-09)

- Inverted `d` for `.mtl`
- Support `coat_affect_color`/`roughness` for OSL standard

### v0.2.2 (2023-01-08)

- Support `.mtl` `Pr`/`Pm` attributes

### v0.2.1 (2023-01-08)

- Added alias `UFBX_VERSION` for `UFBX_HEADER_VERSION`

### v0.2.0 (2023-01-08)

- `.obj` file format support
- optimized `strtod`/`deflate` implementations
- geometry transform / coordinate system handling
- material improvements
