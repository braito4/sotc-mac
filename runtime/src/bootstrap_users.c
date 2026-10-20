/* Process-local, single-user console profile for the CPU diagnostic.
 * ABI layouts: OpenOrbis _types/user.h and UserService.h. Errors: shadPS4
 * userservice_error.h. Profile foundations: deadinside28/bbport,
 * bloodborne-native and AjncSystem's SotC runtime_services.c (GPL-2.0-or-later).
 * This registry is ephemeral: no host accounts, PSN or user storage are used.
 */
#include "bootstrap_contracts.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#define USER_ERROR(code) ((int32_t)(UINT32_C(0x80960000) | (code)))
enum { NOT_INITIALIZED = 2, ALREADY_INITIALIZED = 3, INVALID_ARGUMENT = 5,
       NO_EVENT = 7, NOT_LOGGED_IN = 9, BUFFER_TOO_SHORT = 10 };
static pthread_mutex_t users_lock = PTHREAD_MUTEX_INITIALIZER;
static int initialized, login_pending;
static uint32_t priority;
static size_t initializations, terminations, queries, login_events;
static const int32_t local_user = 1;
static const char local_name[] = "Wander";

static int32_t user_initialize(const uint32_t *params) {
    uint32_t requested = 0x2bc;
    /* Exactly one 32-bit priority field, with NULL selecting the normal
     * console priority. Validate before changing state, including on re-init.
     * There is no service worker here; priority is recorded, not scheduled. */
    if (params) memcpy(&requested, params, sizeof(requested));
    if (requested < 0x100 || requested > 0x2ff) return USER_ERROR(INVALID_ARGUMENT);
    pthread_mutex_lock(&users_lock);
    const int32_t result = initialized ? USER_ERROR(ALREADY_INITIALIZED) : 0;
    if (!result) {
        initialized = login_pending = 1; priority = requested; ++initializations;
    }
    pthread_mutex_unlock(&users_lock);
    return result;
}
static int32_t user_terminate(void) {
    pthread_mutex_lock(&users_lock);
    const int32_t result = initialized ? 0 : USER_ERROR(NOT_INITIALIZED);
    if (!result) {
        initialized = login_pending = 0; priority = 0; ++terminations;
    }
    pthread_mutex_unlock(&users_lock);
    return result;
}
static int32_t user_initial(int32_t *out) {
    pthread_mutex_lock(&users_lock); ++queries;
    const int32_t result = !initialized ? USER_ERROR(NOT_INITIALIZED)
                           : !out ? USER_ERROR(INVALID_ARGUMENT) : 0;
    if (!result) memcpy(out, &local_user, sizeof(local_user));
    pthread_mutex_unlock(&users_lock);
    return result;
}
static int32_t user_list(int32_t *out) {
    pthread_mutex_lock(&users_lock); ++queries;
    const int32_t result = !initialized ? USER_ERROR(NOT_INITIALIZED)
                           : !out ? USER_ERROR(INVALID_ARGUMENT) : 0;
    if (!result) {
        const int32_t ids[4] = {local_user, -1, -1, -1};
        memcpy(out, ids, sizeof(ids));
    }
    pthread_mutex_unlock(&users_lock);
    return result;
}
static int32_t user_name(int32_t id, char *out, uint64_t size) {
    pthread_mutex_lock(&users_lock); ++queries;
    const int32_t result = !initialized ? USER_ERROR(NOT_INITIALIZED)
                           : (!out || id == -1) ? USER_ERROR(INVALID_ARGUMENT)
                           : id != local_user ? USER_ERROR(NOT_LOGGED_IN)
                           : size < sizeof(local_name) ? USER_ERROR(BUFFER_TOO_SHORT) : 0;
    if (!result) memcpy(out, local_name, sizeof(local_name));
    pthread_mutex_unlock(&users_lock);
    return result;
}
static int32_t user_event(int32_t *out) {
    pthread_mutex_lock(&users_lock); ++queries;
    const int32_t result = !initialized ? USER_ERROR(NOT_INITIALIZED)
                           : !out ? USER_ERROR(INVALID_ARGUMENT)
                           : !login_pending ? USER_ERROR(NO_EVENT) : 0;
    if (!result) {
        const int32_t event[2] = {0, local_user}; /* LOGIN */
        memcpy(out, event, sizeof(event));
        login_pending = 0; ++login_events;
    }
    pthread_mutex_unlock(&users_lock);
    return result;
}
uintptr_t bootstrap_users_resolve(const char *name) {
    if (!strcmp(name, "j3YMu1MVNNo#I#J")) return (uintptr_t)user_initialize;
    if (!strcmp(name, "bwFjS+bX9mA#I#J")) return (uintptr_t)user_terminate;
    if (!strcmp(name, "CdWp0oHWGr0#I#J")) return (uintptr_t)user_initial;
    if (!strcmp(name, "fPhymKNvK-A#I#J")) return (uintptr_t)user_list;
    if (!strcmp(name, "1xxcMiGu2fo#I#J")) return (uintptr_t)user_name;
    if (!strcmp(name, "yH17Q6NWtVg#I#J")) return (uintptr_t)user_event;
    return 0;
}
void bootstrap_users_report(void) {
    pthread_mutex_lock(&users_lock);
    printf("Bootstrap: local user service initialized=%d, priority=%u, initializations=%zu, terminations=%zu, queries=%zu, login events=%zu\n",
           initialized, priority, initializations, terminations, queries, login_events);
    pthread_mutex_unlock(&users_lock);
}
