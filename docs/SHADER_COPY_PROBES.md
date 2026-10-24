# Imported-copy probes for shader prefixes

[Fresh consumption probes](SHADER_CONSUME_PROBES.md) now add an independent
prefix read and watch-coverage state at packet consumption under the same flag.
Their separate journal is retained alongside the existing six journals.

A captured shader prefix changed between a pre-enqueue sample and binding about
55 ms later. The next owner run adds `SOTC_SHADER_COPY_TRACE=1` to investigate
overlapping imported copies during that window.

For readable pre-enqueue samples with the recognized prologue and no repeated
packet flag, the renderer registers the numeric eight-byte origin, packet and
capture ID with the host. A dedicated 2,048-slot watch table has 512 buckets and
four ways per bucket, separate from existing pointer watches. It replaces the
least recently registered entry only when all four ways are occupied. Its
bounded static storage is under 256 KiB. Busy registrations are dropped without
waiting, and counters report capacity evictions and contention. A conservative
page filter and bounded bucket checks limit the work for small writes.

Existing imported `memcpy`/`memset` observers record overlapping prefixes before
and after the actual libc operation. The pre-copy observation follows the
runtime's existing write-permission preparation. Records retain the observed
eight bytes or explicit unavailability, copy source/fill, destination, size,
thread and guest-site/stack-return candidate, plus registration packet/capture.
They use the existing pointer journal's two copy channels. No extra journal
mapping or guest memory write is added; other copy events share its finite tail.
Code prefixes are excluded from the out-of-range-pointer selection heuristic.
Cumulative counters separately retain the four overlap-event totals even after
the finite event tail is overwritten. Legacy 256-slot captures have no such
counters; their missing totals remain unknown rather than zero.

At a metadata rejection, a separate `prefix-watch.txt` snapshot reports whether
the requested address is still watched, its packet/capture registration and
timestamp, and its latest readable copy observation if any. Empty, unmatched,
busy, concurrently changed, disabled and absent-bridge states remain explicit.
Snapshotting reads only host-owned atomic metadata and never waits or reads the
guest. A match does not prove continuous coverage or allocation ownership.

Offline metadata-failure reports add `shader_copy_review` only when a matching
pre-enqueue sample exists. Candidate events must match origin, packet and capture
ID and lie between sample and lookup. Per-event comparisons with earlier/later
prefixes remain observations, without automatic causal before/after pairing.

These probes do not intercept direct scalar/vector stores, arbitrary aliases,
other host copy paths, frees/remaps or GPU writes. Registration does not retain
an allocation or prove ownership. No matching event is not evidence of no write.
Even an overlapping copy can race another writer or concern reused memory; its
call site remains a candidate. No shader repair is claimed.

```sh
SOTC_SHADER_COPY_TRACE=1 SOTC_SHADER_BIND_TRACE=1 SOTC_SKIP_INTRO=1 \
  SOTC_REQUIRE_POINTER_TRACE=1 bash prueba_grafica_nativa.command base
```

Set `SOTC_SHADER_COPY_TRACE=0` to disable the new watches. Synthetic controls
exercise actual protected reads, copy/fill and partial overlaps, unavailable
pages, zero/disjoint writes, collision eviction, contention refusal, exclusions
from pointer heuristics, registration identity/time filtering and the actual
renderer submission-to-host registration bridge. Native coverage awaits an owner
run with the rebuilt executable and library; no game/GPU was launched by the agent.
