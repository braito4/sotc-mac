# Register XADD alias correction

An original CPU-only fixture reproduced a shared scalar interpreter and ARM64
JIT fallback defect on October 24, 2026. When both operands of `XADD` name the
same register, the old implementation overwrote the sum with the original value.
For example, `xadd eax,eax` starting from EAX = 1 returned 1 instead of 2.

Intel's operation order is: compute the sum, assign the original destination to
the source, then assign the sum to the destination. The independently reviewed
[Box64 interpreter](https://github.com/ptitSeb/box64/blob/420eda69ff51ab5d5a7cbc3dee2e70433be3411d/src/emu/x64run0f.c)
uses this ordering in its C0/C1 handlers. See also Intel's
[instruction reference, XADD](https://cdrdv2-public.intel.com/789589/334569-sdm-vol-2d.pdf).
This is source comparison, not an executed Box64 or native-x86 comparison.

The hash-checked private scalar companion now writes the source before the
register destination. Saved original operands still determine arithmetic flags.
The memory-destination atomic operation and subsequent source update retain
their previous ordering. The pinned upstream checkout is unchanged. Register
XADD uses this interpreter implementation in both local CPU modes.

`tests/test_cpu_xadd.c` tests eight same/distinct register pairs, all four operand
widths, eight input samples, upper-bit behavior, untouched selected registers,
arithmetic flags and DF preservation. Both backends failed before the correction
on the same-register byte case 1 + 1. All 256 cases pass in each backend afterward.
The fixture is included in the CPU-only contracts command:

```sh
bash packaging/test-arm64.sh --contracts-test
```

This establishes a CPU semantic correction, not a cause of the observed game
failures. Raw opcode matches in the private prepared input do not establish
instruction boundaries, register aliasing or execution. No captured failure has
been traced to this form, and no game or GPU was launched for this correction.
