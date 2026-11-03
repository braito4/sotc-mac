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
