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
    for(unsigned kind=0;kind<4;++kind) {
        Common::ObjectPool<IR::Inst> pool;
        IR::Block block(pool),predecessor(pool);
        IR::IREmitter ir(block);
        u32 words[8]={11,22,33,44,55,66,77,88},ud[16]={};
        uintptr_t pointer=(uintptr_t)words;std::memcpy(ud,&pointer,8);
        Info info{};info.hw_stage=HwStage::Compute;info.sw_stage=SwStage::Compute;info.user_data=ud;
        IR::Program program(info);program.blocks.push_back(&block);program.post_order_blocks.push_back(&block);
        const auto base=ir.CompositeConstruct(ir.GetUserData(IR::ScalarReg::S0),ir.GetUserData(IR::ScalarReg::S1));
        auto offset=ir.Imm32(1u);
        if(kind==2) offset=ir.ReadFirstLane(ir.Imm32(3u));
        else if(kind) {
            auto phi=block.PrependNewInst(block.end(),IR::Opcode::Phi,{},static_cast<u32>(IR::Type::U32));
            phi->AddPhiOperand(&predecessor,ir.Imm32(3u));
            offset=IR::U32{IR::Value{&*phi}};
            if(kind==3) offset=ir.IAdd(offset,ir.Imm32(1u));
        }
        const auto varying=ir.ReadConst(base,offset);
        const auto uniform=ir.ReadConst(base,ir.Imm32(2u));
        Optimization::FlattenExtendedUserdataPass(program);
        assert((varying.Inst()->Flags<u32>()==0)==(kind!=0));
        assert(uniform.Inst()->Flags<u32>()>=16);
        assert(info.flattened_ud_buf.at(uniform.Inst()->Flags<u32>())==33);
        if(!kind) assert(info.flattened_ud_buf.at(varying.Inst()->Flags<u32>())==22);
        Profile profile{};Optimization::CollectShaderInfoPass(program,profile);
        const bool expected=enabled && kind;
        assert(info.uses_dma==expected);
        assert(True(info.readconst_types & Info::ReadConstType::Dynamic)==expected);
        bool page_table=false,fault_buffer=false;
        for(const auto& buffer:info.buffers) {
            page_table|=buffer.buffer_type==BufferType::BdaPagetable;
            fault_buffer|=buffer.buffer_type==BufferType::FaultBuffer;
        }
        assert(page_table==expected && fault_buffer==expected);
    }
    std::printf("PASS: four actual SRT/shader-info IR cases: uniform values preserved, Phi/lane/arithmetic reads left for shader execution, guarded page-table/fault-buffer selection checked with DMA=%d; no GPU initialized\n",enabled);
}
