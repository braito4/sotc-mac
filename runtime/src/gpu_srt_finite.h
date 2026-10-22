// SPDX-License-Identifier: GPL-2.0-or-later
// Original finite-choice scalar SRT lowering; never follows guest pointers here.
#pragma once
#include <algorithm>
#include <vector>
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/passes/resource_pass.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/resource.h"

namespace SotcSrtFinite {
using namespace Shader;
using Values = std::vector<u32>;

// An exhaustive small set, not a range guessed from the first Phi incoming edge.
inline bool Choices(const IR::Value& value, Values& result,
                    std::vector<const IR::Inst*>& active, unsigned& budget) {
    if (!budget) return false;
    --budget;
    if (value.IsImmediate()) { result = {value.U32()}; return true; }
    const auto* inst = value.TryInst();
    if (!inst || active.size() >= 16 ||
        std::find(active.begin(), active.end(), inst) != active.end()) return false;
    active.push_back(inst);
    struct Pop { std::vector<const IR::Inst*>& list; ~Pop() { list.pop_back(); } } pop{active};
    const auto insert = [&](u32 word) {
        if (std::find(result.begin(), result.end(), word) == result.end()) result.push_back(word);
        return result.size() <= 8;
    };
    if (inst->GetOpcode() == IR::Opcode::Phi) {
        if (!inst->NumArgs() || inst->NumArgs() > 8) return false;
        for (unsigned i = 0; i < inst->NumArgs(); ++i) {
            Values edge;
            if (!Choices(inst->Arg(i), edge, active, budget)) return false;
            for (u32 word : edge) if (!insert(word)) return false;
        }
        return true;
    }
    if (inst->GetOpcode() == IR::Opcode::ReadFirstLane)
        return Choices(inst->Arg(0), result, active, budget);
    switch (inst->GetOpcode()) {
    case IR::Opcode::IAdd32: case IR::Opcode::ISub32: case IR::Opcode::IMul32:
    case IR::Opcode::BitwiseAnd32: case IR::Opcode::BitwiseOr32: case IR::Opcode::BitwiseXor32:
    case IR::Opcode::ShiftLeftLogical32: case IR::Opcode::ShiftRightLogical32:
        break;
    default: return false;
    }
    Values left, right;
    if (!Choices(inst->Arg(0), left, active, budget) ||
        !Choices(inst->Arg(1), right, active, budget)) return false;
    for (u32 a : left) for (u32 b : right) {
        u32 word;
        switch (inst->GetOpcode()) {
        case IR::Opcode::IAdd32: word = a + b; break;
        case IR::Opcode::ISub32: word = a - b; break;
        case IR::Opcode::IMul32: word = a * b; break;
        case IR::Opcode::BitwiseAnd32: word = a & b; break;
        case IR::Opcode::BitwiseOr32: word = a | b; break;
        case IR::Opcode::BitwiseXor32: word = a ^ b; break;
        case IR::Opcode::ShiftLeftLogical32: if (b >= 32) return false; word = a << b; break;
        case IR::Opcode::ShiftRightLogical32: if (b >= 32) return false; word = a >> b; break;
        default: return false;
        }
        if (!insert(word)) return false;
    }
    return true;
}

inline unsigned Expand(IR::Program& program, const Optimization::ResourceDiscoveryList& resources) {
    std::vector<IR::Inst*> reads;
    std::vector<const IR::Inst*> protected_nodes, pending;
    // Descriptor dwords are retained by ResourceDiscover outside the IR use list.
    // Protect their complete dependencies, along with every constant-read pointer.
    for (const auto& resource : resources) for (const auto& sharp : resource.sharps) {
        if (pending.size() > 16384) return 0;
        if (sharp.num_dwords > sharp.dwords.size()) return 0;
        for (unsigned i = 0; i < sharp.num_dwords; ++i)
            if (auto* inst = sharp.dwords[i].TryInst()) pending.push_back(inst);
        if (sharp.post_op == SharpFetchPostOp::DisableAnisoIfSingleLod)
            if (auto* inst = sharp.post_op_data.lod_prod.TryInst()) pending.push_back(inst);
    }
    for (auto* block : program.post_order_blocks) for (auto& inst : *block) {
        if (pending.size() > 16384) return 0;
        if (inst.GetOpcode() == IR::Opcode::ReadConst || inst.GetOpcode() == IR::Opcode::ReadConstBuffer) {
            if (auto* base = inst.Arg(0).TryInst()) pending.push_back(base);
            if (inst.GetOpcode() == IR::Opcode::ReadConst) reads.push_back(&inst);
        }
    }
    if (reads.size() > 8192) return 0;
    while (!pending.empty()) {
        const auto* inst = pending.back(); pending.pop_back();
        if (std::find(protected_nodes.begin(), protected_nodes.end(), inst) != protected_nodes.end()) continue;
        if (protected_nodes.size() >= 4096) return 0;
        if (inst->NumArgs() > 64) return 0;
        protected_nodes.push_back(inst);
        for (unsigned i = 0; i < inst->NumArgs(); ++i)
            if (auto* child = inst->Arg(i).TryInst()) pending.push_back(child);
        if (pending.size() > 16384) return 0;
    }
    unsigned expanded = 0;
    for (auto* read : reads) {
        if (expanded == 64) break;
        if (!read->HasUses() || read->Arg(1).IsImmediate() ||
            std::find(protected_nodes.begin(), protected_nodes.end(), read) != protected_nodes.end()) continue;
        const auto* base = read->Arg(0).TryInst();
        if (!base || base->GetOpcode() != IR::Opcode::CompositeConstructU32x2) continue;
        const auto* lo = base->Arg(0).TryInst(); const auto* hi = base->Arg(1).TryInst();
        if (!lo || !hi || lo->GetOpcode() != IR::Opcode::GetUserData ||
            hi->GetOpcode() != IR::Opcode::GetUserData) continue;
        Values choices; std::vector<const IR::Inst*> active; unsigned budget = 256;
        if (!Choices(read->Arg(1), choices, active, budget) || choices.size() < 2 ||
            std::any_of(choices.begin(), choices.end(), [](u32 word) { return word >= 4096; })) continue;
        auto* block = read->GetParent();
        IR::IREmitter ir(*block, block->Instructions().iterator_to(*read));
        const auto address = read->Arg(0); const IR::U32 offset{read->Arg(1)};
        IR::U32 selected = ir.ReadConst(address, ir.Imm32(choices[0]));
        for (unsigned i = 1; i < choices.size(); ++i) {
            const auto loaded = ir.ReadConst(address, ir.Imm32(choices[i]));
            selected = IR::U32{ir.Select(ir.IEqual(offset, ir.Imm32(choices[i])), loaded, selected)};
        }
        // Selection remains in GPU IR. Every possible value was copied separately
        // by the existing protected SRT walker; no arbitrary branch wins on CPU.
        read->ReplaceUsesWith(selected);
        ++expanded;
    }
    return expanded;
}
} // namespace SotcSrtFinite
