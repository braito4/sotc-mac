// Original synthetic IR: guarded strided reads, mixed uses and arithmetic edges.
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>
#include <memory>
#include "common/serdes.h"
#include "gpu_buffer_read_analysis.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "shader_recompiler/profile.h"

using namespace Shader;
using SotcBufferRead::Bound;
constexpr u64 MiB = u64{1} << 20;

struct Fixture {
    Common::ObjectPool<IR::Inst> pool;
    IR::Block header{pool}, body{pool}, after{pool};
    Info info{};
    IR::Program program{info};
    IR::IREmitter head{header}, load{body}, tail{after};
    IR::U32 index, limit;

    explicit Fixture(bool reversed = false, bool signed_compare = false, bool inclusive = false) {
        info.buffers.push_back({.buffer_type = BufferType::Guest});
        index = head.GetAttributeU32(IR::Attribute::LocalInvocationIndex);
        limit = head.ReadConst(head.CompositeConstruct(head.Imm32(1u), head.Imm32(0u)), head.Imm32(0u));
        limit.Inst()->SetFlags<u32>(7);
        const auto cond = inclusive ? head.ILessThanEqual(index, limit, signed_compare)
                          : reversed ? head.IGreaterThan(limit, index, signed_compare)
                                     : head.ILessThan(index, limit, signed_compare);
        using T = IR::AbstractSyntaxNode::Type;
        program.syntax_list = {
            {.data = {.block = &header}, .type = T::Block},
            {.data = {.if_node = {cond, &body, &after}}, .type = T::If},
            {.data = {.block = &body}, .type = T::Block},
            {.data = {.end_if = {&after}}, .type = T::EndIf},
            {.data = {.block = &after}, .type = T::Block},
            {.type = T::Return},
        };
        program.blocks = {&header, &body, &after};
        program.post_order_blocks = {&after, &body, &header};
    }

    IR::U32 Address(u32 stride, u32 offset) {
        return load.IAdd(load.IMul(index, load.Imm32(stride)), load.Imm32(offset));
    }
    void Read(u32 stride = 20, u32 offset = 3, unsigned width = 4) {
        (void)load.LoadBufferU32(width, load.Imm32(0u), Address(stride, offset), {});
    }
    Bound Analyze() {
        SotcBufferRead::Analyze(program);
        return info.buffers[0].read_bound;
    }
};

// Compile the actual production UnifiedRange method against a CPU mapping double.
// No backing storage of GiB size and no Vulkan/Metal device is created.
struct Buffer { u64 size; u64 SizeBytes() const { return size; } };
inline int runtime_memory_pool_phys(VAddr address, u64* phys, std::uintptr_t* end) {
    if (address < 0x100000 || address >= 0x100000 + 4 * 1024 * MiB) return 0;
    *phys = 2 * 1024 * MiB + address - 0x100000;
    *end = 0x100000 + 4 * 1024 * MiB;
    return 1;
}
struct BufferCache {
    static constexpr u64 POOL_WINDOW_STEP = 3 * 1024 * MiB;
    std::array<std::unique_ptr<Buffer>, 2> pool_windows = {
        std::make_unique<Buffer>(Buffer{3584 * MiB}),
        std::make_unique<Buffer>(Buffer{3520 * MiB})};
    unsigned page_updates{};
    void UnifiedPageTable(VAddr, u64) { ++page_updates; }
    std::size_t PoolWindow(u64 phys) const { return phys / POOL_WINDOW_STEP; }
    std::optional<std::pair<const Buffer*, u64>> UnifiedRange(VAddr, u64);
};
#include "unified_range_fixture.inc"

