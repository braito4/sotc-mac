"""Offline build orchestration fixtures; no renderer, downloads or game input."""
import importlib.util
import io
import os
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('build_gpu', Path(__file__).parents[1] / 'tools/build_gpu.py')
gpu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gpu)


class GpuBuildTests(unittest.TestCase):
    def test_checked_recipe_is_portable_and_excludes_app_runtimes(self):
        original = (gpu.ROOT / 'runtime/upstream/scripts/macos/build-deps.sh').read_bytes()
        recipe = gpu.prepare_recipe(original)
        self.assertEqual((gpu.ROOT / 'runtime/upstream/scripts/macos/build-deps.sh').read_bytes(), original)
        self.assertIn('cd -- "$SOTC_GPU_SOURCE"', recipe)
        self.assertIn('jobs=$SOTC_GPU_JOBS', recipe)
        self.assertIn('SPIRV_CROSS_VERSION=', recipe)
        self.assertIn('cmake_package fmt ', recipe)
        self.assertIn('if needed ffmpeg; then', recipe)
        self.assertNotIn('if needed app-bash;', recipe)
        self.assertNotIn('needed app-python;', recipe)
        self.assertNotIn('BASH_VERSINFO', recipe)
        self.assertNotIn('for tool in pkg-config glslangValidator nasm', recipe)
        result = subprocess.run(['/bin/bash', '-n'], input=recipe, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    @unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'), 'CMake/Ninja required')
    def test_generated_include_flag_builds_with_spaces_in_prefix(self):
        original = (gpu.ROOT / 'runtime/upstream/scripts/macos/build-deps.sh').read_bytes()
        recipe = gpu.prepare_recipe(original)
        flag_line = next(line.strip().removesuffix('\\').strip() for line in recipe.splitlines()
                         if '-DCMAKE_C_FLAGS=' in line)
        with tempfile.TemporaryDirectory(prefix='gpu build fixture ') as temporary:
            root = Path(temporary)
            prefix = root / 'dependency prefix with spaces'
            (prefix / 'include').mkdir(parents=True)
            (prefix / 'include/fixture.h').write_text('#define FIXTURE_VALUE 42\n')
            source = root / 'source tree'; source.mkdir()
            (source / 'fixture.c').write_text('#include <fixture.h>\nint main(void) { return FIXTURE_VALUE != 42; }\n')
            (source / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(fixture C)\nadd_executable(fixture fixture.c)\n')
            command = 'set -eu; set -- ' + flag_line + '; printf "%s" "$1"'
            result = subprocess.run(['/bin/bash', '-c', command], env=dict(os.environ, prefix=str(prefix)),
                                    capture_output=True, text=True, check=True)
            build = root / 'build tree'
            for args in (['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja', result.stdout],
                         ['cmake', '--build', str(build)], [str(build / 'fixture')]):
                checked = subprocess.run(args, capture_output=True, text=True)
                self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)

    def test_unknown_recipe_is_rejected(self):
        original = (gpu.ROOT / 'runtime/upstream/scripts/macos/build-deps.sh').read_bytes()
        for data in (b'', original + b'\n', original.replace(b'FMT_VERSION=12.0.0', b'FMT_VERSION=other')):
            with self.assertRaises(gpu.BuildError):
                gpu.prepare_recipe(data)

    def test_parallelism_is_bounded(self):
        for value in ('1', '2', '8'):
            self.assertEqual(gpu.parse_jobs(value), int(value))
        for value in ('0', '9', '-1', '', '2x', ' 2', '02', '\u0662', '9' * 5000):
            with self.assertRaises(gpu.BuildError):
                gpu.parse_jobs(value)

    def test_redirected_build_output_is_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            target = work / 'kept'; target.mkdir()
            sentinel = target / 'fixture'; sentinel.write_text('keep')
            (work / 'gpu-build').symlink_to(target, target_is_directory=True)
            with self.assertRaises(gpu.BuildError):
                gpu.private_path(work, 'gpu-build')
            self.assertEqual(sentinel.read_text(), 'keep')

    def test_failed_download_only_removes_its_own_staging_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            sentinel = work / 'keep'; sentinel.write_text('keep')
            calls = []
            def run(args):
                calls.append(args)
                if len(calls) == 1:
                    Path(args[-1]).mkdir()
                    (Path(args[-1]) / 'partial').write_text('fixture')
                else:
                    raise gpu.BuildError('synthetic checkout failure')
            with self.assertRaises(gpu.BuildError):
                gpu.fetch_source(work, work / 'gpu-source', mock.Mock(run=run))
            self.assertEqual(list(work.iterdir()), [sentinel])
            self.assertEqual(sentinel.read_text(), 'keep')

    def test_verified_source_is_reused_without_fetching_or_overwriting(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary); source = work / 'gpu-source'; source.mkdir()
            sentinel = source / 'fixture'; sentinel.write_text('keep')
            runner = mock.Mock()
            with mock.patch.object(gpu, 'verify_source') as verify:
                gpu.fetch_source(work, source, runner)
            verify.assert_called_once_with(source)
            runner.run.assert_not_called()
            self.assertEqual(sentinel.read_text(), 'keep')

    def test_foreign_existing_source_is_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary); source = work / 'gpu-source'; source.mkdir()
            sentinel = source / 'fixture'; sentinel.write_text('keep')
            runner = mock.Mock()
            with mock.patch.object(gpu, 'verify_source', side_effect=gpu.BuildError('wrong revision')):
                with self.assertRaises(gpu.BuildError):
                    gpu.fetch_source(work, source, runner)
            runner.run.assert_not_called()
            self.assertEqual(sentinel.read_text(), 'keep')

    def test_source_parent_checkout_and_wrong_revision_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / 'source'; source.mkdir()
            with mock.patch.object(gpu, 'git_output', return_value=str(source.parent)):
                with self.assertRaisesRegex(gpu.BuildError, 'standalone'):
                    gpu.verify_source(source)
            with mock.patch.object(gpu, 'git_output', side_effect=[str(source), 'wrong']):
                with self.assertRaisesRegex(gpu.BuildError, 'pinned'):
                    gpu.verify_source(source)

    def test_wrong_gitlink_is_rejected_even_with_existing_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            for status in ('-fixture gpu/third_party/imgui', '+fixture gpu/third_party/imgui', 'Ufixture recomp'):
                with mock.patch.object(gpu, 'git_output', side_effect=[str(source), gpu.REVISION, gpu.REPOSITORY, '', status]):
                    with self.assertRaisesRegex(gpu.BuildError, 'gitlinks'):
                        gpu.verify_source(source)

    def test_inventory_stays_read_only_when_dependencies_are_absent(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with mock.patch.object(gpu, 'ROOT', root), mock.patch.object(gpu.shutil, 'which', return_value=None), \
                 mock.patch.object(gpu, 'glslang_path', side_effect=gpu.BuildError('missing compiler')), \
                 mock.patch('sys.argv', ['build_gpu.py', '--check']), mock.patch('sys.stdout', new_callable=io.StringIO):
                self.assertEqual(gpu.main(), 2)
            self.assertEqual(list(root.iterdir()), [])

    def test_runner_surfaces_failure_and_records_output(self):
        log = io.StringIO()
        with mock.patch('sys.stdout', new_callable=io.StringIO):
            with self.assertRaises(gpu.BuildError):
                gpu.Runner(log).run(['/bin/sh', '-c', 'echo synthetic-fixture; exit 7'])
        self.assertIn('synthetic-fixture', log.getvalue())


if __name__ == '__main__':
    unittest.main()
