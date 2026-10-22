# Reviewed renderer adaptations

`sotc-pool-windows` shortens the first import to 2.5 GiB and positions the next
3.5 GiB import at 2 GiB, then uses 3 GiB strides with 512 MiB overlap. Complete
bindings and BDA table entries use explicit recorded window origins. The private
recorded giant range becomes a direct binding instead of a 3 GiB snapshot;
CPU/sanitizer tests cover boundaries, BDA, overflow, fallback and the prefix
tradeoff. This pool imports 512 MiB more aliases, not extra guest storage.
Native allocation savings and alias correctness remain unvalidated. The exact
old policy is BB_POOL_LAYOUT=legacy; BB_POOL_SPLIT=0 is preserved. Cohort
`pool_previous` migrates with pins/cache 26/9 unchanged. Replay only predicts
retained observations and does not add repeated requests as allocations.

`sotc-task-retirement` replaces immediate completed CPU coroutine-frame destruction
at all seven sites with ordered recording-consumer retirement. The existing
RunInOrder packet copies the frame address; disabled piping drains before the
callback and null rendering reclaims directly. It adds no new retirement queue
and does not change GPU resource lifetimes. Synthetic controls reproduce stale
borrowed storage with immediate release and preserve it with ordered release,
including exact-once and disabled/null cases under sanitizers. The exact previous
cohort migrates as `task_previous`; cache 26/9 and dependency pins stay unchanged.
This is a local adaptation of the ownership warning, not a claim that the Windows
PR is merged, that GPU completion has occurred or that native OOM is fixed.

`sotc-srt-finite` lowers exhaustive small scalar constant-table choices into
constant reads plus GPU selection before SRT flattening. It protects descriptor,
sampler post-op and pointer dependencies; unknown, cyclic, induction and excessive
inputs remain excluded. Original CPU fixtures check both selections, arithmetic
wrap, limits and exclusions against installed flattening. Cache 26/9 and exact
`finite_previous` migration avoid reusing old outputs; dependency pins are fixed.

`sotc-srt-uniform` resolves portable `ReadFirstLane` only through already accepted
uniform scalar offset expressions. Twenty-four original installed-pass fixtures
cover constant/user-data/arithmetic/nested reads and preserve rejection of
lane-dependent/divergent/induction/cyclic/excessive inputs. Compiler cache 25/9
invalidates prior output; dependency pins are unchanged. The same overlay adds
observational statistics every 256 traced staging reuses, without allocation,
ownership, completion or destruction changes. The exact preceding cohort migrates
as `uniform_previous`. Native correctness and OOM relief are unverified.

`sotc-srt-diagnostic` adds bounded failure-only IR shape provenance to the SRT
walker generators: shader identity, stage and pointer/copy site, without reading
guest memory or changing offset resolution. It emits at most eight records per
compiler pass and 128 per process; shapes stop at 32 nodes/six levels/eight inputs
per node/1,024 characters and omit immediate values. Shared/cyclic nodes use
references. Exact prior sources migrate as `srt_previous`; pins/cache remain
unchanged. `tools/check_shader_memory.py` exercises actual installed passes and
bounded provenance on CPU-only synthetic IR, including divergent/cyclic Phi.

