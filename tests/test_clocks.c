/* Synthetic clock/ABI checks; no game input. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "platform.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>

typedef struct { int64_t sec, nsec; } GuestTimespec;
typedef struct { int64_t sec, usec; } GuestTimeval;
typedef struct { int64_t t; uint32_t west_sec, dst_sec; } GuestTimesec;
typedef struct { int32_t minuteswest, dsttime; } GuestTimezone;
static uint64_t ns(const struct timespec *t) { return (uint64_t)t->tv_sec * 1000000000 + t->tv_nsec; }
static uint64_t guest_ns(const GuestTimespec *t) { return (uint64_t)t->sec * 1000000000 + t->nsec; }
static void test_calendar(void) {
    int32_t (*to_local)(int64_t, int64_t *, GuestTimesec *, uint64_t *) =
        (void *)bootstrap_resolve("-o5uEDpN+oY#0#F", 0);
    int32_t (*to_utc)(int64_t, int64_t, int64_t *, GuestTimezone *, int32_t *) =
        (void *)bootstrap_resolve("0NTHN1NKONI#0#F", 0);
    assert(to_local && to_utc && !bootstrap_resolve("-o5uEDpN+oY#1#u", 0));
    assert(!bootstrap_resolve("-o5uEDpN+oY#0#F", 1));
    const char *original_zone = getenv("TZ");
    char saved_zone[1024];
    const int had_zone = original_zone != NULL;
    if (had_zone) { assert(strlen(original_zone) < sizeof(saved_zone)); strcpy(saved_zone, original_zone); }
    const struct { const char *zone; int32_t east, january_dst, july_dst; } zones[] = {
        {"UTC0", 0, 0, 0}, {"Europe/Madrid", 3600, 0, 3600},
        {"America/New_York", -18000, 0, 3600}, {"Australia/Lord_Howe", 37800, 1800, 0},
    };
    /* 2024-01-15 and 2024-07-15, 12:00 UTC: established historical dates. */
    const int64_t epochs[] = {INT64_C(1705320000), INT64_C(1721044800)};
    unsigned char *bridge = runtime_low_map(16384, PROT_READ | PROT_WRITE);
    assert(bridge);
    bridge[0] = 0x48; bridge[1] = 0xb8;
    const uintptr_t target = (uintptr_t)to_local;
    memcpy(bridge + 2, &target, sizeof(target)); bridge[10] = 0xff; bridge[11] = 0xe0;
    bbcpu_add_guest_code((uintptr_t)bridge, 16384);
    for (size_t i = 0; i < sizeof(zones) / sizeof(*zones); ++i) {
        assert(setenv("TZ", zones[i].zone, 1) == 0); tzset();
        for (unsigned season = 0; season < 2; ++season) {
            const int32_t expected_dst = season ? zones[i].july_dst : zones[i].january_dst;
            int64_t local = -7, utc = -8;
            uint64_t dst_seconds = UINT64_MAX;
            struct { uint64_t head; GuestTimesec value; uint64_t tail; } wrapped = {0xa123, {0}, 0xb456};
            const uint64_t args[] = {(uint64_t)epochs[season], (uintptr_t)&local, (uintptr_t)&wrapped.value, (uintptr_t)&dst_seconds};
            assert((int32_t)bbcpu_call((uintptr_t)bridge, 4, args) == 0);
            assert(local == epochs[season] + zones[i].east + expected_dst);
            assert(wrapped.value.t == epochs[season] && (int32_t)wrapped.value.west_sec == zones[i].east);
            assert((int32_t)wrapped.value.dst_sec == expected_dst && dst_seconds == (uint64_t)expected_dst);
            assert(wrapped.head == 0xa123 && wrapped.tail == 0xb456);
            GuestTimezone zone = {0};
            int32_t inverse_dst = -1;
            assert(to_utc(local, 0, &utc, &zone, &inverse_dst) == 0 && utc == epochs[season]);
            assert(zone.minuteswest == -zones[i].east / 60 && zone.dsttime == (expected_dst != 0));
            assert(inverse_dst == expected_dst);
        }
    }
    assert(setenv("TZ", "Europe/Madrid", 1) == 0); tzset();
    /* Madrid's spring 2024 transition: 00:59:59 UTC -> 01:00:00 UTC. */
    int64_t local;
    uint64_t dst;
    assert(to_local(INT64_C(1711846799), &local, NULL, &dst) == 0 && local == INT64_C(1711850399) && dst == 0);
    assert(to_local(INT64_C(1711846800), &local, NULL, &dst) == 0 && local == INT64_C(1711854000) && dst == 3600);
    assert(to_local(epochs[0], NULL, NULL, NULL) == 0);
    *runtime_errno() = 17;
    assert(to_utc(0, 0, &local, NULL, NULL) == (int32_t)UINT32_C(0x80020016) && *runtime_errno() == 17);
    local = -7;
    assert(to_local(INT64_MAX, &local, NULL, NULL) != 0 && local == -7);
    if (had_zone) assert(setenv("TZ", saved_zone, 1) == 0);
    else assert(unsetenv("TZ") == 0);
    tzset();
    puts("PASS: UTC/local ABI and round trips across four zones, seasonal/half-hour DST and transition boundary");
}
void bootstrap_test_clocks(void) {
    uint64_t (*frequency)(void) = (void *)bootstrap_resolve("1j3S3n-tTW4#0#F", 0);
    uint64_t (*tsc)(void) = (void *)bootstrap_resolve("-2IRUCO--PM#0#F", 0);
    uint64_t (*counter)(void) = (void *)bootstrap_resolve("fgxnMeTNUtY#0#F", 0);
    uint64_t (*counter_hz)(void) = (void *)bootstrap_resolve("BNowx2l588E#0#F", 0);
    uint64_t (*process_us)(void) = (void *)bootstrap_resolve("4J2sUJmuHZQ#0#F", 0);
    int32_t (*clock)(uint32_t, GuestTimespec *) = (void *)bootstrap_resolve("QBi7HCK03hw#0#F", 0);
    int32_t (*posix_clock)(uint32_t, GuestTimespec *) = (void *)bootstrap_resolve("lLMT9vJAck0#p#F", 0);
    int32_t (*resolution)(uint32_t, GuestTimespec *) = (void *)bootstrap_resolve("smIj7eqzZE8#p#F", 0);
    int32_t (*sleep)(const GuestTimespec *, GuestTimespec *) = (void *)bootstrap_resolve("QvsZxomvUHs#0#F", 0);
    int32_t (*posix_sleep)(const GuestTimespec *, GuestTimespec *) = (void *)bootstrap_resolve("yS8U2TGCe1A#0#F", 0);
    int32_t (*usleep)(uint32_t) = (void *)bootstrap_resolve("1jfXLRVzisc#0#F", 0);
    int32_t (*timeofday)(GuestTimeval *) = (void *)bootstrap_resolve("ejekcaNQNq0#0#F", 0);
    assert(frequency && tsc && counter && counter_hz && process_us && clock && posix_clock && resolution && sleep && posix_sleep && usleep && timeofday);
    assert(!bootstrap_resolve("1j3S3n-tTW4#1#u", 0) && !bootstrap_resolve("1j3S3n-tTW4#0#F", 1));
    assert(frequency() == BBCPU_TSC_HZ && counter_hz() == 1000000000);
    unsigned char *guest = runtime_low_map(16384, PROT_READ | PROT_WRITE);
    assert(guest);
    /* RDTSC; shl rdx,32; or rax,rdx; ret -- synthetic x86 counter read. */
    const unsigned char code[] = {0x0f,0x31,0x48,0xc1,0xe2,0x20,0x48,0x09,0xd0,0xc3};
    memcpy(guest, code, sizeof(code));
    bbcpu_add_guest_code((uintptr_t)guest, 16384);
    uint64_t before = tsc(), observed = bbcpu_call((uintptr_t)guest, 0, NULL), after = tsc();
    assert(before <= observed && observed <= after);
    before = counter(); observed = process_us(); after = counter();
    assert(before / 1000 <= observed && observed <= after / 1000);
    struct timespec host_before, host_after;
    struct { uint64_t head; GuestTimespec value; uint64_t tail; } wrapped = {0xa123, {0}, 0xb456};
    assert(clock_gettime(CLOCK_MONOTONIC, &host_before) == 0);
    assert(clock(4, &wrapped.value) == 0);
    assert(clock_gettime(CLOCK_MONOTONIC, &host_after) == 0);
    assert(ns(&host_before) <= guest_ns(&wrapped.value) && guest_ns(&wrapped.value) <= ns(&host_after));
    assert(wrapped.head == 0xa123 && wrapped.tail == 0xb456);
    *runtime_errno() = 17;
    assert(clock(UINT32_MAX, &wrapped.value) == (int32_t)UINT32_C(0x80020016) && *runtime_errno() == 17);
    assert(clock(4, NULL) == (int32_t)UINT32_C(0x80020016));
    assert(posix_clock(UINT32_MAX, &wrapped.value) == -1 && *runtime_errno() == 22);
    assert(posix_clock(4, NULL) == -1 && *runtime_errno() == 22);
    const uint32_t ids[] = {0,2,4,5,7,8,9,10,11,12,13,14,15,16,17,18,19};
    for (size_t i = 0; i < sizeof(ids) / sizeof(*ids); ++i) {
        assert(clock(ids[i], &wrapped.value) == 0);
        assert(wrapped.value.sec >= 0 && wrapped.value.nsec >= 0 && wrapped.value.nsec < 1000000000);
    }
    assert(resolution(4, NULL) == 0 && resolution(13, &wrapped.value) == 0);
    assert(wrapped.value.sec == 1 && wrapped.value.nsec == 0);
    GuestTimespec request = {0, 2000000}, remaining = {7, 8};
    assert(clock_gettime(CLOCK_MONOTONIC, &host_before) == 0);
    assert(sleep(&request, &remaining) == 0 && !remaining.sec && !remaining.nsec);
    assert(clock_gettime(CLOCK_MONOTONIC, &host_after) == 0 && ns(&host_after) - ns(&host_before) >= 2000000);
    assert(posix_sleep(NULL, NULL) == -1 && *runtime_errno() == 22);
    request.nsec = 1000000000; remaining = (GuestTimespec){7, 8};
    *runtime_errno() = 17;
    assert(sleep(&request, &remaining) == (int32_t)UINT32_C(0x80020016) && *runtime_errno() == 17);
    assert(remaining.sec == 7 && remaining.nsec == 8);
    request = (GuestTimespec){-1, 0};
    assert(posix_sleep(&request, &remaining) == -1 && *runtime_errno() == 22);
    assert(usleep(0) == 0);
    struct timeval wall_before, wall_after;
    struct { uint64_t head; GuestTimeval value; uint64_t tail; } wall = {0xa123, {0}, 0xb456};
    assert(gettimeofday(&wall_before, NULL) == 0 && timeofday(&wall.value) == 0 && gettimeofday(&wall_after, NULL) == 0);
    const int64_t value_us = wall.value.sec * 1000000 + wall.value.usec;
    assert(wall_before.tv_sec * INT64_C(1000000) + wall_before.tv_usec <= value_us);
    assert(value_us <= wall_after.tv_sec * INT64_C(1000000) + wall_after.tv_usec);
    assert(wall.head == 0xa123 && wall.tail == 0xb456);
    assert(timeofday(NULL) == (int32_t)UINT32_C(0x80020016));
    puts("PASS: guest RDTSC/API clock agreement, counter units, clock IDs/layout/errors, realtime and real sleep duration");
    test_calendar();
}
