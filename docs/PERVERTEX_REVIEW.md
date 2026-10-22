# PerVertexKHR proposals reviewed on 2026-10-22

The owner identified [MoltenVK PR #2858](https://github.com/KhronosGroup/MoltenVK/pull/2858).
GitHub's API reports it **open, draft and unmerged**, head
`6ed66f1b5d3575eed04b04e6332b389f398f9533`. It requires
[SPIRV-Cross PR #2699](https://github.com/KhronosGroup/SPIRV-Cross/pull/2699), also
**open, draft and unmerged**, pinned to
`81739edf516c2bef043ac61f510b4bd78e126a22`. These are dated observations, not moving
production dependencies. The API, commit list, compiler source and published test
contracts were reviewed; the PR title alone is insufficient.

## What could address our rendering failures

The portable path captures actual final vertex-producer outputs in a buffer,
exports their physical layout and emits a replay vertex shader that reads those
records without executing application vertex code again. Fragment `PerVertexKHR`
inputs then read the captured records through per-primitive index tables.
Portable barycentric inputs use replayed corner bases. This is distinct from a
derivative approximation or dropping the per-vertex decoration.

The portable compiler path requires MSL 2.4, not native `vertex_value`. The separate
native MSL 4.0 proposal needs Apple10-family hardware and is not an M1 solution.
[Issue #2569](https://github.com/KhronosGroup/SPIRV-Cross/issues/2569) describes the
native request; a request is not a merged implementation. PR #2699's native path
has reference coverage but its author reports no qualifying GPU execution.

MoltenVK #2858 activates portable capture/replay through the test-only
`mvkEnableTESFixtureDevice(VkPhysicalDevice)` entry point, before device creation.
Its adapter `MoltenVK/Tests/PerVertexTES/EnableTestDevice.mm` is excluded from
shipping targets. Replacing the compiler or setting performance variables does
not enable this path. Our separate native Metal renderer would also need its own
capture/replay integration; a MoltenVK change does not implement that backend.

Runtime obligations include primitive/index ordering, restart, degenerates,
provoking mode, instances/views, reserved buffer/interface slots, synchronization,
cache identity and resource lifetime/bounds. Tessellation, indirect and mesh code
in the branch is not blanket application coverage. The current MoltenVK rebase
documents M4 Pro qualification; its text explicitly says earlier M1/M2 reports
belong to previous revisions. Do not describe that as a current M1 pass or a
working SotC renderer.

## Commits intended to reduce overhead

| Commit in MoltenVK #2858 | Intended effect and tradeoff |
|---|---|
| [5ce16f9d9283](https://github.com/KhronosGroup/MoltenVK/commit/5ce16f9d9283f307267d9d13b9c7113c9fd0f3b4) | Reuse size-independent replay tables across eligible draws, reducing repeated CPU population. Tables grow geometrically and superseded buffers remain resident until pool destruction. |
| [d8159ba58251](https://github.com/KhronosGroup/MoltenVK/commit/d8159ba58251907e0a2c9bc03f21a2cc51747f7c) | `MVK_CONFIG_PERVERTEX_PREPASS=1` groups eligible indexed captures before a subpass, reducing encoder breaks. The look-ahead stops at barriers and unsupported command/state boundaries. |
| [380ec0493e48](https://github.com/KhronosGroup/MoltenVK/commit/380ec0493e488f0572fa57dbd2b37156f20439c2) | `MVK_CONFIG_PERVERTEX_SCRATCH_POOL=1` reuses scratch; independent `MVK_CONFIG_PERVERTEX_BATCH_RESIDENCY=1` batches residency operations. Pooling retains its high-water mark until device destruction. |

The opt-in changes above default off. They have no public reproducible application
timing/FPS result in the PR. Retained scratch/table memory is especially relevant
to our confirmed native memory exhaustion. The final mesh-indirect optimization
`6ed66f1b5d35` concerns the experimental native mesh path, not evidence of a fix
for SotC's ordinary indexed indirect draws. Importing isolated optimization
commits without the underlying capture/replay implementation is not sufficient.

[SPIRV-Cross #2697](https://github.com/KhronosGroup/SPIRV-Cross/pull/2697) is open
and unmerged. It **rejects** member-decorated `PerVertexKHR` that previously lost
its semantics; it adds no support. The related #2690/#2691/#2695/#2696/#2698 fixes
cover SampleMask, captured ClipDistance, interpolant chains, interface locations
and multiview indices; none alone implements general PerVertexKHR. The older
[#2137](https://github.com/KhronosGroup/SPIRV-Cross/pull/2137) is closed without
merge, so its title is not evidence that a fix shipped.

## Our isolated CPU experiment

The exact #2699 head was checked out and its ARM64 static core/GLSL/MSL libraries
built under private `work/pervertex-research`, separately from the renderer's
installed dependencies. Original synthetic fixtures pass an actual producer's
exported 32-byte layout to replay and fragment compilation. Cases select vertex
0, vertex 1, vertex 2 and mixed 0/1 reads:

- All four are rejected with default compiler options.
- All four compile with the portable captured-input API at MSL 2.4.
- Removing the required producer layout is rejected in all four cases.
- Replay uses captured records; the experiment does not use `vertex_value` or
  derivative reconstruction. Its executable loads no Vulkan, Metal or renderer.

Reproduce after preparing the exact clean PR checkout and its static build:

```sh
python3 tools/check_pervertex_proposal.py
```

This verifies synthetic SPIR-V-to-MSL conversion and API validation only. It does
not compile MSL with Apple's compiler, reconstruct a failing SotC pipeline,
validate topology/buffer contents, execute a GPU or measure memory/FPS. Production
still uses MoltenVK `v1.4.2` and SPIRV-Cross `vulkan-sdk-1.4.363.0`; runtime/recomp
pins, shader cache version 23 and the exact-only vertex-zero policy are unchanged.

Next integration work needs an isolated driver/backend experiment with actual
producer/consumer bindings, bounded buffer accounting and native pixel checks.
Preserve the current renderer and use the new [visual evidence](VISUAL_DIAGNOSTICS.md)
to compare it. Do not enable native MSL 4.0 or advertise extension conformance on M1.
