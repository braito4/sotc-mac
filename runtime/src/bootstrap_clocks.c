/* Clock subset adapted from bloodborne-native/bloodborne-runtime
 * 3170535f59cadbe5ce9c38e6bb961ed5fd215194/src/runtime_kernel.c.
 * Public PS4 NIDs and POSIX interruption semantics from shadPS4 kernel/time.cpp
 * and AjncSystem/ShadowOfTheColossus-PC 2250f328feafab953aac85e758e7bddb2e80e891.
 * Foundations: deadinside28/bbport and shadPS4. No game code is embedded.
 * ARM64 only: TSC reads use bbcpu's clock, including its translated RDTSC.
 */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

typedef struct { int64_t sec, nsec; } GuestTimespec;
typedef struct { int64_t sec, usec; } GuestTimeval;
typedef struct { int32_t minuteswest, dsttime; } GuestTimezone;
/* ABI from SotC runtime_sotc.c and shadPS4 OrbisTimesec. Despite the field
 * name, the reference port stores the standard offset EAST in west_sec. */
typedef struct { int64_t t; uint32_t west_sec, dst_sec; } GuestTimesec;
_Static_assert(sizeof(GuestTimesec) == 16, "PS4 time conversion layout");
_Static_assert(sizeof(time_t) == 8, "64-bit host time required");
static struct timespec process_start;
__attribute__((constructor)) static void remember_start(void) {
    if (clock_gettime(CLOCK_MONOTONIC, &process_start)) { perror("clock_gettime"); abort(); }
}
static int32_t guest_error(int host_error) {
    switch (host_error) {
    case 0: return 0;
    case EINTR: return 4;
    case EFAULT: return 14;
    case EINVAL: return 22;
    case EOVERFLOW: return 84;
    default: return 5;
    }
}
static int32_t posix_result(int error) {
    if (!error) return 0;
    *runtime_errno() = guest_error(error);
    return -1;
}
static int32_t kernel_result(int error) {
    return error ? (int32_t)(UINT32_C(0x80020000) | (uint32_t)guest_error(error)) : 0;
}
static int host_clock(uint32_t id, clockid_t *out) {
    switch (id) {
    case 0: case 9: case 10: case 13: *out = CLOCK_REALTIME; return 1;
    case 4: case 5: case 7: case 8: case 11: case 12:
    case 16: case 17: case 18: case 19: *out = CLOCK_MONOTONIC; return 1;
    case 2: case 15: *out = CLOCK_PROCESS_CPUTIME_ID; return 1;
    case 14: *out = CLOCK_THREAD_CPUTIME_ID; return 1;
    default: return 0;
    }
}
static int clock_read(uint32_t id, GuestTimespec *out) {
    clockid_t host;
    if (!out || !host_clock(id, &host)) return EINVAL;
    struct timespec t;
    if (clock_gettime(host, &t)) return errno;
    *out = (GuestTimespec){t.tv_sec, id == 13 ? 0 : t.tv_nsec};
    return 0;
}
static int32_t posix_clock(uint32_t id, GuestTimespec *out) { return posix_result(clock_read(id, out)); }
static int32_t kernel_clock(uint32_t id, GuestTimespec *out) { return kernel_result(clock_read(id, out)); }
static int32_t posix_resolution(uint32_t id, GuestTimespec *out) {
    clockid_t host;
    if (!host_clock(id, &host)) return posix_result(EINVAL);
    struct timespec t;
    if (clock_getres(host, &t)) return posix_result(errno);
    if (out) *out = id == 13 ? (GuestTimespec){1, 0} : (GuestTimespec){t.tv_sec, t.tv_nsec};
    return 0;
}
static uint64_t process_counter(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) { perror("clock_gettime"); abort(); }
    return (uint64_t)(t.tv_sec - process_start.tv_sec) * UINT64_C(1000000000)
           + (uint64_t)(t.tv_nsec - process_start.tv_nsec);
}
static uint64_t process_time_us(void) { return process_counter() / 1000; }
static uint64_t process_frequency(void) { return UINT64_C(1000000000); }
static uint64_t tsc_frequency(void) { return BBCPU_TSC_HZ; }
static uint64_t read_tsc(void) { return bbcpu_tsc(); }
/* GPU timestamps must share the exact origin/units used by guest clock imports. */
uint64_t runtime_process_time_us(void) { return process_time_us(); }
uint64_t runtime_process_time_counter(void) { return process_counter(); }
uint64_t runtime_tsc_frequency(void) { return tsc_frequency(); }
/* Destructive interval snapshots, matching the pinned GPU's statistics contract. */
static _Atomic uint64_t sleep_calls, sleep_total_ns, sleep_requested_ns;
void runtime_sleep_stats(uint64_t *calls, uint64_t *ns, uint64_t *requested_ns) {
    *calls = atomic_exchange_explicit(&sleep_calls, 0, memory_order_relaxed);
    *ns = atomic_exchange_explicit(&sleep_total_ns, 0, memory_order_relaxed);
    *requested_ns = atomic_exchange_explicit(&sleep_requested_ns, 0, memory_order_relaxed);
}

