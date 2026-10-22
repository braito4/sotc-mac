# Reviewed renderer adaptations

`sotc-buffer-read-bounds` addresses the huge read-only fallback copy without
changing imported windows or enabling DMA. The final structured IR may prove
that every raw load of a resource is guarded by the same unsigned comparison
against a flattened uniform value, with a supported affine address. The host
evaluates the proof from the same per-draw snapshot as the shader and binds only
the proven byte range. Missing guards, mixed limits, writes, formatted accesses,
unknown expressions and overflowing U32 arithmetic retain the original path.
There is no arbitrary size cap or shader substitution. Shader metadata version
10 and binary version 24 invalidate the preceding resource layout.
`tools/check_buffer_read_bounds.py` uses actual IR plus the production
`UnifiedRange` method: an original synthetic GiB request cannot fit one window,
while its proven 40,972-byte range can. It checks all read widths, vector tails,
per-draw snapshots and conservative fallbacks without a GPU. Native SotC savings
and pixels still need owner validation. The next run prints bounded
`SotC buffer read bound:` records; retained staging is never freed by this patch.

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
