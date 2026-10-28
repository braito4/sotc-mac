# sotc-mac

Experimental **Shadow of the Colossus for Apple Silicon**, using native ARM64 CPU
translation and Metal rendering. No Rosetta 2.

**Playable on Apple M1 with 16 GB, with graphical artifacts and low frame rates.**
The owner reports a noticeable improvement after the October 28 runtime update.
Vegetation, colored texture speckles and occasional planes across the horizon
still need work. The earlier spikes extending from the character are no longer
reported. Short gameplay runs work; long-session stability is not established.
Save persistence is **not implemented**. A DualSense has connected successfully.
M5 gameplay has not yet been validated.

## Build and run

Requires an Apple Silicon Mac, macOS 15 or newer, Xcode Command Line Tools,
Python 3, CMake, Ninja, pkg-config and glslang. See [build instructions](docs/BUILD.md).

```sh
bash build.sh
bash run.sh /absolute/path/to/CUSA08809
```

The default run lasts 180 seconds and records local logs. Use
`SOTC_RUN_SECONDS=0 bash run.sh /absolute/path/to/CUSA08809` for an unlimited run.
The supported input is your own decrypted **CUSA08809, update 1.01**, plus the
required original system modules. No game, firmware, saves or compiled game
images are included in this repository.

## Portable private build

The [build guide](docs/BUILD.md#portable-private-build) explains how to create one
self-extracting `.command` containing your native executable, libraries and game
files for your own Mac. The destination does not need Python or build tools.
Game-containing packages remain in the ignored `work/` directory and must not
be uploaded to this repository.

## Dependencies and credits

The checked source versions are recorded in [tools/dependencies.json](tools/dependencies.json).
The latest CPU integration adds cached native calls, a larger indirect-branch cache
and a floating-point conversion fix. It passes synthetic CPU checks; its gameplay
performance remains to be compared against the previous build.

Built on [bloodborne-native](https://github.com/bloodborne-native/bloodborne-mac),
[bbport](https://github.com/deadinside28/bloodborne_pc),
[shadPS4](https://github.com/shadps4-emu/shadPS4) and
[AjncSystem's SotC port](https://github.com/AjncSystem/ShadowOfTheColossus-PC).
See [credits and licenses](docs/CREDITS.md). Licensed under [GPL v2 or later](LICENSE).

Independent project; not affiliated with Sony, Bluepoint Games or Team Ico.
