# Shadow of the Colossus on Apple Silicon

Native ARM64 / Metal port based on bloodborne-native and shadPS4.

**Playable on Apple M1 with 16 GB RAM, with visual artifacts and low frame rates.**
Colored noise on textures remains; stray geometry can still occur. Recent private
comparison runs completed without crashing, but stability is not guaranteed.
This is a development build, not a finished port. Saves are not yet validated.

## Build and run

Requires Apple Silicon, macOS 15 or newer, Xcode Command Line Tools, Python 3,
CMake, Ninja, pkg-config and glslang. Use a native ARM64 Terminal.

```sh
git clone https://github.com/braito4/sotc-mac.git
cd sotc-mac
bash build.sh
bash run.sh /absolute/path/to/CUSA08809
```

Supply your own decrypted game (CUSA08809, update 1.01), assets and required
original system modules. None are included here. See [BUILD](docs/BUILD.md)
for tool installation, module locations and settings.

The launcher defaults to a three-minute run. For manual closing:

```sh
SOTC_RUN_SECONDS=0 bash run.sh /absolute/path/to/CUSA08809
```

## What is included

- Pinned dependency recipes, native host adapters and checked renderer patches.
- Index-buffer corrections for offsets above 4 GiB, staging/lifetime safeguards,
  texture-cache corrections, audio and gamepad support.
- Generic fragmented-buffer and region support. Shader-specific propagation
  requires a separately reviewed private profile and is disabled by default.
- Build instructions, licenses and small source/build regression checks.

The repository uses a single `main` branch. Historical trial launchers, duplicate
trial patches, captures and generated builds are excluded from the current tree.
Recent sampled input-freeze experiments are not part of this build.

[Corrections and validation limits](docs/CORRECTIONS.md) ·
[Credits and licenses](docs/CREDITS.md)
