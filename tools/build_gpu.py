"""Build the pinned GPU library in private work output, without game input.

Uses bloodborne-native/bloodborne-runtime's pinned renderer with reviewed, checked
upstream backports and a checked
adaptation of its dependency recipes (bbport and shadPS4; GPL-2.0-or-later). The prepared recipe
omits app-bundle Bash/Python and x86-only assembler prerequisites on ARM64.
It does not launch the renderer or claim a game frame.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
REVISION = 'b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548'
REPOSITORY = 'https://github.com/bloodborne-native/bloodborne-runtime.git'
DEPS_SHA256 = '8866a556db9d2ff2eaec245157d293ca9bd000f4b98d588f7370dd5bdb5e96ef'
SUBMODULES = ('gpu/third_party/fsr-vulkan', 'gpu/third_party/imgui', 'recomp')
REQUIRED_FILES = (
    'gpu/CMakeLists.txt', 'gpu/bbgpu.h', 'scripts/macos/build-deps.sh',
    'gpu/third_party/fsr-vulkan/CMakeLists.txt', 'gpu/third_party/imgui/imgui.h',
    'gpu/third_party/imgui/imgui.cpp', 'recomp/src/cpu/bbcpu.h',
)
REQUIRED_EXPORTS = ('bbgpu_init', 'bbgpu_register_kernel', 'bbgpu_resolve',
                    'bbgpu_window_loop', 'bbgpu_handle_fault', 'bbgpu_symbol_count')
PKGCONFIG_CACHE_CLEAR = tuple(
    item for name in ('FFMPEG', 'SDL3', 'XXHASH') for item in ('-U', '*' + name + '*')
)
TOOLS = ('git', 'clang', 'clang++', 'cmake', 'ninja', 'pkg-config', 'curl', 'make', 'lipo', 'nm')
PATCH_DIR = Path(__file__).resolve().parent / 'gpu_patches'


def source_path(root=ROOT):
    return private_path(root / 'work', 'gpu-source-' + dependency_lock()['workspace_key'])


def build_path(root=ROOT):
    return private_path(root / 'work', 'gpu-build-' + dependency_lock()['workspace_key'])


def dependency_lock():
    data = json.loads((Path(__file__).resolve().parent / 'dependencies.json').read_text())
    if data['runtime']['revision'] != REVISION:
        raise BuildError('Dependency lock and renderer revision disagree.')
    if not re.fullmatch(r'[0-9a-f]{12}', data['workspace_key']):
        raise BuildError('Invalid renderer workspace key.')
    return data


class BuildError(RuntimeError):
    pass


def backport_manifest():
    manifest = json.loads((PATCH_DIR / 'manifest.json').read_text())
    if manifest['base_revision'] != REVISION:
        raise BuildError('GPU backports do not match the pinned base.')
    for name, digest in manifest['patches'].items():
        path = PATCH_DIR / name
        if path.parent != PATCH_DIR or path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise BuildError('GPU backport patch failed its digest check.')
    return manifest


def backport_state(source):
    manifest = backport_manifest()
    states = {'before', 'backports', 'previous', 'validity', 'compatibility', 'lookup', 'command_error', 'visual', 'staging', 'memory', 'after'}
    for name, digests in manifest['files'].items():
        path = source / name
        if path.is_symlink() or not path.resolve().is_relative_to(source.resolve()):
            raise BuildError('Refusing redirected GPU backport source.')
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        matches = {state for state in states if digest == digests.get(state)}
        if not any(digest == value for value in digests.values()):
            raise BuildError('Unrecognized GPU backport source: ' + name)
        states &= matches
    if not states:
        raise BuildError('Incomplete GPU backport set; preserving it unchanged.')
    return next(state for state in ('after', 'memory', 'staging', 'visual', 'command_error', 'lookup', 'compatibility', 'validity', 'previous', 'backports', 'before') if state in states)


def apply_backports(source):
    state = backport_state(source)
    if state == 'after':
        apply_submodule_overrides(source)
        return
    # JSON lines preserve significant unified-diff blank context without adding
    # trailing whitespace to the public source files.
    manifest = backport_manifest()
    applied = manifest.get(state + '_patches', ())
    names = [name for name in manifest['patches'] if name not in applied]
    patches = ''.join('\n'.join(json.loads((PATCH_DIR / name).read_text())['lines']) + '\n'
                      for name in names)
    try:
        subprocess.run(['git', '-C', str(source), 'apply', '--check'], check=True,
                       input=patches, capture_output=True, text=True)
        subprocess.run(['git', '-C', str(source), 'apply'], check=True,
                       input=patches, capture_output=True, text=True)
    except subprocess.CalledProcessError as error:
        raise BuildError('Checked GPU backports could not be applied: ' + error.stderr) from error
    if backport_state(source) != 'after':
        raise BuildError('GPU backport output did not match the reviewed digest.')
    apply_submodule_overrides(source)


def apply_submodule_overrides(source):
    # Synthetic file-only builder fixtures have no checkout to update.
    if not (source / '.git').exists():
        return
    for name, override in backport_manifest().get('submodule_overrides', {}).items():
        child = source / name
        current = git_output(child, 'rev-parse', 'HEAD')
        if current not in (override['base'], override['after']):
            raise BuildError('Unknown renderer submodule revision: ' + name)
        if git_output(child, 'status', '--porcelain', '--untracked-files=no'):
            raise BuildError('Preserving modified renderer submodule: ' + name)
        if current != override['after']:
            subprocess.run(['git', '-C', str(child), 'checkout', '--detach', override['after']], check=True)


def prepare_recipe(data):
    """A checked build-only transformation; never edit the source checkout."""
    if hashlib.sha256(data).hexdigest() != DEPS_SHA256:
        raise BuildError('Unexpected upstream dependency recipe; refusing an unchecked transformation.')
    source = data.decode('utf8')
    start = source.index('set -euo pipefail\n')
    end = source.index('# bash for the app bundle:')
    body = source[start:end]
    replacements = {
        'cd -- "$(dirname -- "$0")/../.."': 'cd -- "$SOTC_GPU_SOURCE"',
        'jobs=$(sysctl -n hw.ncpu)': 'jobs=$SOTC_GPU_JOBS',
        '        make -j "$jobs" >/dev/null && make install >/dev/null)':
            '        python3 "$SOTC_GPU_BUILDER" --repair-ffmpeg "$PWD" "$prefix" &&\n'
            '        make -j "$jobs" >/dev/null && make install >/dev/null)',
        # CMake parses compiler flags again after shell argument expansion.
        '-DCMAKE_C_FLAGS="-I$prefix/include"':
            '-DCMAKE_C_FLAGS="-I\\"$prefix/include\\""',
        'for tool in pkg-config glslangValidator nasm cmake ninja; do':
            'for tool in pkg-config glslangValidator cmake ninja; do',
    }
    for old, new in replacements.items():
        if body.count(old) != 1:
            raise BuildError('Unexpected dependency recipe structure.')
        body = body.replace(old, new)
    packages = dependency_lock()['packages']
    for package in packages:
        variable = package['recipe_variable']
        if variable is None:
            continue
        version = package['version']
        if not re.fullmatch(r'[A-Za-z0-9_.+-]+', version):
            raise BuildError('Invalid dependency version in lock.')
        if package['recipe_version'] is None:
            if variable != 'SPIRV_HEADERS_VERSION':
                raise BuildError('Unreviewed additional dependency: ' + variable)
            body = body.replace('VULKAN_VERSION=', variable + '=' + version + '\nVULKAN_VERSION=', 1)
        else:
            old = variable + '=' + package['recipe_version'] + '\n'
            if body.count(old) != 1:
                raise BuildError('Dependency recipe version drift: ' + variable)
            body = body.replace(old, variable + '=' + version + '\n')
    body = body.replace('cmake_package vulkan-headers "$(gh KhronosGroup/Vulkan-Headers $VULKAN_VERSION)"',
                        'cmake_package vulkan-headers "$(gh KhronosGroup/Vulkan-Headers $VULKAN_VERSION)"\n'
                        'cmake_package spirv-headers "$(gh KhronosGroup/SPIRV-Headers $SPIRV_HEADERS_VERSION)" -DSPIRV_HEADERS_ENABLE_TESTS=OFF')
    if body.count('$work/src/xxhash/cmake_unofficial') != 1:
        raise BuildError('Unexpected xxHash CMake source path.')
    body = body.replace('$work/src/xxhash/cmake_unofficial', '$work/src/xxhash/build/cmake')
    versions = {package['id']: package['version'] for package in packages}
    lock_key = hashlib.sha256(json.dumps(versions, sort_keys=True).encode()).hexdigest()[:12]
    old_work = 'work=$root/deps/build$([[ $arch == arm64 ]] && echo -arm64 || true)'
    if body.count(old_work) != 1:
        raise BuildError('Unexpected dependency cache layout.')
    body = body.replace(old_work, old_work + '-' + lock_key)
    cases = '\n'.join('        ' + name + ') echo ' + shlex.quote(version) + ' ;;'
                      for name, version in versions.items())
    old_mark = 'done_mark() { echo "$prefix/.built-$1"; }'
    body = body.replace(old_mark, 'package_version() {\n    case "$1" in\n' + cases +
                        '\n        *) echo unversioned ;;\n    esac\n}\n' + old_mark)
    body = body.replace('[[ ! -f $(done_mark "$1") ]]',
                        '[[ ! -f $(done_mark "$1") || $(cat "$(done_mark "$1")") != "$(package_version "$1")" ]]')
    body = body.replace('touch "$(done_mark "$1")";',
                        'printf \'%s\\n\' "$(package_version "$1")" > "$(done_mark "$1")";')
    # Each lock has an independent archive/source/build cache. Empty old marker
    # files never certify a newly selected version.
    return ('#!/bin/bash\n# Prepared from the pinned upstream dependency recipe.\n'
            '# ARM64 renderer dependencies only; no app bundle runtimes.\n' + body +
            '\necho "GPU dependencies are in $prefix"\n')


def repair_ffmpeg_paths(build_dir, prefix):
    """Repair generated FFmpeg metadata, not FFmpeg's source or configure."""
    build_dir, prefix = Path(build_dir).resolve(), str(Path(prefix).absolute())
    shell = build_dir / 'ffbuild/config.sh'
    make = build_dir / 'ffbuild/config.mak'
    for path in (shell, make):
        if path.is_symlink() or not path.resolve().is_relative_to(build_dir):
            raise BuildError('Refusing redirected FFmpeg configuration: ' + str(path))
    shell_data, make_data = shell.read_text(), make.read_text()
    raw = 'prefix=' + prefix
    quoted = 'prefix=' + shlex.quote(prefix)
    lines = shell_data.splitlines(keepends=True)
    matches = [i for i, line in enumerate(lines) if line.startswith('prefix=')]
    if len(matches) != 1 or lines[matches[0]].rstrip('\n') not in (raw, quoted):
        raise BuildError('Unexpected FFmpeg shell prefix; configuration left unchanged.')
    lines[matches[0]] = quoted + '\n'
    repaired_shell = ''.join(lines)
    if make_data.splitlines().count(raw) != 1:
        raise BuildError('Unexpected FFmpeg make prefix; configuration left unchanged.')
    token = '-Wl,-install_name,$(INSTALL_NAME_DIR)/$(SLIBNAME_WITH_MAJOR),'
    fixed = '-Wl,-install_name,"$(INSTALL_NAME_DIR)/$(SLIBNAME_WITH_MAJOR)",'
    if make_data.count(token) == 1 and not make_data.count(fixed):
        repaired_make = make_data.replace(token, fixed)
    elif make_data.count(fixed) == 1 and not make_data.count(token):
        repaired_make = make_data
    else:
        raise BuildError('Unexpected FFmpeg install-name linker flags; configuration left unchanged.')
    changed = 0
    for path, before, after in ((shell, shell_data, repaired_shell), (make, make_data, repaired_make)):
        if before != after:
            path.write_text(after)
            changed += 1
    return changed


