/* SotC additions to bloodborne-native/bloodborne-runtime memory backend
 * 3170535f59cadbe5ce9c38e6bb961ed5fd215194. Include the unchanged backend so
 * the extra query shares its actual physical blocks and lock; no parallel
 * allocation ledger or copy of the allocator is maintained.
 * Largest-free-range query adapted from AjncSystem/ShadowOfTheColossus-PC
 * 2250f328feafab953aac85e758e7bddb2e80e891/src/runtime_memory.c.
 * Foundations: deadinside28/bbport and shadPS4.
 */
#define runtime_memory_resolve sotc_upstream_memory_resolve
#include "../upstream/src/runtime_memory.c"
#undef runtime_memory_resolve

/* The PS4 ABI returns the largest aligned contiguous gap in [low, high),
 * not the sum of free bytes. The port's private extra pool is not visible.
 * Querying does not create a host backing object or allocate physical pages. */
static ABI int32_t sotc_direct_available(int64_t low, int64_t high, uint64_t alignment,
                                        int64_t *physical, uint64_t *size) {
    if (!alignment) alignment = PAGE;
    if (!physical || !size || low < 0 || high < 0 || !valid_alignment(alignment)) return INVALID;
    const uint64_t end = (uint64_t)high < GAME_POOL_SIZE ? (uint64_t)high : GAME_POOL_SIZE;
    uint64_t cursor = (uint64_t)low, best = 0, best_start = 0;
    read_lock();
    while (cursor < end) {
        uint64_t next = end, resume = end;
        for (int i = 0; i < LIMIT; ++i) {
            const Block b = blocks[i];
            if (!b.used || b.start + b.size <= cursor) continue;
            if (b.start <= cursor) { next = cursor; resume = b.start + b.size; break; }
            if (b.start < next) { next = b.start; resume = b.start + b.size; }
        }
        const uint64_t start = align_up(cursor, alignment);
        if (next > start && next - start > best) { best = next - start; best_start = start; }
        cursor = resume;
    }
    read_unlock();
    if (!best) return NO_MEMORY;
    *physical = (int64_t)best_start; *size = best;
    return 0;
}

static ABI int32_t sotc_flexible_available(uint64_t *size) {
    if (!size) return INVALID;
    read_lock();
    const uint64_t available = FLEXIBLE_SIZE - flexible_bytes;
    read_unlock();
    *size = available;
    return 0;
}

uintptr_t runtime_memory_resolve(const char *name) {
    if (!strcmp(name, "C0f7TJcbfac#p#J")) return (uintptr_t)sotc_direct_available;
    if (!strcmp(name, "aNz11fnnzi4#p#J")) return (uintptr_t)sotc_flexible_available;
    return sotc_upstream_memory_resolve(name);
}
