# Reviewed renderer adaptations

The three upstream GPL-2.0-or-later backports come from bloodborne-native/bloodborne-runtime,
built on bbport and shadPS4. The separate `sotc-submission-trace` patch is a local
SotC diagnostic adaptation, not an upstream fix. Original source copyright/license notices are preserved.
See [upstream review](../../docs/UPSTREAM_REVIEW.md) for exact commit links, scope,
validation and the cache-version adaptation.

`manifest.json` binds the pinned base, complete patch set, patch bytes and each
source file's base/previous/after digest. Only the private `work/gpu-source` checkout is
adapted. Do not manually patch the pinned submodule or silently accept foreign
source changes. `tools/build_gpu.py` verifies and applies the set, then builds the
library and runs the real auxiliary shader regression.

The exact preceding three-backport state can migrate by applying only the local
diagnostic patch. Mixed, partial and unknown states remain rejected. The local
patch includes the repository-owned `runtime/src/gpu_submission_trace.h` via an
explicit CMake include directory. It records bounded CPU submission metadata and
Metal completion callbacks without GPU markers or guest-memory reads. It does not
identify a running shader from a CPU note or change shader/fence semantics.

Each `.patch.json` stores unified-diff lines, preserving significant blank context
without trailing whitespace in the repository. The builder reconstructs the patch
and feeds it directly to `git apply` after validating all input digests.