def normalize_pkgconfig_paths(prefix):
    """Escape the installation prefix for pkg-config's separate flag parser."""
    prefix = Path(prefix).absolute()
    raw = str(prefix)
    escaped = raw.replace('\\', '\\\\').replace(' ', '\\ ').replace('\t', '\\\t')
    changes = []
    for directory in (prefix / 'lib/pkgconfig', prefix / 'share/pkgconfig'):
        if directory.is_symlink():
            raise BuildError('Refusing redirected pkg-config directory.')
        for path in sorted(directory.glob('*.pc')):
            if path.is_symlink() or not path.resolve().is_relative_to(prefix.resolve()):
                raise BuildError('Refusing redirected pkg-config metadata.')
            data = path.read_text()
            lines = data.splitlines(keepends=True)
            matches = [i for i, line in enumerate(lines) if line.startswith('prefix=')]
            if not matches and path.name == 'SPIRV-Headers.pc':
                # This header-only package emits an absolute includedir and no
                # prefix. Normalize only its exact expected installation path.
                expected = 'includedir=' + raw + '/include\n'
                if lines.count(expected) != 1:
                    raise BuildError('Unexpected SPIRV-Headers include directory.')
                lines[lines.index(expected)] = 'includedir=${prefix}/include\n'
                lines.insert(0, 'prefix=' + escaped + '\n')
                changes.append((path, ''.join(lines)))
                continue
            if len(matches) != 1 or lines[matches[0]].rstrip('\n') not in ('prefix=' + raw, 'prefix=' + escaped):
                raise BuildError('Unexpected installed pkg-config prefix: ' + path.name)
            lines[matches[0]] = 'prefix=' + escaped + '\n'
            for i, line in enumerate(lines):
                if line.partition('=')[0] in ('exec_prefix', 'libdir', 'includedir'):
                    lines[i] = line.replace(raw, escaped)
            updated = ''.join(lines)
            if updated != data:
                changes.append((path, updated))
    for path, data in changes:
        path.write_text(data)
    return len(changes)


