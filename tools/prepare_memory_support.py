"""Prepare the pinned memory backend with growable GPU notification storage.

Queue storage and optional backing-write observations change; allocation,
locking and the upstream checkout are preserved. Existing callback observers run
outside locks; the optional physical ledger records numeric spans under the VMA
lock without mapping queries or external callbacks.
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
            '#include "memory_hook_queue.h"\n#include "memory_write_observer.h"\n#include "buffer_page_history_api.h"',
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
    replacements.update({
        '    zero_phys(start,size); /* zero on reuse */':
            '    zero_phys(start,size); /* zero on reuse */\n'
            '    if (sotc_buffer_page_mapping_v1) sotc_buffer_page_mapping_v1(SOTC_PAGE_ZERO,0,start,size);',
        '    if (kind!=KIND_RESERVED) queue_hook(HOOK_MAP,address,size);':
            '    if (kind!=KIND_RESERVED && sotc_buffer_page_mapping_v1) sotc_buffer_page_mapping_v1(SOTC_PAGE_MAP,address,phys,size);\n'
            '    if (kind!=KIND_RESERVED) queue_hook(HOOK_MAP,address,size);',
        '        if (vmas[i+n].kind==KIND_FLEXIBLE) {':
            '        if (vmas[i+n].kind!=KIND_RESERVED && sotc_buffer_page_mapping_v1) sotc_buffer_page_mapping_v1(SOTC_PAGE_UNMAP,vmas[i+n].start,vmas[i+n].phys,vmas[i+n].end-vmas[i+n].start);\n'
            '        if (vmas[i+n].kind==KIND_FLEXIBLE) {',
        'void runtime_memory_note_cpu_write(uintptr_t address, uint64_t size) {\n':
            'void runtime_memory_note_cpu_write(uintptr_t address, uint64_t size) {\n'
            '    if (sotc_buffer_page_cpu_v1) sotc_buffer_page_cpu_v1(SOTC_PAGE_CPU_WRITE_NOTICE,address,size,0,0);\n',
        '        store_once(backing_base+vmas[i].phys+(at-vmas[i].start),(const unsigned char *)data+(at-address),n);':
            '        store_once(backing_base+vmas[i].phys+(at-vmas[i].start),(const unsigned char *)data+(at-address),n);\n'
            '        if (sotc_buffer_page_backing_v1) sotc_buffer_page_backing_v1(at,vmas[i].phys+(at-vmas[i].start),n,(uintptr_t)data+(at-address),observed_caller);',
    })
    for old, new in replacements.items():
        if source.count(old) != 1:
            raise ValueError('Unexpected GPU notification queue layout.')
        source = source.replace(old, new)
    return '/* Private build copy: growable GPU notification storage. */\n' + source


if __name__ == '__main__':
    Path(sys.argv[2]).write_text(prepare(Path(sys.argv[1]).read_bytes()))
