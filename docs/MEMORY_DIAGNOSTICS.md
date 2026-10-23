# Private resource allocation diagnostics

The newest behavioral memory preparation is `sotc-pool-window-placement`:
balanced-v2 starts its middle import at 1.75 GiB, avoiding all 28 v1 crossing
ranges among 10,390 retained observations, including the last three early OOM
captures. No observed direct hit is lost. The selected 6592 MiB pool retains
three imports/7616 MiB aliases, unchanged from v1; other pool sizes may differ.
Native savings are unmeasured. `BB_POOL_LAYOUT=balanced-v1` restores the exact
previous policy and `BB_POOL_LAYOUT=legacy` the original. Read the first handoff
for sanitizer/BDA validation and the explicit long-tail fallback tradeoff.
`tools/replay_pool_windows.py` uses the actual C++ planner and distinguishes
recorded policies; `--all-retained --pool-mib 6592` uses the supplied pool size,
and private outputs must remain under `work/`. The observations below describe
the separate recorder and historical experiments.

The combined launcher enables `SOTC_MEMORY_TRACE=1`; `0` disables these additional
observations. This instruments the current renderer without changing shader code,
cache versions, buffer sizes, imported windows, resource ownership, eviction
policy, fences or completion. Current scene work uses native resolution;
the historical 640x360 comparisons below do not describe the current setting.
Keep DMA/async submission off.

The separate `sotc-staging-tail-v1` overlay permits aligned non-deferred CPU
uploads to use unused tails within the same tick when no completed buffer fits.
Offsets advance monotonically without overlapping occupied bytes; original
rounding, retention and completion checks remain. `staging_selected` now records
the actual offset; `staging_tail_selected` additionally identifies an appended
request. Both are bounded observations for requests at least 64 MiB. An appended
slice is not another allocation, a duplicate snapshot or proof that the buffer
has completed. CPU fixtures show 3,296 to 3,072 MiB symbolic capacity for one
concurrent-upload sequence; native savings remain unverified. Do not count
each slice's reported backing capacity as an independent allocation.

