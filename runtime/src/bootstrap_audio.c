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
#define AUDIO_ATTR_RESTRICTED UINT32_C(0x10000)
enum { PORTS=9 };
_Static_assert(sizeof(BootstrapAudioSpec)==12, "pinned SDL3 audio-spec ABI");
static pthread_mutex_t audio_lock=PTHREAD_MUTEX_INITIALIZER;
static BootstrapAudioBackend backend;
static int configured, ready, initialized;
static uint32_t sequence;
static struct { int32_t handle; int type, restricted, channels; void *stream; } ports[PORTS];
static uint64_t init_requests, open_requests, opened, closed, failures, state_queries;
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
    const uint32_t format=param&0xff;
    int32_t result = !initialized ? AUDIO_ERROR(0xf)
        : !frames || frames>2048 || (frames&255) ? AUDIO_ERROR(6)
        : freq!=48000 ? AUDIO_ERROR(8)
        : format>7 || (param & ~(AUDIO_ATTR_RESTRICTED|UINT32_C(0xff))) ? AUDIO_ERROR(7)
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
        const BootstrapAudioSpec spec={.format=format>=3 && format!=6 ? 0x8120 : 0x8010,
            .channels=channels[format],.frequency=48000};
        void *stream=backend.open(UINT32_MAX,&spec,NULL,NULL);
        if (!stream || !backend.resume(stream)) {
            if(stream) backend.destroy(stream);
            ++failures;result=AUDIO_ERROR(0x10);
        } else {
            result=(int32_t)(UINT32_C(0x20000000)|++sequence);
            ports[slot].handle=result;ports[slot].type=type;ports[slot].stream=stream;++opened;
            ports[slot].channels=spec.channels;
            /* Restricted is a port attribute, never a sample-format index.
             * These streams only target playback: no capture, loopback or
             * audio dump path is exposed by this adapter. */
            ports[slot].restricted=(param&AUDIO_ATTR_RESTRICTED)!=0;
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
static int32_t audio_state(int32_t handle, void *output) {
    /* Retail checks null first and writes fields through byte 15 only. The
     * padding byte at 3 and reserved words at 16..31 belong to the caller. */
    if(!output) return AUDIO_ERROR(4);
    pthread_mutex_lock(&audio_lock);++state_queries;
    int32_t result=!initialized ? AUDIO_ERROR(0xf) : AUDIO_ERROR(3);
    if(initialized) for(int i=0;i<PORTS;++i) if(ports[i].stream && ports[i].handle==handle) {
        BootstrapAudioSpec spec={0};
        const uint32_t device=backend.device ? backend.device(ports[i].stream) : 0;
        if(!device || !backend.device_format || !backend.device_format(device,&spec,NULL) ||
           spec.channels<=0 || spec.frequency<=0 || !spec.format) {
            result=AUDIO_ERROR(0x10);break;
        }
        /* Main/BGM use the logical primary output route. Mono/stereo ports
         * retain their retail channel count; multichannel follows the bound
         * device, capped by the actual stream's input channels. */
        const uint16_t route=1, volume=UINT16_MAX, reroute=0;
        const uint8_t channels=(uint8_t)(ports[i].channels<=2 || ports[i].channels<spec.channels
            ? ports[i].channels : spec.channels);
        const uint64_t flags=0;
        unsigned char *out=output;
        memcpy(out,&route,2);out[2]=channels;
        memcpy(out+4,&volume,2);memcpy(out+6,&reroute,2);memcpy(out+8,&flags,8);
        result=0;break;
    }
    pthread_mutex_unlock(&audio_lock);return result;
}
uintptr_t bootstrap_audio_resolve(const char *name) {
    if(!configured) return 0;
    if(!strcmp(name,"JfEPXVxhFqA#q#M")) return (uintptr_t)audio_init;
    if(!strcmp(name,"ekNvsT22rsY#q#M")) return (uintptr_t)audio_open;
    if(!strcmp(name,"s1--uE9mBFw#q#M")) return (uintptr_t)audio_close;
    if(!strcmp(name,"GrQ9s4IrNaQ#q#M")) return (uintptr_t)audio_state;
    return 0;
}
void bootstrap_audio_report(void) {
    pthread_mutex_lock(&audio_lock);
    unsigned restricted=0;
    for(int i=0;i<PORTS;++i) if(ports[i].stream && ports[i].restricted) ++restricted;
    if(configured) printf("AudioOut: SDL ready=%d, initialized=%d, init requests=%llu, streams opened=%llu, closed=%llu, failures=%llu, restricted live=%u, state queries=%llu (no output submitted by this adapter)\n",
        ready,initialized,(unsigned long long)init_requests,(unsigned long long)opened,
        (unsigned long long)closed,(unsigned long long)failures,restricted,(unsigned long long)state_queries);
    pthread_mutex_unlock(&audio_lock);
}
