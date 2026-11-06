// Original bounded observation at writable fallback binding; no guest reads/writes.
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
namespace SotcFallbackMapping {
struct Segment { uint64_t start{},physical{},end{};bool mapped{};bool operator==(const Segment&) const=default; };
struct Snapshot {std::array<Segment,8> segments{};unsigned count{};bool complete{};bool operator==(const Snapshot&) const=default;};
template<class Query> Snapshot Capture(uint64_t address,uint64_t size,Query query) {
 Snapshot out;
 if(!size||size>UINT64_MAX-address)return out;
 const uint64_t finish=address+size;auto at=address;
 while(at<finish&&out.count<out.segments.size()) {
  uint64_t physical=0;uintptr_t end=0;const bool mapped=query(uintptr_t(at),&physical,&end)!=0;
  out.segments[out.count++]={at,physical,uint64_t(end),mapped};
  if(!mapped||end<=at)return out;
  at=end<finish?end:finish;
 }
 out.complete=at==finish;return out;
}
template<class Query> void Record(uint64_t address,uint64_t size,bool written,uint64_t tick,
                                  uint64_t staging,uint64_t offset,Query query) {
 static const bool enabled=[] {const char* p=std::getenv("SOTC_FALLBACK_MAPPING");return p&&!std::strcmp(p,"1");}();
 if(!enabled||!written)return;
 struct Entry{uint64_t address{},size{};Snapshot map;};
 static std::mutex mutex;static std::array<Entry,32> entries{};static unsigned count=0,calls=0;
 std::lock_guard lock(mutex);
 if(calls>=256||count>=entries.size())return;
 ++calls;const auto snapshot=Capture(address,size,query);
 bool duplicate=false;for(unsigned i=0;i<count;++i)if(entries[i].address==address&&entries[i].size==size&&entries[i].map==snapshot)duplicate=true;
 if(!duplicate) {
  entries[count++]={address,size,snapshot};
  std::fprintf(stderr,"Writable fallback mapping: record=%u address=%#llx size=%llu tick=%llu staging=%#llx offset=%llu complete=%d snapshot_after_copy=1 atomic_snapshot=0\n",count,(unsigned long long)address,(unsigned long long)size,(unsigned long long)tick,(unsigned long long)staging,(unsigned long long)offset,int(snapshot.complete));
  for(unsigned i=0;i<snapshot.count;++i) {
   const auto& s=snapshot.segments[i];std::fprintf(stderr,"  fallback segment %u start=%#llx physical=%#llx end=%#llx mapped=%d\n",i,(unsigned long long)s.start,(unsigned long long)s.physical,(unsigned long long)s.end,int(s.mapped));
  }
 }
 if(calls==256||count==entries.size())std::fprintf(stderr,"Writable fallback mapping: capture limit reached; later mappings unknown\n");
 std::fflush(stderr);
}
}
