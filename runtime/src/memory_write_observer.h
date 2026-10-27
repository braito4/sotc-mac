/* Original optional diagnostic observer, invoked outside the memory lock. */
#ifndef SOTC_MEMORY_WRITE_OBSERVER_H
#define SOTC_MEMORY_WRITE_OBSERVER_H
#include <stdatomic.h>
#include <stdint.h>
#include <errno.h>
typedef void (*SotcBackingWriteObserver)(uintptr_t,const void*,uint64_t,unsigned,int,uintptr_t);
static _Atomic(SotcBackingWriteObserver) sotc_backing_write_observer;
void sotc_memory_set_backing_write_observer(SotcBackingWriteObserver observer) {
    atomic_store_explicit(&sotc_backing_write_observer,observer,memory_order_release);
}
static void sotc_memory_observe_backing_write(uintptr_t address,const void* source,uint64_t size,
        unsigned after,int success,uintptr_t caller) {
    SotcBackingWriteObserver observer=atomic_load_explicit(&sotc_backing_write_observer,memory_order_acquire);
    if(observer){const int saved=errno;observer(address,source,size,after,success,caller);errno=saved;}
}
#endif
