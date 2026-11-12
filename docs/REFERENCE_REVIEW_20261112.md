# Reference review — November 12, 2026

The fixed split-pool owner comparisons activate two imports, then fail with Metal
out of memory after about 40 seconds. Large bindings cross the fixed window and
request multi-GiB staging copies. These runs do not reach the earlier invalid-index
failure and do not establish a gameplay FPS improvement.

## Isolated adaptation

Adapted the on-demand region strategy from bloodborne-runtime:

- [1050d343: import pool pieces on demand](https://github.com/bloodborne-native/bloodborne-runtime/commit/1050d343).
- [b093fad8: merge regions and release address-buffer references](https://github.com/bloodborne-native/bloodborne-runtime/commit/b093fad8).
- [a9967caf: keep the memory-saving mode optional](https://github.com/bloodborne-native/bloodborne-runtime/commit/a9967caf).

`SOTC_POOL_REGIONS=1` on the isolated renderer imports physical ranges rounded to
16 MiB, only after the existing host query proves full-span physical contiguity.
Overlapping regions merge; no requested descriptor is truncated and no guest bytes
are copied. Importing the entire binding before populating page-table entries
avoids first making many small imports that the same binding would replace.

Existing page-table aliases are rebound to the same physical memory in the new
region. Unmapped blocks are removed from the alias list so a later merge cannot
resurrect them. Old Buffer objects are retained through the backend's existing
past-next-submission deferred release; native allocations are subsequently retired
through that backend's completion path as well. The Metal unregister path removes
strong address/declaration/residency references and keeps the local provenance and
history removal callbacks. Disabled mode preserves the previous unregister path.

This initial adaptation deliberately omits the later idle-region budget eviction
from [ff6c2f07](https://github.com/bloodborne-native/bloodborne-runtime/commit/ff6c2f07).
Active regions may grow to the full pool, and overlapping replacements coexist
until completion. The change is not a hard memory cap or an established OOM fix.
Merged regions may exceed 4 GiB: existing M1 direct/indirect index rebasing remains
enabled. No sparse/discontinuous mapping is promoted to a contiguous import, and
the previous fragmented-mapping limitations remain.

The cumulative patch is `tools/gpu_patches/sotc-pool-regions-trial.patch.json`.
`tools/build_pool_regions_trial.py` requires the verified local control and prior
small-copy trial. It rebuilds all transitive dependents of the changed cache layout
and checks their actual dependency files, preserving original objects and binaries.
`tools/check_pool_regions_trial.py` provides sanitizer controls; the portable
region-manager tests are also in `tools/tests/test_pool_regions.py`.

Validation covers the 3 GiB boundary case, ranges above 4 GiB, bounds/overflow,
reuse/merge accounting, failed imports, alias rebinding, unmaps, deferred retirement,
2,000 independent union-oracle cases and the actual Metal unregister body with CPU
protocol doubles. Native library loading checks exports without initializing the
renderer. These do not validate Metal rendering, native memory reduction or FPS.

## Refreshed sources

Current heads checked before preparing the comparison:

| Repository | Revision |
|---|---|
| bloodborne-runtime | `234006761712f8621966c1a5e9f97b3b789a3d09` |
| bloodborne-mac | `fa99ba4728f40edf1a396d58381866fb626561e8` |
| bloodborne-recomp | `c60ccf8be752b8690f2813505c4daf9eafdb136b` |
| ShadowOfTheColossus-PC | `ec7c7220dc223e1d3a5e37d78be5ce9d2505ffda` |

Runtime's current head restores fixed windows by default and retains regions as an
optional low-memory path. This experiment follows that opt-in separation. Base
dependency pins, CPU binary, strict scatter guard, index rebases and diagnostics
are unchanged; no global dependency update is made.


## Follow-up: fragment reads must resolve demand regions

The owner region run reaches the usual invalid-index guard without the early
fixed-window OOM. Review found that the read-only fragment gather still resolved
only `pool_windows`, which is empty in demand-region mode; it therefore selected
CPU fallback with reason `pool_window`. The new isolated fragment-regions trial
passes each complete mapped physical segment to the demand-region importer before
encoding ordered GPU copies. Fixed-window resolution, mapping generation checks,
full-span planning, the no-partial-copy rule and writable rejection are preserved.
No writable scatter/copyback is introduced. The observed stale fragmented output
and invalid producer input remain open correctness issues.

An optional `SOTC_BINDING_ROUTE_TRIGGER` selects a shader already present in the
route selection. Eight startup packets are retained; 120 more are reserved from
the first eligible trigger packet onward. The fixed total remains 128. No trigger
preserves legacy admission; an invalid trigger exits24. This changes diagnostic
coverage, not dispatch order or data. Original synthetic controls verify late
producer/consumer retention after10,000 startup attempts and concurrent admission.
The actual production resolver is compiled in a CPU fixture with queued writes,
region merges, old-window rejection, fixed-window control and mapping-change refusal.

Builder/checker: `tools/build_fragment_regions_trial.py` and
`tools/check_fragment_regions_trial.py`; cumulative patch:
`tools/gpu_patches/sotc-fragment-regions-trial.patch.json`. Changed diagnostic
headers are explicitly fingerprinted against the prior trial manifest. Existing
native binaries remain unchanged. Game/GPU behavior of this follow-up is pending.
