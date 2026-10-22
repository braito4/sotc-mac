# Private visual diagnostics

The combined graphics launcher enables `SOTC_VISUAL_TRACE=1`. Set it to `0` to
disable the recorder. This adds CPU metadata and native allocation polling, not a
claim that any presentation contains correct pixels. Scene configuration stays
**640x360**; existing UI and output sizes are unchanged.

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

Counters are cumulative **asynchronous process totals**, not per-frame GPU draw
counts: requested draws, missing pipelines, empty draws, resource binding failures,
bound draw requests, scene/reduction decisions and pipeline creation failures.
Native allocation and working-set recommendation are polled at most once per
second during native command submission; sample age is recorded. Tracked Vulkan,
image and staging bytes are separate observations. Staging arrays use device-local,
host-uncached/upload and host-cached/download order, repeated for rings, dedicated
buffers and the held subset of dedicated buffers. Categories overlap; do not add
them to native allocation or interpret them as physical RAM.

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
report fails. Full native JIT/interpreter contracts pass. No new real GPU run
validates readback, rendered pixels, diagnostic overhead or memory improvement.
