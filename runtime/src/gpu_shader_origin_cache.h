// Bounded pre-enqueue prefix index. CPU observations only; no guest ownership,
// retention, shader acceptance, synchronization or GPU execution is inferred.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace SotcShaderOriginCache {
inline constexpr std::size_t Slots=4096,Ways=4,Buckets=Slots/Ways;
struct Observation {
    std::uint64_t address{},packet{},raw{},head{},flags{},capture{},sampled_ns{},context{},physical{},stamp{};
};
struct Snapshot {
    int state{}; // 0 no observations, 1 empty bucket, 2 collision/no address, 5 matched.
    std::uint64_t total{},replacements{};
    std::size_t bucket_population{},count{};
    std::array<Observation,Ways> matches{};
};
class Recorder {
    std::mutex mutex;
    std::array<Observation,Slots> slots{};
    std::uint64_t sequence{},total{},replacements{};
    static std::size_t Bucket(std::uint64_t address) {
        auto key=address>>8;key^=key>>17;key*=UINT64_C(0x9e3779b97f4a7c15);key^=key>>32;
        return std::size_t(key)&(Buckets-1);
    }
public:
    void Observe(Observation value) {
        if(!value.address || !value.packet || !value.capture)return;
        std::lock_guard lock(mutex);
        const auto begin=Bucket(value.address)*Ways;
        std::size_t selected=begin;std::uint64_t oldest=UINT64_MAX;
        for(std::size_t i=begin;i<begin+Ways;++i) {
            if(!slots[i].stamp) {selected=i;oldest=0;break;}
            if(slots[i].stamp<oldest) {selected=i;oldest=slots[i].stamp;}
        }
        if(slots[selected].stamp)++replacements;
        value.stamp=++sequence;slots[selected]=value;++total;
    }
    Snapshot Find(std::uint64_t address) {
        Snapshot result;
        std::lock_guard lock(mutex);result.total=total;result.replacements=replacements;
        if(!total)return result;
        const auto begin=Bucket(address)*Ways;
        for(std::size_t i=begin;i<begin+Ways;++i) {
            if(!slots[i].stamp)continue;
            ++result.bucket_population;
            if(slots[i].address==address && result.count<result.matches.size())
                result.matches[result.count++]=slots[i];
        }
        result.state=result.count?5:result.bucket_population?2:1;
        std::sort(result.matches.begin(),result.matches.begin()+result.count,
                  [](const Observation& a,const Observation& b){return a.stamp<b.stamp;});
        return result;
    }
};
static_assert(sizeof(Recorder)<512*1024,"shader origin index must remain bounded");
inline Recorder& Get(){static Recorder recorder;return recorder;}
inline void Observe(std::uint64_t address,std::uint64_t packet,std::uint64_t raw,
                    std::uint64_t head,std::uint64_t flags,std::uint64_t capture,
                    std::uint64_t sampled_ns,std::uint64_t context,std::uint64_t physical) {
    Get().Observe({address,packet,raw,head,flags,capture,sampled_ns,context,physical});
}
inline Snapshot Find(std::uint64_t address){return Get().Find(address);}
} // namespace SotcShaderOriginCache
