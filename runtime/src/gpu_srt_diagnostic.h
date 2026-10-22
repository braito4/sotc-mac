// Original failure-only IR provenance. No guest-memory reads or GPU operations.
#pragma once
#include <array>
#include <atomic>
#include <cstdio>
#include <string>
#include <magic_enum/magic_enum.hpp>
#include "shader_recompiler/ir/value.h"
#include "shader_recompiler/ir/microinstruction.h"

namespace SotcSrtDiagnostic {
struct Context { unsigned long long shader{}; int stage{-1}; unsigned emitted{}; };
inline thread_local Context current{};
inline std::atomic<unsigned> emitted{};
class Scope {
    Context previous;
public:
    Scope(unsigned long long shader, int stage) : previous(current) {
        current = {shader, stage, 0};
    }
    ~Scope() { current = previous; }
};

// Only IR owned by the active compiler pass is inspected. Shared/cyclic graphs
// are represented by references, with hard limits on nodes, depth and text.
inline std::string Describe(const Shader::IR::Value& root) {
    std::array<const Shader::IR::Inst*, 32> seen{};
    unsigned count = 0;
    std::string text;
    const auto append = [&](std::string_view value) {
        if (text.size() < 1024) text.append(value.substr(0, 1024 - text.size()));
    };
    const auto visit = [&](auto&& self, const Shader::IR::Value& value, unsigned depth) -> void {
        if (text.size() >= 1024) return;
        if (value.IsImmediate()) { append("imm"); return; }
        const auto* inst = value.TryInst();
        if (!inst) { append("non-inst"); return; }
        for (unsigned i = 0; i < count; ++i) {
            if (seen[i] == inst) { append("ref" + std::to_string(i)); return; }
        }
        if (count == seen.size() || depth == 6) { append("limit"); return; }
        const auto id = count;
        seen[count++] = inst;
        append(std::to_string(id)); append(":");
        append(magic_enum::enum_name(inst->GetOpcode())); append("(");
        const auto args = inst->NumArgs();
        for (unsigned i = 0; i < args && i < 8; ++i) {
            if (i) append(",");
            self(self, inst->Arg(i), depth + 1);
        }
        if (args > 8) append(",limit");
        append(")");
    };
    visit(visit, root, 0);
    return text;
}

inline void Failure(const char* site, const Shader::IR::Value& root) {
    if (current.emitted >= 8) return;
    unsigned ticket = emitted.load(std::memory_order_relaxed);
    do {
        if (ticket >= 128) return;
    } while (!emitted.compare_exchange_weak(ticket, ticket + 1, std::memory_order_relaxed));
    ++current.emitted;
    const auto expression = Describe(root);
    std::fprintf(stderr, "GPU SRT diagnostic: shader=%016llx stage=%d site=%s "
                 "expression=%s; bounded IR shape, values omitted, CPU evidence only\n",
                 current.shader, current.stage, site, expression.c_str());
}
} // namespace SotcSrtDiagnostic
