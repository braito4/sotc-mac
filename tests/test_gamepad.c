/* Original synthetic SDL3 gamepad provider: guest ABI and hotplug contracts.
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
static int present, attached, closes, frees, fail_open, output_ok=1;
static int16_t axes[6];
static bool buttons[21], fingers[2];
static float tx[2],ty[2];
static uint16_t low_motor,high_motor;
static uint8_t color[3];
static uint32_t duration;
static uint32_t device_ids[]={7,0};
static uint32_t *devices(int *count) { *count=present; return device_ids; }
static void free_devices(void *p) { assert(p==device_ids); ++frees; }
static void *open_device(uint32_t id) { assert(id==7); return fail_open ? NULL : (void *)7; }
static void close_device(void *p) { assert(p==(void *)7); ++closes; }
static bool connected(void *p) { assert(p==(void *)7); return attached; }
static const char *device_name(void *p) { assert(p==(void *)7); return "Synthetic DS4"; }
static int16_t device_axis(void *p,int a) { assert(p==(void *)7 && a>=0 && a<6); return axes[a]; }
static bool button(void *p,int b) { assert(p==(void *)7 && b>=0 && b<21); return buttons[b]; }
static bool touch(void *p,int pad,int i,bool *down,float *x,float *y,float *pressure) {
    assert(p==(void *)7 && pad==0 && i>=0 && i<2);
    *down=fingers[i]; *x=tx[i]; *y=ty[i]; *pressure=1; return true;
}
static bool rumble(void *p,uint16_t low,uint16_t high,uint32_t ms) {
    assert(p==(void *)7); low_motor=low; high_motor=high; duration=ms; return output_ok;
}
static bool led(void *p,uint8_t r,uint8_t g,uint8_t b) {
    assert(p==(void *)7); color[0]=r;color[1]=g;color[2]=b; return output_ok;
}
int main(void) {
    bootstrap_initialize(1);
    BootstrapKeyboard k={.initialized_subsystems=0x2020,.keyboard=keyboard,.scancode=scancode,.captured=captured};
    BootstrapGamepad g={devices,free_devices,open_device,close_device,connected,device_name,device_axis,button,touch,rumble,led};
    assert(bootstrap_pad_bind_gamepad(&g)==-1);
    assert(bootstrap_pad_bind(&k)==0);
    BootstrapGamepad incomplete=g; incomplete.free_devices=NULL;
    assert(bootstrap_pad_bind_gamepad(&incomplete)==-1);
    assert(bootstrap_pad_bind_gamepad(&g)==0 && bootstrap_pad_bind_gamepad(&g)==-1);
    uintptr_t init=bridge("hv1luiJrqQM#H#I"),open=bridge("xk0AcarP3V4#H#I"),close=bridge("6ncge5+l5Qs#H#I");
    uintptr_t read=bridge("YndgXqQVV7c#H#I"),vibrate=bridge("yFVnOdGxvZY#H#I"),light=bridge("RR4novUEENY#H#I");
    uintptr_t buffered=bridge("q1cHNfGycLI#H#I");
    assert(call(init,0,0,0,0)==0); int h=call(open,1,0,0,0); assert(h>0);
    uint8_t packet[244]; memset(packet,0xa5,sizeof(packet)); uint8_t *data=packet+1;
    keys[44]=true;
    assert(call(read,h,(uintptr_t)data,0,0)==0 && load(data,4)==0x4000 && frees==1);
    present=attached=1; fail_open=1;
    assert(call(read,h,(uintptr_t)data,0,0)==0 && load(data,4)==0x4000);
    fail_open=0; available=0; /* Gamepad reads do not depend on keyboard availability. */
    assert(call(read,h,(uintptr_t)data,0,0)==0 && load(data,4)==0 && data[4]==128 && data[104]==2);
    const uint32_t masks[]={0x4000,0x2000,0x8000,0x1000,0x100000,0,8,2,4,0x400,0x800,0x10,0x40,0x80,0x20,0,0,0,0,0,0x100000};
    for (int i=0;i<21;++i) { buttons[i]=true; assert(call(read,h,(uintptr_t)data,0,0)==0 && load(data,4)==masks[i]); buttons[i]=false; }
    for (int v=-32768;v<=32767;++v) {
        for(int i=0;i<6;++i) axes[i]=v;
        assert(call(read,h,(uintptr_t)data,0,0)==0);
        for(int i=0;i<4;++i) assert(data[4+i]==(v+32768)/256);
        unsigned t=v<=0 ? 0 : (unsigned)v*255/32767;
        assert(data[8]==t && data[9]==t && load(data,4)==(t>127 ? 0x300 : 0));
    }
    memset(axes,0,sizeof(axes));
    fingers[0]=fingers[1]=true;tx[0]=0.5f;ty[0]=1;tx[1]=-1;ty[1]=2;
    assert(call(read,h,(uintptr_t)data,0,0)==0 && data[52]==2 && load(data+60,2)==959 && load(data+62,2)==942 && load(data+68,2)==0 && load(data+70,2)==942);
    uint8_t first=data[64],second=data[72]; assert(first && second && first!=second && !load(data,4));
    fingers[0]=false;
    assert(call(read,h,(uintptr_t)data,0,0)==0 && data[52]==1 && data[64]==second);
    fingers[0]=true;
    assert(call(read,h,(uintptr_t)data,0,0)==0 && data[64]!=first && data[72]==second);
    capture=1; axes[0]=32767; buttons[0]=true;
    assert(call(read,h,(uintptr_t)data,0,0)==0 && !load(data,4) && data[4]==128 && data[52]==0);
    capture=0;
    uint8_t motors[]={17,99},rgb[]={255,44,13};
    assert(call(vibrate,h,(uintptr_t)motors,0,0)==0 && low_motor==17*257 && high_motor==99*257 && duration==1000);
    assert(call(light,h,(uintptr_t)rgb,0,0)==0 && !memcmp(color,rgb,3));
    output_ok=0;
    assert((uint32_t)call(vibrate,h,(uintptr_t)motors,0,0)==0x80920101);
    assert((uint32_t)call(light,h,(uintptr_t)rgb,0,0)==0x80920101);
    assert((uint32_t)call(vibrate,h,1,0,0)==0x80920001);
    assert((uint32_t)call(light,h,1,0,0)==0x80920001);
    rgb[0]=rgb[1]=rgb[2]=12; assert((uint32_t)call(light,h,(uintptr_t)rgb,0,0)==0x80920006);
    assert(call(buffered,h,(uintptr_t)data,2,0)==1);
    assert(packet[0]==0xa5); for(int i=121;i<244;++i) assert(packet[i]==0xa5);
    available=1; attached=present=0;
    assert(call(read,h,(uintptr_t)data,0,0)==0 && closes==1 && load(data,4)==0x4000 && data[104]==3);
    assert((uint32_t)call(vibrate,h,(uintptr_t)motors,0,0)==0x80920101);
    present=attached=output_ok=1;
    assert(call(read,h,(uintptr_t)data,0,0)==0 && data[104]==4);
    assert(call(close,h,0,0,0)==0 && closes==2 && !low_motor && !high_motor && !duration);
    assert((uint32_t)call(read,h,(uintptr_t)data,0,0)==0x80920003);
    bootstrap_pad_report();
    puts("PASS: physical gamepad guest ABI, full signed axis/trigger ranges, buttons, two touch identities, capture, LED/rumble results, failed opens, hotplug, keyboard fallback, close and exact output guards");
}
