/* Synthetic VideoOut label queries and debug packets; no GPU completion or frame. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static const char *const present_names[]={"1qXLHIpROPE#A#B","jiItzS6+22g#A#B",
    "aPIZJTXC+cU#A#B","aj3L-iaFmyk#A#B"};
static uintptr_t label_base;
static int32_t label_result=16;
static unsigned queries;
static int opened,missing;
static void unused_provider(void) {}
static int32_t open_fixture(int32_t user,int32_t bus,int32_t index,const void *p) {
    (void)p;assert(user==255 && !bus && !index && !opened);opened=1;return 37;
}
static int32_t close_fixture(int32_t handle) {assert(handle==37 && opened);opened=0;return 0;}
static int32_t query_fixture(int32_t handle,uintptr_t *out) {
    assert(handle==37 && opened);++queries;*out=label_base;return label_result;
}
static int32_t set_fixture(uint32_t *out,uint32_t size,const char *label) {
    if(!out || !label) return -1;
    size_t len=strlen(label);uint32_t payload=(uint32_t)(((len+8)>>2)+((len+12)>>3)*2);
    if(size!=payload+2) return -1;
    const uint32_t head[]={UINT32_C(0xc0001000)|(payload<<16),UINT32_C(0x68750003)};
    memcpy(out,head,8);memcpy((unsigned char *)out+8,label,len+1);
    memset((unsigned char *)out+9+len,0,payload*4-len-1);return 0;
}
static uintptr_t present_resolver(const char *name) {
    if(!strcmp(name,"Up36PTk687E#G#H")) return (uintptr_t)open_fixture;
    if(!strcmp(name,"uquVH4-Du78#G#H")) return (uintptr_t)close_fixture;
    if(!strcmp(name,"OcQybQejHEY#G#H")) return missing==1 ? 0 : (uintptr_t)query_fixture;
    if(!strcmp(name,"jiItzS6+22g#A#B")) return missing==2 ? 0 : (uintptr_t)set_fixture;
    return (uintptr_t)unused_provider;
}
static uintptr_t present_bridge(uintptr_t target) {
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);
    code[0]=0x48;code[1]=0xb8;memcpy(code+2,&target,8);code[10]=0xff;code[11]=0xe0;
    bbcpu_add_guest_code((uintptr_t)code,16384);return (uintptr_t)code;
}
/* Shared with private original query dependency fixtures. */
static void check_wait_packets(uintptr_t target) {
    const uint64_t bases[]={UINT64_C(0x8000000000),UINT64_C(0x123456789abc),
        UINT64_C(0xffff000000000003),UINT64_MAX-63};
    unsigned char output[64],expected[64];*runtime_errno()=17;
    for(unsigned b=0;b<4;++b) for(unsigned index=0;index<16;++index) {
        label_base=bases[b];uint64_t address=label_base+(uint64_t)index*8;
        const uint32_t packet[]={UINT32_C(0xc0053c00),0x13,(uint32_t)address&~UINT32_C(3),
            (uint32_t)((address>>32)&65535),0,UINT32_MAX,10};
        memset(output,0xa5,sizeof(output));memset(expected,0xa5,sizeof(expected));
        memcpy(expected+1,packet,sizeof(packet));unsigned before=queries;
        const uint64_t args[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000007),
            UINT64_C(0x5678000000000025),UINT64_C(0x9abc000000000000)|index};
        assert((int32_t)bbcpu_call(target,4,args)==0 && queries==before+1);
        assert(!memcmp(output,expected,sizeof(output)) && *runtime_errno()==17);
    }
    const uint64_t invalid[][4]={{1,6,1,0},{1,8,1,0},{1,UINT32_MAX,1,0}};
    unsigned before=queries;
    for(unsigned i=0;i<3;++i) assert((int32_t)bbcpu_call(target,4,invalid[i])==-1);
    assert(queries==before);
}
static void check_present_markers(const uintptr_t targets[3]) {
    char label[256];unsigned char output[2048],expected[2048];
    const uint32_t colors[]={0,UINT32_C(0x12345678),UINT32_C(0x80000000),UINT32_MAX};
    *runtime_errno()=17;
    for(unsigned kind=0;kind<3;++kind) for(unsigned len=0;len<256;++len) for(unsigned c=0;c<4;++c) {
        for(unsigned i=0;i<len;++i) label[i]=(char)('A'+i%26);label[len]=0;
        uint32_t payload=kind ? ((len+12)>>2)+((len+16)>>3)*2 : ((len+8)>>2)+((len+12)>>3)*2;
        uint32_t tag=kind==0 ? UINT32_C(0x68750003) : kind==1 ? UINT32_C(0x6875000e) : UINT32_C(0x6875000f);
        uint32_t header=UINT32_C(0xc0001000)|(payload<<16);
        memset(output,0xa5,sizeof(output));memset(expected,0xa5,sizeof(expected));
        memcpy(expected+1,&header,4);memcpy(expected+5,&tag,4);memset(expected+9,0,payload*4);
        memcpy(expected+9,label,len+1);
        if(kind) memcpy(expected+10+len,&colors[c],4); /* Color immediately follows NUL. */
        uint64_t args[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000000)|(payload+2),
            (uintptr_t)label,UINT64_C(0x5678000000000000)|colors[c]};
        assert((int32_t)bbcpu_call(targets[kind],4,args)==0 && !memcmp(output,expected,sizeof(output)));
        memset(output,0xa5,sizeof(output));args[1]=payload+3;
        assert((int32_t)bbcpu_call(targets[kind],4,args)==-1);
        for(unsigned i=0;i<sizeof(output);++i) assert(output[i]==0xa5);
        assert(*runtime_errno()==17);
    }
    const uint64_t invalid[][4]={{0,8,1,0},{1,8,0,0}};
    for(unsigned kind=0;kind<3;++kind) for(unsigned i=0;i<2;++i)
        assert((int32_t)bbcpu_call(targets[kind],4,invalid[i])==-1);
}
int main(void) {
    bootstrap_initialize(1);for(unsigned i=0;i<4;++i) assert(!bootstrap_resolve(present_names[i],0));
    for(missing=1;missing<=2;++missing) assert(bootstrap_graphics_bind(present_resolver)==-1);
    missing=0;assert(bootstrap_graphics_bind(present_resolver)==0);
    uintptr_t targets[4];
    for(unsigned i=0;i<4;++i) {
        assert(bootstrap_graphics_owns(present_names[i]) && !bootstrap_resolve(present_names[i],1));
        bootstrap_initialize(0);assert(!bootstrap_resolve(present_names[i],0));bootstrap_initialize(1);
        char wrong[64];snprintf(wrong,sizeof(wrong),"%.*s#G#H",(int)strcspn(present_names[i],"#"),present_names[i]);
        assert(!bootstrap_resolve(wrong,0));targets[i]=present_bridge(bootstrap_resolve(present_names[i],0));
    }
    uint64_t wait_args[]={1,7,37,0};unsigned before=queries;
    assert((uint32_t)bbcpu_call(targets[0],4,wait_args)==UINT32_C(0x8029000b) && queries==before);
    uint64_t open_args[]={255,0,0,0};
    uintptr_t open=present_bridge(bootstrap_resolve("Up36PTk687E#G#H",0));
    uintptr_t close=present_bridge(bootstrap_resolve("uquVH4-Du78#G#H",0));
    assert(bbcpu_call(open,4,open_args)==37);check_wait_packets(targets[0]);
    unsigned char output[64];memset(output,0xa5,sizeof(output));
    wait_args[0]=(uintptr_t)(output+1);label_result=(int32_t)UINT32_C(0x8029000b);
    assert((int32_t)bbcpu_call(targets[0],4,wait_args)==label_result);
    label_result=0;assert((int32_t)bbcpu_call(targets[0],4,wait_args)==-1);label_result=16;
    before=queries;wait_args[0]=1;wait_args[3]=16;
    assert((int32_t)bbcpu_call(targets[0],4,wait_args)==-1 && queries==before);
    wait_args[3]=0;wait_args[0]=0;assert((int32_t)bbcpu_call(targets[0],4,wait_args)==-1 && queries==before);
    for(unsigned i=0;i<sizeof(output);++i) assert(output[i]==0xa5);
    uint64_t close_args[]={37};assert(bbcpu_call(close,1,close_args)==0);
    wait_args[0]=1;assert((uint32_t)bbcpu_call(targets[0],4,wait_args)==UINT32_C(0x8029000b) && queries==before);
    check_present_markers(targets+1);
    puts("PASS: real-provider flip label wait packets/lifecycle/errors and three marker encoders: 256 lengths/four colors, exact NUL/color/padding placement, 48-bit addresses, 32-bit ABI, guards/errno and scoped activation");
    return 0;
}
