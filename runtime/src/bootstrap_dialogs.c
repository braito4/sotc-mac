/* Explicit dialog unavailability for the CPU-only diagnostic.
 * Zero-argument ABI and CommonDialog errors follow shadPS4 commondialog.h
 * and msgdialog.cpp (GPL-2.0-or-later). Foundations: deadinside28/bbport,
 * bloodborne-native and shadPS4. This is an error provider, not a dialog UI.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t common_requests, message_requests;
static int32_t common_initialize(void) {
    atomic_fetch_add_explicit(&common_requests, 1, memory_order_relaxed);
    return (int32_t)UINT32_C(0x80b8000f); /* NOT_SUPPORTED: no dialog provider. */
}
static int32_t message_initialize(void) {
    atomic_fetch_add_explicit(&message_requests, 1, memory_order_relaxed);
    return (int32_t)UINT32_C(0x80b80001); /* NOT_SYSTEM_INITIALIZED. */
}
uintptr_t bootstrap_dialogs_resolve(const char *name) {
    if (!strcmp(name, "uoUpLGNkygk#c#c")) return (uintptr_t)common_initialize;
    if (!strcmp(name, "lDqxaY1UbEo#g#g")) return (uintptr_t)message_initialize;
    return 0;
}
void bootstrap_dialogs_report(void) {
    printf("Bootstrap: unavailable dialog initialization requests: common=%zu, message=%zu\n",
           atomic_load_explicit(&common_requests, memory_order_relaxed),
           atomic_load_explicit(&message_requests, memory_order_relaxed));
}
