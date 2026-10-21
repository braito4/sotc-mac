"""Actual source guard regression, compiled without graphics dependencies."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module

class PipelineValidityTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and (ROOT / 'runtime/upstream/gpu/CMakeLists.txt').is_file(),
                         'native compiler and pinned source required')
    def test_original_bug_and_checked_overlay_guards(self):
        builder = load('validity_builder', ROOT / 'tools/build_gpu.py')
        check = load('validity_check', ROOT / 'tools/check_pipeline_validity.py')
        with tempfile.TemporaryDirectory(prefix='pipeline validity ') as temporary:
            root = Path(temporary); source = root / 'source'
            for name in builder.backport_manifest()['files']:
                target = source / name; target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((ROOT / 'runtime/upstream' / name).read_bytes())
            builder.apply_backports(source)
            check.check_sources(ROOT / 'runtime/upstream', source, root / 'output')
