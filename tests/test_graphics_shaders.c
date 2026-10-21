/* Synthetic shader/VGT packet ABI fixtures; no executable shader or game data. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static const char *const names[]={"5uFKckiJYRM#A#B","mLVL7N7BVBg#A#B",
    "V31V01UiScY#A#B","cFCp0NX8wf0#A#B","MYRtYhojKdA#A#B"};
static int missing;
static void unused_provider(void) {}
static uintptr_t shader_bridge(uintptr_t target) {
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);
    code[0]=0x48;code[1]=0xb8;memcpy(code+2,&target,8);code[10]=0xff;code[11]=0xe0;
    bbcpu_add_guest_code((uintptr_t)code,16384);return (uintptr_t)code;
}
static int32_t ps_model(unsigned update,void *out,uint32_t size,const uint32_t *r) {
    if(!out || size<40) return -1;
    if(r && r[1]) return -1;
    uint32_t packet[30];unsigned n=0;
#define WORD(x) (packet[n++]=(x))
#define PAIR(reg,x,y) do {WORD(UINT32_C(0xc0027600));WORD(reg);WORD(x);WORD(y);} while(0)
#define CONTEXT(reg,count,x,y) do {WORD((count==2 ? UINT32_C(0xc0026900) : UINT32_C(0xc0016900)) ^ (update ? UINT32_C(0x7900) : 0));WORD((reg)|(update ? UINT32_C(0xc01e0000) : 0));WORD(x);if(count==2) WORD(y);} while(0)
    PAIR(8,r ? r[0] : 0,0);
    if(r) {
        PAIR(10,r[2],r[3]);CONTEXT(0x1c4,2,r[4],r[5]);CONTEXT(0x1b3,2,r[6],r[7]);
        CONTEXT(0x1b6,1,r[8],0);CONTEXT(0x1b8,1,r[9],0);
        CONTEXT(0x203,1,r[10],0);CONTEXT(0x8f,1,r[11],0);
    } else {CONTEXT(0x203,1,0,0);/* CB_SHADER_MASK remains a normal context write. */
        WORD(UINT32_C(0xc0016900));WORD(0x8f);WORD(15);}
    WORD(r ? UINT32_C(0xc00a1000) : UINT32_C(0xc01c1000));WORD(0);
    memcpy(out,packet,n*4);return 0;
