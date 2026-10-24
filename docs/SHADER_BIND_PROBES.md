# Shader binding prefix probes

The owner launcher enables `SOTC_SHADER_BIND_TRACE=1` with the existing shader
provenance and diagnostic journals. For complete address-pair register packets,
the probe observes the first eight backing bytes at the shader origin after the
register-write observation and before lookup. It records program, origin, packet,
raw address register, actor, range, bank, host thread and monotonic timestamp.

The new `shader-bind-events.bin` has two independent 512-record histories:
prefixes with the recognized prologue, and prefixes without it or unavailable.
Its fixed mapped size is 132 KiB. Existing journals, rejection rules and shader
contents remain unchanged. The probe reads through the existing backing reader
after mapping/range checks, requests no GPU readback and takes no ownership of
guest memory. These observations are non-atomic and can race stores/remapping.
Additional CPU work can affect timing; no performance measurement is claimed.

Offline metadata-failure reports expose `shader_binding_review`. A comparison
requires matching program, origin, packet, raw register, actor and host thread,
with exactly one retained binding observation between the selected register
write and lookup. Missing, ambiguous, foreign-process or unreadable evidence
remains explicit. Old captures without the new journal remain readable.

The result distinguishes an already unrecognized prefix at binding from a
different prefix at lookup. It is not a pre-enqueue observation or proof of an
original writer, allocation identity, entire-program stability or valid shader.
Direct guest stores, aliases, copies and GPU writes are not intercepted by this
probe. Existing submission and imported-copy diagnostics retain their own limits.
It does not implement general PerVertexKHR support or fix SRT/pipeline errors.

```sh
SOTC_SHADER_BIND_TRACE=1 SOTC_SKIP_INTRO=1 SOTC_REQUIRE_POINTER_TRACE=1 \
  bash prueba_grafica_nativa.command base
```

Use `SOTC_SHADER_BIND_TRACE=0` to disable the new probe. Synthetic tests check
real journal retention, exact eight-byte reads, unavailable backing, and strict
join/refusal cases. The owner timeline below confirms native coverage; compilation,
synthetic checks and native observations do not establish a fix.

## First retained prefix-change timeline

An owner run with the new probes provides a paired pre-enqueue prefix with the
recognized prologue. At binding, 54.821 ms later, the sampled eight bytes differ
and lack that prologue; lookup 29 microseconds later retains the changed prefix.
Packet, register and descriptor origin observations agree, with one retained
matching consume candidate. This narrows the window between the earlier sample
and binding. It does not establish that the change happened after actual enqueue,
validate the entire earlier program, identify its writer, or distinguish a store
from allocation/backing reuse. The full private timeline and snapshots are saved.