def parse_jobs(value):
    if value not in tuple('12345678'):
        raise BuildError('SOTC_GPU_JOBS must be an integer from 1 through 8.')
    return int(value)


def private_path(work, name):
    path = work / name
    if path.is_symlink() or (path.exists() and path.resolve().parent != work.resolve()):
        raise BuildError(f'Refusing redirected private build path: {path}')
    return path


def missing_files(source):
    return [name for name in REQUIRED_FILES if not (source / name).is_file()]


def glslang_path():
    configured = os.environ.get('SOTC_GLSLANG')
    if configured:
        path = Path(configured).expanduser().resolve()
        if not path.is_file() or not os.access(path, os.X_OK):
            raise BuildError('SOTC_GLSLANG must name an executable glslang compiler.')
        return path
    local = ROOT / 'work/host-tools/prefix/bin/glslang'
    if local.is_file() and os.access(local, os.X_OK):
        return local.resolve()
    for name in ('glslangValidator', 'glslang'):
        found = shutil.which(name)
        if found:
            return Path(found).resolve()
    # Reuse a host build tool when the related local shadPS4 checkout exists.
    candidate = ROOT.parent / 'shadPS4_SOTC-macos-arm64/build-arm64/externals/glslang/StandAlone/glslang'
    if candidate.is_file() and os.access(candidate, os.X_OK):
        return candidate.resolve()
    raise BuildError('Missing glslang: install it or set SOTC_GLSLANG to an existing compiler.')


