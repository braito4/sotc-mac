/* Synthetic draw command encoders; no vertex/index/shader assets or GPU work. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static const char *const draw_names[]={"4v+otIIdjqg#A#B","ED9-Fjr8Ta4#A#B",
    "oYM+YzfCm2Y#A#B","thbPcG7E7qk#A#B"};
static uint64_t draw_args[10];
static int missing;
static void unused_provider(void) {}
static int32_t draw_model(unsigned kind,const uint64_t a[10]) {
    void *out=(void *)(uintptr_t)a[0];uint32_t size=(uint32_t)a[1];
    if(!out || size!=(kind==3 ? 16u : 9u)) return -1;
    uint32_t offset=(uint32_t)a[2],stage=(uint32_t)a[kind==3 ? 5 : 3];
    uint32_t vertex=(uint32_t)a[kind==3 ? 6 : 4],instance=(uint32_t)a[kind==3 ? 7 : 5];
    uint32_t flags=(uint32_t)a[kind==2 ? 4 : kind==3 ? 8 : 6];
    if(kind!=2 && ((stage!=2 && stage!=4 && stage!=6) || vertex>=16 || instance>=16)) return -1;
    uint32_t p[16]={0};
    if(kind==2) {
        p[0]=UINT32_C(0xc0033500)|(flags&1);p[1]=(uint32_t)a[3];p[2]=offset;p[3]=(uint32_t)a[3];
    } else {
        uint32_t base=stage==2 ? 0x4c : stage==4 ? 0xcc : 0x14c;
        uint32_t v=vertex ? base+vertex : 0,i=instance ? base+instance : 0;
        if(kind==3) {
            p[0]=UINT32_C(0xc0011000);p[3]=UINT32_C(0xc0089d00)|(flags&1);
            p[4]=offset;p[5]=v;p[6]=i;p[7]=a[4] ? UINT32_C(0x40000000) : 0;
            p[8]=(uint32_t)a[3];p[9]=(uint32_t)a[4];p[10]=(uint32_t)(a[4]>>32);p[11]=20;
            p[13]=UINT32_C(0xc0011000);
            memcpy(out,p,8);memcpy((unsigned char *)out+12,p+3,48);return 0;
        }
        p[0]=(kind==0 ? UINT32_C(0xc0032400) : UINT32_C(0xc0032500))|(flags&1);
        p[1]=offset;p[2]=v;p[3]=i;p[4]=kind==0 ? 2 : 0;
    }
    p[5]=UINT32_C(0xc0021000);memcpy(out,p,28);return 0;
}
static int32_t indirect_fixture(uint32_t *out,uint32_t size,uint32_t offset,uint32_t stage,
                                  uint32_t v,uint32_t i,uint32_t flags) {
    const uint64_t a[10]={(uintptr_t)out,size,offset,stage,v,i,flags};return draw_model(0,a);
}
static int32_t indexed_fixture(uint32_t *out,uint32_t size,uint32_t offset,uint32_t stage,
                                 uint32_t v,uint32_t i,uint32_t flags) {
    const uint64_t a[10]={(uintptr_t)out,size,offset,stage,v,i,flags};return draw_model(1,a);
}
static int32_t offset_fixture(uint32_t *out,uint32_t size,uint32_t offset,uint32_t count,uint32_t flags) {
    const uint64_t a[10]={(uintptr_t)out,size,offset,count,flags};return draw_model(2,a);
}
static int32_t count_fixture(uint32_t *out,uint32_t size,uint32_t offset,uint32_t count,
                              uint64_t address,uint32_t stage,uint32_t v,uint32_t i,uint32_t flags) {
    const uint64_t a[10]={(uintptr_t)out,size,offset,count,address,stage,v,i,flags};return draw_model(3,a);
}
static uintptr_t draw_resolver(const char *name) {
    const uintptr_t functions[]={(uintptr_t)indirect_fixture,(uintptr_t)indexed_fixture,
        (uintptr_t)offset_fixture,(uintptr_t)count_fixture};
    for(unsigned i=0;i<4;++i) if(!strcmp(name,draw_names[i])) return missing==(int)i+1 ? 0 : functions[i];
    return (uintptr_t)unused_provider;
}
static uintptr_t draw_bridge(uintptr_t target,unsigned count) {
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);unsigned n=0;
#define EMIT(...) do {const unsigned char b[]={__VA_ARGS__};memcpy(code+n,b,sizeof(b));n+=sizeof(b);} while(0)
    EMIT(0x48,0x83,0xec,0x28,0x48,0xb8);uintptr_t p=(uintptr_t)draw_args;memcpy(code+n,&p,8);n+=8;
    for(unsigned i=6;i<count;++i) {
        EMIT(0x4c,0x8b,0x50,(unsigned char)(i*8),0x4c,0x89,0x54,0x24,(unsigned char)((i-6)*8));
    }
    EMIT(0x48,0x8b,0x38,0x48,0x8b,0x70,8,0x48,0x8b,0x50,16,
         0x48,0x8b,0x48,24,0x4c,0x8b,0x40,32,0x4c,0x8b,0x48,40,0x48,0xb8);
    memcpy(code+n,&target,8);n+=8;EMIT(0xff,0xd0,0x48,0x83,0xc4,0x28,0xc3);
#undef EMIT
    bbcpu_add_guest_code((uintptr_t)code,16384);return (uintptr_t)code;
}
static int32_t call_draw(uintptr_t target,const uint64_t a[10]) {
    memcpy(draw_args,a,sizeof(draw_args));return (int32_t)bbcpu_call(target,0,NULL);
}
static void check_draw_packets(const uintptr_t targets[4]) {
    const uint32_t offsets[]={0,1,15,16,UINT32_MAX},flags[]={0,1,UINT32_C(0xe0000000),UINT32_MAX};
    const uint64_t addresses[]={0,UINT64_C(0x1234567887654321),UINT64_MAX};
    unsigned char out[128],expected[128];*runtime_errno()=17;
    for(unsigned kind=0;kind<4;++kind) for(unsigned st=0;st<9;++st)
        for(unsigned v=0;v<5;++v) for(unsigned i=0;i<5;++i) for(unsigned f=0;f<4;++f) {
            if(kind==2 && (st || v || i)) continue;
            unsigned count=kind==3 ? 3 : 1;
            for(unsigned address=0;address<count;++address) {
                uint32_t stage=st==8 ? UINT32_MAX : st;
                uint64_t a[10]={(uintptr_t)(out+1),kind==3 ? 16 : 9,
                    UINT64_C(0x12340000fedcba98),UINT64_C(0x5678000000000000)|stage,
                    UINT64_C(0x9abc000000000000)|offsets[v],UINT64_C(0xdef0000000000000)|offsets[i],
                    UINT64_C(0x1234000000000000)|flags[f],0,0,0};
                if(kind==2) {a[3]=UINT64_MAX;a[4]=UINT64_C(0x5678000000000000)|flags[f];}
                if(kind==3) {
                    a[3]=UINT64_MAX;a[4]=addresses[address];a[5]=UINT64_C(0x9abc000000000000)|stage;
                    a[6]=UINT64_C(0xdef0000000000000)|offsets[v];
                    a[7]=UINT64_C(0x1234000000000000)|offsets[i];a[8]=UINT64_C(0x5678000000000000)|flags[f];
                }
                a[1]|=UINT64_C(0x9abc000000000000);
                memset(out,0xa5,sizeof(out));memset(expected,0xa5,sizeof(expected));
                uint64_t reference[10];memcpy(reference,a,sizeof(reference));reference[0]=(uintptr_t)(expected+1);
                int32_t result=draw_model(kind,reference);
                assert(call_draw(targets[kind],a)==result && !memcmp(out,expected,sizeof(out)));
                assert(*runtime_errno()==17);
            }
        }
    for(unsigned kind=0;kind<4;++kind) {
        uint64_t a[10]={0,kind==3 ? 16 : 9,0,2,0,2,0,0,0,0};
        assert(call_draw(targets[kind],a)==-1);
        a[0]=1;a[1]++;assert(call_draw(targets[kind],a)==-1);
        a[1]=0;assert(call_draw(targets[kind],a)==-1);
    }
}
int main(void) {
    bootstrap_initialize(1);
    for(unsigned i=0;i<4;++i) assert(!bootstrap_resolve(draw_names[i],0));
    for(missing=1;missing<=4;++missing) assert(bootstrap_graphics_bind(draw_resolver)==-1);
    missing=0;assert(bootstrap_graphics_bind(draw_resolver)==0);
    const unsigned counts[]={7,7,5,9};uintptr_t targets[4];
    for(unsigned i=0;i<4;++i) {
        assert(bootstrap_graphics_owns(draw_names[i]) && !bootstrap_resolve(draw_names[i],1));
        bootstrap_initialize(0);assert(!bootstrap_resolve(draw_names[i],0));bootstrap_initialize(1);
        char wrong[64];snprintf(wrong,sizeof(wrong),"%.*s#G#H",(int)strcspn(draw_names[i],"#"),draw_names[i]);
        assert(!bootstrap_resolve(wrong,0));targets[i]=draw_bridge(bootstrap_resolve(draw_names[i],0),counts[i]);
    }
    check_draw_packets(targets);
    puts("PASS: four draw encoders: retail VS/ES/LS validation, SGPR offsets, indexed/auto/count packets, full 64-bit count address, 5/7/9 argument guest ABI, predicate/NOP padding, guards/errors/errno and scoped activation");
    return 0;
}
