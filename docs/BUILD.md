# Building for Apple Silicon

Use a native ARM64 Terminal on macOS 15 or newer. Install Xcode Command Line
Tools (`xcode-select --install`) and Python 3, CMake, Ninja, pkg-config and glslang.
The build also uses the system Git, curl, make and Apple compiler/linker tools.
With Homebrew, the additional tools can be installed with:

```sh
brew install python cmake ninja pkg-config glslang
```

For the exact shader compiler used in the tested build (16.6.0):

```sh
mkdir -p work/host-tools
git clone --depth 1 --branch 16.6.0 https://github.com/KhronosGroup/glslang.git work/host-tools/glslang-source
cmake -S work/host-tools/glslang-source -B work/host-tools/glslang-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_INSTALL_PREFIX="$PWD/work/host-tools/prefix" -DENABLE_OPT=OFF -DENABLE_GLSLANG_BINARIES=ON -DBUILD_TESTING=OFF
cmake --build work/host-tools/glslang-build --parallel 2
cmake --install work/host-tools/glslang-build
bash build.sh
```

The builder fetches exact upstream revisions into `work/`, checks and applies
SotC's source overlays, builds the CPU and renderer, and verifies host imports.
It needs network access on the build Mac. Default parallelism is two jobs;
`SOTC_GPU_JOBS` accepts 1 through 8. Generated output stays outside Git.

## Game input

Supply your own decrypted CUSA08809 update 1.01 directory containing `eboot.bin`,
`sce_sys/param.sfo`, `sce_module/libc.prx`, `sce_module/libSceFios2.prx` and the
original game assets. Put `libSceSystemGesture.sprx` and `libSceNgs2.sprx` in a
sibling `sys_modules` directory, or set `SOTC_SYSTEM_MODULE_ROOT` to their directory.

```sh
bash run.sh /absolute/path/to/CUSA08809
```

Preparation checks the edition and creates a private linked image. The runtime
keeps its original FS/TLS semantics. This is a development build: artifacts and
low FPS remain, and saving is unavailable. Default gameplay runs stop after
180 seconds; `SOTC_RUN_SECONDS=0` disables that duration limit. Logs go to `logs/`.

## Portable private build

After building and preparing the game with `run.sh`, create one file:

```sh
python3 tools/package_portable.py --game /absolute/path/to/CUSA08809
```

The output is `work/portable/SotC-Mac.command`. It contains **your game data** and
the linked image, so keep it private. It is a shell self-extractor carrying native
ARM64 binaries, rather than one statically linked Mach-O image. All non-system
libraries are bundled, relocated and ad-hoc signed. No Homebrew, Python, compiler
or network access is needed on the destination Mac.

Copy that one file to a USB drive that supports files larger than 4 GB (exFAT or
APFS). On the other Mac, double-click it or run `bash /path/to/SotC-Mac.command`.
It verifies the payload and extracts once to
`~/Library/Application Support/SotC Native/`. Keep enough internal free space
for the unpacked game plus memory/swap needs. Each package uses its own directory.
To remove an installation, remove that package's directory while the game is closed.

`bash /path/to/SotC-Mac.command --verify` checks the archive without extracting or
launching. `--check` extracts and tests library loading without starting the game.
These checks do not validate GPU rendering. The binary targets macOS 15 or newer;
actual M5 gameplay needs an on-device test.

Gameplay logs are saved in `~/Library/Logs/SotC Native/`. The first run is limited
to 180 seconds. To run longer:

```sh
SOTC_RUN_SECONDS=0 bash /path/to/SotC-Mac.command
```

The package is not notarized. If macOS blocks it after transfer, use the specific
file's Open Anyway option in System Settings; do not disable Gatekeeper globally.

## Optional CPU texture helper

`SOTC_TEXTURE_HELPER=1 bash run.sh /absolute/path/to/CUSA08809` enables upstream's
extra texture-binding worker. The portable launcher accepts the same environment
variable. Default is 0. The October 29 M1 trial regressed to about 0.3 FPS from roughly
1–1.7 FPS with serial texture binding, with higher CPU use and helper waits.
Keep it disabled on M1; enabling more workers does not imply useful parallelism.

The portable packager also accepts `--compression none` to pack already-compressed
game assets faster, with a larger output file. Both formats remain self-extracting.

## Optional Metal timing

