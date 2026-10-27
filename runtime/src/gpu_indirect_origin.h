// Bounded provenance; optional snapshot of bytes just copied by the existing host writer.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <array>
#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
namespace SotcIndirectOrigin {
using U64=std::uint64_t;
inline bool Enabled() {static const bool v=[] {const char* p=std::getenv("SOTC_INDIRECT_PREP_TRACE");return p && !std::strcmp(p,"1");}();return v;}
inline bool CaptureCopies(){static const bool v=[] {const char* p=std::getenv("SOTC_INDIRECT_COPY_TRACE");return p && !std::strcmp(p,"1");}();return v;}
struct Record {
    U64 sequence{}, kind{}, guest{}, requested{}, actual{}, tick{}, backend{}, offset{}, length{};
    U64 metal{}, metal_base{}, recording{}, stride{}, count{}, route{};
    U64 destination{}, ticket{}, copy_status{}, captured_bytes{};
    std::uint32_t words[5]{};
};
// Descriptor intent only: no guest reads and no proof of a shader execution/write.
inline bool CaptureWriters(){static const bool v=[] {const char* p=std::getenv("SOTC_INDIRECT_WRITER_TRACE");return p && !std::strcmp(p,"1");}();return v;}
struct WriterRecord { U64 sequence{},guest{},bytes{},shader{},slot{},tick{},paged{};
    U64 first_sequence{},first_tick{},observations{};
};
class WriterJournal {
    using Key=std::array<U64,5>;
    static Key Identity(const WriterRecord& r){return {r.guest,r.bytes,r.shader,r.slot,r.paged};}
    std::mutex mutex;std::list<WriterRecord> records;
    std::map<Key,std::list<WriterRecord>::iterator> index;
    U64 total{},omitted{},evicted{},coalesced{};
public:
    void Add(WriterRecord r){
        std::lock_guard lock(mutex);r.sequence=++total;
        const auto key=Identity(r);const auto found=index.find(key);
        if(found!=index.end()){
            auto it=found->second;it->sequence=r.sequence;it->tick=r.tick;++it->observations;
            ++coalesced;records.splice(records.end(),records,it);return;
        }
        if(records.size()==2048){omitted+=records.front().observations;++evicted;
            index.erase(Identity(records.front()));records.pop_front();}
        r.first_sequence=r.sequence;r.first_tick=r.tick;r.observations=1;
        records.push_back(r);index.emplace(key,std::prev(records.end()));
    }
    void Report(std::FILE* out){
        std::list<WriterRecord> copy;U64 n,lost,e,c;
        {std::lock_guard lock(mutex);copy=records;n=total;lost=omitted;e=evicted;c=coalesced;}
        std::fprintf(out,"GPU indirect writers: total=%llu retained=%zu omitted=%llu limit=2048 schema=2 evicted=%llu coalesced=%llu; writable descriptor summaries, not continuous lifetimes or writes\n",(unsigned long long)n,copy.size(),(unsigned long long)lost,(unsigned long long)e,(unsigned long long)c);
        for(const auto& r:copy)std::fprintf(out,"GPU indirect writer: sequence=%llu guest=%#llx bytes=%llu shader=%#llx slot=%llu tick=%llu paged=%llu first_sequence=%llu first_tick=%llu observations=%llu\n",(unsigned long long)r.sequence,(unsigned long long)r.guest,(unsigned long long)r.bytes,(unsigned long long)r.shader,(unsigned long long)r.slot,(unsigned long long)r.tick,(unsigned long long)r.paged,(unsigned long long)r.first_sequence,(unsigned long long)r.first_tick,(unsigned long long)r.observations);
    }
};
inline WriterJournal& Writers(){static WriterJournal j;return j;}
// Recent DMA fill observations; GPU-recorded is not GPU-completed.
inline bool CaptureFills(){static const bool v=[] {const char* p=std::getenv("SOTC_ARGUMENT_FILL_TRACE");return p && !std::strcmp(p,"1");}();return v;}
struct FillRecord {U64 sequence{},guest{},bytes{},value{},tick{},route{},backend{},offset{};};
class FillJournal {
    std::mutex mutex;std::deque<FillRecord> records;U64 total{};
public:
    void Add(FillRecord r){std::lock_guard lock(mutex);r.sequence=++total;if(records.size()==2048)records.pop_front();records.push_back(r);}
    void ReportMatches(std::FILE* out,U64 guest,U64 bytes,U64 shader,U64 slot,U64 tick){
        std::deque<FillRecord> matches;U64 n,retained;
        {std::lock_guard lock(mutex);n=total;retained=records.size();
            for(const auto& r:records) if(r.route!=3 && bytes && r.bytes &&
                (guest>=r.guest ? guest-r.guest<r.bytes : r.guest-guest<bytes))matches.push_back(r);}
        std::fprintf(out,"GPU argument fills: shader=%#llx slot=%llu guest=%#llx bytes=%llu bind_tick=%llu total=%llu retained=%llu omitted=%llu matches=%zu limit=2048; CPU observation order, not GPU completion\n",(unsigned long long)shader,(unsigned long long)slot,(unsigned long long)guest,(unsigned long long)bytes,(unsigned long long)tick,(unsigned long long)n,(unsigned long long)retained,(unsigned long long)(n-retained),matches.size());
        for(const auto& r:matches)std::fprintf(out,"GPU argument fill: shader=%#llx slot=%llu sequence=%llu guest=%#llx bytes=%llu value=%#llx tick=%llu route=%llu backend=%#llx offset=%llu\n",(unsigned long long)shader,(unsigned long long)slot,(unsigned long long)r.sequence,(unsigned long long)r.guest,(unsigned long long)r.bytes,(unsigned long long)r.value,(unsigned long long)r.tick,(unsigned long long)r.route,(unsigned long long)r.backend,(unsigned long long)r.offset);
    }
};
inline FillJournal& Fills(){static FillJournal j;return j;}
inline void Filled(U64 guest,U64 bytes,U64 value,U64 tick,U64 route,U64 backend=0,U64 offset=0){if(CaptureFills())Fills().Add({0,guest,bytes,value,tick,route,backend,offset});}
// A single shader's descriptor metadata, bounded before any diagnostics are printed.
inline void SelectedBinding(U64 guest,U64 bytes,U64 shader,U64 slot,U64 tick,bool written,bool paged,U64 route=0,U64 backend=0,U64 offset=0,U64 special_type=0){
    static const U64 selected=[]{const char* p=std::getenv("SOTC_ARGUMENT_INPUT_SHADER");if(!p || !*p)return U64(0);char* end=nullptr;const auto v=std::strtoull(p,&end,16);return end && !*end?U64(v):U64(0);}();
    if(!selected || shader!=selected)return;
    static std::atomic<U64> count{0};const U64 n=count.fetch_add(1,std::memory_order_relaxed)+1;
    if(n==129)std::fprintf(stderr,"GPU argument inputs: limit=128 truncated=1; later bindings omitted\n");
    if(n>128)return;
    if(written && CaptureFills() && route!=2)Fills().ReportMatches(stderr,guest,bytes,shader,slot,tick);
    std::fprintf(stderr,"GPU argument input: sequence=%llu shader=%#llx slot=%llu guest=%#llx declared_bytes=%llu tick=%llu written=%u paged=%u route=%llu backend=%#llx offset=%llu special_type=%llu; metadata only, no memory read\n",(unsigned long long)n,(unsigned long long)shader,(unsigned long long)slot,(unsigned long long)guest,(unsigned long long)bytes,(unsigned long long)tick,unsigned(written),unsigned(paged),(unsigned long long)route,(unsigned long long)backend,(unsigned long long)offset,(unsigned long long)special_type);
}
inline void WritableBinding(U64 guest,U64 bytes,U64 shader,U64 slot,U64 tick,bool paged){
    if(Enabled() && CaptureWriters() && guest && bytes)Writers().Add({0,guest,bytes,shader,slot,tick,U64(paged)});
}
class Journal {
    std::mutex mutex;std::deque<Record> records;std::unordered_map<U64,std::deque<Record>> pending;U64 total{},tickets{},pending_dropped{},ambiguous{},pending_count{};
    void AddLocked(Record r){r.sequence=++total;if(records.size()==4096)records.pop_front();records.push_back(r);}
public:
    void Add(Record r) {std::lock_guard lock(mutex);AddLocked(r);}
    U64 Queue(Record r) {std::lock_guard lock(mutex);r.ticket=++tickets;if(pending_count==4096){++pending_dropped;return r.ticket;}pending[r.destination].push_back(r);++pending_count;return r.ticket;}
    void Complete(U64 source,U64 destination,U64 size) {
        std::lock_guard lock(mutex);auto group=pending.find(destination);if(group==pending.end())return;
        auto& candidates=group->second;unsigned matches=0;
        for(const auto& r:candidates) if(r.guest==source && r.destination==destination && r.actual==size)++matches;
        if(!matches)return;
        if(matches>1)++ambiguous;
        for(auto it=candidates.begin();it!=candidates.end();) {
            if(it->guest!=source || it->destination!=destination || it->actual!=size){++it;continue;}
            auto r=*it;it=candidates.erase(it);--pending_count;r.kind=3;r.copy_status=pending_dropped?3:matches==1?1:2;
            // Only the existing copy worker's destination, immediately after copy/flush.
            // Never re-read guest memory or a GPU-produced argument block.
            if(r.copy_status==1 && size==20 && r.count==1 && r.stride==20) {
                std::memcpy(r.words,reinterpret_cast<const void*>(destination),20);r.captured_bytes=20;
            }
            AddLocked(r);
        }
        if(candidates.empty())pending.erase(group);
    }
    void Report(std::FILE* out) {
        if(CaptureWriters())Writers().Report(out);
        std::deque<Record> copy;U64 n,d,a,unresolved;{std::lock_guard lock(mutex);copy=records;n=total;d=pending_dropped;a=ambiguous;unresolved=pending_count;}
        std::fprintf(out,"GPU indirect origins: total=%llu retained=%zu omitted=%llu limit=4096 pending=%llu pending_dropped=%llu ambiguous=%llu; kind=3 is host copy observation, not GPU consumed bytes\n",(unsigned long long)n,copy.size(),(unsigned long long)(n-copy.size()),(unsigned long long)unresolved,(unsigned long long)d,(unsigned long long)a);
        for(const auto& r:copy) std::fprintf(out,"GPU indirect origin: sequence=%llu kind=%llu guest=%#llx requested=%llu actual=%llu tick=%llu backend=%#llx offset=%llu length=%llu metal=%#llx metal_base=%llu recording=%llu stride=%llu count=%llu route=%llu destination=%#llx ticket=%llu copy_status=%llu captured_bytes=%llu word0=%u word1=%u word2=%u word3=%u word4=%u\n",
            (unsigned long long)r.sequence,(unsigned long long)r.kind,(unsigned long long)r.guest,(unsigned long long)r.requested,
            (unsigned long long)r.actual,(unsigned long long)r.tick,(unsigned long long)r.backend,(unsigned long long)r.offset,
            (unsigned long long)r.length,(unsigned long long)r.metal,(unsigned long long)r.metal_base,(unsigned long long)r.recording,
            (unsigned long long)r.stride,(unsigned long long)r.count,(unsigned long long)r.route,
            (unsigned long long)r.destination,(unsigned long long)r.ticket,(unsigned long long)r.copy_status,(unsigned long long)r.captured_bytes,
            r.words[0],r.words[1],r.words[2],r.words[3],r.words[4]);
    }
};
inline Journal& Get(){static Journal j;return j;}
inline thread_local Record* current{};
struct Scope {
    Record record;Record* previous;
    explicit Scope(Record r):record(r),previous(current){current=Enabled()?&record:nullptr;}
    ~Scope(){current=previous;}
    void Finish(U64 backend,U64 offset,U64 length){if(current==&record){record.kind=1;record.backend=backend;record.offset=offset;record.length=length;Get().Add(record);}}
};
// 0 other/unknown route; 1 deferred stream selected (may execute inline); 2 synchronous stream.
inline void Route(U64 route){if(current)current->route=route;}
inline void QueueCopy(U64 destination) {
    if(current && CaptureCopies()) {current->destination=destination;current->ticket=Get().Queue(*current);}
}
inline void Copied(U64 source,U64 destination,U64 size) {
    if(Enabled() && CaptureCopies())Get().Complete(source,destination,size);
}
inline void Resolve(Record r){if(Enabled()){r.kind=2;Get().Add(r);}}
}
