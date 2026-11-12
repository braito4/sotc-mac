#pragma once
// Reviewed layouts only; no guest shader identities or guessed data repair.
#include "gpu_scatter_probe_policy.h"

namespace SotcProducerInput {
using SotcScatterProbe::U64;
using SotcScatterProbe::ValidSpan;
struct Config {
    std::string entry;
    unsigned params{}, params_adjust{}, first_field{}, count_field{};
    unsigned indices{}, indices_adjust{}, normals{}, normals_adjust{}, stride{}, threads{};
};
inline bool Parse(std::string_view text,std::vector<Config>& out) {
    out.clear();
    while(!text.empty()) {
        const auto end=text.find(';');auto row=text.substr(0,end);const auto comma=row.find(',');
        if(comma==row.npos||comma==0||comma>160||out.size()==2)return false;
        Config c;c.entry=std::string(row.substr(0,comma));row.remove_prefix(comma+1);
        for(char ch:c.entry)if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'))return false;
        std::array<unsigned,10> n{};
        for(unsigned i=0;i<n.size();++i) {
            const auto sep=row.find(',');const auto item=row.substr(0,sep);
            const auto parsed=std::from_chars(item.data(),item.data()+item.size(),n[i]);
            if(item.empty()||parsed.ec!=std::errc{}||parsed.ptr!=item.data()+item.size())return false;
            if(i+1<n.size()){if(sep==row.npos)return false;row.remove_prefix(sep+1);}
            else if(sep!=row.npos)return false;
        }
        c.params=n[0];c.params_adjust=n[1];c.first_field=n[2];c.count_field=n[3];
        c.indices=n[4];c.indices_adjust=n[5];c.normals=n[6];c.normals_adjust=n[7];c.stride=n[8];c.threads=n[9];
        if(c.params>=31||c.indices>=31||c.normals>=31||c.params==c.indices||c.params==c.normals||c.indices==c.normals||
           c.params_adjust>=256||c.indices_adjust>=256||c.normals_adjust>=256||c.first_field>4096||c.count_field>4096||
           c.first_field%4||c.count_field%4||c.first_field==c.count_field||
           (c.stride!=4&&c.stride!=8)||!c.threads||c.threads>1024)return false;
        for(const auto& old:out)if(old.entry==c.entry)return false;
        out.push_back(c);if(end==text.npos)break;
        text.remove_prefix(end+1);if(text.empty())return false;
    }
    return !out.empty();
}
struct Result {
    U64 triangles{}, words{}, checked{}, invalid{}, first_bad_word{}, first_bad_index{}, capacity{};
    bool input_short{}, truncated{}, arithmetic_wrap{};
    bool Bad()const{return invalid||input_short||truncated||arithmetic_wrap;}
};
inline Result Inspect(const void* input,U64 copied_words,U64 input_words,
                      std::uint32_t first,std::uint32_t count,U64 dispatched,
                      U64 normal_bytes,unsigned stride,U64 index_adjust_words=0,U64 normal_adjust_words=0) {
    Result r;r.triangles=std::min<U64>(count,dispatched);r.words=r.triangles*3;r.capacity=normal_bytes/stride;
    // The reviewed shaders use 32-bit dword arithmetic. Reject wrap instead of
    // validating a different unbounded mathematical address.
    r.arithmetic_wrap=r.words&&(U64(first)+r.words-1+index_adjust_words>UINT32_MAX);
    r.input_short=r.words&&(first>input_words||r.words>input_words-std::min<U64>(first,input_words));
    const U64 available=first>copied_words?0:copied_words-first;
    r.checked=std::min(r.words,available);r.truncated=r.checked<r.words;
    if(r.arithmetic_wrap)return r;
    const auto* bytes=static_cast<const unsigned char*>(input);
    for(U64 i=0;i<r.checked;++i) {
        std::uint32_t index;std::memcpy(&index,bytes+(U64(first)+i)*4,4);
        const U64 last_dword=U64(index)*(stride/4)+normal_adjust_words+(stride/4-1);
        if(index>=r.capacity||last_dword>UINT32_MAX) {
            if(!r.invalid){r.first_bad_word=i;r.first_bad_index=index;}++r.invalid;
        }
    }
    return r;
}
} // namespace SotcProducerInput
