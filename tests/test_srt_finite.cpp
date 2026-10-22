// Original finite-choice data reads through the installed SRT walker; no GPU.
#include <cassert>
#include <cstring>
#include <cstdio>
#include "gpu_srt_finite.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "shader_recompiler/profile.h"
using namespace Shader;

static u32 Evaluate(const IR::Value& value, const Info& info, unsigned edge) {
    if (value.IsImmediate()) return value.U32();
    const auto* inst=value.Inst();
    const auto arg=[&](unsigned n){return Evaluate(inst->Arg(n),info,edge);};
    switch(inst->GetOpcode()) {
    case IR::Opcode::Phi: return arg(edge%inst->NumArgs());
    case IR::Opcode::ReadFirstLane: return arg(0);
    case IR::Opcode::IAdd32: return arg(0)+arg(1);
    case IR::Opcode::IMul32: return arg(0)*arg(1);
    case IR::Opcode::IEqual32: return arg(0)==arg(1);
    case IR::Opcode::SelectU32: return arg(0)?arg(1):arg(2);
    case IR::Opcode::ReadConst:
        assert(inst->Flags<u32>()>=16);
        return info.flattened_ud_buf.at(inst->Flags<u32>());
    default: assert(false);return 0;
    }
}
int main() {
    for(unsigned kind=0;kind<15;++kind) {
        Common::ObjectPool<IR::Inst> pool;IR::Block block(pool),left(pool),right(pool);
        IR::IREmitter ir(block);
        u32 words[16]={10,20,30,40,50,60,70,80},ud[16]={};
        uintptr_t pointer=reinterpret_cast<uintptr_t>(words);std::memcpy(ud,&pointer,8);
        Info info{};info.hw_stage=HwStage::Compute;info.sw_stage=SwStage::Compute;info.user_data=ud;
        IR::Program program(info);program.blocks.push_back(&block);program.post_order_blocks.push_back(&block);
        const auto base=ir.CompositeConstruct(ir.GetUserData(IR::ScalarReg::S0),ir.GetUserData(IR::ScalarReg::S1));
        const auto phi=[&](std::initializer_list<IR::U32> values) {
            auto it=block.PrependNewInst(block.end(),IR::Opcode::Phi,{},static_cast<u32>(IR::Type::U32));
            unsigned edge=0;for(auto value:values)it->AddPhiOperand(edge++?&right:&left,value);
            return IR::U32{IR::Value{&*it}};
        };
        auto offset=phi({ir.Imm32(3u),ir.Imm32(4u)});
        if(kind==1) offset=phi({ir.Imm32(3u),phi({ir.Imm32(4u),ir.Imm32(5u)})});
        if(kind==2) offset=ir.ReadFirstLane(offset);
        if(kind==3) offset=ir.IAdd(ir.IMul(phi({ir.Imm32(1u),ir.Imm32(2u)}),ir.Imm32(2u)),ir.Imm32(1u));
        if(kind==4) offset.Inst()->AddPhiOperand(&right,ir.IAdd(offset,ir.Imm32(1u)));
        if(kind==5) offset=ir.GetUserData(IR::ScalarReg::S2);
        if(kind==6) offset=ir.LaneId();
        if(kind==7) offset=phi({ir.Imm32(0u),ir.Imm32(1u),ir.Imm32(2u),ir.Imm32(3u),ir.Imm32(4u),ir.Imm32(5u),ir.Imm32(6u),ir.Imm32(7u),ir.Imm32(8u)});
        if(kind==8) offset=phi({ir.Imm32(3u),ir.Imm32(4096u)});
        if(kind==10) offset=phi({ir.Imm32(3u),ir.Imm32(3u)});
        if(kind==12) offset=ir.IAdd(phi({ir.Imm32(0xffffffffu),ir.Imm32(0u)}),ir.Imm32(1u));
        if(kind==14) for(unsigned i=0;i<20;++i) offset=phi({offset,ir.Imm32(3u)});
        const auto read=ir.ReadConst(base,offset);
        const auto sink=ir.IAdd(read,ir.Imm32(7u));
        Optimization::ResourceDiscoveryList resources;
        if(kind==9) {
            resources.emplace_back();resources.back().sharps[0].num_dwords=1;
            resources.back().sharps[0].dwords[0]=read;
        }
        if(kind==13) {
            resources.emplace_back();
            resources.back().sharps[0].post_op=SharpFetchPostOp::DisableAnisoIfSingleLod;
            resources.back().sharps[0].post_op_data.lod_prod=read;
        }
        if(kind==11) {
            // Protect values that form another table pointer, even with ordinary uses.
            const auto pointer_base=ir.CompositeConstruct(read,ir.GetUserData(IR::ScalarReg::S1));
            (void)ir.ReadConst(pointer_base,ir.Imm32(0u));
        }
        const auto expanded=SotcSrtFinite::Expand(program,resources);
        assert(expanded==(kind<4 || kind==12?1u:0u));
        if(kind>=4 && kind!=12) continue;
        Optimization::FlattenExtendedUserdataPass(program);
        for(unsigned edge=0;edge<2;++edge) {
            const u32 index=kind==12 ? edge : kind==1 ? (edge?5:3) : kind==3 ? (edge?5:3) : (edge?4:3);
            assert(Evaluate(sink,info,edge)==words[index]+7);
        }
        Profile profile{};Optimization::CollectShaderInfoPass(program,profile);
        assert(!info.uses_dma);
        assert(!True(info.readconst_types & Info::ReadConstType::Dynamic));
    }
    {
        Common::ObjectPool<IR::Inst> pool;IR::Block block(pool),left(pool),right(pool);IR::IREmitter ir(block);
        Info info{};IR::Program program(info);program.post_order_blocks.push_back(&block);
        auto p=block.PrependNewInst(block.end(),IR::Opcode::Phi,{},static_cast<u32>(IR::Type::U32));
        p->AddPhiOperand(&left,ir.Imm32(0u));p->AddPhiOperand(&right,ir.Imm32(1u));
        auto base=ir.CompositeConstruct(ir.GetUserData(IR::ScalarReg::S0),ir.GetUserData(IR::ScalarReg::S1));
        for(unsigned i=0;i<65;++i) (void)ir.IAdd(ir.ReadConst(base,IR::U32{IR::Value{&*p}}),ir.Imm32(7u));
        assert(SotcSrtFinite::Expand(program,{})==64);
    }
    std::puts("PASS: finite Phi/nested/ReadFirstLane/arithmetic choices keep both runtime selections through actual SRT flattening; cycles, unknown/lane sources, set/offset limits, descriptor and pointer dependencies excluded; no GPU");
}