#undef WORD
#undef PAIR
#undef CONTEXT
}
static int32_t ps_set(uint32_t *o,uint32_t s,const uint32_t *r) {return ps_model(0,o,s,r);}
static int32_t ps_update(uint32_t *o,uint32_t s,const uint32_t *r) {return ps_model(1,o,s,r);}
static int32_t vs_update(uint32_t *out,uint32_t size,const uint32_t *r,uint32_t modifier) {
    if(!out || size<29 || !r || (modifier&UINT32_C(0xfcfffc3f)) || r[1]) return -1;
    uint32_t packet[]={UINT32_C(0xc0027600),0x48,r[0],0,
        UINT32_C(0xc0027600),0x4a,modifier ? (r[2]&UINT32_C(0xfcfffc3f))|modifier : r[2],r[3],
        UINT32_C(0xc0011000),UINT32_C(0xc01e0207),r[6],
        UINT32_C(0xc0011000),UINT32_C(0xc01e01b1),r[4],
        UINT32_C(0xc0011000),UINT32_C(0xc01e01c3),r[5],UINT32_C(0xc00a1000),0};
    memcpy(out,packet,sizeof(packet));return 0;
}
static int32_t vgt_set(uint32_t *out,uint32_t size,uint32_t group,uint32_t partial,uint32_t eop) {
    if(!out || size!=3 || group>255 || partial>1 || eop>1) return -1;
    uint32_t packet[]={UINT32_C(0xc0016900),0x2aa,group|(partial<<16)};
    memcpy(out,packet,sizeof(packet));return 0;
}
static int32_t vgt_reset(uint32_t *out,uint32_t size) {return vgt_set(out,size,255,0,0);}
static uintptr_t resolve_shaders(const char *name) {
    uintptr_t targets[]={(uintptr_t)ps_set,(uintptr_t)ps_update,(uintptr_t)vs_update,
        (uintptr_t)vgt_set,(uintptr_t)vgt_reset};
    for(unsigned i=0;i<5;++i) if(!strcmp(name,names[i])) return missing==(int)i+1 ? 0 : targets[i];
    return (uintptr_t)unused_provider;
}
static void check_shader_packets(const uintptr_t targets[5]) {
    unsigned char output[256],expected[256],input[49];uint32_t regs[12];
    unsigned char *bounded=runtime_low_map(32768,PROT_READ|PROT_WRITE);assert(bounded);
    assert(!mprotect(bounded+16384,16384,PROT_NONE));*runtime_errno()=17;
    for(unsigned kind=0;kind<3;++kind) {
        const uint32_t minimum=kind==2 ? 29 : 40;
        for(unsigned sample=0;sample<32;++sample) {
            for(unsigned i=0;i<12;++i) regs[i]=UINT32_C(0x91abcdef)*(i+sample*12+1);
            regs[1]=0;memcpy(input+1,regs,sizeof(regs));
            for(unsigned m=0;m<(kind==2 ? 64u : 2u);++m) {
                uint32_t modifier=((m&15)<<6)|((m>>4)<<24);
                const uint32_t *r=kind<2 && m==1 ? NULL : (const uint32_t *)(input+1);
                const uint32_t sizes[]={minimum,minimum+1,UINT32_MAX};
                for(unsigned j=0;j<3;++j) {
                    memset(expected,0xa5,sizeof(expected));memset(output,0xa5,sizeof(output));
                    assert((kind==2 ? vs_update((void *)(expected+1),sizes[j],r,modifier) :
                        ps_model(kind,expected+1,sizes[j],r))==0);
                    uint64_t a[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000000)|sizes[j],
                        (uintptr_t)r,UINT64_C(0x5678000000000000)|modifier};
                    assert((int32_t)bbcpu_call(targets[kind],4,a)==0);
                    assert(!memcmp(output,expected,sizeof(output)) && !memcmp(input+1,regs,sizeof(regs)));
                }
            }
        }
        const uint64_t invalid[][4]={{0,minimum,1,0},{1,minimum-1,1,0},{1,0,1,0}};
        for(unsigned i=0;i<3;++i) assert((int32_t)bbcpu_call(targets[kind],4,invalid[i])==-1);
        /* Error reads only the high-address word, not the whole register block. */
        uint32_t bad[2]={0,1};memcpy(bounded+16376,bad,8);memset(output,0xa5,sizeof(output));
        uint64_t a[]={(uintptr_t)(output+1),minimum,(uintptr_t)(bounded+16376),0};
        assert((int32_t)bbcpu_call(targets[kind],4,a)==-1);
        for(unsigned i=0;i<sizeof(output);++i) assert(output[i]==0xa5);
        /* Success reads exactly 12 PS registers or seven VS registers. */
        unsigned bytes=kind==2 ? 28 : 48;regs[1]=0;memcpy(bounded+16384-bytes,regs,bytes);
        a[2]=(uintptr_t)(bounded+16384-bytes);assert((int32_t)bbcpu_call(targets[kind],4,a)==0);
        if(kind==2) {
            a[2]=0;assert((int32_t)bbcpu_call(targets[kind],4,a)==-1);
            for(unsigned bit=0;bit<32;++bit) if(UINT32_C(0xfcfffc3f)&(UINT32_C(1)<<bit)) {
                a[2]=1;a[3]=UINT32_C(1)<<bit;
                assert((int32_t)bbcpu_call(targets[kind],4,a)==-1);
            }
        }
        assert(*runtime_errno()==17);
    }
    assert(!mprotect(bounded+16384,16384,PROT_READ|PROT_WRITE));
    for(unsigned group=0;group<258;++group) for(unsigned partial=0;partial<3;++partial)
        for(unsigned eop=0;eop<3;++eop) {
            memset(output,0xa5,sizeof(output));memset(expected,0xa5,sizeof(expected));
            uint32_t g=group==257 ? UINT32_MAX : group;
            int32_t result=vgt_set((void *)(expected+1),3,g,partial,eop);
            uint64_t a[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000003),
                UINT64_C(0x5678000000000000)|g,UINT64_C(0x9abc000000000000)|partial,
                UINT64_C(0xdef0000000000000)|eop};
            assert((int32_t)bbcpu_call(targets[3],5,a)==result && !memcmp(output,expected,sizeof(output)));
        }
    memset(output,0xa5,sizeof(output));memset(expected,0xa5,sizeof(expected));
    assert(vgt_reset((void *)(expected+1),3)==0);
    uint64_t a[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000003)};
    assert((int32_t)bbcpu_call(targets[4],2,a)==0 && !memcmp(output,expected,sizeof(output)));
    for(unsigned kind=3;kind<5;++kind) {
        const uint64_t invalid[][5]={{0,3,0,0,0},{1,2,0,0,0},{1,4,0,0,0},{1,UINT32_MAX,0,0,0}};
        for(unsigned i=0;i<4;++i) assert((int32_t)bbcpu_call(targets[kind],5,invalid[i])==-1);
    }
    assert(*runtime_errno()==17);
}
int main(void) {
    bootstrap_initialize(1);
    for(unsigned i=0;i<5;++i) assert(!bootstrap_resolve(names[i],0));
    for(missing=1;missing<=5;++missing) assert(bootstrap_graphics_bind(resolve_shaders)==-1);
    missing=0;assert(bootstrap_graphics_bind(resolve_shaders)==0);
    uintptr_t targets[5];
    for(unsigned i=0;i<5;++i) {
        assert(bootstrap_graphics_owns(names[i]) && !bootstrap_resolve(names[i],1));
        bootstrap_initialize(0);assert(!bootstrap_resolve(names[i],0));bootstrap_initialize(1);
        char wrong[64];snprintf(wrong,sizeof(wrong),"%.*s#G#H",(int)strcspn(names[i],"#"),names[i]);
        assert(!bootstrap_graphics_owns(wrong) && !bootstrap_resolve(wrong,0));
        targets[i]=shader_bridge(bootstrap_resolve(names[i],0));
    }
    check_shader_packets(targets);
    puts("PASS: five scoped shader/VGT encoders: PS350 set/update/null disable, all VS modifier combinations, VGT ranges, exact packets/padding, register bounds, guards, 32-bit ABI and errno");
    return 0;
}
