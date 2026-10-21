/* Checked AudioOut initialization/stream lifecycle using actual SDL3 playback.
 * ABI/errors from the pinned bloodborne-runtime runtime_audio.c and SotC
 * reference, GPL-2.0-or-later. No silent timer port or PCM dump is created.
 * Output submission and remaining AudioOut APIs stay unresolved for now.
 */
#include "bootstrap_audio.h"
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#define AUDIO_ERROR(n) ((int32_t)(UINT32_C(0x80260000) | (n)))
enum { PORTS=9 };
_Static_assert(sizeof(BootstrapAudioSpec)==12, "pinned SDL3 audio-spec ABI");
static pthread_mutex_t audio_lock=PTHREAD_MUTEX_INITIALIZER;
static BootstrapAudioBackend backend;
static int configured, ready, initialized;
static uint32_t sequence;
static struct { int32_t handle; int type; void *stream; } ports[PORTS];
static uint64_t init_requests, open_requests, opened, closed, failures;
int bootstrap_audio_bind(const BootstrapAudioBackend *input) {
    pthread_mutex_lock(&audio_lock);
    if (configured) { pthread_mutex_unlock(&audio_lock); return -1; }
    configured=1;
    ready=input && (input->initialized_subsystems & 0x10) && input->driver && !strcmp(input->driver,"coreaudio") &&
          input->open && input->resume && input->destroy;
    if (ready) backend=*input;
    pthread_mutex_unlock(&audio_lock);
    return ready ? 0 : -1;
}
static int32_t audio_init(void) {
    pthread_mutex_lock(&audio_lock);++init_requests;
    int32_t result = !ready ? AUDIO_ERROR(0x10) : initialized ? AUDIO_ERROR(0xe) : 0;
    if (!result) initialized=1;
    if (init_requests<=2)
        fprintf(stderr,"AudioOut: init result=0x%08x (SDL ready=%d)\n",(uint32_t)result,ready);
    pthread_mutex_unlock(&audio_lock);return result;
}
static int32_t audio_open(int32_t user,int32_t type,int32_t index,uint32_t frames,uint32_t freq,uint32_t param) {
    pthread_mutex_lock(&audio_lock);
    ++open_requests;
    int32_t result = !initialized ? AUDIO_ERROR(0xf)
        : !frames || frames>2048 || (frames&255) ? AUDIO_ERROR(6)
        : freq!=48000 ? AUDIO_ERROR(8)
        : param>7 ? AUDIO_ERROR(7)
        /* Main/BGM output may belong to the console system user (0xff),
         * independently of the keyboard profile's signed-in local user. */
        : (user!=1 && user!=255) || type<0 || type>1 || index ? AUDIO_ERROR(0xa) : 0;
    int slot=-1;
    if (!result) {
        for(int i=0;i<PORTS;++i) if (!ports[i].stream && ((type==0 && i<8)||(type==1 && i==8))) { slot=i;break; }
        if(slot<0 || sequence==0xfffffff) result=AUDIO_ERROR(5);
    }
    if (!result) {
        static const int channels[]={1,2,8,1,2,8,8,8};
        const BootstrapAudioSpec spec={.format=param>=3 && param!=6 ? 0x8120 : 0x8010,
            .channels=channels[param],.frequency=48000};
        void *stream=backend.open(UINT32_MAX,&spec,NULL,NULL);
        if (!stream || !backend.resume(stream)) {
            if(stream) backend.destroy(stream);
            ++failures;result=AUDIO_ERROR(0x10);
        } else {
            result=(int32_t)(UINT32_C(0x20000000)|++sequence);
            ports[slot].handle=result;ports[slot].type=type;ports[slot].stream=stream;++opened;
        }
    }
    if (open_requests<=16)
        fprintf(stderr,"AudioOut: open user=%d type=%d index=%d frames=%u frequency=%u parameter=0x%x result=0x%08x\n",
            user,type,index,frames,freq,param,(uint32_t)result);
    pthread_mutex_unlock(&audio_lock);return result;
}
static int32_t audio_close(int32_t handle) {
    pthread_mutex_lock(&audio_lock);
    int32_t result=AUDIO_ERROR(3);
    for(int i=0;i<PORTS;++i) if(ports[i].stream && ports[i].handle==handle) {
        backend.destroy(ports[i].stream);memset(&ports[i],0,sizeof(ports[i]));++closed;result=0;break;
    }
    pthread_mutex_unlock(&audio_lock);return result;
}
uintptr_t bootstrap_audio_resolve(const char *name) {
    if(!configured) return 0;
    if(!strcmp(name,"JfEPXVxhFqA#q#M")) return (uintptr_t)audio_init;
    if(!strcmp(name,"ekNvsT22rsY#q#M")) return (uintptr_t)audio_open;
    if(!strcmp(name,"s1--uE9mBFw#q#M")) return (uintptr_t)audio_close;
    return 0;
}
void bootstrap_audio_report(void) {
    pthread_mutex_lock(&audio_lock);
    if(configured) printf("AudioOut: SDL ready=%d, initialized=%d, init requests=%llu, streams opened=%llu, closed=%llu, failures=%llu (no output submitted by this adapter)\n",
        ready,initialized,(unsigned long long)init_requests,(unsigned long long)opened,
        (unsigned long long)closed,(unsigned long long)failures);
    pthread_mutex_unlock(&audio_lock);
}
