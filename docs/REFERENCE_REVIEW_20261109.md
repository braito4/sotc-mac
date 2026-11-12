# Reference review — 2026-11-09

Reviewed changes since the November 6 branch-head check. Dependency pins remain
unchanged for the queued-binding diagnostic comparison. This is a source review,
not a SotC/M1 performance result.

| Reference | Reviewed head | Relevant change / decision |
| --- | --- | --- |
| bloodborne-runtime | `5e9327fc39bc` | Single pool plus exact reads, interval sets, retained render passes, Metal fences and small compute copies; see priorities below |
| bloodborne-mac | `fb7f5bb05e8b` | Single-pool runtime bump after 0.9.11 expose those runtime updates; packaging search paths, input fixes and higher-resolution memory floor are separate |
| bloodborne-recomp | `c60ccf8be752` | Unchanged from the previous check |
| SotC-PC | `1fe46aa3ff6a` | Unchanged from the previous check |
| shadPS4 | `f46a693c802f` | Append/mbcnt pattern handling, alpha/sample masks and export handling merit separate shader-specific review |
| PS4Recomp | `1b77072bee8b` | Recent stub and package-loading changes do not resolve our descriptor association gap |

## Adaptation priorities

1. **Small copies inside an existing compute encoder**, runtime
   [a03929b5](https://github.com/bloodborne-native/bloodborne-runtime/commit/a03929b5d0d09c070dea48e37c600e6cbf46ae5a).
   This can reduce compute/blit encoder switches. The upstream implementation
   limits total bytes and region count and rejects overlapping writes and same-object
   read/write overlaps. For this port, also account for distinct Metal objects
   sharing physical backing; object inequality alone does not prove independence.
   Preserve all buffer-history events and guard/snapshot ordering when adapting it.
   Start as a separately switchable experiment, with synthetic alias/overlap and
   alignment tests plus an actual same-scene M1 comparison.
2. **Keep render passes open across eligible barriers**, runtime
   [11116ebd](https://github.com/bloodborne-native/bloodborne-runtime/commit/11116ebd222b3d506baf255f4cd6113fc62ac95c).
   The upstream predicate excludes preceding graphics writes and attachment-image
   conflicts. Unknown attachment identity is treated conservatively. This is relevant
   to tile-based GPUs, but needs local attachment, scene-proxy and synchronization
   coverage before activation.
3. **Explicit Metal fence ordering**, runtime
   [c8c3ea8e](https://github.com/bloodborne-native/bloodborne-runtime/commit/c8c3ea8edd515689f5d198cd632965fb0829466e).
   Replaces broad render guest-memory declarations with render/compute/blit fences.
   It changes ownership and ordering across encoder and command-buffer boundaries.
   Preserve fence initialization, cross-buffer waits and retention through completion;
   review the whole dependency chain rather than cherry-picking the hot path alone.
4. **Exact reads / interval overlap sets**, runtime
   [bce636df](https://github.com/bloodborne-native/bloodborne-runtime/commit/bce636dfdf8627599c3ea4cc0468936149dfcf45)
   and [8641bedf](https://github.com/bloodborne-native/bloodborne-runtime/commit/8641bedfac6c1cf8bdfeb1a60034d88222ea22da).
   We already have optional comparisons for these approaches. Upstream now reverses
   the `CoarseReadTracking` toggle predicate relative to our pinned source: migrating
   the existing mask without checking the condition would invert its meaning.
   Do not describe an existing local experiment as a newly imported optimization.

The upstream [performance report](https://github.com/bloodborne-native/bloodborne-runtime/blob/5c7b5b61bd945b1d7aabadea3ba0376e27abee38/docs/PERFORMANCE.md)
reports modest combined gains for the three encoder changes on tested M1 Max/M3 Pro
routes, and larger gains from exact read tracking in its clinic comparison. It also
reports reduced CPU waiting without an FPS increase for interval sets in another
route. These are Bloodborne measurements, not predictions for SotC on an M1.

## Shader correctness candidates

shadPS4's [append/mbcnt generalization](https://github.com/shadps4-emu/shadPS4/commit/bf36bb1e4789c0d4c5d83131c8313c53a48ec532)
and [subsequent workaround restoration](https://github.com/shadps4-emu/shadPS4/commit/f46a693c802f2d60573e07927b5fdaa753136567)
should be reviewed together against the exact selected shader IR. Alpha/sample-mask
changes may matter to vegetation rendering, but do not establish the cause of this
port's vegetation defects or invalid index buffers. Avoid replacing shader behavior
while the producer/consumer identity remains unresolved.

The queued-binding diagnostic carries immutable descriptor observations with a
dispatch and compares them with native encoder bindings. Its original binary is
preserved; see [DEBUGGING.md](DEBUGGING.md).

## Optional small-transfer adaptation prepared

At the owner's request, the first priority is now adapted in a separate renderer:
`tools/build_small_copy_trial.py`, with the checked cumulative overlay
`tools/gpu_patches/sotc-small-copy-trial.patch.json`. It includes the queued-binding
diagnostic and retains the default runtime/CPU pins. `SOTC_SMALL_COPIES=1` opts in;
the same new binary with `SOTC_SMALL_COPIES=0` keeps the previous blit behavior.

Copies, byte fills and updates totaling at most 64 KiB/1024 regions use an already
open serial compute encoder. It refuses pending tessellation work, an open render
or blit encoder, out-of-bounds or overflowing ranges, overlapping writes, and any
same-buffer read/write overlap across regions. Zero-sized regions are omitted.
General copies between distinct native buffers retain blits: different objects can
alias physical memory, and the bounded diagnostic registry is not an ownership
oracle. Only UpdateBuffer's freshly allocated staging is admitted as a known
independent source. This intentionally has narrower coverage than upstream.

The original fill low-byte behavior is preserved. Transfer-history events retain
their type and command association, with explicit compute event/encoder labels.
Temporary row tables and update staging survive through the existing completion
lifetime. All fallible preparation precedes encoder-state changes; kernel/table
failure keeps the previous blit path. Aggregate logs report encoded calls/bytes
and fallback counts. No new waits, guest-data repair or fence policy is introduced.

The isolated ARM64 library compiles and loads. Sanitizer checks cover the planner,
4,000 independent overlap-oracle cases, actual Copy/Fill/Update functions and
Metal protocol doubles. A C++ translation of the exact kernel body matches byte
oracles for 1/4/16-byte lanes; this is not native shader validation. The standalone
Metal compiler is unavailable in this environment. Actual MSL compilation, GPU
ordering, visual correctness and performance still require the owner-run test.
Pass retention, fences and shader changes remain separate future integrations.

## Same-day follow-up: one native pool import

A final refresh at 20:53 UTC found runtime
[5e9327fc39bc](https://github.com/bloodborne-native/bloodborne-runtime/commit/5e9327fc39bc7648db7a21f767d3ce4c6c5822bf)
and the corresponding mac submodule bump. Native Metal now defaults to one pool
import while MoltenVK retains overlapping windows. Our launcher already explicitly
sets `BB_POOL_SPLIT=0`; importing this default change would not reduce our current
memory allocation further. Keep the local direct/indirect index rebasing above
4 GiB: the owner's M1 synthetic cases demonstrated failures without it. Upstream's
limited M3 Pro geometry check does not supersede those M1 results. No pin or
native comparison setting changes are needed for this follow-up.
