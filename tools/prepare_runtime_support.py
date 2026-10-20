"""Make the diagnostic mutex/condition statistics atomic in a private build copy.

Synchronization algorithms and the pinned upstream checkout stay unchanged.
Fail on a different source layout rather than silently applying a broad patch.
"""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
for declaration in ('static size_t created, locks, unlocks;', 'static size_t conds, waits, wakeups;'):
    if source.count(declaration) != 1:
        raise SystemExit('Unexpected pinned mutex counter layout.')
    source = source.replace(declaration, declaration.replace('static size_t', 'static _Atomic size_t'))
Path(sys.argv[2]).write_text('/* Private build copy: atomic diagnostic counters only. */\n#include <stdatomic.h>\n' + source)
