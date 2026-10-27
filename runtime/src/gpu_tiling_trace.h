// SPDX-License-Identifier: GPL-2.0-or-later
// CPU-owned tiling parameters only. No guest reads, GPU reads, or dispatch changes.
#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
namespace SotcTilingTrace {
using U64 = std::uint64_t;
struct Mip { U64 size{}, pitch{}, height{}, offset{}; };
struct Binding { U64 buffer{}, offset{}, range{}; };
struct Record {
    U64 seq{}, ns{}, pipeline{}, groups{}, bits{}, levels{}, flags{}, elements{}, invocations{};
    U64 bank_swizzle{}, micro_tiled_mips{}, tick{};
    bool tiler{};
    std::array<Binding, 3> bindings{};
    std::array<Mip, 16> mips{};
};
// Flags are CPU consistency checks, not proof of an invalid native access or stall cause.
inline void Check(Record& r) {
    r.flags = 0; r.elements = 0; r.invocations = r.groups * 64;
    if (!r.levels || r.levels > 16) r.flags |= 1;
    if (!r.bits || r.bits % 8) { r.flags |= 2; return; }
    const U64 bytes = r.bits / 8;
    for (U64 i = 0; i < r.levels && i < 16; ++i) {
        const auto& m = r.mips[i];
        r.elements += m.size / bytes;
        if (!m.size || !m.pitch || !m.height) r.flags |= 4;
        if (m.size % bytes) r.flags |= 8;
        if (m.offset > r.bindings[0].range || m.size > r.bindings[0].range - m.offset) r.flags |= 16;
    }
    if (r.invocations > r.elements) r.flags |= 32;
    if (r.invocations > r.bindings[1].range / bytes) r.flags |= 64;
}
struct Reuse { U64 seq{}, parameter_seq{}, buffer{}, offset{}, size{}, writer_tick{}, parameter_tick{}, completed{}; };
class Journal {
    std::mutex mutex;
    std::deque<Record> tail, first_invalid;
    U64 total{}, invalid{}, maps{}, overlaps{}, pending{};
    std::deque<Reuse> reuse_tail, first_pending;
public:
    void Map(U64 buffer, U64 offset, U64 size, U64 tick, U64 completed) {
        std::lock_guard lock{mutex}; ++maps;
        for (const auto& r : tail) {
            const auto& b = r.bindings[2];
            if (b.buffer != buffer || !size || !b.range) continue;
            const bool overlap = offset >= b.offset ? offset-b.offset < b.range : b.offset-offset < size;
            if (!overlap) continue;
            Reuse e{++overlaps,r.seq,buffer,offset,size,tick,r.tick,completed};
            if (reuse_tail.size()==128) reuse_tail.pop_front();
            reuse_tail.push_back(e);
            if (r.tick > completed) { ++pending; if (first_pending.size()<16) first_pending.push_back(e); }
        }
    }
    void Add(Record record) {
        Check(record);
        record.ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        std::lock_guard lock{mutex}; record.seq = ++total;
        if (record.flags) { ++invalid; if (first_invalid.size() < 16) first_invalid.push_back(record); }
        if (tail.size() == 128) tail.pop_front();
        tail.push_back(record);
    }
    void Report(std::FILE* out) {
        std::deque<Record> recent, first; std::deque<Reuse> reuse, early; U64 count, bad, map_count, overlap_count, pending_count;
        { std::lock_guard lock{mutex}; recent = tail; first = first_invalid; count = total; bad = invalid; reuse=reuse_tail; early=first_pending; map_count=maps; overlap_count=overlaps; pending_count=pending; }
        std::fprintf(out, "GPU tiling trace: total=%llu retained=%zu omitted=%llu invalid=%llu first_invalid=%zu; CPU parameters, pipeline handles reused, no exact dispatch association\n",
            (unsigned long long)count, recent.size(), (unsigned long long)(count-recent.size()),
            (unsigned long long)bad, first.size());
        std::fprintf(out,"GPU parameter reuse: maps=%llu overlaps=%llu retained=%zu pending=%llu first_pending=%zu; tracked parameters limited to tiling tail; map intent is not a proven write or GPU lifetime violation\n",
            (unsigned long long)map_count,(unsigned long long)overlap_count,reuse.size(),(unsigned long long)pending_count,early.size());
        auto emit_reuse=[out](const Reuse& e,const char* set) {
            std::fprintf(out,"GPU parameter reuse: set=%s seq=%llu parameter_seq=%llu buffer=%#llx offset=%llu size=%llu writer_tick=%llu parameter_tick=%llu completed=%llu\n",set,
                (unsigned long long)e.seq,(unsigned long long)e.parameter_seq,(unsigned long long)e.buffer,(unsigned long long)e.offset,(unsigned long long)e.size,
                (unsigned long long)e.writer_tick,(unsigned long long)e.parameter_tick,(unsigned long long)e.completed);
        };
        for (const auto& e:early) emit_reuse(e,"first_pending");
        for (const auto& e:reuse) emit_reuse(e,"tail");
        auto emit = [out](const Record& r, const char* set) {
            std::fprintf(out, "GPU tiling trace: set=%s seq=%llu ns=%llu pipeline=%#llx tiler=%u groups=%llu bits=%llu levels=%llu flags=%llu elements=%llu invocations=%llu bank_swizzle=%llu micro_tiled_mips=%llu tick=%llu\n", set,
                (unsigned long long)r.seq, (unsigned long long)r.ns, (unsigned long long)r.pipeline,
                unsigned(r.tiler), (unsigned long long)r.groups, (unsigned long long)r.bits,
                (unsigned long long)r.levels, (unsigned long long)r.flags,
                (unsigned long long)r.elements, (unsigned long long)r.invocations,
                (unsigned long long)r.bank_swizzle, (unsigned long long)r.micro_tiled_mips, (unsigned long long)r.tick);
            for (unsigned i=0; i<3; ++i) { const auto& b=r.bindings[i];
                std::fprintf(out,"GPU tiling trace: seq=%llu binding=%u buffer=%#llx offset=%llu range=%llu\n",
                    (unsigned long long)r.seq,i,(unsigned long long)b.buffer,(unsigned long long)b.offset,(unsigned long long)b.range); }
            for (unsigned i=0; i<r.levels && i<16; ++i) { const auto& m=r.mips[i];
                std::fprintf(out,"GPU tiling trace: seq=%llu mip=%u size=%llu pitch=%llu height=%llu offset=%llu\n",
                    (unsigned long long)r.seq,i,(unsigned long long)m.size,(unsigned long long)m.pitch,(unsigned long long)m.height,(unsigned long long)m.offset); }
        };
        for (const auto& r:first) emit(r,"first_invalid");
        for (const auto& r:recent) emit(r,"tail");
    }
};
inline Journal& Get() { static Journal journal; return journal; }
inline bool Enabled() {
    static const bool value=[] { const char* p=std::getenv("SOTC_GPU_SUBMIT_TRACE"); return p && std::strcmp(p,"1")==0; }();
    return value;
}
template<class Params, class Buffer>
void Observe(U64 pipeline, bool tiler, U64 groups, U64 bits, const Params& params,
             const Buffer& tiled, const Buffer& linear, const Buffer& uniform, U64 tick = 0) {
    if (!Enabled()) return;
    Record r{}; r.pipeline=pipeline; r.tiler=tiler; r.groups=groups; r.bits=bits; r.levels=params.num_mips;
    r.tick=tick;
    r.bank_swizzle=params.bank_swizzle; r.micro_tiled_mips=params.micro_tiled_mips;
    const Buffer bindings[]={tiled,linear,uniform};
    for (unsigned i=0;i<3;++i) r.bindings[i]={bindings[i].buffer,bindings[i].offset,bindings[i].range};
    for (unsigned i=0;i<r.levels && i<16;++i) {
        const auto& m=params.mips[i]; r.mips[i]={m.size,m.pitch,m.height,m.offset};
    }
    Get().Add(r);
}
} // namespace SotcTilingTrace
