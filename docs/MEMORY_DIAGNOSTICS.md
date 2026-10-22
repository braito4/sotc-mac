# Private resource allocation diagnostics

The combined launcher enables `SOTC_MEMORY_TRACE=1`; `0` disables these additional
observations. This instruments the current renderer without changing shader code,
cache versions (23/9), buffer sizes, imported windows, resource ownership, eviction
policy, fences or completion. Keep scene **640x360**, DMA/async submission off.
The measured native boundary remains Metal out-of-memory code 8 after 175.402
seconds in the 09:25Z owner run on 2026-10-22, now with these resource records.
The owner previously recovered visible
output after the earlier experiment was withdrawn. These traces are not a fix.

## Recorded evidence

Each fresh private `BB_GPU_USER_DIR/memory/` contains:

| File | Content and bound |
|---|---|
| `buffers.jsonl` | First 512 KiB of observations for requests at least 64 MiB. Preserves early origins rather than rotating them away. |
| `images.jsonl` | First 8 MiB of successful image allocations, ownership/state changes and actual VMA destruction events. |
| `summary.jsonl`, `summary.previous.jsonl` | Two rotating files, at most 1 MiB each. A snapshot at most every five seconds during collection, plus a forced snapshot before native error exit 23. |

Each complete record is flushed. Metadata uses host monotonic nanoseconds,
process-local thread/sequence identifiers, private resource handles, shader/slot
identifiers and guest range metadata. No guest content or image pixels are read
by this recorder. These files contain private addresses and must remain outside
Git and public reports. The existing offline `visual/report.html` includes them
after the child stops; it also reads old runs that lack memory traces.

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
The 09:25Z owner run on 2026-10-22 exercises the records and final forced failure
summary. It confirms two cross-window copies sharing one retained 3 GiB buffer,
and late growth in unassociated images. The detailed image journal hits its 8 MiB
cap; the live tracker has no overflow, unmatched free or diagnostic exception.
See the [current checkpoint](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).
Pixel correctness and a tracing-overhead comparison remain unverified.
