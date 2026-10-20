/* Guest thread-specific keys. ABI/errors and 255-key/four-pass profile adapted
 * from pinned bloodborne-runtime runtime_kernel.c (GPL-2.0-or-later).
 * Foundations: deadinside28/bbport, shadPS4 and the SotC reference.
 * Generations prevent deleted/reused slots inheriting another thread's value.
 * Registry locks are never held while executing guest destructors.
 */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { KEY_SLOTS = 256, DESTRUCTOR_PASSES = 4 };
typedef struct { uint64_t generation; uintptr_t destructor; int used; } Key;
typedef struct { uint64_t generation; void *value; } Value;
static pthread_mutex_t key_lock = PTHREAD_MUTEX_INITIALIZER;
static Key keys[KEY_SLOTS];
static _Thread_local Value values[KEY_SLOTS];
static _Thread_local int cleaning;
static uint64_t generation, created, deleted, sets, callbacks;
#define KEY_ERROR(n) ((int32_t)(UINT32_C(0x80020000) | (n)))

static int32_t key_create(void *out, uintptr_t destructor) {
    if (!out || (destructor && !bbcpu_is_guest(destructor))) return KEY_ERROR(22);
    pthread_mutex_lock(&key_lock);
    if (generation != UINT64_MAX) for (uint32_t i = 1; i < KEY_SLOTS; ++i) if (!keys[i].used) {
        keys[i] = (Key){.generation=++generation, .destructor=destructor, .used=1};
        ++created;
        memcpy(out, &i, sizeof(i));
        pthread_mutex_unlock(&key_lock);
        return 0;
    }
    pthread_mutex_unlock(&key_lock);
    return KEY_ERROR(35); /* Guest EAGAIN; do not leak Darwin's errno numbering. */
}
static int32_t key_delete(uint32_t key) {
    pthread_mutex_lock(&key_lock);
    int32_t result = KEY_ERROR(22);
    if (key && key < KEY_SLOTS && keys[key].used) {
        keys[key].used = 0; keys[key].destructor = 0; ++deleted; result = 0;
    }
    pthread_mutex_unlock(&key_lock);
    return result;
}
static int32_t key_set(uint32_t key, void *value) {
    pthread_mutex_lock(&key_lock);
    int32_t result = KEY_ERROR(22);
    if (key && key < KEY_SLOTS && keys[key].used) {
        values[key] = (Value){keys[key].generation, value}; ++sets; result = 0;
    }
    pthread_mutex_unlock(&key_lock);
    return result;
}
static void *key_get(uint32_t key) {
    pthread_mutex_lock(&key_lock);
    void *value = key && key < KEY_SLOTS && keys[key].used &&
                  values[key].generation == keys[key].generation ? values[key].value : NULL;
    pthread_mutex_unlock(&key_lock);
    return value;
}
static int32_t posix_create(void *out, uintptr_t destructor) { return key_create(out, destructor) & 0xffff; }
static int32_t posix_delete(uint32_t key) { return key_delete(key) & 0xffff; }
static int32_t posix_set(uint32_t key, void *value) { return key_set(key, value) & 0xffff; }

void bootstrap_keys_cleanup(void) {
    if (cleaning) {
        fputs("STOP: recursive thread exit from a key destructor is unsupported\n", stderr);
        _Exit(23);
    }
    cleaning = 1;
    for (unsigned pass = 0; pass < DESTRUCTOR_PASSES; ++pass) {
        int any = 0;
        for (unsigned i = 1; i < KEY_SLOTS; ++i) {
            pthread_mutex_lock(&key_lock);
            const Key key = keys[i];
            void *value = values[i].value;
            const int run = key.used && key.destructor && value && values[i].generation == key.generation;
            if (run) { values[i].value = NULL; ++callbacks; }
            pthread_mutex_unlock(&key_lock);
            if (run) {
                const uint64_t argument = (uintptr_t)value;
                any = 1;
                runtime_guest_call((void *)key.destructor, 1, &argument);
            }
        }
        if (!any) break;
    }
    memset(values, 0, sizeof(values));
    cleaning = 0;
}
uintptr_t bootstrap_keys_resolve(const char *name) {
    static const struct { const char *name; void *fn; } exports[] = {
        {"geDaqgH9lTg#p#J",key_create}, {"PrdHuuDekhY#p#J",key_delete},
        {"+BzXYkqYeLE#p#J",key_set}, {"eoht7mQOCmo#p#J",key_get},
        {"mqULNdimTn0#I#J",posix_create}, {"6BpEZuDT7YI#I#J",posix_delete},
        {"WrOLvHU0yQM#I#J",posix_set}, {"0-KXaS70xy4#I#J",key_get},
    };
    for (size_t i=0; i<sizeof(exports)/sizeof(*exports); ++i)
        if (!strcmp(name, exports[i].name)) return (uintptr_t)exports[i].fn;
    return 0;
}
void bootstrap_keys_report(void) {
    pthread_mutex_lock(&key_lock);
    printf("Runtime: thread keys created=%llu, deleted=%llu, sets=%llu, destructor calls=%llu\n",
           (unsigned long long)created, (unsigned long long)deleted,
           (unsigned long long)sets, (unsigned long long)callbacks);
    pthread_mutex_unlock(&key_lock);
}
