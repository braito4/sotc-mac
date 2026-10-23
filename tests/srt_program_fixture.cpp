// Original portable bytecode/IR controls; no GPU or game-derived instructions.
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include "gpu_srt_program.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "shader_recompiler/ir/passes/srt.h"
using namespace Shader;
static constexpr u32 magic=0x42545253;
static PFN_SrtWalker Install(const std::vector<u32>& code) {
    return RegisterWalkerCode(reinterpret_cast<const u8*>(code.data()),code.size()*4);
}
int main(int argc,char** argv) {
    const std::string mode=argc>1?argv[1]:"valid";
    SotcShaderDecode::Scope scope("srt-fixture",0x5678,0,nullptr);
    if(mode!="valid") {
        std::vector<u32> c{magic};
        if(mode=="values") {for(unsigned i=0;i<33;++i)c.insert(c.end(),{8,0});}
        else if(mode=="depth") {for(unsigned i=0;i<65;++i)c.insert(c.end(),{1,0});}
        else if(mode=="pop") c.push_back(3);
        else if(mode=="arith") c.push_back(9);
        else if(mode=="copy") c.insert(c.end(),{5,16});
        else if(mode=="truncated") c.push_back(4);
        else if(mode=="opcode") c.push_back(21);
        else if(mode=="flag") c.push_back(0x80000008u);
        else if(mode=="end") c.insert(c.end(),{8,0});
        else if(mode=="trailing") c.insert(c.end(),{0,8,0});
        else if(mode=="missing") { c.insert(c.end(),{1,0}); (void)Install(c); return 99; }
        else if(mode=="bitfield") { (void)SotcSrtProgram::Extract(0,31,2);return 99; }
        else return 98;
        c.push_back(0);(void)Install(c);return 99;
    }
    // Maxima are accepted, not reduced. Also check the runtime's patched-load flag.
    std::vector<u32> depth{magic};
    for(unsigned i=0;i<64;++i)depth.insert(depth.end(),{1,0});
    for(unsigned i=0;i<64;++i)depth.push_back(3);
    depth.push_back(0);assert(Install(depth));
    std::vector<u32> values{magic};
    for(unsigned i=0;i<32;++i)values.insert(values.end(),{8,i});
    for(unsigned i=0;i<31;++i)values.push_back(9);
    values.insert(values.end(),{5,16,0});assert(Install(values));
    assert(Install({magic,0x80000001u,0,0x80000004u,0,16,3,0}));
    u32 words[16],ud[16]{},flat[32]{};
    for(unsigned i=0;i<16;++i)words[i]=0x100+i;
    const auto address=reinterpret_cast<uintptr_t>(words);std::memcpy(ud,&address,8);
    unsigned cases=0;
    for(u32 base:{0u,7u,0x89abcdefu,0xffffffffu}) {
        for(u32 offset=0;offset<=32;++offset)for(u32 bits=0;bits<=32-offset;++bits) {
            const u32 expected=bits?u32((u64(base)>>offset)&((u64{1}<<bits)-1)):0;
            assert(SotcSrtProgram::Extract(base,offset,bits)==expected);
            auto walker=Install({magic,1,0,8,base,8,offset,8,bits,20,8,15,14,5,16,3,0});
            assert(walker);CallSrtWalker(walker,ud,flat);assert(flat[16]==words[expected&15]);
            Common::ObjectPool<IR::Inst> pool;IR::Block block(pool);IR::IREmitter ir(block);
            auto value=ir.BitFieldExtract(ir.Imm32(base),ir.Imm32(offset),ir.Imm32(bits));
            ir.SetScalarReg(IR::ScalarReg::S0,value);
            IR::BlockList blocks;blocks.push_back(&block);
            Optimization::ConstantPropagationPass(blocks);
            const auto folded=block.Instructions().back().Arg(1);
            assert(folded.IsImmediate() && folded.U32()==expected);
            ++cases;
        }
    }
    std::printf("PASS: %u bitfield cases vs 64-bit oracle through installed SRT and constant folding; exact 32-value/64-pointer limits; no GPU\n",cases);
}
