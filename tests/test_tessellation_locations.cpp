// Original synthetic SPIR-V interface regression; no GPU or game shader.
#include <cassert>
#include <cstdio>
#include <set>
#include <unordered_map>
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "shader_recompiler/runtime_info.h"

int main() {
    using namespace Shader;
    using namespace Shader::Backend::SPIRV;
    unsigned checked = 0;
    for (unsigned pattern=0; pattern<5; ++pattern) {
        HwFragmentRuntimeInfo info{};
        info.num_inputs = pattern == 0 ? 0 : 32;
        std::set<u32> expected;
        for (u32 i=0; i<info.num_inputs; ++i) {
            auto& input = info.inputs[i];
            input.param_index = pattern == 1 ? 0 : pattern == 2 ? i%5 : i;
            input.is_default = pattern == 3 || (pattern == 4 && i%3 == 0);
            input.is_flat = pattern == 4 && i%2 == 0;
            if (!input.IsDefault()) expected.insert(input.param_index);
        }
        for (const auto type : {AuxShaderType::RectListTCS, AuxShaderType::QuadListTCS,
                                AuxShaderType::PassthroughTES}) {
            const auto words = EmitAuxilaryTessShader(type, info);
            assert(words.size() > 5 && words[0] == 0x07230203);
            std::unordered_map<u32,u32> location, storage;
            for (size_t i=5; i<words.size();) {
                const u32 length = words[i] >> 16, opcode = words[i] & 0xffff;
                assert(length && length <= words.size()-i);
                // OpDecorate Location and OpVariable storage class.
                if (opcode == 71 && length == 4 && words[i+2] == 30)
                    assert(location.emplace(words[i+1], words[i+3]).second);
                if (opcode == 59 && length >= 4) storage.emplace(words[i+2], words[i+3]);
                i += length;
            }
            std::set<u32> inputs, outputs;
            for (const auto& [id, index] : location) {
                assert(storage.contains(id));
                auto& found = storage.at(id) == 1 ? inputs : outputs;
                assert(storage.at(id) == 1 || storage.at(id) == 3);
                if (!found.insert(index).second) {
                    std::fprintf(stderr,"Duplicate auxiliary tessellation location %u\n",index);
                    return 1;
                }
            }
            assert(inputs == expected && outputs == expected);
            ++checked;
        }
    }
    std::printf("PASS: %u actual auxiliary SPIR-V modules: unique matching input/output locations, repeated/sparse/default/flat/full/empty interfaces; no GPU initialized\n",checked);
}
