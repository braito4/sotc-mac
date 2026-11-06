# Repository boundaries and integration policy

## Target

Follow the component ownership used by
[bloodborne-native](https://github.com/bloodborne-native/bloodborne-mac): the Mac
application integrates a launcher and runtime; CPU execution and ahead-of-time
recompilation have independent repositories. Separation makes updates reviewable;
it does not itself improve FPS. This project has not yet completed that split.

| Component | Current SotC ownership | Medium-term destination |
| --- | --- | --- |
| App, packaging, release pins | `build.sh`, `run.sh`, packaging tools and documentation | Keep in `sotc-mac`, integrating exact component revisions |
| Launcher | Shell launch settings, no independent GUI | Separate launcher only once an actual reusable launcher exists |
| Runtime and Metal | Pinned bloodborne-runtime plus checked GPU overlay and `runtime/src` adapters | Runtime fork with small topic branches; generic fixes proposed upstream; SotC-specific adapters retained |
| CPU execution | Independent CPU builder; unchanged layout control and opt-in bbcpu | Pinned standalone bbcpu, with separately reviewed SotC CPU patches |
| AOT tools | Not required for the host; AOT is not loaded | Independent recomp pin and output, only after ABI and differential validation |

Do not create empty repositories just to match a repository count. Extract a
component with its build, license, public tests and revision history once its
interface is stable. The final app should consume exact Git submodule commits
(or equivalent immutable source locks), not copy arbitrary snapshots of main.
Until that migration, the reviewed overlay remains the reproducible integration
mechanism. It is not a substitute for the eventual component split.

## Changes implemented on November 4, 2026

- The host builder verifies the complete CPU source inventory and the selected
  upstream host files against `tools/host_sources.json`. The inventory was
  derived from the commits in `tools/dependencies.json`, not from a dirty working
  tree. Local snapshot manifests alone are not trusted.
- Both reviewed CPU variants are supported. Unexpected edits and additional CPU
  files are rejected before compilation. Updating a dependency requires reviewing
  and changing its source fingerprints as well as its revision.
- Host source inputs are captured before compilation and rechecked afterward.
  The executable is built under a temporary name before replacing the requested
  output. `host-build.json` records source hashes, local adapter hashes, CPU
  selection, compiler version and executable SHA-256. This is source provenance,
  not a claim of a hermetic or byte-reproducible toolchain.
- Default runtime source selection follows the dependency lock's workspace key.
  Existing CPU/runtime defaults and the renderer are unchanged.
- The public CI workflow uses only public scripts and synthetic build tests.
  It needs neither `work/` nor a game dump. It checks pins, source boundaries,
  shell/Python syntax and build orchestration. It does not run the game or prove
  native compilation. Native builds and gameplay remain separate acceptance steps.
- The PR template records component ownership, exact revisions, tests, gameplay
  evidence and rollback. Merely recording a reviewed upstream revision must never
  promote it to the selected/default revision.

## Integration cycle

1. Review upstream commits and PRs for one component. Record merged/open status,
   exact SHA, source/license and the SotC problem being addressed. Open PRs remain
   experiments until independently reviewed and validated.
2. Use one topic branch, e.g. `runtime/save-mounts`, `cpu/standalone` or
   `metal/upload-ranges`. Keep upstream changes and SotC adaptations distinguishable
   in commits. Do not combine a CPU update with a renderer optimization.
3. Build an isolated output. Never overwrite the playable control for an experiment.
   Keep the renderer hash and cache identity fixed for CPU/runtime-only comparisons.
4. Pass public CI, applicable native contracts, compilation and host/renderer link
   checks. These are separate from gameplay acceptance.
5. Compare the same route on M1, with cache state and settings recorded. Record
   artifacts, frame times, memory, shader compilation and actual close reason.
   Manual closure is not a spontaneous crash. One good scene is not full validation.
6. Promote the integration pin only with that evidence. Preserve the preceding
   source lock and build so rollback means selecting a known version. Promote
   release tags with a manifest and no game files or private logs.
7. Extract the validated runtime/CPU patch series into their component forks. Once
   clean component builds reproduce the integration, replace the corresponding
   overlay machinery with pinned component checkouts. App release PRs then update
   component pins and compatibility evidence rather than importing source changes.

Branch protection and required CI checks are a future GitHub configuration step;
adding a workflow file does not enable those server-side settings. No automatic
updater, remote forks, release publication or upstream PR submission is configured
by this change.

## Next implementation sequence

| Stage | Concrete scope | Acceptance gate |
| --- | --- | --- |
| Save persistence | Optional per-title save root, serialized mounts/file operations, read-only enforcement, atomic file replacement, exact error propagation | Cross-process write/reload, interruption, invalid path, read-only and metadata tests; then a real SotC save/load |
| Reproduce transition | Retry from a real in-game checkpoint if supported | Identify the late-route CPU fault and shader-metadata stop separately; no fabricated checkpoint or pointer repair |
| Metal submission | Review exact read ranges/copy hoisting, then pass/barrier changes from runtime PRs #11/#12 separately | Validate hazards and resource lifetimes; compare M1 frame times with the same renderer options/cache policy |
| Shader preparation | Profile repeated compilation, source sharing and safe prewarming | Measured stall reduction, correct textures and geometry, bounded cache memory |
| Component extraction | Runtime patch series and CPU adapters as independent builds, then app pins | Fresh checkout reproduces control; ABI/link checks, public CI and M1 route validation |

Save support is **not enabled** by the repository-management change. The reviewed
backend lacks some SotC exports; its mount mode does not enforce read-only file
access, and some metadata writes ignore errors. The dialog adapter also reports
unavailable. These need explicit integration rather than exposing the entire
backend unchanged. An arbitrary resume position is not promised.

The opt-in newer CPU is not promoted: `20261103T182439Z` recorded a CPU SIGSEGV;
the old-CPU comparison `20261103T183114Z` recorded a different shader metadata stop.
Their relationship is unproven. Preserve both as separate investigations.

## Local maintenance checks

```sh
python3 tools/check_public_source.py
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

For a native build using the unchanged CPU in its separate directory:

```sh
python3 tools/prepare_cpu_layout.py
SOTC_CPU_SOURCE="$PWD/work/cpu-layout-4d9c38444cac" \
SOTC_HOST_BUILD_DIR="$PWD/work/host-provenance-20261104" \
bash tools/build_host.sh
```

The output is an integration/build check, not a new performance experiment.

### Follow-up: optional savedata backend implemented

The November4 follow-up adds a separately compiled service without changing the
CPU or renderer pin. See [build and limitations](BUILD.md#optional-persistent-savedata-trial-m1).
Native synthetic persistence/interruption/backup and access-control tests pass;
the host and renderer load check pass. The earlier savedata assessment above
records the reason for keeping this opt-in. Game save/load and dialog integration
remain unvalidated; do not mark the savedata acceptance gate complete yet.
