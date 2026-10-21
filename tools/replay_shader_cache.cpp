// CPU-only SPIRV-Cross replay. No renderer/device or game code is linked.
#include "gpu_msl_compat.h"
#include <spirv_cross/spirv_msl.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if(argc != 3) return 64;
    try {
        std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
        const auto size = file.tellg();
        if(!file || size < 20 || size > 8*1024*1024 || size % 4) throw std::runtime_error("invalid SPIR-V size");
        std::vector<u32> original(static_cast<size_t>(size)/4);
        file.seekg(0);
        if(!file.read(reinterpret_cast<char*>(original.data()), size)) throw std::runtime_error("SPIR-V read failed");
        if(original[0] != 0x07230203 || original[1] < 0x10000 || original[1] > 0x10600 ||
           !original[3] || original[3] > 1000000 || original[4]) throw std::runtime_error("invalid SPIR-V header");
        for(size_t i=5;i<original.size();) {
            const auto count=original[i] >> 16;
            if(!count || count > original.size()-i) throw std::runtime_error("truncated SPIR-V instruction");
            i+=count;
        }
        const auto code=Sotc::MslCompatibleSpirv(original, std::string_view(argv[2]) == "1");
        // No MSL or derived SPIR-V is written. The result describes conversion only.
        std::cout << (code != original) << '\n';
        spirv_cross::CompilerMSL compiler(code);
        const auto entries=compiler.get_entry_points_and_stages();
        if(entries.size() != 1) throw std::runtime_error("expected exactly one entry point");
        std::cout << unsigned(entries[0].execution_model) << '\n';
        auto options=compiler.get_msl_options();
        options.platform=spirv_cross::CompilerMSL::Options::macOS;
        options.set_msl_version(3,1);
        options.argument_buffers=false;
        options.texture_1D_as_2D=true;
        compiler.set_msl_options(options);
        if(compiler.compile().empty()) throw std::runtime_error("empty MSL output");
        std::cout << "ok\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
