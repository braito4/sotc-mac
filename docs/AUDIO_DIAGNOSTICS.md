# Audio delivery diagnostics

The owner reports audible cuts during the defective 3D animation. The latest
existing game log opens three 256-frame, 48 kHz, eight-channel float AudioOut
streams, with the initial successful output batches using two ports. Each buffer
represents about **5.33 ms**. That log records only the first eight output calls;
it contains no periodic delivery evidence with which to locate later cuts.
An unsupported auxiliary port-type request also appears, while main output
continues. Neither this nor shader/memory errors proves the cause of the cuts.

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

The existing PCM handling, channel conversion, two-input-buffer bound, cadence,
drain behavior, real provider errors and guest frame-count return are unchanged.
No silence padding or fabricated success is introduced. The next owner run can
establish whether cuts coincide with late production, output-lock contention,
backpressure, errors or broader CPU/GPU pauses before changing buffering policy.

CPU tests use original synthetic PCM and timing inputs. They cover timing/reset
thresholds and run the actual guest-output fixture in both interpreter and JIT
with tracing enabled, including exact PCM/channel mapping, partial provider
failures, bounded queue waiting and a real two-second simulated timeout. Metadata
must preserve those existing contracts. These tests do not play audio or validate
that the owner's audible cuts have been fixed.
