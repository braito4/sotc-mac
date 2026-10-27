"""CPU parameter consistency and bounded retention; no device or game input."""
import os
import subprocess
from pathlib import Path
from analyze_gpu_submission import analyze
ROOT = Path(__file__).resolve().parents[1]
def main():
    work=ROOT/'work/tiling-trace-fixture'; work.mkdir(exist_ok=True)
    cpp=work/'fixture.cpp'; binary=work/'fixture'
    cpp.write_text(r'''
#include "gpu_tiling_trace.h"
#include <cassert>
#include <string>
using namespace SotcTilingTrace;
int main() {
    Record valid{}; valid.bits=64; valid.levels=2; valid.groups=2;
    valid.bindings[0]={1,0,1024}; valid.bindings[1]={2,0,1024};
    valid.mips[0]={512,8,8,0}; valid.mips[1]={512,8,8,512};
    Check(valid); assert(valid.flags==0 && valid.elements==128 && valid.invocations==128);
    Record r=valid; r.groups=3; Check(r); assert((r.flags & 32) && (r.flags & 64));
    r=valid; r.levels=17; Check(r); assert(r.flags & 1);
    r=valid; r.levels=0; Check(r); assert(r.flags & 1);
    r=valid; r.bits=0; Check(r); assert(r.flags & 2);
    r=valid; r.bits=7; Check(r); assert(r.flags & 2);
    r=valid; r.mips[1].pitch=0; Check(r); assert(r.flags & 4);
    r=valid; r.mips[1].size=511; Check(r); assert((r.flags & 8) && (r.flags & 32));
    r=valid; r.mips[1].offset=~U64{0}; Check(r); assert(r.flags & 16);
    r=valid; r.bits=128; r.groups=1; Check(r); assert(r.flags==0 && r.elements==64);
    Journal reuse;
    Record parameter=valid; parameter.bindings[2]={77,256,128}; parameter.tick=9;
    reuse.Add(parameter);
    reuse.Map(77,384,64,10,8); // adjacent, no overlap
    reuse.Map(78,256,128,10,8); // another buffer
    reuse.Map(77,300,16,10,8); // pending overlap
    reuse.Map(77,256,128,11,9); // retired overlap
    auto* reuse_file=std::tmpfile(); assert(reuse_file); reuse.Report(reuse_file);std::rewind(reuse_file);
    std::string reuse_log; char reuse_bytes[1024];
    while(auto n=std::fread(reuse_bytes,1,sizeof(reuse_bytes),reuse_file)) reuse_log.append(reuse_bytes,n);
    std::fclose(reuse_file);
    assert(reuse_log.find("maps=4 overlaps=2 retained=2 pending=1 first_pending=1")!=std::string::npos);
    std::fwrite(reuse_log.data(),1,reuse_log.size(),stdout);
    Journal journal;
    r=valid; r.groups=3; journal.Add(r);
    for (int i=0;i<200;++i) journal.Add(valid);
    auto* file=std::tmpfile(); assert(file); journal.Report(file); std::rewind(file);
    std::string log; char buffer[1024];
    while(auto n=std::fread(buffer,1,sizeof(buffer),file)) log.append(buffer,n);
    std::fclose(file);
    assert(log.find("total=201 retained=128 omitted=73 invalid=1 first_invalid=1")!=std::string::npos);
    assert(log.find("set=first_invalid seq=1 ")!=std::string::npos);
    struct Params { unsigned num_mips; std::array<Mip,16> mips; unsigned bank_swizzle{}, micro_tiled_mips{}; } params{2,valid.mips};
    Observe(123,false,2,64,params,valid.bindings[0],valid.bindings[1],valid.bindings[2]);
    file=std::tmpfile(); assert(file); Get().Report(file);std::rewind(file);log.clear();
    while(auto n=std::fread(buffer,1,sizeof(buffer),file)) log.append(buffer,n);
    std::fclose(file);
    assert(log.find(Enabled()?"total=1 retained=1":"total=0 retained=0")!=std::string::npos);
    std::fwrite(log.data(),1,log.size(),stdout);
}
''')
    subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-fno-sanitize-recover=undefined','-I'+str(ROOT/'runtime/src'),str(cpp),'-o',str(binary)],check=True)
    for flag in ('0','1'):
        run=subprocess.run([str(binary)],env=dict(os.environ,SOTC_GPU_SUBMIT_TRACE=flag),check=True,timeout=10,
                           capture_output=True,text=True)
        report=analyze(run.stdout)
        snapshot=report['tiling_parameters'][-1]
        assert report['parameter_reuse'][0]['pending']==1
        assert report['parameter_reuse'][0]['records'][0]['parameter_tick']==9
        assert snapshot['total']==int(flag)
        if flag=='1':
            record=snapshot['records'][0]
            assert record['pipeline']==123 and record['flags']==0
            assert len(record['bindings'])==3 and len(record['mips'])==2
            assert record['mips'][1]['offset']==512
    print('PASS: tiling parameter bounds, overflow-safe ranges, 64/128-bit layouts, opt-in and first-invalid retention after tail eviction; CPU only.')
if __name__=='__main__':main()
