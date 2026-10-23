# Investigating intermittent memory failures

This is the preferred workflow for this project and the most productive method
we have used so far. It has moved investigations from a terminal crash address
to readable source fields, candidate referring entries and earlier/later values
of the same numeric field. We have not compared every debugging method, and do
not claim universal superiority or a native corruption fix.

1. Preserve bounded CPU, shader, pointer and CPU-frame lifecycle journals before
   failure. A truncated final signal report must not erase committed evidence.
2. Reconstruct the combined diagnostic offline and compare it with the saved
   report. Bind probes to the actual input signatures and reject foreign PIDs.
3. Identify the failing instruction, saved registers and source read. Separate
   CPU faults, missing shader metadata, decoding failures and GPU stalls.
4. Trace candidate referring entries and the actual source field. Match observed
   values to terminal registers; retain earlier different observations and their
   timestamps. A previous value is not automatically valid, and a numeric address
   is not an allocation identity.
5. Prioritize a small set of source watches. Keep high-volume helper probes in
   observation mode. Report admission refusal, hash collisions, contention and
   overwritten history; never equate configured probes with successful coverage.
6. Capture imported-copy observations before and after the actual operation.
   Copies can legitimately initialize reused addresses. Direct guest stores,
   other aliases and GPU writes remain outside this observer.
7. Test the proposed instrumentation with original CPU fixtures: valid ordered
   use, premature/stale access, imported versus direct writes, partial copies,
   saturation, contention, missing output and JIT/interpreter comparisons.
8. Change native behavior only when a cause and an appropriate control are
   established. Do not substitute plausible pointers, suppress shader failures,
   fabricate synchronization progress or change timeouts to conceal the stop.

The latest allocator investigation retains two different values of the same
source word within a 404.567 ms observation interval. Repeated content across
captures is a useful lead, but does not identify its writer or prove a common
cause. Protected CPU-frame quarantine tests one host lifetime mechanism; a
missing match does not rule out guest object reuse or other lifetime failures.

Raw logs, input signatures, game addresses, disassembly and captured content
remain private. Public updates describe methods, evidence limits and synthetic
validation. See [CPU diagnostics](CPU_DIAGNOSTICS.md) and the current
[handoff](DEVELOPMENT_HANDOFF.md).
