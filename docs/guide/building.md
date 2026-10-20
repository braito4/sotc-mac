# Building from source

Use an ARM64 macOS Terminal. `uname -m` must print `arm64`. Rosetta is not required for this
workflow. The official upstream comparison suite uses an x86 reference; it is not part of
these native-only checks.

## Sources

Clone this repository from its GitHub page, enter its directory and initialize the required
submodules:

```sh
bash packaging/test-arm64.sh --fetch
```

Only the runtime, CPU and SotC reference sources are needed for these diagnostics; GPU and
launcher dependencies are not built. The runtime and CPU revisions are pinned in
[Credits](../CREDITS.md). The fetch script checks out those revisions, not the latest upstream
branch. Locally supplied ZIP snapshots can be used at the same paths; do not run `--fetch`
over non-Git snapshot directories.

## CPU library and dependencies

Install Xcode Command Line Tools, CMake, Ninja and Python 3 if needed. Then:

```sh
bash packaging/test-arm64.sh --check
bash packaging/test-arm64.sh --test
bash packaging/test-arm64.sh --contracts-test
```

The upstream CPU build installs Zydis 4.1.1 and its bundled Zycore when absent. Its default
prefix in this project is `work/deps-arm64`. For an existing compatible static installation:

```sh
export BB_DEPS="/absolute/path/to/arm64-dependencies"
bash packaging/test-arm64.sh --test
bash packaging/test-arm64.sh --contracts-test
```

The prefix must contain `include/Zydis`, `include/Zycore`, `lib/libZydis.a` and
`lib/libZycore.a`. Both libraries must be ARM64 builds.

The build copies the CPU archive privately and removes `cpu.c.o` and `interp_vec.c.o`.
It includes unchanged pinned source files through a context adapter and a vector extension.
The extension handles PCMPISTRM/VPCMPISTRM; other vector instructions use the original
implementation. The decoder, JIT and remaining CPU objects are reused. The original archive
and upstream source files are unchanged.

`--test` runs the native emitter, JIT fault checks linked to the adapted archive, and synthetic
string-mask checks in both CPU modes. `--contracts-test` also includes the string-mask checks
alongside the runtime contracts. Neither command needs game files or Rosetta.

The thread adapter similarly includes the pinned thread backend and wraps its start, TLS and
lifecycle boundaries. A private build copy of the mutex backend makes only diagnostic counters
atomic. The source checkout is not patched. Synthetic tests now also start concurrent guest
workers and test condition waits, isolated TLS, thread completion and serialized read-only I/O.

## Optional game initialization probe

Prepare [your own game files](game-files.md), then run `--linked-test` in a normal Terminal.
The linked test requires POSIX shared-memory creation for direct-memory backing. Some
restricted automation environments reject that operation; `--contracts-test` does not need it.

No renderer or playable app is built by these commands.
