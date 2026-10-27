# Metal shader quality comparison

## Owner comparison and next measured trial

The owner reports substantially better graphics with safe math, but intermittent
vegetation and low frame rate. `20261027T140449Z` (safe math) ends after 157.000 s;
`20261027T140741Z` (fast math) ends after 52.057 s. Both report Metal command
error 8, insufficient memory, near 11.6 GiB allocated. Different durations and
workloads are not a performance comparison. Neither records a physical gamepad
connection; the logical keyboard-backed controller is not a detected Sony pad.

`bash prueba_calidad_memoria.command` retains safe math and the staging correction,
selects the existing single-pool import path, and enables frame statistics. This
removes 1,024 MiB of overlapping imported ranges versus balanced-v2; it does not
guarantee that physical allocation falls by that amount or that OOM is fixed.
Single-pool native Metal behavior remains a comparison: the original split path
was introduced for large-offset problems in the MoltenVK bridge.

An optional supervisor sampler writes `graphics-game-*.log.gpu.csv` every five
seconds, using macOS IOAccelerator counters. Device/renderer/tiler utilization
includes other apps; it is not process-specific, and missing counters are marked
unavailable, never zero. The shared launcher also writes its per-flip CSV in the
new private game-state directory. Frame rates and shader-compilation stalls can
now be compared alongside whole-device load without guessing from draw counts.

The new launcher explicitly enables SDL HIDAPI Sony PS4/PS5 support and background
input. Startup prints the number of SDL-visible gamepads; existing hotplug and
keyboard fallback stay enabled. No Sony HID device was visible during this review.
Pairing/connection and real button delivery require owner verification; no input
fix is claimed merely from setting driver hints. Vegetation flicker remains
unattributed; no culling, depth, alpha test or guest shader is bypassed.

Validation: 70 GPU Python tests passed; utilization parser controls and live
IOAccelerator sampling passed, including preservation of a synthetic child's
nonzero exit. The complete launcher chain preserves the requested settings.
Full ARM64 JIT/interpreter CPU contracts pass, including synthetic gamepad
buttons, axes, touch, capture, failed open and hotplug cases. An initially stale
launcher fixture referenced the previous dependency directory; it now derives
the directory from the pinned runtime revision and passes. No game was launched.

October 27, 2026. The latest owner capture, `20261027T131207Z`, ends after
87.035 seconds with a requested window close and child/supervisor exit zero.
The last pipeline summary reports 644 graphics and 456 compute pipelines,
with zero failures. SRT diagnostic expressions are not compilation failures.
Texture-cache reports still show memory pressure around 11.6 GiB (reported
in MiB). These observations do not validate pixels or sustained stability.

The installed Metal compiler defaults to fast math. Its existing
`BB_MSL_SAFE_MATH=1` option selects `MTLMathModeSafe` on macOS 15 and later.
This comparison tests whether floating-point optimization contributes to visual
defects; there is no demonstrated connection to a particular texture or shader.
It does not implement missing SRT operations or repair texture residency.

Run `bash prueba_calidad_shader.command` for safe math, or
`bash prueba_calidad_shader.command baseline` for explicit fast math. Both chain
through the existing staging-fixed launcher, retaining its resolution, barriers,
pool geometry and single-command admission limit. The shared launcher creates
a fresh user/cache directory for each invocation; no existing cache is deleted.
The supervisor records both the math selection and upload-batch setting.

Compare the same save, viewpoint and camera motion, especially ground textures,
surface flickering, shadows and distant texture transitions. Keep first-load
shader compilation separate from subsequent frame pacing. Safe math may cost
performance and is an opt-in experiment, not a validated graphical correction.
No native game run was performed to prepare this comparison.
