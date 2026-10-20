# Development status

Updated 2026-10-20. **CPU bootstrap research; not playable.**

| Area | Verified scope |
|---|---|
| Host CPU | ARM64-only diagnostic and bbcpu library; no Rosetta |
| Upstream checks | Native emitter and JIT fault recovery checks passed |
| Runtime contracts | Scoped resolution, capability gate, heap/realloc, recursive mutex lifecycle and destructor callbacks |
| Read/write locks | Non-timed kernel backend: synthetic guest lifecycle/errors, shared readers, exclusive writes and ownership checks pass in both modes |
| Memory | Largest-free-range queries against an independent bitmap; real direct aliases, release and flexible availability verified in normal Terminal |
| Console | x86-to-ARM64 writes, descriptor isolation, embedded NUL and guest errors |
| Clocks | TSC/API agreement, clock ABI, real sleeps and realtime |
| Timezones | UTC/local round trips across four zones, seasonal/half-hour DST and a transition boundary |
| Content API | Read-only fixture reads, seek/pread/EOF, stat layout, error and write-rejection tests |
| Original modules | libc and Fios2 initializers returned zero in JIT and interpreter |
| Fibers | Synthetic initialize ABI, yield/resume/switch, stack/register/FPU state, restart, attach-context, lifecycle errors and host-thread migration in both modes |
| Module registry | Reference-counted availability for the implemented fiber module; unsupported modules stay unavailable |
| Guest threads | Synthetic return/exit/join/detach, competing joins, separate TCB/static/module TLS and errno, DTV, low stacks, inherited FPU, condition waits and concurrent read-only I/O pass in both modes |

The latest normal Terminal game baseline completed early memory setup and entered Fios2 startup.
Both modes used three read/write locks and three condition variables, then stopped at the same
unresolved worker-thread creation call. File, worker-thread and game-fiber counters remained zero.

Non-timed kernel read/write-lock calls now reuse the upstream backend, with synthetic guest
lifecycle, shared-reader, exclusive-writer, ownership and busy-destroy checks. The kernel
relative-timeout NIDs stay unresolved because their argument contract differs from the upstream
absolute-timespec helpers.

Checked thread creation, return, exit, join and detach are now connected to native pthreads.
Dynamic module TLS and DTV tables are isolated per thread; module TLS allocations are released
when a worker finishes. Synthetic guest workers test this concurrently, including two callers
competing to join one worker. Original libc/Fios2 initializers still return zero in both modes.
Linked game progress after enabling worker threads remains to be verified in a normal Terminal;
tool execution still rejects shared-memory backing with EPERM.

Thread keys and their destructors are unavailable and remain unresolved. Exiting through a live
fiber or trap frame stops explicitly. Guest stacks, TCBs and diagnostic CPU records retain the
upstream process-lifetime allocation policy; long-running thread churn is not validated.

## Next verification

Run `bash packaging/test-arm64.sh --linked-test` in a normal native Terminal. Compare both
stops, original module initialization, memory accounting and actual file counters. Do not
infer content loading merely from linking Fios2 successfully.

## Remaining work

- Resolve the game's packed static-image pointer/address requirements.
- Implement additional required system services with checked ABI and error behavior.
- Validate actual Fios2 workers and subsequent game threading; add missing thread-key contracts
  and complete lifecycle/resource reclamation where needed.
- Validate actual game content loading, not only synthetic filesystem fixtures.
- Integrate and validate graphics, audio, input and a SotC launcher.
- Add save support with a separate tested workflow when gameplay is possible.

The diagnostic budget is deliberately bounded. No release, gameplay compatibility or FPS
claim is implied by initialization progress.
