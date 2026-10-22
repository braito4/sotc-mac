/* Checked AudioOut initialization/stream lifecycle using actual SDL3 playback.
 * ABI/errors from the pinned bloodborne-runtime runtime_audio.c and SotC
 * reference, GPL-2.0-or-later. No silent timer port or PCM dump is created.
 * Output submission uses actual SDL queues; volume updates remain unresolved.
 */
#include "bootstrap_audio.h"
#include "audio_trace_metrics.h"
#include "platform.h"
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define AUDIO_ERROR(n) ((int32_t)(UINT32_C(0x80260000) | (n)))
#define AUDIO_ATTR_RESTRICTED UINT32_C(0x10000)
enum { PORTS=9 };
_Static_assert(sizeof(BootstrapAudioSpec)==12, "pinned SDL3 audio-spec ABI");
static pthread_mutex_t audio_lock=PTHREAD_MUTEX_INITIALIZER;
static BootstrapAudioBackend backend;
static int configured, ready, initialized;
static uint32_t sequence;
typedef struct {
    int32_t handle;
    int type, restricted, channels, sample_bytes, standard_layout;
    uint32_t frames;
    uint64_t next_deadline;
    SotcAudioMetrics metrics;
    void *stream;
} AudioPort;
static AudioPort ports[PORTS];
static uint64_t init_requests, open_requests, opened, closed, failures, state_queries;
static uint64_t output_calls, output_errors, submitted_buffers, submitted_bytes, drains;
static int trace_enabled, trace_open_attempted;
static unsigned trace_lines, trace_error_lines;
static FILE *trace_file;
static uint64_t audio_now(void);
/* audio_lock serializes all trace state. Input queue zero is not a device
 * underrun: SDL_GetAudioStreamQueued reports unconverted input bytes only. */
static void audio_trace_write(const char *line) {
    if(!trace_enabled || trace_lines>=1024) return;
    if(!trace_open_attempted) {
        trace_open_attempted=1;
        const char *root=getenv("BB_GPU_USER_DIR");
        char path[PATH_MAX];
        const int n=root && root[0]=='/' ? snprintf(path,sizeof(path),"%s/audio.jsonl",root):-1;
        if(n>0 && (size_t)n<sizeof(path)) trace_file=fopen(path,"wx");
    }
    fprintf(stderr,"AudioTrace: %s\n",line);
    if(trace_file) { fprintf(trace_file,"%s\n",line);fflush(trace_file); }
    ++trace_lines;
}
static void audio_trace_port(AudioPort *p, uint64_t now, int boundary) {
    SotcAudioMetrics *s=&p->metrics;
    if(!trace_enabled || (!s->buffers && !s->queue_wait_failures) ||
       (!boundary && now-s->start_ns<UINT64_C(5000000000))) return;
    static const char *reasons[]={"periodic","close","drain","queue_or_put_error","report"};
    char line[1400];
    snprintf(line,sizeof(line),
        "{\"schema\":1,\"event\":\"audio_window\",\"monotonic_ns\":%llu,\"port\":%d,"
        "\"boundary\":\"%s\",\"frames\":%u,\"channels\":%d,\"period_us\":%llu,\"window_us\":%llu,"
        "\"accepted_buffers\":%llu,\"accepted_bytes\":%llu,\"max_submit_gap_us\":%llu,"
        "\"gaps_over_two_periods\":%llu,\"max_deadline_late_us\":%llu,\"late_over_period\":%llu,"
        "\"input_queue_min_bytes\":%d,\"input_queue_max_bytes\":%d,\"input_queue_samples\":%llu,"
        "\"input_queue_zero_samples\":%llu,\"queue_wait_us\":%llu,\"max_queue_wait_us\":%llu,"
        "\"queue_wait_failures\":%llu,\"max_output_lock_wait_us\":%llu}",
        (unsigned long long)now,p->handle,reasons[boundary],p->frames,p->channels,
        (unsigned long long)((uint64_t)p->frames*1000000/48000),
        (unsigned long long)((now>=s->start_ns ? now-s->start_ns:0)/1000),
        (unsigned long long)s->buffers,(unsigned long long)s->bytes,
        (unsigned long long)(s->max_gap_ns/1000),(unsigned long long)s->gaps_over_two_periods,
        (unsigned long long)(s->max_late_ns/1000),(unsigned long long)s->late_over_period,
        s->input_samples ? s->input_min:-1,s->input_max,(unsigned long long)s->input_samples,
        (unsigned long long)s->input_zero_samples,(unsigned long long)(s->queue_wait_ns/1000),
        (unsigned long long)(s->max_queue_wait_ns/1000),(unsigned long long)s->queue_wait_failures,
        (unsigned long long)(s->max_lock_wait_ns/1000));
    audio_trace_write(line);sotc_audio_reset(s,now);
}
int bootstrap_audio_bind(const BootstrapAudioBackend *input) {
    pthread_mutex_lock(&audio_lock);
    if (configured) { pthread_mutex_unlock(&audio_lock); return -1; }
    configured=1;
    const char *trace=getenv("SOTC_AUDIO_TRACE");
    trace_enabled=trace && !strcmp(trace,"1");
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
            ports[slot].frames=frames;ports[slot].sample_bytes=spec.format==0x8120 ? 4:2;
            ports[slot].standard_layout=format>=6;
            sotc_audio_reset(&ports[slot].metrics,audio_now());
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
        audio_trace_port(&ports[i],audio_now(),1);
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
typedef struct { int32_t handle; uint32_t padding; uint64_t data; } AudioOutput;
_Static_assert(sizeof(AudioOutput)==16,"retail AudioOut descriptor ABI");
static uint64_t audio_now(void) {
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC,&now)) return 0;
    return (uint64_t)now.tv_sec*UINT64_C(1000000000)+(uint64_t)now.tv_nsec;
}
static int wait_queue(AudioPort *port,int limit,uint64_t timeout,int *level) {
    for(;;) {
        const int queued=backend.queued(port->stream);
        if(level) *level=queued;
        if(queued<0) return -1;
        if(queued<=limit) return 0;
        const uint64_t now=audio_now();
        if(!now || now>=timeout) return -1;
        bb_sleep_until(now+UINT64_C(500000));
    }
}
/* Called under audio_lock: close cannot destroy a stream during an output or
 * drain. Validate the entire batch and stage guest bytes before submitting any
 * PCM. A later provider failure may still leave earlier streams submitted. */
