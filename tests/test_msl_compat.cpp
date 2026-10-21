// Original synthetic fragment inputs through the production compatibility policy.
// Sirit and SPIRV-Cross use different SPIR-V enum headers: separate translation units.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#ifdef SOTC_SPIRV_EMITTER
#include <sirit/sirit.h>
std::vector<uint32_t> fragment_case(unsigned vertices) {
    Sirit::Module m(0x00010300);
    m.AddCapability(spv::Capability::Shader);
    if(vertices) {
        m.AddCapability(spv::Capability::FragmentBarycentricKHR);
        m.AddExtension("SPV_KHR_fragment_shader_barycentric");
    }
    m.SetMemoryModel(spv::AddressingModel::Logical,spv::MemoryModel::GLSL450);
    const auto f=m.TypeFloat(32),v=m.TypeVector(f,4),u=m.TypeUInt(32);
    const auto t=vertices ? m.TypeArray(v,m.Constant(u,3u)) : v;
    const auto input=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Input,t),spv::StorageClass::Input);
    const auto output=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output,v),spv::StorageClass::Output);
    m.Decorate(input,spv::Decoration::Location,0u);
    m.Decorate(output,spv::Decoration::Location,0u);
    if(vertices) m.Decorate(input,spv::Decoration::PerVertexKHR);
    const auto main=m.OpFunction(m.TypeVoid(),spv::FunctionControlMask::MaskNone,m.TypeFunction(m.TypeVoid()));
    const std::array interface{input,output};
    m.AddEntryPoint(spv::ExecutionModel::Fragment,main,"main",interface);
    m.AddExecutionMode(main,spv::ExecutionMode::OriginUpperLeft);
    m.AddLabel();
    if(!vertices) m.OpStore(output,m.OpLoad(v,input));
    for(unsigned i=0;i<3;++i) if(vertices & (1u<<i)) {
        const auto ptr=m.OpAccessChain(m.TypePointer(spv::StorageClass::Input,v),input,m.Constant(u,i));
        m.OpStore(output,m.OpLoad(v,ptr));
    }
    m.OpReturn();m.OpFunctionEnd();return m.Assemble();
}
#else
#include <spirv_cross/spirv_msl.hpp>
#include "gpu_msl_compat.h"
std::vector<uint32_t> fragment_case(unsigned vertices);
int main() {
    for(unsigned vertices : {0u,1u,2u,4u,3u}) {
        const auto original=fragment_case(vertices);
        assert(Sotc::MslCompatibleSpirv(original,false)==original);
        const auto adapted=Sotc::MslCompatibleSpirv(original,true);
        const bool exact=vertices==1;
        assert((adapted!=original)==exact);
        if(exact) {
            assert(!Metal::HasPerVertexInputs(adapted));
            bool flat=false;
            for(size_t i=5;i<adapted.size();i+=adapted[i]>>16) {
                const auto op=adapted[i]&0xffff;
                flat|=op==spv::OpDecorate && adapted[i+2]==spv::DecorationFlat;
                assert(op!=spv::OpDPdxFine && op!=spv::OpDPdyFine && op!=spv::OpFAdd && op!=spv::OpFMul);
            }
            assert(flat);
        }
        bool converted=false;
        try {
            spirv_cross::CompilerMSL c(adapted);auto o=c.get_msl_options();
            o.platform=spirv_cross::CompilerMSL::Options::macOS;o.set_msl_version(3,1);c.set_msl_options(o);
            const auto msl=c.compile();assert(!msl.empty());converted=true;
            if(exact) assert(msl.find("flat")!=std::string::npos);
        } catch(const spirv_cross::CompilerError& e) {
            assert(std::string(e.what()).find("PerVertexKHR decoration is not supported in MSL")!=std::string::npos);
        }
        assert(converted==(vertices==0 || exact));
    }
    puts("PASS: ordinary input unchanged; vertex-zero input converts exactly with Flat and no derivative arithmetic; vertex one/two/mixed modules preserved and explicitly rejected");
}
#endif
