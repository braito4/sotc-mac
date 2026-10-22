"""Actual installed CFG construction and isolated SetPC UBSan controls, CPU only."""
from pathlib import Path
import os
import subprocess
import tempfile
from build_gpu import ROOT,source_path,build_path
from check_buffer_copy_chunks import function
from check_gpu_link import host_symbol,inspect,parse_symbols


def main():
    tree=source_path(ROOT);prefix=tree/'deps/macos-arm64';build=build_path(ROOT)
    library=build/'libbbgpu.dylib'
    _,imports=parse_symbols(inspect(library))
    definitions=['#include <stdlib.h>','#include <stdint.h>','#include <setjmp.h>']
    for name in sorted(n for n in imports if host_symbol(n)):
        if name=='runtime_fault_recover':definitions.append('_Thread_local sigjmp_buf *runtime_fault_recover;')
        elif name in ('bb_image_base','runtime_disabled_optimizations','runtime_experiment_bits'):definitions.append('uint64_t '+name+';')
        elif name=='runtime_restarting':definitions.append('volatile int runtime_restarting;')
        else:definitions.append('void '+name+'(void) { abort(); }')
    with tempfile.TemporaryDirectory(prefix='shader-cfg-cpu-',dir=ROOT/'work') as temp:
        out=Path(temp);host=out/'host.c';obj=out/'host.o';host.write_text('\n'.join(definitions)+'\n')
        subprocess.run(['clang','-arch','arm64','-mmacosx-version-min=15.0',
                        '-c',str(host),'-o',str(obj)],check=True)
        template=(ROOT/'tests/shader_cfg_fixture.cpp.in').read_text()
        signature='static std::optional<u32> ResolveSetPcTarget('
        path='gpu/shadps4/shader_recompiler/frontend/control_flow_graph.cpp'
        for label,source in [('pinned',ROOT/'runtime/upstream'),('adapted',tree)]:
            text=template.replace('@RESOLVER@',function((source/path).read_text(),signature))
            cpp=out/(label+'.cpp');binary=out/label;cpp.write_text(text)
            subprocess.run(['clang++','-std=c++23','-arch','arm64','-mmacosx-version-min=15.0',
                '-O1','-fsanitize=undefined','-fno-sanitize-recover=undefined',
                '-I'+str(ROOT/'runtime/src'),'-I'+str(tree/'gpu/shim'),'-I'+str(tree/'gpu/shadps4'),
                '-I'+str(prefix/'include'),'-I'+str(build/'include'),str(cpp),str(obj),str(library),
                '-L'+str(prefix/'lib'),'-lfmt','-Wl,-rpath,'+str(build),'-Wl,-export_dynamic','-o',str(binary)],check=True)
            env=dict(os.environ,BB_GPU_USER_DIR=str(out))
            if label=='pinned':
                result=subprocess.run([str(binary),'resolver'],capture_output=True,text=True,timeout=15,env=env)
                if result.returncode==0 or 'runtime error:' not in result.stderr:
                    raise RuntimeError('Pinned SetPC undefined-overflow control did not reproduce: '+result.stderr)
                print('pinned: UBSan reproduces signed SetPC overflow')
                continue
            cases={'valid':None,'empty':'empty_program','no-blocks':'no_blocks',
                   'outside':'target_outside_program','inside':'target_inside_instruction',
                   'end-target':'branch_target_missing','zero-length':'invalid_instruction_length',
                   'misaligned-length':'invalid_instruction_length','overflow-length':'invalid_instruction_length',
                   'unresolved-setpc':'unresolved_setpc'}
            for mode,reason in cases.items():
                result=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=15,env=env)
                if result.returncode!=(0 if reason is None else 23):
                    raise RuntimeError(mode+': '+result.stdout+result.stderr)
                if reason and ('reason='+reason not in result.stderr or 'shader=0000000000005678' not in result.stderr):
                    raise RuntimeError(mode+': missing CFG failure provenance: '+result.stderr)
                if mode=='outside' and 'source_pc_known=0' not in result.stderr:
                    raise RuntimeError('Label-only evidence must not invent a source instruction')
                if mode=='end-target' and 'source_pc_known=1' not in result.stderr:
                    raise RuntimeError('Known branch source missing from rejection evidence')
            print('PASS: 60 defined SetPC targets, eight valid installed CFGs, nine explicit malformed/unsupported controls; CPU only')


if __name__=='__main__':main()
