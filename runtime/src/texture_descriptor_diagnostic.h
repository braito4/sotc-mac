// Original bounded diagnostics. Raw descriptors appear only in private run logs.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
namespace SotcTextureDescriptor {
inline bool InvalidLayers(uint64_t type,uint64_t layers) {
    return layers==0 || layers>2048 || (type==3 && layers!=1);
}
inline void Record(const char* stage,uint64_t type,uint64_t width,uint64_t height,
                   uint64_t depth,uint64_t levels,uint64_t layers,uint64_t address,
                   uint64_t size,const void* raw=nullptr,size_t raw_size=0) {
    static std::mutex mutex;
    static unsigned count=0;
    std::lock_guard lock(mutex);
    if(count>=32)return;
    ++count;
    std::fprintf(stderr,"Texture descriptor candidate: stage=%s seq=%u type=%llu width=%llu height=%llu depth=%llu levels=%llu layers=%llu address=%#llx bytes=%llu raw_bytes=%zu\n",
        stage,count,(unsigned long long)type,(unsigned long long)width,
        (unsigned long long)height,(unsigned long long)depth,(unsigned long long)levels,
        (unsigned long long)layers,(unsigned long long)address,(unsigned long long)size,
        raw ? (raw_size<128?raw_size:128) : size_t(0));
    if(raw) {
        const auto* p=static_cast<const unsigned char*>(raw);
        const size_t n=raw_size<128?raw_size:128;
        std::fprintf(stderr,"  raw descriptor prefix:");
        for(size_t i=0;i<n;++i)std::fprintf(stderr,"%02x",p[i]);
        std::fprintf(stderr,"\n");
    }
    std::fflush(stderr);
}
inline void CheckMetal(uint64_t type,uint64_t width,uint64_t height,uint64_t depth,
                       uint64_t levels,uint64_t layers,uint64_t format) {
    if(!InvalidLayers(type,layers))return;
    Record("metal-rejected",type,width,height,depth,levels,layers,0,0);
    std::fprintf(stderr,"STOP_GPU_TEXTURE_DESCRIPTOR: invalid array length=%llu type=%llu format=%llu limit=2048; descriptor not clamped; origin remains unproven\n",
        (unsigned long long)layers,(unsigned long long)type,(unsigned long long)format);
    std::fflush(stderr);std::_Exit(23);
}
}