static int sleep_for(const GuestTimespec *request, GuestTimespec *remaining) {
    if (!request || request->sec < 0 || request->nsec < 0 || request->nsec >= 1000000000) return EINVAL;
    const struct timespec rq = {request->sec, request->nsec};
    struct timespec rem = {0}, a, b;
    if (clock_gettime(CLOCK_MONOTONIC, &a)) return errno;
    const int result = nanosleep(&rq, &rem), saved_error = result ? errno : 0;
    atomic_fetch_add_explicit(&sleep_calls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&sleep_requested_ns,
        (uint64_t)request->sec * UINT64_C(1000000000) + (uint64_t)request->nsec, memory_order_relaxed);
    if (!clock_gettime(CLOCK_MONOTONIC, &b)) {
        const uint64_t elapsed = (uint64_t)(b.tv_sec - a.tv_sec) * UINT64_C(1000000000)
                                 + (uint64_t)(b.tv_nsec - a.tv_nsec);
        atomic_fetch_add_explicit(&sleep_total_ns, elapsed, memory_order_relaxed);
        runtime_wait_note(3, elapsed);
    }
    /* POSIX writes the remainder on interruption. Keep it untouched on errors
     * such as EINVAL; a successful wait has no remaining duration. */
    if (remaining && (!saved_error || saved_error == EINTR)) *remaining = (GuestTimespec){rem.tv_sec, rem.tv_nsec};
    return saved_error;
}
static int32_t posix_nanosleep(const GuestTimespec *rq, GuestTimespec *rem) { return posix_result(sleep_for(rq, rem)); }
static int32_t kernel_nanosleep(const GuestTimespec *rq, GuestTimespec *rem) { return kernel_result(sleep_for(rq, rem)); }
static int32_t posix_usleep(uint32_t usec) {
    const GuestTimespec rq = {usec / 1000000, (usec % 1000000) * 1000};
    return posix_nanosleep(&rq, NULL);
}
static int32_t kernel_usleep(uint32_t usec) {
    const GuestTimespec rq = {usec / 1000000, (usec % 1000000) * 1000};
    return kernel_nanosleep(&rq, NULL);
}
/* Same UTC/local scalar representation as SotC's reference adapter. Resolve
 * offsets at the supplied epoch, including historical DST, rather than using
 * today's offset for every date. mktime with tm_isdst=0 determines the actual
 * DST adjustment, including zones whose adjustment is not one hour. */
