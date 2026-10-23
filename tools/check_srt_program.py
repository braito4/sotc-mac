"""Installed portable SRT validation and original-source ASan failure control."""
from pathlib import Path
import os,re,subprocess,tempfile
from build_gpu import ROOT,source_path,build_path
from check_buffer_copy_chunks import function
from check_gpu_link import host_symbol,inspect,parse_symbols

def main():
    tree=source_path(ROOT);prefix=tree/'deps/macos-arm64';build=build_path(ROOT);library=build/'libbbgpu.dylib'
    _,imports=parse_symbols(inspect(library));defs=['#include <stdlib.h>','#include <stdint.h>','#include <setjmp.h>']
    for n in sorted(x for x in imports if host_symbol(x)):
        if n=='runtime_fault_recover':defs.append('_Thread_local sigjmp_buf *runtime_fault_recover;')
        elif n in ('bb_image_base','runtime_disabled_optimizations','runtime_experiment_bits'):defs.append('uint64_t '+n+';')
        elif n=='runtime_restarting':defs.append('volatile int runtime_restarting;')
        else:defs.append('void '+n+'(void) { abort(); }')
    with tempfile.TemporaryDirectory(prefix='srt-program-cpu-',dir=ROOT/'work') as temp:
        out=Path(temp);host=out/'host.c';obj=out/'host.o';binary=out/'fixture';host.write_text('\n'.join(defs)+'\n')
        subprocess.run(['clang','-arch','arm64','-mmacosx-version-min=15.0','-c',str(host),'-o',str(obj)],check=True)
        subprocess.run(['clang++','-std=c++23','-arch','arm64','-mmacosx-version-min=15.0','-O1',
            '-I'+str(ROOT/'runtime/src'),'-I'+str(tree/'gpu/shim'),'-I'+str(tree/'gpu/shadps4'),
            '-I'+str(prefix/'include'),'-I'+str(build/'include'),str(ROOT/'tests/srt_program_fixture.cpp'),str(obj),str(library),
            '-L'+str(prefix/'lib'),'-lfmt','-Wl,-rpath,'+str(build),'-Wl,-export_dynamic','-o',str(binary)],check=True)
        env=dict(os.environ,BB_GPU_USER_DIR=str(out),BB_SRT_INTERP='1')
        cases={'valid':None,'values':'srt_value_overflow','depth':'srt_pointer_overflow','pop':'srt_pointer_underflow',
            'arith':'srt_value_underflow','copy':'srt_value_underflow','truncated':'srt_truncated_operand',
            'opcode':'srt_bad_opcode','flag':'srt_bad_opcode','end':'srt_unbalanced_end',
            'trailing':'srt_unbalanced_end','missing':'srt_missing_end','bitfield':'srt_invalid_bitfield'}
        for mode,reason in cases.items():
            result=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=25,env=env)
            if result.returncode!=(0 if reason is None else 23):raise RuntimeError(mode+': '+result.stdout+result.stderr)
            if reason and ('reason='+reason not in result.stderr or 'phase=srt-fixture' not in result.stderr):raise RuntimeError(result.stderr)
            if not reason:print(result.stdout.strip())
        original=(ROOT/'runtime/upstream/gpu/shadps4/shader_recompiler/ir/passes/flatten_extended_userdata_pass.cpp').read_text()
        enum=original[original.index('enum SrtOp : u32 {'):original.index('} // namespace',original.index('enum SrtOp : u32 {'))]
        body=function(original,'void RunSrtProgram(')
        cpp=out/'before.cpp';before=out/'before'
        cpp.write_text('''#include <algorithm>
#include <cstdint>
#include <cstring>
#include <csetjmp>
#include <vector>
#include <cstdlib>
using u8=uint8_t;using u32=uint32_t;using u64=uint64_t;
sigjmp_buf* runtime_fault_recover;
#define LOG_WARNING(...) ((void)0)
#define UNREACHABLE_MSG(...) std::abort()
'''+enum+body+'''
int main(int argc,char**){
 u32 ud[16]{},flat[32]{};
 if(argc>1){
  u32 data[16];for(unsigned i=0;i<16;++i)data[i]=0x100+i;
  auto address=reinterpret_cast<uintptr_t>(data);std::memcpy(ud,&address,8);
  const u32 p[]={SrtMagic,OpPushImm,0,OpImm,7,OpImm,0,OpImm,32,OpBitFieldUExtract,OpCopyDyn,16,OpPop,OpEnd};
  RunSrtProgram(p,ud,flat);return flat[16]==data[7]?0:97;
 }
 std::vector<u32> p{SrtMagic};for(unsigned i=0;i<33;++i)p.insert(p.end(),{OpImm,0});p.push_back(OpEnd);RunSrtProgram(p.data(),ud,flat);
}
''')
        subprocess.run(['clang++','-std=c++20','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(cpp),'-o',str(before)],check=True)
        result=subprocess.run([str(before)],capture_output=True,text=True,timeout=15)
        if result.returncode==0 or not any(x in result.stderr for x in ('runtime error:','AddressSanitizer')):raise RuntimeError('Original value-stack overflow did not reproduce: '+result.stderr)
        result=subprocess.run([str(before),'bitfield'],capture_output=True,text=True,timeout=15)
        if result.returncode!=97:raise RuntimeError('Original full-width extraction did not return the wrong element: '+result.stderr)
        constant=(ROOT/'runtime/upstream/gpu/shadps4/shader_recompiler/ir/passes/constant_propagation_pass.cpp').read_text()
        part=constant.split('case IR::Opcode::BitFieldUExtract:',1)[1].split('case IR::Opcode::BitFieldSExtract:',1)[0]
        body=re.search(r'\[\]\(u32 base, u32 shift, u32 count\) \{(.*?)\n        \}\);',part,re.S)[1]
        cpp.write_text('#include <cstdint>\n#include <cstddef>\n#include <cstdlib>\nusing u32=uint32_t;\n#define UNREACHABLE_MSG(...) std::abort()\nu32 fold(u32 base,u32 shift,u32 count){'+body+'\n}\nint main(){return fold(7,0,32)==7?0:97;}\n')
        subprocess.run(['clang++','-std=c++20','-O1','-fsanitize=undefined','-fno-sanitize-recover=all',str(cpp),'-o',str(before)],check=True)
        result=subprocess.run([str(before)],capture_output=True,text=True,timeout=15)
        if result.returncode==0 or 'runtime error:' not in result.stderr:raise RuntimeError('Original constant-fold shift UB did not reproduce: '+result.stderr)
        print('PASS: original VM overflow, wrong full-width offset and constant-fold UBSan defect reproduced; twelve installed invalid controls rejected')

if __name__=='__main__':main()
