"""Compare diagnostic boundaries; these logs stay private and outside Git."""
from pathlib import Path
import sys

def stop(path):
    return tuple(line for line in Path(path).read_text().splitlines()
                 if line.startswith(('STOP_AT_IMPORT:', 'Guest caller offset:', 'Guest thread:',
                                     'STOP_HOST_RESOURCE:', 'STOP_MEMORY_LAYOUT:')))

a, b = map(stop, sys.argv[1:3])
if not a or a != b:
    if a and b and any('Runtime: guest threads created=' in Path(p).read_text()
                       and 'Runtime: guest threads created=0,' not in Path(p).read_text() for p in sys.argv[1:3]):
        raise SystemExit('INCONCLUSIVE: concurrent startup reached different stopping boundaries; this does not establish CPU-mode equivalence.')
    raise SystemExit('ERROR: JIT and interpreter did not reach the same stopping point.')
if sys.argv[3] == '--linked-test':
    for path in sys.argv[1:3]:
        log = Path(path).read_text()
        for module in (0, 1):
            if f'Linked module {module} initializer returned 0' not in log:
                raise SystemExit('ERROR: libc/Fios2 initialization did not complete.')
if a[0].startswith(('STOP_HOST_RESOURCE:', 'STOP_MEMORY_LAYOUT:')):
    print('BLOCKED: both modes reached the same memory failure. Review the private logs.')
else:
    print('PASS: JIT and interpreter reached the same unimplemented import. Gameplay has not started.')
