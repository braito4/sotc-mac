"""Private paired-module experiment, not a renderer integration or pipeline test."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_pervertex_proposal import ROOT, REVISION


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('cache',type=Path)
    parser.add_argument('--metal-compiler',type=Path,required=True)
    parser.add_argument('--limit',type=int,default=32)
    args=parser.parse_args()
    if not 1<=args.limit<=64:parser.error('limit must be 1..64')
    source=ROOT/'work/pervertex-research/spirv-cross-pr2699'
    build=ROOT/'work/pervertex-research/spirv-cross-pr2699-build'
    if subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()!=REVISION:
        parser.error('Unreviewed compiler revision')
    if subprocess.check_output(['git','-C',str(source),'status','--porcelain','--untracked-files=no'],text=True).strip():
        parser.error('Modified compiler checkout')
    compiler=args.metal_compiler.resolve()
    version=subprocess.check_output([str(compiler),'--version'],text=True,timeout=15).strip()
    if args.log.stat().st_size>128<<20:parser.error('Log too large')
    pairs=list(dict.fromkeys(re.findall(r'Failed to create graphics pipeline fs_(0x[0-9a-f]+),vs_(0x[0-9a-f]+)',args.log.read_text())))
    output=Path(tempfile.mkdtemp(prefix='paired-pervertex-',dir=ROOT/'work/pervertex-research'))
    binary=output/'convert'
    subprocess.run(['clang++','-std=c++20','-O1','-arch','arm64','-mmacosx-version-min=15.0',
                    '-I'+str(source),str(ROOT/'tools/replay_pervertex_proposal.cpp'),
                    *(str(build/('libspirv-cross-'+name+'.a')) for name in ('msl','glsl','core')),
                    '-o',str(binary)],check=True)
    records=[];counts=Counter();converted=0
    for fragment,vertex in pairs:
        if converted>=args.limit:break
        vs=list(args.cache.glob(vertex+'_*.spv'));fs=list(args.cache.glob(fragment+'_*.spv'))
        # Log shader pairs omit permutation identity. Do not silently pick one.
        if len(vs)!=1 or len(fs)!=1:
            counts['ambiguous_or_missing_pair']+=1;continue
        if any(p.is_symlink() or p.stat().st_size>8<<20 for p in [*vs,*fs]):
            counts['invalid_module']+=1;continue
        folder=output/str(converted);folder.mkdir();converted+=1
        run=subprocess.run([str(binary),str(vs[0]),str(fs[0]),str(folder)],capture_output=True,text=True,timeout=20)
        row={'vertex':vs[0].name,'fragment':fs[0].name,'conversion_exit':run.returncode,
             'conversion_diagnostic':run.stderr[:4096]}
        if run.returncode:
            counts['conversion_failed']+=1
        else:
            checks=[]
            for shader in sorted(folder.glob('*.metal')):
                result=subprocess.run([str(compiler),'-x','metal','-std=metal3.1','-fsyntax-only','-fno-modules',str(shader)],capture_output=True,text=True,timeout=30)
                checks.append({'file':shader.name,'exit':result.returncode,'diagnostic':result.stderr[:4096]})
            row['metal_syntax']=checks
            counts['converted_apple_syntax_pass' if len(checks)==3 and all(x['exit']==0 for x in checks) else 'converted_apple_syntax_failed']+=1
        records.append(row)
    report={'revision':REVISION,'compiler_version':version,'counts':dict(counts),'pairs':records,
            'limits':'Log pairs omit variants. Default standalone compiler settings and experimental slots do not reconstruct runtime bindings, specialization or pipelines. No GPU/game executed; no rendering correctness claim.'}
    (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(dict(counts));print(output/'report.json')


if __name__=='__main__':main()
