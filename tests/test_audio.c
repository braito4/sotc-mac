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
static int no_device,query_fail,device_queries;
static BootstrapAudioSpec physical={0x8120,2,48000};
static int physical_frames=1024;
static uint32_t stream_device(void *stream) {
    assert((unsigned char *)stream>=streams && (unsigned char *)stream<streams+created);
    return no_device ? 0:100;
}
static bool device_format(uint32_t device,BootstrapAudioSpec *spec,int *frames) {
    assert(device==100 && spec);++device_queries;
    if(frames) *frames=physical_frames;
    *spec=physical;return !query_fail;
}
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
static void check_state(uintptr_t state,int32_t handle,int channels) {
    unsigned char guarded[34],expected[34];
    memset(guarded,0xa5,sizeof(guarded));memset(expected,0xa5,sizeof(expected));
    expected[1]=1;expected[2]=0;expected[3]=(unsigned char)channels;
    expected[5]=expected[6]=0xff;memset(expected+7,0,10);
    assert(call(state,UINT64_C(0x100000000)|(uint32_t)handle,(uintptr_t)(guarded+1),0,0,0,0)==0);
    assert(!memcmp(guarded,expected,sizeof(guarded))); /* Unaligned output, reserved bytes and guards. */
}
int main(int argc,char **argv) {
    bootstrap_initialize(1);
    assert(!bootstrap_resolve("JfEPXVxhFqA#q#M",0));
    BootstrapAudioBackend backend={.initialized_subsystems=0x10,.driver="coreaudio",
        .open=open_stream,.resume=resume,.destroy=destroy,.device=stream_device,.device_format=device_format};
    const int missing_query=argc==2 && !strcmp(argv[1],"--missing-query");
    if(argc==2) {
        if(missing_query) backend.device_format=NULL;
        else if(!strcmp(argv[1],"--unavailable")) backend.driver="dummy";
        else if(!strcmp(argv[1],"--not-ready")) backend.initialized_subsystems=0;
        else { assert(!strcmp(argv[1],"--missing-callback"));backend.open=NULL; }
    }
    assert(bootstrap_audio_bind(&backend)==(argc==2 && !missing_query ? -1:0));
    assert(bootstrap_audio_bind(&backend)==-1);
    uintptr_t init=bridge("JfEPXVxhFqA#q#M"),open=bridge("ekNvsT22rsY#q#M"),close=bridge("s1--uE9mBFw#q#M");
    uintptr_t state=bridge("GrQ9s4IrNaQ#q#M");
    assert(!bootstrap_resolve("GrQ9s4IrNaQ#G#A",0));
    assert(!bootstrap_resolve("JfEPXVxhFqA#G#H",0));
    assert(bootstrap_resolve("w3PdaSTSwGE#q#M",0) && bootstrap_resolve("QOQtbeDqsT4#q#M",0));
    assert(!bootstrap_resolve("b+uAV89IlxE#q#M",0)); /* Volume updates stay unresolved. */
    *runtime_errno()=77;
    unsigned char output[32],unchanged[32];memset(output,0xa5,32);memcpy(unchanged,output,32);
    assert((uint32_t)call(state,0,0,0,0,0,0)==0x80260004);
    assert((uint32_t)call(state,0,(uintptr_t)output,0,0,0,0)==0x8026000f && !memcmp(output,unchanged,32));
    assert((uint32_t)call(open,1,0,0,256,48000,1)==0x8026000f);
    if(argc==2 && !missing_query) {
        for(int i=0;i<2;++i) assert((uint32_t)call(init,1,2,3,4,5,6)==0x80260010);
        assert(!created && !resumed && !destroyed && *runtime_errno()==77);
        puts("PASS: unavailable/dummy audio backend rejects initialization without claiming ports");return 0;
    }
    assert(call(init,1,2,3,4,5,6)==0); /* No-argument ABI ignores stale registers. */
    assert((uint32_t)call(init,0,0,0,0,0,0)==0x8026000e);
    assert((uint32_t)call(state,0,0,0,0,0,0)==0x80260004);
    assert((uint32_t)call(state,0,1,0,0,0,0)==0x80260003); /* Invalid handle must not touch output. */
    assert((uint32_t)call(open,1,0,0,0,48000,1)==0x80260006);
    assert((uint32_t)call(open,1,0,0,257,48000,1)==0x80260006);
    assert((uint32_t)call(open,1,0,0,2304,48000,1)==0x80260006);
    assert((uint32_t)call(open,1,0,0,256,44100,1)==0x80260008);
    assert((uint32_t)call(open,1,0,0,256,48000,8)==0x80260007);
    const uint32_t invalid_params[]={0x10008,0x100,0x20005,0x30005,0x40005,0x80000005,UINT32_MAX};
    for(unsigned i=0;i<sizeof(invalid_params)/sizeof(*invalid_params);++i)
        assert((uint32_t)call(open,255,0,0,256,48000,invalid_params[i])==0x80260007);
    assert((uint32_t)call(open,1,4,0,256,48000,1)==0x8026000a);
    assert((uint32_t)call(open,1,0,1,256,48000,1)==0x8026000a);
    assert((uint32_t)call(open,2,0,0,256,48000,1)==0x8026000a);
    const int32_t invalid_users[]={-1,0,254,256};
    for(unsigned i=0;i<sizeof(invalid_users)/sizeof(*invalid_users);++i)
        assert((uint32_t)call(open,(uint32_t)invalid_users[i],0,0,256,48000,5)==0x8026000a);
    assert(!created);
    open_fail=1;assert((uint32_t)call(open,255,0,0,256,48000,5)==0x80260010 && !created);open_fail=0;
    resume_fail=1;assert((uint32_t)call(open,255,0,0,256,48000,0x10005)==0x80260010);
    assert(created==1 && destroyed==1);resume_fail=0;
    int32_t system=call(open,UINT64_C(0x1000000ff),0,0,256,48000,5);
    assert(system>0 && received.channels==8 && received.format==0x8120 && received.frequency==48000);
    if(missing_query) {
        assert((uint32_t)call(state,(uint32_t)system,(uintptr_t)output,0,0,0,0)==0x80260010);
        assert(!memcmp(output,unchanged,32));assert(call(close,(uint32_t)system,0,0,0,0,0)==0);
        puts("PASS: missing SDL device query rejects port state without changing output");return 0;
    }
    check_state(state,system,2);
    const int physical_channels[]={1,6,8,16};
    for(unsigned i=0;i<sizeof(physical_channels)/sizeof(*physical_channels);++i) {
        physical.channels=physical_channels[i];check_state(state,system,physical.channels>8 ? 8:physical.channels);
    }
    physical.channels=2;
    for(int failure=0;failure<5;++failure) {
        no_device=failure==0;query_fail=failure==1;
        physical.channels=failure==2 ? 0:2;physical.frequency=failure==3 ? 0:48000;
        physical.format=failure==4 ? 0:0x8120;
        assert((uint32_t)call(state,(uint32_t)system,(uintptr_t)output,0,0,0,0)==0x80260010);
        assert(!memcmp(output,unchanged,32));
    }
    no_device=query_fail=0;physical=(BootstrapAudioSpec){0x8120,2,48000};
    unsigned char *pages=mmap(NULL,32768,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);assert(pages!=MAP_FAILED);
    assert(!mprotect(pages,16384,PROT_READ|PROT_WRITE));memset(pages,0xa5,16384);
    assert(!call(state,(uint32_t)system,(uintptr_t)(pages+16384-16),0,0,0,0));
    assert(pages[16384-17]==0xa5 && pages[16384-13]==0xa5);assert(!munmap(pages,32768));
    int32_t restricted=call(open,255,0,0,256,48000,UINT64_C(0x100010005));
    assert(restricted>0 && restricted!=system && received.channels==8 && received.format==0x8120 && received.frequency==48000);
    check_state(state,restricted,2);
    assert(call(close,(uint32_t)restricted,0,0,0,0,0)==0);
    assert(call(close,(uint32_t)system,0,0,0,0,0)==0);
    const int queries_before=device_queries;
    assert((uint32_t)call(state,(uint32_t)system,(uintptr_t)output,0,0,0,0)==0x80260003);
    assert(device_queries==queries_before && !memcmp(output,unchanged,32));
    for(unsigned attr=0;attr<=0x10000;attr+=0x10000) for(unsigned format=0;format<8;++format) {
        int32_t handle=call(open,1,0,0,256,48000,attr|format);assert(handle>0);
        static const int channels[]={1,2,8,1,2,8,8,8};
        assert(received.channels==channels[format] && received.frequency==48000);
        assert(received.format==(format>=3 && format!=6 ? 0x8120:0x8010));
        check_state(state,handle,channels[format]>2 ? 2:channels[format]);
        assert(call(close,(uint32_t)handle,0,0,0,0,0)==0);
        assert((uint32_t)call(close,(uint32_t)handle,0,0,0,0,0)==0x80260003);
    }
    int32_t handles[9];
    for(int i=0;i<9;++i) { handles[i]=call(open,i%2 ? 1:255,i==8,0,256,48000,i%2 ? 1:0x10001);assert(handles[i]>0); }
    assert((uint32_t)call(open,255,0,0,256,48000,1)==0x80260005);
    assert((uint32_t)call(open,1,1,0,256,48000,1)==0x80260005);
    for(int i=0;i<9;++i) assert(call(close,(uint32_t)handles[i],0,0,0,0,0)==0);
    int32_t again=call(open,1,0,0,UINT64_C(0x100000100),UINT64_C(0x10000bb80),UINT64_C(0x100000001));
    assert(again>0 && again!=handles[0]);assert(call(close,(uint32_t)again,0,0,0,0,0)==0);
    assert(created==destroyed && *runtime_errno()==77);
    puts("PASS: AudioOut guest lifecycle/state ABI, local/system users, restricted attributes, formats, live device queries, guarded/unaligned output, reserved bytes, errors, shared capacity and stream ownership contract");
    return 0;
}
