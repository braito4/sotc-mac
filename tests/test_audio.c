/* Original synthetic provider; no speakers or game samples are used. */
#include "bootstrap_audio.h"
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static int open_fail,resume_fail,created,destroyed,resumed;
static BootstrapAudioSpec received;
static unsigned char streams[32];
static void *open_stream(uint32_t device,const BootstrapAudioSpec *spec,void *callback,void *userdata) {
    assert(device==UINT32_MAX && !callback && !userdata);
    received=*spec;
    if(open_fail)return NULL;
    assert(created<32);return &streams[created++];
}
static bool resume(void *stream) { assert(stream);++resumed;return !resume_fail; }
static void destroy(void *stream) { assert(stream);++destroyed; }
static uintptr_t bridge(const char *name) {
    uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);
    code[0]=0x48;code[1]=0xb8;memcpy(code+2,&fn,8);code[10]=0xff;code[11]=0xe0;
    bbcpu_add_guest_code((uintptr_t)code,16384);return (uintptr_t)code;
}
static int32_t call(uintptr_t fn,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f) {
    const uint64_t args[]={a,b,c,d,e,f};return (int32_t)bbcpu_call(fn,6,args);
}
int main(int argc,char **argv) {
    bootstrap_initialize(1);
    assert(!bootstrap_resolve("JfEPXVxhFqA#q#M",0));
    BootstrapAudioBackend backend={.initialized_subsystems=0x10,.driver="coreaudio",
        .open=open_stream,.resume=resume,.destroy=destroy};
    if(argc==2) {
        if(!strcmp(argv[1],"--unavailable")) backend.driver="dummy";
        else if(!strcmp(argv[1],"--not-ready")) backend.initialized_subsystems=0;
        else { assert(!strcmp(argv[1],"--missing-callback"));backend.open=NULL; }
    }
    assert(bootstrap_audio_bind(&backend)==(argc==2 ? -1:0));
    assert(bootstrap_audio_bind(&backend)==-1);
    uintptr_t init=bridge("JfEPXVxhFqA#q#M"),open=bridge("ekNvsT22rsY#q#M"),close=bridge("s1--uE9mBFw#q#M");
    assert(!bootstrap_resolve("JfEPXVxhFqA#G#H",0));
    assert(!bootstrap_resolve("w3PdaSTSwGE#q#M",0)); /* PCM output stays unresolved. */
    *runtime_errno()=77;
    assert((uint32_t)call(open,1,0,0,256,48000,1)==0x8026000f);
    if(argc==2) {
        for(int i=0;i<2;++i) assert((uint32_t)call(init,1,2,3,4,5,6)==0x80260010);
        assert(!created && !resumed && !destroyed && *runtime_errno()==77);
        puts("PASS: unavailable/dummy audio backend rejects initialization without claiming ports");return 0;
    }
    assert(call(init,1,2,3,4,5,6)==0); /* No-argument ABI ignores stale registers. */
    assert((uint32_t)call(init,0,0,0,0,0,0)==0x8026000e);
    assert((uint32_t)call(open,1,0,0,0,48000,1)==0x80260006);
    assert((uint32_t)call(open,1,0,0,257,48000,1)==0x80260006);
    assert((uint32_t)call(open,1,0,0,2304,48000,1)==0x80260006);
    assert((uint32_t)call(open,1,0,0,256,44100,1)==0x80260008);
    assert((uint32_t)call(open,1,0,0,256,48000,8)==0x80260007);
    assert((uint32_t)call(open,1,4,0,256,48000,1)==0x8026000a);
    assert((uint32_t)call(open,1,0,1,256,48000,1)==0x8026000a);
    assert((uint32_t)call(open,2,0,0,256,48000,1)==0x8026000a);
    const int32_t invalid_users[]={-1,0,254,256};
    for(unsigned i=0;i<sizeof(invalid_users)/sizeof(*invalid_users);++i)
        assert((uint32_t)call(open,(uint32_t)invalid_users[i],0,0,256,48000,5)==0x8026000a);
    assert(!created);
    open_fail=1;assert((uint32_t)call(open,255,0,0,256,48000,5)==0x80260010 && !created);open_fail=0;
    resume_fail=1;assert((uint32_t)call(open,255,0,0,256,48000,5)==0x80260010);
    assert(created==1 && destroyed==1);resume_fail=0;
    int32_t system=call(open,UINT64_C(0x1000000ff),0,0,256,48000,5);
    assert(system>0 && received.channels==8 && received.format==0x8120 && received.frequency==48000);
    assert(call(close,(uint32_t)system,0,0,0,0,0)==0);
    for(unsigned format=0;format<8;++format) {
        int32_t handle=call(open,1,0,0,256,48000,format);assert(handle>0);
        static const int channels[]={1,2,8,1,2,8,8,8};
        assert(received.channels==channels[format] && received.frequency==48000);
        assert(received.format==(format>=3 && format!=6 ? 0x8120:0x8010));
        assert(call(close,(uint32_t)handle,0,0,0,0,0)==0);
        assert((uint32_t)call(close,(uint32_t)handle,0,0,0,0,0)==0x80260003);
    }
    int32_t handles[9];
    for(int i=0;i<9;++i) { handles[i]=call(open,i%2 ? 1:255,i==8,0,256,48000,1);assert(handles[i]>0); }
    assert((uint32_t)call(open,255,0,0,256,48000,1)==0x80260005);
    assert((uint32_t)call(open,1,1,0,256,48000,1)==0x80260005);
    for(int i=0;i<9;++i) assert(call(close,(uint32_t)handles[i],0,0,0,0,0)==0);
    int32_t again=call(open,1,0,0,UINT64_C(0x100000100),UINT64_C(0x10000bb80),UINT64_C(0x100000001));
    assert(again>0 && again!=handles[0]);assert(call(close,(uint32_t)again,0,0,0,0,0)==0);
    assert(created==destroyed && *runtime_errno()==77);
    puts("PASS: AudioOut guest init/open/close ABI, local/system users, format negotiation, scope/validation, provider failures, shared capacity, stale handles and stream ownership contract");
}
