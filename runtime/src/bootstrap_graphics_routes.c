/* Optional graphics routing for the pinned shadPS4/bloodborne-native provider.
 * Only explicit SotC scopes and reviewed entry points cross this boundary.
 * Unknown imports and unrepresentable color profiles stop explicitly.
 * The CPU-only diagnostic remains the default; no provider is replaced there.
 */
#include "bootstrap_contracts.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

typedef uintptr_t (*Resolver)(const char *);
static Resolver graphics_resolver;
static pthread_mutex_t display_lock = PTHREAD_MUTEX_INITIALIZER;
static _Atomic size_t margin_requests;
static int32_t live_display = -1; /* Pinned provider supports one main port. */
static int32_t (*graphics_open)(int32_t, int32_t, int32_t, const void *);
static int32_t (*graphics_close)(int32_t);
static int32_t display_open(int32_t user, int32_t bus, int32_t index, const void *params) {
    pthread_mutex_lock(&display_lock);
    const int32_t result = graphics_open(user, bus, index, params);
    if (result >= 0) live_display = result;
    pthread_mutex_unlock(&display_lock);
    return result;
}
static int32_t display_close(int32_t handle) {
    pthread_mutex_lock(&display_lock);
    int32_t result = (int32_t)UINT32_C(0x8029000b);
    if (handle == live_display && live_display >= 0) {
        result = graphics_close(handle);
        if (!result) live_display = -1;
    }
    pthread_mutex_unlock(&display_lock);
    return result;
}

static int32_t (*graphics_flip_status)(int32_t, void *);
static int32_t (*graphics_adjust_color)(int32_t, const void *);
static int32_t (*graphics_capability)(int32_t, void *);
static int32_t (*graphics_labels)(int32_t,uintptr_t *);
static int32_t validate_display(int32_t handle) {
    /* Pinned FlipStatus is 64 bytes. Lookup precedes accessing the caller's
     * settings/output, matching the supplied retail empty-handle behavior. */
    if (handle != live_display || live_display < 0) return (int32_t)UINT32_C(0x8029000b);
    uint64_t status[8] = {0};
    return graphics_flip_status(handle, status);
}
static int32_t retail_window_margins(int32_t handle,int32_t top,int32_t bottom) {
    (void)top;(void)bottom;
    atomic_fetch_add_explicit(&margin_requests,1,memory_order_relaxed);
    pthread_mutex_lock(&display_lock);
    const int32_t result=validate_display(handle);
    pthread_mutex_unlock(&display_lock);
    /* The pinned renderer has no window-margin implementation: never call
     * its unconditional-success stub. Retail rejects an otherwise valid main
     * port without margin capability before inspecting margin values. */
    return result ? result : (int32_t)UINT32_C(0x8029001c);
}
void bootstrap_graphics_report(void) {
    if (graphics_resolver) printf("Graphics: unsupported VideoOut window-margin requests=%zu (no margin capability; no display update)\n",
        atomic_load_explicit(&margin_requests,memory_order_relaxed));
}
static _Noreturn void unsupported_color(void) {
    fputs("STOP_GRAPHICS_COLOR: unsupported color size, unequal channel gamma or reserved data\n", stderr);
    fflush(NULL);
    _Exit(29);
}
static int32_t adjust_color_locked(int32_t handle, const void *settings, uint32_t size) {
    const int32_t result = validate_display(handle);
    if (result) return result;
    if (size != 12 && size != 16) unsupported_color();
    if (!settings) return graphics_adjust_color(handle, NULL);
    uint32_t channels[4] = {0};
    memcpy(channels, settings, size);
    /* The checked retail gamma helper repeats one gamma in RGB. Translate
     * that representable profile into the HLE's scalar gamma + reserved words.
     * Bit comparisons retain NaN payloads and avoid silently averaging RGB. */
    if (channels[0] != channels[1] || channels[0] != channels[2] || channels[3]) unsupported_color();
    struct { float gamma; uint32_t reserved[3]; } native = {0};
    memcpy(&native.gamma, channels, sizeof(native.gamma));
    return graphics_adjust_color(handle, &native);
}
static int32_t capability_locked(int32_t handle, void *output, uint64_t size) {
    const int32_t result = validate_display(handle);
    if (result) return result;
    /* Retail uses an exact eight-byte output and a full-width size, unlike
     * the upstream two-argument helper. Stage the output to preserve guards,
     * alignment and failure behavior without inventing capability bits. */
    if (!output || size != sizeof(uint64_t)) return (int32_t)UINT32_C(0x80290001);
    uint64_t capability = 0;
    const int32_t query = graphics_capability(handle, &capability);
    if (!query) memcpy(output, &capability, sizeof(capability));
    return query;
}