static int zone_at(int64_t utc, int32_t *standard_east, int32_t *dst) {
    const time_t epoch = utc;
    struct tm local;
    errno = 0;
    if (!localtime_r(&epoch, &local)) return errno ? errno : EOVERFLOW;
    int64_t daylight = 0;
    if (local.tm_isdst > 0) {
        struct tm standard = local;
        standard.tm_isdst = 0;
        errno = 0;
        const time_t without_dst = mktime(&standard);
        if (without_dst == (time_t)-1 && errno) return errno;
        if (__builtin_sub_overflow((int64_t)without_dst, utc, &daylight)) return EOVERFLOW;
    }
    int64_t east;
    if (__builtin_sub_overflow((int64_t)local.tm_gmtoff, daylight, &east) ||
        east < INT32_MIN || east > INT32_MAX || daylight < INT32_MIN || daylight > INT32_MAX) return EOVERFLOW;
    *standard_east = (int32_t)east; *dst = (int32_t)daylight;
    return 0;
}
static int32_t utc_to_local(int64_t utc, int64_t *local, GuestTimesec *info, uint64_t *dst_seconds) {
    int32_t east, dst;
    int error = zone_at(utc, &east, &dst);
    int64_t result;
    if (error) return kernel_result(error);
    if (__builtin_add_overflow(utc, (int64_t)east + dst, &result)) return kernel_result(EOVERFLOW);
    if (local) *local = result;
    if (info) *info = (GuestTimesec){utc, (uint32_t)east, (uint32_t)dst};
    if (dst_seconds) *dst_seconds = (uint64_t)(int64_t)dst;
    return 0;
}
static int32_t local_to_utc(int64_t local, int64_t unused, int64_t *utc, GuestTimezone *zone, int32_t *dst_seconds) {
    /* The reference SotC adapter ignores this argument. Ambiguous/nonexistent
     * local calendar times follow the host mktime policy (tm_isdst=-1). */
    (void)unused;
    if (!zone) return kernel_result(EINVAL);
    const time_t local_scalar = local;
    struct tm calendar;
    errno = 0;
    if (!gmtime_r(&local_scalar, &calendar)) return kernel_result(errno ? errno : EOVERFLOW);
    calendar.tm_isdst = -1;
    errno = 0;
    const time_t result = mktime(&calendar);
    if (result == (time_t)-1 && errno) return kernel_result(errno);
    int32_t east, dst;
    const int error = zone_at(result, &east, &dst);
    if (error) return kernel_result(error);
    if (utc) *utc = result;
    *zone = (GuestTimezone){-east / 60, dst != 0};
    if (dst_seconds) *dst_seconds = dst;
    return 0;
}
static int32_t kernel_timezone(GuestTimezone *out) {
    if (!out) return kernel_result(EINVAL);
    int32_t east, dst;
    const int error = zone_at((int64_t)time(NULL), &east, &dst);
    if (error) return kernel_result(error);
    *out = (GuestTimezone){-east / 60, dst != 0};
    return 0;
}
static int32_t posix_timeofday(GuestTimeval *out, GuestTimezone *zone) {
    struct timeval t;
    if (gettimeofday(&t, NULL)) return posix_result(errno);
    if (out) *out = (GuestTimeval){t.tv_sec, t.tv_usec};
    if (zone && kernel_timezone(zone)) return posix_result(EINVAL);
    return 0;
}
static int32_t kernel_timeofday(GuestTimeval *out) {
    if (!out) return kernel_result(EINVAL);
    struct timeval t;
    if (gettimeofday(&t, NULL)) return kernel_result(errno);
    *out = (GuestTimeval){t.tv_sec, t.tv_usec};
    return 0;
}
/* RTC calendar subset; proleptic Gregorian helpers adapted from the pinned
 * runtime_rtc.c (bloodborne-native/bbport, GPL-2.0-or-later). One tick is one
 * microsecond since year 1. No network-clock or host-clock-setting API is bound. */
