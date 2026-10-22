/* Original bounded input-queue policy based on the actual SDL device quantum. */
#ifndef SOTC_AUDIO_QUEUE_POLICY_H
#define SOTC_AUDIO_QUEUE_POLICY_H
#include <stdint.h>
static inline int sotc_audio_queue_limit(uint32_t frames, unsigned channels, unsigned sample_bytes,
                                        int device_frames, int device_frequency) {
    if(!frames || frames>2048 || !channels || channels>8 ||
       (sample_bytes!=2 && sample_bytes!=4) || device_frames<=0 || device_frequency<=0) return -1;
    /* Convert device output frames to fixed 48 kHz input frames, rounding up.
     * Leave one guest block beyond a complete device quantum. The total queue
     * is bounded to 200 ms of input, at most 300 KiB for eight float channels.
     * Refuse an unusable device quantum instead of silently underfeeding it. */
    const uint64_t source_frames=((uint64_t)(unsigned)device_frames*48000+
                                   (unsigned)device_frequency-1)/(unsigned)device_frequency;
    const uint64_t buffers=(source_frames+frames-1)/frames+1;
    if(buffers*frames>9600) return -1;
    return (int)((buffers-1)*frames*channels*sample_bytes);
}
#endif
