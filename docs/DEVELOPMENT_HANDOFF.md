# Development and debugging handoff

Snapshot: 2026-10-21. This is the starting point for the next developer. Read
[AGENTS.md](../AGENTS.md), this page and [Build instructions](guide/building.md)
before changing code. [Status](STATUS.md) and [Graphics bring-up](GRAPHICS_BRINGUP.md)
contain chronological investigations; their earlier “latest run” sections describe
older boundaries, not the present state.

## Goal and current evidence

The goal is native Apple Silicon execution of the PS4 game through bbcpu and the
pinned renderer, ultimately producing correct game video. There is no playable app,
verified game frame, save backend or gameplay FPS result yet.

Supported input is the owner's readable CUSA08809 update 1.01 dump. ARM64 macOS on
Apple M1 is the observed host; the isolated GPU build targets macOS 15 or later.
Other game editions and hardware are unverified.

The last native Terminal run completes initialization at about 0.92 seconds and
enters the main loop at about 1.37 seconds. It passes the vector widening batch and
stops at `sceSaveDataMount2` on the Save Task Thread after a dirty-profile save request.
No unsupported CPU instruction, notification overflow or duplicate `locn0` appears.
GPU samples include thousands of draws and dispatches per second; these are command
counts, not gameplay FPS or verification of correct pixels. The pipeline archive
reports 209 pipelines. Correct game video remains unverified.

The renderer still skips pipelines because of PerVertexKHR-to-MSL conversion (70
reported errors in the last run), and has SRT Phi and compressed-texture usage errors.
The isolated fragment diagnostic reproduces the compiler limitation without game data
or a GPU. IT_GET_LOD_STATS also remains unimplemented.