The separate `sotc-binding-copy-sharing-v1` correction shares an exact read-only
fallback snapshot between consecutive slots of one shader binding preparation.
It preserves the pool's ownership/completion rules and never shares across a
new preparation, write, formatted/intervening binding, changed effective range,
mapping generation or tick. The CPU original/adapted fixture retains symbolic
6 GiB before / 3 GiB after for the duplicate sequence; native savings are pending.
The 16:26 and 16:32 UTC captures each contain one large fallback copy and no large
shared-binding event, so neither exercises that duplicate sequence.
`copy_binding_shared` records a shared large binding, not a new allocation/copy
or permission to release storage. Each slot still executes mapping and barrier
bookkeeping. See the [handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The latest owner run (2026-10-22 16:32 UTC) ends after **104.474 seconds on Metal
OOM 8**, confirmed exit 23, with **11.588 GiB native allocation** at failure
(11.596 GiB sampled peak). One 2.885 GiB fallback request allocates 3 GiB, with
117.47 MiB padding. Complete image replay matches the forced ledger: 297 live
images / 780.69 MiB, including 279 registered images / 762.37 MiB and 18 proxies /
18.31 MiB after 938 actual destructions. No dropped records or tracker errors
occur. The last staging inventory is 41.947 seconds old; collection is sampled
440.292 ms before failure. Neither observation permits early resource release.

The 16:26 UTC run ends on the same error after 15.167 seconds, with 11.660 GiB
native allocation, 124 registered images / 496.39 MiB and no proxies yet. Its
single 3.027 GiB request rounds to **3.5 GiB**, leaving 484.47 MiB padding.
The rounding function is unchanged from the pinned runtime. These differing
effective ranges expose a 512 MiB allocation-class boundary, not an established
cause of the difference in progress. Smaller classes need synthetic growing-
request controls to avoid retaining additional buffers. Preserve requested
bytes, ownership, held guards and actual GPU completion. See the
[handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The preceding owner run (2026-10-22 15:58 UTC) ends after **11.704 seconds on Metal
OOM 8**, with confirmed child/supervisor exit 23 and **13.889 GiB native allocation**.
The uncapped buffer journal identifies two separate 3 GiB upload buffers. The
first is reused between submissions, then two read-only/unformatted slots of the
same shader/submission copy the same effective range into distinct buffers.
The failure staging sample is 773.754 ms old and contains 6 GiB dedicated upload.
Complete image replay agrees with the forced failure ledger: 86 registered
images / 347.54 MiB, 103 allocations / 17 destructions, no scene proxies or
tracking losses. Sharing an exact fallback snapshot within one binding
preparation is now prepared with synthetic lifetime/write/alias controls;
do not reuse or release occupied allocations by weakening completion guards.
See the [current handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The preceding owner run (2026-10-22 15:27 UTC) stops on GPU progress, with supervisor
exit 1 after 136.225 seconds and child exit unconfirmed. Native allocation peaks
at **11.584 GiB** before the stop; no forced final snapshot records later pressure.
Image-event replay matches the last bounded summary, then continues 18.844 seconds:
**464 live images / 1,135.69 MiB**, including 435 registered images / 1,107.32 MiB
and **26 proxies / 23.46 MiB after 1,037 actual destructions**. Two other images
have deletion queued (4.91 MiB); one small image has no association yet. The
image journal stays below its cap with no unmatched replay events. The buffer
journal is capped, and the 3 GiB upload's last staging sample is already 57.953
seconds old at the final presentation. These observations are not safe-release
permission, proof of a leak or a final native-memory measurement. See the
[current handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The preceding owner run (2026-10-22 14:58 UTC) ends after **120.309 seconds on Metal
OOM 8**, child/supervisor exit 23. Native allocation is 11.571 GiB at failure,
with a sampled peak of 11.577 GiB. The forced ledger and complete event replay
agree: **383 live images / 899.23 MiB**, including **23 proxies / 20.30 MiB**
after 900 actual proxy destructions. The other 360 images are 357 registered
(852.91 MiB) and three recently queued for deletion (26.02 MiB); retirement
observations are only 5.9–7.6 ms old. No dropped record or tracking error occurs.
One early cross-window copy allocates the retained 3 GiB upload. From the first
presentation result after 20 seconds, all 386.20 MiB of tracked Vulkan growth
is image growth. This narrows the pressure accounting, not the failing resource
or a proven leak. The last collection sample is 3.430 seconds old; a preceding
30.006-second sampling gap coincides with a long presentation gap, then sampling
resumes. See the [current handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The preceding owner run (2026-10-22 14:23 UTC) ends after 120.001 seconds on a CPU
SIGBUS, child/supervisor exit 138, without a recorded Metal OOM. The last
resource summary records 1,535 proxy creations / 1,505 actual destructions /
30 live proxies (27.32 MiB). Native allocation still peaks at 11.435 GiB and the
3 GiB upload remains (3,112 MiB total dedicated upload in the last staging
sample). The detailed image journal reaches its cap, with 875
dropped image events by the final summary; the bounded ledger reports no
tracking errors. CPU faults do not force a resource snapshot, so these are last
sampled values. Sustained memory stability remains unverified.
The preceding integrated-cohort run ended on Metal OOM 8 after 78.963 seconds,
retaining 336 proxies without retirement despite 317 source UIDs being destroyed.
The owner previously recovered visible
output after the earlier experiment was withdrawn. These traces are not a fix.

The current dependency cohort uses cache **24/9**; older native captures use
23/9. Its separate upstream collector correction retains unresolved GPU-written
contents when a neighboring page write makes download unsafe. The recorder now
counts those decisions as `gc_kept_unresolved_gpu`, separately from tiled
downloads and downloads kept without pressure. These are decision counts, not
distinct-image counts. That correctness fix can increase retained memory and
records 37 retention decisions in the 14:58 failure summary, separately from 67
tiled-download decisions (51 unresolved-GPU decisions in the preceding run); correctness of
the retained contents remains unverified.

The earlier scene-proxy retirement overlay retired no proxy in the OOM run.
A new ordered-frame maintenance overlay prevents waiting for all queues to empty
before running throttled cache maintenance. CPU serial/pipelined lifetime and
progress fixtures pass; the latest run now observes actual proxy destruction.
This does not establish exact concurrent memory savings or OOM avoidance. The
recorder remains observational. [Current checkpoint](DEVELOPMENT_HANDOFF.md#continuation-checkpoint)
describes deferred image/view ownership and cached-view invalidation.

## Recorded evidence

Each fresh private `BB_GPU_USER_DIR/memory/` contains:

| File | Content and bound |
|---|---|
| `buffers.jsonl` | First 512 KiB of observations for requests at least 64 MiB. Preserves early origins rather than rotating them away. |
| `images.jsonl` | First 32 MiB of successful image allocations, ownership/state changes and actual VMA destruction events (previously 8 MiB). |
| `summary.jsonl`, `summary.previous.jsonl` | Two rotating files, at most 1 MiB each. A snapshot at most every five seconds during collection, plus a forced snapshot before native error exit 23. |

Each complete record is flushed. Metadata uses host monotonic nanoseconds,
process-local thread/sequence identifiers, private resource handles, shader/slot
identifiers and guest range metadata. No guest content or image pixels are read
by this recorder. These files contain private addresses and must remain outside
Git and public reports. The existing offline `visual/report.html` includes them
after the child stops; it also reads old runs that lack memory traces.

The image-file cap was raised after the October 23 09:39 Madrid owner run
reported 7,229 dropped image records at its former 8 MiB limit. This expands
bounded on-disk evidence only; it does not reserve or free guest/Metal memory,
reconstruct missing past records or guarantee that a longer run stays below
the new cap. Existing dropped-record and fixed-inventory checks remain active.

Large buffer events connect a shader binding's declared and mapping-clamped size
with its pool physical offset, selected window, available window size and the
staging allocation selected. Events distinguish a direct window hit, a request
crossing its end and a missing/short pool mapping. A copy records its start and
successful CPU completion, selected buffer identity, rounded capacity, scheduler
tick and CPU staging duration. Dedicated staging observations identify new versus
reused storage and deferred ownership. A zero shader denotes unavailable binding
context, not an identified shader. The host thread and binding address help
separate interleaved requests. Cumulative copied bytes are traffic, not concurrent
allocation; CPU copy completion is not GPU completion.

Images are tracked in fixed storage for at most **8192 live allocations**. Every
successful `UniqueImage::Create` supplies its allocated size, dimensions, mips,
layers, samples, Vulkan format/usage and allocation flags. Initial and additional
sample backings are associated with their existing cache image UID. Registration,
unregistration and deletion requests update that association. Actual destruction
is observed only after `vmaDestroyImage` returns, including explicit destruction
and the destructor. Moving ownership does not create another allocation.

`image_scene_proxy` tags reduced-scene allocations with role **1**, original UID
and source mip (`source_level`). Role 0 means unspecified. These proxies are not
registered guest-cache images: their active state remains 0, while retirement
sets state 3 and records the required completion tick (current + 1).
`role_count`/`role_bytes` arrays contain unspecified/proxy live inventory;
`scene_proxy_created_*` and `scene_proxy_destroyed_*` count tagged creations and
actual destructions, while `scene_proxy_pending_*` remains live and owned by the
deferred callback. A capped event file does not stop these bounded-ledger
summaries. Tracker overflow makes per-role inventory/counters incomplete too.
Old logs have no role tags; their unassociated images cannot be retroactively
classified as proven scene proxies.

Snapshots retain allocation/destruction totals, net live bytes/count, bounded
tracked totals, size buckets and cache-state buckets. States are 0 constructed or
unknown, 1 registered, 2 unregistered and 3 deletion queued. The twelve largest
tracked live images retain their creation/deletion-request times and metadata.
Queued deletion is still live memory. Backings outside the cache can have no
known owner. Overflow, unmatched/untracked frees, diagnostic exceptions and
dropped records are explicit; a capped event journal is not complete history.
Summary rotation does not discard the fixed live tracker. Diagnostics catch their
own C++ exceptions and preserve the workload.

Collection snapshots record the observed usage, trigger/pressure/critical
thresholds, whether the collector is enabled and whether its usage comes from
the driver. They include existing kept/freed window samples and separate counts for
GPU-written tiled images and downloads retained without pressure. These values
are sampled before a collection pass; the failure snapshot reports their age.
The upstream kept/freed counters reset when frame statistics consume them.
`gc_kept_window_sample` and `gc_freed_window_sample` are not cumulative totals;
older logs called these same resettable observations `gc_kept_total` and
`gc_freed_total`. The reader accepts both names with the corrected interpretation.
The recorder's separate retention-reason counts are cumulative decisions, not
distinct images.
`gc_tick` and `scheduler_tick` are current collection/recording counters, **not
confirmed GPU completion** and not permission to free anything. Native allocated
bytes and the recommendation are supplied on the forced Metal failure snapshot;
ordinary resource snapshots leave those two native values zero/unknown. The
existing visual journal supplies periodic native observations.

## Interpreting the next owner run

Keep the existing command; memory metadata now defaults on:

```sh
BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 BB_UPSCALE_PRESET=4 BB_FRAME_STATS=1 \
bash packaging/test-arm64.sh --graphics-game-test
```

Inspect the first large copy's originating shader/slot, declared/clamped sizes,
window end and rounded allocation. Distinguish additional allocations from reuse
and repeated traffic through the same retained buffer. Then compare image
allocation/destruction totals and live-size/state buckets as native memory grows.
Growing queued-deletion bytes suggest examining deferred progress; growing
registered bytes suggest examining the working set and collection policy. Neither
observation alone proves a leak. A failure with unknown inventory or dropped
events needs that limitation preserved. Do not trim resources or cap guest buffers
based solely on these records. Driver temporaries, pipelines, physical residency
and the precise GPU instruction that fails remain outside this image ledger.

The source verifier rejects untracked C/C++/Objective-C sources or headers inside
the renderer's compiled directories, including ignored files: CMake globbing can
otherwise compile sync/conflict copies. Detected files are preserved. Four local
copies found during this work were preserved outside the build tree; this is not
evidence that they caused the previous runtime failures.

## Validation boundary

The ARM64 incremental library builds with the existing dependencies/generated
shaders. A completely fresh dependency build still needs external glslang.
CPU fixtures cover fixed-table overflow/reuse, nested binding context, concurrent
allocation accounting, pending versus actual destruction, bounded output,
rotation, directory redirection, exception containment, failure-exit flushing and
report escaping/partial input. Actual production `UnifiedRange`, `UnifiedCopy`,
dedicated staging allocation/rounding and both VMA destruction paths are compiled
against CPU resource doubles; an original synthetic request rounds to 3 GiB with
matching metadata, without allocating that amount or initializing a GPU.
The other renderer regressions and full JIT/interpreter contracts remain required.
The 09:25Z owner run on 2026-10-22 first exercises the records and final forced
failure summary, with a capped detailed image journal. The 09:41Z repeat has
complete uncapped lifetime history, no lost record or tracker error. It confirms
one large copy into 2.5 GiB staging, and 981 unassociated 640x360/320x180 images
retaining 806.35 MiB without destruction. Caller-role attribution still needs
isolation before a proxy-lifetime fix can be qualified.
See the [current checkpoint](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).
Pixel correctness and a tracing-overhead comparison remain unverified.
