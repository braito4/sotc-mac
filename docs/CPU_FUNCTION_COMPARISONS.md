# Controlled CPU function comparisons

The owner requested reversible changes for three priority functions. These
variants change the execution backend of one reviewed function interval to the
existing interpreter while the rest of the image keeps the JIT. They test
whether translation of that function contributes to the observed behavior.
They are not established corruption fixes or native source rewrites.

| Variant | Selected interval | Comparison question |
|---|---|---|
| `base` | None | Normal diagnostic JIT path with the same pointer profile. |
| `caller` | Indexed-argument caller | Does interpreting argument/index preparation change the result? |
| `buffer` | Local-buffer owner | Does interpreting setup, loop and guard comparison change the result? |
| `table` | Table generator | Does interpreting table construction change the result? |

The owner launcher accepts one label:

```sh
bash ./prueba_grafica_nativa.command caller
bash ./prueba_grafica_nativa.command base
bash ./prueba_grafica_nativa.command buffer
bash ./prueba_grafica_nativa.command table
```

The private selection file supplies the default when no argument is given.
Unknown labels are rejected. Each invocation launches one supervised run, without
retries. The launcher clears inherited range and instruction-denial selections
before choosing the comparison, so they do not silently combine.

## Scope and validation

`check_cpu_experiment.py` validates a bounded private profile containing the exact
prepared-input hash and complete interval hashes. It checks the selected range
against one executable image segment before game execution. The fixed labels
never become shell code; profiles are parsed as JSON. Saved validation uses
exclusive no-follow creation with mode 0600. The launcher keeps `umask 077`.
Invalid ranges are also rejected by the native input reader.

The checked JIT generator splits blocks before a selected interval. Selected
instructions go through the existing interpreter dispatcher, including branches,
CALL and RET; they receive no translated code or cache entry. Thus block chaining
and indirect-cache hits cannot bypass the selection. Callees outside the lexical
interval keep their ordinary backend. Configuration is immutable before first
execution; changing it after compilation is unsupported.

No guest instruction, operand, copy size, return value, guard, shader acceptance,
memory-ordering rule or executable-memory protection is edited. Invalid accesses
still fail and unsupported instructions retain their failure path. A fault
inside the interpreter can lack translated terminal registers; retained evidence
must not acquire an invented guest context.

The supervisor records the variant, requested interval and profile path. Each
state directory retains `cpu-experiment-check.json`. The CPU logs configuration
and its first actual entry into the selected interval. Offline review separates
validated entry, validated but unobserved selection, base mode, missing validation
and mismatched evidence. The pointer profile and its fixed journal budget remain
unchanged.

## Interpretation of results

Original synthetic controls check block-boundary crossings, conditional flags,
nested calls, returns, guarded output, preserved registers, predecoded blocks,
repeated cached execution and concurrent threads. They confirm that instructions
outside the selected interval still translate and that selected instructions do
not. Invalid memory and unsupported-instruction controls retain their failures.
Private-profile and launcher controls cover exact hashes, executable segments,
malformed selections, explicit base mode and output-file protections.

A native run that reaches further or stops differently can reflect timing. Keep
the base and repeat each comparison before attributing a defect to CPU
translation. A changed exit is not a correction. The previous executable and
profiles are preserved privately, and source/dependency pins are unchanged.
