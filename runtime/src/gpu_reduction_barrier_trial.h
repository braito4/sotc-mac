#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <array>
namespace SotcReductionTrial {
inline bool Enabled(){static const bool v=[] {const char* p=std::getenv("SOTC_TEST_REDUCTION_BARRIERS");return p && !std::strcmp(p,"1");}();return v;}
// Only called for exact SPIR-V XXH3 fece6f0b82549285: 64-thread reduction.
// All six insertion points are at kernel scope, outside lane-dependent branches.
// Validate every anchor before mutation; reject changed source rather than guessing.
inline bool Apply(std::string& source){
    constexpr std::array<unsigned,6> limits{32,16,8,4,2,1};
    std::array<std::string,6> anchors;
    for(unsigned i=0;i<limits.size();++i){
        anchors[i]="\n    if ("+std::to_string(limits[i])+"u > gl_LocalInvocationID.x)\n";
        const auto at=source.find(anchors[i]);
        if(at==std::string::npos || source.find(anchors[i],at+1)!=std::string::npos)return false;
        // Check the anchor's brace depth is the top-level kernel body.
        int depth=0;for(std::size_t j=0;j<at;++j){if(source[j]=='{')++depth;else if(source[j]=='}')--depth;}
        if(depth!=1)return false;
    }
    if(source.find("threadgroup_barrier")!=std::string::npos)return false;
    for(const auto& anchor:anchors){const auto at=source.find(anchor);source.insert(at,"\n    threadgroup_barrier(mem_flags::mem_threadgroup);");}
    return true;
}
}
