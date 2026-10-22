# Audio delivery diagnostics

The owner reports audible cuts during the defective 3D animation. The newest
07:00Z comparison now includes **22 native audio windows** from two active ports.
Each delivers about **23,912 input frames/s instead of 48,000**, around 470
256-frame blocks per five seconds instead of 938. Maximum observed submission gap
is 74.33 ms; output-lock waits peak at 8 microseconds, and no output/queue failure
is recorded. Queue waits occupy a substantial part of the windows. This is direct
delivery evidence consistent with the audible cuts, not a recording of speaker
output. The earlier uninstrumented logs recorded only the first eight calls.

Source review finds that the old queue bound admitted only two guest blocks
(512 input frames), independently of the hardware callback size. The actual
preceding adapter, tested against a deterministic 1024-frame device, reproduces
the measured half-rate pattern: 471 blocks in five simulated seconds, with half
the requested input unavailable at the callbacks. This model fits the native
observations; the old log did not query the actual native device quantum, so
1024 frames is not yet a measured hardware value for that run.

The adapter now obtains the real device buffer size and frequency from
[SDL_GetAudioDeviceFormat](https://wiki.libsdl.org/SDL3/SDL_GetAudioDeviceFormat).
It converts the output quantum to 48 kHz input frames, rounds to whole guest
blocks and admits one additional guest block. The overall input queue is capped
at 200 ms, at most 300 KiB for eight float channels; invalid, unknown or oversized
quanta return the existing provider error rather than silently underfeeding the
device. The same production-adapter simulation now delivers **939 blocks** and
239,616 input frames in five seconds, with no simulated callback input deficit.
No silence padding, sample duplication or time-stretched output is introduced.
Native device behavior and audible improvement still need owner verification.

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
replaced by the bounded device-quantum policy above. The next owner run should
record the real quantum and compare per-port delivery rates/gaps before claiming
the audible problem is fixed. Larger renderer stalls can still interrupt audio.

CPU tests use original synthetic PCM and timing inputs. They cover timing/reset
thresholds and run the actual guest-output fixture in both interpreter and JIT
with tracing enabled, including exact PCM/channel mapping, partial provider
failures, bounded queue waiting and a real two-second simulated timeout. Metadata
must preserve those existing contracts. These tests do not play audio or validate
that the owner's audible cuts have been fixed.

`python3 tools/check_audio_quantum.py` compares the reviewed preceding adapter
at commit `54d8b45` with the current adapter, using two synthetic streams, known
PCM and a fake clock/device. It loads no SDL, audio hardware or GPU. This check
requires that preceding Git object; CI fetches history and native contracts run
it as well. Pure policy fixtures also cover channel/sample sizes, 44.1 kHz device
conversion, invalid quanta and the queue bound.
