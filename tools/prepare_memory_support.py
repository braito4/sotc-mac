"""Prepare the pinned memory backend with growable GPU notification storage.

Only the queue storage and capacity guard change; allocation, callback dispatch,
locking and the upstream checkout are preserved.
"""
import hashlib
from pathlib import Path
import sys


def prepare(data):
    if hashlib.sha256(data).hexdigest() != '9f9d825488473dd3d46b6d8c05e4525959ad4b859dfcfea3f06a98ee143cfff7':
        raise ValueError('Unexpected pinned memory backend; refusing unchecked adaptation.')
    source = data.decode()
    replacements = {
        'static PendingHook pending[64]; static size_t pending_count;':
            '#include "memory_hook_queue.h"',
        '    if (pending_count==sizeof(pending)/sizeof(*pending)) { fputs("STOP: GPU hook queue overflow\\n",stderr); exit(21); }':
            '    sotc_reserve_pending_hook();',
    }
    for old, new in replacements.items():
        if source.count(old) != 1:
            raise ValueError('Unexpected GPU notification queue layout.')
        source = source.replace(old, new)
    return '/* Private build copy: growable GPU notification storage. */\n' + source


if __name__ == '__main__':
    Path(sys.argv[2]).write_text(prepare(Path(sys.argv[1]).read_bytes()))