Physical SDL3 gamepad input now complements the keyboard fallback. See
[DualShock input](guide/building.md#dualshock-input) for connection and logging.
Guest ABI tests cover the entire stick/trigger domain, buttons, two touch identities,
hotplug, output errors and synthetic callback dispatch in both CPU modes. Hardware input,
Bluetooth/USB output and game behavior still need a connected-controller Terminal run.
No GPU rebuild is needed for this host adapter change.

## First session: reproduce the boundary

Run commands from the repository root in a native macOS Terminal. Quote absolute
paths containing spaces. Keep stdout/stderr and generated reports private.

```sh
uname -m
# Must print arm64.
git status --short
git submodule status --recursive
bash packaging/test-arm64.sh --contracts-test
```

On an existing machine with prepared input and GPU dependencies:

```sh
bash packaging/build-gpu-arm64.sh
SOTC_MEMORY_BACKING=file SOTC_DIAGNOSTIC_SECONDS=60 BB_JIT=1 \
  bash packaging/test-arm64.sh --graphics-game-test
```

Read the newest `logs/graphics-game-*.log` by modification time, not an old filename
from chat. The script prints the log path. Run the same command with `BB_JIT=0` to
compare the interpreter, allowing for its slower progress within the same budget.
`SOTC_DIAGNOSTIC_SECONDS` accepts only 1..60. A timeout is an observation boundary,
not successful gameplay or evidence that all imports are implemented.

The combined command rebuilds the host diagnostic, prepares a separate linked image,
checks Metal access and host symbol coverage, and launches the renderer. SDL/window
initialization stays on the host main thread; guest execution runs on a worker.
Each run has an isolated GPU configuration/cache directory under `work`.

## Reconstructing the environment

The full setup lives in [Building](guide/building.md) and [Game files](guide/game-files.md).
The main entry points are:

| Command | Purpose and limit |
|---|---|
| `bash packaging/test-arm64.sh --fetch` | Fetch exact pinned sources; do not run over non-Git snapshot directories |
| `bash packaging/test-arm64.sh --test` | Build CPU dependencies/archive and run native emitter, JIT and vector checks |
| `bash packaging/test-arm64.sh --contracts-test` | Synthetic runtime/ABI tests in both CPU modes; no game or real GPU required |
| `bash packaging/test-arm64.sh --prepare` | Prepare the owner's original executable privately |
| `bash packaging/test-arm64.sh --content-check` | Audit supplied archive manifests/extents; does not establish asset decoding |
| `bash packaging/test-arm64.sh --linked-test` | Paired headless initialization comparison; intentionally stops at unsupported imports |
| `bash packaging/build-gpu-arm64.sh` | Build/resume the pinned isolated renderer and dependencies |
| `bash packaging/test-arm64.sh --graphics-check` | Synthetic offscreen Metal draw/readback; no game frame |
| `bash packaging/test-arm64.sh --vulkan-check` | Check Vulkan surface extension discovery |
| `python3 tools/check_gpu_link.py` | Inspect actual host/library symbol coverage; does not validate ABI or load the library |
| `bash packaging/test-arm64.sh --gpu-load-check` | Load the library without renderer initialization |
| `bash packaging/test-arm64.sh --renderer-check` | Initialize the renderer and diagnostic window without guest commands |
| `bash packaging/test-arm64.sh --graphics-game-test` | Bounded combined experiment; inspect the result rather than assuming success |

Important environment variables:

| Variable | Meaning |
|---|---|
| `BB_DEPS` | Optional compatible ARM64 static Zydis/Zycore prefix; default `work/deps-arm64` |
| `SOTC_GAME_ROOT` | Owner's readable game directory containing `eboot.bin` and `sce_module` |
| `SOTC_SYSTEM_MODULE_ROOT` | Original gesture/NGS2 modules; default sibling `sys_modules` directory |
| `SOTC_MEMORY_BACKING=file` | Unlinked temporary-file backing with shared aliases when POSIX shm is unavailable |
| `BB_JIT=1` / `BB_JIT=0` | ARM64 translation / interpreter |
| `SOTC_DIAGNOSTIC_SECONDS=60` | Maximum supported bounded game experiment |
| `SOTC_GPU_JOBS` | Isolated build parallelism, 1..8 |

The launch script explicitly selects its bundled Vulkan loader, MoltenVK ICD and
SDL Vulkan library. Preserve this: the Metal path still depends on Vulkan WSI.
`VK_KHR_surface` failures can come from selecting a different system loader/ICD.
The builder repairs generated FFmpeg/pkg-config metadata for prefixes with spaces.
Do not move the repository merely to conceal a quoting regression.

A restricted automation process can compile and run CPU contracts while exposing
no Metal device. Use native Terminal for real renderer experiments. A failed device
precheck in that process is not evidence that the user's GPU is unsupported.

## Code ownership and execution path

| Area | Files and responsibility |
|---|---|
| Build/test orchestration | `packaging/test-arm64.sh`; compiler flags, source lists, isolated modes, log capture |
| Input preparation | `tools/prepare_game.py`, `tools/prepare_linked_arm64.py`; private BBPROBE images and original module/TLS metadata |
| Host entry and stops | `runtime/src/probe_entry_arm64.c`; image mapping, relocations, initializer order, watchdog, import/fault reporting |
| Import policy | `runtime/src/bootstrap_contracts.c`; scoped namespace mappings and capability gates |
| CPU context and vector support | `bootstrap_cpu_context.c`, `bootstrap_cpu_vector.c`; pinned bbcpu adapters and explicit extensions |
| Threads/TLS/fibers | `bootstrap_threads.c`, `bootstrap_tls.c`, `bootstrap_keys.c`, `bootstrap_fibers.c` |
| Shared memory and address policy | `platform_arm64.c`, `runtime_memory_arm64.c`; real physical backing/aliases and low-address constraints |
| GPU notification storage | `tools/prepare_memory_support.py`, `runtime/src/memory_hook_queue.h`; checked private source adaptation with growable storage |
| Library/device binding | `runtime/src/bootstrap_gpu.c`; checked dlopen/dlsym, main-thread initialization and actual backend publication |
| Graphics import adapters | `runtime/src/bootstrap_graphics_routes.c`; reviewed encoders, live display validation, submit ABI and provider errors |
| Local services | Other `bootstrap_*.c` files; explicit narrow clocks/input/audio/service contracts |
| Renderer build | `tools/build_gpu.py`; pinned private source, checked upstream backports, dependencies and `work/gpu-build/libbbgpu.dylib` |
| Native GPU internals | Pinned `runtime/upstream/gpu` and its private build checkout; shaders, pipelines, buffers, textures, Metal/Vulkan |

Input preparation preserves original FS/TLS loads; do not reintroduce native x86 TLS
rewrites. The CPU executes supplied original libc/Fios2 and, in graphics mode,
gesture/NGS2 modules. Scoped host imports call reviewed adapters or an actual initialized
GPU provider. Unknown imports stop; missing services report explicit unavailability.

The host build copies the CPU archive and replaces two members with local context/
vector adaptations. Do not mutate the original archive. Mutex statistics and GPU
notification storage are transformed only in checked private build copies. The memory
preparation validates the complete pinned source digest and replaces just its storage
declaration and capacity guard. The source checkout remains unchanged.

Do not run two host build/test scripts concurrently: both regenerate the same private
archive and diagnostic sources in `work`.

## Prioritized debugging work

1. **Save mounting and physical input verification.** The current explicit stop is
   `sceSaveDataMount2`. SaveData initialization and directory searches already report
   unavailable/not initialized, so implement and test the corresponding mount failure
   contract before considering any persistence backend. No save access is authorized
   by this adapter. Test DualShock input in native Terminal with a connected controller;
   distinguish accepted SDL callbacks from confirmed physical output. The completed
   widening and SSE4a fixtures remain in the CPU regression suite.
2. **MSL fragment interface support.** The newest log has 70 `PerVertexKHR` conversion
   failures. `python3 tools/check_fragment_interface.py` isolates this with the actual
   installed SPIRV-Cross library: an ordinary input converts, raw per-vertex input
   reading vertex one fails. Exit 2 means the known limitation is reproduced; it does
   not mean shader support passed. Neither case initializes a GPU. The Vulkan device
   advertises fragment barycentrics, but this alone does not prove the selected MSL
   conversion path supports raw vertex arrays. Trace `V_INTERP_MOV_F32` and
   `Qualifier::PerVertex` through `vector_interpolation.cpp` and
   `spirv_emit_context.cpp`. Do not remove the decoration or substitute interpolated
   input without preserving the requested vertex selection. The earlier duplicate
   auxiliary location error is absent; not every tessellation path is verified.
3. **SRT resource tracking.** `flatten_extended_userdata_pass.cpp` fails offset
   computation on `Phi` and then fails its SRT walk. Investigate predecessor/value
   handling and resource provenance in the recompiler. Buffer-size clamps may relate
   to bad descriptors, but their causal link is not established. Do not treat clamps
   as a repair or replace unknown offsets with zero.
4. **Compressed texture usage.** BC1/BC5 format/usage errors request combinations
   including storage and extended views. Check capability detection and whether each
   image actually requires storage access; design a supported decode/view path if it
   does. Dropping usage flags without tracing consumers is not sufficient.
5. **Prove visible output.** Trace registered VideoOut buffers, actual submissions,
   completed GPU work and presentation, then inspect a real frame locally. Encoding
   commands, a window, accepted audio buffers or zero “unsupported commands” do not
   establish correct game pixels.

Renderer correctness remains open; distinguish the validated SPIR-V correction from
its pending visual verification. The float-denorm capability warning alone does not
identify the cause of the final stop. Do not silently toggle label/fence completion
policies to advance execution; completion must reflect actual GPU work.

## Resolving further imports in batches

For `STOP_AT_IMPORT`, retain the exact scoped name, thread, first argument and caller
context privately. Numeric import indices depend on the prepared image; do not use
them as API identity. Inspect the supplied module's symbol identity, pinned source
and original calling convention before binding a function. Review related calls as
a family to avoid one-import-per-run progress, but keep unknown contracts unresolved.

The original x86-64 guest passes its first six integer arguments in registers and
later arguments on its stack. The host ARM64 ABI differs. Public `bbcpu_call` takes
at most six arguments: for seven or more, construct a synthetic x86 thunk as in
`tests/test_graphics_submit.c` or `tests/test_graphics_draw.c`. Test 32-bit narrowing,
signed/full-width values, output layout, untouched failure buffers, namespace and
capability gates, errno and actual backend errors. Symbol presence alone is not ABI
compatibility.

Known compatibility repairs include compute packet header bits, indirect-draw stage
validation, colored-marker layout, submit pointer/size checks and flip-wait label
queries. Do not replace these adapters with raw native exports just because names
match. SaveData, trophy, AppContent and dialogs mostly expose explicit unavailable/
no-instance behavior; they are not working persistence/UI services.

Where useful, compare a synthetic fixture against the owner's original module in a
private local harness. Provide only the real dependencies or narrowly controlled test
inputs necessary for that comparison. Distinguish original-compatible valid behavior
from deliberate malformed-input safety guards. Publish the synthetic fixture and
findings, never the original binary, flattened module or generated guest code.

## Validation and shipping a change

For runtime changes, run the full contracts command. CPU context/vector changes also
require `--test`. For renderer changes, run the relevant synthetic regression and
rebuild the isolated GPU library before requesting a combined Terminal run. Preserve
these pins unless an explicit, reviewed dependency update is intended:

| Source | Revision |
|---|---|
| runtime | `3170535f59cadbe5ce9c38e6bb961ed5fd215194` |
| recomp | `f749c155c2ff3bb932795de85c9c8fe9c4aa59ad` |
| SotC reference | `2250f328feafab953aac85e758e7bddb2e80e891` |

The queue regression is `tests/test_memory_hooks.c`: provider replay and guest-ABI
release of 257 real aliases, 8,193 mixed FIFO events, callbacks outside the region
lock, reentry, reuse, count rollback, absent hooks and explicit allocation/capacity
failure. `tests/test_prepare_memory.py` checks the restricted transformation and
rejection of changed upstream input. The SSE4a fixture is `tests/test_cpu_sse4a.c`, run by both `--test` and
`--contracts-test`, including the documented QEMU-compatible truncation policy in all four forms.
The widening family is `tests/test_cpu_widen.c`, also run in both modes by those
commands. It exercises all existing width pairs as well as the new qword pairs.
The GPU-only auxiliary emitter regression is `python3 tools/check_tessellation.py`;
it needs the isolated build but neither a GPU device nor game data. Backport source
checks are part of `tests/test_gpu_build.py`. Other graphics regressions are split across
`test_graphics_{routes,color,shaders,submit,present,draw}.c`; synthetic dynamic-loader
coverage lives in `tests/test_graphics_entry.py`. RTC is `tests/test_rtc.c`.

Before committing, stage only intended public source/documentation and run:

```sh
git diff --cached --check
python3 tools/check_repository.py
git remote -v
```

The audit reads staged/tracked content. The Linux source-check CI is a partial static/
fixture gate; a green CI run does not replace native ARM64 contracts or GPU testing.
Record the exact command, CPU mode, build revision, first stop and verification limit
in the change description. Never promote game addresses, execution logs, generated
images or private paths into public documentation.

## Local state and handoff maintenance

`game/`, `work/` and `logs/` are ignored private state. They contain the owner's input,
generated executable images/module analysis, build products, caches and execution
logs. Fresh clones do not contain them. Recreate input from the owner's dump; do not
copy an unrelated developer's `work` directory or assume private oracle scripts ship
with this repository. Keep game content read-only and do not access saves.

Update this page when the observed stopping boundary or reproduction command changes.
Separate what was seen in Terminal, what synthetic tests establish, what is only a
hypothesis and what remains unverified. A successful next run should replace the
current SaveData mount boundary above with its actual result, including any timeout.

## Preferred upstream reference

The user explicitly selected [bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac/)
as the reference when resolving development doubts. Start there and follow its
[runtime](https://github.com/bloodborne-native/bloodborne-runtime) and
[recomp](https://github.com/bloodborne-native/bloodborne-recomp) sources for the actual
implementation. Check relevant current commits against our pins before adapting code.
Keep source provenance and tests, and reject Bloodborne-specific object/layout
assumptions for SotC. This preference does not authorize unreviewed dependency bumps.
When the reference lacks an implementation, document another primary source explicitly;
the SSE4a overrun policy here follows
[QEMU v10.1.0](https://github.com/qemu/qemu/blob/v10.1.0/target/i386/ops_sse.h).