class Runner:
    def __init__(self, log):
        self.log = log

    def run(self, args, *, env=None):
        args = list(map(str, args))
        line = '+ ' + shlex.join(args)
        print(line, flush=True)
        self.log.write(line + '\n'); self.log.flush()
        with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, env=env) as process:
            for line in process.stdout:
                print(line, end='', flush=True)
                self.log.write(line); self.log.flush()
            status = process.wait()
        if status:
            raise BuildError(f'Command failed with exit {status}: {args[0]}')


def git_output(source, *args):
    return subprocess.check_output(['git', '-C', str(source), *args], text=True,
                                   stderr=subprocess.PIPE).strip()


def verify_source(source):
    try:
        if Path(git_output(source, 'rev-parse', '--show-toplevel')).resolve() != source.resolve():
            raise BuildError('GPU source is not a standalone Git checkout.')
        if git_output(source, 'rev-parse', 'HEAD') != REVISION:
            raise BuildError('GPU source is not at the required pinned revision; preserving it unchanged.')
        if git_output(source, 'remote', 'get-url', 'origin') != REPOSITORY:
            raise BuildError('Unexpected GPU source remote; preserving it unchanged.')
        if git_output(source, 'status', '--porcelain', '--untracked-files=no'):
            changed = set(git_output(source, 'diff', '--name-only', 'HEAD', '--').splitlines())
            state = backport_state(source)
            expected = {name for name, digests in backport_manifest()['files'].items()
                        if digests[state] != digests['before']}
            for name, override in backport_manifest().get('submodule_overrides', {}).items():
                if git_output(source / name, 'rev-parse', 'HEAD') == override['after']:
                    expected.add(name)
            if state not in ('backports', 'previous', 'validity', 'compatibility', 'lookup', 'command_error', 'visual', 'staging', 'memory', 'after') or changed != expected:
                raise BuildError('GPU checkout has unrecognized tracked modifications; preserving it unchanged.')
    except subprocess.CalledProcessError as error:
        raise BuildError('Cannot verify the isolated GPU source checkout.') from error
    # A gitlink marked uninitialized is not a complete source checkout.
    status = git_output(source, 'submodule', 'status', '--recursive', '--', *SUBMODULES)
    overrides = backport_manifest().get('submodule_overrides', {})
    for line in status.splitlines():
        if not line:
            continue
        if line[0] in '-U':
            raise BuildError('GPU submodules are missing or do not match the pinned gitlinks.')
        if line[0] == '+':
            revision, name, *_ = line[1:].split()
            if name not in overrides or revision != overrides[name]['after']:
                raise BuildError('GPU submodule does not match its reviewed override: ' + name)
    for name, override in overrides.items():
        child = source / name
        if git_output(child, 'rev-parse', 'HEAD') not in (override['base'], override['after']):
            raise BuildError('Unrecognized renderer submodule: ' + name)
        if git_output(child, 'status', '--porcelain', '--untracked-files=no'):
            raise BuildError('Modified renderer submodule: ' + name)
    absent = missing_files(source)
    if absent:
        raise BuildError('Incomplete GPU source: ' + ', '.join(absent))
    reject_untracked_renderer_sources(source)


