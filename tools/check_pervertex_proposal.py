"""CPU-only synthetic experiment against an isolated, explicitly pinned PR checkout.

No downloads, dependency replacement, Vulkan/Metal loading or game input. Obtain
SPIRV-Cross PR #2699 separately under work/pervertex-research before running this.
API design: KhronosGroup/SPIRV-Cross PR #2699 by Jean-Philippe Meunier (carbonimax).
"""
from pathlib import Path
from build_gpu import source_path, build_path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REVISION = '81739edf516c2bef043ac61f510b4bd78e126a22'


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
    with tempfile.TemporaryDirectory(prefix='pervertex-proposal-', dir=ROOT/'work') as temp:
        work = Path(temp)
        objects = []
        for fixture in ('test_msl_compat', 'test_pervertex_replay'):
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
        subprocess.run([str(binary)], check=True, timeout=15)


if __name__ == '__main__':
    main()
