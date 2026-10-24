# Fresh shader prefixes at packet consumption

For the next owner control, the recorder also samples the runtime pool offset
before enqueue. On a changed prefix it queries that offset again and saves both
in `shader-mapping-events.bin` (another bounded 132 KiB journal). The observation
pool remains capped below 640 KiB. An unavailable query stays explicit; the hook
is resolved dynamically and adds no mandatory renderer import. Same pool offset
does not prove a surviving allocation, uninterrupted mapping or absence of reuse.
Different offsets establish a backing change between two separate observations,
without establishing its cause. The offline mapping review requires exact
capture/packet/origin/thread/time and prefix matches with a changed consume event.

`SOTC_SHADER_COPY_TRACE=1` now adds a fresh checked eight-byte backing read when
the submission recorder's packet is consumed, before its register update. The
existing consume-phase submission record still contains the original immutable
pre-enqueue sample; it is not relabeled as a new read. The new observation has
its own `shader-consume-events.bin`, two 512-entry tails and a 132 KiB mapping.
Changed, unavailable and unrecognized prefixes use a separate tail from ordinary
unchanged recognized prefixes. Cumulative counters retain total observations,
sampled changes and unreadable heads after tail overwrite.

Each fresh record retains capture, packet, origin, register index, pre-enqueue
prefix, fresh prefix, availability, repeated-packet ambiguity, thread and time.
Watch-state snapshots after scan/registration and at consumption help distinguish
an ineligible sample, an unconfirmed registration, a watch subsequently missing,
and the same or another registration still present. These host-metadata snapshots
do not wait or read guest memory. Missing coverage is not evidence of no write.

The offline report adds `shader_consumption_review`. It requires one unique
matching origin, packet, register range, graphics/compute role, consumer thread
and ordered consumption/write/lookup timestamps. Ambiguous repeated packets are
not used to infer a change interval. Together with bind/lookup this can locate a
sampled prefix change before consumption or after that observation; it cannot
identify the original writer or prove the same allocation survived the interval.

If the general shader journal has lost the requested lookup, offline analysis
can recover it from the separately saved failure-program history. The report
labels that evidence source and leaves timing unavailable. It does not merge
sequence domains or invent timestamps/thread IDs for stage associations.

Reads reuse the renderer's mapped/range checks and backing reader. They can race
stores or remapping and are not an atomic snapshot. The probe observes the
original scanned packet's origin; a later packet-payload change can make it
different from the address actually bound, in which case association is refused.
No permissions, shader acceptance, queue ordering or retry policy changes.

CPU-only controls cover changed and unchanged heads, a changed recognized prefix,
compute and indirect packets, repeated packets, post-scan unavailability, bounded
capture pressure, concurrency, inherited disable settings and conservative
association refusals. No game or GPU is launched by those controls.

```sh
SOTC_SHADER_COPY_TRACE=1 SOTC_SHADER_BIND_TRACE=1 SOTC_SKIP_INTRO=1 \
  SOTC_REQUIRE_POINTER_TRACE=1 bash prueba_grafica_nativa.command base
```
