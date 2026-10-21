# Troubleshooting

## Host watchdog kernel panic

The owner reported no watchdogd check-ins for 90 seconds, then authorized gradual
GPU testing. The incident-associated private game log ends during renderer initialization,
before guest execution. The preceding complete run already reports a GPU
submission unfinished for more than ten seconds, then the diagnostic timeout.
These observations do not identify the cause of the system panic. The newest native
run now uses DMA off and ends at its 120-second limit without a GPU-stall message,
unresolved import or CPU fault. This does not establish system stability.

Dynamic shader reads are disabled; every explicitly supplied value other than
`BB_DIRECT_MEMORY_ACCESS=0` is rejected before library loading and launch prechecks.
The combined default is 120 seconds at the owner's request. The rollback does not
prove the renderer is safe. The user-space watchdog cannot guarantee recovery from a
kernel/driver hang. The owner has authorized one brief supervised comparison at a
time; do not repeat the DMA-on incident or run a sequence automatically. Continue
using `--contracts-test` for CPU/synthetic verification. See the
[current comparison](../DEVELOPMENT_HANDOFF.md#gradual-gpu-testing).

`STOP_GPU_STALL` (exit 31) means the external supervisor saw a renderer
`GPU watchdog:` message and stopped the process group. `STOP_GPU_WALL_TIMEOUT`
(exit 21) is its wall deadline, including renderer startup; it is an observation
boundary, not successful gameplay. The log retains child output and the reason for
stopping. A normal child stop keeps its original exit status. Never automatically
retry either stop; inspect the saved log first.

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
`SOTC_DIAGNOSTIC_SECONDS=1..1800` changes its bounded duration without disabling it.
Headless probes default to 30 seconds; the combined game/graphics default is 120.
Explicit budgets remain accepted, but increasing them does not resolve a GPU stall
or protect against the reported system watchdog panic. Use one supervised test at a time.

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
