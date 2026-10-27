"""Bounded per-render metadata and production reporter/parser; CPU only."""
import subprocess
from build_gpu import ROOT
from analyze_gpu_submission import analyze

def main():
    work=ROOT/'work/render-trace-fixture';work.mkdir(exist_ok=True)
    cpp=work/'fixture.cpp';binary=work/'fixture'
    cpp.write_text(r'''
#include "gpu_submission_trace.h"
#include <cassert>
#include <string>
using namespace SotcGpuTrace;
int main() {
    Recorder r; const auto recording=r.Begin(10);
    SotcRenderTrace::Scope scope{};scope.encoder=11;scope.command=12;
    scope.attachments[0]={13,0,70,1920,1080,1,1,1,0,0,0,1,1,0,0};
    scope.attachments[8]={14,0,252,1920,1080,1,1,1,0,0,0,2,1,0,0};
    ComputeBinding binding{};binding.pipeline=70;binding.resource=30;binding.offset=0;binding.length=68;binding.slot=2;
    r.ComputeBuffer(10,binding);
    NativeEvent dispatch{};std::snprintf(dispatch.event.data(),32,"dispatch_direct");dispatch.pipeline=70;
    r.Native(10,dispatch);
    for(unsigned i=0;i<129;++i) r.ComputeBuffer(10,binding);
    NativeEvent begin{};std::snprintf(begin.event.data(),32,"render_begin");r.Native(10,begin);
    r.RenderBegin(10,scope);
    SotcRenderTrace::Draw d{};d.kind=3;d.pipeline=20;d.indirect=30;d.offset=48;d.indirect_length=68;d.argument_bytes=20;
    d.index=31;d.index_length=64;d.index_element_bytes=2;SotcRenderTrace::CheckRanges(d);
    std::snprintf(d.vertex.data(),d.vertex.size(),"vertex_fixture");
    std::snprintf(d.fragment.data(),d.fragment.size(),"fragment_fixture");
    for(unsigned i=0;i<300;++i) r.RenderDraw(10,d);
    r.RenderBegin(10,scope);d.pipeline=999;r.RenderDraw(10,d); // second scope retained
    NativeEvent checkpoint{};std::snprintf(checkpoint.event.data(),32,"render_checkpoint");
    checkpoint.checkpoint=5;checkpoint.progress_event=42;r.Native(10,checkpoint);
    for(unsigned i=2;i<40;++i) {r.RenderBegin(10,scope);r.RenderDraw(10,d);}
    Submission sub{};sub.scheduler=1;sub.tick=1;const Key buffers[]={10};r.Submit(sub,buffers);
    auto* out=std::tmpfile();assert(out);r.Report(out,1,1,0);std::rewind(out);
    std::string log;char bytes[2048];while(auto n=std::fread(bytes,1,sizeof(bytes),out)) log.append(bytes,n);
    std::fclose(out);
    assert(log.find("draws=300 retained=288 omitted=12 pipelines=1")!=std::string::npos);
    assert(log.find("pipeline=0x3e7")!=std::string::npos);
    assert(log.find("draw=32 ")!=std::string::npos && log.find("draw=45 ")!=std::string::npos);
    std::fwrite(log.data(),1,log.size(),stdout);
    assert(r.Begin(10)!=recording);sub.tick=2;r.Submit(sub,buffers);
    out=std::tmpfile();assert(out);r.Report(out,1,2,0);std::rewind(out);log.clear();
    while(auto n=std::fread(bytes,1,sizeof(bytes),out)) log.append(bytes,n);std::fclose(out);
    assert(log.find("GPU render trace:")==std::string::npos);
    for(unsigned tick=3;tick<=10;++tick) {r.Begin(10);sub.tick=tick;r.Submit(sub,buffers);}
    out=std::tmpfile();assert(out);r.Report(out,1,1,0);std::rewind(out);log.clear();
    while(auto n=std::fread(bytes,1,sizeof(bytes),out)) log.append(bytes,n);std::fclose(out);
    assert(log.find("total=40 retained=0 omitted=40")!=std::string::npos);
    SotcRenderTrace::Draw ranges{};ranges.kind=3;ranges.indirect=1;ranges.index=2;
    ranges.indirect_length=64;ranges.offset=44;ranges.argument_bytes=20;
    ranges.index_length=16;ranges.index_element_bytes=4;ranges.index_offset=16;
    SotcRenderTrace::CheckRanges(ranges);
    assert(ranges.argument_range==1 && ranges.index_base_range==1 && ranges.index_draw_range==0);
    ranges.offset=45;SotcRenderTrace::CheckRanges(ranges);assert(ranges.argument_range==2);
    ranges.kind=1;ranges.count=1;SotcRenderTrace::CheckRanges(ranges);assert(ranges.index_draw_range==2);
    ranges.index_offset=12;SotcRenderTrace::CheckRanges(ranges);assert(ranges.index_draw_range==1);
    ranges.count=~U64(0);SotcRenderTrace::CheckRanges(ranges);assert(ranges.index_draw_range==2);
    assert(!SotcRenderTrace::Fits(16,~U64(0),1,4));
    SotcRenderTrace::Scope bounded;
    for(unsigned i=0;i<80;++i) {d.pipeline=i+1;bounded.Add(d);}
    assert(bounded.pipelines.size()==64 && bounded.pipeline_untracked==16);
}
''')
    subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=undefined',
        '-fno-sanitize-recover=undefined','-I'+str(ROOT/'runtime/src'),str(cpp),'-o',str(binary)],check=True)
    run=subprocess.run([str(binary)],capture_output=True,text=True,check=True,timeout=10)
    batch=analyze(run.stdout)['reports'][0]['submissions'][0]
    assert batch['compute_binding_coverage']['total']==130
    assert batch['compute_binding_coverage']['omitted']==2
    assert batch['render_scopes'][0]['preceding_compute_binding_matches'][0]['slot']==2
    assert batch['render_coverage']['total']==40 and batch['render_coverage']['omitted']==8
    assert len(batch['render_scopes'])==32
    assert batch['render_scopes'][1]['checkpoint']==5 and batch['render_scopes'][1]['progress_event']==42
    assert batch['render_scopes'][-1]['draws']==1
    scope=batch['render_scopes'][0]
    assert scope['draws']==300 and scope['omitted']==12 and len(scope['draw_details'])==288
    assert len(scope['attachments'])==10 and scope['attachments'][8]['format']==252
    assert scope['pipeline_details'][0]['vertex']=='vertex_fixture'
    assert scope['pipeline_details'][0]['draws']==300
    assert scope['draw_details'][0]['offset']==48 and scope['draw_details'][0]['kind']==3
    # Shared identity and verified dispatch association are required; no inferred writer.
    for old,new in [('resource=0x1e','resource=0x99'),('inline_bytes=0','inline_bytes=1'),
                    ('dispatch_event=1','dispatch_event=999'),('pipeline=0x46','pipeline=0x47'),
                    ('offset=0','offset=49'),('length=68','length=67')]:
        modified='\n'.join(line.replace(old,new) if line.startswith('GPU compute binding: ') else line
                           for line in run.stdout.splitlines())
        parsed=analyze(modified)
        assert not parsed['reports'][0]['submissions'][0]['render_scopes'][0]['preceding_compute_binding_matches']
    # Feed GPU event observations into the production parser independently of CPU draw notes.
    lines=run.stdout.splitlines()
    at=next(i for i,line in enumerate(lines) if 'buffer[0]=' in line)+1
    for reached,expected in [(4,'first_unconfirmed_interval'),(5,'completed_prefix'),(2,'later_unconfirmed')]:
        extra=['GPU submission trace: Metal chunk=1 scheduled_callback=1 completed_callback=0',
               f'GPU submission trace: Metal progress chunk=1 enabled=1 available=1 encoded=8 reached={reached} omitted=0 event=0x2a']
        parsed=analyze('\n'.join(lines[:at]+extra+lines[at:]))
        assert parsed['reports'][0]['submissions'][0]['render_scopes'][1]['completion']==expected
    assert batch['render_scopes'][1]['completion']=='unknown'
    assert scope['draw_details'][0]['argument_range']==1
    assert scope['draw_details'][0]['index_draw_range']==0  # indirect arguments unread
    assert scope['draw_details'][0]['indirect_length']==68
    print('PASS: buffer bounds and unread indirect counts, multiple render scopes, checkpoint association, scope overflow accounting, early/tail draw retention, bounded pipeline census, attachments, buffer reuse isolation and actual reporter/parser; CPU only.')
if __name__=='__main__':main()