`SOTC_METAL_TIMING=1` enables five-second summaries from the existing owned-command
completion callback. It records GPU execution duration, precommit-to-GPU-start
delay (including driver/queue time), admission wait, callback lag, and commands
per completed buffer. It adds no GPU wait. Zero or inconsistent timestamps count
as invalid samples. The default is off.

Scope is owned command buffers passing through `MetalFrame::CommitCommandBuffer`,
not every Metal submission in the process. Averages are per valid completed buffer;
windows are grouped by callback arrival. They are not frame timings or GPU utilization.
The instrumentation itself may affect CPU scheduling. Apple timestamp reference:
https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpustarttime

## Isolated CPU layout experiment

The host now builds only the CPU archive; it does not build the recompiler tools.
The default source and output remain unchanged. To extract byte-identical pinned
CPU sources and ABI headers and build a separate host:

```sh
python3 tools/prepare_cpu_layout.py
SOTC_CPU_SOURCE="$PWD/work/cpu-layout-4d9c38444cac" \
SOTC_HOST_BUILD_DIR="$PWD/work/host-layout-20261103" bash tools/build_host.sh
```

The extraction verifies every copied source against the pinned Git commit and
writes a private hash manifest. Existing modified copies are rejected.
`SOTC_HOST_BUILD_DIR` isolates generated adapters, objects, archive and executable.
Keep the existing renderer for this comparison: it still includes the same CPU
ABI from the legacy source path. This does not yet enable the newer standalone
CPU or its AOT tools, and does not imply a performance gain.

## Standalone CPU candidate (opt-in)

The tested default remains unchanged. The standalone CPU candidate is pinned in
`tools/dependencies.json` independently of the runtime and recompiler. Prepare
the exact commit and compile to a separate directory:

```sh
python3 tools/prepare_standalone_cpu.py
SOTC_CPU_SOURCE="$PWD/work/bbcpu-source-3f9b6c1a11d5" \
SOTC_HOST_BUILD_DIR="$PWD/work/host-bbcpu-3f9b6c1a11d5" bash tools/build_host.sh
```

An existing local Git clone can be supplied with `--repository PATH`. Sources
are verified against the selected Git objects; modified copies are rejected.
The generated source manifest records each file's SHA-256. Scalar and JIT
adapters accept only reviewed hashes and still verify every replacement site.
No AOT library is generated or required. The existing renderer ABI is retained;
keep the control executable available until gameplay validation completes.

## Optional persistent savedata trial (M1)

The default host still reports savedata unavailable. To build the isolated trial:

```sh
python3 tools/prepare_cpu_layout.py
python3 tools/test_savedata_trial.py
SOTC_SAVE_TRIAL=1 \
SOTC_CPU_SOURCE="$PWD/work/cpu-layout-4d9c38444cac" \
SOTC_HOST_BUILD_DIR="$PWD/work/host-savedata-20261104" \
bash tools/build_host.sh
```

At launch, `SOTC_SAVE_ROOT` must name an existing absolute writable directory.
Without it, the trial host retains the unavailable-service behavior. Local M1
launcher `prueba_guardado.command` uses `work/saves-trial-20261104`, the unchanged
renderer, the old CPU control and GC11264. It is a private launcher using the
existing local game preparation, not part of a fresh public checkout.

The service supports user1/CUSA08809, mount/mount2, directory search, metadata,
icons, mount space information and unmount with backups. It serializes file and
mount operations, rejects traversal/symlinks and read-only writes, and rejects a
second process using the same save root. Writes use the pinned backend's temporary
file followed by replacement on close; an interrupted unclosed file preserves
its predecessor. This is per-file atomicity, not a transaction across every file
in a save or a guarantee against power loss. Backups are separate immutable
copies, recognized by their `complete` marker; they are not automatically pruned.

Space reports use the slot's persisted 32-KiB quota (minimum96 blocks), following
the SotC reference port; reporting the entire disk for each slot breaks SotC's
space calculation. Quota reporting is implemented; full virtual-quota enforcement
and reservation are not. Metadata write failures are returned as errors.

Still unsupported: save-dialog UI, save deletion, automatic backup restoration,
SaveDataMemory, importing existing console saves and arbitrary checkpoint selection.
The service is intentionally opt-in; a synthetic success does not establish that
SotC can save/load through its current UI. Look for `Save trial:` and `Save trace:`
messages in the game log. Keep this trial's saves separate from valuable originals.

