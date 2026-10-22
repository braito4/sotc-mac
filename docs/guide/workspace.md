# Consolidated local workspace

Use this checkout as the single active project. `runtime/src`, `tools`, `tests`
and `packaging` contain maintained source; `runtime/upstream` and `reference`
retain the selected Git histories. `work`, `game` and `logs` remain private and
ignored by Git. Historical diagnostic records preserve their original paths as
evidence; those strings are not active dependency locations.

The busy-queue maintenance correction is already integrated. No separate patch
installation is needed. The [handoff](../DEVELOPMENT_HANDOFF.md) identifies the
measured native failure and CPU-only validation boundary.

## Move the prepared checkout to Documents

From a normal macOS Terminal, run the prepared checkout's
`packaging/finalizar-en-documentos.command`. It moves the complete folder to
`~/Documents/sotc-mac` unless an explicit first argument selects another new
location. It refuses to overwrite an existing destination, repairs local paths,
rebuilds the ARM64 renderer and runs the CPU contracts. It never starts a game.
Afterwards select the destination as the task's workspace to continue editing.

## Relocate again

Run from the moved project root before using any generated build:

```sh
python3 tools/relocate_workspace.py
bash packaging/build-gpu-arm64.sh
bash packaging/test-arm64.sh --contracts-test
```

The private checkpoint remembers previous project roots. For an unrecorded
copy, pass `--from /absolute/previous/project` to the relocation tool. It updates
installed library load commands and ad-hoc signatures, installed metadata and
internal links, leaving game input and historical captures unchanged. The
builder detects copied source/output locations and freshly configures CMake.
Do not run an old generated Ninja build before this step.

The [build guide](building.md) contains the normal commands. A new game/GPU
comparison requires a fresh public upstream review and the existing gradual
owner-supervised procedure; copying and building do not validate OOM avoidance.
