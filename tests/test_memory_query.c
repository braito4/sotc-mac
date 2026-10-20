/* Synthetic physical-allocation states only; no host backing or game input.
 * Include the adapter in this test alone to populate its private block table.
 * A separate page bitmap supplies the expected largest free interval. */
#include "runtime_memory_arm64.c"
#include "bootstrap_contracts.h"
#include <assert.h>

static uint32_t random_state = UINT32_C(0xa37e42d1);
static uint32_t next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
int main(void) {
    bootstrap_initialize(1);
    typedef int32_t (*Query)(int64_t, int64_t, uint64_t, int64_t *, uint64_t *);
    Query query = (Query)bootstrap_resolve("C0f7TJcbfac#0#F", 0);
    int32_t (*flexible_query)(uint64_t *) = (void *)bootstrap_resolve("aNz11fnnzi4#0#F", 0);
    assert(query && !bootstrap_resolve("C0f7TJcbfac#1#u", 0));
    assert(!bootstrap_resolve("C0f7TJcbfac#0#F", 1));
    assert(flexible_query && !bootstrap_resolve("aNz11fnnzi4#1#u", 0));
    uint64_t flexible_size = 0;
    assert(flexible_query(NULL) == INVALID);
    assert(flexible_query(&flexible_size) == 0 && flexible_size == FLEXIBLE_SIZE);
    flexible_bytes = 3 * PAGE;
    assert(flexible_query(&flexible_size) == 0 && flexible_size == FLEXIBLE_SIZE - 3 * PAGE);
    flexible_bytes = 0;
    const uint64_t budget = GAME_POOL_SIZE;
    int64_t physical = -7;
    uint64_t size = 9;
    assert(query(0, INT64_MAX, 0, &physical, &size) == 0 && !physical && size == budget);
    assert(query(-1, budget, 0, &physical, &size) == INVALID);
    assert(query(0, -1, 0, &physical, &size) == INVALID);
    assert(query(0, budget, PAGE * 3, &physical, &size) == INVALID);
    assert(query(0, budget, PAGE / 2, &physical, &size) == INVALID);
    assert(query(0, budget, 0, NULL, &size) == INVALID);
    assert(query(0, budget, 0, &physical, NULL) == INVALID);
    physical = -7; size = 9;
    assert(query(budget, INT64_MAX, 0, &physical, &size) == NO_MEMORY && physical == -7 && size == 9);
    assert(query(PAGE, 0, 0, &physical, &size) == NO_MEMORY && physical == -7 && size == 9);
    blocks[0] = (Block){0, 131072, 3, 1};
    /* A real port-owned allocation beyond the game budget must stay hidden. */
    blocks[1] = (Block){budget + PAGE, PAGE, 3, 1};
    assert(query(0, INT64_MAX, 0, &physical, &size) == 0 && physical == 131072 && size == budget - 131072);
    memset(blocks, 0, sizeof(blocks));
    enum { PAGES = 128, STATES = 128, QUERIES = 32 };
    for (unsigned state = 0; state < STATES; ++state) {
        unsigned char used[PAGES] = {0};
        memset(blocks, 0, sizeof(blocks));
        /* Non-overlapping page allocations; deliberately unsorted block slots. */
        for (unsigned i = 0; i < 32; ++i) {
            unsigned p = next_random() % PAGES;
            if (!used[p]) { used[p] = 1; blocks[i] = (Block){p * PAGE, PAGE, 3, 1}; }
        }
        for (unsigned trial = 0; trial < QUERIES; ++trial) {
            uint64_t low = next_random() % (PAGES * PAGE);
            uint64_t high = next_random() % (PAGES * PAGE + 1);
            uint64_t alignment = PAGE << (next_random() % 5);
            uint64_t best = 0, best_start = 0;
            for (uint64_t start = 0; start < high; start += alignment) {
                if (start < low || used[start / PAGE]) continue;
                uint64_t limit = (start / PAGE + 1) * PAGE;
                while (limit < high && !used[limit / PAGE]) limit += PAGE;
                if (limit > high) limit = high;
                if (limit - start > best) { best = limit - start; best_start = start; }
            }
            physical = -7; size = 9;
            const int32_t result = query(low, high, alignment, &physical, &size);
            if (best) assert(result == 0 && physical == (int64_t)best_start && size == best);
            else assert(result == NO_MEMORY && physical == -7 && size == 9);
        }
    }
    puts("PASS: direct/flexible availability bounds/errors, reserved bytes and 4096 fragmented/aligned queries vs page-bitmap oracle");
}