Native synthetic validation passed: disabled fallback; write and reload in a
second process; interrupted replacement preserving old bytes; backup content;
read-only and invalid-path rejection; open-descriptor unmount rejection; empty
file creation; nested directories; positional writes/truncation; metadata/icon
handling; quota reporting; and concurrent-process exclusion. ARM64 trial host
compiled and the existing GPU library loaded successfully. Real gameplay save/load
remains pending.

## Exact read tracking experiment

On the pinned994846bb7bb5 renderer, `SOTC_EXACT_READS=1` selects exact buffer read
ranges instead of rounding them to4KiB. Default0 preserves the control. This
reuses the existing implementation; shader code is unchanged, but the toggle-file
hash gives this profile a separate cache identity. Its first run is cold. The toggle meaning must be rechecked when updating the runtime.

For the existing local M1 workspace, run `./prueba_lecturas_exactas.command`.
Use `./prueba_lecturas_exactas.command 0` for the matched control. Both use the
same verified host/renderer, GC11264 and shader-cache policy, with savedata off.
Close before300seconds, and compare the same route. No FPS gain or visual
correctness is claimed until gameplay evidence is available.

## Focused pool-mapping diagnostic host

`SOTC_POOL_PROBE=1` adds a bounded observational query wrapper to an isolated host:

```sh
SOTC_POOL_PROBE=1 \
SOTC_CPU_SOURCE="$PWD/work/cpu-layout-4d9c38444cac" \
SOTC_HOST_BUILD_DIR="$PWD/work/host-memory-probe-20261104" \
bash tools/build_host.sh
```

It remains inactive unless `SOTC_POOL_MAPPING_TRACE=1` is set at launch. The local
`prueba_memoria_compartida.command` also enables private signature-checked CPU
probes for the observed recursive-list failure, with bounded event journals.
Those private game-derived selections are not distributed. This is an observation
build, not a memory corruption fix or a performance measurement. It retains the
control renderer, coarse read tracking and disabled savedata.

The local focal launcher now allows20minutes by default, so instrumentation does
not force the player to finish the route in5minutes:

```sh
./prueba_memoria_compartida.command
```

An optional first argument selects600,900,1200 or1800seconds. Other comparison
launchers retain their existing duration. Close the game when the reproduction
attempt is finished; there is no need to use the entire budget. The GPU watchdog
and bounded diagnostic journals remain enabled.

The local shader-origin diagnostic (2026-11-05) uses
`./prueba_origen_shader.command` (default1200seconds; optional600/900/1800).
It reuses the verified host/renderer and enables bounded submission/consumption,
register and binding evidence while disabling list-pointer and pool-map probes.
Close normally or allow the original failure to stop it; do not interpret its
FPS as a baseline. Shader writer watches are inactive in this profile. Launchers,
logs and binary journals remain private local artifacts, not build prerequisites.

For the isolated contiguous-pool host (default behavior remains off):

```sh
SOTC_CPU_SOURCE="$PWD/work/cpu-layout-4d9c38444cac" \
SOTC_HOST_BUILD_DIR="$PWD/work/host-contiguous-20261105" \
SOTC_POOL_PROBE=1 SOTC_SAVE_TRIAL=0 bash tools/build_host.sh
```

The private `./prueba_pool_contiguo.command` selects this verified host and enables
`SOTC_POOL_CONTIGUOUS=1`; renderer and CPU versions stay unchanged. Only mappings
with matching attributes and continuous physical offsets can extend the returned
pool span. This is not a general writeback fix for discontiguous buffers.

Texture alias trial (isolated optional renderer): after the verified control GPU
build exists, run `python3 tools/check_texture_alias_trial.py` then
`python3 tools/build_texture_alias_trial.py`. The private gameplay launcher is
`./prueba_alias_texturas.command` (1200seconds by default). The build tool uses
existing native objects plus two freshly compiled patched files, records input
hashes, and keeps the control library/source unchanged. It does not integrate the
patch into the default clean build. Both CPU host and renderer are hash-checked
by the local launcher; contiguous-pool lookup is explicitly disabled for this trial.

After the texture-alias trial stall, use the private
`./prueba_progreso_gpu.command` for a control-renderer comparison with bounded
Metal encoder checkpoints. Duration defaults to1200seconds; CPU/list and shader
prefix scanning are off. `SOTC_SUPERVISOR_HEARTBEAT=1` adds a15-second status line;
this is supervisor liveness, not evidence that frames or GPU work are advancing.
The renderer watchdog remains120seconds and the total deadline is not extended.

