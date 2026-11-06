# Independent trials prepared for the next owner session

All profiles keep virtual geometry disabled, the same verified host, two recording
threads, DMA/async submission off, the original index-rebase protections, and a
120-second archive-write interval. No gameplay tests are run automatically.
The local launcher verifies exact host and renderer hashes before continuing.

| Profile | Change from control | Reference and purpose |
|---|---|---|
| `control` | None | Vegetation/cache baseline, four preparation workers |
| `staging` | Opt-in power-of-two classes for new 16–64 MiB dedicated allocations | SotC-PC a2479f79 staging implementation, attributed upstream to PR #339; reduce repeated allocations across nearby sizes |
| `intervalos` | Merged interval sets for copy-overlap queries | Bloodborne runtime PR #11, 683602c64c3a; reduce CPU bookkeeping without changing access tracking |
| `texturas` | Final-image mip/layer offsets and matching | shadPS4 PR #5627, d85003474ea8; correctness experiment, not an established FPS gain |
| `cpu6` | Six preparation workers instead of four | Existing Bloodborne draw-preparation mechanism; same renderer/cache identity as control |

Use `./prueba_estrategias.command PROFILE`, optionally followed by the play budget
(600, 900, 1200 or 1800 seconds). Default is control/1800. Run only one at a time,
close normally, and inspect its log before choosing another. No combined/all mode
is provided. The older `prueba_sin_vegetacion_cache.command` remains available.

Start with control. Staging is the next performance candidate; interval sets
follow only if the eligible batching path is actually reached (look for the
copy-hoist mode marker). The previous interval trial did not demonstrate that.
The earlier texture-alias trial ended in a GPU watchdog stop; no causal attribution
was established. Keep this trial separate and later in the sequence.

## Conservative staging adaptation

Only allocation capacity changes, and only when allocating a new dedicated buffer.
Completed best-fit reuse, held/deferred ownership, same-tick tail bounds, real GPU
completion checks, ring allocation and destruction policies are unchanged. Unlike
the reference, this does not add unsynchronized reuse or new trimming/reserves.
Requests above 64 MiB retain the original size policy, including giant transfers.

Additional rounded capacity relative to the legacy allocation size is atomically
charged against a process-lifetime 64 MiB budget. Charges are never refunded, even
if a buffer is later trimmed. This bounds added padding, not total staging memory
or the difference in peak memory between gameplay runs: changed capacity can alter
later reuse. First allocations report activation and up to eight applied changes;
no applications means no demonstrated use of the strategy.

## CPU parallelism: evidence and limits

The 21:29:54Z run ends at roughly 377% of one CPU core. This is process CPU time,
not a map of unused cores. An M1 has unequal performance/efficiency cores, and
waiting/dependencies can leave capacity idle despite a slow frame.

Current preparation workers already prepare graphics pipelines, resource sharps,
texture descriptor hashes and vertex input ranges. The reported prepared-pipeline
usage totals 99.38% in that run; this covers the cases counted by DrawPreparation,
not every renderer operation or worker occupancy. Six workers are an exploratory
comparison, not a proven bottleneck fix. Preserve the two recording workers and
all queue ordering; increased concurrency can contend for bandwidth and cache.

Further CPU work should start with bounded worker busy/queue-wait measurements,
and allocation/decompression/hash cost attribution. Do not move rasterization or
shader execution to CPU merely to raise CPU utilization. Do not parallelize guest
memory writes, pipeline cache mutation or command ordering without ownership tests.

## Build and validation

Build one independent renderer with:

```sh
python3 tools/build_strategy_trial.py staging
python3 tools/build_strategy_trial.py intervalos
python3 tools/build_strategy_trial.py texturas
```

Each build combines the existing vegetation/cache patch with only its strategy,
uses verified baseline objects and records adapter header provenance. Original
binaries and source trees remain unchanged; CPU6 needs no new renderer.

Checks passed: all three native ARM64 builds, 50-symbol coverage plus load/unload,
actual-source alias/cache-hit controls, 600000 overlap queries/10000 batch decisions,
and actual staging-policy ASan/UBSan checks including 80000 concurrent budget
charges and boundary/overflow cases. Source comparison proves staging lifetime
and reuse code is unchanged. Dry launch chains verify all five profiles, exact
worker/feature settings and invalid-choice refusal. Public tool tests and source
boundary checks pass. No owner gameplay/FPS validation has occurred for this batch.

Renderer-specific cache identity remains strict. Different strategy binaries may
start cold. Compare repeated routes with similar cache state; do not classify
first-use shader compilation as a regression in a different subsystem.

Lazy texture usages, broad memory-ownership fixes and encoder/fence changes remain
pending dedicated adaptation, not silently included in these builds.
