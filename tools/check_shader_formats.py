"""Production format-table bounds with sanitizers; no GPU or game-derived inputs."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
from build_gpu import ROOT, source_path, build_path


def main():
    tree=source_path(ROOT)
    path=Path('gpu/shadps4/shader_recompiler/frontend/format.cpp')
    with tempfile.TemporaryDirectory(prefix='format-bounds-',dir=ROOT/'work') as folder:
        out=Path(folder)
        for label,base in [('pinned',ROOT/'runtime/upstream'),('adapted',tree)]:
            text=(base/path).read_text()
            tables=re.findall(r'constexpr std::array<InstFormat, (\d+)> InstructionFormat(\w+)\s*=',text)
            assert len(tables)==17, tables
            # Keep every production array and lookup; only the unused assertion
            # dependency is replaced with abort. No successful shader substitutes.
            text=text.replace('#include "common/assert.h"','#include <cstdlib>')
            at=text.index('namespace Shader::Gcn {')
            text=text[:at]+'#undef UNREACHABLE\n#define UNREACHABLE() std::abort()\n'+text[at:]
            text+='''
#include <cassert>
#include <cstdio>
using namespace Shader::Gcn;
int main(int argc,char**) {
    if(argc>1) {
        volatile unsigned bad=InstructionFormatSOPK.size();
        volatile auto result=InstructionFormat(InstEncoding::SOPK,bad);(void)result;return 0;
    }
    unsigned valid=0,rejected=0;
'''
            for size,encoding in tables:
                table='InstructionFormat'+encoding
                text+='''
    for(unsigned i=0;i<@TABLE@.size();++i) {
        const auto a=InstructionFormat(InstEncoding::@ENC@,i),b=@TABLE@[i];
        assert(a.inst_class==b.inst_class && a.inst_category==b.inst_category &&
               a.src_count==b.src_count && a.dst_count==b.dst_count &&
               a.src_type==b.src_type && a.dst_type==b.dst_type);++valid;
    }
    for(unsigned i:{unsigned(@TABLE@.size()),0xffffffffu}) {
        const auto a=InstructionFormat(InstEncoding::@ENC@,i);
        assert(a.src_type==ScalarType::Undefined && a.dst_type==ScalarType::Undefined);++rejected;
    }
'''.replace('@TABLE@',table).replace('@ENC@',encoding)
            text+='std::printf("PASS: %u unchanged entries and %u rejected indices across 17 tables; no GPU\\n",valid,rejected);}\n'
            cpp=out/(label+'.cpp');binary=out/label;cpp.write_text(text)
            subprocess.run(['clang++','-std=c++23','-O1','-Wall','-Wextra','-Werror',
                '-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE','-fsanitize=address,undefined,bounds','-fno-sanitize-recover=all',
                '-I'+str(ROOT/'runtime/src'),'-I'+str(tree/'gpu/shim'),
                '-I'+str(tree/'gpu/shadps4'),'-I'+str(tree/'deps/macos-arm64/include'),
                '-I'+str(build_path(ROOT)/'include'),str(cpp),'-o',str(binary)],check=True)
            run=subprocess.run([str(binary)]+(['invalid'] if label=='pinned' else []),
                               capture_output=True,text=True,timeout=10,env=dict(os.environ))
            if label=='pinned':
                assert run.returncode in (-5,-6) or (run.returncode!=0 and ('runtime error:' in run.stderr or 'AddressSanitizer' in run.stderr)),(run.returncode,run.stderr)
                print('PASS: pinned production lookup reproduces an out-of-bounds access with checked std::array')
            else:
                assert run.returncode==0,run.stderr
                print(run.stdout.strip())


if __name__=='__main__':main()
