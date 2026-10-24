# CPU translator testing references

The first concrete use of this reference workflow found a shared local
[masked-zero scalar shift defect](CPU_ZERO_COUNT_SHIFTS.md). Box64 source was
inspected at the recorded commit; Box64 execution remains unavailable locally.

Reviewed October 24, 2026 from the projects' own GitHub documentation. These are
testing/debugging references, not integrated replacement CPU backends.

## Standing Box64 reference requested by the owner

When investigating a suspected JIT defect, inspect the corresponding Box64
interpreter and ARM64 dynarec implementation at a recorded commit. Use its testing
method as an independent comparison, preserving the exact source revision and
configuration. Do not equate matching crash symptoms with the same defect.

1. Reduce the suspect instruction sequence to an original synthetic fixture with
   explicit initial registers, flags and memory. Keep I/O, GPU work and unrelated
   threads outside the fixture.
2. Check results under the local interpreter and ARM64 JIT. Record the first
   differing register/flag, effective address, memory write or exception.
3. Compare the same input and semantics with Box64 in a compatible environment,
   using range-limited interpreter/dynarec comparison when appropriate. Inspect
   its source to explain the result; source reading alone is not an executed test.
4. If implementations disagree, check native x86 execution or the ISA definition.
   Box64 is a second implementation, not the sole correctness oracle. Two engines
   can share an error, and an interpreter/JIT pair can share a decoder defect.
5. Add a local original regression fixture before adapting a fix. Preserve guest
   faults, register width, defined flags and memory-ordering requirements. Separate
   instruction semantics from ABI, TLS, callback, lifetime and scheduling failures.

Record unavailable independent execution explicitly. Box64 has not been installed
or run on this Mac as part of this work. No Box64 code, configuration workaround
or runtime backend has been substituted into the port.

| Project | Relevant workflow | Useful adaptation |
|---|---|---|
| [Box64](https://github.com/ptitSeb/box64/blob/main/docs/USAGE.md) | `BOX64_DYNAREC_TEST` compares interpreter and dynarec registers/memory per opcode, optionally within an address range. `BOX64_NODYNAREC` prevents translation within a range; GDBJIT offers generated-code debug metadata. | Differential checks on an isolated synthetic reproduction of the reader/index instructions, stopping at the first divergence. Range interpretation already has a local analogue, but does not itself compare two executions. |
| [FEX](https://github.com/FEX-Emu/FEX/blob/main/unittests/Readme.md) | Handwritten x86/x64 assembly cases, test harnesses, CMake/CTest and Python runners; additional compiler/POSIX tests. Its [example](https://github.com/FEX-Emu/FEX/blob/main/unittests/Example.asm) declares register expectations and memory inputs. | Explicit initial state and expected results for integer width, addressing, flags, callbacks, SIMD and decoder regressions. Do not assume every external test runs in CI. |
| [QEMU TCG](https://github.com/qemu/qemu/blob/master/tests/tcg/README) | Guest regression programs with architecture-specific and multiarch directories, driven by `make check-tcg`; the [x86 tests](https://github.com/qemu/qemu/tree/master/tests/tcg/i386) include flags, shifts, SIMD and exceptions. | Portable instruction regressions and an independent semantic reference. QEMU's user/system layers are distinct from our PS4 host contracts. |

The next useful experiment is a deterministic original fixture for the four-byte
read, saved index, scaling and effective-address calculation, with explicit memory
and register inputs. Compare native x86 expected results with the local interpreter
and ARM64 JIT, then minimize any discrepancy. The current local synthetic reader
fixture checks both backends and invalid-access preservation; it does not yet
provide whole-block shadow execution or an independent native-x86 oracle.

Live comparison of an entire game cannot simply execute it twice: I/O, callbacks,
GPU commands, synchronization, thread timing and memory writes need isolation or
controlled replay. A matching interpreter/JIT result also cannot exclude a shared
decoder error or already incorrect input. The new [index capture](INDEX_CAPTURE.md)
is intended to retain those inputs before choosing the next fixture.

For CI, FEX's reviewed
[Build + Test workflow](https://github.com/FEX-Emu/FEX/blob/main/.github/workflows/ccpp.yml)
invokes `asm_tests`, POSIX and gVisor targets after a successful build. Its separate
GitLab packaging pipeline disables testing; those pipelines should not be
confused. Box64's reviewed manual GitHub workflow builds configured targets, which
does not establish that its interpreter/dynarec comparison runs in CI.

Box64 and FEX target different host/application environments. Replacing bbcpu
would require preserving the existing loader, PS4 imports, guest calls, TLS,
threading, memory and fault ABI. No such migration, memory-ordering relaxation,
dependency installation or game run was performed during this review.

## Register XADD alias regression

The Box64 C0/C1 source at the reviewed revision provides the independent
assignment-order reference for the [local register XADD correction](CPU_XADD.md).
The original fixture fails before the change and passes afterward in both CPU
modes. No executed Box64/native-x86 differential test or game causality is claimed.
