"""Prevent missing Apple tooling from being reported as a shader pass/failure."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import check_pervertex_proposal as proposal


class MetalValidationTests(unittest.TestCase):
    def run_check(self, responses, sources=True):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)
            if sources:
                for name in ('producer', 'replay', 'fragment-1', 'fragment-2', 'fragment-3', 'fragment-4'):
                    (output/(name+'.metal')).write_text('synthetic source')
            with mock.patch.dict(proposal.os.environ,
                                 {'DEVELOPER_DIR': '/Applications/Xcode.app/Contents/Developer'}, clear=True), \
                 mock.patch.object(proposal.subprocess, 'run', side_effect=responses):
                return proposal.check_metal_syntax(output)

    @staticmethod
    def result(code=0, text=''):
        return subprocess.CompletedProcess([], code, stdout=text, stderr='')

    def test_missing_component_is_blocked(self):
        report=self.run_check([self.result(text='/usr/bin/true\n'),
                               self.result(1, 'missing Metal Toolchain')])
        self.assertEqual(report['status'], 'blocked')
        self.assertIn('missing Metal Toolchain', report['discovery'][0]['version_diagnostic'])

    def test_six_sources_must_compile(self):
        report=self.run_check([self.result(text='/usr/bin/true\n'), self.result(text='Metal compiler'),
                               *[self.result() for _ in range(6)]])
        self.assertEqual(report['status'], 'pass')
        self.assertEqual(len(report['shaders']), 6)

    def test_bad_shader_is_a_failure(self):
        report=self.run_check([self.result(text='/usr/bin/true\n'), self.result(),
                               self.result(1, 'syntax error'), *[self.result() for _ in range(5)]])
        self.assertEqual(report['status'], 'failed')

    def test_empty_output_is_not_a_pass(self):
        report=self.run_check([self.result(text='/usr/bin/true\n'), self.result()], sources=False)
        self.assertEqual(report['status'], 'failed')


if __name__ == '__main__':
    unittest.main()
