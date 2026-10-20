/* Synthetic test ABI shared by the ARM64 host and an x86-64 guest fixture. */
typedef unsigned long long FWord;
typedef struct Fixture {
    FWord return_fn, switch_fn, self_fn, run_fn, nested_fn;
    FWord self, other, first, resumed, switched, second, canary, local, thread;
    unsigned int mxcsr, initial_mxcsr, errors;
} Fixture;
typedef struct InitFixture {
    FWord target, fiber, name, entry, arg, stack, size, opt, flags, version;
} InitFixture;