The active manifest now targets **b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548** and
uses the consolidated `sotc-current-runtime` overlay. Earlier individual patch
files below document the previous 3170535f development history; the active builder
does not reapply them to the new base. The new base already incorporates the
three original upstream backports. The consolidated set preserves local
corrections/diagnostics, adds Ajnc's completed-task cleanup and uses current
installed SPIRV-Headers. Dear ImGui 1.93.0 is separately revision checked.
Unknown/partial source sets remain rejected; obsolete cross-base migrations are
not accepted as if they were the new base. Cache is **26/9**. The upstream
unresolved-image collector guard remains intact and receives its own observational
retention counter. See [current versions](../../docs/DEPENDENCIES.md) and the
[current review](../../docs/UPSTREAM_REVIEW.md#owner-requested-five-source-refresh-and-selected-adaptation).

`sotc-metal-inflight` adds a CPU admission gate to the actual native commit
wrapper: default eight reserved/inflight commands, `SOTC_METAL_INFLIGHT=0` for
uncapped control, explicit 1..64 limits. Only native completion releases capacity;
errors keep fatal exit 23 and do not become progress. Unsubmitted abandonment is
separate. CPU gate, real-wrapper Foundation/ARC and real cancellation-channel
fixtures run without Metal/device/game initialization. Existing chunks, resource
retention and ordering within each producer remain. Native savings/liveness/FPS
are unverified; eight is not a measured optimum or a total memory cap. The exact
previous cohort migrates as `inflight_previous`; pins/cache remain fixed.

`sotc-shader-decode` is an original defensive overlay on the shadPS4 decoder.
It enforces supplied slice ends for peeks/32-bit/64-bit reads, copies 64-bit
values without alignment/aliasing assumptions and rejects empty main/copy input
before indexing it. Failure-only private provenance distinguishes main/fetch/copy
and prints the supplied parent hash, offset and captured token. Errors still stop
through the real assertion handler; no unsupported instruction is emulated or
skipped. `tools/check_shader_decode.py` runs thirteen synthetic installed-decoder
modes with guard pages and a pinned-header overread control, without GPU/game
input. Fetch's synthetic end and borrowed-memory lifetime remain separate gaps.
The exact preceding cohort migrates as `decode_previous`; pins/cache stay fixed.

`sotc-metal-tess-instances` adapts bloodborne-runtime `dcaf50060880` (credited
to Wooloomooloo2 `4565225`, GPL-2.0-or-later). It routes each tessellated
instance through the existing single-instance encoder and preserves its base
instance in `PendingPatch` replay. `BB_TESS_INSTANCES=0` restores the previous
multi-instance skip. `tools/check_metal_tess_instances.py` exercises production
routing/parameter/pending fragments and complete `FlushPending` with a CPU
encoder double; kernel encoding and GPU pixels remain untested. The preceding
weak-probe sources migrate through the exact `tess_previous` cohort. No shader
cache, selected source pin, resource-lifetime or queue-completion rule changes.

`sotc-metal-retention` requests retained references for initial and rollover native
Metal batches, following the lifetime distinction described by MoltenVK #2860.
It keeps the existing past-next, residency and scratch completion guards and
changes no dependency or cache version. `tools/check_metal_retention.py` extracts
the production creation/commit/rollover/scratch methods and uses Foundation CPU
doubles to check explicit-resource retention, independent completion and reset.
Native memory behavior and GPU correctness remain unverified. Exact preceding
staging-tail sources migrate through the checked `retention_previous` cohort;
mixed/unrecognized inputs remain rejected.

`sotc-metal-probe` registers the repository-owned weak Objective-C bridge for
each traced native command buffer. Only stall/failure reports query its current
status/error/retention policy and device allocation; unavailable evidence is
explicit. The probe changes neither ownership nor queue synchronization and
does not wait for GPU completion. Journal fixtures check reentry outside the
lock and handle-generation isolation. The retention regression additionally
compiles the actual bridge with Foundation doubles and checks ARC weak ownership.
The checked `probe_previous` cohort migrates the preceding retained-reference
build without accepting unknown source changes. Native validation is pending.

`sotc-scene-proxy-retirement` adapts stale-proxy detection introduced in
[bmy 8185b88d8057](https://github.com/bmy/bbport-mac/commit/8185b88d80573fa300595d5c54450183b4109935)
(bbport/shadPS4/bmy, GPL-2.0-or-later). At the already-drained `OnSubmit`, proxies
whose exact original ImageId/UID no longer exists leave the active map, tracked
set and recent cache; generation invalidates remembered texture-set views. Moved
entries own views and images through real past-next completion. This replaces
the reference's global `Finish` with our existing conservative deferred lifetime;
live originals/content are preserved. New role/UID/mip counters are observational.
The actual-source CPU regression retains 204 proxies before versus two active/202
destroyed after, and checks mip/UID reuse, invalidation and deferred destruction.
The ARM64 incremental build passes; native savings/OOM behavior remain unverified.
The builder accepts the exact preceding memory state and rejects mixed changes.

`sotc-memory-trace` is an original observational overlay for large buffer origins,
staging selection, actual image allocation/destruction, cache/deferred lifetimes
and collector thresholds. It changes no resource size, lifetime, shader or cache
version. [Memory diagnostics](../../docs/MEMORY_DIAGNOSTICS.md) records bounds,
evidence semantics and the pending native validation. `tools/check_memory_trace.py`
compiles production range/copy/staging/destruction methods against CPU doubles.
The builder also accepts the exact preceding staging state and rejects untracked
renderer source/header copies that CMake globbing could otherwise include.

`sotc-staging-reuse` removes the upper size-class exclusion from best-fit dedicated
buffer reuse while preserving type, held ownership and actual GPU completion.
It releases no retained allocation. Inventory is now published after prewarm/new
allocations and includes sample age, allocation counts and reusable bytes.
`tools/check_staging_reuse.py` compiles actual allocation/release methods: the
pinned sequential fixture retains 288 MiB across five buffers; the adapted one
retains 128 MiB in one, with held/pending/type/best-fit checks passing. These are
synthetic capacities, not measured native game savings. The builder accepts and
migrates the exact preceding visual/copy overlay state through checked digests.

The `sotc-visual-trace` overlay adds bounded private presentation/draw/memory
evidence and optional completed game-image readbacks; see
[visual diagnostics](../../docs/VISUAL_DIAGNOSTICS.md). Metadata defaults on in the
combined launcher; pixel capture defaults off. No rendering resolution, fence
completion or existing resource lifetime is replaced.

`sotc-buffer-copy-chunks` adapts
[bmy/bbport-mac 5f381d5dd190](https://github.com/bmy/bbport-mac/commit/5f381d5dd190f55b6dae5608a162b6807c688928)
(GPL-2.0-or-later) to both pinned buffer-copy paths. Groups of at most 4096 regions
fit data/alignment/command headroom in a recording chunk; barriers/access tracking
stay outside the grouping. `tools/check_buffer_copy_chunks.py` compiles actual
source methods and the actual scheduler reservation guard, reproduces the pinned
overflow and checks the correction without a GPU. This is separate from native
Metal out-of-memory errors. The manifest also accepts the exact preceding
command-error overlay state and migrates it without reapplying earlier patches.

The three upstream GPL-2.0-or-later backports come from bloodborne-native/bloodborne-runtime,
built on bbport and shadPS4. The separate `sotc-submission-trace` patch is a local
SotC diagnostic adaptation, not an upstream fix. The separate
`sotc-pipeline-validity` patch rejects empty-handle cache hits and null Metal
registration. Original source copyright/license notices are preserved.
The `sotc-shader-compat` patch keeps wave64 scratch in one U32 view, invalidates
older shader binaries and applies the exact-only vertex-zero compatibility policy
before both new and cached MoltenVK shader modules when provoking vertex is fixed
to the first vertex. Configurable provoking vertex support disables the rewrite.
Other vertex selections keep
their original module and unsupported error. It does not admit derivative fallback.
See [upstream review](../../docs/UPSTREAM_REVIEW.md) for exact commit links, scope,
validation and the cache-version adaptation.

`manifest.json` binds the pinned base, complete patch set, patch bytes and each
source file's base/backports/previous/validity/after digest. Only the private `work/gpu-source` checkout is
adapted. Do not manually patch the pinned submodule or silently accept foreign
source changes. `tools/build_gpu.py` verifies and applies the set, then builds the
library and runs the real auxiliary shader regression.

The exact original three-backport state and preceding submission-journal state can
migrate by applying only their missing local patches, as can the pipeline-validity
state preceding the shader correction. Mixed, partial and unknown
states remain rejected. The diagnostic
patch includes the repository-owned `runtime/src/gpu_submission_trace.h` via an
explicit CMake include directory. It records bounded CPU submission metadata and
Metal completion callbacks without GPU markers or guest-memory reads. It does not
identify a running shader from a CPU note or change shader/fence semantics.

The builder also runs `tools/check_pipeline_validity.py`: actual cache-hit and
Metal registration guards are compiled with synthetic handles. The pinned guards
must reproduce invalid acceptance; the adapted guards must preserve valid handles
and reject empty ones. This is a CPU regression, not a rendered-image test.

Each `.patch.json` stores unified-diff lines, preserving significant blank context
without trailing whitespace in the repository. The builder reconstructs the patch
and feeds it directly to `git apply` after validating all input digests.

`tools/check_shader_compat.py` compares the original and adapted wave64 pass using
actual IR and SPIR-V emitters. Nine original synthetic modules reproduce missing
MSL workgroup declarations; nine corrected modules convert. It also checks exact
Flat conversion for vertex zero and unchanged rejection of other vertices. No GPU
is initialized; driver compilation and rendering need native verification.
