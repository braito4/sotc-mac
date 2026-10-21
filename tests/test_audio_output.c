/* Original synthetic PCM: no speakers, files or game audio are used. */
#define main lifecycle_fixture_main
#include "test_audio.c"
#undef main
#include <time.h>
static unsigned char captured[32][65536];
static int lengths[32],put_calls,accepted,flush_calls,queue_calls;
static int put_fail_at,queue_fail,flush_fail,pending,drain_step,stalled;
static uint64_t now_ns(void) {
    struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));
    return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec;
}
static bool put_pcm(void *stream,const void *data,int bytes) {
    assert(stream && data && bytes>0 && bytes<=65536);
    ++put_calls;
    if(put_fail_at && put_calls==put_fail_at) return false;
    assert(pending<=bytes); /* At most one old input buffer before this put. */
    const size_t i=(unsigned char *)stream-streams;assert(i<32);
    lengths[i]=bytes;memcpy(captured[i],data,(size_t)bytes);++accepted;return true;
}
static int queued_pcm(void *stream) {
    assert(stream);++queue_calls;
    if(queue_fail)return -1;
    const int result=pending;
    if(!stalled && pending>0) pending=pending>drain_step ? pending-drain_step:0;
    return result;
}
static bool flush_pcm(void *stream) { assert(stream);++flush_calls;return !flush_fail; }
typedef struct { int32_t handle; uint32_t padding; uint64_t data; } Descriptor;
_Static_assert(sizeof(Descriptor)==16,"independent guest batch layout");
static int32_t batch_call(uintptr_t output,const Descriptor *batch,uint64_t count) {
    return call(output,(uintptr_t)batch,count,0,0,0,0);
}
int main(int argc,char **argv) {
    bootstrap_initialize(1);
    assert(!bootstrap_resolve("w3PdaSTSwGE#q#M",0));
    BootstrapAudioBackend b={.initialized_subsystems=0x10,.driver="coreaudio",.open=open_stream,
        .resume=resume,.destroy=destroy,.device=stream_device,.device_format=device_format,
        .put=put_pcm,.queued=queued_pcm,.flush=flush_pcm};
    if(argc==2) { assert(!strcmp(argv[1],"--missing-output"));b.put=NULL; }
    assert(!bootstrap_audio_bind(&b));
    uintptr_t init=bridge("JfEPXVxhFqA#q#M"),open=bridge("ekNvsT22rsY#q#M"),close=bridge("s1--uE9mBFw#q#M");
    uintptr_t single=bridge("QOQtbeDqsT4#q#M"),multi=bridge("w3PdaSTSwGE#q#M");
    assert(!bootstrap_resolve("w3PdaSTSwGE#G#A",0) && !bootstrap_resolve("w3PdaSTSwGE#q#M",1));
    *runtime_errno()=77;
    assert((uint32_t)batch_call(multi,NULL,0)==0x8026000f);
    assert(call(init,0,0,0,0,0,0)==0);
    assert((uint32_t)batch_call(multi,NULL,0)==0x80260005);
    assert((uint32_t)batch_call(multi,NULL,1)==0x80260004);
    assert((uint32_t)batch_call(multi,(const void *)1,26)==0x80260005);
    unsigned char source[65537],expected[65536];
    for(unsigned i=0;i<sizeof(source);++i) source[i]=(unsigned char)(i*37+13);
    if(argc==2) {
        int32_t h=call(open,255,0,0,256,48000,5);assert(h>0);
        assert((uint32_t)call(single,(uint32_t)h,(uintptr_t)(source+1),0,0,0,0)==0x80260010);
        assert(!accepted && !put_calls && *runtime_errno()==77);
        assert(!call(close,(uint32_t)h,0,0,0,0,0));
        puts("PASS: missing SDL PCM submission cannot claim output success");return 0;
    }
    const unsigned channels[]={1,2,8,1,2,8,8,8};
    for(unsigned format=0;format<8;++format) {
        const unsigned sample_bytes=format>=3 && format!=6 ? 4:2,stride=channels[format]*sample_bytes;
        int32_t h=call(open,255,0,0,256,48000,format|0x10000);assert(h>0);
        const int stream=created-1,bytes=(int)(256*stride);
        memcpy(expected,source+1,(size_t)bytes);
        if(format==2 || format==5) {
            /* Independently swap the rear/side stereo pairs in each frame. */
            for(unsigned f=0;f<256;++f) {
                memcpy(expected+f*stride+4*sample_bytes,source+1+f*stride+6*sample_bytes,2*sample_bytes);
                memcpy(expected+f*stride+6*sample_bytes,source+1+f*stride+4*sample_bytes,2*sample_bytes);
            }
        }
        assert(call(single,(uint32_t)h,(uintptr_t)(source+1),0,0,0,0)==256);
        assert(lengths[stream]==bytes && !memcmp(captured[stream],expected,(size_t)bytes));
        assert(!call(close,(uint32_t)h,0,0,0,0,0));
        assert((uint32_t)call(single,(uint32_t)h,1,0,0,0,0)==0x80260003);
    }
    int32_t handles[4];
    for(int i=0;i<3;++i) { handles[i]=call(open,255,i==2,0,256,48000,i==1 ? 0x10005:5);assert(handles[i]>0); }
    handles[3]=call(open,255,0,0,512,48000,5);assert(handles[3]>0);
    Descriptor params[25];
    for(int i=0;i<3;++i) params[i]=(Descriptor){handles[i],0xa5a5a5a5,(uintptr_t)(source+1)};
    unsigned char unaligned[49],before[49];memset(unaligned,0xa5,sizeof(unaligned));
    memcpy(unaligned+1,params,48);memcpy(before,unaligned,sizeof(before));
    int prior=accepted;
    assert(batch_call(multi,(const void *)(unaligned+1),UINT64_C(0x100000003))==256);
    assert(accepted==prior+3 && !memcmp(before,unaligned,sizeof(before)));
    uint64_t started=now_ns();assert(batch_call(multi,params,3)==256);
    assert(now_ns()-started>=UINT64_C(4000000)); /* The batch is paced once. */
    prior=accepted;
    Descriptor bad[3];memcpy(bad,params,sizeof(bad));bad[2].handle=handles[3];
    assert((uint32_t)batch_call(multi,bad,3)==0x80260006 && accepted==prior);
    bad[2]=params[0];assert((uint32_t)batch_call(multi,bad,3)==0x80260003 && accepted==prior);
    bad[2]=params[2];bad[2].handle=0;assert((uint32_t)batch_call(multi,bad,3)==0x80260003 && accepted==prior);
    bad[2]=params[2];bad[2].data=1;assert((uint32_t)batch_call(multi,bad,3)==0x80260004 && accepted==prior);
    for(int i=0;i<25;++i) params[i]=params[0];
    assert((uint32_t)batch_call(multi,params,25)==0x80260003 && accepted==prior);
    unsigned char *pages=mmap(NULL,32768,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);assert(pages!=MAP_FAILED);
    assert(!mprotect(pages,16384,PROT_READ|PROT_WRITE));
    assert((uint32_t)batch_call(multi,(const void *)(pages+16384-15),1)==0x80260004 && accepted==prior);
    assert((uint32_t)call(single,(uint32_t)handles[0],(uintptr_t)(pages+16384-8191),0,0,0,0)==0x80260004 && accepted==prior);
    assert(!munmap(pages,32768));
    queue_fail=1;assert((uint32_t)call(single,(uint32_t)handles[0],(uintptr_t)source,0,0,0,0)==0x80260010 && accepted==prior);queue_fail=0;
    for(int i=0;i<3;++i) params[i]=(Descriptor){handles[i],0,(uintptr_t)source};
    put_fail_at=put_calls+2;
    assert((uint32_t)batch_call(multi,params,3)==0x80260010 && accepted==prior+1);put_fail_at=0;
    pending=8192*7;drain_step=8192;
    const int queries_before=queue_calls;
    assert(batch_call(multi,params,3)==256 && queue_calls>queries_before+6 && !pending);
    prior=accepted;
    flush_fail=1;assert((uint32_t)call(single,(uint32_t)handles[0],0,0,0,0,0)==0x80260010 && accepted==prior);flush_fail=0;
    pending=16384;drain_step=8192;
    assert(call(single,(uint32_t)handles[0],0,0,0,0,0)==256 && !pending && accepted==prior && flush_calls>=2);
    pending=8193;stalled=1;started=now_ns();
    assert((uint32_t)call(single,(uint32_t)handles[0],(uintptr_t)source,0,0,0,0)==0x80260010 && accepted==prior);
    assert(now_ns()-started>=UINT64_C(1900000000));pending=stalled=0;
    for(unsigned i=0;i<4;++i) assert(!call(close,(uint32_t)handles[i],0,0,0,0,0));
    int32_t maximum[9];const int first_stream=created;
    for(unsigned i=0;i<9;++i) {
        maximum[i]=call(open,255,i==8,0,2048,48000,7);assert(maximum[i]>0);
        params[i]=(Descriptor){maximum[i],0,(uintptr_t)(source+1)};
    }
    prior=accepted;assert(batch_call(multi,params,9)==2048 && accepted==prior+9);
    for(unsigned i=0;i<9;++i) {
        assert(lengths[first_stream+i]==65536 && !memcmp(captured[first_stream+i],source+1,65536));
        assert(!call(close,(uint32_t)maximum[i],0,0,0,0,0));
    }
    assert(created==destroyed && *runtime_errno()==77);
    puts("PASS: AudioOut single/batch guest PCM ABI, exact bytes and channel ordering, frame returns, validation before submission, protected buffers, device errors, partial failure, one batch cadence, bounded queue/drain and stream ownership");
    return 0;
}
