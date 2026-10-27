// SPDX-License-Identifier: GPL-2.0-or-later
// Bounded CPU encoding metadata. Handles do not prove resource lifetime or GPU progress.
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>
namespace SotcRenderTrace {
using U64=std::uint64_t;
struct Attachment {
    U64 texture{}, resolve{}, format{}, width{}, height{}, depth{}, layers{}, samples{};
    U64 level{}, slice{}, depth_plane{}, load{}, store{}, resolve_level{}, resolve_slice{};
};
struct Draw {
    U64 ordinal{}, pipeline{}, indirect{}, offset{}, index{}, index_offset{}, count{}, instances{}, first{}, base_instance{};
    U64 indirect_length{}, index_length{}, argument_bytes{}, index_element_bytes{};
    // 0 unknown/not applicable, 1 in bounds, 2 out of bounds. No GPU argument reads.
    unsigned argument_range{}, index_base_range{}, index_draw_range{};
    std::int64_t base_vertex{};
    unsigned kind{}, primitive{}, samples{};
    std::array<char,160> vertex{}, fragment{};
    bool truncated{};
};
inline bool Fits(U64 length, U64 offset, U64 count, U64 stride) {
    return offset<=length && stride && count<=(length-offset)/stride;
}
inline void CheckRanges(Draw& d) {
    d.argument_range=d.index_base_range=d.index_draw_range=0;
    if(d.kind==2 || d.kind==3)
        d.argument_range=d.indirect && Fits(d.indirect_length,d.offset,1,d.argument_bytes)?1:2;
    if(d.kind==1 || d.kind==3 || d.kind==5) {
        d.index_base_range=d.index && Fits(d.index_length,d.index_offset,0,d.index_element_bytes)?1:2;
        if(d.kind!=3) d.index_draw_range=d.index && Fits(d.index_length,d.index_offset,d.count,d.index_element_bytes)?1:2;
    }
}
struct Pipeline { Draw info; U64 draws{}; };
struct Scope {
    U64 buffer{}, recording{}, encoder{}, command{}, begin_event{}, end_event{}, checkpoint{}, progress_event{}, total{}, pipeline_untracked{};
    std::array<Attachment,10> attachments{}; // colors 0..7, depth 8, stencil 9
    std::deque<Draw> first, tail;
    std::vector<Pipeline> pipelines;
    void Add(Draw draw) {
        draw.ordinal=++total;
        if(first.size()<32) first.push_back(draw);
        else { if(tail.size()==256) tail.pop_front(); tail.push_back(draw); }
        for(auto& p:pipelines) if(p.info.pipeline==draw.pipeline) { ++p.draws;return; }
        if(pipelines.size()<64) pipelines.push_back({draw,1}); else ++pipeline_untracked;
    }
    void Report(std::FILE* out) const {
        std::fprintf(out,"GPU render trace: buffer=%#llx recording=%llu encoder=%#llx command=%#llx begin_event=%llu end_event=%llu checkpoint=%llu progress_event=%#llx draws=%llu retained=%zu omitted=%llu pipelines=%zu pipeline_untracked_draws=%llu; bounded regular render scopes, CPU encoding only\n",
            (unsigned long long)buffer,(unsigned long long)recording,(unsigned long long)encoder,(unsigned long long)command,(unsigned long long)begin_event,
            (unsigned long long)end_event,(unsigned long long)checkpoint,(unsigned long long)progress_event,
            (unsigned long long)total,first.size()+tail.size(),(unsigned long long)(total-first.size()-tail.size()),pipelines.size(),(unsigned long long)pipeline_untracked);
        for(unsigned i=0;i<10;++i) { const auto& a=attachments[i];
            std::fprintf(out,"GPU render trace: attachment=%u texture=%#llx resolve=%#llx format=%llu width=%llu height=%llu depth=%llu layers=%llu samples=%llu level=%llu slice=%llu depth_plane=%llu load=%llu store=%llu resolve_level=%llu resolve_slice=%llu\n",i,
                (unsigned long long)a.texture,(unsigned long long)a.resolve,(unsigned long long)a.format,(unsigned long long)a.width,(unsigned long long)a.height,
                (unsigned long long)a.depth,(unsigned long long)a.layers,(unsigned long long)a.samples,(unsigned long long)a.level,(unsigned long long)a.slice,
                (unsigned long long)a.depth_plane,(unsigned long long)a.load,(unsigned long long)a.store,(unsigned long long)a.resolve_level,(unsigned long long)a.resolve_slice);
        }
        for(const auto& p:pipelines) std::fprintf(out,"GPU render trace: pipeline=%#llx draws=%llu primitive=%u samples=%u vertex=%s fragment=%s truncated=%u\n",
            (unsigned long long)p.info.pipeline,(unsigned long long)p.draws,p.info.primitive,p.info.samples,
            p.info.vertex[0]?p.info.vertex.data():"-",p.info.fragment[0]?p.info.fragment.data():"-",unsigned(p.info.truncated));
        auto emit=[out](const Draw& d) { std::fprintf(out,"GPU render trace: draw=%llu kind=%u pipeline=%#llx count=%llu instances=%llu first=%llu base_vertex=%lld base_instance=%llu indirect=%#llx offset=%llu index=%#llx index_offset=%llu indirect_length=%llu index_length=%llu argument_bytes=%llu index_element_bytes=%llu argument_range=%u index_base_range=%u index_draw_range=%u\n",
            (unsigned long long)d.ordinal,d.kind,(unsigned long long)d.pipeline,(unsigned long long)d.count,(unsigned long long)d.instances,
            (unsigned long long)d.first,(long long)d.base_vertex,(unsigned long long)d.base_instance,(unsigned long long)d.indirect,(unsigned long long)d.offset,
            (unsigned long long)d.index,(unsigned long long)d.index_offset,(unsigned long long)d.indirect_length,(unsigned long long)d.index_length,
            (unsigned long long)d.argument_bytes,(unsigned long long)d.index_element_bytes,d.argument_range,d.index_base_range,d.index_draw_range); };
        for(const auto& d:first) emit(d);for(const auto& d:tail) emit(d);
    }
};
}
