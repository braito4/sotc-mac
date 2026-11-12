"""Queued metadata across threads, strict mismatch reasons and Metal callback doubles.

Original synthetic resources only; does not execute a native GPU dispatch or game.
"""
import ast,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CORE=r'''
#include "gpu_binding_route.h"
#include <cassert>
#include <thread>
using namespace SotcBindingRoute;
static constexpr U64 size=1<<20;
static auto prepare(U64 guest=4096){
 Begin(42);Expect(1,1);unsigned char sharp[16]={};std::memcpy(sharp,&guest,8);
 Bound(1,guest,size,size,true,10,32,size,0,sharp);Descriptor(1,10,32,size,0,1);return Take();
}
int main(int argc,char** argv){
 (void)argc;SotcBufferHistory::Register(10,100,16,4*size);
 if(!std::strcmp(argv[1],"disabled")){assert(!prepare());return 0;}
 auto p=prepare();assert(p&&p->count==1&&!Take());
 // Repeated later preparation and ring eviction cannot overwrite the queued value.
 for(unsigned i=0;i<70;++i){auto next=prepare(8192+i);assert(next&&next->id!=p->id);}
 const auto producer=Thread();
 std::thread encoder([&]{
   assert(Thread()!=producer);Scope scope(p);
   auto r=SotcBindingAudit::Find(1,100,48,size);assert(r.sequence==p->id&&r.guest==4096&&r.thread==producer&&r.encoder_thread==Thread());
   assert(!std::strcmp(SotcBindingAudit::Find(1,100,49,size).route_reason,"native_extent_changed"));
   {Scope empty({});assert(!std::strcmp(SotcBindingAudit::Find(1,100,48,size).route_reason,"no_queued_packet"));}
   assert(SotcBindingAudit::Find(1,100,48,size).sequence==p->id);
 });encoder.join();assert(!current&&!SotcBindingAudit::routed_find);
 {Scope scope(p);assert(!std::strcmp(Find(2,100,48,size).route_reason,"slot_not_observed"));}
 // Reuse of the native identity must refuse the old packet.
 SotcBufferHistory::Remove(10);SotcBufferHistory::Register(10,100,16,4*size);
 {Scope scope(p);assert(!std::strcmp(Find(1,100,48,size).route_reason,"native_epoch_changed"));}
 Begin(42);Expect(1,1);unsigned char sharp[16]={};Bound(1,4096,size,size,false,10,32,size,0,sharp);
 Descriptor(1,10,64,size,0,1);auto bad=Take();{Scope scope(bad);assert(!std::strcmp(Find(1,100,48,size).route_reason,"queued_descriptor_mismatch"));}
 Begin(42);Expect(1,1);Bound(1,4096,size,size,false,10,32,size,0,sharp);
 Descriptor(1,10,32,size,0,1);Descriptor(1,10,32,size,0,1);bad=Take();{Scope scope(bad);assert(!std::strcmp(Find(1,100,48,size).route_reason,"duplicate_descriptor"));}
 Begin(42);Expect(1,1);Bound(1,4096,size,size,false,10,32,size,0,sharp);
 bad=Take();{Scope scope(bad);assert(!std::strcmp(Find(1,100,48,size).route_reason,"descriptor_not_recorded"));}
 Begin(42);Expect(1,1);Bound(1,4096,size,size,false,10,32,size,0,sharp);
 Descriptor(1,10,32,size,0,1);for(unsigned i=0;i<40;++i)Expect(2+i,2+i);
 bad=Take();{Scope scope(bad);assert(!std::strcmp(Find(1,100,48,size).route_reason,"packet_truncated"));}
 Begin(42);Expect(1,1);Bound(1,4096,size,size,false,10,32,size,0,sharp);Begin(99);assert(!Take());
 unsigned accepted=0;for(unsigned i=0;i<200;++i)accepted+=bool(prepare());assert(accepted<128&&omitted.load());
}
'''
def literal(path,name):
    return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body
                if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
