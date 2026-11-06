#pragma once
// Original diagnostic policy: configured layouts only, no game identities.
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace SotcScatterProbe {
using U64 = std::uint64_t;
struct Config {
    std::string entry;
    unsigned count_slot{}, count_field{}, count_adjust{}, index_slot{}, index_adjust{};
    std::array<unsigned,2> output_slot{}, output_adjust{}, stride{};
    unsigned threads_x{};
};
inline bool Parse(std::string_view value, std::vector<Config>& out) {
    out.clear();
    while (!value.empty()) {
        const auto end=value.find(';');
        auto row=value.substr(0,end);
        const auto comma=row.find(',');
        if (comma==row.npos || comma==0 || comma>160 || out.size()==4) return false;
        Config c;c.entry=std::string(row.substr(0,comma));row.remove_prefix(comma+1);
        for (const char ch:c.entry) if (!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_')) return false;
        std::array<unsigned,12> n{};
        for (unsigned i=0;i<n.size();++i) {
            const auto sep=row.find(',');const auto item=row.substr(0,sep);
            if (item.empty()) return false;
            const auto r=std::from_chars(item.data(),item.data()+item.size(),n[i]);
            if (r.ec!=std::errc{} || r.ptr!=item.data()+item.size()) return false;
            if (i+1<n.size()) { if(sep==row.npos)return false;row.remove_prefix(sep+1); }
            else if (sep!=row.npos) return false;
        }
        c.count_slot=n[0];c.count_field=n[1];c.count_adjust=n[2];
        c.index_slot=n[3];c.index_adjust=n[4];
        c.output_slot={n[5],n[8]};c.output_adjust={n[6],n[9]};c.stride={n[7],n[10]};c.threads_x=n[11];
        if(c.count_slot>=31||c.index_slot>=31||c.output_slot[0]>=31||c.output_slot[1]>=31||
           c.count_adjust>=256||c.index_adjust>=256||c.output_adjust[0]>=256||c.output_adjust[1]>=256||
           c.count_field>4096||c.count_field%4||!c.threads_x||c.threads_x>1024||
           !c.stride[0]||!c.stride[1]||c.stride[0]>256||c.stride[1]>256) return false;
        for(const auto& old:out)if(old.entry==c.entry)return false;
        out.push_back(c);
        if(end==value.npos)break;
        value.remove_prefix(end+1);if(value.empty())return false;
    }
    return !out.empty();
}
struct Result {
    U64 active{}, checked{}, invalid{}, first_thread{}, first_index{}, maximum{};
    bool input_short{}, truncated{};
};
inline Result Inspect(const void* copied, U64 copied_words, std::uint32_t count,
                      U64 dispatched, U64 input_words, std::array<U64,2> output_bytes,
                      std::array<unsigned,2> stride) {
    Result r;r.active=std::min<U64>(count,dispatched);
    r.input_short=r.active>input_words;r.checked=std::min({r.active,input_words,copied_words});
    r.truncated=r.checked<r.active;
    const auto* bytes=static_cast<const unsigned char*>(copied);
    for(U64 i=0;i<r.checked;++i){
        std::uint32_t index;std::memcpy(&index,bytes+i*4,4);
        r.maximum=std::max<U64>(r.maximum,index);
        // Division avoids multiplication/wrap and requires the entire output element.
        if(index>=output_bytes[0]/stride[0]||index>=output_bytes[1]/stride[1]){
            if(!r.invalid){r.first_thread=i;r.first_index=index;}++r.invalid;
        }
    }
    return r;
}
inline bool ValidSpan(U64 offset,U64 size,U64 allocation){return offset<=allocation&&size<=allocation-offset;}
} // namespace SotcScatterProbe
