# Review of the supplied SotC-Mac-Port snapshot

Reviewed 2026-10-20: DehydratedFlask/SotC-Mac-Port, supplied ZIP SHA-256
`3d97f64f0d73f41f1c91812ffd6c1f9f320ef50402289189a55d828d09a93936`.

The snapshot contains native C++ recovery fixtures, a PSARC inspector, a Swift launcher
shell and design documents. It does not contain a working Metal renderer or native game
executable. Its graphics tasks are waiting on dependencies. Its final architecture replaces
game functions with ordinary C++; this project currently translates the original CPU code.
These approaches do not make their runtime components interchangeable.

The review built its native fixture suite (155/155 passing) and ran its PSARC tests (16/16).
Two of its pointer-analysis verifiers also matched the locally supplied executable. These
checks establish limited source/format facts, not successful gameplay or complete recovery.
No explicit license was found in the supplied snapshot. None of its implementation was
copied into this repository; its archive and temporary review builds remain private.

## Independently implemented follow-up

`tools/audit_content.py` validates PSARC v1.4 entry/block extents and decodes only each name
manifest. It counts types and reports case collisions without merging names, extracting
files or changing the input. It rejects oversized metadata, malformed compressed name
blocks, payloads outside the archive, duplicate/unsafe names and reports inside the game
root. Archive precedence must be explicit; it does not infer patch order from case folding.

The pointer arithmetic helpers distinguish untagged static values, null and tagged heap
values. They validate unsigned widths, arithmetic overflow and the 2 GiB window constraint.
Static values are not converted to host pointers. This is an analysis tool, not a replacement
for the game's pointer decoder; stricter diagnostic rejection must not alter guest semantics.
The game's static pointer resolution and full address model remain open.

The read-only local audit passed on six supplied archives containing 130,238 member names.
No case collisions were reported within those manifests. Asset payload decoding and content
correctness were not tested. The report stays in the ignored private work directory.

```sh
bash packaging/test-arm64.sh --content-check
```

## Shared backing improvement

A separate native adaptation, not taken from the snapshot, adds explicit file backing:

```sh
SOTC_MEMORY_BACKING=file bash packaging/test-arm64.sh --linked-test
```

The default remains the pinned POSIX shared-memory backend. File mode creates a private
0600 temporary file, unlinks it before returning its close-on-exec descriptor and sizes it
sparsely. MAP_SHARED aliases still share one object. It is not private anonymous memory,
a copy-on-write substitute, a persistent game save or a renderer. It can use temporary
filesystem space while the process holds mappings; it is intended for CPU diagnostics.

Tests verify zero initialization, both alias directions after closing the descriptor,
clearing through a view, unlinked lifetime, permissions, close-on-exec and errors. The full
linked suite also checks fixed maps, overwrite rejection, release and flexible accounting.
This allowed the linked game diagnostic to run in the restricted tool environment where
POSIX named shared-memory creation had returned EPERM. Neither permission policy nor guest
mapping bounds were changed.
