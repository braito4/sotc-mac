# Reviewed renderer adaptations

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
