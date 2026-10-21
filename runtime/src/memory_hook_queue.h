/* Included after PendingHook in the checked private memory-backend copy.
 * The existing region write lock owns storage, count and capacity. Keep indices
 * stable across growth: direct_unmap rolls the count back after early notification.
 * Callback dispatch remains outside the region lock, as in the pinned backend.
 */
static PendingHook *pending;
static size_t pending_count, pending_capacity;

static void sotc_reserve_pending_hook(void) {
    if (pending_count < pending_capacity) return;
    const size_t maximum = SIZE_MAX / sizeof(*pending);
    if (pending_capacity >= maximum) {
        fputs("STOP: GPU notification storage exhausted\n", stderr);
        exit(21);
    }
    const size_t capacity = !pending_capacity ? 64 :
        pending_capacity > maximum / 2 ? maximum : pending_capacity * 2;
    PendingHook *grown = realloc(pending, capacity * sizeof(*pending));
    if (!grown) {
        fputs("STOP: GPU notification allocation failed\n", stderr);
        exit(21);
    }
    pending = grown;
    pending_capacity = capacity;
}
