# Development status

Updated 2026-10-20. **CPU bootstrap research; not playable.**

| Area | Verified scope |
|---|---|
| Host CPU | ARM64-only diagnostic and bbcpu library; no Rosetta |
| Upstream checks | Native emitter and JIT fault recovery checks passed |
| Runtime contracts | Scoped resolution, capability gate, heap/realloc, recursive mutex lifecycle and destructor callbacks |
| Memory | Largest-free-range queries against an independent bitmap; real direct aliases, release and flexible availability verified in normal Terminal |
| Console | x86-to-ARM64 writes, descriptor isolation, embedded NUL and guest errors |
| Clocks | TSC/API agreement, clock ABI, real sleeps and realtime |
| Timezones | UTC/local round trips across four zones, seasonal/half-hour DST and a transition boundary |
| Content API | Read-only fixture reads, seek/pread/EOF, stat layout, error and write-rejection tests |
| Original modules | libc and Fios2 initializers returned zero in JIT and interpreter |
| Fibers | Synthetic initialize ABI, yield/resume/switch, stack/register/FPU state, restart, attach-context, lifecycle errors and host-thread migration in both modes |
| Module registry | Reference-counted availability for the implemented fiber module; unsupported modules stay unavailable |

The last normal Terminal game baseline completed early memory setup and reached an unresolved
system-module load in both CPU modes. File counters remained zero. The new fiber/module support
has passed synthetic tests; linked game progress after this addition remains to be verified.

## Next verification

Run `bash packaging/test-arm64.sh --linked-test` in a normal native Terminal. Compare both
stops, original module initialization, memory accounting and actual file counters. Do not
infer content loading merely from linking Fios2 successfully.

## Remaining work

- Resolve the game's packed static-image pointer/address requirements.
- Implement additional required system services with checked ABI and error behavior.
- Integrate guest multithreading and per-thread module TLS before enabling thread creation.
- Validate actual game content loading, not only synthetic filesystem fixtures.
- Integrate and validate graphics, audio, input and a SotC launcher.
- Add save support with a separate tested workflow when gameplay is possible.

The diagnostic budget is deliberately bounded. No release, gameplay compatibility or FPS
claim is implied by initialization progress.
