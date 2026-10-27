// Synthetic checked runtime hooks with the actual shader registers and wrapper.
#include "gpu_shader_lookup.h"
#include <cassert>
#include <cstring>
[[maybe_unused]] static void mutate_history_backing();
#ifdef SOTC_TEST_HISTORY_HOOK
extern "C" void sotc_cpu_field_trace_on_gpu_assert() {
    std::fputs("SYNTHETIC_CPU_HISTORY\n",stderr);
    mutate_history_backing();
}
#endif

alignas(256) static std::array<std::byte, 65564> backing;
static std::size_t mapped = 4096;
static bool deny_reads;
static unsigned reads;
static const auto base = reinterpret_cast<std::uintptr_t>(backing.data());
static AmdGpu::ShaderProgram* tracked_program;
[[maybe_unused]] static void mutate_history_backing() {
    if(std::getenv("SOTC_TEST_MUTATE_HISTORY"))backing.fill(std::byte{0x7f});
    if(std::getenv("SOTC_TEST_MUTATE_DESCRIPTOR"))tracked_program->address=(base+256)>>8;
}

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
    if(std::getenv("SOTC_TEST_MUTATE_PROGRAM") && reads==1)
        tracked_program->address=(base+256)>>8;
}

int main(int argc, char** argv) {
    AmdGpu::ShaderProgram program{};
    tracked_program=&program;
    program.address = base >> 8;
    assert(reinterpret_cast<std::uintptr_t>(program.Address<const u32*>()) == base);
    program.user_data[7] = 0x11223344;
    if(argc>1 && std::string_view(argv[1])=="compute-required") {
        AmdGpu::ComputeProgram compute{};
        compute.dispatch_initiator=1;compute.dim_x=1;compute.address=base>>8;
        Sotc::RequireShaderParams(compute,"compute");
        return 99;
    }
    if(argc>1 && std::string_view(argv[1])=="association-required") {
        AmdGpu::BinaryInfo info{};info.signature=info.signature_ref;info.length=32;info.shader_hash=0x1234;
        const u32 wrong_hint[]={0xBEEB03FF,15};
        std::memcpy(backing.data(),wrong_hint,sizeof(wrong_hint));
        std::memcpy(backing.data()+512,&info,sizeof(info));
        Sotc::RequireShaderParams(program,"synthetic-conflicting-hint");
        return 99;
    }
    if(argc>1 && std::string_view(argv[1])=="indexed-fail") {
        SotcShaderOriginCache::Observe(base,0x800,base>>8,
            std::uint64_t(4)<<32|0xbeeb03ffu,1,7,10000,
            std::uint64_t(8)<<32|0xc0027600u,4096);
        Sotc::RequireShaderParams(program,"synthetic-indexed-required");
        return 99;
    }
    if(argc>1 && std::string_view(argv[1]).starts_with("fallback-")) {
        const std::string_view mode=argv[1];
        AmdGpu::BinaryInfo info{};info.signature=info.signature_ref;info.length=32;
        AmdGpu::ComputeProgram compute{};compute.address=base>>8;
        const unsigned attempts=mode=="fallback-cap" || mode=="fallback-types" ? 12 :
            mode=="fallback-duplicate" ? 20 : 1;
        for(unsigned i=0;i<attempts;++i) {
            info.shader_hash=mode=="fallback-duplicate" ? 0x1234 : 0x1234+i;
            std::memcpy(backing.data()+512,&info,sizeof(info));
            reads=0;
            const auto params=mode=="fallback-types" && i>=6 ?
                Sotc::TryShaderParams(compute,"synthetic-compute-fallback",false) :
                Sotc::TryShaderParams(program,"synthetic-graphics-fallback",false);
            assert(!params && reads==2);
        }
        // An unrelated later exit and source reuse must not remove the earlier
        // owned scan or force a fresh guest read during evidence writing.
        backing.fill(std::byte{0x7f});
        std::_Exit(mode=="fallback-exit" ? 53 : 0);
    }
    if(argc > 1) {
        if(std::string_view(argv[1])=="changed-valid") {
            AmdGpu::BinaryInfo info{};info.signature=info.signature_ref;
            info.length=32;info.shader_hash=0x12345678;
            const u32 head[]={0xBEEB03FF,7};
            std::memcpy(backing.data(),head,sizeof(head));
            std::memcpy(backing.data()+64,&info,sizeof(info));
            const auto valid=Sotc::TryShaderParams(program,"synthetic-changed-descriptor",true);
            assert(valid && valid->hash==info.shader_hash && valid->code.size()==8);
            assert(reinterpret_cast<std::uintptr_t>(program.Address<const u32*>())==base+256);
            assert(valid->code.data()==reinterpret_cast<const u32*>(base));
            std::puts("PASS: returned code and metadata use the same captured address despite descriptor mutation");
            return 0;
        }
        if (std::string_view(argv[1])=="large") mapped=backing.size();
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
