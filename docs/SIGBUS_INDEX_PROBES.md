# Indexed-table SIGBUS probes

The private owner profile now observes the newly captured indexed-table fault:

- Original object and its child-pointer field, before the first load.
- Child object and the source index field, before the 32-bit load.
- Related count/context, before the source load.
- Numeric index before multiplication by three, without reading through it.
- Numeric scaled index before the table access, without reading through it.
- Table base and the first table entry, independently of the indexed target.
- Frame identity and an old output local, before the result is stored.

All 24 previous pointer targets remain; two observation-only register probes
bring the count to 26. The existing four-byte reader/index probes remain active.
Five scalar comparator observations were reassigned, keeping the scalar count at
28. Prior profiles and the matching executable are preserved privately. The two
extra pointer channels add 32 KiB; persistent watch banks do not grow.

The private profile checks exact instruction signatures against the prepared
input before launch. Its named analysis scenarios identify each stage. Numeric
matches and nearby timestamps are candidates, not automatic causal joins;
compare thread/frame identity and the pointer chain independently. Scalar reads
sample a qword, so only the low 32 bits of the source-index sample correspond to
the guest index. An unavailable qword sample does not prove a four-byte guest
load would fail. The frame-local sample precedes the store and is not its result.

An observation at the faulting instruction routes it through the existing
interpreter probe path. That can remove the translated terminal register report;
independent committed journals retain the pre-instruction observations. Timing
changes and another stop do not constitute a correction.

Original synthetic tests exercise 26 pointer targets/28 journal channels, a
multiply-by-three index, exact address arithmetic and preservation of invalid
accesses under JIT and interpreter execution. They use no game instructions or
data. The first completed owner run with this profile records 5,501 hits at each
of the seven new observation roles, then stops on a shader metadata rejection.
This confirms probe coverage, not reproduction or correction of the SIGBUS.

```sh
SOTC_SHADER_BIND_TRACE=1 SOTC_SKIP_INTRO=1 SOTC_REQUIRE_POINTER_TRACE=1 \
  bash prueba_grafica_nativa.command base
```

The new profile does not clamp indices, replace pointers or change memory
permissions. Shader binding probes remain enabled independently.
