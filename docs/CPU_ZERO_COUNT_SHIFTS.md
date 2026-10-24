# Masked-zero 32-bit register shifts

On October 24, 2026, an original scalar fixture reproduced a shared interpreter
and ARM64 JIT defect: `SHL`, `SHR` and `SAR` with a 32-bit register destination
preserved its upper 32 bits when the masked count was zero. A 32-bit destination
must zero-extend while the zero-count instruction preserves flags. For example,
starting with RAX `0x1234567887654321`, `shl eax,0` must leave RAX `0x87654321`.
Both local backends previously returned the full initial RAX.

The pinned upstream checkout remains unchanged. The diagnostic build prepares
hash-checked private copies of the JIT and scalar interpreter. The interpreter
uses its existing register-write helper on this exact case. The JIT emits a
flag-preserving W-register move for immediate masked-zero shifts and before the
CL zero-count branch. Saving the CL count before the move also supports ECX as
the destination. Memory destinations and their access behavior are unchanged.
This correction is limited to these three shift mnemonics.

The original regression fixture `tests/test_cpu_zero_shift.c` covers immediate
and CL counts, EAX/ECX/R8D/R15D and their 64-bit counterparts, high initial bits,
zero and nonzero masked counts, and exact flag preservation at zero count.
All 1,296 cases pass in each backend after the correction, and the full synthetic
contracts suite passes. The fixture runs in both backends as part of:

```sh
bash packaging/test-arm64.sh --contracts-test
```

The independent implementation reference is
[Box64 ARM64 dynarec at the reviewed commit](https://github.com/ptitSeb/box64/blob/420eda69ff51ab5d5a7cbc3dee2e70433be3411d/src/dynarec/arm64/dynarec_arm64_00.c).
Its C1 shift handlers explicitly emit `MOVw_REG(ed,ed)` for masked-zero register
destinations, and its D3 handlers retain zero-extension around the count branch.
This was source inspection, not an executed Box64 or native-x86 comparison.
The local expected values use the register-width contract and a scalar shift
oracle; matching local engines alone is not independent correctness evidence.

This fixes a reproducible CPU semantic defect found during the investigation.
No captured game failure has yet been traced to a zero-count 32-bit shift, and
no game or GPU was launched to validate its effect on the observed corruption.
The existing reader/index diagnostic profile and base selection remain active.
