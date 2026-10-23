/* Original finite sample-count selection and explicit failure provenance.
 * Preserves the renderer's supported guest domain (1,2,4,8,16) and downward
 * fallback. Never invents a sample count when the device mask has no match. */
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
void assert_fail_impl();
namespace SotcSampleCounts {
enum class Error { None, GuestCount, NoFallback };
struct Choice { std::uint32_t count; Error error; };
constexpr Choice Select(std::uint32_t requested, std::uint32_t supported) {
    if (!requested || requested > 16 || (requested & (requested - 1)))
        return {0, Error::GuestCount};
    for (auto count = requested; count; count >>= 1)
        if (supported & count) return {count, Error::None};
    return {0, Error::NoFallback};
}
[[noreturn]] inline void Fail(const char* reason, std::uint32_t requested,
                            std::uint32_t supported, const char* origin) {
    std::fprintf(stderr, "GPU sample-count failure: reason=%s requested=%u supported=0x%x origin=%s; unsupported state, no replacement invented\n",
                 reason, requested, supported, origin ? origin : "unspecified");
    assert_fail_impl();
    std::abort(); // A returning assertion handler must not continue invalid work.
}
}
