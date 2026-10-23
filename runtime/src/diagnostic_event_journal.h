/* Original bounded pre-failure evidence, shared with the offline collector.
 * Initialization is ordinary startup work. Record only publishes numeric CPU
 * observations to an existing MAP_SHARED mapping: no file I/O, locks, allocation,
 * driver calls or extra guest reads. Never initialize from a signal handler.
 * Survives process _Exit/signals; not durable against power loss/kernel failure.
 */
#ifndef SOTC_DIAGNOSTIC_EVENT_JOURNAL_H
#define SOTC_DIAGNOSTIC_EVENT_JOURNAL_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "guest_failure_state.h"

enum { SOTC_JOURNAL_HEADER_WORDS=512, SOTC_JOURNAL_RECORD_WORDS=16,
       SOTC_JOURNAL_DATA_WORDS=11, SOTC_JOURNAL_MAX_CHANNELS=28 };
/* Per-channel counters end at word 127; terminal context starts at 128. */
typedef char SotcJournalCountersBeforeTerminal[(16+4*SOTC_JOURNAL_MAX_CHANNELS<=128)?1:-1];
typedef struct { uint64_t *map; unsigned channels, capacity; } SotcEventJournal;

/* Fixed little-endian uint64 layout on supported native ARM64 macOS hosts.
 * Header: magic, version, channels, capacity/channel, record words, reservations,
 * busy drops, suppressed, producer PID. Per channel at 16+4*c: reservations, busy drops.
 * Record: committed global sequence, busy, channel ticket+1, global sequence,
 * monotonic ns, eleven payload words. Commit is published last. A killed writer
 * leaves an invalid slot; another channel cannot evict this channel's history.
 */
static inline void sotc_event_journal_init(SotcEventJournal *journal,
                                          const char *name,unsigned channels,unsigned capacity) {
    const char *enabled=getenv("SOTC_DIAGNOSTIC_JOURNAL"), *directory=getenv("BB_GPU_USER_DIR");
    if (!enabled || strcmp(enabled,"1")) return;
    if (!directory || !*directory || !channels || channels>SOTC_JOURNAL_MAX_CHANNELS ||
        !capacity || capacity>512 || !__atomic_always_lock_free(8,0)) {
        fprintf(stderr,"Diagnostic journal unavailable: %s invalid configuration\n",name); return;
    }
    char path[4096];
    const int n=snprintf(path,sizeof(path),"%s/%s",directory,name);
    if (n<0 || (size_t)n>=sizeof(path)) {
        fprintf(stderr,"Diagnostic journal unavailable: %s path too long\n",name);return;
    }
    const size_t bytes=(SOTC_JOURNAL_HEADER_WORDS+(size_t)channels*capacity*SOTC_JOURNAL_RECORD_WORDS)*8;
    const int fd=open(path,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if (fd<0) {fprintf(stderr,"Diagnostic journal unavailable: %s create failed\n",name);return;}
    void *mapping=MAP_FAILED;
    if (!ftruncate(fd,(off_t)bytes)) mapping=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    close(fd);
    if (mapping==MAP_FAILED) {
        unlink(path);fprintf(stderr,"Diagnostic journal unavailable: %s mapping failed\n",name);return;
    }
    uint64_t *header=(uint64_t *)mapping;
    header[1]=1;header[2]=channels;header[3]=capacity;header[4]=SOTC_JOURNAL_RECORD_WORDS;
    header[8]=(uint64_t)getpid();
    memcpy(header,"SOTCEV01",8);
    journal->channels=channels;journal->capacity=capacity;journal->map=header;
    fprintf(stderr,"Diagnostic journal ready: %s channels=%u capacity=%u bytes=%zu; bounded CPU observations, not ownership or GPU progress\n",
            name,channels,capacity,bytes);
}
static inline void sotc_event_journal_suppressed(SotcEventJournal *journal) {
    if (journal->map) __atomic_fetch_add(journal->map+7,1,__ATOMIC_RELAXED);
}
static inline void sotc_event_journal_record(SotcEventJournal *journal,unsigned channel,
                                            uint64_t ns,const uint64_t data[SOTC_JOURNAL_DATA_WORDS]) {
    if (!journal->map || channel>=journal->channels) return;
    uint64_t *header=journal->map;
    const uint64_t ticket=__atomic_fetch_add(header+16+channel*4,1,__ATOMIC_RELAXED);
    const uint64_t seq=__atomic_fetch_add(header+5,1,__ATOMIC_RELAXED)+1;
    uint64_t *record=header+SOTC_JOURNAL_HEADER_WORDS+
        ((size_t)channel*journal->capacity+ticket%journal->capacity)*SOTC_JOURNAL_RECORD_WORDS;
    uint64_t expected=0;
    if (!__atomic_compare_exchange_n(record+1,&expected,1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) {
        __atomic_fetch_add(header+6,1,__ATOMIC_RELAXED);
        __atomic_fetch_add(header+17+channel*4,1,__ATOMIC_RELAXED);return;
    }
    /* A delayed older reservation must not replace a newer lap of this slot. */
    if (__atomic_load_n(record+2,__ATOMIC_RELAXED)>ticket+1) {
        __atomic_fetch_add(header+6,1,__ATOMIC_RELAXED);
        __atomic_fetch_add(header+17+channel*4,1,__ATOMIC_RELAXED);
        __atomic_store_n(record+1,0,__ATOMIC_RELEASE);return;
    }
    __atomic_store_n(record,0,__ATOMIC_RELEASE);
    __atomic_store_n(record+2,ticket+1,__ATOMIC_RELAXED);
    __atomic_store_n(record+3,seq,__ATOMIC_RELAXED);
    __atomic_store_n(record+4,ns,__ATOMIC_RELAXED);
    for(unsigned i=0;i<SOTC_JOURNAL_DATA_WORDS;++i)
        __atomic_store_n(record+5+i,data[i],__ATOMIC_RELAXED);
    __atomic_store_n(record,seq,__ATOMIC_RELEASE);
    __atomic_store_n(record+1,0,__ATOMIC_RELEASE);
}
/* First terminating register snapshot occupies reserved header words 128..150.
 * kind 1: stack-protector import, kind 2: translated fatal fault. No I/O or
 * pointed-to memory reads; interrupted publication is rejected offline. */
static inline void sotc_event_journal_failure(SotcEventJournal *journal,uint64_t kind,
                                             const SotcGuestFailureState *state) {
    if (!journal->map) return;
    uint64_t *slot=journal->map+128, expected=0;
    if (!__atomic_compare_exchange_n(slot,&expected,UINT64_MAX,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) return;
    const uint64_t values[]={kind,state->available,state->rip,state->fs_base,state->stack_top,state->thread};
    for(unsigned i=0;i<6;++i)__atomic_store_n(slot+1+i,values[i],__ATOMIC_RELAXED);
    for(unsigned i=0;i<16;++i)__atomic_store_n(slot+7+i,state->regs[i],__ATOMIC_RELAXED);
    __atomic_store_n(slot,1,__ATOMIC_RELEASE);
}
#endif