The isolated interval-set performance backport can be built with
`python3 tools/build_hoist_interval_trial.py` and checked with
`python3 tools/check_hoist_interval_trial.py`, after the control GPU build exists.
It includes only PR11's overlap-search optimization, not exact-read tracking.
The private launcher `./prueba_rendimiento_intervalos.command` defaults to mode1
and1800seconds; `./prueba_rendimiento_intervalos.command 0` uses the same renderer
with linear overlap searches. An optional second argument selects600/900/1200/1800
seconds. Expensive shader/GPU diagnostics are off in both modes. Preserve route,
cache warmth and launch settings when comparing; the default clean build is not
silently updated by this optional incremental trial.

For the optional archive scheduling trial, build with
`python3 tools/build_archive_interval_trial.py` and validate with
`python3 tools/check_archive_interval_trial.py` after the control GPU build exists.
The private launcher `./prueba_cache_menos_escrituras.command` keeps a30-minute
route budget and selects120seconds between background archive rebuild attempts.
Use first argument10 for the same-binary control. Explicit flush/normal shutdown
and atomic archive replacement remain unchanged; crashing can lose more recently
compiled, not-yet-written cache entries. This does not change the game save system.


Vegetation controls are an isolated opt-in trial. Build the renderer with
`python3 tools/build_vegetation_trial.py` against the existing verified control
build, and check the generic parameter adapter with
`python3 tools/check_vegetation_trial.py`. The build records the adapter header
hash as well as source/object provenance. Native host startup-option coverage is
in `tools/tests/test_startup_options.py`. The private local launcher supplies the
reviewed shader profile and supports `ligera`, `sin`, `original`, and
`personalizada`; it is not a redistributable release launcher. The engine's
visibility and cascade options still need owner visual validation.


The subsequent no-vegetation/cache trial is built with
`python3 tools/build_vegetation_cache_trial.py`. It combines the vegetation and
archive-interval patches in an isolated output, preserving both earlier builds.
The private launcher `prueba_sin_vegetacion_cache.command` keeps vegetation off;
its optional first argument is `120` (default) or `10` (writer control), and the
second is the existing play budget. Cache compatibility still requires exact
renderer identity; no old cache metadata is rewritten to bypass that check.

The adaptive-memory trial is built with
`python3 tools/build_memory_adaptive_trial.py`. The private local launcher
`./prueba_memoria_adaptativa.command` combines the existing low-load graphics
profile, ongoing mapping observations and optional staging size classes. Its
first argument selects 600/900/1200/1800 seconds (default 1800). The baseline
renderer remains unchanged. Validate staging behavior with
`python3 tools/check_staging_classes_trial.py` and the memory selector with
`python3 -m unittest discover -s tools/tests -p test_hardware_memory.py`.

Both normal runtime settings and newly created portable packages include
`tools/hardware-memory.sh`. Each launch detects the destination Mac's physical
RAM, rather than preserving the packaging machine's value. `SOTC_RAM_PROFILE`
defaults to `auto`; `conservative` forces the existing 512 MiB retention setting.

| Detected physical RAM | Staging retention threshold per ring |
| --- | --- |
| Unknown or below 24 GiB | 512 MiB |
| 24–31 GiB | 768 MiB |
| 32–63 GiB | 1024 MiB |
| 64 GiB or more | 1536 MiB |

These thresholds retain already allocated buffers for reuse. They are neither
up-front reservations nor total memory caps; multiple staging rings can each
retain memory. Prewarming, graphics quality and GPU completion guards do not
change. Size classes are a separate opt-in renderer feature with a cumulative
64 MiB padding budget, not automatically enabled in the baseline renderer.
More RAM does not enable direct guest-memory import. If macOS denies the RAM
query, the conservative threshold is used. Higher-RAM tiers have synthetic
selection coverage, but require gameplay validation on the destination machine.

The isolated scatter diagnostic builds with
`python3 tools/build_scatter_probe_trial.py` after the verified baseline build.
Run `python3 tools/check_scatter_probe_trial.py` for the original policy fixtures.
The private `./prueba_indices_normales.command` selects the reviewed two-entry
profile and a 1800-second budget; the optional first argument accepts
600/900/1200/1800. Look for `GPU scatter probe`, `GPU scatter encoded` and
`GPU scatter result` in the ordinary game log. `invalid` or `input_short` is
evidence of indices outside the captured binding extents. Zero violations only
cover the reported completed snapshots; skipped/truncated work, CPU races and
page reuse remain unresolved. No original dispatch or write is suppressed.
