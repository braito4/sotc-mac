/* Per-thread module TLS, adapted from the pinned bloodborne-runtime runtime.c
 * and shadPS4's FreeBSD-style TCB/DTV layout. GPL-2.0-or-later.
 * Foundations: deadinside28/bbport, bloodborne-native and shadPS4.
 */
#include "bootstrap_contracts.h"
#include "runtime.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MODULES = 8 };
typedef struct { const void *data; uint64_t filesz, memsz; int registered; } Template;
static Template templates[MODULES];
static pthread_mutex_t template_lock = PTHREAD_MUTEX_INITIALIZER;
static int frozen;
static _Atomic size_t live_blocks;
size_t bootstrap_tls_live_blocks(void) { return atomic_load(&live_blocks); }
static _Thread_local unsigned char *blocks[MODULES];
static _Thread_local uint64_t *thread_tcb, *dtv;
static _Noreturn void fail(const char *reason) {
    fprintf(stderr, "STOP: module TLS %s\n", reason); _Exit(23);
}
void bootstrap_set_module_tls(uint64_t module, const void *data, uint64_t filesz, uint64_t memsz) {
    if (module < 2 || module >= MODULES || filesz > memsz || !memsz || memsz > 1024 * 1024 || (filesz && !data))
        fail("template is invalid");
    pthread_mutex_lock(&template_lock);
    if (frozen || templates[module].registered) { pthread_mutex_unlock(&template_lock); fail("configuration is already published"); }
    templates[module] = (Template){data, filesz, memsz, 1};
    pthread_mutex_unlock(&template_lock);
}
void bootstrap_tls_freeze(void) {
    pthread_mutex_lock(&template_lock); frozen = 1; pthread_mutex_unlock(&template_lock);
}
void bootstrap_tls_attach(uint64_t *tcb, uint64_t static_size) {
    if (thread_tcb || !tcb) fail("thread attachment is invalid");
    dtv = bb_low_calloc(MODULES + 1, sizeof(*dtv));
    if (!dtv) fail("DTV allocation failed");
    thread_tcb = tcb; tcb[1] = (uintptr_t)dtv;
    dtv[0] = 1; dtv[1] = 1; dtv[2] = (uintptr_t)tcb - static_size;
    pthread_mutex_lock(&template_lock);
    for (uint64_t m = 2; m < MODULES; ++m) if (templates[m].registered) dtv[1] = m;
    pthread_mutex_unlock(&template_lock);
}
void *bootstrap_tls_get_addr(const uint64_t *index) {
    if (!index || index[0] < 2 || index[0] >= MODULES) fail("module index is invalid");
    const uint64_t module = index[0], offset = index[1];
    pthread_mutex_lock(&template_lock);
    const Template t = templates[module];
    pthread_mutex_unlock(&template_lock);
    if (!t.registered || offset >= t.memsz) fail("module or offset is unavailable");
    if (!blocks[module]) {
        blocks[module] = bb_low_calloc(1, (size_t)t.memsz);
        if (!blocks[module]) fail("allocation failed");
        atomic_fetch_add(&live_blocks, 1);
        if (t.filesz) memcpy(blocks[module], t.data, (size_t)t.filesz);
        if (dtv) { dtv[module + 1] = (uintptr_t)blocks[module]; if (dtv[1] < module) dtv[1] = module; }
    }
    return blocks[module] + offset;
}
void bootstrap_tls_cleanup(void) {
    for (unsigned m = 2; m < MODULES; ++m) {
        if (blocks[m]) { bb_low_free(blocks[m]); blocks[m] = NULL; atomic_fetch_sub(&live_blocks, 1); }
    }
    if (thread_tcb) thread_tcb[1] = 0;
    bb_low_free(dtv); dtv = NULL; thread_tcb = NULL;
}
