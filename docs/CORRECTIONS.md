# Included corrections and validation limits

The build keeps the pinned runtime and CPU revisions, along with the reviewed
Metal adapters. This cleanup does not update dependency versions.

## Default behavior

The launcher retains direct and indirect index rebasing for buffers crossing
4 GiB, synchronous submission, safe math, bounded in-flight commands, audio and
controller adapters. The checked overlay retains staging/resource lifetime,
texture-view/overlap and command-decoder corrections.

## Fragmented memory

One consolidated overlay replaces the cumulative local trial patch chain. Its
renderer source and headers match the previously compiled group-writeback
control. It includes optional pool regions, fragmented reads and checked output
publication. Unknown/stale mappings, invalid bounds and GPU failures retain
explicit errors; they are not treated as successful rendering.

Shader-specific writeback and guards require exact, separately reviewed selectors.
They stay disabled without those selectors. This repository contains no extracted
shader identities, private layout profiles or game data. It therefore does not
reproduce the owner's private shader-specific comparison configuration by default.

Initializer/append publication passed the recurring crash location in private
owner runs, but later runs still showed artifacts and some failures. Paired-output
publication and sampled input freezes are not established general fixes. The latter
experiments are excluded from this public build; no freeze is enabled globally.

## Evidence limits

Recent private owner runs on M1 reported fewer artifacts and normal closing,
with frame-rate windows around 5 FPS. Routes, duration and cache warmth differ;
these observations are not a controlled performance benchmark. Colored texture
noise and occasional geometry artifacts remain unresolved.

Build/source checks establish reproducible inputs and compilation contracts.
Synthetic checks and matching source hashes do not establish correct pixels,
GPU/CPU race freedom, save persistence or behavior on other Apple Silicon models.

Publication checks: 37 public source/build tests passed; native CPU and host
compiled using the renderer dependency prefix. All 23 changed or cache-layout-dependent renderer objects were rebuilt with the public headers, then linked
with unchanged control objects. The resulting library covered all 50 required
host imports and passed load/six-export/unload checks without initializing Metal.
Both pristine-source and prior-overlay patch paths passed digest validation and
idempotence checks. The existing full native CPU contract suite also passed in
JIT and interpreter modes using the same dependency prefix. This was not a fresh download/rebuild of every dependency or
a new gameplay run.

## Metal reference update, 22 November 2026

* Fragmented texture uploads can gather several mapped ranges in GPU command order. The complete upload must be covered by existing imports. The launcher enables this from 24 GiB of RAM. Set `SOTC_TEX_GATHER=1` to test it on a smaller Mac. [SotC-PC changeset](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/fc20cf117794b5e988aaf78ccbf33bb16ada918b)
* The portable ARM64 shader interpreter and SPIR-V emitter support bounded 8 KiB reads for variable occluder indices. GPU-written lighting textures also survive overlaps confined to smaller mip levels. Both adaptations come from an open pull request. [SotC-PC #461](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/461)
* The low-memory profile caps the integrated GPU texture collector budget at 5120 MiB, respecting a smaller driver budget. Select it with `BB_LOW_MEMORY=1`. [Bloodborne runtime changeset](https://github.com/bloodborne-native/bloodborne-runtime/commit/d97fc678a912b2b6c23715f24ab0f197f646d55b)
* `SOTC_SYNC_COMPILE_LOG=1` logs the first 200 shader translations. The source launcher can reuse completed compatible caches between runs. Set `SOTC_REUSE_SHADER_CACHE=0` for a cold comparison. The compute prefetch and Vulkan culling queue remain specific to the upstream implementation. [SotC-PC changeset](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/53fd02920061cb41453b2f10802fe532d9510daa)

After a native build, `python3 tools/check_reference_renderer.py` exercises a synthetic shader through the ARM64 table reader, SPIR-V validation and Metal shader translation. It also tests fragmented texture planning. Install SPIRV-Tools to run this check.
