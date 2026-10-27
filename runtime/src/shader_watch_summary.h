/* Bounded per-address registration summaries. Numeric watch identity only.
 * Initialized before execution; no guest reads, allocation or waits on record. */
#ifndef SOTC_SHADER_WATCH_SUMMARY_H
#define SOTC_SHADER_WATCH_SUMMARY_H
#include "diagnostic_event_journal.h"
#include <stdatomic.h>
enum { SOTC_WS_SLOTS=8192, SOTC_WS_WAYS=4, SOTC_WS_BUCKETS=2048 };
static uint64_t *sotc_ws_map;
static _Atomic unsigned sotc_ws_busy[SOTC_WS_BUCKETS];
static void sotc_ws_init(void) {
    SotcEventJournal storage={0};
    sotc_event_journal_init(&storage,"shader-watch-summary.bin",16,512);
    if(!storage.map)return;
    sotc_ws_map=storage.map;
    memcpy(sotc_ws_map,"SOTCWS01",8);
    sotc_ws_map[1]=1;sotc_ws_map[2]=SOTC_WS_SLOTS;sotc_ws_map[3]=SOTC_WS_WAYS;
    /* Header 5 attempts, 6 contention drops, 7 replacements. PID remains 8. */
}
static unsigned sotc_ws_bucket(uint64_t address) {
    uint64_t key=address>>8;key^=key>>17;key*=UINT64_C(0x9e3779b97f4a7c15);
    key^=key>>32;return (unsigned)key&(SOTC_WS_BUCKETS-1);
}
static void sotc_ws_record(unsigned action,uint64_t address,uint64_t packet,
        uint64_t capture,uint64_t generation,uint64_t related,uint64_t ns) {
    if(!sotc_ws_map || action<1 || action>4 || !address)return;
    uint64_t ticket=__atomic_add_fetch(sotc_ws_map+5,1,__ATOMIC_RELAXED);
    unsigned bucket=sotc_ws_bucket(address),expected=0;
    if(!atomic_compare_exchange_strong(&sotc_ws_busy[bucket],&expected,1)) {
        __atomic_fetch_add(sotc_ws_map+6,1,__ATOMIC_RELAXED);return;
    }
    uint64_t *selected=NULL;uint64_t oldest=UINT64_MAX;
    for(unsigned i=0;i<SOTC_WS_WAYS;++i) {
        uint64_t *row=sotc_ws_map+512+(bucket*SOTC_WS_WAYS+i)*16;
        if(row[1]==address){selected=row;break;}
        if(!row[1]){selected=row;break;}
        if(row[13]<oldest){selected=row;oldest=row[13];}
    }
    /* Reservations can arrive out of order. Never replace newer evidence. */
    if(selected[1] && selected[13]>ticket) {
        __atomic_fetch_add(sotc_ws_map+6,1,__ATOMIC_RELAXED);
        atomic_store(&sotc_ws_busy[bucket],0);return;
    }
    __atomic_store_n(selected,1,__ATOMIC_SEQ_CST);
    if(selected[1]!=address) {
        if(selected[1])__atomic_fetch_add(sotc_ws_map+7,1,__ATOMIC_RELAXED);
        for(unsigned i=1;i<16;++i)selected[i]=0;
        selected[1]=address;selected[2]=ticket;selected[3]=ns;selected[12]=ticket;
    }
    selected[4]=ns;selected[4+action]++;
    selected[9]=packet;selected[10]=capture;selected[11]=generation;
    selected[13]=ticket;selected[14]=related;selected[15]++;
    __atomic_store_n(selected,0,__ATOMIC_SEQ_CST);
    atomic_store(&sotc_ws_busy[bucket],0);
}
#endif
