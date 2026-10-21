/* Synthetic RTC guest ABI and host calendar comparisons; no game or network. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
typedef struct {uint16_t year,month,day,hour,minute,second;uint32_t microsecond;} TestRtcDate;
_Static_assert(sizeof(TestRtcDate)==16,"RTC test ABI");
static const char *const rtc_names[]={"8lfvnRMqwEM#X#Y","ZPD1YOKI+Kw#X#Y",
    "jMNwqYr4R-k#X#Y","CyIK-i4XdgQ#X#Y"};
static uintptr_t rtc_bridge(uintptr_t target) {
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);
    code[0]=0x48;code[1]=0xb8;memcpy(code+2,&target,8);code[10]=0xff;code[11]=0xe0;
    bbcpu_add_guest_code((uintptr_t)code,16384);return (uintptr_t)code;
}
static uint64_t realtime_tick(void) {
    struct timespec now;assert(!clock_gettime(CLOCK_REALTIME,&now));
    return UINT64_C(62135596800000000)+(uint64_t)now.tv_sec*1000000+(uint64_t)now.tv_nsec/1000;
}
static TestRtcDate expected_date(uint64_t tick) {
    time_t seconds=(time_t)(tick/1000000)-INT64_C(62135596800);struct tm t;assert(gmtime_r(&seconds,&t));
    return (TestRtcDate){(uint16_t)(t.tm_year+1900),(uint16_t)(t.tm_mon+1),(uint16_t)t.tm_mday,
        (uint16_t)t.tm_hour,(uint16_t)t.tm_min,(uint16_t)t.tm_sec,(uint32_t)(tick%1000000)};
}
static void check_rtc(const uintptr_t target[4]) {
    const int32_t minutes[]={0,60,-60,345,-210,840,-720,INT32_MAX,INT32_MIN};
    *runtime_errno()=17;
    for(unsigned local=0;local<2;++local) for(unsigned i=0;i<(local ? 1u : 9u);++i) {
        unsigned char output[40];memset(output,0xa5,sizeof(output));
        uint64_t before=realtime_tick();
        uint64_t args[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000000)|(uint32_t)minutes[i],1,3,5,7};
        assert((int32_t)bbcpu_call(target[local],6,args)==0);
        uint64_t after=realtime_tick();assert(after>=before && after-before<5000000);
        int64_t offset=minutes[i];
        if(local) {time_t sec=(time_t)(before/1000000)-INT64_C(62135596800);struct tm t;
            assert(localtime_r(&sec,&t));offset=t.tm_gmtoff/60;}
        before+=(uint64_t)(offset*60000000);after+=(uint64_t)(offset*60000000);
        TestRtcDate actual;memcpy(&actual,output+1,16);
        int found=0;
        /* Each call is tightly bracketed; reconstruct through independent libc
         * calendar conversion, including the original uint16 year narrowing. */
        for(uint64_t tick=before;tick<=after;++tick) {
            TestRtcDate expected=expected_date(tick);
            if(!memcmp(&actual,&expected,16)) {found=1;break;}
        }
        assert(found);for(unsigned j=0;j<sizeof(output);++j) if(j<1 || j>=17) assert(output[j]==0xa5);
        args[0]=0;assert((uint32_t)bbcpu_call(target[local],6,args)==UINT32_C(0x80b50002));
        assert(*runtime_errno()==17);
    }
    const uint64_t stale[]={1,UINT64_MAX,3,5,7,9};
    assert(bbcpu_call(target[2],6,stale)==1000000);
    /* January weekday anchors generated with Python datetime.date; month/day
     * progression is independent of the runtime's Sakamoto formula. macOS timegm
     * cannot serve as a reference for the full year-1..9999 input domain. */
    const int years[]={1,4,100,400,1600,1900,1970,2000,2024,2026,2100,2400,9999};
    const unsigned january[]={1,4,5,6,6,1,4,6,1,4,5,6,5};
    const int month_days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    for(unsigned y=0;y<sizeof(years)/sizeof(*years);++y) {
        unsigned weekday=january[y];
        for(int month=1;month<=12;++month) {
            int leap=years[y]%4==0 && (years[y]%100!=0 || years[y]%400==0);
            int days=month_days[month-1]+(month==2 && leap);
            for(int day=1;day<=31;++day) {
                uint32_t expected=day<=days ? (weekday+day-1)%7 : UINT32_C(0x80b5000a);
                const uint64_t a[]={UINT64_C(0x1234000000000000)|(uint32_t)years[y],
                    UINT64_C(0x5678000000000000)|(uint32_t)month,UINT64_C(0x9abc000000000000)|(uint32_t)day};
                uint32_t got=(uint32_t)bbcpu_call(target[3],3,a);
                if(got!=expected) fprintf(stderr,"RTC weekday mismatch: %d-%02d-%02d got=%#x expected=%#x\n",years[y],month,day,got,expected);
                assert(got==expected);
            }
            weekday=(weekday+days)%7;
        }
    }
    const uint64_t invalid[][3]={{0,0,0},{10000,13,32},{0,1,1},{10000,1,1},
        {UINT32_MAX,1,1},{2026,1,0},{2026,1,UINT32_MAX}};
    const uint32_t errors[]={0x80b50009,0x80b50009,0x80b50008,0x80b50008,0x80b50008,0x80b5000a,0x80b5000a};
    for(unsigned i=0;i<7;++i) assert((uint32_t)bbcpu_call(target[3],3,invalid[i])==errors[i]);
    assert(*runtime_errno()==17);
}
int main(void) {
    bootstrap_initialize(1);uintptr_t targets[4];
    for(unsigned i=0;i<4;++i) {
        bootstrap_initialize(0);assert(!bootstrap_resolve(rtc_names[i],0));bootstrap_initialize(1);
        uintptr_t fn=bootstrap_resolve(rtc_names[i],0);assert(fn && !bootstrap_resolve(rtc_names[i],1));
        char wrong[64];snprintf(wrong,sizeof(wrong),"%.*s#0#F",(int)strcspn(rtc_names[i],"#"),rtc_names[i]);
        assert(!bootstrap_resolve(wrong,0));targets[i]=rtc_bridge(fn);
    }
    const char *zones[]={"UTC","Europe/Madrid","Asia/Kathmandu","America/New_York"};
    for(unsigned i=0;i<4;++i) {assert(!setenv("TZ",zones[i],1));tzset();check_rtc(targets);}
    assert(!bootstrap_resolve("zO9UL3qIINQ#X#Y",0)); /* No network time is invented. */
    puts("PASS: four scoped RTC APIs: real UTC/local clock brackets in four zones, signed minute offsets/extremes, 16-byte guarded unaligned output, Gregorian weekday grid/SDK error order, 32-bit ABI, resolution and errno");
    return 0;
}
