// Original synthetic IR through the actual wave64 lowering pass and SPIR-V emitter.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#ifdef SOTC_SPIRV_EMITTER
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
using namespace Shader;
static u32 offset_value(IR::Value v,u32 invocation) {
    if(v.IsImmediate()) return v.U32();
    const auto* i=v.Inst();
    if(i->GetOpcode()==IR::Opcode::GetAttributeU32) {
        assert(i->Arg(0).Attribute()==IR::Attribute::LocalInvocationIndex);return invocation;
    }
    const u32 a=offset_value(i->Arg(0),invocation),b=offset_value(i->Arg(1),invocation);
    switch(i->GetOpcode()) {
    case IR::Opcode::IAdd32:return a+b;
    case IR::Opcode::BitwiseAnd32:return a&b;
    case IR::Opcode::ShiftLeftLogical32:return a<<b;
    case IR::Opcode::ShiftRightLogical32:return a>>b;
    default:assert(false);return 0;
    }
}
std::vector<uint32_t> wave_case(unsigned threads,unsigned bytes) {
    Info info{};info.hw_stage=HwStage::Compute;info.sw_stage=SwStage::Compute;
    RuntimeInfo runtime{};runtime.Initialize(info.hw_stage,info.sw_stage);
    runtime.hw.cs.workgroup_size={threads,1,1};runtime.hw.cs.shared_memory_size=bytes;
    Profile profile{};profile.subgroup_size=32;profile.support_int64=true;profile.supported_spirv=0x00010600;
    Common::ObjectPool<IR::Inst> pool;IR::Block block(pool);IR::IREmitter ir(block);
    IR::Program program(info);program.blocks.push_back(&block);program.post_order_blocks.push_back(&block);
    program.syntax_list.push_back({.data={.block=&block},.type=IR::AbstractSyntaxNode::Type::Block});
    program.syntax_list.push_back({.type=IR::AbstractSyntaxNode::Type::Return});
    const auto id=ir.GetAttributeU32(IR::Attribute::LocalInvocationIndex);
    const auto predicate=ir.INotEqual(ir.BitwiseAnd(id,ir.Imm32(5u)),ir.Imm32(0u));
    const auto unpack=ir.UnpackUint2x32(ir.Ballot(predicate));
    ir.WriteShared(32,ir.CompositeExtract(unpack,0),ir.Imm32(0u));
    ir.WriteShared(32,ir.CompositeExtract(unpack,1),ir.Imm32(4u));
    Optimization::LowerWave64BallotPass(program,runtime,profile);
    Optimization::CollectShaderInfoPass(program,profile);
    if(info.shared_types==IR::Type::U32) {
        const auto* mask=unpack.Inst()->Arg(0).Inst();assert(mask->GetOpcode()==IR::Opcode::PackUint2x32);
        const auto* pair=mask->Arg(0).Inst();assert(pair->GetOpcode()==IR::Opcode::CompositeConstructU32x2);
        const auto* low=pair->Arg(0).Inst();const auto* high=pair->Arg(1).Inst();
        assert(low->GetOpcode()==IR::Opcode::LoadSharedU32 && high->GetOpcode()==IR::Opcode::LoadSharedU32);
        for(u32 tid=0;tid<threads;++tid) {
            const u32 base=((bytes+7u)&~7u)+(tid/64u)*8u;
            assert(offset_value(low->Arg(0),tid)==base && offset_value(high->Arg(0),tid)==base+4u);
        }
        unsigned barriers=0;for(const auto& i:block.Instructions()) barriers+=i.GetOpcode()==IR::Opcode::Barrier;
        assert(barriers==2);
    }
    Backend::Bindings bindings{};
    return Backend::SPIRV::EmitSPIRV(profile,runtime,program,bindings);
}
int main(int argc,char** argv) {
    assert(argc==2);unsigned count=0;
    for(unsigned threads : {64u,128u,256u}) for(unsigned bytes : {8u,20u,256u}) {
        const auto words=wave_case(threads,bytes);
        std::ofstream out(std::filesystem::path(argv[1])/(std::to_string(count++)+".spv"),std::ios::binary);
        out.write(reinterpret_cast<const char*>(words.data()),words.size()*sizeof(uint32_t));assert(out);
    }
    puts("PASS: synthetic wave64 IR emitted; single-view cases check paired mask loads, per-invocation scratch offsets and both barriers");
}
#else
#include <regex>
#include <spirv_cross/spirv_msl.hpp>
int main(int argc,char** argv) {
    assert(argc==2);
    unsigned cases=0,missing=0;
    for(const auto& entry:std::filesystem::directory_iterator(argv[1])) {
        if(entry.path().extension()!=".spv") continue;
        std::ifstream in(entry.path(),std::ios::binary|std::ios::ate);const auto bytes=in.tellg();assert(bytes>=20 && bytes%4==0);in.seekg(0);
        std::vector<uint32_t> words(size_t(bytes)/4);in.read(reinterpret_cast<char*>(words.data()),bytes);assert(in);
        spirv_cross::CompilerMSL c(words);auto o=c.get_msl_options();
        o.platform=spirv_cross::CompilerMSL::Options::macOS;o.set_msl_version(3,1);c.set_msl_options(o);
        const auto msl=c.compile();++cases;
        missing+=!std::regex_search(msl,std::regex(R"(threadgroup[^;\n]*\bshared_mem_u32\b)"));
    }
    assert(cases==9);
    printf("wave64: %u synthetic compute modules, %u with missing shared-U32 declarations; no GPU initialized\n",cases,missing);
    return missing ? 2 : 0;
}
#endif