typedef struct { uint16_t year,month,day,hour,minute,second;uint32_t microsecond; } RtcDate;
_Static_assert(sizeof(RtcDate)==16,"RTC calendar ABI");
static void rtc_calendar(uint64_t tick,void *out) {
    int64_t z=(int64_t)(tick/UINT64_C(86400000000))+306;
    int64_t era=z/146097,doe=z-era*146097;
    int64_t yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
    int64_t doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153;
    int64_t month=mp<10 ? mp+3 : mp-9;
    uint64_t us=tick%UINT64_C(86400000000);
    const RtcDate date={(uint16_t)(yoe+era*400+(month<=2)),(uint16_t)month,
        (uint16_t)(doy-(153*mp+2)/5+1),(uint16_t)(us/UINT64_C(3600000000)),
        (uint16_t)(us/UINT64_C(60000000)%60),(uint16_t)(us/1000000%60),(uint32_t)(us%1000000)};
    memcpy(out,&date,sizeof(date));
}
static int32_t rtc_current(void *out,int32_t minutes) {
    if(!out) return (int32_t)UINT32_C(0x80b50002);
    struct timespec now;if(clock_gettime(CLOCK_REALTIME,&now)) return kernel_result(errno);
    uint64_t tick=UINT64_C(62135596800000000)+(uint64_t)now.tv_sec*1000000+(uint64_t)now.tv_nsec/1000;
    tick+=(uint64_t)((int64_t)minutes*60000000);rtc_calendar(tick,out);return 0;
}
static int32_t rtc_current_local(void *out) {
    if(!out) return (int32_t)UINT32_C(0x80b50002);
    struct timespec now;if(clock_gettime(CLOCK_REALTIME,&now)) return kernel_result(errno);
    int32_t standard,dst;int error=zone_at(now.tv_sec,&standard,&dst);if(error) return kernel_result(error);
    /* Retail truncates the combined offset to whole minutes before adding it. */
    int64_t minutes=((int64_t)standard+dst)/60;
    uint64_t tick=UINT64_C(62135596800000000)+(uint64_t)now.tv_sec*1000000+(uint64_t)now.tv_nsec/1000;
    tick+=(uint64_t)(minutes*60000000);rtc_calendar(tick,out);return 0;
}
static uint32_t rtc_resolution(void) {return 1000000;}
static int32_t rtc_weekday(int32_t year,int32_t month,int32_t day) {
    /* The supported retail title uses the SDK >= 3 validation order. */
    if(month<1 || month>12) return (int32_t)UINT32_C(0x80b50009);
    if(year<1 || year>9999) return (int32_t)UINT32_C(0x80b50008);
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int leap=year%4==0 && (year%100!=0 || year%400==0);
    if(day<1 || day>days[month-1]+(month==2 && leap)) return (int32_t)UINT32_C(0x80b5000a);
    static const int shift[]={0,3,2,5,0,3,5,1,4,6,2,4};
    year-=month<3;
    return (year+year/4-year/100+year/400+shift[month-1]+day)%7;
}
uintptr_t bootstrap_clocks_resolve(const char *name) {
    static const struct { const char *name; void *function; } exports[] = {
        {"8lfvnRMqwEM#X#Y",rtc_current},{"ZPD1YOKI+Kw#X#Y",rtc_current_local},
        {"jMNwqYr4R-k#X#Y",rtc_resolution},{"CyIK-i4XdgQ#X#Y",rtc_weekday},
        {"1j3S3n-tTW4#0#F", tsc_frequency}, {"-2IRUCO--PM#0#F", read_tsc},
        {"4J2sUJmuHZQ#0#F", process_time_us}, {"fgxnMeTNUtY#0#F", process_counter},
        {"BNowx2l588E#0#F", process_frequency}, {"QBi7HCK03hw#0#F", kernel_clock},
        {"lLMT9vJAck0#0#F", posix_clock}, {"lLMT9vJAck0#p#F", posix_clock},
        {"smIj7eqzZE8#0#F", posix_resolution}, {"smIj7eqzZE8#p#F", posix_resolution},
        /* The original NGS2 module imports POSIX usleep from libkernel itself.
         * Keep its POSIX errno contract in both verified library namespaces. */
        {"1jfXLRVzisc#0#F", kernel_usleep},
        {"QcteRwbsnV0#0#F", posix_usleep}, {"QcteRwbsnV0#p#F", posix_usleep},
        {"QvsZxomvUHs#0#F", kernel_nanosleep},
        {"yS8U2TGCe1A#0#F", posix_nanosleep}, {"yS8U2TGCe1A#p#F", posix_nanosleep},
        {"NhpspxdjEKU#0#F", posix_nanosleep}, {"NhpspxdjEKU#p#F", posix_nanosleep},
        {"ejekcaNQNq0#0#F", kernel_timeofday}, {"kOcnerypnQA#0#F", kernel_timezone},
        {"n88vx3C5nW8#0#F", posix_timeofday}, {"n88vx3C5nW8#p#F", posix_timeofday},
        {"-o5uEDpN+oY#0#F", utc_to_local}, {"0NTHN1NKONI#0#F", local_to_utc},
    };
    for (size_t i = 0; i < sizeof(exports) / sizeof(*exports); ++i)
        if (!strcmp(name, exports[i].name)) return (uintptr_t)exports[i].function;
    return 0;
}
