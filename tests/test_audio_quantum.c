/* Original deterministic CoreAudio-like device double. No sound or real clocks. */
#include "bootstrap_audio.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
static uint64_t now=1000000000, next_callback;
static int pending[2], opened;
static uint64_t submitted[2], consumed[2], deficits[2];
static unsigned char streams[2];
enum { Quantum=1024, FrameBytes=32 };
static void advance(uint64_t target) {
    while(next_callback<=target) {
        for(int i=0;i<opened;++i) {
            const int frames=pending[i]/FrameBytes;
            const int took=frames<Quantum ? frames:Quantum;
            consumed[i]+=(unsigned)took;deficits[i]+=(unsigned)(Quantum-took);
            pending[i]-=took*FrameBytes;
        }
        next_callback+=(uint64_t)Quantum*1000000000/48000;
    }
    now=target;
}
int sotc_fixture_clock_gettime(clockid_t id,struct timespec *out) {
    assert(id==CLOCK_MONOTONIC);*out=(struct timespec){(time_t)(now/1000000000),(long)(now%1000000000)};return 0;
}
void bb_sleep_until(uint64_t due) { if(due>now)advance(due); }
size_t bb_read_memory(void *dst,uintptr_t src,size_t n) { memcpy(dst,(const void *)src,n);return n; }
static void *open_stream(uint32_t device,const BootstrapAudioSpec *spec,void *cb,void *data) {
    assert(device==UINT32_MAX && spec->channels==8 && spec->frequency==48000 && !cb && !data && opened<2);
    return &streams[opened++];
}
static bool resume(void *stream) { assert(stream);return true; }
static void destroy(void *stream) { assert(stream); }
static uint32_t device(void *stream) { assert(stream);return 10; }
static bool format(uint32_t dev,BootstrapAudioSpec *spec,int *frames) {
    assert(dev==10);*spec=(BootstrapAudioSpec){0x8120,2,48000};if(frames)*frames=Quantum;return true;
}
static bool put(void *stream,const void *pcm,int bytes) {
    const int i=(int)((unsigned char *)stream-streams);assert(i>=0 && i<2 && bytes==8192);
    for(int n=0;n<bytes;++n)assert(((const unsigned char *)pcm)[n]==0x5a);
    pending[i]+=bytes;++submitted[i];return true;
}
static int queued(void *stream) { return pending[(unsigned char *)stream-streams]; }
static bool flush(void *stream) { assert(stream);return true; }
int main(void) {
    const BootstrapAudioBackend backend={.initialized_subsystems=0x10,.driver="coreaudio",.open=open_stream,
        .resume=resume,.destroy=destroy,.device=device,.device_format=format,.put=put,.queued=queued,.flush=flush};
    assert(!bootstrap_audio_bind(&backend));
    assert(!((int32_t(*)(void))bootstrap_audio_resolve("JfEPXVxhFqA#q#M"))());
    int32_t (*open_port)(int32_t,int32_t,int32_t,uint32_t,uint32_t,uint32_t)=(void *)bootstrap_audio_resolve("ekNvsT22rsY#q#M");
    int32_t (*output)(const void *,uint32_t)=(void *)bootstrap_audio_resolve("w3PdaSTSwGE#q#M");
    unsigned char pcm[8192];memset(pcm,0x5a,sizeof(pcm));
    struct {int32_t handle;uint32_t pad;uint64_t data;} batch[2];
    for(unsigned i=0;i<2;++i)batch[i]=(typeof(batch[0])){open_port(255,0,0,256,48000,5),0,(uintptr_t)pcm};
    next_callback=now+(uint64_t)Quantum*1000000000/48000;
    const uint64_t stop=now+5000000000;
    while(now<stop)assert(output(batch,2)==256);
    const int fixed=submitted[0]>=930 && consumed[0]>=235000;
    assert(submitted[0]==submitted[1] && consumed[0]==consumed[1]);
    printf("buffers_per_port=%llu input_frames_consumed=%llu input_deficit=%llu simulation_seconds=5\n",
        (unsigned long long)submitted[0],(unsigned long long)consumed[0],(unsigned long long)deficits[0]);
    return fixed ? 0:2;
}
