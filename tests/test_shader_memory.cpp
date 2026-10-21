// Original synthetic IR through the actual installed SRT and shader-info passes.
// No GPU, guest code or game shader. Host callbacks are fail-closed in the driver.
#include <cassert>
#include <cstdio>
#include <cstring>
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "shader_recompiler/profile.h"
#include "core/emulator_settings.h"
int main() {
    using namespace Shader;
    const bool enabled=EmulatorSettings.IsDirectMemoryAccessEnabled();
    for(unsigned kind=0;kind<12;++kind) {
        Common::ObjectPool<IR::Inst> pool;
        IR::Block block(pool),predecessor(pool),alternate(pool);
        IR::IREmitter ir(block);
        u32 words[8]={11,22,33,44,55,66,77,88},ud[16]={};
        uintptr_t pointer=(uintptr_t)words;std::memcpy(ud,&pointer,8);
        ud[2]=3;
        Info info{};info.hw_stage=HwStage::Compute;info.sw_stage=SwStage::Compute;info.user_data=ud;
        IR::Program program(info);program.blocks.push_back(&block);program.post_order_blocks.push_back(&block);
        const auto base=ir.CompositeConstruct(ir.GetUserData(IR::ScalarReg::S0),ir.GetUserData(IR::ScalarReg::S1));
        const auto phi_value=[&](std::initializer_list<IR::U32> values) {
            auto phi=block.PrependNewInst(block.end(),IR::Opcode::Phi,{},static_cast<u32>(IR::Type::U32));
            unsigned edge=0;
            for(const auto& value:values) phi->AddPhiOperand(edge++ ? &alternate : &predecessor,value);
            return IR::U32{IR::Value{&*phi}};
        };
        auto offset=ir.Imm32(1u);
        if(kind==2) offset=ir.ReadFirstLane(ir.Imm32(3u));
        else if(kind==1 || kind==3) {
            offset=phi_value({ir.Imm32(3u)});
            if(kind==3) offset=ir.IAdd(offset,ir.Imm32(1u));
        }
        else if(kind==4) offset=phi_value({ir.Imm32(3u),ir.Imm32(3u)});
        else if(kind==5) offset=phi_value({ir.Imm32(3u),ir.Imm32(4u)});
        else if(kind==6) offset=phi_value({ir.GetUserData(IR::ScalarReg::S2),ir.GetUserData(IR::ScalarReg::S2)});
        else if(kind==7 || kind==8) {
            const auto a=ir.IAdd(ir.GetUserData(IR::ScalarReg::S2),ir.Imm32(1u));
            const auto b=kind==7 ? ir.IAdd(ir.GetUserData(IR::ScalarReg::S2),ir.Imm32(1u))
                                 : ir.ISub(ir.GetUserData(IR::ScalarReg::S2),ir.Imm32(1u));
            offset=phi_value({a,b});
        }
        else if(kind==9) offset=phi_value({phi_value({ir.Imm32(3u),ir.Imm32(3u)}),ir.Imm32(3u)});
        else if(kind==10) {
            offset=phi_value({ir.Imm32(3u)});
            offset.Inst()->AddPhiOperand(&alternate,offset);
        }
        else if(kind==11) {
            offset=ir.GetUserData(IR::ScalarReg::S2);
            // A small shared DAG otherwise expands exponentially while comparing
            // identical incoming programs. Exhaustion must preserve a dynamic read.
            for(unsigned i=0;i<24;++i) offset=phi_value({offset,offset});
        }
        const bool dynamic=kind==2 || kind==5 || kind==8 || kind==10 || kind==11;
        const u32 expected_word=kind==0 ? 22 : (kind==3 || kind==7 ? 55 : 44);
        const auto varying=ir.ReadConst(base,offset);
        const auto uniform=ir.ReadConst(base,ir.Imm32(2u));
        Optimization::FlattenExtendedUserdataPass(program);
        assert((varying.Inst()->Flags<u32>()==0)==dynamic);
        assert(uniform.Inst()->Flags<u32>()>=16);
        assert(info.flattened_ud_buf.at(uniform.Inst()->Flags<u32>())==33);
        if(!dynamic) assert(info.flattened_ud_buf.at(varying.Inst()->Flags<u32>())==expected_word);
        Profile profile{};Optimization::CollectShaderInfoPass(program,profile);
        const bool expected=enabled && dynamic;
        assert(info.uses_dma==expected);
        assert(True(info.readconst_types & Info::ReadConstType::Dynamic)==expected);
        bool page_table=false,fault_buffer=false;
        for(const auto& buffer:info.buffers) {
            page_table|=buffer.buffer_type==BufferType::BdaPagetable;
            fault_buffer|=buffer.buffer_type==BufferType::FaultBuffer;
        }
        assert(page_table==expected && fault_buffer==expected);
    }
    std::printf("PASS: twelve actual SRT/shader-info IR cases: equal constant/uniform/arithmetic/nested Phi offsets preserved; differing edges, lane reads, cycles and expansion limits remain dynamic; DMA=%d; no GPU initialized\n",enabled);
}
