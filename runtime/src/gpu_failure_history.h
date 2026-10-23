// Original optional host history bridge. No driver access or guest memory reads.
#pragma once
#if defined(__APPLE__)
#include <dlfcn.h>
#else
extern "C" __attribute__((weak)) void sotc_cpu_field_trace_on_gpu_assert();
#endif
namespace Sotc {
inline void PreserveCpuFailureHistory() {
    // The host owns the once-only freeze/dump. Standalone renderers may omit it.
#if defined(__APPLE__)
    // Mach-O executables cannot leave a provider-less weak import unresolved.
    // This ordinary renderer stop path may query the loader; signal handlers
    // call their local lock-free snapshot directly and never enter this bridge.
    const auto hook = reinterpret_cast<void (*)()>(dlsym(RTLD_DEFAULT,"sotc_cpu_field_trace_on_gpu_assert"));
    if (hook) hook();
#else
    if (sotc_cpu_field_trace_on_gpu_assert) sotc_cpu_field_trace_on_gpu_assert();
#endif
}
} // namespace Sotc
