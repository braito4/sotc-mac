// Synthetic checked runtime hooks with the actual shader registers and wrapper.
#include "gpu_shader_lookup.h"
#include <cassert>
#include <cstring>

alignas(256) static std::array<std::byte, 4096> backing;
static std::size_t mapped = backing.size();
static bool deny_reads;
static unsigned reads;
static const auto base = reinterpret_cast<std::uintptr_t>(backing.data());

extern "C" std::uint64_t runtime_memory_clamp(std::uintptr_t address, std::uint64_t bytes) {
    if(address < base || address - base >= mapped) return 0;
    return std::min(bytes, std::uint64_t(mapped - (address - base)));
}
extern "C" int runtime_memory_is_mapped(std::uintptr_t address, std::uint64_t bytes) {
    return !deny_reads && address >= base && address-base <= mapped && bytes <= mapped-(address-base);
}
extern "C" void runtime_memory_read_backing(std::uintptr_t address, void* out, std::uint64_t bytes) {
    assert(runtime_memory_is_mapped(address, bytes));
    ++reads;
    std::memcpy(out, backing.data()+(address-base), bytes);
}

int main(int argc, char**) {
    AmdGpu::ShaderProgram program{};
    program.address = base >> 8;
    assert(reinterpret_cast<std::uintptr_t>(program.Address<const u32*>()) == base);
    program.user_data[7] = 0x11223344;
    if(argc > 1) {
        Sotc::RequireShaderParams(program, "synthetic-required");
        return 99;
    }
    // Unavailable speculative work must produce no substitute parameters.
    assert(!Sotc::TryShaderParams(program, "synthetic-prepare", false));
    AmdGpu::BinaryInfo info{};
    info.signature = info.signature_ref;
    info.length = 32;
    info.shader_hash = 0xfedcba9876543210;
    const u32 head[] = {0xBEEB03FF, 7};
    std::memcpy(backing.data(), head, sizeof(head));
    std::memcpy(backing.data()+64, &info, sizeof(info));
    reads = 0;
    const auto valid = Sotc::TryShaderParams(program, "synthetic-graphics", true);
    assert(valid && valid->hash == info.shader_hash && valid->code.data() == program.Address<const u32*>());
    assert(valid->code.size() == 8 && valid->user_data[7] == program.user_data[7] && reads == 2);
    deny_reads = true;
    reads = 0;
    assert(!Sotc::TryShaderParams(program, "synthetic-revalidate", false) && reads == 0);
    deny_reads = false;
    mapped = 7;
    assert(!Sotc::TryShaderParams(program, "synthetic-truncated", false));
    program.address = 0;
    assert(!Sotc::TryShaderParams(program, "synthetic-unmapped", false));
    std::puts("PASS: actual shader registers and runtime lookup preserve real hash/code/user data, reject unavailable speculative reads, and never read outside synthetic mappings");
}
