/* Explicit savedata unavailability for the graphics diagnostic.
 * One-pointer Initialize3 ABI checked against the supplied retail export;
 * INTERNAL error from shadPS4 save_data/savedata_error.h (GPL-2.0-or-later).
 * Foundations: AjncSystem, deadinside28/bbport, bloodborne-native and shadPS4.
 * No save paths, accounts, mounts, files or persistent state are accessed.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t initialization_requests;
static _Atomic size_t search_requests;
static _Atomic size_t mount_requests, mount2_requests, unmount_requests, backup_unmount_requests;
static _Atomic size_t info_requests, parameter_requests, icon_requests, delete_requests;
/* These exports check the initialized context before touching guest pointers.
 * Checked against the supplied retail module. Upstream mount wrappers have a
 * different null-check order; use the measured retail precedence here. Since
 * Initialize3 failed, no mount/result/metadata is created. */
static int32_t save_mount(const void *mount,void *result) {
    (void)mount;(void)result;
    atomic_fetch_add_explicit(&mount_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x809f0001);
}
static int32_t save_mount2(const void *mount,void *result) {
    (void)mount;(void)result;
    atomic_fetch_add_explicit(&mount2_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x809f0001);
}
static int32_t save_unmount(const void *point) {
    (void)point;
    atomic_fetch_add_explicit(&unmount_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x809f0001);
}
static int32_t save_unmount_backup(const void *point) {
    (void)point;
    atomic_fetch_add_explicit(&backup_unmount_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x809f0001);
}
static int32_t save_mount_info(const void *point,void *result) {
    (void)point;(void)result;
    atomic_fetch_add_explicit(&info_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x809f0001);
}
static int32_t save_set_param(const void *point,uint32_t type,const void *buffer,uint64_t size) {
    (void)point;(void)type;(void)buffer;(void)size;
    atomic_fetch_add_explicit(&parameter_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x809f0001);
}
static int32_t save_icon(const void *point,const void *icon) {
    (void)point;(void)icon;
    atomic_fetch_add_explicit(&icon_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x809f0001);
}
static int32_t save_delete(const void *param) {
    (void)param;
    atomic_fetch_add_explicit(&delete_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x809f0001);
}
static int32_t save_dir_search(const void *condition,void *result) {
    (void)condition;(void)result;
    atomic_fetch_add_explicit(&search_requests,1,memory_order_relaxed);
    /* Retail checks the initialized context before either input/output
     * pointer. Initialization has not succeeded in this diagnostic. Do not
     * search host files or fabricate an empty successful result. */
    return (int32_t)UINT32_C(0x809f0001); /* SaveData NOT_INITIALIZED. */
}
static int32_t save_initialize3(const void *param) {
    (void)param;
    atomic_fetch_add_explicit(&initialization_requests,1,memory_order_relaxed);
    /* Diagnostic policy: there is no savedata backend to initialize. The
     * optional parameter remains opaque; do not inspect it, register a save
     * context, create a substitute filesystem or claim initialized state. */
    return (int32_t)UINT32_C(0x809f000b); /* SaveData INTERNAL. */
}
uintptr_t bootstrap_savedata_resolve(const char *name) {
    if (!strcmp(name,"TywrFKCoLGY#u#d")) return (uintptr_t)save_initialize3;
    if (!strcmp(name,"dyIhnXq-0SM#u#d")) return (uintptr_t)save_dir_search;
    static const struct { const char *name; void *fn; } exports[]={
        {"32HQAQdwM2o#u#d",save_mount}, {"0z45PIH+SNI#u#d",save_mount2},
        {"BMR4F-Uek3E#u#d",save_unmount}, {"VwadwBBBJ80#u#d",save_unmount_backup},
        {"65VH0Qaaz6s#u#d",save_mount_info}, {"85zul--eGXs#u#d",save_set_param},
        {"c88Yy54Mx0w#u#d",save_icon}, {"S1GkePI17zQ#u#d",save_delete},
    };
    for (size_t i=0;i<sizeof(exports)/sizeof(*exports);++i)
        if (!strcmp(name,exports[i].name)) return (uintptr_t)exports[i].fn;
    return 0;
}
void bootstrap_savedata_report(void) {
    printf("Bootstrap: unavailable SaveData mount requests: mount=%zu, mount2=%zu, unmount=%zu, backup unmount=%zu, info=%zu, parameter=%zu, icon=%zu, delete=%zu (not initialized; outputs untouched; no save access)\n",
        atomic_load_explicit(&mount_requests,memory_order_relaxed),
        atomic_load_explicit(&mount2_requests,memory_order_relaxed),
        atomic_load_explicit(&unmount_requests,memory_order_relaxed),
        atomic_load_explicit(&backup_unmount_requests,memory_order_relaxed),
        atomic_load_explicit(&info_requests,memory_order_relaxed),
        atomic_load_explicit(&parameter_requests,memory_order_relaxed),
        atomic_load_explicit(&icon_requests,memory_order_relaxed),
        atomic_load_explicit(&delete_requests,memory_order_relaxed));
    printf("Bootstrap: unavailable SaveData directory searches=%zu (not initialized; outputs untouched)\n",
           atomic_load_explicit(&search_requests,memory_order_relaxed));
    printf("Bootstrap: unavailable SaveData Initialize3 requests=%zu (no save backend; no save access)\n",
           atomic_load_explicit(&initialization_requests,memory_order_relaxed));
}
