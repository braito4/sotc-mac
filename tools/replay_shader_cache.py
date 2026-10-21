"""Replay private SPIR-V captures through the exact-only MSL policy on the CPU.

No Vulkan/Metal device is created. These are standalone SPIRV-Cross conversions,
not MoltenVK pipeline creation, Metal compilation or image validation. Pipeline
bindings/specialization and tessellation variants are not reconstructed.
Reports include private shader identities/errors: output must remain under work/.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def build_converter(output):
    source=ROOT/'work/gpu-source'
    prefix=source/'deps/macos-arm64'
    subprocess.run(['clang++','-std=c++23','-O1','-arch','arm64','-mmacosx-version-min=15.0',
                    '-I'+str(ROOT/'runtime/src'),'-I'+str(source/'gpu/shim'),
                    '-I'+str(source/'gpu/shadps4'),'-I'+str(prefix/'include'),
                    str(ROOT/'tools/replay_shader_cache.cpp'),
                    str(source/'gpu/shadps4/video_core/renderer_metal/mtl_spirv_pervertex.cpp'),
                    '-L'+str(prefix/'lib'),'-lspirv-cross-msl','-lspirv-cross-glsl',
                    '-lspirv-cross-core','-lfmt','-o',str(output)],check=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--first-vertex-provoking',action='store_true',
                        help='Use only when the target device fixes provoking vertex to first.')
    args=parser.parse_args()
    cache=args.cache.resolve()
    output=args.output.absolute()
    if output.is_symlink() or not output.resolve().is_relative_to((ROOT/'work').resolve()):
        parser.error('Private reports must be saved under work/.')
    if not cache.is_dir(): parser.error('A shader cache directory is required.')
    modules=sorted(cache.glob('*.spv'))
    if not modules or len(modules)>10000: parser.error('Expected 1..10000 SPIR-V files.')
    records=[]
    counts=Counter()
    with tempfile.TemporaryDirectory(prefix='shader-replay-',dir=ROOT/'work') as directory:
        binary=Path(directory)/'convert'
        build_converter(binary)
        for index,module in enumerate(modules,1):
            record={'file':module.name}
            if module.is_symlink() or not module.is_file() or module.stat().st_size>8*1024*1024:
                record.update(status='invalid',error='Not a bounded regular file')
            else:
                try:
                    run=subprocess.run([str(binary),str(module),str(int(args.first_vertex_provoking))],
                                       capture_output=True,text=True,timeout=10)
                    lines=run.stdout.splitlines()
                    record.update(status='ok' if run.returncode==0 else 'failed',
                                  rewritten=bool(lines and lines[0]=='1'),
                                  stage=lines[1] if len(lines)>1 else None)
                    if run.returncode: record.update(exit=run.returncode,error=run.stderr.strip()[:4096])
                except subprocess.TimeoutExpired:
                    record.update(status='timeout',error='10-second CPU conversion budget exceeded')
            counts[record['status']]+=1
            if record.get('rewritten'): counts['exact_rewrites']+=1
            records.append(record)
            if index%100==0: print(f'CPU replay {index}/{len(modules)}: {dict(counts)}',flush=True)
    report={'kind':'standalone-cpu-spirv-cross-msl-3.1','gpu_initialized':False,
            'first_vertex_provoking':args.first_vertex_provoking,'counts':dict(counts),'modules':records}
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'CPU replay complete: {dict(counts)}; private report: {output}',flush=True)


if __name__=='__main__':
    main()
