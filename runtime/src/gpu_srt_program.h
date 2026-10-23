// SPDX-License-Identifier: GPL-2.0-or-later
// Original structural validation of the selected portable SRT bytecode.
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include "gpu_shader_decode.h"

namespace SotcSrtProgram {
struct Error { const char* reason{}; std::size_t word{}; };
// Opcodes are the persisted SRTB format, checked against the producer enum.
inline Error Validate(std::span<const std::uint32_t> code) {
    if (code.empty() || code[0] != 0x42545253u) return {"srt_bad_magic", 0};
    unsigned depth = 0, values = 0;
    for (std::size_t pc = 1; pc < code.size();) {
        const auto at = pc;
        const auto word = code[pc++], op = word & 0x7fffffffu;
        if (op > 20 || ((word & 0x80000000u) && op != 1 && op != 2 && op != 4 && op != 5))
            return {"srt_bad_opcode", at};
        const unsigned immediates = op == 4 ? 2 :
            (op == 1 || op == 5 || op == 6 || op == 7 || op == 8 ? 1 : 0);
        if (code.size() - pc < immediates) return {"srt_truncated_operand", at};
        pc += immediates;
        if (!op) {
            if (pc != code.size() || depth || values) return {"srt_unbalanced_end", at};
            return {};
        }
        if (op == 1 || op == 2) {
            if (op == 2) {
                if (!values) return {"srt_value_underflow", at};
                --values;
            }
            if (depth == 64) return {"srt_pointer_overflow", at};
            ++depth;
        } else if (op == 3) {
            if (!depth) return {"srt_pointer_underflow", at};
            --depth;
        } else if (op == 5) {
            if (!values) return {"srt_value_underflow", at};
            --values;
        } else if (op >= 6 && op <= 8) {
            if (values == 32) return {"srt_value_overflow", at};
            ++values;
        } else if (op >= 9) {
            const unsigned required = op == 17 ? 1 : op == 20 ? 3 : 2;
            if (values < required) return {"srt_value_underflow", at};
            values -= required - 1;
        }
    }
    return {"srt_missing_end", code.size()};
}
inline std::uint32_t Extract(std::uint32_t base, std::uint32_t offset, std::uint32_t bits) {
    if (offset > 32 || bits > 32 - offset)
        SotcShaderDecode::Fail("srt_invalid_bitfield", 0, offset, bits);
    if (!bits) return 0;
    if (bits == 32) return base;
    return (base >> offset) & ((std::uint32_t{1} << bits) - 1);
}
} // namespace SotcSrtProgram
