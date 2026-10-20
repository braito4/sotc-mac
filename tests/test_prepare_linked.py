"""Original synthetic preparation fixtures; no game code or firmware required."""
import importlib.util
import json
from pathlib import Path
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('prepare_linked', ROOT/'tools/prepare_linked_arm64.py')
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)


class PrepareLinkedTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sotc module space ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.game = self.root/'game'
        (self.game/'sce_module').mkdir(parents=True)
        for name in PREPARE.MODULES:
            (self.game/'sce_module'/name).write_bytes(b'synthetic source marker')
        self.boot = self.root/'original.bin'
        self.boot.write_bytes(b'BBPROBE2')
        self.gesture = self.root/'modules with spaces/libSceSystemGesture.sprx'
        self.gesture.parent.mkdir()
        self.gesture.write_bytes(b'synthetic gesture source marker')
        self.ngs2 = self.gesture.parent/'libSceNgs2.sprx'
        self.ngs2.write_bytes(b'synthetic NGS2 source marker')
        self.out = self.root/'private output'
        self.identity = ('3pcAvmwKCvM', ('libSceSystemGesture', 1), ('libSceSystemGesture', 257))
        self.metadata = dict(missing=[], symbols=[dict(section=1, type=2, identity=self.identity)])
        self.ngs2_metadata = dict(missing=[], symbols=[dict(section=1, type=2,
            identity=(n, ('libSceNgs2', 1), ('libSceNgs2', 257))) for n in ('mPYgU4oYpuY','i0VnXM-C9fc')])
        self.calls = []
        self.unresolved = []
        self.patched = 0
        def link(game, out, modules):
            self.calls.append((game, out, list(modules)))
            (out/'link.json').write_text(json.dumps(dict(modules=[dict(file=f) for f in modules],
                fs_loads_patched=self.patched, unresolved_imports=self.unresolved)))
        fake = types.SimpleNamespace(module=lambda path: self.ngs2_metadata if path == self.ngs2 else self.metadata,
                                     link=link, patch_fs_loads=None)
        self.addCleanup(patch.stopall)
        patch.object(PREPARE, 'GAME', self.game).start()
        patch.object(PREPARE, 'ORIGINAL', self.boot).start()
        patch.dict('sys.modules', link_modules=fake).start()

    def run_prepare(self, gesture=True, ngs2=False):
        args = ['--out', str(self.out)]
        if gesture:
            args += ['--system-gesture-module', str(self.gesture)]
        if ngs2:
            args += ['--ngs2-module', str(self.ngs2)]
        PREPARE.main(args)

    def test_ngs2_linking_and_identity_failures(self):
        self.run_prepare(ngs2=True)
        self.assertEqual(self.calls[0][2], [*PREPARE.MODULES,str(self.gesture),str(self.ngs2)])
        self.ngs2_metadata['symbols'].pop()
        with self.assertRaisesRegex(SystemExit, 'Unsupported supplied NGS2'):
            self.run_prepare(ngs2=True)
        self.ngs2.unlink()
        with self.assertRaisesRegex(SystemExit, 'Missing supplied libSceNgs2'):
            self.run_prepare(ngs2=True)

    def test_unresolved_ngs2_is_rejected(self):
        self.unresolved = ['i0VnXM-C9fc#z#t']
        with self.assertRaisesRegex(SystemExit, 'did not resolve the game NGS2'):
            self.run_prepare(ngs2=True)

    def test_default_stays_two_modules(self):
        self.run_prepare(False)
        self.assertEqual(self.calls[0][2], list(PREPARE.MODULES))

    def test_empty_tls_metadata_only_is_omitted(self):
        empty = dict(type=7, filesz=0, memsz=0)
        populated = dict(type=7, filesz=0, memsz=16)
        code = dict(type=1, filesz=64, memsz=64)
        metadata = dict(ph=[code, empty, populated], elf=b'unchanged synthetic bytes')
        normalized = PREPARE.without_empty_tls(metadata)
        self.assertEqual(normalized['ph'], [code, populated])
        self.assertEqual(metadata['ph'], [code, empty, populated])
        self.assertEqual(normalized['elf'], metadata['elf'])

    def test_gesture_links_original_after_libc_and_fios_without_input_changes(self):
        self.run_prepare()
        self.assertEqual(self.calls, [(self.game, self.out, [*PREPARE.MODULES, str(self.gesture)])])
        self.assertEqual((self.out/'boot.bin').resolve(), self.boot)
        self.assertEqual(self.gesture.read_bytes(), b'synthetic gesture source marker')
        self.assertFalse((self.game/'sce_module'/'libSceSystemGesture.sprx').exists())
        self.run_prepare()  # Reuse only our expected symlink.

    def test_missing_module_fails_without_substitute(self):
        self.gesture.unlink()
        with self.assertRaisesRegex(SystemExit, 'Missing supplied'):
            self.run_prepare()
        self.assertFalse(self.calls)

    def test_wrong_identity_or_missing_load_data_rejected(self):
        for metadata in (dict(missing=[1], symbols=self.metadata['symbols']),
                         dict(missing=[], symbols=[])):
            self.metadata = metadata
            with self.assertRaisesRegex(SystemExit, 'Unsupported supplied'):
                self.run_prepare()
        self.assertFalse(self.calls)

    def test_unresolved_gestures_and_tls_rewrites_rejected(self):
        self.unresolved = ['3pcAvmwKCvM#m#m']
        with self.assertRaisesRegex(SystemExit, 'did not resolve'):
            self.run_prepare()
        self.unresolved = []
        self.patched = 1
        with self.assertRaisesRegex(SystemExit, 'Unexpected TLS rewrite'):
            self.run_prepare()

    def test_preserves_unexpected_output_file(self):
        self.out.mkdir()
        (self.out/'boot.bin').write_bytes(b'do not overwrite')
        with self.assertRaisesRegex(SystemExit, 'Preserving'):
            self.run_prepare()
        self.assertEqual((self.out/'boot.bin').read_bytes(), b'do not overwrite')
        self.assertFalse(self.calls)


if __name__ == '__main__':
    unittest.main()
