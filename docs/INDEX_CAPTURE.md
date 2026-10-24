# Four-byte reader and index capture

The owner launcher now selects `base` by default with a reviewed reader/index
observation profile. The previous profile and executable are preserved privately.
All original 22 pointer targets remain; two observation-only numeric register
targets bring the total to 24. Eleven former scalar targets are reassigned,
keeping 28 scalar channels. With all five journals enabled, fixed mapped journal
storage is 1204 KiB, an increase of 32 KiB. Watch-bank capacity is unchanged.
Earlier base/caller timing series used the previous observation profile.

The new observations retain:

- The four-byte value in a register immediately after a fast reader's input load.
- The fast reader's buffer, cursor, end and output-address observations.
- The returned output and reader fields before the next read overwrites the local.
- The saved index before and after scaling, the object base and effective address.

The raw input observation uses the register already loaded by the guest; it does
not reread or copy the entire input buffer. The fast path is only one reader
path. Missing observations cannot establish that the refill path ran. Other
threads, bounded tails and callbacks interrupted at failure can leave gaps.
The generic reader is shared by other calls: its numeric thread and output
address help distinguish candidates but do not prove object ownership.

An unused CPU-journal payload word now retains the native numeric thread identity,
as the pointer journal already does. Older records leave it zero and remain
readable. Logical CPU thread numbers and native identities are separate fields.
No journal layout, commit protocol or locking rule changes.

The reviewed private profile annotates the channels. Its normal pre-launch
signature/selection check retains those annotations. `index_capture` in the
offline report labels the available observations, readable values, low 32-bit
output and missing channels. An eight-byte scalar read is never presented as
an exact four-byte input sample. The report does not invent causal joins from
equal values, addresses or nearby timestamps. Formula annotations do not prove
the valid index range or its original writer.

Original synthetic x86 controls run the reader, saved-value scaling and an
indexed access under both JIT and interpreter. Valid accesses retain the expected
result; invalid indices retain an actual memory fault at the computed address
(SIGBUS or SIGSEGV depending on mapping). Legacy/missing threads and invalid or
mismatched annotations are checked too. Full contracts and final targeted controls
pass without game or GPU execution. Native coverage and overhead remain untested.

No value is clamped, guest instruction changed, failed access skipped or memory
protection relaxed. A probe at an instruction can route that instruction through
the existing interpreter, where a fault may lack translated terminal registers.
Its independent pre-failure journal is retained without inventing that context.

Run one owner-supervised capture from the repository root:

```sh
SOTC_REQUIRE_POINTER_TRACE=1 bash ./prueba_grafica_nativa.command base
```

Keep this profile distinct from the preceding five-run baseline series. The
[function comparisons](CPU_FUNCTION_COMPARISONS.md) remain available as explicit
launcher arguments, but the next capture uses ordinary diagnostic JIT execution.
