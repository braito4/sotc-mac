/* Optional graphics routing for the pinned shadPS4/bloodborne-native provider.
 * Only explicit SotC scopes and reviewed entry points cross this boundary.
 * Unknown imports and incompatible color/capability operations remain unresolved.
 * The CPU-only diagnostic remains the default; no provider is replaced there.
 */
#include "bootstrap_contracts.h"
#include <string.h>

typedef uintptr_t (*Resolver)(const char *);
static Resolver graphics_resolver;
static const char *const graphics_imports[] = {
    /* One provider owns creation, waits, user events and GPU notifications. */
    "D0OdFMjp46I#0#F", "jpFjmgAC5AE#0#F", "fzyMKs9kim0#0#F",
    "4R6-OvI2cEA#0#F", "WDszmSbWuDk#0#F", "F6e0kwo4cnk#0#F",
    "LJDwdSNTnDg#0#F", "vz+pg2zdopI#0#F", "mJ7aghmgvfc#0#F",
    "23CPPI1tyBY#0#F", "kwGyyjohI50#0#F",
    /* Display ports, buffers, flip/vblank events and status. Data preparation
     * stays in the checked adapter; suffixed color/capability calls are not
     * compatible with the upstream two-argument HLE functions. */
    "Up36PTk687E#G#H", "uquVH4-Du78#G#H", "w3BY+tAEiQY#G#H",
    "N5KDtkIjjJ4#G#H", "U46NwOiJpys#G#H", "HXzjK9yI30k#G#H",
    "Xru92wHJRmg#G#H", "CBiu4mCE1DA#G#H", "SbU3dwp80lQ#G#H",
    "6kPnj51T62Y#G#H", "j6RaAUlaLv0#G#H", "1FZBKy8HeNU#G#H",
    "U2JJtSqNKZI#G#H", "rWUTcKdkUzQ#G#H", "Mt4QHHkxkOc#G#H",
    "-Ozn0F1AFRg#G#H", "oNOQn3knW6s#G#H", "OcQybQejHEY#G#H",
    "N1bEoJ4SRw4#G#H",
    /* Real compute mapping, command encoding, submission and completion.
     * Retail resource registration retains its existing checked failure path. */
    "29oKvKXzEZo#A#B", "ArSg-TGinhk#A#B", "bX5IbRvECXk#A#B",
    "zwY0YV91TTI#A#B", "xbxNatawohc#A#B", "yvZ73uQUqrk#A#B",
    "Idffwf3yh8s#A#B", "QhnyReteJ1M#A#B", "0H2vBYbTLHI#A#B",
    "yb2cRhagD1I#A#B", "8lH54sfjfmU#A#B", "im2ZuItabu4#A#B",
    "0BzLGljcwBo#A#B", "HlTPoZ-oY7Y#A#B", "GGsn7jMTxw4#A#B",
    "KXltnCwEJHQ#A#B", "bQVd5YzCal0#A#B", "gAhCn6UiU4Y#A#B",
};
static int incompatible_operation(const char *name) {
    return !strcmp(name, "pv9CI5VC+R0#G#H") || !strcmp(name, "kGVLc3htQE8#G#H");
}
int bootstrap_graphics_owns(const char *name) {
    if (!graphics_resolver) return 0;
    if (incompatible_operation(name)) return 1;
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
    graphics_resolver = resolver;
    return 0;
}
uintptr_t bootstrap_graphics_resolve(const char *name) {
    if (incompatible_operation(name)) return 0;
    return bootstrap_graphics_owns(name) ? graphics_resolver(name) : 0;
}