def reject_untracked_renderer_sources(source):
    # CMake globbing can otherwise compile sync/conflict copies alongside checked files.
    # Include ignored files: ignoring a source does not keep it out of CMake's glob.
    names = git_output(source, 'ls-files', '--others', '-z', '--',
                       'gpu/shadps4', 'gpu/shim', 'gpu/layer', 'gpu/cp', 'gpu/games').split('\0')
    unexpected = [name for name in names if Path(name).suffix.lower() in
                  ('.c', '.cc', '.cpp', '.cxx', '.m', '.mm', '.h', '.hpp', '.inl')]
    if unexpected:
        raise BuildError('Untracked renderer source may enter the build; files preserved: ' + ', '.join(unexpected))


def fetch_source(work, source, runner):
    if source.exists():
        verify_source(source)
        return
    stage = Path(tempfile.mkdtemp(prefix='gpu-fetch-', dir=work))
    try:
        checkout = stage / 'source'
        runner.run(['git', 'clone', '--no-checkout', REPOSITORY, checkout])
        runner.run(['git', '-C', checkout, 'checkout', '--detach', REVISION])
        runner.run(['git', '-C', checkout, 'submodule', 'update', '--init', '--recursive', '--', *SUBMODULES])
        verify_source(checkout)
        if source.exists() or source.is_symlink():
            raise BuildError('GPU source destination appeared during fetch; preserving it unchanged.')
        checkout.rename(source)
    finally:
        # Only this fresh temporary download is removed, never a user's checkout.
        shutil.rmtree(stage)


