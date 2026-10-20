/* Synthetic provider routing/ownership checks; no renderer or game input. */
#include "bootstrap_contracts.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int missing_wait, calls;
static void marker(void) {}
static uintptr_t resolver(const char *name) {
    ++calls;
    if (missing_wait && !strcmp(name, "fzyMKs9kim0#0#F")) return 0;
    return (uintptr_t)marker;
}
int main(void) {
    bootstrap_initialize(1);
    const uintptr_t original = bootstrap_resolve("D0OdFMjp46I#0#F", 0);
    assert(original && !bootstrap_graphics_owns("D0OdFMjp46I#0#F"));
    assert(bootstrap_graphics_bind(NULL) == -1);
    missing_wait = 1;
    assert(bootstrap_graphics_bind(resolver) == -1);
    assert(bootstrap_resolve("D0OdFMjp46I#0#F", 0) == original);
    missing_wait = 0;
    int32_t (*create)(uintptr_t *, const char *) = (void *)original;
    int32_t (*destroy)(uintptr_t) = (void *)bootstrap_resolve("jpFjmgAC5AE#0#F", 0);
    uintptr_t handle = 0;
    assert(create(&handle, "routing fixture") == 0);
    assert(bootstrap_graphics_bind(resolver) == -1); /* Existing registry must not be replaced. */
    assert(destroy(handle) == 0);
    assert(bootstrap_graphics_bind(resolver) == 0);
    assert(bootstrap_graphics_bind(resolver) == -1);
    const char *owned[] = {"D0OdFMjp46I#0#F", "fzyMKs9kim0#0#F", "HXzjK9yI30k#G#H",
                           "29oKvKXzEZo#A#B", "zwY0YV91TTI#A#B"};
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
    puts("PASS: atomic graphics provider activation, strict scopes, queue ownership, ABI exclusions and no headless fallback");
}
