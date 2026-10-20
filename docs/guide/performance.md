# Performance

There are no gameplay FPS, loading-time or renderer performance measurements for this port.
It is not playable yet. Bloodborne's published performance does not apply to SotC.

Current verification covers native ARM64 emitter and JIT fault checks, synthetic runtime
contracts, and bounded initialization probes in JIT and interpreter modes. These establish
specific correctness properties; they are not game performance benchmarks.

The full upstream Rosetta comparison suite has not been run. Native-only synthetic checks
are described in [Development status](../STATUS.md).
