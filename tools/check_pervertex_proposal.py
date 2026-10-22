"""CPU-only synthetic experiment against an isolated, explicitly pinned PR checkout.

No downloads, dependency replacement, Vulkan/Metal runtime loading or game input.
Generated MSL is optionally checked with the Apple compiler; blocked/failed checks
are reported explicitly and never reported as passing GPU validation. Obtain
SPIRV-Cross PR #2699 separately under work/pervertex-research before running this.
API design: KhronosGroup/SPIRV-Cross PR #2699 by Jean-Philippe Meunier (carbonimax).
"""
from pathlib import Path
from build_gpu import source_path, build_path
import subprocess
import json
import os
import time
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REVISION = '81739edf516c2bef043ac61f510b4bd78e126a22'


def check_metal_syntax(output):
    # Select Xcode only for this subprocess; do not change xcode-select, accept
    # licenses, install tools, or create any Metal device.
    env = dict(os.environ)
    candidates = [env]
    xcode = Path('/Applications/Xcode.app/Contents/Developer')
    if xcode.is_dir() and env.get('DEVELOPER_DIR') != str(xcode):
        candidates.append(dict(env, DEVELOPER_DIR=str(xcode)))
    attempts = []
    for candidate in candidates:
        probe = subprocess.run(['xcrun', '--find', 'metal'], env=candidate,
                               capture_output=True, text=True, timeout=30)
        attempts.append({'developer_dir': candidate.get('DEVELOPER_DIR', 'selected'),
                         'returncode': probe.returncode,
                         'diagnostic': (probe.stdout + probe.stderr).strip()[:4096]})
        if probe.returncode == 0 and Path(probe.stdout.strip()).is_file():
            version = subprocess.run(['xcrun', 'metal', '--version'], env=candidate,
                                     capture_output=True, text=True, timeout=30)
            attempts[-1]['version_returncode'] = version.returncode
            attempts[-1]['version_diagnostic'] = (version.stdout + version.stderr).strip()[:4096]
            if version.returncode == 0:
                env = candidate
                break
    else:
        return {'status': 'blocked', 'discovery': attempts,
                'reason': 'Apple Metal compiler unavailable or Xcode license/setup incomplete'}
    results = []
    for shader in sorted(output.glob('*.metal')):
        result = subprocess.run(['xcrun', 'metal', '-x', 'metal', '-std=macos-metal2.4',
                                 '-fsyntax-only', '-fno-modules', str(shader)], env=env,
                                capture_output=True, text=True, timeout=60)
        results.append({'file': shader.name, 'returncode': result.returncode,
                        'diagnostic': (result.stdout + result.stderr)[:8192]})
    return {'status': 'pass' if len(results) == 6 and all(r['returncode'] == 0 for r in results)
            else 'failed', 'discovery': attempts, 'shaders': results}


def main():
    source = ROOT/'work/pervertex-research/spirv-cross-pr2699'
    build = ROOT/'work/pervertex-research/spirv-cross-pr2699-build'
    if subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip() != REVISION:
        raise SystemExit('The isolated PR checkout does not match the reviewed revision')
    if subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain', '--untracked-files=no'], text=True).strip():
        raise SystemExit('The isolated PR checkout has tracked modifications')
    gpu = source_path(ROOT)
    sirit = gpu/'gpu/third_party/sirit'
    flags = ['clang++', '-std=c++20', '-O1', '-arch', 'arm64', '-mmacosx-version-min=15.0']
    includes = ['-I'+str(sirit/'include'), '-I'+str(sirit/'externals/SPIRV-Headers/include')]
    output = ROOT/'work/pervertex-research'/time.strftime('validation-%Y%m%dT%H%M%SZ', time.gmtime())
    output = Path(tempfile.mkdtemp(prefix=output.name+'-', dir=output.parent))
    with tempfile.TemporaryDirectory(prefix='pervertex-proposal-', dir=ROOT/'work') as temp:
        work = Path(temp)
        objects = []
        for fixture in ('test_pervertex_replay',):
            obj = work/(fixture+'.o'); objects.append(str(obj))
            subprocess.run([*flags, *includes, '-DSOTC_SPIRV_EMITTER', '-c',
                            str(ROOT/'tests'/ (fixture+'.cpp')), '-o', str(obj)], check=True)
        binary = work/'proposal'
        subprocess.run([*flags, '-I'+str(source), str(ROOT/'tests/test_pervertex_replay.cpp'),
                        *objects, str(build_path(ROOT)/'third_party/sirit/src/libsirit.a'),
                        *(str(build/('libspirv-cross-'+name+'.a')) for name in ('msl', 'glsl', 'core')),
                        '-o', str(binary)], check=True)
        dependencies = subprocess.check_output(['otool', '-L', str(binary)], text=True)
        assert not any(name in dependencies for name in ('MoltenVK', '/Metal.', 'libbbgpu', '/Vulkan.'))
        print('Isolated SPIRV-Cross PR #2699: ' + REVISION, flush=True)
        result = subprocess.run([str(binary), str(output)], check=True, timeout=15,
                                capture_output=True, text=True)
        print(result.stdout, end='')
        (output/'cpu-checks.txt').write_text(result.stdout + result.stderr)
    report = {'revision': REVISION, 'cpu_conversion_and_abi': 'pass',
              'gpu_execution': 'not-run', 'renderer_integration': 'not-enabled',
              'metal_syntax': check_metal_syntax(output)}
    (output/'validation.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Validation report: ' + str(output/'validation.json'))
    print('Metal syntax: ' + report['metal_syntax']['status'])
    return {'pass': 0, 'failed': 1, 'blocked': 2}[report['metal_syntax']['status']]


if __name__ == '__main__':
    raise SystemExit(main())
