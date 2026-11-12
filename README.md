# sotc-mac

Experimental **Shadow of the Colossus for Apple Silicon**, using native ARM64 CPU
translation and Metal rendering. No Rosetta 2.

**Playable on Apple M1 with 16 GB, with graphical artifacts and low frame rates.**
Colored texture speckles and other route-dependent artifacts remain. The latest
instrumented propagation trials passed the previously recurring failure point and
ended with manual window closes; the latest run reported a median of 5.3 FPS.
This is a targeted validation, not a general crash fix or controlled benchmark.
The paired-output extension is tested synthetically and still awaits gameplay
validation. These optional trials are documented in the [contributor handoff](docs/DEBUGGING.md).
Save persistence is **disabled by default**. An [opt-in save backend](docs/BUILD.md#optional-persistent-savedata-trial-m1) has passed synthetic tests; in-game save/load remains unvalidated. A DualSense has connected successfully.
The owner has also run the private build on an M5 with 32 GB: late samples were
roughly 2.4–5.6 FPS. This confirms execution, not correct rendering or smooth play.

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
The October 29 update preserves those CPU fixes and refreshes the runtime and
translator. It adds unique persisted shader-permutation IDs, avoids a repeated
environment-lock lookup in draw workers, and assigns guest threads upstream
macOS QoS. The shader cache is version 30/9. Subsequent M1 runs reached gameplay; the owner
reported no visible artifacts in one tested route, while broader visual correctness
and long-session stability remain unverified. An opt-in standalone bbcpu build and
isolated output directories are documented in [BUILD.md](docs/BUILD.md).

Built on [bloodborne-native](https://github.com/bloodborne-native/bloodborne-mac),
[bbport](https://github.com/deadinside28/bloodborne_pc),
[shadPS4](https://github.com/shadps4-emu/shadPS4) and
[AjncSystem's SotC port](https://github.com/AjncSystem/ShadowOfTheColossus-PC).
See [credits and licenses](docs/CREDITS.md). Licensed under [GPL v2 or later](LICENSE).

Independent project; not affiliated with Sony, Bluepoint Games or Team Ico.

Porting approaches and review priorities: [Mac port strategies](docs/MAC_PORT_STRATEGIES.md).

Component ownership, upstream integration and the planned repository split:
[repository workflow](docs/REPOSITORY_WORKFLOW.md). Public CI runs without game data;
new host builds include source fingerprints in `host-build.json`.

## Continuing development

Start with the [contributor handoff](docs/DEBUGGING.md): current buffer-coherence
findings, known diagnostic gaps, source locations, validation commands and next
steps. Optional renderer trials remain separate from the default build. Their
incremental builders require an existing verified native build; some intentionally
require the original local control binary and private shader selections. They are
research sources, not ready-to-run releases for every checkout.
