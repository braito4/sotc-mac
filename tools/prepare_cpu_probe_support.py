"""Prepare a checked private JIT copy with scalar correction and probe boundaries.

The pinned source checkout is unchanged. Probes must be registered before guest
execution. An optional immutable range uses the existing interpreter dispatcher
for a controlled comparison; no guest opcode, operand or protection is changed.
"""
import hashlib
from pathlib import Path
import sys

PINNED_SHA256='6088b98b870f59beba0779c227af18c7a32016786624a037ded28cc01c142818'


def prepare(data):
    if hashlib.sha256(data).hexdigest() not in (PINNED_SHA256,
            '4cb1c681f98f0f2ec602883bf0e42381fa4f5f1de304d881f5bbeb4bf79510e3',
            '5eaf90b5d8ee20561b1f12decf8b37c52e945b6a8b944e5ce10a60d87b86af28'):
        raise ValueError('Unexpected pinned CPU JIT; refusing unchecked probe adaptation.')
    source=data.decode()
    # A 32-bit register destination clears its upper half even when the
    # masked shift count is zero. ARM64 W-register moves preserve NZCV.
    old='''    if (!count) return 1; /* no change, flags kept */'''
    new='''    if (!count) {
        if (size == 4 && dst->type == OP_REG)
            a64_mov(a, 0, G[dst->reg], G[dst->reg]);
        return 1; /* flags kept; a 32-bit destination still zero-extends */
    }'''
    if source.count(old)!=1:raise ValueError('Unexpected immediate shift zero-count path.')
    source=source.replace(old,new)
    old='''    a64_logic_imm(a, 0, 0, T5, G[RCX], 0, sf ? 6 : 5); /* count */
    uint32_t *skip = a64_here(a);'''
    new='''    a64_logic_imm(a, 0, 0, T5, G[RCX], 0, sf ? 6 : 5); /* count */
    if (size == 4 && dst->type == OP_REG)
        a64_mov(a, 0, G[dst->reg], G[dst->reg]);
    uint32_t *skip = a64_here(a);'''
    if source.count(old)!=1:raise ValueError('Unexpected CL shift zero-count path.')
    source=source.replace(old,new)
    old='''    for (uint32_t i = 0; i < block->count && !ended && !recorded; ++i) {
        const BbInsn *in = &block->insn[i];'''
    new='''    for (uint32_t i = 0; i < block->count && !ended && !recorded; ++i) {
        /* Diagnostic probes can name an interior instruction of a cached
         * decoded block. Return before translating it, preserving normal exit
         * state; the dispatcher then runs its unchanged bbcpu_step/probe path.
         * Registration must finish before execution, as in the original API. */
        if (i && bbcpu_probe_count && bbcpu_probe_at(rip)) break;
        const BbInsn *in = &block->insn[i];'''
    if source.count(old)!=1:raise ValueError('Unexpected JIT translation loop.')
    source=source.replace(old,new)
    anchor='static const uint8_t G[16] = '
    if source.count(anchor)!=1:raise ValueError('Unexpected JIT register map.')
    support='''#include "cpu_interp_region.h"
#include <time.h>
#include <stdatomic.h>
#include <unistd.h>
/* Optional post-store diagnostic hook supplied by the local runtime overlay.
 * It is emitted only for the existing opt-in shader-prefix trace. */
__attribute__((weak)) void sotc_cpu_shader_write_observe(uintptr_t address,uint64_t low,
        uint64_t high,uint64_t size,uint64_t rip,unsigned route) {
    (void)address;(void)low;(void)high;(void)size;(void)rip;(void)route;
}
static int sotc_cpu_shader_write_enabled(void) {
    static int enabled=-1;
    if(enabled<0) { const char *p=getenv("SOTC_SHADER_COPY_TRACE"); enabled=p&&p[0]=='1'&&sotc_cpu_shader_write_observe; }
    return enabled;
}
__attribute__((weak)) int sotc_cpu_interp_fault_guest_regs(BbGuestRegs *regs) {
    (void)regs;
    return 0; /* Scalar diagnostic overlay supplies the strong implementation. */
}
void sotc_cpu_region_dump(void);
static SotcCpuInterpRegion sotc_cpu_region;
static uint64_t sotc_cpu_region_steps;
enum { SOTC_CPU_REGION_THREADS=16, SOTC_CPU_REGION_TAIL=8 };
_Static_assert(ATOMIC_LLONG_LOCK_FREE==2, "Region snapshots require lock-free 64-bit atomics");
static _Atomic uint64_t sotc_cpu_region_dropped;
typedef struct {
    _Atomic uint64_t sequence, kind, rip, ns, rax, rcx, rdx, rsi, rdi;
} SotcCpuRegionEvent;
typedef struct {
    _Atomic uint64_t thread, next;
    SotcCpuRegionEvent events[SOTC_CPU_REGION_TAIL];
} SotcCpuRegionThread;
static SotcCpuRegionThread sotc_cpu_region_threads[SOTC_CPU_REGION_THREADS];
static _Thread_local int sotc_cpu_region_active;
static uint64_t sotc_cpu_region_now_ns(void) {
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC,&ts))return 0;
    return (uint64_t)ts.tv_sec*UINT64_C(1000000000)+(uint64_t)ts.tv_nsec;
}
static void sotc_cpu_region_boundary(const char *kind,uint64_t rip,const BbCpu *cpu) {
    const uint64_t tid=(uintptr_t)pthread_self();
    SotcCpuRegionThread *slot=NULL;
    for(unsigned i=0;i<SOTC_CPU_REGION_THREADS;++i) {
        uint64_t owner=atomic_load_explicit(&sotc_cpu_region_threads[i].thread,memory_order_seq_cst);
        if(owner==tid) { slot=&sotc_cpu_region_threads[i]; break; }
        if(!owner && atomic_compare_exchange_strong_explicit(&sotc_cpu_region_threads[i].thread,&owner,tid,
                memory_order_seq_cst,memory_order_seq_cst)) { slot=&sotc_cpu_region_threads[i]; break; }
    }
    if(!slot) { atomic_fetch_add(&sotc_cpu_region_dropped,1); return; } /* bounded thread table full; execution remains unchanged */
    const uint64_t seq=atomic_fetch_add_explicit(&slot->next,1,memory_order_seq_cst)+1;
    SotcCpuRegionEvent *e=&slot->events[(seq-1)%SOTC_CPU_REGION_TAIL];
    atomic_store_explicit(&e->sequence,0,memory_order_seq_cst);
    atomic_store_explicit(&e->kind,!strcmp(kind,"enter")?1:2,memory_order_seq_cst);
    atomic_store_explicit(&e->rip,rip-bb_image_base_for_reports(),memory_order_seq_cst);
    atomic_store_explicit(&e->ns,sotc_cpu_region_now_ns(),memory_order_seq_cst);
    atomic_store_explicit(&e->rax,cpu->r[RAX],memory_order_seq_cst);
    atomic_store_explicit(&e->rcx,cpu->r[RCX],memory_order_seq_cst);
    atomic_store_explicit(&e->rdx,cpu->r[RDX],memory_order_seq_cst);
    atomic_store_explicit(&e->rsi,cpu->r[RSI],memory_order_seq_cst);
    atomic_store_explicit(&e->rdi,cpu->r[RDI],memory_order_seq_cst);
    atomic_store_explicit(&e->sequence,seq,memory_order_seq_cst);
}
/* No stdio, allocation, mutex, guest reads or retries in termination context.
 * SC atomic payloads plus two sequence reads reject an overwritten snapshot. */
static size_t sotc_region_text(char *out,const char *text) {
    size_t n=0;while(*text)out[n++]=*text++;return n;
}
static size_t sotc_region_hex(char *out,const char *label,uint64_t value) {
    size_t n=sotc_region_text(out,label);out[n++]='0';out[n++]='x';
    int started=0;
    for(int shift=60;shift>=0;shift-=4) {
        unsigned digit=(unsigned)((value>>shift)&15);
        if(digit || started || !shift) {out[n++]="0123456789abcdef"[digit];started=1;}
    }
    return n;
}
void sotc_cpu_region_dump(void) {
    const uint64_t dropped=atomic_load_explicit(&sotc_cpu_region_dropped,memory_order_seq_cst);
    if(dropped) {
        char line[96];size_t n=sotc_region_hex(line,"CPU region tail dropped=",dropped);
        line[n++]='\\n';if(write(STDERR_FILENO,line,n)!=(ssize_t)n)return;
    }
    for(unsigned i=0;i<SOTC_CPU_REGION_THREADS;++i) {
        SotcCpuRegionThread *slot=&sotc_cpu_region_threads[i];
        const uint64_t tid=atomic_load_explicit(&slot->thread,memory_order_seq_cst);
        if(!tid)continue;
        const uint64_t end=atomic_load_explicit(&slot->next,memory_order_seq_cst);
        const uint64_t begin=end>SOTC_CPU_REGION_TAIL?end-SOTC_CPU_REGION_TAIL:0;
        for(uint64_t n=begin;n<end;++n) {
            SotcCpuRegionEvent *e=&slot->events[n%SOTC_CPU_REGION_TAIL];
            const uint64_t expected=n+1, a=atomic_load_explicit(&e->sequence,memory_order_seq_cst);
            if(a!=expected)continue;
            const uint64_t kind=atomic_load_explicit(&e->kind,memory_order_seq_cst);
            const uint64_t rip=atomic_load_explicit(&e->rip,memory_order_seq_cst);
            const uint64_t ns=atomic_load_explicit(&e->ns,memory_order_seq_cst);
            const uint64_t rax=atomic_load_explicit(&e->rax,memory_order_seq_cst);
            const uint64_t rcx=atomic_load_explicit(&e->rcx,memory_order_seq_cst);
            const uint64_t rdx=atomic_load_explicit(&e->rdx,memory_order_seq_cst);
            const uint64_t rsi=atomic_load_explicit(&e->rsi,memory_order_seq_cst);
            const uint64_t rdi=atomic_load_explicit(&e->rdi,memory_order_seq_cst);
            if(atomic_load_explicit(&e->sequence,memory_order_seq_cst)!=a)continue;
            char line[512];size_t used=0;
            used+=sotc_region_text(line+used,"CPU region tail:");
            used+=sotc_region_hex(line+used," thread=",tid);
            used+=sotc_region_hex(line+used," seq=",expected);
            used+=sotc_region_text(line+used,kind==1?" kind=enter":" kind=exit");
            used+=sotc_region_hex(line+used," offset=",rip);
            used+=sotc_region_hex(line+used," ns=",ns);
            used+=sotc_region_hex(line+used," rax=",rax);
            used+=sotc_region_hex(line+used," rcx=",rcx);
            used+=sotc_region_hex(line+used," rdx=",rdx);
            used+=sotc_region_hex(line+used," rsi=",rsi);
            used+=sotc_region_hex(line+used," rdi=",rdi);
            line[used++]='\\n';
            if(write(STDERR_FILENO,line,used)!=(ssize_t)used)return;

        }
    }
}
static void sotc_cpu_region_init(void) {
    static int ready;
    if(ready)return; /* callers hold jit_lock; selection is immutable */
    if(sotc_cpu_region_parse(getenv("SOTC_CPU_INTERPRET_RANGE"),&sotc_cpu_region)) {
        fputs("STOP_CPU_EXPERIMENT: invalid interpreter range\\n",stderr);
        _Exit(24);
    }
    ready=1;
    if(sotc_cpu_region.enabled)
        fprintf(stderr,"CPU region interpreter configured: begin=%#llx end=%#llx\\n",
            (unsigned long long)sotc_cpu_region.begin,(unsigned long long)sotc_cpu_region.end);
}
static int sotc_cpu_region_at(uint64_t rip) {
    return sotc_cpu_region_contains(&sotc_cpu_region,rip,bb_image_base_for_reports());
}
uint64_t sotc_cpu_region_step_count(void) {
    return __atomic_load_n(&sotc_cpu_region_steps,__ATOMIC_RELAXED);
}

'''
    source=source.replace(anchor,support+anchor)
    declaration='''static void sotc_emit_cpu_shader_write(Tx *,int,int,int,uint64_t,unsigned);
'''
    anchor='/* The misaligned accesses\' plain versions, after the block\'s code. */'
    if source.count(anchor)!=1:raise ValueError('Unexpected slow-access helper anchor.')
    source=source.replace(anchor,declaration+anchor)
    old='static void init_deny(void) {\n'
    if source.count(old)!=1:raise ValueError('Unexpected JIT deny initialization.')
    source=source.replace(old,old+'    sotc_cpu_region_init();\n')
    old='''        if (i && bbcpu_probe_count && bbcpu_probe_at(rip)) break;
        const BbInsn *in = &block->insn[i];'''
    new='''        if (i && bbcpu_probe_count && bbcpu_probe_at(rip)) break;
        /* A block beginning outside the range must not translate across it.
         * Selected instructions get no code/cache entry, so direct chaining
         * and indirect-cache hits cannot bypass the interpreter dispatcher. */
        if (sotc_cpu_region_at(rip)) break;
        const BbInsn *in = &block->insn[i];'''
    if source.count(old)!=1:raise ValueError('Unexpected diagnostic block boundary.')
    source=source.replace(old,new)
    old='''        BbBlock *block = (BbBlock *)bbcpu_block(cpu->rip);
        void *code = __atomic_load_n(&block->jit_code, __ATOMIC_ACQUIRE);'''
    new='''        BbBlock *block = (BbBlock *)bbcpu_block(cpu->rip);
        if (sotc_cpu_region_at(cpu->rip)) {
            cpu->jit_link=0;
            jit_to_x86(cpu);
            if(!sotc_cpu_region_active) {
                sotc_cpu_region_active=1;
                sotc_cpu_region_boundary("enter",cpu->rip,cpu);
            }
            if(__atomic_fetch_add(&sotc_cpu_region_steps,1,__ATOMIC_RELAXED)==0)
                fprintf(stderr,"CPU region interpreter entered: offset=%#llx\\n",
                    (unsigned long long)(cpu->rip-bb_image_base_for_reports()));
            cpu->rip=bbcpu_step(cpu,&block->insn[0]);
            if(!sotc_cpu_region_at(cpu->rip)) {
                sotc_cpu_region_boundary("exit",cpu->rip,cpu);
                sotc_cpu_region_active=0;
            }
            x86_to_jit(cpu);
            continue;
        }
        void *code = __atomic_load_n(&block->jit_code, __ATOMIC_ACQUIRE);'''
    if source.count(old)!=1:raise ValueError('Unexpected JIT dispatcher.')
    source=source.replace(old,new)
    old='''int bbcpu_fault_guest_regs(void *context, BbGuestRegs *regs) {
    const CodeRange *range = fault_range(context);
    if (!range) return 0;
    ucontext_t *uc = context;'''
    new='''int sotc_cpu_fault_context_kind(void *context) {
    if(fault_range(context))return 2; /* generated JIT code */
    BbGuestRegs regs;
    return sotc_cpu_interp_fault_guest_regs(&regs) ? 4 : 0;
}

int bbcpu_fault_guest_regs(void *context, BbGuestRegs *regs) {
    const CodeRange *range = fault_range(context);
    if (!range) return sotc_cpu_interp_fault_guest_regs(regs);
    ucontext_t *uc = context;'''
    if source.count(old)!=1:raise ValueError('Unexpected translated fault register helper.')
    source=source.replace(old,new)
    old='''int bbcpu_describe_fault(void *context, char *out, size_t size) {
    const CodeRange *range = fault_range(context);
    if (!range) return 0;
    bbcpu_watch_dump();'''
    new='''int bbcpu_describe_fault(void *context, char *out, size_t size) {
    const CodeRange *range = fault_range(context);
    if (!range) {
        BbGuestRegs regs;
        if(!sotc_cpu_interp_fault_guest_regs(&regs))return 0;
        const uint64_t *g=regs.r;
        snprintf(out,size,
                 "  interpreted guest instruction %#llx (guest offset %#llx)\\n"
                 "  rax=%#llx rcx=%#llx rdx=%#llx rbx=%#llx rsp=%#llx rbp=%#llx rsi=%#llx rdi=%#llx\\n"
                 "  r8=%#llx r9=%#llx r10=%#llx r11=%#llx r12=%#llx r13=%#llx r14=%#llx r15=%#llx\\n",
                 (unsigned long long)regs.rip,
                 (unsigned long long)(regs.rip-bb_image_base_for_reports()),
                 (unsigned long long)g[0],(unsigned long long)g[1],
                 (unsigned long long)g[2],(unsigned long long)g[3],
                 (unsigned long long)g[4],(unsigned long long)g[5],
                 (unsigned long long)g[6],(unsigned long long)g[7],
                 (unsigned long long)g[8],(unsigned long long)g[9],
                 (unsigned long long)g[10],(unsigned long long)g[11],
                 (unsigned long long)g[12],(unsigned long long)g[13],
                 (unsigned long long)g[14],(unsigned long long)g[15]);
        return 1;
    }
    bbcpu_watch_dump();'''
    if source.count(old)!=1:raise ValueError('Unexpected translated fault description helper.')
    source=source.replace(old,new)
    old='''static void load(Tx *t, int size, int rt, int addr) {
    if (plain_access || unordered()) { a64_ldr_uoff(&t->a, size_log2(size), rt, addr, 0); return; }
    ordered_access(t, 0, size, rt, addr);
}
static void store(Tx *t, int size, int rt, int addr) {
    if (plain_access || unordered()) { a64_str_uoff(&t->a, size_log2(size), rt, addr, 0); return; }
    ordered_access(t, 1, size, rt, addr);
}'''
    new='''static void load(Tx *t, int size, int rt, int addr) {
    if (plain_access || unordered()) { a64_ldr_uoff(&t->a, size_log2(size), rt, addr, 0); return; }
    ordered_access(t, 0, size, rt, addr);
}
static void sotc_emit_cpu_shader_write(Tx *t,int addr,int low,int high,uint64_t size,unsigned route) {
    if(!sotc_cpu_shader_write_enabled())return;
    A64 *a=&t->a;
    /* Post-store observation: keep caller-saved GPRs, NZCV, and LR intact. */
    a64_sub_imm(a,1,31,31,160,0);
    for(int r=0;r<18;++r)a64_str_uoff(a,3,r,31,(uint32_t)r*8);
    a64_mrs_nzcv(a,17);a64_str_uoff(a,3,17,31,144);a64_str_uoff(a,3,30,31,152);
    if(addr<18)a64_ldr_uoff(a,3,0,31,(uint32_t)addr*8);else a64_mov(a,1,0,addr);
    if(low<18)a64_ldr_uoff(a,3,1,31,(uint32_t)low*8);else a64_mov(a,1,1,low);
    if(high<18)a64_ldr_uoff(a,3,2,31,(uint32_t)high*8);else a64_mov(a,1,2,high);
    a64_mov_imm(a,3,size);a64_mov_imm(a,4,t->rip);a64_mov_imm(a,5,route);
    a64_mov_imm(a,16,(uintptr_t)sotc_cpu_shader_write_observe);a64_blr(a,16);
    for(int r=0;r<18;++r)a64_ldr_uoff(a,3,r,31,(uint32_t)r*8);
    a64_ldr_uoff(a,3,30,31,152);a64_ldr_uoff(a,3,17,31,144);a64_msr_nzcv(a,17);
    a64_add_imm(a,1,31,31,160,0);
}
static void store(Tx *t, int size, int rt, int addr) {
    if (plain_access || unordered()) {
        a64_str_uoff(&t->a, size_log2(size), rt, addr, 0);
        sotc_emit_cpu_shader_write(t,addr,rt,31,(uint64_t)size,1);return;
    }
    ordered_access(t, 1, size, rt, addr);
    sotc_emit_cpu_shader_write(t,addr,rt,31,(uint64_t)size,1);
}'''
    if source.count(old)!=1:raise ValueError('Unexpected JIT load/store emitters.')
    source=source.replace(old,new)
    old='''    if (vec_ordered()) a64_dmb_ish(&t->a); /* x86 store ordering */
    a64_str_v(&t->a, size, src, TA, 0);
}'''
    new='''    if (vec_ordered()) a64_dmb_ish(&t->a); /* x86 store ordering */
    a64_str_v(&t->a, size, src, TA, 0);
    if(sotc_cpu_shader_write_enabled()) {
        a64_umov(&t->a,8,0,src,0);a64_umov(&t->a,8,1,src,1);
        sotc_emit_cpu_shader_write(t,TA,0,1,(uint64_t)bytes,2);
    }
}'''
    if source.count(old)!=1:raise ValueError('Unexpected vector memory store emitter.')
    source=source.replace(old,new)
    old='''        if (s->store) {
            a64_dmb_ish(a);
            a64_str_uoff(a, s->lg, s->rt, s->addr, 0);
        } else {'''
    new='''        if (s->store) {
            a64_dmb_ish(a);
            a64_str_uoff(a, s->lg, s->rt, s->addr, 0);
            sotc_emit_cpu_shader_write(t,s->addr,s->rt,31,UINT64_C(1)<<s->lg,1);
        } else {'''
    if source.count(old)!=1:raise ValueError('Unexpected out-of-line aligned-store fallback.')
    source=source.replace(old,new)
    return '/* Private CPU build: zero-count shifts, probe boundaries and optional interpreter range. */\n'+source


if __name__=='__main__':
    Path(sys.argv[2]).write_text(prepare(Path(sys.argv[1]).read_bytes()))
