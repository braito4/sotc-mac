"""Offline build orchestration fixtures; no renderer, downloads or game input."""
import importlib.util
import json
import io
import os
import shutil
import platform
import shlex
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('build_gpu', Path(__file__).parents[1] / 'tools/build_gpu.py')
gpu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gpu)


class GpuBuildTests(unittest.TestCase):
    def test_untracked_and_ignored_compile_sources_are_rejected_without_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            subprocess.run(['git','init','-q',str(source)],check=True)
            directory=source/'gpu/shadps4';directory.mkdir(parents=True)
            tracked=directory/'normal.cpp';tracked.write_text('// tracked\n')
            subprocess.run(['git','-C',str(source),'add',str(tracked)],check=True)
            (directory/'notes.txt').write_text('not a compile input\n')
            gpu.reject_untracked_renderer_sources(source)
            duplicate=directory/'normal 2.cpp';duplicate.write_text('// preserved\n')
            (source/'.gitignore').write_text('* 2.cpp\n')
            with self.assertRaisesRegex(gpu.BuildError,'Untracked renderer source'):
                gpu.reject_untracked_renderer_sources(source)
            self.assertEqual(duplicate.read_text(),'// preserved\n')

    def test_staging_state_migrates_to_memory_observations(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            manifest = gpu.backport_manifest()
            patches = ''.join('\n'.join(json.loads((gpu.PATCH_DIR/name).read_text())['lines'])+'\n'
                              for name in manifest['staging_patches'])
            subprocess.run(['git', '-C', str(source), 'apply'], input=patches, text=True,
                           capture_output=True, check=True)
            self.assertEqual(gpu.backport_state(source), 'staging')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source), 'after')

    def test_visual_state_migrates_to_staging_reuse(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            manifest = gpu.backport_manifest()
            patches = ''.join('\n'.join(json.loads((gpu.PATCH_DIR/name).read_text())['lines'])+'\n'
                              for name in manifest['visual_patches'])
            subprocess.run(['git', '-C', str(source), 'apply'], input=patches, text=True,
                           capture_output=True, check=True)
            self.assertEqual(gpu.backport_state(source), 'visual')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source), 'after')

    def test_command_error_state_migrates_to_visual_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            manifest = gpu.backport_manifest()
            patches = ''.join('\n'.join(json.loads((gpu.PATCH_DIR/name).read_text())['lines'])+'\n'
                              for name in manifest['command_error_patches'])
            subprocess.run(['git', '-C', str(source), 'apply'], input=patches, text=True,
                           capture_output=True, check=True)
            self.assertEqual(gpu.backport_state(source), 'command_error')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source), 'after')

    def backport_fixture(self, root):
        for name in gpu.backport_manifest()['files']:
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((gpu.ROOT / 'runtime/upstream' / name).read_bytes())

    def test_backports_match_reviewed_outputs_and_are_idempotent(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            self.assertEqual(gpu.backport_state(source), 'before')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source), 'after')
            once = {name: (source / name).read_bytes() for name in gpu.backport_manifest()['files']}
            gpu.apply_backports(source)
            self.assertEqual(once, {name: (source / name).read_bytes() for name in once})

    def test_backports_reject_unknown_or_partial_sources_without_writing(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            name = next(iter(gpu.backport_manifest()['files']))
            path = source / name
            path.write_bytes(path.read_bytes() + b'\n')
            before = {n: (source / n).read_bytes() for n in gpu.backport_manifest()['files']}
            with self.assertRaises(gpu.BuildError):
                gpu.apply_backports(source)
            self.assertEqual(before, {n: (source / n).read_bytes() for n in before})
            self.backport_fixture(source)
            gpu.apply_backports(source)
            path.write_bytes((gpu.ROOT / 'runtime/upstream' / name).read_bytes())
            with self.assertRaisesRegex(gpu.BuildError, 'Incomplete'):
                gpu.apply_backports(source)

    def test_exact_previous_backport_set_migrates_without_reapplying_it(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            manifest = gpu.backport_manifest()
            patches = ''.join('\n'.join(json.loads((gpu.PATCH_DIR / name).read_text())['lines']) + '\n'
                              for name in manifest['previous_patches'])
            subprocess.run(['git', '-C', str(source), 'apply'], input=patches,
                           text=True, capture_output=True, check=True)
            self.assertEqual(gpu.backport_state(source), 'previous')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source), 'after')

    def test_pipeline_validity_state_migrates_to_shader_compatibility(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            manifest = gpu.backport_manifest()
            patches = ''.join('\n'.join(json.loads((gpu.PATCH_DIR / name).read_text())['lines']) + '\n'
                              for name in manifest['validity_patches'])
            subprocess.run(['git', '-C', str(source), 'apply'], input=patches,
                           text=True, capture_output=True, check=True)
            self.assertEqual(gpu.backport_state(source), 'validity')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source), 'after')

    def test_shader_compatibility_state_migrates_to_lookup_and_srt(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            manifest = gpu.backport_manifest()
            patches = ''.join('\n'.join(json.loads((gpu.PATCH_DIR / name).read_text())['lines']) + '\n'
                              for name in manifest['compatibility_patches'])
            subprocess.run(['git', '-C', str(source), 'apply'], input=patches,
                           text=True, capture_output=True, check=True)
            self.assertEqual(gpu.backport_state(source), 'compatibility')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source), 'after')

    def test_lookup_and_srt_state_migrates_to_metal_failure_check(self):
        with tempfile.TemporaryDirectory() as temporary:
            source=Path(temporary)
            self.backport_fixture(source)
            manifest=gpu.backport_manifest()
            patches=''.join('\n'.join(json.loads((gpu.PATCH_DIR/name).read_text())['lines'])+'\n'
                            for name in manifest['lookup_patches'])
            subprocess.run(['git','-C',str(source),'apply'],input=patches,text=True,capture_output=True,check=True)
            self.assertEqual(gpu.backport_state(source),'lookup')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source),'after')

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

    def test_original_three_backports_migrate_through_both_local_overlays(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.backport_fixture(source)
            manifest = gpu.backport_manifest()
            patches = ''.join('\n'.join(json.loads((gpu.PATCH_DIR / name).read_text())['lines']) + '\n'
                              for name in manifest['backport_patches'])
            subprocess.run(['git', '-C', str(source), 'apply'], input=patches,
                           text=True, capture_output=True, check=True)
            self.assertEqual(gpu.backport_state(source), 'backports')
            gpu.apply_backports(source)
            self.assertEqual(gpu.backport_state(source), 'after')

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

    @staticmethod
    def ffmpeg_metadata(build, prefix):
        (build / 'ffbuild').mkdir(parents=True)
        (build / 'ffbuild/config.sh').write_text('prefix=' + str(prefix) + '\nlibdir=${prefix}/lib\n')
        (build / 'ffbuild/config.mak').write_text(
            'prefix=' + str(prefix) + '\nINSTALL_NAME_DIR=' + str(prefix / 'lib') + '\n'
            'SLIBNAME_WITH_MAJOR=libfixture.1.dylib\nLIBVERSION=1.0.0\nLIBMAJOR=1\n'
            'SHFLAGS=-dynamiclib -Wl,-install_name,$(INSTALL_NAME_DIR)/$(SLIBNAME_WITH_MAJOR),'
            '-current_version,$(LIBVERSION),-compatibility_version,$(LIBMAJOR)\n')

    def test_ffmpeg_metadata_repair_is_idempotent_and_shell_readable(self):
        with tempfile.TemporaryDirectory(prefix='ffmpeg fixture ') as temporary:
            root = Path(temporary); prefix = root / 'prefix with spaces'; prefix.mkdir()
            build = root / 'build'; self.ffmpeg_metadata(build, prefix)
            self.assertEqual(gpu.repair_ffmpeg_paths(build, prefix), 2)
            self.assertEqual(gpu.repair_ffmpeg_paths(build, prefix), 0)
            result = subprocess.run(['/bin/sh', '-c', '. ffbuild/config.sh; printf "%s" "$prefix"'],
                                    cwd=build, capture_output=True, text=True, check=True)
            self.assertEqual(result.stdout, str(prefix))

    def test_unknown_ffmpeg_flags_leave_both_generated_files_unchanged(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); prefix = root / 'prefix with spaces'; prefix.mkdir()
            build = root / 'build'; self.ffmpeg_metadata(build, prefix)
            make = build / 'ffbuild/config.mak'; make.write_text(make.read_text().replace('SHFLAGS=', 'OTHER=').replace('-Wl,-install_name,', '-unexpected,'))
            before = {p: p.read_bytes() for p in (build / 'ffbuild').iterdir()}
            with self.assertRaises(gpu.BuildError):
                gpu.repair_ffmpeg_paths(build, prefix)
            self.assertEqual(before, {p: p.read_bytes() for p in before})

    def test_foreign_pkgconfig_prefix_does_not_modify_other_metadata(self):
        with tempfile.TemporaryDirectory(prefix='pkgconfig fixture ') as temporary:
            prefix = Path(temporary); directory = prefix / 'lib/pkgconfig'; directory.mkdir(parents=True)
            (directory / 'a.pc').write_text('prefix=' + str(prefix) + '\n')
            (directory / 'z.pc').write_text('prefix=/foreign/prefix\n')
            before = {p: p.read_bytes() for p in directory.iterdir()}
            with self.assertRaises(gpu.BuildError):
                gpu.normalize_pkgconfig_paths(prefix)
            self.assertEqual(before, {p: p.read_bytes() for p in before})

    @unittest.skipUnless(platform.system() == 'Darwin' and all(shutil.which(x) for x in ('clang', 'make', 'cmake', 'ninja', 'pkg-config', 'otool')), 'macOS build tools required')
    def test_spaced_ffmpeg_style_library_install_and_pkgconfig_consumer(self):
        with tempfile.TemporaryDirectory(prefix='ffmpeg link fixture ') as temporary:
            root = Path(temporary); prefix = root / 'installed dependency with spaces'; prefix.mkdir()
            build = root / 'build tree'; self.ffmpeg_metadata(build, prefix)
            (build / 'fixture.c').write_text('int fixture_value(void) { return 42; }\n')
            (build / 'Makefile').write_text('include ffbuild/config.mak\n'
                'libfixture.1.dylib: fixture.c\n\tclang $(SHFLAGS) -o $@ $<\n'
                'install: libfixture.1.dylib\n\tmkdir -p "$(prefix)/lib"\n\tcp $< "$(prefix)/lib/$<"\n')
            gpu.repair_ffmpeg_paths(build, prefix)
            result = subprocess.run(['make', 'install'], cwd=build, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            library = prefix / 'lib/libfixture.1.dylib'
            identity = subprocess.check_output(['otool', '-D', library], text=True)
            self.assertEqual(identity.splitlines()[1].strip(), str(library))
            (prefix / 'lib/libfixture.dylib').symlink_to(library.name)
            (prefix / 'include').mkdir()
            (prefix / 'include/fixture.h').write_text('int fixture_value(void);\n')
            metadata = prefix / 'lib/pkgconfig'; metadata.mkdir()
            (metadata / 'fixture.pc').write_text('prefix=' + str(prefix) + '\n'
                'libdir=' + str(prefix / 'lib') + '\nincludedir=' + str(prefix / 'include') + '\nName: fixture\nDescription: fixture\nVersion: 1\n'
                'Libs: -L${libdir} -Wl,-rpath,${libdir} -lfixture\nCflags: -I${includedir}\n')
            env = dict(os.environ, PKG_CONFIG_LIBDIR=str(metadata), PKG_CONFIG_PATH='')
            source = root / 'consumer source'; source.mkdir()
            (source / 'main.c').write_text('#include <fixture.h>\nint main(void) { return fixture_value() != 42; }\n')
            (source / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(consumer C)\n'
                'find_package(PkgConfig REQUIRED)\npkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET fixture)\n'
                'add_executable(consumer main.c)\ntarget_link_libraries(consumer PRIVATE PkgConfig::FFMPEG)\n')
            output = root / 'consumer build'
            failed = subprocess.run(['cmake', '-S', str(source), '-B', str(output), '-G', 'Ninja'],
                                    env=env, capture_output=True, text=True)
            self.assertNotEqual(failed.returncode, 0)  # Cache now contains the broken unescaped paths.
            self.assertEqual(gpu.normalize_pkgconfig_paths(prefix), 1)
            self.assertEqual(gpu.normalize_pkgconfig_paths(prefix), 0)
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'fixture'], env=env, text=True)
            self.assertIn('-I' + str(prefix / 'include'), shlex.split(flags))
            self.assertIn('-L' + str(prefix / 'lib'), shlex.split(flags))
            for args in (['cmake', *gpu.PKGCONFIG_CACHE_CLEAR, '-S', str(source), '-B', str(output), '-G', 'Ninja'],
                         ['cmake', '--build', str(output)], [str(output / 'consumer')]):
                result = subprocess.run(args, env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

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
