// Original optional proof over the renderer's final structured IR. No GPU work.
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>
#include "gpu_buffer_read_bound.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/ir/passes/resource_pass.h"

namespace SotcBufferRead {
namespace Detail {
using namespace Shader;
constexpr std::uint64_t MaxU32 = std::numeric_limits<std::uint32_t>::max();

struct Guard {
    IR::Value index;
    std::uint32_t flat{};
};

inline std::optional<Guard> ReadGuard(IR::Value condition) {
    if (condition.IsImmediate() || condition.IsEmpty()) return std::nullopt;
    const auto* inst = condition.Inst();
    IR::Value index, limit;
    if (inst->GetOpcode() == IR::Opcode::ULessThan32) {
        index = inst->Arg(0); limit = inst->Arg(1);
    } else if (inst->GetOpcode() == IR::Opcode::UGreaterThan32) {
        index = inst->Arg(1); limit = inst->Arg(0);
    } else {
        return std::nullopt; // No signed, inclusive, mask or inferred loop bounds.
    }
    if (index.IsImmediate() || limit.IsImmediate() || limit.IsEmpty()) return std::nullopt;
    const auto* load = limit.Inst();
    if (load->GetOpcode() != IR::Opcode::ReadConst || !load->Flags<std::uint32_t>())
        return std::nullopt; // Offset zero can be unresolved/dynamic; never infer it.
    return Guard{index, load->Flags<std::uint32_t>()};
}

struct Affine {
    std::uint64_t scale{}, add{};
    std::uint64_t safe{MaxU32};
};

inline std::optional<Affine> AddressImpl(IR::Value value, IR::Value index,
                                       unsigned depth, unsigned& remaining) {
    if (!remaining || depth > 32 || value.IsEmpty() || value.Type() != IR::Type::U32)
        return std::nullopt;
    --remaining; // Shared DAGs must not expand into an exponential proof search.
    if (value == index) return Affine{1, 0, MaxU32};
    if (value.IsImmediate()) return Affine{0, value.U32(), MaxU32};
    const auto* inst = value.Inst();
    const auto op = inst->GetOpcode();
    if (op != IR::Opcode::IAdd32 && op != IR::Opcode::IMul32 &&
        op != IR::Opcode::ShiftRightLogical32) return std::nullopt;
    auto left = AddressImpl(inst->Arg(0), index, depth + 1, remaining);
    if (!left) return std::nullopt;
    if (op == IR::Opcode::ShiftRightLogical32) {
        const auto shift = inst->Arg(1);
        if (!shift.IsImmediate() || shift.U32() >= 32) return std::nullopt;
        const std::uint64_t divisor = std::uint64_t{1} << shift.U32();
        if (left->scale % divisor) return std::nullopt;
        // Keep the pre-shift safe cap: a later shift cannot justify earlier wrap.
        left->scale /= divisor; left->add /= divisor;
        return left;
    }
    const auto right = AddressImpl(inst->Arg(1), index, depth + 1, remaining);
    if (!right) return std::nullopt;
    Affine result;
    result.safe = std::min(left->safe, right->safe);
    if (op == IR::Opcode::IAdd32) {
        result.scale = left->scale + right->scale;
        result.add = left->add + right->add;
    } else {
        if (left->scale && right->scale) return std::nullopt;
        result.scale = left->scale * right->add + right->scale * left->add;
        result.add = left->add * right->add;
    }
    if (result.add > MaxU32 || result.scale > MaxU32) return std::nullopt;
    if (result.scale)
        result.safe = std::min(result.safe, (MaxU32 - result.add) / result.scale);
    return result;
}

inline std::optional<Affine> Address(IR::Value value, IR::Value index) {
    unsigned remaining = 128;
    return AddressImpl(value, index, 0, remaining);
}

inline std::pair<unsigned, unsigned> Access(IR::Opcode op) {
    using O = IR::Opcode;
    switch (op) {
    case O::LoadBufferU8: return {1, 1};
    case O::LoadBufferU16: return {2, 2};
    case O::LoadBufferU64: return {8, 8};
    case O::LoadBufferU32: case O::LoadBufferF32: return {4, 4};
    case O::LoadBufferU32x2: case O::LoadBufferF32x2: return {4, 8};
    case O::LoadBufferU32x3: case O::LoadBufferF32x3: return {4, 12};
    case O::LoadBufferU32x4: case O::LoadBufferF32x4: return {4, 16};
    default: return {0, 0}; // Stores, atomics and formatted/constant accesses reject.
    }
}
} // namespace Detail

inline void Analyze(Shader::IR::Program& program) {
    using namespace Shader;
    auto& buffers = program.info.buffers;
    for (auto& buffer : buffers) buffer.read_bound = {};
    std::vector<bool> rejected(buffers.size());
    std::vector<std::optional<Detail::Guard>> guards;
    std::unordered_set<const IR::Block*> visited;
    bool malformed = false;
    for (const auto& node : program.syntax_list) {
        using T = IR::AbstractSyntaxNode::Type;
        if (node.type == T::If) {
            guards.push_back(Detail::ReadGuard(node.data.if_node.cond));
        } else if (node.type == T::EndIf) {
            if (guards.empty()) { malformed = true; break; }
            guards.pop_back();
        } else if (node.type == T::Block) {
            const auto* block = node.data.block;
            if (!block || !visited.insert(block).second) { malformed = true; break; }
            for (const auto& inst : block->Instructions()) {
                if (!Optimization::IsBufferInstruction(inst)) continue;
                const auto handle = inst.Arg(0);
                if (!handle.IsImmediate() || handle.Type() != IR::Type::U32 ||
                    handle.U32() >= buffers.size()) { malformed = true; break; }
                const auto slot = handle.U32();
                auto& buffer = buffers[slot];
                const auto [unit, width] = Detail::Access(inst.GetOpcode());
                if (!unit || buffer.IsSpecial() || buffer.is_written || buffer.is_formatted) {
                    rejected[slot] = true; continue;
                }
                std::optional<Bound> found;
                for (auto it = guards.rbegin(); it != guards.rend(); ++it) {
                    if (!*it) continue;
                    const auto address = Detail::Address(inst.Arg(1), (*it)->index);
                    if (!address || !address->scale) continue;
                    const auto scale = address->scale * unit;
                    const auto end_add = address->add * unit + width;
                    if (scale > Detail::MaxU32 || end_add > Detail::MaxU32) continue;
                    found = Bound{(*it)->flat, std::uint32_t(scale), std::uint32_t(end_add),
                                  std::uint32_t(address->safe)};
                    break;
                }
                if (!found) { rejected[slot] = true; continue; }
                auto& bound = buffer.read_bound;
                if (!bound.scale) {
                    bound = *found;
                } else if (bound.limit_dword != found->limit_dword) {
                    rejected[slot] = true;
                } else {
                    bound.scale = std::max(bound.scale, found->scale);
                    bound.end_add = std::max(bound.end_add, found->end_add);
                    bound.safe_index_max = std::min(bound.safe_index_max, found->safe_index_max);
                }
            }
        }
    }
    malformed |= !guards.empty();
    // Missing/duplicated blocks could hide an unguarded use. Fail closed.
    for (const auto* block : program.blocks) malformed |= !visited.contains(block);
    for (std::size_t slot = 0; slot < buffers.size(); ++slot)
        if (malformed || rejected[slot]) buffers[slot].read_bound = {};
}

} // namespace SotcBufferRead
