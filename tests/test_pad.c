/* Original synthetic keyboard provider: guest ABI, lifecycle and real sampling.
 * Expected wire offsets come from the 120-byte public OrbisPadData layout. */
#include "bootstrap_pad.h"
#include "bootstrap_contracts.h"
#include "runtime.h"
#include "bbcpu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static bool keys[512];
static int capture, available=1, key_calls;
static const struct { const char *name; int code; } names[] = {
    {"Space",44},{"Left Shift",225},{"E",8},{"Q",20},{"1",30},{"3",32},{"R",21},{"F",9},
    {"Z",29},{"C",6},{"Return",40},{"I",12},{"L",15},{"K",14},{"J",13},
    {"A",4},{"D",7},{"W",26},{"S",22},{"Left",80},{"Right",79},{"Up",82},{"Down",81},
    {"Tab",43},{"Backspace",42},
};
static const bool *keyboard(int *count) { ++key_calls; *count=512; return available ? keys : NULL; }
static int scancode(const char *name) {
    for (size_t i=0;i<sizeof(names)/sizeof(*names);++i) if (!strcmp(name,names[i].name)) return names[i].code;
    return 0;
}
static int captured(void) { return capture; }
static uintptr_t bridge(const char *name) {
    const uintptr_t target=bootstrap_resolve(name,0); assert(target && !bootstrap_resolve(name,1));
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE); assert(code);
    code[0]=0x48; code[1]=0xb8; memcpy(code+2,&target,8); code[10]=0xff; code[11]=0xe0;
    bbcpu_add_guest_code((uintptr_t)code,16384); return (uintptr_t)code;
}
static int32_t call(uintptr_t entry,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
    const uint64_t args[4]={a,b,c,d}; return (int32_t)bbcpu_call(entry,4,args);
}
static uint64_t load(const void *p,size_t size) { uint64_t v=0;memcpy(&v,p,size);return v; }
int main(void) {
    bootstrap_initialize(1);
    assert(!bootstrap_resolve("hv1luiJrqQM#H#I",0));
    assert(!bootstrap_resolve("RR4novUEENY#H#I",0));
    BootstrapKeyboard backend={.initialized_subsystems=0x2020,.keyboard=keyboard,.scancode=scancode,.captured=captured};
    backend.initialized_subsystems=0x20; assert(bootstrap_pad_bind(&backend)==-1);
    assert(!bootstrap_resolve("hv1luiJrqQM#H#I",0));
    backend.initialized_subsystems=0x2020; available=0; assert(bootstrap_pad_bind(&backend)==-1);
    available=1; assert(bootstrap_pad_bind(&backend)==0); assert(bootstrap_pad_bind(&backend)==-1);
    assert(!bootstrap_resolve("hv1luiJrqQM#A#B",0));
    assert(!bootstrap_resolve("rIZnR6eSpvk#H#I",0)); /* Unsupported sensor API stays unresolved. */
    bootstrap_initialize(0);assert(!bootstrap_resolve("RR4novUEENY#H#I",0));bootstrap_initialize(1);
    assert(!bootstrap_resolve("RR4novUEENY#A#B",0));
    const uintptr_t lightbar=bridge("RR4novUEENY#H#I");
    *runtime_errno()=17;
    assert((uint32_t)call(lightbar,1,0,0,0)==0x80920005);
    assert((uint32_t)call(lightbar,UINT64_MAX,1,0,0)==0x80920005);
    const uintptr_t init=bridge("hv1luiJrqQM#H#I"),open=bridge("xk0AcarP3V4#H#I");
    const uintptr_t close=bridge("6ncge5+l5Qs#H#I"),read=bridge("YndgXqQVV7c#H#I");
    const uintptr_t buffered=bridge("q1cHNfGycLI#H#I"),info=bridge("gjP9-KQzoUk#H#I");
    assert((uint32_t)call(open,1,0,0,0)==0x80920005);
    assert(call(init,0,0,0,0)==0 && call(init,0,0,0,0)==0);
    assert((uint32_t)call(open,2,0,0,0)==0x80920001);
    assert((uint32_t)call(open,1,2,0,0)==0x80920001);
    assert((uint32_t)call(open,1,0,1,0)==0x80920001);
    assert((uint32_t)call(open,1,0,0,1)==0x80920001);
    int32_t handle=call(open,1,0,0,0); assert(handle>0);
    assert((uint32_t)call(open,1,0,0,0)==0x80920004);
    assert((uint32_t)call(lightbar,UINT64_MAX,0,0,0)==0x80920001);
    assert((uint32_t)call(lightbar,UINT64_MAX,1,0,0)==0x80920003);
    assert((uint32_t)call(lightbar,handle,1,0,0)==0x80920001);
    const size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guard=mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    assert(guard!=MAP_FAILED && !mprotect(guard+page,page,PROT_NONE));
    unsigned char *rgb=guard+page-3;
    for(unsigned r=0;r<=13;++r) for(unsigned g=0;g<=13;++g) for(unsigned b=0;b<=13;++b) {
        rgb[0]=(unsigned char)r;rgb[1]=(unsigned char)g;rgb[2]=(unsigned char)b;
        const uint32_t expected_error=r<=12 && g<=12 && b<=12 ? 0x80920006 : 0x80920101;
        assert((uint32_t)call(lightbar,UINT64_C(0x1234000000000000)|(uint32_t)handle,(uintptr_t)rgb,1,UINT64_MAX)==expected_error);
        assert(rgb[0]==r && rgb[1]==g && rgb[2]==b && *runtime_errno()==17);
    }
    rgb[0]=255;assert((uint32_t)call(lightbar,handle,(uintptr_t)rgb,0,0)==0x80920101);
    assert((uint32_t)call(lightbar,handle,(uintptr_t)(guard+page-2),0,0)==0x80920001);
    assert((uint32_t)call(lightbar,handle,(uintptr_t)(guard+page),0,0)==0x80920001);
    assert(!munmap(guard,page*2));
    unsigned char packet[128],expected[128]; memset(packet,0xa5,sizeof(packet));
    const uint64_t before=runtime_process_time_us();
    assert(call(read,handle,(uintptr_t)(packet+1),0,0)==0);
    const uint64_t timestamp=load(packet+1+80,8); assert(timestamp>=before && timestamp<=runtime_process_time_us());
    memset(expected,0xa5,sizeof(expected)); memset(expected+1,0,120);
    memset(expected+1+4,128,4); const float identity=1; memcpy(expected+1+24,&identity,4);
    expected[1+76]=expected[1+104]=1; memcpy(expected+1+80,&timestamp,8);
    assert(!memcmp(packet,expected,sizeof(packet))); /* Exact 120 bytes including all reserved bytes. */
    keys[44]=keys[26]=keys[7]=keys[21]=true; /* Space, W, D, R. */
    assert(call(read,handle,(uintptr_t)(packet+1),0,0)==0);
    assert(load(packet+1,4)==0x4100 && packet[1+4]==255 && packet[1+5]==0 && packet[1+8]==255);
    keys[4]=keys[22]=true; /* Opposing directions cancel to center. */
    assert(call(read,handle,(uintptr_t)(packet+1),0,0)==0 && packet[1+4]==128 && packet[1+5]==128);
    keys[43]=true;
    assert(call(read,handle,(uintptr_t)(packet+1),0,0)==0 && packet[1+52]==1);
    const unsigned touch_id=packet[1+64]; assert(touch_id && load(packet+1+60,2)==480);
    assert(call(read,handle,(uintptr_t)(packet+1),0,0)==0 && packet[1+64]==touch_id);
    capture=1; assert(call(read,handle,(uintptr_t)(packet+1),0,0)==0);
    assert(!load(packet+1,4) && packet[1+52]==0 && packet[1+4]==128 && packet[1+5]==128);
    capture=0; keys[43]=false; keys[42]=true;
    assert(call(read,handle,(uintptr_t)(packet+1),0,0)==0 && load(packet+1+60,2)==1440 && packet[1+64]!=touch_id);
    unsigned char many[244]; memset(many,0xa5,sizeof(many));
    assert(call(buffered,handle,(uintptr_t)(many+1),2,0)==1);
    for (size_t i=121;i<sizeof(many);++i) assert(many[i]==0xa5);
    assert((uint32_t)call(buffered,handle,1,0,0)==0x80920001);
    assert((uint32_t)call(buffered,handle,1,65,0)==0x80920001);
    assert((uint32_t)call(read,handle,0,0,0)==0x80920001);
    unsigned char details[32]; memset(details,0xa5,sizeof(details));
    assert(call(info,handle,(uintptr_t)(details+1),0,0)==0);
    assert(details[0]==0xa5 && details[29]==0xa5 && load(details+1+4,2)==1920 && load(details+1+6,2)==943);
    assert(details[1+11]==1 && details[1+12]==1);
    memcpy(expected,packet,sizeof(packet)); available=0;
    assert((uint32_t)call(read,handle,(uintptr_t)(packet+1),0,0)==0x80920005 && !memcmp(expected,packet,sizeof(packet)));
    available=1; assert(call(close,handle,0,0,0)==0);
    const int previous_calls=key_calls;
    assert((uint32_t)call(lightbar,handle,1,0,0)==0x80920003);
    assert((uint32_t)call(read,handle,1,0,0)==0x80920003 && key_calls==previous_calls);
    assert((uint32_t)call(info,handle,1,0,0)==0x80920003);
    int32_t next=call(open,1,0,0,0); assert(next>0 && next!=handle);
    assert((uint32_t)call(read,handle,1,0,0)==0x80920003);
    assert((uint32_t)call(lightbar,handle,1,0,0)==0x80920003);
    assert(call(close,next,0,0,0)==0);
    puts("PASS: keyboard pad guest ABI, initialization/lifecycle, provider key samples, overlay capture, touch transitions, exact layouts, buffer guards, stale handles and unavailable light-bar validation/three-byte guarded RGB/errno");
}
