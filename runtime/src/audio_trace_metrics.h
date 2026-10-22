/* Original metadata-only AudioOut timing; no samples, clocks or device calls. */
#ifndef SOTC_AUDIO_TRACE_METRICS_H
#define SOTC_AUDIO_TRACE_METRICS_H
#include <stdint.h>
#include <limits.h>
typedef struct {
    uint64_t start_ns, last_submit_ns, buffers, bytes;
    uint64_t max_gap_ns, gaps_over_two_periods, max_late_ns, late_over_period;
    uint64_t queue_wait_ns, max_queue_wait_ns, max_lock_wait_ns;
    uint64_t input_samples, input_zero_samples, queue_wait_failures;
    int input_min, input_max;
} SotcAudioMetrics;
static inline void sotc_audio_reset(SotcAudioMetrics *s, uint64_t now) {
    const uint64_t last=s->last_submit_ns;
    *s=(SotcAudioMetrics){.start_ns=now,.last_submit_ns=last,.input_min=INT_MAX};
}
static inline void sotc_audio_wait(SotcAudioMetrics *s, uint64_t ns, int queued, int failed) {
    s->queue_wait_ns+=ns;
    if(ns>s->max_queue_wait_ns) s->max_queue_wait_ns=ns;
    s->queue_wait_failures+=(failed!=0);
    if(queued>=0) {
        ++s->input_samples;s->input_zero_samples+=(queued==0);
        if(queued<s->input_min) s->input_min=queued;
        if(queued>s->input_max) s->input_max=queued;
    }
}
static inline void sotc_audio_submit(SotcAudioMetrics *s, uint64_t now, uint64_t period,
                                    uint64_t due, uint64_t lock_wait, unsigned bytes) {
    if(s->last_submit_ns && now>=s->last_submit_ns) {
        const uint64_t gap=now-s->last_submit_ns;
        if(gap>s->max_gap_ns) s->max_gap_ns=gap;
        s->gaps_over_two_periods+=(gap>2*period);
    }
    const uint64_t late=now>due ? now-due:0;
    if(late>s->max_late_ns) s->max_late_ns=late;
    s->late_over_period+=(late>period);
    if(lock_wait>s->max_lock_wait_ns) s->max_lock_wait_ns=lock_wait;
    s->last_submit_ns=now;++s->buffers;s->bytes+=bytes;
}
#endif
