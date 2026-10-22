# Audio delivery diagnostics

The **09:25Z comparison on 2026-10-22** has 68 complete native audio windows,
34 per active port. Mean delivery is about **47,488 input frames/s per port**;
submission gaps reach **120.404 ms** and output-lock waits 89 microseconds.
No queue/output failure is recorded. The run ends after 175.402 seconds on Metal
memory exhaustion. These are input-delivery observations, not evidence of
uninterrupted speaker output or audiovisual alignment.

The earlier **08:50Z comparison on 2026-10-22** after the renderer rollback has 36 native
audio windows, eighteen per active port, spanning about 90 seconds per port.
Mean delivery is about **46,200 input frames/s per port** and maximum submission
gap is **186.015 ms**. No queue/output failure is recorded; output-lock waits peak
at 234 microseconds. The device remains 1024 frames at 48 kHz. These observations
do not establish uninterrupted playback or a hardware underrun. The game ends
on Metal memory exhaustion after 94.305 seconds; this checkpoint changes evidence
only, retaining the quantum correction and existing audio contracts.

The owner previously reported audio **much improved**. The earlier 07:23Z comparison on
2026-10-22 includes **52 native audio windows**, 26 per active port, spanning about
130 seconds per port. Mean delivery is **47,684 input frames/s of the requested
48,000**, about 99.34%; median window rates are 47,988–47,995 frames/s. The native
device reports a **1024-frame quantum at 48 kHz**. The bounded input limit is
32,768 bytes before adding the next 8,192-byte guest block, allowing five blocks.
Output-lock waits peak at 54 microseconds, with no queue/output failure recorded.
Submission gaps still reach **121.85 ms**; audible improvement does not establish
uninterrupted output or audiovisual synchronization.

The preceding 07:00Z comparison contains 22 windows and delivers about **23,912
input frames/s per port**, around half rate, with a maximum 74.33 ms gap. Delivery
has roughly doubled after the device-quantum correction. These are input delivery
measurements, not speaker recordings; maximum gaps from recordings of different
lengths are not directly comparable jitter rates. Earlier uninstrumented logs
recorded only the first eight calls.

Source review finds that the old queue bound admitted only two guest blocks
(512 input frames), independently of the hardware callback size. The actual
preceding adapter, tested against a deterministic 1024-frame device, reproduces
the measured half-rate pattern: 471 blocks in five simulated seconds, with half
the requested input unavailable at the callbacks. This model fits the native
observations. The old log did not query the native device quantum; the corrected
07:23Z run now confirms 1024 frames on the actual device.

The adapter now obtains the real device buffer size and frequency from
[SDL_GetAudioDeviceFormat](https://wiki.libsdl.org/SDL3/SDL_GetAudioDeviceFormat).
It converts the output quantum to 48 kHz input frames, rounds to whole guest
blocks and admits one additional guest block. The overall input queue is capped
at 200 ms, at most 300 KiB for eight float channels; invalid, unknown or oversized
quanta return the existing provider error rather than silently underfeeding the
device. The same production-adapter simulation now delivers **939 blocks** and
239,616 input frames in five seconds, with no simulated callback input deficit.
No silence padding, sample duplication or time-stretched output is introduced.
The newest native comparison and owner report now confirm delivery and audible
improvement; larger scheduling gaps remain.

The combined launcher now enables `SOTC_AUDIO_TRACE=1`; `0` disables it. This adds
metadata to stderr and a fresh private `BB_GPU_USER_DIR/audio.jsonl`. No sound,
sample data, microphone input or audio recording is saved. The visual report
reads this adjacent metadata file when available.

Each active port emits a window after five seconds when an output arrives, plus
pending statistics on close, drain, queue/submission failure and normal final
reporting. Fields include accepted buffer/byte counts, actual maximum gap between
successful submissions, gaps exceeding two buffer periods, deadline lateness,
input-queue byte extrema/zero observations, queue-wait duration/failures and the
maximum wait for the AudioOut output mutex. Timestamps use the host monotonic
clock. Windows reset their totals while preserving the preceding submission time
for the next gap. A successful drain resets that gap history.
New windows also record `device_frames`, `device_frequency` and
`input_queue_limit_bytes` (the limit **before** adding the next guest block).

The first sixteen output errors are recorded separately, including validation
errors; they must not all be classified as device errors. All metadata is limited
to 1024 complete lines per process and flushed per line. An immediate native
error exit can still omit the pending, less-than-five-second audio window.
Diagnostic I/O itself can affect timing, so compare tracing on/off if overhead
becomes material. The HTML reader bounds audio input and tolerates a partial tail.

[SDL_GetAudioStreamQueued](https://wiki.libsdl.org/SDL3/SDL_GetAudioStreamQueued)
reports bytes of **input**, not the amount ready at the device. A zero input queue
does not prove an audible underrun; a long submission gap is a timing lead, not
a recording of what came out of the speakers. Device/conversion queues, actual
hardware playback and audiovisual alignment remain unmeasured. Do not infer a
particular video frame from an audio metadata window.

PCM handling, channel conversion, cadence, drain behavior, real provider errors
and the guest frame-count return are preserved. The two-block queue bound is
replaced by the bounded device-quantum policy above. Keep the measured correction
while investigating residual gaps and rendering stalls; complete playback and
alignment still need verification. Larger renderer stalls can interrupt audio.

CPU tests use original synthetic PCM and timing inputs. They cover timing/reset
thresholds and run the actual guest-output fixture in both interpreter and JIT
with tracing enabled, including exact PCM/channel mapping, partial provider
failures, bounded queue waiting and a real two-second simulated timeout. Metadata
must preserve those existing contracts. These CPU tests do not play audio; the
separate native comparison and owner report establish the audible improvement.

`python3 tools/check_audio_quantum.py` compares the reviewed preceding adapter
at commit `54d8b45` with the current adapter, using two synthetic streams, known
PCM and a fake clock/device. It loads no SDL, audio hardware or GPU. This check
requires that preceding Git object; CI fetches history and native contracts run
it as well. Pure policy fixtures also cover channel/sample sizes, 44.1 kHz device
conversion, invalid quanta and the queue bound.