static int32_t output_batch(const void *parameters,uint32_t count,uint64_t lock_wait) {
    if(!initialized) return AUDIO_ERROR(0xf);
    if(!count || count>25) return AUDIO_ERROR(5);
    if(!parameters || (uintptr_t)parameters>UINTPTR_MAX-count*sizeof(AudioOutput)) return AUDIO_ERROR(4);
    AudioOutput batch[25];
    AudioPort *selected[25];
    size_t offsets[25],total=0;
    if(bb_read_memory(batch,(uintptr_t)parameters,count*sizeof(*batch))!=count*sizeof(*batch)) return AUDIO_ERROR(4);
    uint64_t due=0;
    for(uint32_t i=0;i<count;++i) {
        for(uint32_t j=0;j<i;++j) if(batch[j].handle==batch[i].handle) return AUDIO_ERROR(3);
        selected[i]=NULL;
        for(unsigned j=0;j<PORTS;++j) if(ports[j].stream && ports[j].handle==batch[i].handle) selected[i]=&ports[j];
        if(!selected[i]) return AUDIO_ERROR(3);
        if(i && selected[i]->frames!=selected[0]->frames) return AUDIO_ERROR(6);
        offsets[i]=total;
        const size_t bytes=(size_t)selected[i]->frames*(unsigned)selected[i]->channels*(unsigned)selected[i]->sample_bytes;
        if(batch[i].data) {
            if(batch[i].data>UINTPTR_MAX-bytes) return AUDIO_ERROR(4);
            total+=bytes;
        }
        if(selected[i]->next_deadline>due) due=selected[i]->next_deadline;
    }
    if(!backend.put || !backend.queued || !backend.device || !backend.device_format) return AUDIO_ERROR(0x10);
    for(uint32_t i=0;i<count;++i) {
        BootstrapAudioSpec spec={0};
        const uint32_t device=backend.device(selected[i]->stream);
        if(!device || !backend.device_format(device,&spec,NULL) || spec.channels<=0 || spec.frequency<=0 || !spec.format ||
           backend.queued(selected[i]->stream)<0 || (!batch[i].data && !backend.flush)) return AUDIO_ERROR(0x10);
    }
    unsigned char *pcm=total ? malloc(total) : NULL;
    if(total && !pcm) return AUDIO_ERROR(0x10);
    int32_t result=AUDIO_ERROR(0x10);
    for(uint32_t i=0;i<count;++i) if(batch[i].data) {
        const AudioPort *p=selected[i];
        const size_t stride=(unsigned)p->channels*(unsigned)p->sample_bytes,bytes=p->frames*stride;
        unsigned char *out=pcm+offsets[i];
        if(bb_read_memory(out,(uintptr_t)batch[i].data,bytes)!=bytes) { result=AUDIO_ERROR(4);goto done; }
        if(p->channels==8 && !p->standard_layout) {
            /* PS4 legacy L R C LFE SL SR BL BR -> SDL FL FR FC LFE BL BR SL SR. */
            static const unsigned map[]={0,1,2,3,6,7,4,5};
            unsigned char frame[32];
            for(uint32_t f=0;f<p->frames;++f) {
                memcpy(frame,out+f*stride,stride);
                for(unsigned c=0;c<8;++c) memcpy(out+f*stride+map[c]*(unsigned)p->sample_bytes,
                    frame+c*(unsigned)p->sample_bytes,(unsigned)p->sample_bytes);
            }
        }
    }
    uint64_t now=audio_now();
    const uint64_t period=(uint64_t)selected[0]->frames*UINT64_C(1000000000)/48000;
    if(!now) goto done;
    if(!due || (now>due && now-due>8*period)) due=now;
    bb_sleep_until(due); /* One deadline for the whole batch, not one per port. */
    now=audio_now();
    if(!now) goto done;
    const uint64_t timeout=now+UINT64_C(2000000000);
    for(uint32_t i=0;i<count;++i) {
        AudioPort *p=selected[i];
        const int bytes=(int)(p->frames*(unsigned)p->channels*(unsigned)p->sample_bytes);
        if(!batch[i].data) {
            if(!backend.flush(p->stream) || wait_queue(p,0,timeout,NULL)) goto done;
            ++drains;p->next_deadline=0;
            if(trace_enabled) { audio_trace_port(p,audio_now(),2);p->metrics.last_submit_ns=0; }
        } else {
            /* At most two input buffers may remain queued. No silence is
             * inserted and a stalled/erroring device never claims success. */
            const uint64_t before=trace_enabled ? audio_now():0;
            int queued=-1;
            const int waited=wait_queue(p,bytes,timeout,trace_enabled ? &queued:NULL);
            if(trace_enabled) {
                const uint64_t after=audio_now();
                sotc_audio_wait(&p->metrics,after>=before ? after-before:0,queued,waited);
            }
            if(waited || !backend.put(p->stream,pcm+offsets[i],bytes)) {
                if(trace_enabled) audio_trace_port(p,audio_now(),3);
                goto done;
            }
            ++submitted_buffers;submitted_bytes+=(unsigned)bytes;
            now=audio_now();
            if(trace_enabled) {
                sotc_audio_submit(&p->metrics,now,period,due,lock_wait,(unsigned)bytes);
                audio_trace_port(p,now,0);
            }
            p->next_deadline=(now>due && now-due>8*period ? now:due)+period;
        }
    }
    result=(int32_t)selected[0]->frames; /* Retail returns frames, not frames*channels. */
done:
    free(pcm);return result;
}
static int32_t audio_outputs(const void *parameters,uint32_t count) {
    const uint64_t entered=trace_enabled ? audio_now():0;
    pthread_mutex_lock(&audio_lock);++output_calls;
    const uint64_t locked=trace_enabled ? audio_now():0;
    const int32_t result=output_batch(parameters,count,locked>=entered ? locked-entered:0);
    if(result<0) ++output_errors;
    if(trace_enabled && result<0 && trace_error_lines<16) {
        char line[256];
        snprintf(line,sizeof(line),"{\"schema\":1,\"event\":\"audio_output_error\",\"monotonic_ns\":%llu,\"ports\":%u,\"result\":%d,\"output_errors\":%llu}",
            (unsigned long long)audio_now(),count,result,(unsigned long long)output_errors);
        audio_trace_write(line);++trace_error_lines;
    }
    if(output_calls<=8) fprintf(stderr,"AudioOut: output ports=%u result=0x%08x, PCM buffers accepted=%llu\n",
        count,(uint32_t)result,(unsigned long long)submitted_buffers);
    pthread_mutex_unlock(&audio_lock);return result;
}
static int32_t audio_output(int32_t handle,const void *data) {
    const AudioOutput parameter={.handle=handle,.data=(uintptr_t)data};
    return audio_outputs(&parameter,1);
}
uintptr_t bootstrap_audio_resolve(const char *name) {
    if(!configured) return 0;
    if(!strcmp(name,"JfEPXVxhFqA#q#M")) return (uintptr_t)audio_init;
    if(!strcmp(name,"ekNvsT22rsY#q#M")) return (uintptr_t)audio_open;
    if(!strcmp(name,"s1--uE9mBFw#q#M")) return (uintptr_t)audio_close;
    if(!strcmp(name,"GrQ9s4IrNaQ#q#M")) return (uintptr_t)audio_state;
    if(!strcmp(name,"w3PdaSTSwGE#q#M")) return (uintptr_t)audio_outputs;
    if(!strcmp(name,"QOQtbeDqsT4#q#M")) return (uintptr_t)audio_output;
    return 0;
}
void bootstrap_audio_report(void) {
    pthread_mutex_lock(&audio_lock);
    unsigned restricted=0;
    for(int i=0;i<PORTS;++i) if(ports[i].stream) {
        restricted+=ports[i].restricted;
        audio_trace_port(&ports[i],audio_now(),4);
    }
    if(configured) printf("AudioOut: SDL ready=%d, initialized=%d, init requests=%llu, streams opened=%llu, closed=%llu, failures=%llu, restricted live=%u, state queries=%llu, output calls=%llu, PCM buffers=%llu, PCM bytes=%llu, drains=%llu, output errors=%llu\n",
        ready,initialized,(unsigned long long)init_requests,(unsigned long long)opened,
        (unsigned long long)closed,(unsigned long long)failures,restricted,(unsigned long long)state_queries,
        (unsigned long long)output_calls,(unsigned long long)submitted_buffers,(unsigned long long)submitted_bytes,
        (unsigned long long)drains,(unsigned long long)output_errors);
    pthread_mutex_unlock(&audio_lock);
}
