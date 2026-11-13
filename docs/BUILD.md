# Building for Apple Silicon

Use native ARM64 macOS 15 or newer. Install Xcode Command Line Tools with
`xcode-select --install`. With Homebrew, install the additional build tools:

```sh
brew install python cmake ninja pkg-config glslang
bash build.sh
```

The script fetches the pinned sources, applies digest-checked patches, builds
renderer dependencies and the renderer, then builds the CPU/native host using
that same dependency prefix. Finally it checks loader symbols. Network access
is required on the build Mac. `SOTC_GPU_JOBS` accepts 1–8 (default 2).
A successful compilation does not establish correct gameplay.

The tested glslang version is 16.6.0. To select an installed version explicitly,
set `SOTC_GLSLANG` to its executable. Dependency revisions and package versions
are recorded in `tools/dependencies.json`; the build does not track upstream HEAD.

## Game input

Supply your own decrypted CUSA08809 update 1.01 directory with `eboot.bin`,
`sce_sys/param.sfo`, `sce_module/libc.prx`, `sce_module/libSceFios2.prx` and the
original assets. Place `libSceSystemGesture.sprx` and `libSceNgs2.sprx` in a
sibling `sys_modules` directory, or set `SOTC_SYSTEM_MODULE_ROOT` to their directory.
Input files are read without modification; prepared output stays in `work/`.

```sh
SOTC_RUN_SECONDS=0 bash run.sh /absolute/path/to/CUSA08809
```

Without `SOTC_RUN_SECONDS=0`, the default run budget is 180 seconds. Connect a
compatible controller before starting. SDL's PS4/PS5 HID support is enabled.

## Settings

`tools/runtime-settings.sh` retains the tested single-pool, synchronous submission,
safe-math and index-rebase settings. Shader-specific buffer profiles are not
supplied or enabled by this launcher. Private comparison results using those
profiles must not be interpreted as validation of this default configuration.

For the optional reduced-graphics engine settings:

```sh
source tools/graphics-low-load.sh
SOTC_RUN_SECONDS=0 bash run.sh /absolute/path/to/CUSA08809
```

The preset requests vegetation, ambient occlusion, motion blur, fog and vignette
reductions. Their effect depends on the game path; the preset is not proof of
higher FPS or removal of all affected rendering work.

RAM is detected at launch. Retained staging thresholds per ring are 512 MiB
below 24 GiB RAM (or unknown), 768 MiB at 24–31 GiB, 1024 MiB at 32–63 GiB and
1536 MiB at 64 GiB or above. `SOTC_RAM_PROFILE=conservative` forces 512 MiB.
These are reuse thresholds, not total memory caps or up-front allocations.
Higher-RAM tiers need validation on the destination Mac.

## Source checks

```sh
python3 tools/check_public_source.py
python3 -m unittest discover -s tools/tests
```

Generated libraries, prepared game images, local logs and shader caches stay
outside Git. The public tree does not need prior trial builds or private capture
files to compile. Do not commit any game input or extracted/generated game code.
