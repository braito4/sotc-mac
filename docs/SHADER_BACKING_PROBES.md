# Runtime backing-write overlap probes

Adaptive round 2 reused the journal for `runtime_memory_note_write` notifications,
including those surrounding runtime read/pread destinations. Notification events
are explicitly distinct from actual backing writes and have no success result.
The next revision also observes the generic GPU-write intent sites already called
for fences, command writes, DMA and buffer/image downloads. Its optional bridge
runs before the upstream free-check enable guard, without enabling that
game-specific allocator or label-trap diagnostic. GPU intent events retain the
source enum and fence sequence, not a successful-write claim. All three kinds
share the same bounded journal; layouts 1 and 2 remain readable.

A changed shader prefix was observed before packet consumption with the same
sampled runtime pool offset at both ends. Imported memcpy/memset probes have no
overlap records, so the next CPU companion also observes the distinct
`runtime_memory_write_backing` path. The pinned upstream file remains unchanged;
its hash-checked private preparation adds optional before/after callbacks outside
that function's region lock. Allocation, write result, partial-failure behavior
and queue semantics remain unchanged. The callback preserves errno.

Only overlapping currently registered eight-byte shader prefixes are sampled.
The dedicated `shader-backing-events.bin` has two bounded 512-entry tails and a
132 KiB mapping. Records retain source/destination/length, prefix or explicit
unavailability, packet/capture registration, thread/time, guest-site and native
return-address candidates. The after record preserves actual success/failure;
a failed operation may have written a partial range. Cumulative before/after
totals survive finite history overwrite. A conservative page filter and bounded
bucket lookup reduce small-write scanning.

Separate observations can race other stores or registration replacement. This
does not observe direct guest stores, every host copy path, frees/remaps, aliases
or GPU execution; no exclusive writer or surviving allocation is implied. Offline
numeric-overlap candidates report whether their registration matches the retained
pre-enqueue context, without assuming it must do so across address reuse.

After adaptive round 9 exposed the remaining route gap, writable shader buffers
are observed in both the classic `ObtainBuffer` and paged `LayerPagedRecord`
branches. Round 13 then ruled out overlaps in the finite retained buffer window
while preserving a 260 ms prefix change, exposing writable storage images as a
separate guest-addressable descriptor route. Layout 8 reports classic, paged and
`storage_image` counters and labels. The image hook uses the already constructed
descriptor's guest address and size before binding; it does not change the image,
descriptor, barrier, invalidation or GPU ordering. Special buffers without a guest
address/range remain explicitly outside numeric-overlap coverage.

Adaptive round 14 exercised 92,435 `storage_image` bindings but ended on a
separately preserved SIGSEGV before a shader-metadata rejection. Review then
identified color and depth render targets as another guest-addressable GPU-write
route outside the bridge. Layout 9 adds a `render_target` route using the cached
image's guest address and size when render state is prepared. It records the
fragment-program hash when one is available, and uses zero explicitly when the
pipeline has no color target. The same fixed 4,096-entry history now accepts all
four routes, including targets bound before prefix registration. The hook does
not change target selection, image contents, barriers, permissions or draw order.
As with other bindings, it records write intent and cannot prove that a draw ran
or that the GPU stored into the overlapping bytes.

Source review after round 15 identified a narrower alias gap in storage-image
coverage. `FindImage` can resolve a requested descriptor to a cached larger image
or subresource whose actual `Image::info` guest range differs from the requested
`ImageDesc::info` range. The earlier hook recorded only the requested range, so a
shader prefix covered by the resolved backing but not by that descriptor could be
missed. Layout 10 adds `storage_image_resolved` and emits it only when the two
ranges differ, after rebind resolution and before view lookup. It records the
actual resolved guest address/extent while retaining the requested extent in the
declared-size field. It does not change overlap resolution, image/view selection,
descriptors, barriers, invalidation, guest bytes or GPU ordering. A numeric match
would still be write intent rather than proof of an executed store, allocation
identity or original writer. Addressless resources and direct guest stores remain
outside this probe.

CPU-only controls check real backing writes, successful and failed results,
callbacks outside the memory lock with query reentry, preserved errno/bytes,
disable behavior, watched partial overlaps and protected pages. They also require
a render target recorded before prefix registration to survive as a labelled
retrospective match, distinguish the fifth route, preserve layouts 1 through 9,
and migrate the exact prior renderer overlay without reapplying earlier patches.
Native coverage awaits an owner-started adaptive controller with the rebuilt CPU
executable and renderer.
