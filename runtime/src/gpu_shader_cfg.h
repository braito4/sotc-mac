// Original CFG failure provenance. No guest reads, code dump or recovery.
#pragma once
#include "gpu_shader_decode.h"
#include <cstring>

namespace SotcShaderCfg {
[[noreturn]] inline void Fail(const char* reason, std::uint32_t source,
                            std::uint32_t target, std::uint32_t bytes,
                            std::size_t instructions) {
    const auto& context=SotcShaderDecode::current;
    const bool known_source=instructions && source!=UINT32_MAX &&
        std::strcmp(reason,"target_outside_program") &&
        std::strcmp(reason,"target_inside_instruction") && std::strcmp(reason,"no_blocks");
    std::fprintf(stderr,"GPU shader CFG: reason=%s phase=%s shader=%016llx stage=%d "
                 "source_pc_known=%d source_pc=%u target_pc=%u decoded_bytes=%u instructions=%llu; "
                 "CPU control-flow evidence, no target fabricated\n",
                 reason,context.phase,static_cast<unsigned long long>(context.shader),
                 context.stage,int(known_source),source,target,bytes,
                 static_cast<unsigned long long>(instructions));
    std::fflush(stderr);
    assert_fail_impl();
    std::abort();
}
} // namespace SotcShaderCfg