with tempfile.TemporaryDirectory(prefix='binding-route-') as tmp:
    p=Path(tmp);(p/'core.cpp').write_text(CORE)
    subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread','-I'+str(ROOT/'runtime/src'),str(p/'core.cpp'),'-o',str(p/'core')],check=True)
    env=dict(os.environ,SOTC_BUFFER_HISTORY='1',SOTC_BINDING_ROUTE='1',SOTC_BINDING_ROUTE_SHADERS='42',SOTC_BINDING_AUDIT='1',SOTC_BINDING_AUDIT_SHADER='42')
    subprocess.run([str(p/'core'),'enabled'],env=env,check=True)
    subprocess.run([str(p/'core'),'disabled'],env=dict(env,SOTC_BINDING_ROUTE='0'),check=True)
    print('PASS: immutable cross-thread packet, no stale reuse, nested scopes, native epoch/range/descriptor rejection, selection and bounds under ASan/UBSan')
    base=literal(ROOT/'tools/check_scatter_guard_trial.py','SOURCE')
    source=base[:base.index('int main(')]+r'''
#include "gpu_binding_route_metal.h"
#include <thread>
int main(int argc,char** argv){@autoreleasepool {
 assert(argc==2);(void)argv;
 TestDevice* device=[TestDevice new];Shader shader;shader.entry="synthetic_route";
 shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,2,1,0,1});
 Pipeline pipeline{&shader};Owned o;o.compute_pipeline=&pipeline;o.cmd=(id<MTLCommandBuffer>)[TestCommand new];
 auto buffer=[device newBufferWithLength:(1<<20) options:MTLResourceStorageModeShared];
 o.compute_bindings[1].elements.push_back({buffer,0,1<<20});
 const auto identity=reinterpret_cast<uintptr_t>((__bridge void*)buffer);
 using namespace SotcBindingRoute;
 SotcBufferHistory::Register(10,identity,0,1<<20);
 Begin(42);Expect(1,1);unsigned char sharp[16]={};U64 guest=4096;std::memcpy(sharp,&guest,8);
 Bound(1,guest,1<<20,1<<20,true,10,0,1<<20,0,sharp);Descriptor(1,10,0,1<<20,0,1);auto packet=Take();assert(packet);
 std::thread worker([&]{@autoreleasepool {
   Scope scope(packet);Before(o);
   auto r=SotcBindingAudit::Find(1,identity,0,1<<20);assert(r.sequence&&r.guest==4096);
   Encoded(o);[(TestCommand*)o.cmd complete];
 }});worker.join();
 // Exclusive files must preserve the first observation; no truncation/overwrite.
 Write(packet->id,"before","invalid replacement");
}}
'''
    (p/'metal.mm').write_text(source)
    subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-framework','Foundation','-framework','Metal','-I'+str(ROOT/'runtime/src'),str(p/'metal.mm'),'-o',str(p/'metal')],check=True)
    evidence=p/'evidence';evidence.mkdir()
    subprocess.run([str(p/'metal'),'fixture'],env=dict(env,SOTC_SCATTER_GUARD_DIR=str(evidence)),check=True)
    before=json.loads(next(evidence.glob('*-before.json')).read_text());encoded=json.loads(next(evidence.glob('*-encoded.json')).read_text());done=json.loads(next(evidence.glob('*-completed.json')).read_text())
    assert before['preparation_thread']!=before['encoder_thread']
    assert before['bindings'][0]['audit']['matched'] and before['bindings'][0]['audit']['guest']==4096
    assert before['route']==encoded['route']==done['route'] and before['command']==done['command']
    assert done['success'] and done['ns']>=encoded['encoding_ns']>=before['encoding_ns']
    assert all(f.stat().st_mode&0o777==0o600 for f in evidence.glob('*.json'))
    print('PASS: actual Metal route adapter, selected shader/slot identity, encoded vs completed callback, private exclusive reports; protocol doubles only')
