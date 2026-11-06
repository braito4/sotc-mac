// Original continuing, rate-limited mapping observation; no guest content reads.
#pragma once
#include "gpu_fallback_mapping.h"
#include <chrono>
namespace SotcFallbackMapping {
struct ProbeWindow {
 struct Key {uint64_t address{},size{};};
 std::array<Key,32> keys{};
 uint64_t start{},skipped{},duplicates{};
 unsigned count{};
 bool initialized{};
 bool Admit(uint64_t now,uint64_t address,uint64_t size) {
  constexpr uint64_t period=10000000000ULL;
  if(!initialized || now<start || now-start>=period) {
   initialized=true;start=now;count=0;skipped=0;duplicates=0;
  }
  for(unsigned i=0;i<count;++i)if(keys[i].address==address&&keys[i].size==size) {
   ++duplicates;return false;
  }
  if(count==keys.size()){++skipped;return false;}
  keys[count++]={address,size};return true;
 }
};
template<class Query> void RecordWindow(uint64_t address,uint64_t size,bool written,uint64_t tick,
                                       uint64_t staging,uint64_t offset,Query query) {
 static const bool enabled=[] {const char* p=std::getenv("SOTC_FALLBACK_MAPPING");return p&&!std::strcmp(p,"1");}();
 if(!enabled||!written)return;
 static std::mutex mutex;static ProbeWindow budget;static uint64_t serial=0;
 std::lock_guard lock(mutex);
 const auto now=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
     std::chrono::steady_clock::now().time_since_epoch()).count());
 const auto old_start=budget.start,old_skipped=budget.skipped,old_duplicates=budget.duplicates;
 const bool admitted=budget.Admit(now,address,size);
 if(old_start&&old_start!=budget.start)
  std::fprintf(stderr,"Writable fallback window ended: omitted_new_ranges=%llu duplicate_calls=%llu; coverage_sampled_not_exhaustive=1\n",
    (unsigned long long)old_skipped,(unsigned long long)old_duplicates);
 if(!admitted)return;
 const auto snapshot=Capture(address,size,query);
 std::fprintf(stderr,"Writable fallback mapping window: record=%llu monotonic_ns=%llu address=%#llx size=%llu tick=%llu staging=%#llx offset=%llu complete=%d snapshot_after_copy=1 atomic_snapshot=0\n",
  (unsigned long long)++serial,(unsigned long long)now,(unsigned long long)address,
  (unsigned long long)size,(unsigned long long)tick,(unsigned long long)staging,
  (unsigned long long)offset,int(snapshot.complete));
 for(unsigned i=0;i<snapshot.count;++i){
  const auto& s=snapshot.segments[i];
  std::fprintf(stderr,"  fallback segment %u start=%#llx physical=%#llx end=%#llx mapped=%d\n",
   i,(unsigned long long)s.start,(unsigned long long)s.physical,(unsigned long long)s.end,int(s.mapped));
 }
 if(budget.count==budget.keys.size())std::fprintf(stderr,"Writable fallback window full: new ranges omitted until next 10-second window\n");
 std::fflush(stderr);
}
}
