/* Included once by the checked scalar host-call bridge, used by JIT and interpreter.
 * The origin is captured BEFORE host code acquires runtime locks. No guest state
 * changes, decoder calls or unchecked stack loads. Nested calls restore TLS. */
#ifndef SOTC_CPU_IMPORT_ORIGIN_H
#define SOTC_CPU_IMPORT_ORIGIN_H
#include "cpu_import_origin_api.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
static _Thread_local SotcImportOrigin sotc_import_origin;
static int sotc_import_trace_enabled(void) {
    static int configured;
    int n=__atomic_load_n(&configured,__ATOMIC_ACQUIRE);
    if(!n){const char *s=getenv("SOTC_PAGE_HISTORY");n=s&&!strcmp(s,"1")?2:1;__atomic_store_n(&configured,n,__ATOMIC_RELEASE);}
    return n==2;
}
static SotcImportOrigin sotc_import_enter(uint64_t pc,uint64_t rsp,uint64_t stack_args,uint64_t target) {
    const int saved_errno=errno;
    SotcImportOrigin previous=sotc_import_origin,next={0};
    next.active=1;next.cpu_pc=pc;next.rsp=rsp;next.target=target;
    if(bbcpu_is_guest_code(pc))next.guest_instruction=pc;
    /* The pinned bridge passes rsp for an interpreter CALL (no return pushed),
     * and rsp+8 for a JIT host exit or interpreter tail JMP. Never confuse them. */
    if(rsp&&rsp<=UINT64_MAX-8&&stack_args==rsp+8) {
        uint64_t address=0;int read_ok=0;
#if defined(__APPLE__)
        mach_vm_size_t copied=0;
        read_ok=mach_vm_read_overwrite(mach_task_self(),(mach_vm_address_t)rsp,8,
            (mach_vm_address_t)(uintptr_t)&address,&copied)==KERN_SUCCESS&&copied==8;
#endif
        next.return_status=!read_ok?1:!bbcpu_is_guest_code(address)?2:3;
        if(next.return_status==3)next.guest_return=address;
    }
    sotc_import_origin=next;errno=saved_errno;return previous;
}
static void sotc_import_leave(SotcImportOrigin previous) {sotc_import_origin=previous;}
int sotc_cpu_import_origin_v1(uint64_t pc,uint64_t rsp,SotcImportOrigin *out) {
    memset(out,0,sizeof(*out));
    /* Also reject mismatched state after callbacks/nonlocal CPU exits. No saved
     * stack pointer is dereferenced by this query or by the memory observer. */
    if(!sotc_import_origin.active||sotc_import_origin.cpu_pc!=pc||sotc_import_origin.rsp!=rsp)return 0;
    *out=sotc_import_origin;return 1;
}
#endif
