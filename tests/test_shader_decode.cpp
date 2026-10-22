// Original synthetic GCN inputs through the installed decoder; no GPU or game.
#include <cassert>
#include <cstring>
#include <string>
#include <thread>
#include <sys/mman.h>
#include <unistd.h>
#include "shader_recompiler/frontend/decode.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/frontend/copy_shader.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/runtime_info.h"
#include "shader_recompiler/profile.h"

int main(int argc, char** argv) {
    using namespace Shader;
    using namespace Shader::Gcn;
    const std::string mode = argc > 1 ? argv[1] : "valid";
    const auto page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void* pages = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(pages != MAP_FAILED);
    assert(mprotect(static_cast<char*>(pages) + page, page, PROT_NONE) == 0);
    auto* last = reinterpret_cast<u32*>(static_cast<char*>(pages) + page - sizeof(u32));
    *last = 0xbf810000u; // synthetic s_endpgm
    SotcShaderDecode::Scope outer("fixture", 0x1234, 2, last);
    GcnDecodeContext decoder;
    if (mode == "read64") {
        GcnCodeSlice slice(last, last + 1);
        (void)slice.readu64();
    } else if (mode == "read32") {
        GcnCodeSlice slice(last, last + 1);
        (void)slice.readu32();
        (void)slice.readu32();
    } else if (mode == "peek") {
        GcnCodeSlice slice(last, last + 1);
        (void)slice.at(1);
    } else if (mode == "literal" || mode == "wide" || mode == "sdwa" || mode == "dpp" || mode == "illegal") {
        *last = mode == "literal" ? 0xbeeb03ffu : mode == "wide" ? 0xd0000000u :
                mode == "sdwa" ? 0x7e0002f9u : mode == "dpp" ? 0x7e0002fau : 0xffffffffu;
        GcnCodeSlice slice(last, last + 1);
        (void)decoder.decodeInstruction(slice);
    } else if (mode == "reserved") {
        // Build a reserved SMRD opcode from fields, independent of any game.
        u32 words[] = {0xbf800000u, u32(InstEncoding::SMRD) | (15u << 22u)};
        GcnCodeSlice slice(words, words + 2);
        (void)decoder.decodeInstruction(slice);
        (void)decoder.decodeInstruction(slice);
    } else if (mode == "fetch") {
        u32 words[] = {u32(InstEncoding::SMRD) | (15u << 22u)};
        u32 user[16]{};
        const auto* pointer = words;
        std::memcpy(user, &pointer, sizeof(pointer));
        Info info{};
        info.has_fetch_shader = true;
        info.fetch_shader_sgpr_base = 0;
        info.user_data = user;
        info.pgm_hash = 0x5678;
        info.hw_stage = HwStage::Vertex;
        (void)ParseFetchShader(info);
    } else if (mode == "main-empty") {
        Info info{};
        info.pgm_hash = 0x5678;
        info.hw_stage = HwStage::Compute;
        RuntimeInfo runtime{};
        Profile profile{};
        Pools pools;
        (void)TranslateProgram(std::span<const u32>(last + 1, size_t(0)), pools,
                               info, runtime, profile);
    } else if (mode == "copy-empty") {
        (void)ParseCopyShader(std::span<const u32>(last + 1, size_t(0)));
    } else {
        assert(mode == "valid");
        u32 words[] = {0xbf800000u, 0xbf810000u};
        GcnCodeSlice slice(words, words + 2);
        assert(decoder.decodeInstruction(slice).opcode == Opcode::S_NOP);
        assert(slice.Offset() == 1 && slice.Remaining() == 1);
        assert(decoder.decodeInstruction(slice).opcode == Opcode::S_ENDPGM);
        assert(slice.atEnd() && slice.Offset() == 2);
        u32 aligned[] = {0, 0x11223344u, 0x55667788u};
        GcnCodeSlice odd(aligned + 1, aligned + 3);
        assert(odd.readu64() == 0x5566778811223344ull && odd.atEnd());
        u32 literal[] = {0xbeeb03ffu, 0x11223344u};
        GcnCodeSlice constants(literal, literal + 2);
        assert(decoder.decodeInstruction(constants).length == 8 && constants.atEnd());
        u32 scalar[] = {u32(InstEncoding::SMRD) | (1u << 8u)};
        GcnCodeSlice smrd(scalar, scalar + 1);
        assert(decoder.decodeInstruction(smrd).opcode == Opcode::S_LOAD_DWORD && smrd.atEnd());
        assert(u32(OpcodeMap::OP_MAP_SMRD) + 15 == 1601);
        const auto format = InstructionFormat(InstEncoding::SMRD, 15);
        assert(format.src_type == ScalarType::Undefined || format.dst_type == ScalarType::Undefined);
        {
            SotcShaderDecode::Scope inner("nested", 0x5678, 1, nullptr);
            assert(SotcShaderDecode::current.shader == 0x5678);
        }
        assert(SotcShaderDecode::current.shader == 0x1234);
        std::thread other([] {
            assert(SotcShaderDecode::current.shader == 0);
            SotcShaderDecode::Scope thread("thread", 0x9abc, 0, nullptr);
            assert(SotcShaderDecode::current.shader == 0x9abc);
        });
        other.join();
        assert(SotcShaderDecode::current.shader == 0x1234);
        munmap(pages, page * 2);
        return 0;
    }
    return 99; // Every malformed case must stop through the real assertion handler.
}
