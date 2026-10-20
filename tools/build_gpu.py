"""Build the pinned GPU library in private work output, without game input.

Uses bloodborne-native/bloodborne-runtime's unchanged renderer and dependency
recipes (bbport and shadPS4 foundations; GPL-2.0-or-later). The prepared recipe
omits app-bundle Bash/Python and x86-only assembler prerequisites on ARM64.
It does not launch the renderer or claim a game frame.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
REVISION = '3170535f59cadbe5ce9c38e6bb961ed5fd215194'
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
TOOLS = ('git', 'clang', 'clang++', 'cmake', 'ninja', 'pkg-config', 'curl', 'make', 'lipo', 'nm')


class BuildError(RuntimeError):
    pass


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
        'for tool in pkg-config glslangValidator nasm cmake ninja; do':
            'for tool in pkg-config glslangValidator cmake ninja; do',
    }
    for old, new in replacements.items():
        if body.count(old) != 1:
            raise BuildError('Unexpected dependency recipe structure.')
        body = body.replace(old, new)
    return ('#!/bin/bash\n# Prepared from the pinned upstream dependency recipe.\n'
            '# ARM64 renderer dependencies only; no app bundle runtimes.\n' + body +
            '\necho "GPU dependencies are in $prefix"\n')


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
            raise BuildError('GPU checkout has tracked modifications; preserving it unchanged.')
    except subprocess.CalledProcessError as error:
        raise BuildError('Cannot verify the isolated GPU source checkout.') from error
    # A gitlink marked uninitialized is not a complete source checkout.
    status = git_output(source, 'submodule', 'status', '--recursive', '--', *SUBMODULES)
    if any(line and line[0] in '-+U' for line in status.splitlines()):
        raise BuildError('GPU submodules are missing or do not match the pinned gitlinks.')
    absent = missing_files(source)
    if absent:
        raise BuildError('Incomplete GPU source: ' + ', '.join(absent))


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
    source = private_path(work, 'gpu-source')
    fetch_source(work, source, runner)
    generated = private_path(work, 'gpu-dependencies.sh')
    for name in ('deps', 'deps/macos-arm64', 'deps/build-arm64'):
        if (source / name).is_symlink():
            raise BuildError('Refusing redirected GPU dependency output: ' + name)
    recipe = prepare_recipe((source / 'scripts/macos/build-deps.sh').read_bytes())
    generated.write_text(recipe)
    tool_dir = private_path(work, 'gpu-tools')
    tool_dir.mkdir(exist_ok=True)
    alias = tool_dir / 'glslangValidator'
    if alias.is_symlink():
        if alias.resolve() != compiler:
            raise BuildError('Existing glslang alias points at a different compiler.')
    elif alias.exists():
        raise BuildError('Preserving an unexpected existing glslang tool.')
    else:
        alias.symlink_to(compiler)
    env = dict(os.environ, BB_ARCH='arm64', SOTC_GPU_SOURCE=str(source), SOTC_GPU_JOBS=str(jobs))
    env.pop('BB_DEPS_REBUILD', None)  # Rebuild requests belong to the dependency maintainer.
    env['PATH'] = str(tool_dir) + os.pathsep + env.get('PATH', '')
    runner.run(['/bin/bash', generated], env=env)
    prefix = source / 'deps/macos-arm64'
    env['PKG_CONFIG_LIBDIR'] = str(prefix / 'lib/pkgconfig') + os.pathsep + str(prefix / 'share/pkgconfig')
    env['PKG_CONFIG_PATH'] = ''
    env['PATH'] = str(prefix / 'bin') + os.pathsep + env['PATH']
    output = private_path(work, 'gpu-build')
    runner.run(['cmake', '-S', source / 'gpu', '-B', output, '-G', 'Ninja',
                '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0',
                '-DCMAKE_BUILD_TYPE=RelWithDebInfo', '-DBB_LTO=OFF', '-DBB_PGO=off',
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
    undefined = subprocess.check_output(['nm', '-u', library], text=True)
    runtime_symbols = sorted({line.split()[-1] for line in undefined.splitlines() if line.split()
                              and line.split()[-1].startswith(('_runtime_', '_bbcpu_', '_bb_image_base'))})
    report = dict(upstream=REPOSITORY, revision=REVISION, architecture=architecture,
                  library=str(library), dependencies=str(prefix), jobs=jobs, runtime_symbols=runtime_symbols,
                  dependency_recipe_sha256=DEPS_SHA256,
                  renderer_launched=False, game_frame_verified=False)
    private_path(work, 'gpu-build.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'GPU library built: {library}\nNo renderer was launched; loader integration and game video are still pending.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Read-only inventory; no network, downloads or builds.')
    parser.add_argument('--fetch', action='store_true', help='Fetch the exact GPU source and submodules only.')
    args = parser.parse_args()
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
        source = private_path(work, 'gpu-source')
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
            fetch_source(work, private_path(work, 'gpu-source'), runner)
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
