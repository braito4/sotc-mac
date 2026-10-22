// Original bounded failure provenance; observations stay in private diagnostics.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>

void assert_fail_impl();

namespace SotcShaderDecode {
struct Context {
    const char* phase{"unspecified"};
    std::uint64_t shader{};
    int stage{-1};
    const void* origin{};
};
inline thread_local Context current{};
class Scope {
    Context previous;
public:
    Scope(const char* phase, std::uint64_t shader, int stage, const void* origin)
        : previous(current) { current = {phase, shader, stage, origin}; }
    ~Scope() { current = previous; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
[[noreturn]] inline void Fail(const char* reason, std::uint64_t offset,
                            std::int64_t remaining, std::int64_t required,
                            bool has_word = false, std::uint32_t word = 0,
                            std::uint32_t opcode = 0, std::uint32_t encoding = 0) {
    // Never reread guest memory here, especially after a bounds failure.
    std::fprintf(stderr, "GPU shader decode: reason=%s phase=%s shader=%016llx "
                 "stage=%d origin=%p word_offset=%llu remaining_words=%lld "
                 "required_words=%lld raw_word_available=%d raw_word=%08x "
                 "opcode=%u encoding=%08x; CPU decode evidence, not GPU execution\n",
                 reason, current.phase, static_cast<unsigned long long>(current.shader),
                 current.stage, current.origin, static_cast<unsigned long long>(offset),
                 static_cast<long long>(remaining),
                 static_cast<long long>(required), int(has_word), word, opcode, encoding);
    std::fflush(stderr);
    assert_fail_impl();
    // An assertion handler must not permit an out-of-range read to continue.
    std::abort();
}
} // namespace SotcShaderDecode
