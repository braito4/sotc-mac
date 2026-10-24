# Private visual diagnostics

## Combined private run report

The owner launcher now also enables bounded pre-failure CPU/shader journals.
The report includes pre_failure_journals and capture_readiness even when a CPU
signal, shader _Exit, fiber guard failure or cooperative stop bypasses another
subsystem's final dump. CPU observations remain per-site and shader writes/
lookups have independent channels. Interrupted/stale slots and foreign producer
PIDs are excluded; finite retention and absent requested files are explicit.
See [CPU diagnostics](CPU_DIAGNOSTICS.md#combined-pre-failure-capture).


After each supervised graphics run, the launcher writes `diagnostic.json` in its
fresh private state directory. The offline collector combines CPU history state
and retained time span, host register-image candidates, actual stop/exit markers,
pipeline/SRT failures, both presentation journal rotations, last periodic memory
summary, audio windows and a saved metadata-slice fingerprint. It also summarizes
up to eight preceding graphics logs separately, keeping different CPU fault sites
and missing/partial histories explicit. Co-occurrence or lower error counts is
not a common-cause finding or a fix. Rotation may omit older presentations, native
samples are not fault-time memory, and no missing sample is replaced with zero.
Malformed journal tails are reported as gaps; report failure never changes the
original diagnostic status. No device access or external publication is involved.

For an existing private run use `python3 tools/analyze_diagnostic_run.py --log
logs/OWNER_CAPTURE.log --state work/OWNER_STATE --output work/OWNER_STATE/diagnostic.json`.
This can reconstruct only evidence already retained on disk. In particular it
cannot recover CPU history skipped by an older direct-exit path. Required shader
metadata stops now retain that history and save the entire bounded scan-sized
slice; separate snapshot reads can still observe changed backing.

The combined report also retains `shader_decode_failures` separately from a
generic assertion, plus owned decode snapshot fingerprints/range context when
available. Missing older/borrowed captures are reported explicitly. `progress_review`
flags a long presentation gap before exit even when window closure returns zero.
It is a post-run clue, not a live timeout or GPU-hang proof: legitimate pauses,
minimization, startup/clock offsets and incomplete journals limit interpretation.

Main-shader decode failure saves at most 64 KiB from the already owned active
program, centred on the failed offset/end. Context identifies the saved word
range. It never rereads the guest pointer or follows adjacent metadata. Borrowed
fetch/copy failures do not produce an owned snapshot. First evidence is retained;
I/O failure preserves the original assertion/exit 23. Initial copying can still
race or start from invalid data, so saved bytes do not identify their writer or
prove correct shader provenance.

## Shader address producer observations

New kind-1 shader events with `required=1` mark a supplied packet observation.
For graphics events only, `hash` stores the post-copy raw address register, `length`
stores the original packet header, and `head` stores the SET_SH_REG address pair
when `found=1`. These are not shader hashes, code lengths or shader-code words.
Kind-2 lookup-event meanings remain unchanged. Older kind-1 events have no such
marker and cannot supply packet data retrospectively.

Legacy compute write actors 3/4 store the first eight bytes of ComputeProgram,
which are dispatch fields rather than its later address register. The offline
reader preserves this as `raw_program_prefix` and marks register/packet address
comparisons unavailable. Earlier apparent mismatches from this layout are not
corruption evidence. Corrected actors 6/7 capture the actual address words at byte
offset 48, checked against the pinned ComputeProgram layout, and match packet
coverage against register 0x20c rather than dispatch register 0x200. Required
compute descriptor contexts now identify the address offset; old compute contexts
with the graphics layout remain unknown. Original CPU fixtures cover both paths,
deliberately changed registers, partial writes and guarded short packets.

The packet observation occurs during command consumption, after the register
copy; it is not a submission-time snapshot. Equal failure/trace files can be
copies of the same lookup scan. They do not establish unchanged shader content
between enqueue and consumption; that comparison requires two timed observations
and a submission/packet identity, including indirect-buffer paths.

The checked overlay passes the actual packet extent. Pair decoding requires the
SET_SH_REG layout and coverage of both address words; partial/short/other packets
are unavailable, never interpreted as zero addresses. Raw packet, register and
lookup are separate observations. No additional shader-code reads or GPU work
are performed, and existing event-journal capacities remain unchanged.

Required metadata rejection saves `descriptor.bin` before the host-history hook.
Its context identifies the program slot and thread. `shader_origin_review` picks
the nearest retained matching program/thread write before the lookup and reports
packet/register/lookup agreement independently. An absent write or old event is
explicit. Agreement is not proof that the address/content is correct; differences
do not name the responsible writer. Raw descriptors are not atomic snapshots.

## Fallback shader lookup trace

With `SOTC_SHADER_PROVENANCE=1`, the first eight distinct unbound scanner
candidates save private `shader-lookup-trace/case-N` evidence before compilation.
`code.bin` is the exact existing scanner input (at most 65,564 bytes for the
current BinaryInfo); `metadata.bin` is its accepted metadata slice.
`descriptor.bin` is a separate CPU register-object observation, at most 512 bytes,
not an atomic guest descriptor snapshot. Context retains role, origin, hash,
length, metadata offset, hint, PID/thread/time and initial-head comparison.
`provenance.txt` retains the existing bounded register-write/lookup history.

The eight-slot budget is shared across graphics/compute types and never evicts
an earlier case. The distinct key uses origin, hash, metadata offset, length and
two initial words; it is not allocation or whole-content identity. Duplicate,
refused, failed, admitted and completed counts live in existing journal header
space. Cases add bounded private disk output without enlarging event journals;
ordinary hinted lookups perform no new capture I/O. Exit/signals can interrupt a
case; incomplete output is reported. Files are not a power-loss guarantee.

`diagnostic.json` includes `shader_lookup_trace` even when an unrelated CPU stop
ends the run. It checks saved extents/metadata, fingerprints content, excludes
foreign PIDs and compares an available decode window by numeric origin/hash.
Matching bytes do not identify an original writer; differing observations need
ownership/order investigation. The main-program lookup now accepts only metadata at the recognized prologue
reference, with a bounded non-overlapping code extent. Scanned candidates are
evidence only, and context marks them rejected with an association status.
Unrecognized layouts remain unbound. This does not validate guest object lifetime
or make the initial reads atomic.

## Offline shader failure grouping

Run `python3 tools/analyze_shader_failures.py logs/OWNER_CAPTURE.log --output
work/analysis/shader-failures.json` with an existing capture. Output is restricted
to private `work/` because shader identifiers belong to owner evidence. The JSON
groups failed fragment/vertex pairs, driver results, immediately adjacent MSL
conversion errors, SRT sites and any new bounded SRT provenance. It does not
infer shader semantics or replay game input. Counts are events; pairs omit other
pipeline state. Intervening unrelated lines break conversion association.
Separate fragment-identifier evidence elsewhere in the log does not replace
the strict adjacent-conversion classification or prove identical module variants.

The checked SRT diagnostic reports shader/stage, pointer/copy site and bounded
IR shapes on compilation failures. It omits immediate values, reads no guest
memory and preserves rejection/offset behavior. Limits are eight records per
pass and 128 per process, with at most 32 nodes, six levels, eight inputs per
node and 1,024 shape characters. Shared/cyclic nodes use references and truncated
traversal uses `limit`. A shape is not a complete or unique expression and cannot
replace original synthetic reproductions. Cached shaders may bypass compilation;
absence of records does not mean the shader is valid. No automatic cache purge
or game run is needed to analyze existing logs. Native provenance remains pending.

## Submission observations

The stall/failure journal retains a separate tail of **128 guest draw/dispatch
candidates**, in addition to the original 96-note all-command tail. Native
bind/state traffic cannot evict that separate guest tail. Candidate reports keep
command-buffer generation, local ordinal, shader IDs and draw/dispatch counts,
with total/retained/omitted values. The latest 118-candidate awaited batch would
fit this bound, but other batches can exceed it. These remain CPU recording
candidates, not per-command GPU progress or proof of the stalled shader.
Original CPU fixtures cover native-state noise, multi-buffer merge, truncation,
recycled handles and concurrent snapshots. Native validation is pending.

The submission journal's stall/failure dump now has separate `Metal native`
property observations. A weak command-buffer observer samples status/error,
retained-reference policy and device allocation only while reporting, without
GPU waits. `probe_registered=0`, `available=0`, `probe_error=1` and
`memory_available=0` denote distinct missing evidence; numeric zero fields do
not supply a missing memory measurement. Callback `status=0` without a completion
callback is still unobserved, not native NotEnqueued. Read the separate property
line and its availability flags. These sequential CPU properties are not an
atomic GPU-state capture or a shader progress marker. The October 23 09:39 Madrid
owner capture exercises these probes: both awaited chunks are available with
status 3/error 0, while the completed preceding batch has an unavailable released
probe. This validates observation availability, not correct completion or a hang
fix; the new separate candidate tail above still needs native validation.

The combined graphics launcher enables `SOTC_VISUAL_TRACE=1`. Set it to `0` to
disable the recorder. This adds CPU metadata and native allocation polling, not a
claim that any presentation contains correct pixels. The owner now selects
**original scene resolution**, preset 0 with the upscaler disabled, through
`prueba_grafica_nativa.command`. Earlier 640x360 captures remain historical.

Each run has a fresh private `BB_GPU_USER_DIR`. Its `visual/` directory contains:

| File | Evidence |
|---|---|
| `frames.jsonl`, `frames.previous.jsonl` | Two rotating journals, at most 4 MiB each; recorded presentations, actual queue submission and presentation API results, skipped presentations, capture decisions and native command failures. Each complete line is flushed. |
| `captures.jsonl` | Completed pixel-sample records retained separately from timeline rotation; at most 24 samples. |
| `frame-000001.ppm`, etc. | Optional previews of the game's own image, fitted within 320x180. No desktop capture. |
| `report.html` | Offline summary with embedded PNG previews, memory samples, counters and failure reasons. Generated after the supervised child stops, including error stops. |

The report also summarizes adjacent `audio.jsonl` metadata from
[AudioOut delivery tracing](AUDIO_DIAGNOSTICS.md), enabled by default in the
combined launcher with `SOTC_AUDIO_TRACE=1`.
It now includes [resource memory diagnostics](MEMORY_DIAGNOSTICS.md): large-buffer
origins, image allocation/destruction and deferred deletion, bounded live-image
summaries and collector thresholds. `SOTC_MEMORY_TRACE=1` defaults on independently
of pixel capture. The 09:25Z owner run on 2026-10-22 exercises these resource
records and the final native-failure summary; pixel capture remains untested.

Counters are cumulative **asynchronous process totals**, not per-frame GPU draw
counts: requested draws, missing pipelines, empty draws, resource binding failures,
bound draw requests, scene/reduction decisions and pipeline creation failures.
Native allocation and working-set recommendation are polled at most once per
second during native command submission; sample age is recorded. Tracked Vulkan,
image and staging bytes are separate observations. Staging arrays use device-local,
host-uncached/upload and host-cached/download order, repeated for rings, dedicated
buffers and the held subset of dedicated buffers. Categories overlap; do not add
them to native allocation or interpret them as physical RAM.
The newest overlay publishes inventory after prewarming/new allocations as well
as frame ticks; previously it could remain all-zero until the first pool tick.
`staging_sample_age_ms` is negative until a real inventory exists. New fields
record ring/dedicated allocation counts and reusable dedicated bytes per memory
type, admitted only when not held and the actual GPU tick has completed. An old
journal's zero array with no sample-age field must be treated as unknown inventory.
Counters also track dedicated allocations, reuse and reuse that the former size
class restriction would have rejected.

`present_result=1` is the swapchain API result. It does not establish image
contents, scanout or a completed game scene. Native failure events associate with
the latest CPU presentation sequence only; use the existing submission journal
for the actual failing command-buffer generation and retained candidates.

## Optional pixel evidence

Pixel capture defaults to **off**. For the next owner-run comparison, after the
previous diagnostic has closed, use the existing preset and enable it explicitly:

```sh
BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 BB_UPSCALE_PRESET=4 \
BB_FRAME_STATS=1 SOTC_VISUAL_CAPTURE=1 \
bash packaging/test-arm64.sh --graphics-game-test
```

The existing no-duration-limit default and GPU-progress supervision remain. The
launcher prints the private log and report locations. `SOTC_VISUAL_TRACE=1` also
line-buffers the optional frame-statistics CSV, reducing loss of final complete
rows on the native error handler's immediate exit.

Capture adds one dedicated host-visible readback buffer and a copy command. It
does not allocate another render target or reduce the rendering resolution.
Only RGBA8/BGRA8 unorm/sRGB game images are admitted. A reservation requires a
native memory sample no older than two seconds and at least 256 MiB between
allocation and the recommendation. This headroom rule is a diagnostic precaution,
not a guarantee against out-of-memory errors; the recommendation is not a hard
limit. Unsupported formats, oversized images and memory skips are recorded.

Limits: one outstanding readback, at least five seconds between eligible attempts,
24 attempts per run, at most 16 MiB per readback and a 320x180 RGB preview. The
source is the game-frame image before the window overlay. Pixels are analyzed
only from a deferred callback after the scheduler reports completion of the copy;
recording or queueing the copy does not count as a captured image. If GPU work
fails first, an outstanding request can remain without a completion record.

The CPU analysis box-averages the complete image into the preview. It records
encoded RGB luma mean/deviation, an 8x4 tile-luma grid, dark fraction, a content
hash and change from the preceding same-size sample. Near-black means at least
99% of preview pixels have all RGB channels at most 4. Changed fraction counts
preview pixels whose largest channel difference exceeds 8; identical requires
exact equality of preview bytes. These are heuristics: a static menu is not proof
of a hang, and dark previews do not identify a missing 3D scene. Luma is not
calibrated physical luminance, and previews lose fine detail.

Rebuild a report without running the game or GPU:

```sh
python3 tools/analyze_visual_trace.py work/gpu-game-state-XXXXXX/visual
```

The reader tolerates partial final JSON lines, bounds input, rejects redirected
preview files and embeds only strictly named local PPMs. Report failure preserves
the supervised diagnostic's exit status. Generated logs, reports and images stay
private under ignored output; never commit or upload them.

## Validation boundary

The ARM64 library builds. CPU synthetic tests cover pixel analysis, size overflow,
cadence/memory/in-flight/attempt limits, concurrent counters, timeline rotation,
retention of capture records, partial logs, HTML escaping and preview validation.
Launcher fixtures check defaults/overrides and preservation of exit 23 when the
report fails. Full native JIT/interpreter contracts pass. Owner runs now exercise
the metadata journal, presentation API results and native failure recording.
The 07:23Z run on 2026-10-22 also confirms the actual audio quantum, near-48 kHz
input delivery and four oversized staging reuses. Dedicated upload inventory is
512 MiB lower than in the preceding comparison, but the game still ends on native
memory exhaustion. The final staging snapshot is 31.8 seconds old; its reusable
counts are not proof that releasing resources at failure is safe. See the
[current checkpoint](DEVELOPMENT_HANDOFF.md#continuation-checkpoint) for the
comparison and limits. Pixel capture stays off. Readback, rendered pixels and a
tracing-overhead comparison still need native validation.
