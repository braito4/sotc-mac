# Upstream review: 2026-10-22

## Current dependency integration

At the owner's explicit request, the runtime is advanced from 3170535f to
[b2ff9cd0b982](https://github.com/bloodborne-native/bloodborne-runtime/commit/b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548),
CPU from f749c155 to b9652d75 and the SotC reference from 2250f328 to
411fdd86/v0.60.5. The last public default-head refresh found b2ff9cd0 and app head
ab1f0c57 after the first integration build; that additional runtime fix is included.
[Dependency versions](DEPENDENCIES.md) and its lock identify the complete current
set, exact SHA, selected stable channel and available releases/development heads.
Open PerVertex proposals and the alternative KosmicKrisp architecture are tracked
separately from the integrated stable libraries.

The consolidated overlay is checked against b2ff9cd0. Earlier tessellation and
Metal-lifetime backports are now part of the base. Local ARM64/SRT/Phi, shader
compatibility, copy batching, audio/staging, failure diagnostics and deferred
scene-proxy ownership remain. Cache advances to **24/9** for the updated shader
toolchain/interfaces; no withdrawn bound/metadata experiment returns.

Ajnc's c9be8142 completed-coroutine correction is now adapted into the Mac
renderer at three indirect-buffer sites and two analogous self-test sites.
The production promise/loop CPU fixture reproduces 500 outstanding child frames
before versus zero after, and checks that suspended children survive. This is
synthetic ownership evidence, not the reference author's heap measurement or an
M1 Metal result. PR #219 remains open/non-draft/unmerged at its recorded head;
the incorporated commit and our adaptation are distinct from a GitHub PR merge.

The newest upstream b2ff9cd0 guard retains GPU-modified images whose contents are
not safely downloadable and which have no actual CpuDirty overwrite. MaybeCpuDirty
can originate from a neighboring tracked-page write; evicting that image discarded
its only valid GPU contents. The actual old/current/adapted predicates are covered
on the CPU, including normal pressure eviction and tiled-download retention.
The recorder now labels this reason separately. This may increase retained
memory and is not a demonstrated fix for our out-of-memory failure.

The new library/dependency cohort builds as ARM64 with freshly generated shaders.
Native CPU checks and full contracts pass; the handoff records final renderer
regressions and symbol/audit results. New game/GPU validation remains pending at
640x360, DMA/async off. The last measured OOM predates this integration.
The sections below retain the earlier reviews and their historical boundaries.

## Reduced-scene proxy retirement

Fresh public API and raw-source review on **2026-10-22** finds a directly relevant
cleanup in bmy's current default branch `macos-native-0.6`, head
`a69f56af2069df45357e86c27396baa945b53231`:
[`gpu/upscale/vk_scene_resolution.cpp`](https://github.com/bmy/bbport-mac/blob/a69f56af2069df45357e86c27396baa945b53231/gpu/upscale/vk_scene_resolution.cpp).
Comparing exact parent 72e5c8ce to
[8185b88d8057](https://github.com/bmy/bbport-mac/commit/8185b88d80573fa300595d5c54450183b4109935)
on **2026-10-19 at 06:30:55Z** identifies the addition of
`SceneTargets::CleanupStaleEntries`. It scans original ImageId/UID lookup, calls
`scheduler.Finish`, erases stale entries and clears recent pointers. The same
commit adds a temporal-upscaler call every 60 observed frames; the source remains
on the current branch. This is committed default-branch code, not an unmerged PR.
The current bloodborne runtime main
`b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548` still lacks this cleanup.

Only that lifetime problem is adapted here through a checked local overlay,
preserving bbport/shadPS4/bmy GPL credits. Maintenance runs in our already-drained
`OnSubmit`, without requiring temporal upscaling or a GPU-wide wait. Stale entries
move into a callback owning images and views through actual past-next completion.
Tracked UIDs and recent pointers are removed and scene generation is advanced to
invalidate texture-set views. Live originals, including still-pending native
deletion, keep their proxies. Source pins, cache 23/9, 640x360, DMA/async off,
large staging and other memory policies remain unchanged. New role metadata makes
the hypothesis testable. This is an original adaptation, not an unchanged cherry-
pick or import of bmy's large 0.6 commit.

The production-method CPU comparison reproduces 204 retained proxies before and
two active/202 destroyed after correction, with UID/mip, cache and real-tick
ownership checks. ARM64 incremental compilation and all current CPU/source
checks pass; our game and M1 have not validated this change. bmy's source comment
describes VRAM-leak prevention, while its flicker investigation says this cleanup
did not trigger in those particular runs. Neither is a measured SotC/M1 result.
No claim is made that all 981 untagged allocations in our preceding log have this
role or that their full 806.35 MiB will be reclaimed.

Relevant current heads/releases and PRs were refreshed, with full PR file lists:
MoltenVK #2858 remains open/draft/unmerged at
`6ed66f1b5d3575eed04b04e6332b389f398f9533`; SPIRV-Cross #2699 remains
open/draft/unmerged at `81739edf516c2bef043ac61f510b4bd78e126a22`, and #2697
open/non-draft/unmerged at `aa7879476d018098bd2c1279893409ba61cf3f03`.
There are no changed heads relative to the preceding review. Ajnc's v0.60.5 and
coroutine-fix/PR distinction below remain current. The independent CPU coroutine
backport stays deferred so the next native comparison isolates proxy retirement.

## SotC reference v0.60.5 comparison

This comparison predates the integration above. Public metadata/source were
checked on **2026-10-22** while the reference pin was
`2250f328feafab953aac85e758e7bddb2e80e891`. Its default branch was
`main`, observed at `411fdd86dadce347dcc4443228b009ae9265a548`, **72 commits ahead**.
[Release v0.60.5](https://github.com/AjncSystem/ShadowOfTheColossus-PC/releases/tag/v0.60.5)
was published at 01:09:58Z on 22 October, targeting
`43a619f8251b68761c082f7df5ca5acf69877883`; one later documentation commit is on main.
The complete update was not integrated at that checkpoint. The reference pin is
now updated as described above; the Mac renderer remains the reviewed bloodborne
runtime with local adaptations, rather than the Windows reference renderer.

The particularly relevant
[c9be8142ac76](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/c9be8142ac7675ccc553a0445273202fc5a96981)
adds `task.handle.destroy()` after each completed nested CE, graphics and compute
command-buffer coroutine. The diff is present on main; the corresponding
[PR #219](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/219) is still
**open, non-draft and unmerged** in the public API, head
`e391e03e956808abcb178a9e714df06ad871be78`. Do not confuse the incorporated commit
with a GitHub merge of the PR. Before integration our renderer lacked those three
destructions and its `Task` had no owning destructor. The correction is now
included through the separate checked adaptation/regression described above;
it is not part of the purely observational memory recorder.

The commit author's comparison reports process-heap growth of approximately
756 to 1706 MiB over fifteen minutes, then approximately 820 MiB after the fix.
Those are author measurements of CPU coroutine memory in the reference project,
not this port's Apple M1 results. Such growth can affect system pressure but does
not identify or remove our early 3 GiB GPU staging allocation, explain later
tracked image growth, or demonstrate a fix for Metal command error 8. Keep the
new resource diagnostics isolated from behavioral backports for the next native
comparison. The release also advertises scheduling and texture-quality changes;
those require individual source/ABI review before adoption, not a bulk pin bump.

The new [PerVertexKHR investigation](PERVERTEX_REVIEW.md) records MoltenVK #2858,
SPIRV-Cross #2699, relevant overhead commits and the isolated synthetic compiler
experiment. [Dependency watch](DEPENDENCY_WATCH.md) records the requested
bmy/bbport-mac review, current branch and continuing review sources. Its applicable
copy-region recording fix is adapted with checked digests and an actual-source
CPU regression. Native memory exhaustion and correct scene rendering remain
unresolved. The historical upstream comparison below is retained with its dated
observations.

The requested [bloodborne-mac history](https://github.com/bloodborne-native/bloodborne-mac/commits/main/)
contains newer release notes and runtime bumps than our original review. The app
repository primarily packages runtime/launcher revisions; renderer fixes are in
[bloodborne-runtime](https://github.com/bloodborne-native/bloodborne-runtime).
This review fetched current commit metadata and file patches through GitHub's API,
then compared the affected files with our pinned base. It does not infer suitability
from commit titles alone.

Observed heads during review were `17d3c4cdb0e7` for the app and `22923f080229`
for runtime. These are dated observations, not automatic update targets. Our runtime,
CPU and reference pins remain unchanged.

## Scoped shader compatibility and POSIX yield (2026-10-21)

The pinned runtime already has a PerVertexKHR rewrite in its native Metal compiler,
but MoltenVK compiles the original module first. The local policy reuses only the
existing exact vertex-zero/Flat case before new and cached module creation, and
only on devices with fixed first-vertex provoking behavior. Configurable provoking
vertex support and other vertex selections preserve the original module. No
derivative reconstruction, dropped vertex selection or replacement shader is
introduced. SPIRV-Cross's general per-vertex limitation remains.

The pinned wave64 lowering pass runs after shared-memory simplification. Its
ballot emulation introduced an additional U64 scratch view on devices without
explicit workgroup aliasing. The local correction loads the aligned low/high U32
pair and packs the same mask, preserving allocation and barriers. The original
pass reproduces undeclared shared-U32 MSL references in nine synthetic modules;
the adapted pass converts all nine and checks offsets for every invocation.
Shader binary version 22 invalidates the old output. These are original local
corrections on the pinned bloodborne-native/bbport/shadPS4 sources; source pins
are unchanged. The existing Metal rewriter retains its Wooloomooloo2 and Huax-1839
credits. Native driver compilation, pixels and gameplay remain unverified.

The newest native import stop is POSIX `sched_yield`, distinct from the existing
`scePthreadYield` export. The local scoped adapter calls the actual host scheduler
and preserves POSIX return/errno semantics. Full guest ABI contracts pass in both
CPU modes. No game execution was launched to validate these changes.

## Selected backports

| Upstream change | Relevance and implementation |
|---|---|
| [ef0080bb4331](https://github.com/bloodborne-native/bloodborne-runtime/commit/ef0080bb433152b476ab27b398d53eb23e8e934f) | Deduplicates auxiliary rect/quad tessellation input/output locations when several fragment inputs consume the same vertex parameter. Directly matches our MoltenVK duplicated `locn0` error. Applied unchanged. |
| [0e940e96cb08](https://github.com/bloodborne-native/bloodborne-runtime/commit/0e940e96cb08e06e0ea588bd41800dd62b00c659) | Invalidates cached shader binaries generated before location deduplication. Adapted to move our base's version 18 to 21; upstream's immediate predecessor was version 20. This does not import intervening aspect-ratio changes. |
| [e0348c24d911](https://github.com/bloodborne-native/bloodborne-runtime/commit/e0348c24d9117af5b22de2caa09d17a5e6a3dd07) | Retains shader module/compiled forms and graphics entries through deferred Metal draws and Vulkan handle reuse. Applied unchanged as a renderer lifetime correction. Our queue-overflow log does not prove that this separate lifetime fault occurred locally. |

The three patches above are GPL-2.0-or-later upstream work, not a new renderer authored here.
Patch files and input/output digests live in `tools/gpu_patches`. The build accepts
only a pristine pinned private checkout, the exact previous three-backport state,
the preceding submission-journal overlay, or the complete reviewed set with the
local pipeline-validity correction;
unknown modifications and partial applications are rejected. Digests verify source
identity; they are not proof of runtime correctness. `runtime/upstream` stays unchanged.

The isolated builder now applies the backports and records their full commit IDs in
`work/gpu-build.json`. Re-running it verifies/reuses the same patch set. Updating a
backport requires reviewing its source, manifest, tests and provenance together;
never relax the digest checks merely to get a build through.

The separate `sotc-submission-trace` overlay is local diagnostic code, not another
upstream fix. It uses the repository-owned CPU journal to connect recorded shader/API
notes and semaphore waits to a logical submission, with Metal chunk callback
observations. It does not use the unavailable AMD marker extension or change shader,
resource, fence or queue-order semantics. Bounded metadata is dumped before the
watchdog prefix, preserving the existing supervisor stop. The native ARM64 build,
CPU fixtures and exact previous-set migration pass; real GPU identification remains
limited to the recorded candidates described in the
[native capture](DEVELOPMENT_HANDOFF.md#black-scene-and-native-submission-evidence).

The later local `sotc-pipeline-validity` overlay corrects two source defects:
ordinary graphics-cache hits returned rejected objects with empty handles, and
game Metal registration accepted that null lookup key. Both paths now reject
empty handles consistently with first creation and prepared draws. Actual-source
CPU guard fixtures reproduce the original behavior and pass the correction;
native ARM64 compilation and the auxiliary shader regression pass. This is not
PerVertexKHR support or a proven black-scene/GPU-stall fix. The shader-cache state,
renderer settings and dependency pins are unchanged.

## What was validated

The native ARM64 GPU library rebuild succeeds. The actual auxiliary SPIR-V emitter
is tested with 15 synthetic modules across all three auxiliary shader kinds, including
repeated and sparse locations, default/flat inputs, all 32 inputs and an empty
interface. Both sides of each interface contain the expected unique locations.
The same regression reproduces duplicate location zero in the original pinned emitter.
This is a shader-generation regression, not a Metal device or rendered-pixel test.

Run it after building without game input or a GPU device:

```sh
python3 tools/check_tessellation.py
```

Python tests verify exact backport output, repeat application and rejection of
unknown/partial source. Host symbol coverage remains complete after the library
rebuild. The lifetime change compiles but still needs real renderer exercise; no
new visible frame is claimed. Both host runtime contracts and this GPU regression
are needed because they validate different components.

## Changes not imported

The newer runtime also contains Bloodborne-specific shadow-context/game hooks,
aspect-ratio/input features, launcher/release work, MetalFX and additional pipeline
cache guards. We have not adopted the entire main branch or its CPU updates.
Bloodborne-specific object layouts must not be applied to SotC.

No fix for SotC's SRT Phi offset failures, oversized descriptor clamps or incompatible
BC1/BC5 image usage has been established by this review. Native presentation and
correct pixels remain unverified. Keep these as separate work items in the
[development handoff](DEVELOPMENT_HANDOFF.md).

The next combined Terminal experiment must use both the rebuilt GPU library and the
updated host notification queue. It should first establish whether the queue stop
and duplicate-location pipeline error disappear, then record the first remaining
failure and any actual visible output. Preserve existing isolated cache/configuration
behavior and read-only game input.

## Follow-up Terminal observation

The next combined run no longer reports the queue overflow or duplicate auxiliary
location error, and reaches an unsupported EXTRQ CPU instruction. A new PerVertexKHR
MSL interface error skips some pipelines. The CPU instruction family is handled by a
separate local extension; it is not part of these renderer backports. SRT, compressed
texture usage and visible-frame verification remain open. See the current
[handoff](DEVELOPMENT_HANDOFF.md) for the next reproduction step.

## Integer widening review after the VPMOVZXWQ stop

The preferred reference was followed through its runtime/recomp split. Observed
heads during this follow-up were app `6b23bd27663e6a36338433abb9f03cde60b14fa8`,
runtime `22923f08022909b7e22f7ae8c02ad4c3f6554bfd` and recomp
`b9652d75edc0a281f06cec0da6ff72e9f5150a70`.
The [reviewed interpreter](https://github.com/bloodborne-native/bloodborne-recomp/blob/b9652d75edc0a281f06cec0da6ff72e9f5150a70/src/cpu/interp_vec.c)
and [ARM64 JIT](https://github.com/bloodborne-native/bloodborne-recomp/blob/b9652d75edc0a281f06cec0da6ff72e9f5150a70/src/cpu/jit_arm64.c)
implement eight widening pairs, but do not contain cases for signed/unsigned
byte/word-to-qword extension, including VPMOVZXWQ. No ready implementation was found
in those reviewed files; this is not a claim about every upstream branch or PR.

The SotC vector adapter therefore completes the eight missing SSE/VEX mnemonic
variants locally, preserving the pinned implementation for other pairs. The source
pins and JIT code are unchanged. `tests/test_cpu_widen.c` checks the entire widening
family, including existing native JIT paths, across 64,512 synthetic cases per mode.
Exact-width guard-page reads, sign/zero fill, aliasing and upper-register behavior
are tested without game input or native x86 execution. The next Terminal run must
establish continuation beyond the reported instruction.


## SDL3 gamepad adapter review (2026-10-21)

Reviewed `src/runtime_pad.c` at the pinned runtime and at preferred upstream runtime
[`22923f08022909b7e22f7ae8c02ad4c3f6554bfd`](https://github.com/bloodborne-native/bloodborne-runtime/blob/22923f08022909b7e22f7ae8c02ad4c3f6554bfd/src/runtime_pad.c).
Both use SDL gamepad enumeration/open/connection checks and free the device list.
The local host adapter follows that lifecycle and the DS4 button mapping, while
keeping SDL initialization on the renderer main thread, explicit output errors,
existing guest-handle validation and keyboard fallback. No game hooks, replay,
heap offsets, calibration or dependency updates were imported.

SDL3 callback signatures, button/axis enum values and thread-safety were checked
against the installed pinned SDL3 headers. See the primary
[SDL3 gamepad API](https://wiki.libsdl.org/SDL3/CategoryGamepad).
The vibration byte order is large motor then small motor, consistent with the
[shadPS4 pad layout](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/pad/pad.h)
and the preferred runtime's low/high-frequency forwarding.

`--contracts-test` passes with synthetic keyboard and physical-gamepad callback
providers in both CPU modes, including all 65,536 signed axis inputs. The installed
SDL library exposes all required and optional callbacks; a native headless SDL
initialization/enumeration check found no connected gamepad. Physical USB/Bluetooth
input, rumble and LED behavior are not yet verified.


## Matching public error reports and mount failure batch (2026-10-21)

| Local symptom | Primary GitHub evidence | Applicability |
|---|---|---|
| SRT `ComputeOffset` rejects `Phi` | [SotC issue 41](https://github.com/AjncSystem/ShadowOfTheColossus-PC/issues/41) reports the same error during transition from menus/cinematic to asset loading | Same message and game; different Windows/Vega hardware. The reported wave32 improvement targets AMD GCN, not Apple M1, and does not establish that Phi is repaired. |
| PerVertexKHR-to-MSL conversion failure | [MoltenVK issue 2258](https://github.com/KhronosGroup/MoltenVK/issues/2258) and [2059](https://github.com/KhronosGroup/MoltenVK/issues/2059) | Exact converter error. An advertised barycentric feature does not prove support for raw per-vertex input arrays. Our isolated fragment diagnostic reproduces the installed compiler boundary. |
| Proposed Metal 4 per-vertex support | [SPIRV-Cross issue 2569](https://github.com/KhronosGroup/SPIRV-Cross/issues/2569) | Closed as not planned on 2026-10-16, not evidence of a merged fix. Metal language capability alone does not validate this installed compiler or the M1 path. |
| Resource-walker runtime reads | [Pink-shadPS4 commit 33da14d5a500860f3b87bbd89796a18c84d87c52](https://github.com/luizgustavs/Pink-shadPS4/commit/33da14d5a500860f3b87bbd89796a18c84d87c52) recognizes runtime offsets | Candidate for further review. Its Xbyak walker differs from our ARM64 portable walker; skipping compile-time reads requires working dynamic shader reads and cannot be copied as a log suppression. |

The local SaveData stop is an unresolved import, not a returned mount error. The
preferred runtime at `22923f08022909b7e22f7ae8c02ad4c3f6554bfd` implements host-directory
persistence; that implementation was reviewed but not enabled in this read-only
content diagnostic. Its mount wrappers check a null parameter before reaching the
initialization check, whereas the supplied retail exports check the context first.
The local adapter follows the measured retail failure path.

Eight related calls now return NOT_INITIALIZED: Mount, Mount2, Umount,
UmountWithBackup, GetMountInfo, SetParam, SaveIcon and Delete. Inputs and output
buffers remain untouched, including null/protected pointers and full-width sizes.
No initializer succeeds and no save storage, mount point, backup or metadata is
created. The original-module comparison executes only uninitialized error paths;
its private harness/binary remain outside Git. Full contracts and the original
comparison pass with both JIT and interpreter. The synthetic real-loader fixture
also resolves and calls all eight exports before its deliberate unknown-import stop.
Native Terminal continuation after this change remains to be observed.

The owner separately reports seeing logos and the menu. This is direct visual
evidence of game video, while in-game scene rendering and playability are pending.


## Save-dialog startup continuation (2026-10-21)

The next native run passes SaveData Mount2's explicit NOT_INITIALIZED result and
stops at SaveDataDialog Initialize. The preferred runtime's `runtime_services.c`
at [`22923f08022909b7e22f7ae8c02ad4c3f6554bfd`](https://github.com/bloodborne-native/bloodborne-runtime/blob/22923f08022909b7e22f7ae8c02ad4c3f6554bfd/src/runtime_services.c)
was reviewed against our pin. Its immediate dialog-completion behavior was not
adopted: this diagnostic has no CommonDialog session or save dialog UI.

Initialize has a zero-argument ABI and returns NOT_SYSTEM_INITIALIZED under the
same missing-dependency policy as MsgDialog. It never creates a dialog object. A
private fixture checks that the supplied original Initialize wrapper propagates
an injected NOT_SYSTEM_INITIALIZED dependency error, including object cleanup.
This checks error propagation, not successful original platform initialization.

Open has a one-pointer ABI. Comparisons use both the supplied original Open export
and the original CommonDialog base-parameter validator. The observed order is null
pointer, 48-byte base header (64-bit size, address-dependent cookie, 36 reserved zero
bytes), signed 32-bit outer size of 152, then the absent-instance error. Nested mode,
user and save pointers remain opaque on this failure path. The local adapter also
safely rejects unreadable pointers and missing size bytes; these guards deliberately
replace original memory faults rather than claim identical malformed-pointer behavior.

Full contracts and original-module comparisons pass in JIT and interpreter modes.
Synthetic loader coverage calls both startup exports and reaches its deliberate
unknown-import stop. Existing status/result/close/progress/termination checks still
pass after failed startup: no initialized/completed dialog or user selection is
fabricated. No GPU rebuild is required. Native game continuation remains pending.


## Guarded shader reads and extended observation (2026-10-21)

**The runtime experiment described in this section was subsequently withdrawn.
Real GPU testing is paused; see the host incident below.**

The installed preferred runtime defaults `BB_DIRECT_MEMORY_ACCESS` to false. Its
SRT walker leaves ReadConst offset zero when it cannot evaluate a Phi/lane-derived
address on the CPU. Shader-info collection marks those reads as dynamic, but clears
the dynamic flags/resources when direct memory access is disabled. The SPIR-V emitter
then uses a flat-buffer load rather than its guarded `read_const_dynamic` function.

The combined SotC host now defaults that existing setting to 1 before dlopen, while
preserving an explicit user override. No dependency or GPU source is modified. Four
synthetic IR cases run through the installed SRT and shader-info passes with DMA
both off and on: uniform values are preserved, and dynamic flags plus page-table/
fault-buffer resources appear only when needed and enabled. Host callbacks are
abort-only fixtures; no renderer or GPU is initialized. This validates resource
selection, not shader execution or game-image correctness. Phi/lane log messages,
PerVertexKHR conversion and memory mapping remain open investigations.

The combined diagnostic now defaults to 300 seconds instead of 30; explicitly
configured budgets accept 1..1800 instead of 1..60. Headless defaults remain 30 and
renderer-only initialization remains capped at 60. CLI tests reject malformed,
zero and excessive values, and one-second synthetic infinite loops still terminate
in both CPU modes. Loader tests verify the 300-second default, 1800-second override,
and preservation of explicit DMA off/on settings without waiting for those budgets.

## Host watchdog incident: DMA experiment withdrawn (2026-10-21)

The owner reported a host kernel watchdog panic. The newest private log selects
DMA but ends during renderer initialization before guest entry. The preceding run
already logs an unfinished GPU submission with DMA at its former default. This
does not establish which path caused the panic. The offline IR fixture above
cannot validate hardware execution or system stability.

The host again selects the pinned renderer's default-off DMA setting, and rejects
nonzero or malformed explicit overrides before loading the library or running
launch prechecks. The combined default budget is 120 seconds at the owner's request;
accepted explicit budgets remain 1..1800. No upstream source, dependency pin or shader
semantics changed in this rollback. GPU tests are paused; only CPU/synthetic
contracts and read-only source/log review are used during containment. A user-space
timeout cannot guarantee recovery from a kernel/driver hang.
