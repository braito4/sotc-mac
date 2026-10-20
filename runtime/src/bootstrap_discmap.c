/* DiscMap's installed-content/no-optical-bitmap profile, following
 * bloodborne-native runtime_services.c 3170535f59cadbe5ce9c38e6bb961ed5fd215194
 * and shadPS4's documented DiscMap queries/error namespace (GPL-2.0-or-later).
 * Foundations: deadinside28/bbport, bloodborne-native and shadPS4.
 * Bitmap tables are not loaded by this host provider. Queries never invent
 * extent locations. Request-location validation and 64-bit failure outputs
 * are checked separately from the bitmap-availability queries.
 */
#include "bootstrap_contracts.h"
#include <string.h>
#define NO_BITMAP ((int32_t)UINT32_C(0x81100004))
#define INVALID_ARGUMENT ((int32_t)UINT32_C(0x81100001))
#define LOCATION_NOT_MAPPED ((int32_t)UINT32_C(0x81100002))
#define FILE_NOT_FOUND ((int32_t)UINT32_C(0x81100003))
static int32_t package_size(int64_t flags, uint64_t *first, uint64_t *second) {
    (void)flags; (void)first; (void)second;
    return NO_BITMAP;
}
static int32_t on_hdd(const char *path, int64_t offset, int64_t bytes, int32_t *result) {
    (void)path; (void)offset; (void)bytes; (void)result;
    return NO_BITMAP;
}
static int32_t query_8a82(const char *path, int64_t offset, int64_t bytes,
                         uint64_t *flags, uint64_t *first, uint64_t *second) {
    (void)path; (void)offset; (void)bytes; (void)flags; (void)first; (void)second;
    return NO_BITMAP;
}
/* This query validates and initializes its three 64-bit outputs even when
 * mapping tables are absent. A missing table entry is FILE_NOT_FOUND in the
 * DiscMap namespace; it says nothing about ordinary filesystem existence.
 * Invalid arguments leave all outputs unchanged. FIOS can then use its
 * ordinary file path without consuming invented physical placement data. */
static int32_t request_location(const char *path, int64_t offset, int64_t bytes,
                                 uint64_t *flags, uint64_t *first, uint64_t *count) {
    if (!path || !flags || !first || !count || offset < 0 || bytes <= 0)
        return INVALID_ARGUMENT;
    *flags = UINT64_MAX;
    *first = UINT64_MAX;
    *count = UINT64_MAX;
    if (strncmp(path, "/app0/", 6) && strncmp(path, "/app1/", 6))
        return LOCATION_NOT_MAPPED;
    return FILE_NOT_FOUND;
}
uintptr_t bootstrap_discmap_resolve(const char *name) {
    if (!bootstrap_files_mounted()) return 0;
    if (!strcmp(name, "fl1eoDnwQ4s#libSceDiscMap")) return (uintptr_t)package_size;
    if (!strcmp(name, "lbQKqsERhtE#libSceDiscMap")) return (uintptr_t)on_hdd;
    if (!strcmp(name, "ioKMruft1ek#libSceDiscMap")) return (uintptr_t)query_8a82;
    if (!strcmp(name, "fJgP+wqifno#libSceDiscMap")) return (uintptr_t)request_location;
    return 0;
}
