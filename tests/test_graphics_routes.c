/* Synthetic provider routing/ownership checks; no renderer or game input. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static int missing_wait, missing_permission, missing_doorbell, calls;
static uint32_t submit_lock;
static int32_t submits_allowed(void) {return submit_lock==0;}
static void marker(void) {}
/* Synthetic provider packet writers; public PM4 NOP debug-marker layout.
 * Actual pinned renderer/original-module comparisons are separate checks. */
static int32_t marker_push(uint32_t *out,uint32_t size,const char *label) {
    if (!out || !label) return -1;
    size_t len=strlen(label);uint32_t payload=(uint32_t)(((len+8)>>2)+((len+12)>>3)*2);
    if(size!=payload+2) return -1;
    uint32_t head[2]={UINT32_C(0xc0001000)|(payload<<16),UINT32_C(0x68750001)};
    memcpy(out,head,8);memcpy((unsigned char *)out+8,label,len+1);
    memset((unsigned char *)out+9+len,0,payload*4-len-1);return 0;
}
static int32_t marker_pop(uint32_t *out,uint32_t size) {
    if(!out || size!=6) return -1;
    const uint32_t packet[6]={UINT32_C(0xc0041000),UINT32_C(0x68750002),0,0,0,0};
    memcpy(out,packet,sizeof(packet));return 0;
}
static uintptr_t marker_bridge(uintptr_t target) {
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);
    code[0]=0x48;code[1]=0xb8;memcpy(code+2,&target,8);code[10]=0xff;code[11]=0xe0;
    bbcpu_add_guest_code((uintptr_t)code,16384);return (uintptr_t)code;
}
static void check_marker_packets(uintptr_t push,uintptr_t pop) {
    unsigned char output[2048],expected[2048];char label[256];
    *runtime_errno()=17;
    for(unsigned len=0;len<256;++len) {
        for(unsigned i=0;i<len;++i) label[i]=(char)('A'+i%26);
        label[len]=0;
        const uint32_t payload=((len+8)>>2)+((len+12)>>3)*2,size=payload+2;
        memset(expected,0xa5,sizeof(expected));
        uint32_t header=UINT32_C(0xc0001000)|(payload<<16),tag=UINT32_C(0x68750001);
        memcpy(expected+1,&header,4);memcpy(expected+5,&tag,4);
        memset(expected+9,0,payload*4);memcpy(expected+9,label,len+1);
        memset(output,0xa5,sizeof(output));
        uint64_t args[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000000)|size,(uintptr_t)label};
        assert((int32_t)bbcpu_call(push,3,args)==0 && !memcmp(output,expected,sizeof(output)));
        memset(output,0xa5,sizeof(output));args[1]=size+1;
        assert((int32_t)bbcpu_call(push,3,args)==-1);
        for(unsigned i=0;i<sizeof(output);++i) assert(output[i]==0xa5);
    }
    const uint64_t invalid_push[][3]={{0,6,1},{1,6,0}};
    for(unsigned i=0;i<2;++i) assert((int32_t)bbcpu_call(push,3,invalid_push[i])==-1);
    memset(output,0xa5,sizeof(output));memset(expected,0xa5,sizeof(expected));
    const uint32_t packet[6]={UINT32_C(0xc0041000),UINT32_C(0x68750002),0,0,0,0};
    memcpy(expected+1,packet,sizeof(packet));
    const uint64_t args[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000006),1,3,5,7};
    assert((int32_t)bbcpu_call(pop,6,args)==0 && !memcmp(output,expected,sizeof(output)));
    const uint64_t invalid_pop[][2]={{0,6},{1,5},{1,7}};
    for(unsigned i=0;i<3;++i) assert((int32_t)bbcpu_call(pop,2,invalid_pop[i])==-1);
    assert(*runtime_errno()==17);
}
/* Independent synthetic PM4 expectations, shared with private original checks. */
static void check_cs_packets(uintptr_t target) {
    unsigned char out[160],expected[160],registers[30];
    uint32_t r[7];*runtime_errno()=17;
    for (unsigned sample=0;sample<64;++sample) {
        for(unsigned i=0;i<7;++i) r[i]=UINT32_C(0x9182abcd)*(sample*7+i+1);
        r[1]=0;memcpy(registers+1,r,sizeof(r));
        for(unsigned m=0;m<16;++m) {
            uint32_t modifier=m<<6;
            uint32_t packet[]={UINT32_C(0xc0027602),0x20c,r[0],0,
                UINT32_C(0xc0027602),0x212,
                modifier ? (r[2]&~UINT32_C(0x3c0))|modifier : r[2],r[3],
                UINT32_C(0xc0037602),0x207,r[4],r[5],r[6],UINT32_C(0xc00a1000),0};
            memset(expected,0xa5,sizeof(expected));memcpy(expected+1,packet,sizeof(packet));
            const uint32_t sizes[]={25,26,UINT32_MAX};
            for(unsigned j=0;j<3;++j) {
                memset(out,0xa5,sizeof(out));
                uint64_t args[]={(uintptr_t)(out+1),UINT64_C(0x1234000000000000)|sizes[j],
                    (uintptr_t)(registers+1),UINT64_C(0x5678000000000000)|modifier};
                assert((int32_t)bbcpu_call(target,4,args)==0);
                assert(!memcmp(out,expected,sizeof(out)));
                assert(!memcmp(registers+1,r,sizeof(r)));
            }
        }
    }
    const uint64_t invalid[][4]={{0,25,1,0},{1,24,1,0},{1,0,1,0},
        {1,25,0,0},{1,25,1,1},{1,25,1,UINT32_C(0x80000000)}};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i)
        assert((int32_t)bbcpu_call(target,4,invalid[i])==-1);
    for(unsigned bit=0;bit<32;++bit) {
        if(bit>=6 && bit<=9) continue;
        memset(out,0xa5,sizeof(out));
        uint64_t args[]={(uintptr_t)(out+1),25,(uintptr_t)(registers+1),UINT32_C(1)<<bit};
        assert((int32_t)bbcpu_call(target,4,args)==-1);
        for(unsigned i=0;i<sizeof(out);++i) assert(out[i]==0xa5);
    }
    r[1]=1;memcpy(registers+1,r,sizeof(r));memset(out,0xa5,sizeof(out));
    uint64_t args[]={(uintptr_t)(out+1),25,(uintptr_t)(registers+1),0};
    assert((int32_t)bbcpu_call(target,4,args)==-1);
    for(unsigned i=0;i<sizeof(out);++i) assert(out[i]==0xa5);
    unsigned char *bounded=runtime_low_map(32768,PROT_READ|PROT_WRITE);assert(bounded);
    assert(!mprotect(bounded+16384,16384,PROT_NONE));
    /* Bad high address is rejected after eight bytes, before remaining regs. */
    uint32_t bad[2]={0,1};memcpy(bounded+16376,bad,sizeof(bad));
    args[2]=(uintptr_t)(bounded+16376);
    assert((int32_t)bbcpu_call(target,4,args)==-1);
    /* Valid inputs need exactly seven dwords. */
    r[1]=0;memcpy(bounded+16356,r,sizeof(r));args[2]=(uintptr_t)(bounded+16356);
    assert((int32_t)bbcpu_call(target,4,args)==0);
    assert(!mprotect(bounded+16384,16384,PROT_READ|PROT_WRITE));
    assert(*runtime_errno()==17);
}
/* Synthetic base-PS4 provider: packet semantics, not hardware initialization. */
static uint32_t dispatch_init(uint32_t *out,uint32_t size) {
    if(size<256) return 0;
    const uint32_t packet[]={UINT32_C(0xc0017602),0x216,UINT32_MAX,
        UINT32_C(0xc0017602),0x217,UINT32_MAX,
        UINT32_C(0xc0017602),0x215,0x170,
        UINT32_C(0xc0055800),UINT32_C(0x28000000),0,0,0,0,10,
        UINT32_C(0xc0ee1000),0};
    memcpy(out,packet,sizeof(packet));return 256;
}
static void check_dispatch_packets(uintptr_t target) {
    unsigned char output[1152],expected[1152];*runtime_errno()=17;
    for(unsigned size=0;size<256;++size) {
        const uint64_t args[]={size&1 ? 0 : 1,UINT64_C(0x1234000000000000)|size};
        assert((uint32_t)bbcpu_call(target,2,args)==0);
    }
    const uint32_t sizes[]={256,257,1024,UINT32_MAX};
    for(unsigned fill=0;fill<3;++fill) {
        for(unsigned i=0;i<sizeof(output);++i)
            expected[i]=fill==0 ? 0 : fill==1 ? 0xa5 : (unsigned char)(i*37+11);
        uint32_t packets[]={UINT32_C(0xc0017602),0x216,UINT32_MAX,
            UINT32_C(0xc0017602),0x217,UINT32_MAX,
            UINT32_C(0xc0017602),0x215,0x170,
            UINT32_C(0xc0055800),UINT32_C(0x28000000),0,0,0,0,10,
            UINT32_C(0xc0ee1000),0};
        for(unsigned j=0;j<4;++j) {
            memcpy(output,expected,sizeof(output));
            unsigned char wanted[sizeof(output)];memcpy(wanted,expected,sizeof(wanted));
            memcpy(wanted+1,packets,sizeof(packets));
            const uint64_t args[]={(uintptr_t)(output+1),UINT64_C(0x5678000000000000)|sizes[j],1,3};
            assert((uint32_t)bbcpu_call(target,4,args)==256);
            assert(!memcmp(output,wanted,sizeof(output)));
        }
    }
    assert(*runtime_errno()==17);
}
/* Shared with private original/native state comparisons; no GPU work. */
static void check_submit_permission(uintptr_t target,uint32_t *lock) {
    /* The provider lock lifecycle uses only clear/set states. */
    const uint32_t states[]={0,1,0,1,1,0};
    unsigned char ignored[64];memset(ignored,0xa5,sizeof(ignored));
    const uint64_t stale[]={1,UINT64_MAX,(uintptr_t)ignored,3,5,7};
    *runtime_errno()=17;
    for(unsigned i=0;i<sizeof(states)/sizeof(*states);++i) {
        *lock=states[i];
        for(unsigned repeat=0;repeat<3;++repeat) {
            assert((int32_t)bbcpu_call(target,6,stale)==(states[i]==0));
            assert(*lock==states[i]);
            for(unsigned j=0;j<sizeof(ignored);++j) assert(ignored[j]==0xa5);
            assert(*runtime_errno()==17);
        }
    }
}
static uint32_t doorbell_queue,doorbell_offset;
static uint64_t doorbell_workload;
static unsigned doorbell_calls;
static void doorbell(uint32_t queue,uint32_t offset,uint64_t workload) {
    doorbell_queue=queue;doorbell_offset=offset;doorbell_workload=workload;++doorbell_calls;
}
static void check_doorbell_route(uintptr_t target) {
    const uint64_t workloads[]={0,1,UINT64_C(0x1234567887654321),UINT64_MAX};
    *runtime_errno()=17;
    for(unsigned i=0;i<4;++i) {
        uint64_t args[]={UINT64_C(0x1234000000000001)+i,
            UINT64_C(0x5678000080000000)+i,workloads[i],1,3,5};
        unsigned previous=doorbell_calls;
        (void)bbcpu_call(target,6,args); /* A void export has no result contract. */
        assert(doorbell_calls==previous+1 && doorbell_queue==(uint32_t)args[0]);
        assert(doorbell_offset==(uint32_t)args[1] && doorbell_workload==workloads[i]);
        assert(*runtime_errno()==17);
    }
}
/* Queue zero is a no-op before renderer/queue access, in original and provider. */
static void check_zero_doorbell(uintptr_t target) {
    const uint64_t workloads[]={0,1,UINT64_C(0x1234567887654321),UINT64_MAX};
    *runtime_errno()=17;
    for(unsigned i=0;i<4;++i) {
        const uint64_t args[]={UINT64_C(0x1234000000000000),UINT64_C(0x56780000ffffffff),workloads[i]};
        (void)bbcpu_call(target,3,args);assert(*runtime_errno()==17);
    }
}
/* Batch packet model: original synthetic operands only, never shader/game data. */
static int32_t compute_packet_model(unsigned kind,void *out,uint32_t size,uint64_t address,
                                    uint32_t a,uint32_t b,uint32_t c) {
    const uint32_t sizes[]={7,8,4,14};
    if(!out || size!=sizes[kind]) return -1;
    if(kind==1 && (!address || (address&3))) return -1;
    uint32_t packet[14]={0};unsigned count=0;
    if(kind==0 || kind==1) {
        packet[0]=(kind==0 ? UINT32_C(0xc0011602) : UINT32_C(0xc0021602))|(a&1);
        packet[1]=(uint32_t)address;
        unsigned i=2;if(kind==1) packet[i++]=(uint32_t)(address>>32);
        packet[i++]=(a&0x18)|1;packet[i++]=UINT32_C(0xc0021000);packet[i++]=0;count=i;
    } else if(kind==2) {packet[0]=UINT32_C(0xc0021000);count=4;}
    else {
        unsigned memory=address>=UINT64_C(0x10000);
        packet[0]=UINT32_C(0xc0021000);packet[4]=UINT32_C(0xc0053c00);
        packet[5]=(memory<<4)|(b&7);
        packet[6]=(uint32_t)address & (memory ? ~UINT32_C(3) : UINT32_C(0xffff));
        packet[7]=(uint32_t)((address>>32)&65535);packet[8]=c;packet[9]=a;
        packet[10]=10;packet[11]=UINT32_C(0xc0011000);
        memcpy(out,packet,8);memcpy((unsigned char *)out+16,packet+4,36);return 0;
    }
    memcpy(out,packet,count*4);return 0;
}
static int32_t indirect_dispatch(uint32_t *out,uint32_t size,uint32_t address,uint32_t flags) {
    return compute_packet_model(0,out,size,address,flags,0,0);
}
static int32_t indirect_mec(uint32_t *out,uint32_t size,uint64_t address,uint32_t flags) {
    return compute_packet_model(1,out,size,address,flags,0,0);
}
static int32_t dingdong_marker(uint32_t *out,uint32_t size) {
    return compute_packet_model(2,out,size,0,0,0,0);
}
static void check_compute_batch(uintptr_t targets[4],int native_wait) {
    const uint32_t sizes[]={7,8,4,14};
    const uint64_t addresses[]={0,1,4,0xffff,0x10000,0x10003,
        UINT64_C(0x123456789abc),UINT64_C(0xffff000000000004),UINT64_MAX};
    unsigned char output[128],expected[128];*runtime_errno()=17;
    for(unsigned kind=0;kind<4;++kind) for(unsigned n=0;n<9;++n) for(unsigned flags=0;flags<32;++flags) {
        uint64_t address=addresses[n];
        /* Native HLE's high-address field is wider than retail: test representable
         * addresses there; the checked local adapter handles all original inputs. */
        if(native_wait && kind==3 && address>UINT64_C(0xffffffffffff)) continue;
        memset(expected,0xa5,sizeof(expected));memset(output,0xa5,sizeof(output));
        int32_t result=compute_packet_model(kind,expected+1,sizes[kind],kind==0 ? (uint32_t)address : address,
            kind==3 ? UINT32_C(0xfedcba98) : flags,flags,UINT32_C(0xabcdef01));
        uint64_t args[]={(uintptr_t)(output+1),UINT64_C(0x1234000000000000)|sizes[kind],address,
            UINT64_C(0x5678000000000000)|(kind==3 ? UINT32_C(0xfedcba98) : flags),
            UINT64_C(0x9abc000000000000)|flags,UINT64_C(0xdef00000abcdef01)};
        assert((int32_t)bbcpu_call(targets[kind],6,args)==result);
        assert(!memcmp(output,expected,sizeof(output)));
        for(unsigned delta=1;delta<=2;++delta) {
            memset(output,0xa5,sizeof(output));args[1]=sizes[kind]+delta;
            assert((int32_t)bbcpu_call(targets[kind],6,args)==-1);
            for(unsigned j=0;j<sizeof(output);++j) assert(output[j]==0xa5);
        }
        args[0]=0;args[1]=sizes[kind];
        assert((int32_t)bbcpu_call(targets[kind],6,args)==-1);
        assert(*runtime_errno()==17);
    }
}
static void check_plain_cs(uintptr_t target) {
    uint32_t regs[]={0x12345678,0,0xabcdef01,4,5,6,7};
    unsigned char out[128],expected[128];memset(out,0xa5,sizeof(out));memset(expected,0xa5,sizeof(expected));
    uint32_t packet[]={UINT32_C(0xc0027602),0x20c,regs[0],0,
        UINT32_C(0xc0027602),0x212,regs[2],regs[3],UINT32_C(0xc0037602),0x207,
        regs[4],regs[5],regs[6],UINT32_C(0xc00a1000),0};
    memcpy(expected+1,packet,sizeof(packet));
    uint64_t args[]={(uintptr_t)(out+1),UINT64_C(0x1234000000000019),(uintptr_t)regs,UINT64_MAX};
    assert((int32_t)bbcpu_call(target,4,args)==0 && !memcmp(out,expected,sizeof(out)));
    args[1]=24;args[2]=1;assert((int32_t)bbcpu_call(target,4,args)==-1);
    args[0]=0;args[1]=25;assert((int32_t)bbcpu_call(target,4,args)==-1);
    regs[1]=1;args[0]=(uintptr_t)(out+1);args[2]=(uintptr_t)regs;
    assert((int32_t)bbcpu_call(target,4,args)==-1);
}
static uintptr_t resolver(const char *name) {
    ++calls;
    if(!strcmp(name,"Z43vKp5k7r0#A#B")) return (uintptr_t)indirect_dispatch;
    if(!strcmp(name,"wED4ZXCFJT0#A#B")) return (uintptr_t)indirect_mec;
    if(!strcmp(name,"NfvOrNzy6sk#A#B")) return (uintptr_t)dingdong_marker;
    if(!strcmp(name,"byXlqupd8cE#A#B")) return missing_doorbell ? 0 : (uintptr_t)doorbell;
    if(!strcmp(name,"b08AgtPlHPg#A#B")) return missing_permission ? 0 : (uintptr_t)submits_allowed;
    if(!strcmp(name,"nF6bFRUBRAU#A#B")) return (uintptr_t)dispatch_init;
    if(!strcmp(name,"W1Etj-jlW7Y#A#B")) return (uintptr_t)marker_push;
    if(!strcmp(name,"7qZVNgEu+SY#A#B")) return (uintptr_t)marker_pop;
    if (missing_wait && !strcmp(name, "fzyMKs9kim0#0#F")) return 0;
    return (uintptr_t)marker;
}
int main(void) {
    bootstrap_initialize(1);
    assert(!bootstrap_resolve("Kx-h-nWQJ8A#A#B",0));
    assert(!bootstrap_resolve("nF6bFRUBRAU#A#B",0));
    assert(!bootstrap_resolve("b08AgtPlHPg#A#B",0));
    assert(!bootstrap_resolve("byXlqupd8cE#A#B",0));
    assert(!bootstrap_resolve("Z43vKp5k7r0#A#B",0));
    assert(!bootstrap_resolve("W1Etj-jlW7Y#A#B",0) && !bootstrap_resolve("7qZVNgEu+SY#A#B",0));
    const uintptr_t original = bootstrap_resolve("D0OdFMjp46I#0#F", 0);
    assert(original && !bootstrap_graphics_owns("D0OdFMjp46I#0#F"));
    assert(bootstrap_graphics_bind(NULL) == -1);
    missing_wait = 1;
    assert(bootstrap_graphics_bind(resolver) == -1);
    assert(bootstrap_resolve("D0OdFMjp46I#0#F", 0) == original);
    missing_wait = 0;
    missing_permission=1;assert(bootstrap_graphics_bind(resolver)==-1);missing_permission=0;
    missing_doorbell=1;assert(bootstrap_graphics_bind(resolver)==-1);missing_doorbell=0;
    int32_t (*create)(uintptr_t *, const char *) = (void *)original;
    int32_t (*destroy)(uintptr_t) = (void *)bootstrap_resolve("jpFjmgAC5AE#0#F", 0);
    uintptr_t handle = 0;
    assert(create(&handle, "routing fixture") == 0);
    assert(bootstrap_graphics_bind(resolver) == -1); /* Existing registry must not be replaced. */
    assert(destroy(handle) == 0);
    assert(bootstrap_graphics_bind(resolver) == 0);
    assert(bootstrap_graphics_bind(resolver) == -1);
    const char *owned[] = {"D0OdFMjp46I#0#F", "fzyMKs9kim0#0#F", "HXzjK9yI30k#G#H",
                           "29oKvKXzEZo#A#B", "bX5IbRvECXk#A#B"};
    for (size_t i = 0; i < sizeof(owned)/sizeof(*owned); ++i) {
        assert(bootstrap_graphics_owns(owned[i]));
        assert(bootstrap_resolve(owned[i], 0) == (uintptr_t)marker);
        assert(!bootstrap_resolve(owned[i], 1));
    }
    const char *excluded[] = {"Up36PTk687E#0#F", "D0OdFMjp46I#G#H", "29oKvKXzEZo#G#H",
        "unknown-nid#A#B", "DYhhWbJSeRg#G#H",
        "i6-sR91Wt-4#G#H", "ln33zjBrfjk#A#B", "ZFqKFl23aMc#A#B"};
    const int previous_calls = calls;
    for (size_t i = 0; i < sizeof(excluded)/sizeof(*excluded); ++i) {
        assert(!bootstrap_graphics_owns(excluded[i]));
        assert(!bootstrap_graphics_resolve(excluded[i]));
    }
    assert(calls == previous_calls);
    assert(bootstrap_graphics_owns("pv9CI5VC+R0#G#H"));
    assert(bootstrap_graphics_owns("kGVLc3htQE8#G#H"));
    assert(bootstrap_resolve("pv9CI5VC+R0#G#H", 0) != (uintptr_t)marker);
    assert(bootstrap_resolve("kGVLc3htQE8#G#H", 0) != (uintptr_t)marker);
    missing_wait = 1;
    assert(!bootstrap_resolve("fzyMKs9kim0#0#F", 0)); /* Never fall back to CPU queues. */
    const char *marker_names[]={"W1Etj-jlW7Y#A#B","7qZVNgEu+SY#A#B"};uintptr_t markers[2];
    for(unsigned i=0;i<2;++i) {
        assert(bootstrap_graphics_owns(marker_names[i]));
        assert(!bootstrap_resolve(marker_names[i],1));
        bootstrap_initialize(0);assert(!bootstrap_resolve(marker_names[i],0));bootstrap_initialize(1);
        markers[i]=marker_bridge(bootstrap_resolve(marker_names[i],0));
    }
    assert(!bootstrap_resolve("W1Etj-jlW7Y#G#H",0) && !bootstrap_resolve("7qZVNgEu+SY#G#H",0));
    check_marker_packets(markers[0],markers[1]);
    assert(bootstrap_graphics_owns("Kx-h-nWQJ8A#A#B"));
    assert(!bootstrap_resolve("Kx-h-nWQJ8A#G#H",0));
    assert(!bootstrap_resolve("Kx-h-nWQJ8A#A#B",1));
    bootstrap_initialize(0);assert(!bootstrap_resolve("Kx-h-nWQJ8A#A#B",0));bootstrap_initialize(1);
    assert(bootstrap_graphics_owns("nF6bFRUBRAU#A#B"));
    assert(!bootstrap_resolve("nF6bFRUBRAU#G#H",0));
    assert(!bootstrap_resolve("nF6bFRUBRAU#A#B",1));
    bootstrap_initialize(0);assert(!bootstrap_resolve("nF6bFRUBRAU#A#B",0));bootstrap_initialize(1);
    assert(bootstrap_graphics_owns("b08AgtPlHPg#A#B"));
    assert(!bootstrap_resolve("b08AgtPlHPg#G#H",0));
    assert(!bootstrap_resolve("b08AgtPlHPg#A#B",1));
    bootstrap_initialize(0);assert(!bootstrap_resolve("b08AgtPlHPg#A#B",0));bootstrap_initialize(1);
    assert(bootstrap_graphics_owns("byXlqupd8cE#A#B"));
    assert(!bootstrap_resolve("byXlqupd8cE#G#H",0));
    assert(!bootstrap_resolve("byXlqupd8cE#A#B",1));
    bootstrap_initialize(0);assert(!bootstrap_resolve("byXlqupd8cE#A#B",0));bootstrap_initialize(1);
    const char *batch[]={"Z43vKp5k7r0#A#B","wED4ZXCFJT0#A#B","NfvOrNzy6sk#A#B","ffrNQOshows#A#B","KXltnCwEJHQ#A#B"};
    uintptr_t batch_targets[5];
    for(unsigned i=0;i<5;++i) {
        assert(bootstrap_graphics_owns(batch[i]) && !bootstrap_resolve(batch[i],1));
        bootstrap_initialize(0);assert(!bootstrap_resolve(batch[i],0));bootstrap_initialize(1);
        batch_targets[i]=marker_bridge(bootstrap_resolve(batch[i],0));
    }
    assert(!bootstrap_resolve("Z43vKp5k7r0#G#H",0) && !bootstrap_resolve("ffrNQOshows#G#H",0));
    check_compute_batch(batch_targets,0);check_plain_cs(batch_targets[4]);
    puts("PASS: batch indirect/MEC dispatch, doorbell marker, compute wait/address masking and plain compute shader packet ABI, flags, padding, guards and errors");
    check_doorbell_route(marker_bridge(bootstrap_resolve("byXlqupd8cE#A#B",0)));
    check_zero_doorbell(marker_bridge(bootstrap_resolve("byXlqupd8cE#A#B",0)));
    assert(doorbell_queue==0);
    puts("PASS: scoped workload doorbell forwards 32-bit queue/offset and 64-bit workload through void guest ABI, preserves errno and rejects missing provider support");
    check_submit_permission(marker_bridge(bootstrap_resolve("b08AgtPlHPg#A#B",0)),&submit_lock);
    puts("PASS: scoped no-argument submit permission preserves allowed/blocked provider transitions, stale arguments, buffers/errno and atomic activation");
    check_dispatch_packets(marker_bridge(bootstrap_resolve("nF6bFRUBRAU#A#B",0)));
    puts("PASS: scoped compute default-state guest packets, register/barrier/NOP semantics, short-buffer rejection, 32-bit size, unaligned guards, untouched tail and errno");
    check_cs_packets(marker_bridge(bootstrap_resolve("Kx-h-nWQJ8A#A#B",0)));
    puts("PASS: scoped compute shader modifier packets, compute header bits, all 16 masks, 64 register patterns, 32-bit ABI, untouched NOP tail/guards/errors/errno");
    puts("PASS: scoped push/pop marker guest ABI, exact PM4 headers/payload/padding for 256 lengths, unaligned output guards, 32-bit sizes, invalid arguments and errno");
    puts("PASS: atomic graphics provider activation, strict scopes, queue ownership, ABI exclusions and no headless fallback");
    return 0;
}
