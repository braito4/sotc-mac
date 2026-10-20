/* DiscMap's installed-content/no-optical-bitmap profile, following
 * bloodborne-native runtime_services.c 3170535f59cadbe5ce9c38e6bb961ed5fd215194
 * and shadPS4's documented DiscMap queries/error namespace (GPL-2.0-or-later).
 * Foundations: deadinside28/bbport, bloodborne-native and shadPS4.
 * Queries report absent metadata; they never invent extent locations or write
 * successful result values. Opaque unsupported queries stay unresolved.
 */
#include "bootstrap_contracts.h"
#include <string.h>
#define NO_BITMAP ((int32_t)UINT32_C(0x81100004))
static int32_t package_size(int64_t flags, int32_t *first, int32_t *second) {
    (void)flags; (void)first; (void)second;
    return NO_BITMAP;
}
static int32_t on_hdd(const char *path, int64_t offset, int64_t bytes, int32_t *result) {
    (void)path; (void)offset; (void)bytes; (void)result;
    return NO_BITMAP;
}
static int32_t query_8a82(const char *path, int64_t offset, int64_t bytes,
                         int32_t *flags, int32_t *first, int32_t *second) {
    (void)path; (void)offset; (void)bytes; (void)flags; (void)first; (void)second;
    return NO_BITMAP;
}
uintptr_t bootstrap_discmap_resolve(const char *name) {
    if (!bootstrap_files_mounted()) return 0;
    if (!strcmp(name, "fl1eoDnwQ4s#libSceDiscMap")) return (uintptr_t)package_size;
    if (!strcmp(name, "lbQKqsERhtE#libSceDiscMap")) return (uintptr_t)on_hdd;
    if (!strcmp(name, "ioKMruft1ek#libSceDiscMap")) return (uintptr_t)query_8a82;
    return 0;
}