static int32_t retail_adjust_color(int32_t handle, const void *settings, uint32_t size) {
    pthread_mutex_lock(&display_lock);
    const int32_t result = adjust_color_locked(handle, settings, size);
    pthread_mutex_unlock(&display_lock);
    return result;
}
static int32_t retail_capability(int32_t handle, void *output, uint64_t size) {
    pthread_mutex_lock(&display_lock);
    const int32_t result = capability_locked(handle, output, size);
    pthread_mutex_unlock(&display_lock);
    return result;
}
/* Retail compute packet writer. The pinned provider defaults SetShReg to
 * graphics headers here; bit 1 must select compute for all three register sets.
 * Only 15 dwords are initialized; the remainder of the 25-dword reservation
 * belongs to the trailing NOP and must retain its previous contents. */
static int32_t retail_cs_shader(uint32_t *commands,uint32_t size,
                               const uint32_t *regs,uint32_t modifier) {
    if (!commands || size < 25 || !regs || (modifier & UINT32_C(0xfffffc3f))) return -1;
    uint32_t high;memcpy(&high,(const unsigned char *)regs+4,sizeof(high));
    if (high) return -1;
    uint32_t r[7];memcpy(r,regs,sizeof(r));
    const uint32_t packet[15]={
        UINT32_C(0xc0027602),0x20c,r[0],0,
        UINT32_C(0xc0027602),0x212,
        modifier ? (r[2] & UINT32_C(0xfffffc3f)) | modifier : r[2],r[3],
        UINT32_C(0xc0037602),0x207,r[4],r[5],r[6],
        UINT32_C(0xc00a1000),0
    };
    memcpy(commands,packet,sizeof(packet));return 0;
}
static int32_t retail_cs_shader_plain(uint32_t *commands,uint32_t size,const uint32_t *regs) {
    return retail_cs_shader(commands,size,regs,0);
}
/* Retail WAIT_REG_MEM uses a 48-bit address field; preserve NOP padding. */
static int32_t retail_compute_wait(uint32_t *commands,uint32_t size,uintptr_t address,
                                   uint32_t mask,uint32_t compare,uint32_t reference) {
    if (!commands || size!=14) return -1;
    uint32_t memory=address>UINT32_C(0xffff);
    uint32_t packet[13]={UINT32_C(0xc0021000),0,0,0,
        UINT32_C(0xc0053c00),(memory<<4)|(compare&7),
        (uint32_t)address & (memory ? UINT32_C(0xfffffffc) : UINT32_C(0xffff)),
        (uint32_t)(address>>32)&UINT32_C(0xffff),reference,mask,10,
        UINT32_C(0xc0011000),0};
    /* The first NOP's two padding dwords are not initialized by retail. */
    memcpy(commands,packet,8);memcpy((unsigned char *)commands+16,packet+4,36);return 0;
}
/* Read a real provider-owned label address; never fabricate a flip/fence. */
static int32_t retail_wait_flip(uint32_t *commands,uint32_t size,int32_t handle,uint32_t index) {
    if(size!=7 || !commands || index>=16) return -1;
    pthread_mutex_lock(&display_lock);
    int32_t result=(int32_t)UINT32_C(0x8029000b);uintptr_t labels=0;
    if(handle==live_display && live_display>=0) result=graphics_labels(handle,&labels);
    if(result==16) {
        uint64_t address=labels+(uint64_t)index*8;
        const uint32_t packet[]={UINT32_C(0xc0053c00),0x13,
            (uint32_t)address & UINT32_C(0xfffffffc),
            (uint32_t)(address>>32)&UINT32_C(0xffff),0,UINT32_MAX,10};
        memcpy(commands,packet,sizeof(packet));result=0;
    } else if(result>=0) result=-1;
    pthread_mutex_unlock(&display_lock);return result;
}
static int32_t retail_color_marker(uint32_t *commands,uint32_t size,const char *label,
                                     uint32_t color,uint32_t tag) {
    if(!commands || !label) return -1;
    size_t len=strlen(label);if(len>UINT32_MAX-16) return -1;
    uint32_t payload=(uint32_t)(((len+12)>>2)+((len+16)>>3)*2);
    if(size!=payload+2) return -1;
    const uint32_t header[]={UINT32_C(0xc0001000)|(payload<<16),tag};
    memcpy(commands,header,8);
    unsigned char *data=(unsigned char *)commands+8;
    memcpy(data,label,len+1);memcpy(data+len+1,&color,4);
    memset(data+len+5,0,payload*4-len-5);return 0;
}
static int32_t retail_push_color(uint32_t *c,uint32_t s,const char *label,uint32_t color) {
    return retail_color_marker(c,s,label,color,UINT32_C(0x6875000e));
}
static int32_t retail_set_color(uint32_t *c,uint32_t s,const char *label,uint32_t color) {
    return retail_color_marker(c,s,label,color,UINT32_C(0x6875000f));
}
typedef int32_t (*IndirectDraw)(uint32_t *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static IndirectDraw graphics_indirect_draw,graphics_index_indirect_draw;
static int32_t retail_indirect_draw(uint32_t *out,uint32_t size,uint32_t offset,uint32_t stage,
                                     uint32_t vertex,uint32_t instance,uint32_t flags) {
    /* Retail accepts VS/ES/LS only; the pinned simple encoders accept all stages. */
    if(stage!=2 && stage!=4 && stage!=6) return -1;
    return graphics_indirect_draw(out,size,offset,stage,vertex,instance,flags);
}
static int32_t retail_index_indirect_draw(uint32_t *out,uint32_t size,uint32_t offset,uint32_t stage,
                                           uint32_t vertex,uint32_t instance,uint32_t flags) {
    if(stage!=2 && stage!=4 && stage!=6) return -1;
    return graphics_index_indirect_draw(out,size,offset,stage,vertex,instance,flags);
}
typedef int32_t (*Submit)(uint32_t,uint32_t,const uint32_t *const *,const uint32_t *,
                           const uint32_t *const *,const uint32_t *);
typedef int32_t (*SubmitFlip)(uint32_t,uint32_t,const uint32_t *const *,const uint32_t *,
                               const uint32_t *const *,const uint32_t *,uint32_t,uint32_t,
                               uint32_t,int64_t);
static Submit graphics_submit;
static SubmitFlip graphics_submit_flip;
static int32_t submit_arguments(uint32_t count,const uint32_t *const *dcb,const uint32_t *sizes,
                                 const uint32_t *const *ccb,const uint32_t *csizes) {
    if(!count) return 0; /* Retail checks pointers only for nonempty submissions. */
    if(!dcb || !sizes || (csizes && !ccb)) return (int32_t)UINT32_C(0x80d11000);
    for(uint32_t i=0;i<count;++i) {
        uint32_t n;memcpy(&n,(const unsigned char *)sizes+i*4ULL,4);
        if(!n || n>UINT32_C(0x3ffffc)) return (int32_t)UINT32_C(0x80d11000);
        if(csizes) {
            memcpy(&n,(const unsigned char *)csizes+i*4ULL,4);
            if(n>UINT32_C(0x3ffffc)) return (int32_t)UINT32_C(0x80d11000);
        }
    }
    return 0;
}
static int32_t retail_submit(uint32_t workload,uint32_t count,const uint32_t *const *dcb,
                              const uint32_t *sizes,const uint32_t *const *ccb,const uint32_t *csizes) {
    int32_t error=submit_arguments(count,dcb,sizes,ccb,csizes);if(error) return error;
    /* Native HLE rejects null arrays even for count zero. It does not inspect
     * entries in that case; provide real empty storage while keeping its backend
     * initialization/submission path and actual return value. */
    const uint32_t *empty=NULL;const uint32_t zero=0;
    if(!count) return graphics_submit(workload,0,&empty,&zero,NULL,NULL);
    return graphics_submit(workload,count,dcb,sizes,ccb,csizes);
}
static int32_t retail_submit_plain(uint32_t count,const uint32_t *const *dcb,const uint32_t *sizes,
                                    const uint32_t *const *ccb,const uint32_t *csizes) {
    return retail_submit(count,count,dcb,sizes,ccb,csizes);
}
static int32_t retail_submit_flip(uint32_t workload,uint32_t count,const uint32_t *const *dcb,
                                   const uint32_t *sizes,const uint32_t *const *ccb,const uint32_t *csizes,
                                   uint32_t handle,uint32_t index,uint32_t mode,int64_t argument) {
    int32_t error=submit_arguments(count,dcb,sizes,ccb,csizes);if(error) return error;
    /* Neither flip implementation can select a final DCB from an empty list.
     * Reject it explicitly before the native count-minus-one access. */
    if(!count) return (int32_t)UINT32_C(0x80d11000);
    return graphics_submit_flip(workload,count,dcb,sizes,ccb,csizes,handle,index,mode,argument);
}
static int32_t retail_submit_flip_plain(uint32_t count,const uint32_t *const *dcb,const uint32_t *sizes,
                                         const uint32_t *const *ccb,const uint32_t *csizes,
                                         uint32_t handle,uint32_t index,uint32_t mode,int64_t argument) {
    return retail_submit_flip(count,count,dcb,sizes,ccb,csizes,handle,index,mode,argument);
}
static const char *const graphics_imports[] = {
    /* One provider owns creation, waits, user events and GPU notifications. */
    "D0OdFMjp46I#0#F", "jpFjmgAC5AE#0#F", "fzyMKs9kim0#0#F",
    "4R6-OvI2cEA#0#F", "WDszmSbWuDk#0#F", "F6e0kwo4cnk#0#F",
    "LJDwdSNTnDg#0#F", "vz+pg2zdopI#0#F", "mJ7aghmgvfc#0#F",
    "23CPPI1tyBY#0#F", "kwGyyjohI50#0#F",
    /* Display ports, buffers, flip/vblank events and status. Data preparation
     * stays in the checked adapter; suffixed color/capability calls use typed
     * wrappers around the upstream two-argument HLE functions. */
    "Up36PTk687E#G#H", "uquVH4-Du78#G#H", "w3BY+tAEiQY#G#H",
    "N5KDtkIjjJ4#G#H", "U46NwOiJpys#G#H", "HXzjK9yI30k#G#H",
    "Xru92wHJRmg#G#H", "CBiu4mCE1DA#G#H", "SbU3dwp80lQ#G#H",
    "6kPnj51T62Y#G#H", "j6RaAUlaLv0#G#H", "1FZBKy8HeNU#G#H",
    "U2JJtSqNKZI#G#H", "rWUTcKdkUzQ#G#H", "Mt4QHHkxkOc#G#H",
    "-Ozn0F1AFRg#G#H", "oNOQn3knW6s#G#H", "OcQybQejHEY#G#H",
    "N1bEoJ4SRw4#G#H",
    /* Real compute mapping, command encoding, submission and completion.
     * Retail resource registration retains its existing checked failure path. */
    /* Pure PM4 debug-scope packet writers, not successful submissions/draws. */
    "W1Etj-jlW7Y#A#B", "7qZVNgEu+SY#A#B", "jiItzS6+22g#A#B",
    /* Base-PS4 compute initialization: real register/barrier/NOP packets. */
    "nF6bFRUBRAU#A#B",
    /* Query the same provider lock used by submission/completion. */
    "b08AgtPlHPg#A#B",
    /* Notify the provider-owned mapped compute ring; void, three arguments. */
    "byXlqupd8cE#A#B",
    /* Reviewed indirect dispatch and doorbell-marker packet writers. */
    "Z43vKp5k7r0#A#B", "wED4ZXCFJT0#A#B", "NfvOrNzy6sk#A#B",
    /* Reviewed retail pixel/vertex updates and base-PS4 primitive grouping. */
    "5uFKckiJYRM#A#B", "mLVL7N7BVBg#A#B", "V31V01UiScY#A#B",
    "cFCp0NX8wf0#A#B", "MYRtYhojKdA#A#B",
    "jRcI8VcgTz4#A#B", "Ga6r7H6Y0RI#A#B",
    "4v+otIIdjqg#A#B", "ED9-Fjr8Ta4#A#B", "oYM+YzfCm2Y#A#B", "thbPcG7E7qk#A#B",
    "29oKvKXzEZo#A#B", "ArSg-TGinhk#A#B", "bX5IbRvECXk#A#B",
    "zwY0YV91TTI#A#B", "xbxNatawohc#A#B", "yvZ73uQUqrk#A#B",
    "Idffwf3yh8s#A#B", "QhnyReteJ1M#A#B", "0H2vBYbTLHI#A#B",
    "yb2cRhagD1I#A#B", "8lH54sfjfmU#A#B", "im2ZuItabu4#A#B",
    "0BzLGljcwBo#A#B", "HlTPoZ-oY7Y#A#B", "GGsn7jMTxw4#A#B",
    "KXltnCwEJHQ#A#B", "bQVd5YzCal0#A#B", "gAhCn6UiU4Y#A#B",
};
static int adapted_operation(const char *name) {
    return !strcmp(name, "4v+otIIdjqg#A#B") || !strcmp(name, "ED9-Fjr8Ta4#A#B") || !strcmp(name, "1qXLHIpROPE#A#B") || !strcmp(name, "aPIZJTXC+cU#A#B") || !strcmp(name, "aj3L-iaFmyk#A#B") || !strcmp(name, "jRcI8VcgTz4#A#B") || !strcmp(name, "Ga6r7H6Y0RI#A#B") || !strcmp(name, "zwY0YV91TTI#A#B") || !strcmp(name, "xbxNatawohc#A#B") || !strcmp(name, "ffrNQOshows#A#B") || !strcmp(name, "KXltnCwEJHQ#A#B") || !strcmp(name, "Kx-h-nWQJ8A#A#B") || !strcmp(name, "MTxxrOCeSig#G#H") || !strcmp(name, "pv9CI5VC+R0#G#H") || !strcmp(name, "kGVLc3htQE8#G#H");
}
int bootstrap_graphics_owns(const char *name) {
    if (!graphics_resolver) return 0;
    if (adapted_operation(name)) return 1;
    for (size_t i = 0; i < sizeof(graphics_imports)/sizeof(*graphics_imports); ++i)
        if (!strcmp(name, graphics_imports[i])) return 1;
    return 0;
}
int bootstrap_graphics_bind(Resolver resolver) {
    /* Publish before guest startup only. Never mix existing CPU queue handles
     * with the renderer's queue registry, or publish a partial provider. */
    if (!resolver || graphics_resolver || bootstrap_equeue_objects()) return -1;
    for (size_t i = 0; i < sizeof(graphics_imports)/sizeof(*graphics_imports); ++i)
        if (!resolver(graphics_imports[i])) return -1;
    const uintptr_t adjust = resolver("pv9CI5VC+R0#G#H");
    const uintptr_t capability = resolver("kGVLc3htQE8#G#H");
    const uintptr_t open = resolver("Up36PTk687E#G#H");
    const uintptr_t close = resolver("uquVH4-Du78#G#H");
    const uintptr_t status = resolver("SbU3dwp80lQ#G#H");
    const uintptr_t indirect=resolver("4v+otIIdjqg#A#B"),index_indirect=resolver("ED9-Fjr8Ta4#A#B");
    const uintptr_t labels=resolver("OcQybQejHEY#G#H");
    const uintptr_t submit=resolver("jRcI8VcgTz4#A#B"),submit_flip=resolver("Ga6r7H6Y0RI#A#B");
    if (!adjust || !capability || !open || !close || !status || !indirect || !index_indirect || !labels || !submit || !submit_flip) return -1;
    graphics_indirect_draw=(IndirectDraw)indirect;
    graphics_index_indirect_draw=(IndirectDraw)index_indirect;
    graphics_labels=(void *)labels;
    graphics_submit = (Submit)submit;
    graphics_submit_flip = (SubmitFlip)submit_flip;
    graphics_open = (void *)open;
    graphics_close = (void *)close;
    graphics_flip_status = (void *)status;
    graphics_adjust_color = (void *)adjust;
    graphics_capability = (void *)capability;
    graphics_resolver = resolver;
    return 0;
}
uintptr_t bootstrap_graphics_resolve(const char *name) {
    if (!graphics_resolver) return 0;
    if (!strcmp(name, "4v+otIIdjqg#A#B")) return (uintptr_t)retail_indirect_draw;
    if (!strcmp(name, "ED9-Fjr8Ta4#A#B")) return (uintptr_t)retail_index_indirect_draw;
    if (!strcmp(name, "1qXLHIpROPE#A#B")) return (uintptr_t)retail_wait_flip;
    if (!strcmp(name, "aPIZJTXC+cU#A#B")) return (uintptr_t)retail_push_color;
    if (!strcmp(name, "aj3L-iaFmyk#A#B")) return (uintptr_t)retail_set_color;
    if (!strcmp(name, "jRcI8VcgTz4#A#B")) return (uintptr_t)retail_submit;
    if (!strcmp(name, "Ga6r7H6Y0RI#A#B")) return (uintptr_t)retail_submit_flip;
    if (!strcmp(name, "zwY0YV91TTI#A#B")) return (uintptr_t)retail_submit_plain;
    if (!strcmp(name, "xbxNatawohc#A#B")) return (uintptr_t)retail_submit_flip_plain;
    if (!strcmp(name, "ffrNQOshows#A#B")) return (uintptr_t)retail_compute_wait;
    if (!strcmp(name, "KXltnCwEJHQ#A#B")) return (uintptr_t)retail_cs_shader_plain;
    if (!strcmp(name, "Kx-h-nWQJ8A#A#B")) return (uintptr_t)retail_cs_shader;
    if (!strcmp(name, "MTxxrOCeSig#G#H")) return (uintptr_t)retail_window_margins;
    if (!strcmp(name, "Up36PTk687E#G#H")) return (uintptr_t)display_open;
    if (!strcmp(name, "uquVH4-Du78#G#H")) return (uintptr_t)display_close;
    if (!strcmp(name, "pv9CI5VC+R0#G#H")) return (uintptr_t)retail_adjust_color;
    if (!strcmp(name, "kGVLc3htQE8#G#H")) return (uintptr_t)retail_capability;
    return bootstrap_graphics_owns(name) ? graphics_resolver(name) : 0;
}
