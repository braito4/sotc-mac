/* Synthetic notification bursts and actual shared aliases; no GPU or game.
 * Include the checked memory adapter to inspect its private queue and lock. */
#include <stdlib.h>
#include "runtime.h"
static int fail_queue_allocation;
static void *test_realloc(void *, size_t);
#undef realloc
#define realloc test_realloc
#include "runtime_memory_arm64.c"
#undef realloc
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include <assert.h>

static void *test_realloc(void *pointer, size_t size) {
    return fail_queue_allocation ? NULL : bb_low_realloc(pointer, size);
}
static PendingHook expected[10000];
static size_t expected_count, delivered;
static unsigned backing_observed,notifications_observed;
static uintptr_t backing_target;
static void observe_backing(uintptr_t address,const void* source,uint64_t bytes,unsigned after,int ok,uintptr_t caller) {
    assert(!exclusive_depth && bytes==8 && caller);
    assert(!pthread_rwlock_trywrlock(&lock));pthread_rwlock_unlock(&lock);
    assert(runtime_memory_is_mapped(backing_target,8)); /* Real query reentry. */
    if(after==2){assert(!source&&!ok);++notifications_observed;errno=EDOM;return;}
    assert(source);
    assert(after==(backing_observed%2));
    if(after)assert(ok==(address==backing_target));
    ++backing_observed;errno=EDOM;
}
static void record(int kind, uintptr_t address, uint64_t size) {
    assert(!exclusive_depth); /* Never call the GPU while holding the region lock. */
    assert(delivered < expected_count);
    PendingHook e = expected[delivered++];
    assert(e.kind == kind && e.address == address && e.size == size);
    /* Real callback reentry into the region lock must remain possible. */
    read_lock(); read_unlock();
}
static void mapped(uintptr_t address, uint64_t size) { record(HOOK_MAP, address, size); }
static void unmapped(uintptr_t address, uint64_t size) { record(HOOK_UNMAP, address, size); }
static void invalidated(uintptr_t address, uint64_t size) { record(HOOK_INVALIDATE, address, size); }
static uintptr_t bridge(uintptr_t target) {
    unsigned char *code = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(code);
    code[0]=0x48; code[1]=0xb8; memcpy(code+2, &target, 8); code[10]=0xff; code[11]=0xe0;
    bbcpu_add_guest_code((uintptr_t)code, 16384); return (uintptr_t)code;
}
int main(int argc, char **argv) {
    if (argc == 2) {
        hook_map = mapped;
        if (!strcmp(argv[1], "--allocation-failure")) fail_queue_allocation = 1;
        else if (!strcmp(argv[1], "--capacity-failure"))
            pending_count = pending_capacity = SIZE_MAX / sizeof(*pending);
        else return 2;
        write_lock(); queue_hook(HOOK_MAP, PAGE, PAGE);
        return 3; /* Failure must stop explicitly, never drop the event. */
    }
    bootstrap_initialize(1);
    int64_t physical;
    assert(!direct_allocate(0, GAME_POOL_SIZE, PAGE, PAGE, 3, &physical));
    /* Registering a provider replays every live mapping in one locked batch.
     * Releasing one physical page later unmaps all 257 aliases in one batch. */
    for (unsigned i=0; i<257; ++i) {
        void *address = NULL;
        assert(!direct_map(&address, PAGE, 0x33, 0, physical, PAGE));
    }
    for (size_t i=0; i<vma_count; ++i) if (vmas[i].kind == KIND_DIRECT)
        expected[expected_count++] = (PendingHook){HOOK_MAP, vmas[i].start, vmas[i].end-vmas[i].start};
    assert(expected_count == 257 && !pending_count && !pending_capacity);
    runtime_memory_set_gpu_hooks(mapped, unmapped, invalidated);
    assert(delivered == expected_count && !pending_count && pending_capacity >= 257);
    delivered = 0;
    for (size_t i=0; i<expected_count; ++i) expected[i].kind = HOOK_UNMAP;
    const uint64_t arguments[]={(uint64_t)physical, PAGE};
    assert((int32_t)bbcpu_call(bridge((uintptr_t)direct_release), 2, arguments) == 0);
    assert(delivered == expected_count && !pending_count && !live_bytes);
    for (size_t i=0; i<vma_count; ++i) assert(vmas[i].kind != KIND_DIRECT);

    /* Mixed kinds, many growth boundaries, FIFO, reuse and direct_unmap's
     * count-rollback contract; do not combine or discard adjacent events. */
    for (unsigned round=0; round<3; ++round) {
        expected_count=round == 1 ? 65 : 8193; delivered=0;
        write_lock();
        for (size_t i=0; i<expected_count; ++i) {
            expected[i]=(PendingHook){(int)(i%3), (i+1)*PAGE, (i%7+1)*PAGE};
            queue_hook(expected[i].kind, expected[i].address, expected[i].size);
        }
        size_t before=pending_count;
        for (unsigned i=0; i<100; ++i) queue_hook(HOOK_UNMAP, PAGE, PAGE);
        pending_count=before;
        write_unlock(); flush_hooks();
        assert(delivered == expected_count && !pending_count);
    }
    hook_map=NULL; write_lock(); queue_hook(HOOK_MAP, PAGE, PAGE); write_unlock();
    assert(!pending_count);
    hook_map=NULL;hook_unmap=NULL;hook_invalidate=NULL;
    void* memory=NULL;assert(!map_flexible(&memory,PAGE,3,0));backing_target=(uintptr_t)memory;
    uint64_t input=UINT64_C(0x1122334455667788),output=0;
    sotc_memory_set_backing_write_observer(observe_backing);errno=ERANGE;
    assert(runtime_memory_write_backing(backing_target,&input,8));assert(errno==ERANGE);
    runtime_memory_read_backing(backing_target,&output,8);assert(output==input&&backing_observed==2);
    assert(!runtime_memory_write_backing(1,&input,8)&&backing_observed==4);
    runtime_memory_note_write(backing_target,8);assert(notifications_observed==1&&errno==ERANGE);
    sotc_memory_set_backing_write_observer(NULL);
    assert(runtime_memory_write_backing(backing_target,&input,8)&&backing_observed==4);
    puts("PASS: growable GPU notifications: 257 real aliases replayed/released through guest ABI, 8193 mixed events in FIFO order, unlocked/reentrant callbacks, rollback, reuse and absent hooks");
    puts("PASS: backing-write observers before/after success and failure, outside region locks, reentrant queries, unchanged bytes/results/errno and disabled control");
    return 0;
}
