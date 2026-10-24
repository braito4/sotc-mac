// Original bounded failure provenance; observations stay in private diagnostics.
#pragma once
#include "gpu_failure_history.h"
#include "gpu_shader_provenance.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

void assert_fail_impl();

namespace SotcShaderDecode {
struct Context {
    const char* phase{"unspecified"};
    std::uint64_t shader{};
    int stage{-1};
    const void* origin{};
    std::int64_t supplied_words{-1};
    bool input_owned{};
    std::span<const std::uint32_t> owned_words{};
};
inline thread_local Context current{};
class Scope {
    Context previous;
public:
    Scope(const char* phase, std::uint64_t shader, int stage, const void* origin,
          std::int64_t supplied_words = -1)
        : previous(current) { current = {phase, shader, stage, origin, supplied_words, false, {}}; }
    ~Scope() { current = previous; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
inline void SaveOwnedEvidence(const char* reason, std::uint64_t offset) noexcept {
    // Only bytes owned by this active decode scope, never current.origin or
    // adjacent guest memory. Borrowed/fetch/copy paths remain explicitly absent.
    if (!current.input_owned) return;
    const char* directory=std::getenv("BB_GPU_USER_DIR");
    if (!directory) return;
    try {
        std::error_code error;
        const auto folder=std::filesystem::path(directory)/"shader-decode-failure";
        if (!std::filesystem::create_directory(folder,error) || error) return;
        constexpr std::size_t limit=16384;
        const auto center=std::size_t(std::min<std::uint64_t>(offset,current.owned_words.size()));
        const auto start=center>limit/2 ? center-limit/2 : 0;
        const auto count=std::min(limit,current.owned_words.size()-start);
        std::ofstream data(folder/"code.bin",std::ios::binary);
        if (count) data.write(reinterpret_cast<const char*>(current.owned_words.data()+start),count*sizeof(std::uint32_t));
        std::ofstream meta(folder/"context.txt");
        meta<<"reason="<<reason<<"\nphase="<<current.phase<<"\nstage="<<current.stage
            <<"\nshader="<<current.shader<<"\norigin="<<reinterpret_cast<std::uintptr_t>(current.origin)<<"\nword_offset="<<offset
            <<"\nsupplied_words="<<current.supplied_words<<"\nowned_words="<<current.owned_words.size()
            <<"\nsaved_start_word="<<start<<"\nsaved_words="<<count<<"\ninput_owned=1\n";
        if (SotcShaderProvenance::Enabled()) {
            if(auto* report=std::fopen((folder/"provenance.txt").c_str(),"w")) {
                SotcShaderProvenance::Get().ReportAddress(report,
                    reinterpret_cast<std::uintptr_t>(current.origin));
                std::fclose(report);
            }
        }
        if (data && meta) std::fprintf(stderr,"GPU shader decode evidence: %s\n",folder.c_str());
        else std::fputs("GPU shader decode evidence write failed\n",stderr);
    } catch (...) {
        std::fputs("GPU shader decode evidence unavailable; original rejection preserved\n",stderr);
    }
}
[[noreturn]] inline void Fail(const char* reason, std::uint64_t offset,
                            std::int64_t remaining, std::int64_t required,
                            bool has_word = false, std::uint32_t word = 0,
                            std::uint32_t opcode = 0, std::uint32_t encoding = 0) {
    // Never reread guest memory here, especially after a bounds failure.
    std::fprintf(stderr, "GPU shader decode: reason=%s phase=%s shader=%016llx "
                 "stage=%d origin=%p word_offset=%llu remaining_words=%lld "
                 "required_words=%lld raw_word_available=%d raw_word=%08x "
                 "opcode=%u encoding=%08x supplied_words=%lld input_owned=%d; "
                 "CPU decode evidence, not GPU execution\n",
                 reason, current.phase, static_cast<unsigned long long>(current.shader),
                 current.stage, current.origin, static_cast<unsigned long long>(offset),
                 static_cast<long long>(remaining),
                 static_cast<long long>(required), int(has_word), word, opcode, encoding,
                 static_cast<long long>(current.supplied_words), int(current.input_owned));
    std::fflush(stderr);
    // Retain already recorded descriptor/write/lookup evidence before the host
    // callback; previously only metadata-lookup failures reported this context.
    if (SotcShaderProvenance::Enabled())
        SotcShaderProvenance::Get().ReportAddress(stderr,
            reinterpret_cast<std::uintptr_t>(current.origin));
    SaveOwnedEvidence(reason,offset);
    Sotc::PreserveCpuFailureHistory();
    assert_fail_impl();
    // An assertion handler must not permit an out-of-range read to continue.
    std::abort();
}

// Own only the caller-supplied main program. No adjacent metadata/guest scans,
// global cache, retries or instruction repair. The initial read still requires
// valid stable caller storage; a memcpy cannot make concurrent writes atomic.
class OwnedCode {
    std::vector<std::uint32_t> words;
    std::span<const std::uint32_t> previous=current.owned_words;
public:
    explicit OwnedCode(std::span<const std::uint32_t> source) {
        // PS4 metadata has a 24-bit byte length and instructions are whole dwords.
        constexpr std::size_t max_words = ((1u << 24) - 1) / sizeof(std::uint32_t);
        if (source.size() > max_words) {
            Fail("invalid_program_size", 0, source.size(), max_words);
        }
        if (!source.empty()) words.assign(source.begin(), source.end());
        current.owned_words=words;
    }
    ~OwnedCode() { current.owned_words=previous; }
    OwnedCode(const OwnedCode&)=delete;
    OwnedCode& operator=(const OwnedCode&)=delete;
    std::span<const std::uint32_t> Words() const { return words; }
};
} // namespace SotcShaderDecode
