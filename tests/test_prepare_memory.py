"""Pinned queue and outside-lock backing-observer adaptation boundaries."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).parents[1]
spec = importlib.util.spec_from_file_location('prepare_memory', ROOT / 'tools/prepare_memory_support.py')
memory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(memory)


class MemoryPreparationTests(unittest.TestCase):
    def test_only_storage_guard_and_observer_calls_change(self):
        path = ROOT / 'runtime/upstream/src/runtime_memory.c'
        original = path.read_bytes()
        prepared = memory.prepare(original)
        self.assertIn('#include "memory_hook_queue.h"', prepared)
        self.assertIn('    sotc_reserve_pending_hook();', prepared)
        restored = prepared.partition('\n')[2].replace(
            '#include "memory_hook_queue.h"\n#include "memory_write_observer.h"',
            'static PendingHook pending[64]; static size_t pending_count;').replace(
            '    sotc_reserve_pending_hook();',
            '    if (pending_count==sizeof(pending)/sizeof(*pending)) { fputs("STOP: GPU hook queue overflow\\n",stderr); exit(21); }')
        for line in [
            '    const uintptr_t observed_caller=(uintptr_t)__builtin_return_address(0);\n',
            '    sotc_memory_observe_backing_write(address,data,size,0,0,observed_caller);\n',
            '    sotc_memory_observe_backing_write(address,data,size,1,ok,observed_caller);\n',
            '    sotc_memory_observe_backing_write(address,NULL,size,2,0,(uintptr_t)__builtin_return_address(0));\n']:
            self.assertEqual(restored.count(line),1)
            restored=restored.replace(line,'')
        self.assertEqual(restored.encode(), original)
        self.assertEqual(path.read_bytes(), original)

    def test_changed_source_is_rejected(self):
        original = (ROOT / 'runtime/upstream/src/runtime_memory.c').read_bytes()
        for data in (b'', original + b'\n', original.replace(b'pending[64]', b'pending[128]')):
            with self.assertRaises(ValueError):
                memory.prepare(data)


if __name__ == '__main__':
    unittest.main()
