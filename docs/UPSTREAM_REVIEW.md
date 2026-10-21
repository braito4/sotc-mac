# Upstream review: 2026-10-21

The requested [bloodborne-mac history](https://github.com/bloodborne-native/bloodborne-mac/commits/main/)
contains newer release notes and runtime bumps than our original review. The app
repository primarily packages runtime/launcher revisions; renderer fixes are in
[bloodborne-runtime](https://github.com/bloodborne-native/bloodborne-runtime).
This review fetched current commit metadata and file patches through GitHub's API,
then compared the affected files with our pinned base. It does not infer suitability
from commit titles alone.

Observed heads during review were `17d3c4cdb0e7` for the app and `22923f080229`
for runtime. These are dated observations, not automatic update targets. Our runtime,
CPU and reference pins remain unchanged.

## Selected backports

| Upstream change | Relevance and implementation |
|---|---|
| [ef0080bb4331](https://github.com/bloodborne-native/bloodborne-runtime/commit/ef0080bb433152b476ab27b398d53eb23e8e934f) | Deduplicates auxiliary rect/quad tessellation input/output locations when several fragment inputs consume the same vertex parameter. Directly matches our MoltenVK duplicated `locn0` error. Applied unchanged. |
| [0e940e96cb08](https://github.com/bloodborne-native/bloodborne-runtime/commit/0e940e96cb08e06e0ea588bd41800dd62b00c659) | Invalidates cached shader binaries generated before location deduplication. Adapted to move our base's version 18 to 21; upstream's immediate predecessor was version 20. This does not import intervening aspect-ratio changes. |
| [e0348c24d911](https://github.com/bloodborne-native/bloodborne-runtime/commit/e0348c24d9117af5b22de2caa09d17a5e6a3dd07) | Retains shader module/compiled forms and graphics entries through deferred Metal draws and Vulkan handle reuse. Applied unchanged as a renderer lifetime correction. Our queue-overflow log does not prove that this separate lifetime fault occurred locally. |

All patches are GPL-2.0-or-later upstream work, not a new renderer authored here.
Patch files and input/output digests live in `tools/gpu_patches`. The build accepts
only a pristine pinned private checkout or exactly the reviewed complete patch set;
unknown modifications and partial applications are rejected. Digests verify source
identity; they are not proof of runtime correctness. `runtime/upstream` stays unchanged.

The isolated builder now applies the backports and records their full commit IDs in
`work/gpu-build.json`. Re-running it verifies/reuses the same patch set. Updating a
backport requires reviewing its source, manifest, tests and provenance together;
never relax the digest checks merely to get a build through.

## What was validated

The native ARM64 GPU library rebuild succeeds. The actual auxiliary SPIR-V emitter
is tested with 15 synthetic modules across all three auxiliary shader kinds, including
repeated and sparse locations, default/flat inputs, all 32 inputs and an empty
interface. Both sides of each interface contain the expected unique locations.
The same regression reproduces duplicate location zero in the original pinned emitter.
This is a shader-generation regression, not a Metal device or rendered-pixel test.

Run it after building without game input or a GPU device:

```sh
python3 tools/check_tessellation.py
```

Python tests verify exact backport output, repeat application and rejection of
unknown/partial source. Host symbol coverage remains complete after the library
rebuild. The lifetime change compiles but still needs real renderer exercise; no
new visible frame is claimed. Both host runtime contracts and this GPU regression
are needed because they validate different components.

## Changes not imported

The newer runtime also contains Bloodborne-specific shadow-context/game hooks,
aspect-ratio/input features, launcher/release work, MetalFX and additional pipeline
cache guards. We have not adopted the entire main branch or its CPU updates.
Bloodborne-specific object layouts must not be applied to SotC.

No fix for SotC's SRT Phi offset failures, oversized descriptor clamps or incompatible
BC1/BC5 image usage has been established by this review. Native presentation and
correct pixels remain unverified. Keep these as separate work items in the
[development handoff](DEVELOPMENT_HANDOFF.md).

The next combined Terminal experiment must use both the rebuilt GPU library and the
updated host notification queue. It should first establish whether the queue stop
and duplicate-location pipeline error disappear, then record the first remaining
failure and any actual visible output. Preserve existing isolated cache/configuration
behavior and read-only game input.
