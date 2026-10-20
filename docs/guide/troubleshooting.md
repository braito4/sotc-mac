# Troubleshooting

## Expected diagnostic stops

`STOP_AT_IMPORT` means the CPU reached a system function outside the implemented contract set.
A matching stop in JIT and interpreter is a bounded initialization result, not a game boot.

Once worker threads are active, different scheduling may expose different first missing APIs.
The comparison reports such a difference as `INCONCLUSIVE`, not a successful equivalence check.
The terminating log names the reporting guest thread; only the first thread prints the stop.

`STOP_HOST_RESOURCE` means a required host resource could not be created. Restricted execution
environments may reject POSIX shared-memory creation with `EPERM`; run the linked diagnostic
in a normal native macOS Terminal. The synthetic `--contracts-test` avoids that backing pool.

`STOP_MEMORY_LAYOUT` means a requested mapping could not be satisfied. Do not disable fixed-map
bounds or overwrite host shared regions to continue. The temporary image mapping still leaves
the packed-pointer problem unresolved.

An unsupported instruction, timeout, signal or unrecognized exit fails the test. Keep the
private log locally and describe the symptom without publishing game code or addresses.

## Source and dependency setup

Missing `libbbcpu.a`: run `--test` after initializing the pinned sources. Missing Zydis/Zycore:
let the CPU build install them or set `BB_DEPS` as explained in [Building](building.md).

The `--fetch` command is for Git clones. Existing directories extracted from user-supplied
ZIPs do not have Git metadata; do not delete working snapshots to satisfy a checkout command.

Use `SOTC_GAME_ROOT` for a different game location. Keep the same version for preparation,
linking and content mounts. There is no supported Rosetta path in this repository.