def build(root, runner, jobs, compiler):
    work = root / 'work'
    source = source_path(root)
    fetch_source(work, source, runner)
    apply_backports(source)
    generated = private_path(work, 'gpu-dependencies.sh')
    for name in ('deps', 'deps/macos-arm64', 'deps/build-arm64'):
        if (source / name).is_symlink():
            raise BuildError('Refusing redirected GPU dependency output: ' + name)
    recipe = prepare_recipe((source / 'scripts/macos/build-deps.sh').read_bytes())
    generated.write_text(recipe)
    tool_dir = private_path(work, 'gpu-tools-' + REVISION[:12])
    tool_dir.mkdir(exist_ok=True)
    alias = tool_dir / 'glslangValidator'
    if alias.is_symlink():
        if alias.resolve() != compiler:
            raise BuildError('Existing glslang alias points at a different compiler.')
    elif alias.exists():
        raise BuildError('Preserving an unexpected existing glslang tool.')
    else:
        alias.symlink_to(compiler)
    env = dict(os.environ, BB_ARCH='arm64', SOTC_GPU_SOURCE=str(source), SOTC_GPU_JOBS=str(jobs),
               SOTC_GPU_BUILDER=str(Path(__file__).resolve()))
    env.pop('BB_DEPS_REBUILD', None)  # Rebuild requests belong to the dependency maintainer.
    env['PATH'] = str(tool_dir) + os.pathsep + env.get('PATH', '')
    runner.run(['/bin/bash', generated], env=env)
    prefix = source / 'deps/macos-arm64'
    repaired = normalize_pkgconfig_paths(prefix)
    print(f'Normalized spaced pkg-config prefixes in {repaired} installed metadata files.', flush=True)
    env['PKG_CONFIG_LIBDIR'] = str(prefix / 'lib/pkgconfig') + os.pathsep + str(prefix / 'share/pkgconfig')
    env['PKG_CONFIG_PATH'] = ''
    env['PATH'] = str(prefix / 'bin') + os.pathsep + env['PATH']
    output = build_path(root)
    runner.run(['cmake', *PKGCONFIG_CACHE_CLEAR, '-S', source / 'gpu', '-B', output, '-G', 'Ninja',
                '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0',
                '-DCMAKE_BUILD_TYPE=RelWithDebInfo', '-DBB_LTO=OFF', '-DBB_PGO=off',
                '-DSOTC_DIAGNOSTIC_INCLUDE_DIR=' + str(root / 'runtime/src'),
                '-DCMAKE_PREFIX_PATH=' + str(prefix), '-DGLSLANG_COMPILER=' + str(compiler)], env=env)
    runner.run(['cmake', '--build', output, '--target', 'bbgpu', '--parallel', str(jobs)], env=env)
    library = output / 'libbbgpu.dylib'
    architecture = subprocess.check_output(['lipo', '-archs', library], text=True).strip()
    if architecture != 'arm64':
        raise BuildError('Built GPU library is not ARM64-only.')
    exports = subprocess.check_output(['nm', '-gU', library], text=True)
    symbols = {line.split()[-1] for line in exports.splitlines() if line.split()}
    if any('_' + name not in symbols for name in REQUIRED_EXPORTS):
        raise BuildError('Built GPU library does not export the required loader ABI.')
    verify_source(source)
    runner.run([sys.executable, root / 'tools/check_tessellation.py'])
    runner.run([sys.executable, root / 'tools/check_pipeline_validity.py'])
    runner.run([sys.executable, root / 'tools/check_buffer_copy_chunks.py'])
    runner.run([sys.executable, root / 'tools/check_staging_reuse.py'])
    runner.run([sys.executable, root / 'tools/check_memory_trace.py'])
    runner.run([sys.executable, root / 'tools/check_scene_proxy_retirement.py'])
    runner.run([sys.executable, root / 'tools/check_nested_task_lifetime.py'])
    runner.run([sys.executable, root / 'tools/check_collection_safety.py'])
    runner.run([sys.executable, root / 'tools/check_shader_compat.py'])
    undefined = subprocess.check_output(['nm', '-u', library], text=True)
    runtime_symbols = sorted({line.split()[-1] for line in undefined.splitlines() if line.split()
                              and line.split()[-1].startswith(('_runtime_', '_bbcpu_', '_bb_image_base'))})
    report = dict(upstream=REPOSITORY, revision=REVISION, architecture=architecture,
                  library=str(library), dependencies=str(prefix), jobs=jobs, runtime_symbols=runtime_symbols,
                  macos_minimum='15.0',
                  dependency_recipe_sha256=DEPS_SHA256,
                  renderer_backports=backport_manifest()['upstream_commits'],
                  renderer_local_adaptations=backport_manifest().get('local_adaptations', []),
                  renderer_workspace_key=dependency_lock()['workspace_key'],
                  dependency_versions={item['id']:item['version'] for item in dependency_lock()['packages']},
                  shader_binary_version=24, shader_meta_version=9,
                  renderer_launched=False, game_frame_verified=False)
    private_path(work, 'gpu-build.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'GPU library built: {library}\nNo renderer was launched; loader integration and game video are still pending.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Read-only inventory; no network, downloads or builds.')
    parser.add_argument('--fetch', action='store_true', help='Fetch the exact GPU source and submodules only.')
    parser.add_argument('--repair-ffmpeg', nargs=2, metavar=('BUILD', 'PREFIX'),
                        help='Repair generated FFmpeg shell/linker metadata before make.')
    args = parser.parse_args()
    if args.repair_ffmpeg:
        if args.check or args.fetch:
            parser.error('FFmpeg metadata repair is a separate stage.')
        count = repair_ffmpeg_paths(*args.repair_ffmpeg)
        print(f'Repaired generated FFmpeg paths in {count} configuration files.')
        return 0
    if args.check and args.fetch:
        parser.error('Choose --check or --fetch, not both.')
    work = ROOT / 'work'
    if work.is_symlink():
        raise BuildError('Refusing a redirected work directory.')
    jobs = parse_jobs(os.environ.get('SOTC_GPU_JOBS', '2'))
    needed_tools = ('git',) if args.fetch else TOOLS
    missing = [name for name in needed_tools if not shutil.which(name)]
    compiler = None
    compiler_error = None
    if not args.fetch:
        try:
            compiler = glslang_path()
        except BuildError as error:
            compiler_error = str(error)
    if args.check:
        source = source_path(ROOT)
        print('Target: native ARM64; maximum compiler jobs:', jobs)
        print('Pinned renderer revision:', REVISION)
        print('glslang:', compiler if compiler else compiler_error)
        print('Missing build tools:', ', '.join(missing) or 'none')
        print('Isolated renderer sources:', 'present; revision verified during build' if not missing_files(source) else 'not complete')
        print('Reference snapshot missing:', ', '.join(missing_files(ROOT / 'runtime/upstream')) or 'none')
        print('Read-only check: no network attempted, no renderer built or launched.')
        return 2 if missing or compiler_error or missing_files(source) else 0
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        raise BuildError('Run from a native ARM64 macOS session; Rosetta builds are not supported.')
    if missing:
        raise BuildError('Missing build tools: ' + ', '.join(missing))
    if compiler_error:
        raise BuildError(compiler_error)
    work.mkdir(exist_ok=True)
    logs = ROOT / 'logs'
    if logs.is_symlink():
        raise BuildError('Refusing a redirected log directory.')
    logs.mkdir(exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix='gpu-build-' + time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()) + '-',
                                        suffix='.log', dir=logs)
    path = Path(name)
    print('Private build log:', path, flush=True)
    with os.fdopen(descriptor, 'w') as log:
        runner = Runner(log)
        if args.fetch:
            fetch_source(work, source_path(ROOT), runner)
            print('Pinned source and renderer submodules ready; no libraries were built.')
        else:
            build(ROOT, runner, jobs, compiler)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (BuildError, OSError, subprocess.CalledProcessError) as error:
        print('GPU build stopped:', error, file=sys.stderr)
        raise SystemExit(1)