int main() {
    const u64 declared = (u64{1} << 32) - 32;
    std::array<u32, 16> flat{};
    for (bool reversed : {false, true}) {
        Fixture f(reversed); for (u32 offset : {3u, 11u, 19u}) f.Read(20, offset);
        const auto bound = f.Analyze(); assert(bound.scale == 80 && bound.end_add == 92);
        for (u32 limit = 0; limit < 512; ++limit) {
            flat[7] = limit;
            const auto narrowed = SotcBufferRead::BindingSize(bound, flat, declared, false, false);
            assert(narrowed < 64 * MiB && narrowed >= 1);
            for (u32 index = 0; index < limit; ++index) for (u32 offset : {3u, 11u, 19u}) {
                const u64 address = u64(index) * 80 + u64(offset) * 4;
                assert(address + 16 <= narrowed); // Includes every component of each vector load.
            }
        }
        flat[7] = 512;
        assert(SotcBufferRead::BindingSize(bound, flat, declared, true, false) == declared);
        assert(SotcBufferRead::BindingSize(bound, flat, declared, false, true) == declared);
        assert(SotcBufferRead::BindingSize(bound, {}, declared, false, false) == declared);
        assert(SotcBufferRead::BindingSize(bound, flat, 32, false, false) == 32);
        BufferCache cache;
        const u64 old_size = 3072 * MiB - 4096;
        assert(!cache.UnifiedRange(0x100000, old_size)); // The original full copy is GiB sized.
        const u64 reduced = SotcBufferRead::BindingSize(bound, flat, old_size, false, false);
        const auto range = cache.UnifiedRange(0x100000, reduced);
        assert(range && range->first == cache.pool_windows[0].get());
        assert(range->second == 2 * 1024 * MiB && reduced == 40972);
        std::printf("guarded synthetic binding: %llu -> %llu bytes, original pool window reused\n",
                    (unsigned long long)old_size, (unsigned long long)reduced);
        // Snapshot values are read afresh for every draw; no compiled-in limit.
        flat[7] = 1024; assert(SotcBufferRead::BindingSize(bound, flat, old_size, false, false) > reduced);
        flat[7] = std::numeric_limits<u32>::max();
        assert(SotcBufferRead::BindingSize(bound, flat, declared, false, false) == declared);
    }
    for (unsigned kind = 0; kind < 11; ++kind) {
        Fixture f(false, kind == 0, kind == 1); f.Read();
        if (kind == 2) (void)f.tail.LoadBufferU32(1, f.tail.Imm32(0u), f.tail.Imm32(0u), {});
        if (kind == 3) f.info.buffers[0].is_written = true;
        if (kind == 4) f.info.buffers[0].is_formatted = true;
        if (kind == 5) f.limit.Inst()->SetFlags<u32>(0); // Unresolved SRT offset is not a proof.
        if (kind == 6) f.load.StoreBufferU32(1, f.load.Imm32(0u), f.load.Imm32(0u), f.load.Imm32(1u), {});
        if (kind == 7) { // Non-affine address.
            (void)f.load.LoadBufferU32(1, f.load.Imm32(0u), f.load.IMul(f.index, f.index), {});
        }
        if (kind == 8) f.program.blocks.push_back(&f.body); // A duplicate vector entry is harmless.
        if (kind == 9) { // A block absent from the syntax could hide another access.
            f.program.syntax_list.erase(f.program.syntax_list.begin() + 4);
        }
        if (kind == 10) { // An unguarded path must invalidate the entire resource's proof.
            (void)f.head.LoadBufferU32(1, f.head.Imm32(0u), f.head.Imm32(999u), {});
        }
        const auto bound = f.Analyze();
        assert(bool(bound.scale) == (kind == 8));
    }
    { // A later division cannot hide an earlier U32 multiplication overflow.
        Fixture f;
        auto address = f.load.ShiftRightLogical(f.load.IMul(f.index, f.load.Imm32(1024u)), f.load.Imm32(2u));
        (void)f.load.LoadBufferU32(1, f.load.Imm32(0u), address, {});
        const auto bound = f.Analyze(); assert(bound.scale == 1024);
        flat[7] = (1u << 22) + 1;
        assert(SotcBufferRead::BindingSize(bound, flat, declared, false, false) == declared);
    }
    { // Read outside a supported guard (inside an unrelated one) must stay conservative.
        Fixture f; f.Read();
        auto other = f.head.GetAttributeU32(IR::Attribute::WorkgroupId);
        (void)f.load.LoadBufferU32(1, f.load.Imm32(0u), other, {});
        assert(!f.Analyze().scale);
    }
    for (unsigned kind = 0; kind < 10; ++kind) {
        Fixture f;
        const auto handle = f.load.Imm32(0u), address = f.Address(16, 7);
        unsigned unit = 4, width = 4;
        if (kind == 0) { (void)f.load.LoadBufferU8(handle, address, {}); unit = width = 1; }
        else if (kind == 1) { (void)f.load.LoadBufferU16(handle, address, {}); unit = width = 2; }
        else if (kind == 2) { (void)f.load.LoadBufferU64(handle, address, {}); unit = width = 8; }
        else if (kind < 6) {
            width = (kind - 2) * 4;
            (void)f.load.LoadBufferU32(kind - 2, handle, address, {});
        } else {
            width = (kind - 5) * 4;
            (void)f.load.LoadBufferF32(kind - 5, handle, address, {});
        }
        const auto bound = f.Analyze();
        flat[7] = 100;
        assert(SotcBufferRead::ByteEnd(bound, flat) == (99 * 16 + 7) * unit + width);
    }
    { // A different guard limit cannot be merged into an existing proof.
        Fixture f; f.Read();
        auto second = f.head.ReadConst(f.head.CompositeConstruct(f.head.Imm32(1u), f.head.Imm32(0u)), f.head.Imm32(0u));
        second.Inst()->SetFlags<u32>(8);
        auto cond = f.head.ILessThan(f.index, second, false);
        IR::Block another(f.pool); IR::IREmitter ir(another);
        (void)ir.LoadBufferU32(1, ir.Imm32(0u), f.index, {});
        using T = IR::AbstractSyntaxNode::Type;
        f.program.syntax_list.insert(f.program.syntax_list.end() - 1, {
            {.data = {.if_node = {cond, &another, &f.after}}, .type = T::If},
            {.data = {.block = &another}, .type = T::Block},
            {.data = {.end_if = {&f.after}}, .type = T::EndIf},
        });
        f.program.blocks.push_back(&another);
        assert(!f.Analyze().scale);
    }
    { // The host uses the same thread-local draw snapshot as the shader's flat buffer.
        Fixture f; f.Read(); const auto bound = f.Analyze();
        f.info.flattened_ud_buf.assign(flat.begin(), flat.end());
        f.info.flattened_ud_buf[7] = 12;
        flat[7] = 100;
        Info::ud_snapshots[0] = {.info = &f.info, .flat = flat.data(), .flat_size = u32(flat.size())};
        Info::num_ud_snapshots = 1;
        assert(SotcBufferRead::ByteEnd(bound, f.info.FlatUserData()) == 99 * 80 + 28);
        Info::num_ud_snapshots = 0;
        assert(SotcBufferRead::ByteEnd(bound, f.info.FlatUserData()) == 11 * 80 + 28);
    }
    { // Exercise the actual production collection call and metadata serializer.
        Fixture f; f.Read();
        f.info.hw_stage = HwStage::Compute; f.info.sw_stage = SwStage::Compute;
        Profile profile{};
        Optimization::CollectShaderInfoPass(f.program, profile);
        assert(f.info.buffers[0].read_bound.scale == 80);
        f.info.flattened_ud_buf.assign(flat.begin(), flat.end());
        f.info.flattened_ud_buf[7] = 64;
        Serialization::Archive writer;
        f.info.Serialize(writer);
        Serialization::Archive reader(writer.TakeOff());
        Info restored{};
        assert(restored.Deserialize(reader));
        const auto& bound = restored.buffers[0].read_bound;
        assert(bound.limit_dword == 7 && bound.scale == 80 && bound.end_add == 28);
        assert(SotcBufferRead::ByteEnd(bound, restored.FlatUserData()) == 63 * 80 + 28);
    }
    { // A shared expression graph cannot trigger exponential compile-time work.
        Fixture f; auto address = f.index;
        for (unsigned i = 0; i < 20; ++i) address = f.load.IAdd(address, address);
        (void)f.load.LoadBufferU32(1, f.load.Imm32(0u), address, {});
        assert(!f.Analyze().scale);
    }
    puts("PASS: final structured IR proves every read; unguarded/written/formatted/unknown/overflow paths retain original bounds; no GPU initialized");
}
