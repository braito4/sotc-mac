// Synthetic fragment interfaces through the installed SPIRV-Cross MSL compiler.
// No game shader, driver, device or pixel validation. Exit 2 reproduces the known
// raw-per-vertex limitation; exit 0 means this compiler can convert both cases.
#include <array>
#include <cstdio>
#include <string>
#include <cstdint>
#include <vector>
#ifdef SOTC_SPIRV_EMITTER
#include <sirit/sirit.h>

std::vector<uint32_t> synthetic_fragment(bool per_vertex) {
    Sirit::Module m(0x00010300);
    m.AddCapability(spv::Capability::Shader);
    if (per_vertex) {
        m.AddCapability(spv::Capability::FragmentBarycentricKHR);
        m.AddExtension("SPV_KHR_fragment_shader_barycentric");
    }
    m.SetMemoryModel(spv::AddressingModel::Logical,spv::MemoryModel::GLSL450);
    const auto f=m.TypeFloat(32),v=m.TypeVector(f,4),u=m.TypeUInt(32);
    const auto three=m.Constant(u,3u),one=m.Constant(u,1u);
    const auto input_type=per_vertex ? m.TypeArray(v,three) : v;
    const auto input=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Input,input_type),spv::StorageClass::Input);
    const auto output=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output,v),spv::StorageClass::Output);
    m.Decorate(input,spv::Decoration::Location,0u);
    m.Decorate(output,spv::Decoration::Location,0u);
    if (per_vertex) m.Decorate(input,spv::Decoration::PerVertexKHR);
    const auto main=m.OpFunction(m.TypeVoid(),spv::FunctionControlMask::MaskNone,m.TypeFunction(m.TypeVoid()));
    const std::array interface{input,output};
    m.AddEntryPoint(spv::ExecutionModel::Fragment,main,"main",interface);
    m.AddExecutionMode(main,spv::ExecutionMode::OriginUpperLeft);
    m.AddLabel();
    // Reading vertex one is intentional: an ordinary interpolated input cannot
    // replace this interface while preserving access to three separate vertices.
    const auto pointer=per_vertex ? m.OpAccessChain(m.TypePointer(spv::StorageClass::Input,v),input,one) : input;
    m.OpStore(output,m.OpLoad(v,pointer));
    m.OpReturn(); m.OpFunctionEnd();
    return m.Assemble();
}
#else
// SPIRV-Cross and Sirit ship different SPIR-V C++ enum headers. Keep their
// headers in separate translation units, as the actual renderer does.
#include <spirv_msl.hpp>
std::vector<uint32_t> synthetic_fragment(bool per_vertex);
int main() {
    for (bool per_vertex : {false,true}) {
        try {
            spirv_cross::CompilerMSL compiler(synthetic_fragment(per_vertex));
            auto options=compiler.get_msl_options();
            options.platform=spirv_cross::CompilerMSL::Options::macOS;
            options.msl_version=spirv_cross::CompilerMSL::Options::make_msl_version(2,3);
            compiler.set_msl_options(options);
            if (compiler.compile().empty()) return 1;
            std::printf("PASS: synthetic %s fragment interface converts to MSL\n",per_vertex ? "raw per-vertex" : "ordinary");
        } catch (const spirv_cross::CompilerError& error) {
            if (per_vertex && std::string(error.what()).find("PerVertexKHR decoration is not supported in MSL")!=std::string::npos) {
                std::puts("KNOWN_LIMIT: installed SPIRV-Cross rejects the synthetic raw per-vertex fragment interface; no game or GPU used");
                return 2;
            }
            std::fprintf(stderr,"Unexpected fragment conversion failure: %s\n",error.what());
            return 1;
        }
    }
    return 0;
}
#endif
