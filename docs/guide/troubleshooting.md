# Troubleshooting

## Expected diagnostic stops

`STOP_AT_IMPORT` means the CPU reached a system function outside the implemented contract set.
A matching stop in JIT and interpreter is a bounded initialization result, not a game boot.

Once worker threads are active, different scheduling may expose different first missing APIs.
The comparison reports such a difference as `INCONCLUSIVE`, not a successful equivalence check.
The terminating log names the reporting guest thread; only the first thread prints the stop.

`STOP_HOST_RESOURCE` means a required host resource could not be created. Restricted execution
environments may reject POSIX shared-memory creation with `EPERM`. The CPU diagnostic can
instead use an unlinked temporary file with identical MAP_SHARED alias semantics:

```sh
SOTC_MEMORY_BACKING=file bash packaging/test-arm64.sh --linked-test
```

This mode uses local temporary filesystem space while the process is alive. It changes
neither mapping bounds nor permissions. Without the option the pinned shared-memory backend
is unchanged. See [Reference review and follow-up](../REFERENCE_REVIEW.md) for verified scope.

`STOP_HOST_GRAPHICS` means the standalone Metal prerequisite check could not access or verify
the GPU. Zero enumerated devices describes this process, not necessarily the physical Mac.
Run `--graphics-check` in a native Terminal; no synthetic game frame or fallback GPU is created.
A successful check establishes only an offscreen synthetic draw. See
[Graphics bring-up](../GRAPHICS_BRINGUP.md).

`STOP_TIMEOUT` remains a failed diagnostic. The default watchdog is 30 seconds;
`SOTC_DIAGNOSTIC_SECONDS=1..60` changes its bounded duration without disabling it.

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
