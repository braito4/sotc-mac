"""Prepare the pinned memory backend with growable GPU notification storage.

Queue storage and optional backing-write observations change; allocation,
locking and the upstream checkout are preserved. Observers run outside locks.
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
            '#include "memory_hook_queue.h"\n#include "memory_write_observer.h"',
        '    if (pending_count==sizeof(pending)/sizeof(*pending)) { fputs("STOP: GPU hook queue overflow\\n",stderr); exit(21); }':
            '    sotc_reserve_pending_hook();',
        'int runtime_memory_write_backing(uintptr_t address, const void *data, uint64_t size) {\n    read_lock();':
            'int runtime_memory_write_backing(uintptr_t address, const void *data, uint64_t size) {\n'
            '    const uintptr_t observed_caller=(uintptr_t)__builtin_return_address(0);\n'
            '    sotc_memory_observe_backing_write(address,data,size,0,0,observed_caller);\n    read_lock();',
        '    read_unlock();\n    return ok;\n}\n/* Reads through the backing view:':
            '    read_unlock();\n'
            '    sotc_memory_observe_backing_write(address,data,size,1,ok,observed_caller);\n'
            '    return ok;\n}\n/* Reads through the backing view:',
        'void runtime_memory_note_write(uintptr_t address, uint64_t size) {\n':
            'void runtime_memory_note_write(uintptr_t address, uint64_t size) {\n'
            '    sotc_memory_observe_backing_write(address,NULL,size,2,0,(uintptr_t)__builtin_return_address(0));\n',
    }
    for old, new in replacements.items():
        if source.count(old) != 1:
            raise ValueError('Unexpected GPU notification queue layout.')
        source = source.replace(old, new)
    return '/* Private build copy: growable GPU notification storage. */\n' + source


if __name__ == '__main__':
    Path(sys.argv[2]).write_text(prepare(Path(sys.argv[1]).read_bytes()))
