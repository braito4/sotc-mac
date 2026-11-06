"""Public, synthetic provenance regressions. No game or downloaded sources."""
import copy
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import host_build_manifest as host


class HostManifestTests(unittest.TestCase):
    def fixture(self, root):
        path = root / 'src/cpu/cpu.c'
        path.parent.mkdir(parents=True)
        path.write_bytes(b'reviewed CPU')
        return {'repository': 'https://example.invalid/cpu', 'revision': '1' * 40,
                'files': {'src/cpu/cpu.c': host.digest(path)}}

    def test_modified_source_rejected_despite_unchanged_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            component = self.fixture(root)
            host.verify_component(root, component, cpu=True)
            (root / 'src/cpu/cpu.c').write_bytes(b'unreviewed CPU')
            with self.assertRaises(ValueError):
                host.verify_component(root, component, cpu=True)

    def test_extra_globbed_source_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            component = self.fixture(root)
            (root / 'src/cpu/conflict.c').write_text('duplicate')
            with self.assertRaises(ValueError):
                host.verify_component(root, component, cpu=True)

    def test_missing_source_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            component = self.fixture(root)
            (root / 'src/cpu/cpu.c').unlink()
            with self.assertRaises(ValueError):
                host.verify_component(root, component, cpu=True)

    def test_identical_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            component = self.fixture(root)
            path = root / 'src/cpu/cpu.c'
            path.rename(root / 'outside.c')
            path.symlink_to(root / 'outside.c')
            with self.assertRaises(ValueError):
                host.verify_component(root, component, cpu=True)

    def test_change_during_build_cannot_be_promoted(self):
        with self.assertRaises(ValueError):
            host.finalize({'cpu': 'old'}, {'cpu': 'new'}, Path('not-accessed'))

    def test_build_report_identifies_binary_not_gameplay(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'executable'
            path.write_bytes(b'synthetic executable')
            inputs = {'gameplay_validation': 'not established by this build'}
            with patch.object(host.subprocess, 'check_output', return_value='synthetic clang'):
                report = host.finalize(copy.deepcopy(inputs), inputs, path)
            self.assertEqual(report['executable']['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertEqual(report['gameplay_validation'], inputs['gameplay_validation'])


if __name__ == '__main__':
    unittest.main()
