// Original opt-in parameter scaling. Shader identities/layouts are supplied privately.
#pragma once
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <mutex>
namespace SotcFlatScale {
struct Rule { unsigned long long hash; unsigned first,count; float scale; };
inline bool Parse(const char* text,std::vector<Rule>& out) {
    out.clear(); if(!text||!*text)return true;
    if(std::strlen(text)>1024)return false;
    while(*text) {
        Rule r{}; int n=0;
        if(std::sscanf(text,"%llx:%u:%u:%f%n",&r.hash,&r.first,&r.count,&r.scale,&n)!=4 ||
           n<=0 || !r.hash || !r.count || r.count>16 || r.first>4096-r.count ||
           !std::isfinite(r.scale) || r.scale<0.125f || r.scale>4.f || out.size()>=8)
            {out.clear();return false;}
        for(const auto& old:out)if(old.hash==r.hash && r.first<old.first+old.count && old.first<r.first+r.count)
            {out.clear();return false;}
        out.push_back(r);text+=n;
        if(!*text)break;
        if(*text++!=';'||!*text){out.clear();return false;}
    }
    return true;
}
inline bool Apply(unsigned long long hash,const uint32_t* input,size_t size,
                  const std::vector<Rule>& rules,std::vector<uint32_t>& out) {
    out.clear(); bool match=false;
    for(const auto& r:rules)if(r.hash==hash && r.scale!=1.f) {
        match=true;
        if(r.first>size || r.count>size-r.first)return false;
        for(unsigned i=0;i<r.count;++i) {
            float value;std::memcpy(&value,input+r.first+i,4);
            if(!std::isfinite(value)||!std::isfinite(value*r.scale))return false;
        }
    }
    if(!match)return false;
    out.assign(input,input+size);
    for(const auto& r:rules)if(r.hash==hash && r.scale!=1.f)
        for(unsigned i=0;i<r.count;++i) {
            float value;std::memcpy(&value,&out[r.first+i],4);value*=r.scale;
            std::memcpy(&out[r.first+i],&value,4);
        }
    return true;
}
inline bool Copy(unsigned long long hash,const uint32_t* input,size_t size,std::vector<uint32_t>& out) {
    static const auto rules=[] {
        std::vector<Rule> r;
        if(!Parse(std::getenv("SOTC_FLAT_SCALES"),r)) {
            std::fprintf(stderr,"STOP_VEGETATION_OPTIONS: invalid scaling profile\n");std::exit(24);
        }
        return r;
    }();
    const bool changed=Apply(hash,input,size,rules,out);
    if(changed) {
        static std::mutex mutex;static std::vector<unsigned long long> seen;
        std::lock_guard lock(mutex);
        bool found=false;for(auto h:seen)found|=h==hash;
        if(!found) {seen.push_back(hash);std::fprintf(stderr,"Vegetation trial: parameter copy applied (program %zu)\n",seen.size());}
    }
    return changed;
}
}
