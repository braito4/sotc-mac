/* Explicit dialog unavailability and absent-instance status for the diagnostic.
 * Zero-argument ABI and CommonDialog errors follow shadPS4 commondialog.h
 * and msgdialog.cpp (GPL-2.0-or-later). Foundations: deadinside28/bbport,
 * bloodborne-native and shadPS4. Save-dialog status paths also checked against
 * supplied retail exports. No dialog UI or completed interaction is created.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t common_requests, message_requests;
static _Atomic size_t save_status_queries,save_update_queries;
static _Atomic size_t save_terminate_requests,save_close_requests,save_result_requests;
static _Atomic size_t save_ready_requests,save_progress_requests;
static int32_t save_terminate(void) {
    atomic_fetch_add_explicit(&save_terminate_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x80b80003);
}
static int32_t save_close(const void *parameter) {
    (void)parameter;atomic_fetch_add_explicit(&save_close_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x80b80003); /* Instance check precedes parameters. */
}
static int32_t save_get_result(void *result) {
    atomic_fetch_add_explicit(&save_result_requests,1,memory_order_relaxed);
    return (int32_t)(result ? UINT32_C(0x80b80003) : UINT32_C(0x80b8000d));
}
static int32_t save_ready(void) {
    atomic_fetch_add_explicit(&save_ready_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x80b80003); /* Error code, not Boolean readiness. */
}
static int32_t save_progress(uint32_t target,uint32_t value) {
    (void)target;(void)value;
    atomic_fetch_add_explicit(&save_progress_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x80b80003);
}
static int32_t save_status(void) {
    atomic_fetch_add_explicit(&save_status_queries,1,memory_order_relaxed);
    return 0; /* NONE: no save dialog exists, not a successful UI result. */
}
static int32_t save_update_status(void) {
    atomic_fetch_add_explicit(&save_update_queries,1,memory_order_relaxed);
    return 0; /* Retail also returns NONE when there is no instance. */
}
static int32_t common_initialize(void) {
    atomic_fetch_add_explicit(&common_requests, 1, memory_order_relaxed);
    return (int32_t)UINT32_C(0x80b8000f); /* NOT_SUPPORTED: no dialog provider. */
}
static int32_t message_initialize(void) {
    atomic_fetch_add_explicit(&message_requests, 1, memory_order_relaxed);
    return (int32_t)UINT32_C(0x80b80001); /* NOT_SYSTEM_INITIALIZED. */
}
uintptr_t bootstrap_dialogs_resolve(const char *name) {
    if (!strcmp(name, "YuH2FA7azqQ#v#f")) return (uintptr_t)save_terminate;
    if (!strcmp(name, "fH46Lag88XY#v#f")) return (uintptr_t)save_close;
    if (!strcmp(name, "yEiJ-qqr6Cg#v#f")) return (uintptr_t)save_get_result;
    if (!strcmp(name, "en7gNVnh878#v#f")) return (uintptr_t)save_ready;
    if (!strcmp(name, "hay1CfTmLyA#v#f")) return (uintptr_t)save_progress;
    if (!strcmp(name, "ERKzksauAJA#v#f")) return (uintptr_t)save_status;
    if (!strcmp(name, "KK3Bdg1RWK0#v#f")) return (uintptr_t)save_update_status;
    if (!strcmp(name, "uoUpLGNkygk#c#c")) return (uintptr_t)common_initialize;
    if (!strcmp(name, "lDqxaY1UbEo#g#g")) return (uintptr_t)message_initialize;
    return 0;
}
void bootstrap_dialogs_report(void) {
    printf("Bootstrap: save-dialog status queries=%zu, updates=%zu (NONE; no dialog instance)\n",
           atomic_load_explicit(&save_status_queries,memory_order_relaxed),
           atomic_load_explicit(&save_update_queries,memory_order_relaxed));
    printf("Bootstrap: unavailable save-dialog requests: terminate=%zu, close=%zu, result=%zu, ready=%zu, progress=%zu (not initialized; no dialog or save access)\n",
        atomic_load_explicit(&save_terminate_requests,memory_order_relaxed),
        atomic_load_explicit(&save_close_requests,memory_order_relaxed),
        atomic_load_explicit(&save_result_requests,memory_order_relaxed),
        atomic_load_explicit(&save_ready_requests,memory_order_relaxed),
        atomic_load_explicit(&save_progress_requests,memory_order_relaxed));
    printf("Bootstrap: unavailable dialog initialization requests: common=%zu, message=%zu\n",
           atomic_load_explicit(&common_requests, memory_order_relaxed),
           atomic_load_explicit(&message_requests, memory_order_relaxed));
}
