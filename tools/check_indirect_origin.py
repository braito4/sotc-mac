"""Bounded original indirect preparation metadata: CPU only, no guest reads."""
import os
import subprocess
from build_gpu import ROOT
from analyze_gpu_submission import analyze

def main():
    folder=ROOT/'work/indirect-origin-fixture';folder.mkdir(exist_ok=True)
    cpp=folder/'fixture.cpp';binary=folder/'fixture'
    cpp.write_text(r'''
#include "gpu_indirect_origin.h"
#include <cassert>
using namespace SotcIndirectOrigin;
int main(){
 Record r{};r.guest=100;r.requested=20;r.actual=20;r.tick=5;r.stride=20;r.count=1;
 {Scope outer(r);Route(1);{Scope inner(r);Route(2);inner.Finish(10,32,128);}outer.Finish(10,64,128);}
 assert(current==nullptr);
 Resolve({.backend=10,.offset=64,.length=128,.metal=20,.metal_base=0,.recording=7,.stride=20,.count=1});
 Get().Report(stdout);
 Journal bounded;
 for(unsigned i=0;i<4100;++i)bounded.Add(r);
 bounded.Report(stdout);
 Journal copies;std::uint32_t words[5]={120,2,3,0xfffffffc,9};
 r.destination=reinterpret_cast<U64>(words);r.route=1;
 auto ticket=copies.Queue(r);assert(ticket==1);
 copies.Complete(r.guest,r.destination,20);
 copies.Queue(r);copies.Queue(r);copies.Complete(r.guest,r.destination,20);
 r.actual=16;copies.Queue(r);copies.Complete(r.guest,r.destination,16);
 copies.Report(stdout);
 Journal pending;r.actual=20;for(unsigned i=0;i<4097;++i){r.destination=i;pending.Queue(r);}
 pending.Complete(r.guest,0,20);pending.Report(stdout);
 WriterJournal writers;
 for(unsigned i=0;i<2050;++i) writers.Add({0,100+i,20,0xab,3,7,1});
 writers.Report(stdout);
 WritableBinding(256,20,0xcd,4,8,false);
 WritableBinding(0,20,0xcd,4,8,false);
 WritableBinding(256,0,0xcd,4,8,false);
 Writers().Report(stdout);
 WriterJournal repeats;
 for(unsigned i=0;i<10000;++i)repeats.Add({0,256,20,0xab,3,i,0});
 repeats.Add({0,256,20,0xab,4,10000,0}); // slot identity must not merge
 repeats.Add({0,256,20,0xab,3,10001,1}); // paged identity must not merge
 repeats.Report(stdout);
 WriterJournal recency;
 for(unsigned i=0;i<2048;++i)recency.Add({0,100+i,20,0xab,3,1,0});
 recency.Add({0,100,20,0xab,3,2,0}); // refresh oldest
 recency.Add({0,9999,20,0xab,3,3,0});
 recency.Report(stdout);
 FillJournal fills;
 fills.Add({0,100,20,0,7,1,0,0});
 fills.Add({0,110,8,0x1234,8,2,0x1000,16});
 fills.Add({0,114,20,0,8,1,0,0}); // adjacent to query, not overlapping
 fills.Add({0,100,20,0,8,3,0,0}); // GDS is a different address space
 fills.ReportMatches(stdout,110,4,0xab,2,9);
 fills.ReportMatches(stdout,~U64(0)-1,1,0xab,3,9); // no addition overflow
 FillJournal lost;
 for(unsigned i=0;i<2050;++i)lost.Add({0,100+i,1,0,7,1,0,0});
 lost.ReportMatches(stdout,100,1,0xab,2,9); // omitted is not absence

 SelectedBinding(100,20,0xbc,3,7,false,false); // other shader excluded
 for(unsigned i=0;i<130;++i)SelectedBinding(100+i,20,0xab,i%4,7,i%4==3,false,i%3,0x1000,64,2);
}
''')
    subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-fno-sanitize-recover=undefined','-I'+str(ROOT/'runtime/src'),str(cpp),'-o',str(binary)],check=True)
    for enabled in ('0','1'):
        run=subprocess.run([str(binary)],env=dict(os.environ,SOTC_INDIRECT_PREP_TRACE=enabled,SOTC_INDIRECT_WRITER_TRACE=enabled,SOTC_ARGUMENT_INPUT_SHADER="ab" if enabled=="1" else ""),
                           capture_output=True,text=True,check=True)
        inputs=[line for line in run.stderr.splitlines() if line.startswith('GPU argument input:')]
        assert len(inputs)==(128 if enabled=='1' else 0)
        assert run.stderr.count('limit=128 truncated=1')==int(enabled)
        if inputs:
            assert 'shader=0xab slot=0 guest=0x64 declared_bytes=20 tick=7 written=0 paged=0' in inputs[0]
            assert 'written=1' in inputs[3]
            assert 'route=1 backend=0x1000 offset=64 special_type=2' in inputs[1]
            assert 'route=2' in inputs[2]
        parsed_inputs=analyze(run.stderr)
        assert len(parsed_inputs['argument_inputs'])==len(inputs)
        assert parsed_inputs['argument_inputs_truncated']==bool(int(enabled))
        decoded=analyze(run.stdout)
        snapshots=decoded['indirect_origins']
        writer_reports=decoded['indirect_writers']
        repeated,recency=writer_reports[-2:]
        writer_reports=writer_reports[:-2]
        assert repeated['total']==10002 and repeated['retained']==3 and repeated['omitted']==0
        assert repeated['coalesced']==9999
        r=repeated['records'][0]
        assert (r['first_sequence'],r['sequence'],r['first_tick'],r['tick'],r['observations'])==(1,10000,0,9999,10000)
        assert recency['omitted']==1 and recency['evicted']==1
        assert {r['guest'] for r in recency['records']}==({100+i for i in range(2048)}-{101}|{9999})
        assert sum(r['observations'] for r in recency['records'])+recency['omitted']==recency['total']
        assert len(writer_reports[-2]['records'])==2048
        assert writer_reports[-2]['records'][0]['sequence']==3
        assert writer_reports[-2]['records'][0]['shader']==0xab
        assert len(writer_reports[-1]['records'])==int(enabled)
        fill_reports=decoded['argument_fills']
        assert [r['matches'] for r in fill_reports]==[2,0,0]
        assert [r['route'] for r in fill_reports[0]['records']]==[1,2]
        assert fill_reports[0]['records'][1]['value']==0x1234
        assert fill_reports[2]['omitted']==2 and fill_reports[2]['retained']==2048
        first,last,copies,pending=snapshots
        writer_headers=[row for row in run.stdout.splitlines() if row.startswith('GPU indirect writers:')][:-2]
        assert 'total=2050 retained=2048 omitted=2' in writer_headers[-2]
        assert ('total=1 retained=1' if enabled=='1' else 'total=0 retained=0') in writer_headers[-1]
        assert 'shader=0xab slot=3 tick=7 paged=1' in run.stdout
        assert first['total']==(3 if enabled=='1' else 0)
        if enabled=='1':
            assert [r['route'] for r in first['records'][:2]]==[2,1]
            assert first['records'][2]['kind']==2 and first['records'][2]['metal']==20
        assert last['total']==4100 and last['retained']==4096 and last['omitted']==4
        assert last['records'][0]['sequence']==5
        assert copies['pending']==0 and copies['ambiguous']==1
        c=copies['records'];assert c[0]['captured_bytes']==20 and c[0]['word3']==0xfffffffc
        assert c[0]['ticket']==1 and c[0]['copy_status']==1
        assert all(x['copy_status']==2 and x['captured_bytes']==0 for x in c[1:3])
        assert c[3]['captured_bytes']==0
        assert pending['pending']==4095 and pending['pending_dropped']==1
        assert pending['records'][0]['copy_status']==3 and pending['records'][0]['captured_bytes']==0
    print('PASS: indirect origin opt-in, nested thread-local scopes, route/resolve identities, cap loss accounting and production parser; no GPU/guest memory reads')
if __name__=='__main__':main()
