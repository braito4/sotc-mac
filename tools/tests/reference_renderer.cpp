// Synthetic resources only; link against the production renderer built for this profile.
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <spirv_cross/spirv_msl.hpp>
#include "gpu_srt_program.h"
#include "gpu_texture_gather.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

static std::array<uint32_t, 2048> table;
extern "C" void runtime_memory_read_backing(uintptr_t at, void* out, uint64_t count) {
    assert(at == reinterpret_cast<uintptr_t>(table.data()) && count == sizeof(table));
    std::memcpy(out, table.data(), count);
}

static void check_window(const char* output) {
    using namespace Shader;
    Info info{};
    info.hw_stage = HwStage::Compute;
    info.sw_stage = SwStage::Compute;
    std::array<u32, 16> ud{};
    const auto address = reinterpret_cast<uintptr_t>(table.data());
    std::memcpy(ud.data(), &address, sizeof(address));
    info.user_data = ud;
    for (u32 i = 0; i < table.size(); ++i) table[i] = i * 7 + 9;

    Common::ObjectPool<IR::Inst> pool;
    IR::Block block(pool);
    IR::IREmitter ir(block);
    IR::Program p(info);
    p.blocks.push_back(&block);
    p.post_order_blocks.push_back(&block);
    p.syntax_list.push_back({.data={.block=&block}, .type=IR::AbstractSyntaxNode::Type::Block});
    p.syntax_list.push_back({.type=IR::AbstractSyntaxNode::Type::Return});
    const auto lo = ir.GetUserData(IR::ScalarReg::S0);
    const auto hi = ir.GetUserData(IR::ScalarReg::S1);
    const auto varying = ir.GetAttributeU32(IR::Attribute::LocalInvocationIndex);
    const IR::U32 sum{ir.IAdd(lo, varying)};
    const IR::U32 carry{ir.Select(ir.ILessThan(sum, lo, false), ir.Imm32(1), ir.Imm32(0))};
    const auto address_pair = ir.CompositeConstruct(ir.ReadFirstLane(sum),
                                                    ir.ReadFirstLane(IR::U32{ir.IAdd(hi, carry)}));
    const auto first = ir.ReadConst(address_pair, ir.Imm32(0));
    const auto next = ir.ReadConst(address_pair, ir.Imm32(1));
    ir.WriteShared(32, first, ir.Imm32(0));
    ir.WriteShared(32, next, ir.Imm32(4));
    Optimization::FlattenExtendedUserdataPass(p);
    const auto flags = first.Inst()->Flags<u32>();
    assert((flags & SotcSrtWindow::Flag) && flags == next.Inst()->Flags<u32>());
    const auto base = flags & ~SotcSrtWindow::Flag;
    assert(info.flattened_ud_buf.size() == base + table.size());
    assert(std::equal(table.begin(), table.end(), info.flattened_ud_buf.begin() + base));
    const auto* code = reinterpret_cast<const u32*>(SrtWalkerCode(info.srt_info.walker_func));
    const auto span = std::span(code, info.srt_info.walker_func_size / sizeof(u32));
    assert(!SotcSrtProgram::Validate(span, base + table.size()).reason);
    assert(SotcSrtProgram::Validate(span, base + table.size() - 1).reason);
    assert(SotcSrtProgram::Validate(span).reason);
    assert(next.Inst()->Arg(1).Inst()->GetOpcode() == IR::Opcode::IAdd32);

    Profile profile{};
    profile.subgroup_size = 32;
    profile.support_int64 = true;
    profile.supported_spirv = 0x00010600;
    RuntimeInfo rt{};
    rt.Initialize(info.hw_stage, info.sw_stage);
    rt.hw.cs.workgroup_size = {32, 1, 1};
    rt.hw.cs.shared_memory_size = 8;
    Optimization::CollectShaderInfoPass(p, profile);
    Backend::Bindings bindings{};
    const auto spv = Backend::SPIRV::EmitSPIRV(profile, rt, p, bindings);
    std::ofstream file(output, std::ios::binary);
    file.write(reinterpret_cast<const char*>(spv.data()), spv.size() * sizeof(u32));
    assert(file.good());
    spirv_cross::CompilerMSL compiler(spv);
    auto settings = compiler.get_msl_options();
    settings.platform = spirv_cross::CompilerMSL::Options::macOS;
    settings.set_msl_version(3, 1);
    compiler.set_msl_options(settings);
    assert(!compiler.compile().empty());
}

static void check_texture_parts() {
    using namespace SotcTextureGather;
    int buffers[2]{};
    auto mapping = [](Bytes at, Bytes* phys, uintptr_t* end) {
        if (at >= 12) return false;
        *phys = at;
        *end = at < 4 ? 4 : 12;
        return true;
    };
    auto imported = [&](Bytes phys) -> std::optional<View<int>> {
        if (phys >= 12) return {};
        return View<int>{buffers + (phys >= 8), phys % 8, 8 - phys % 8};
    };
    auto result = Plan<int>(0, 12, mapping, imported);
    assert(result.size() == 2 && result[0].size == 8 && result[1].size == 4);
    assert(result[1].source == 0 && result[1].destination == 8);
    assert(Plan<int>(0, 16, mapping, imported).empty());
    assert(Plan<int>(1, 8, mapping, imported).empty());
    auto missing = [&](Bytes at) { return at < 8 ? imported(at) : std::optional<View<int>>{}; };
    assert(Plan<int>(0, 12, mapping, missing).empty());
}

int main(int argc, char** argv) {
    assert(argc == 2);
    check_texture_parts();
    check_window(argv[1]);
}
