#pragma once
// Opt-in exact-entry/output-slot propagation. Complete GPU work before touching
// shared backing; the host checks the original mapping generation under its lock.
// No guessed dirty mask, index repair, partial writeback, or lifetime extension.
#include "gpu_producer_bundle_metal.h"
#include "fragment_writeback_api.h"

namespace SotcFragmentWriteback {
using U64=std::uint64_t;
using Transfer=int(*)(const SotcFragmentTransfer*,unsigned,unsigned);
[[noreturn]] inline void Stop(const char* reason) {
    std::fprintf(stderr,"STOP_GPU_FRAGMENT_WRITEBACK: reason=%s; no unchecked propagation; exit=23\n",reason);
    std::fflush(stderr);std::_Exit(23);
}
struct Config {std::string entry;std::vector<unsigned> slots;unsigned counter=64;U64 minimum{};};
inline const std::vector<Config>& Settings() {
    static const std::vector<Config> settings=[] {
        std::vector<Config> result;const char* value=std::getenv("SOTC_FRAGMENT_WRITEBACK");
        if(!value||!*value)return result;
        if(!SotcFragment::Enabled())Stop("provenance_disabled");
        auto number=[](const std::string& text)->U64 {
            if(text.empty()||text.find_first_not_of("0123456789")!=std::string::npos)Stop("invalid_number");
            errno=0;char* end=nullptr;const auto n=std::strtoull(text.c_str(),&end,10);
            if(errno||!end||*end)Stop("invalid_number");return n;
        };
        const std::string text(value);size_t start=0;
        do {
            const auto end=text.find(';',start);
            const auto record=text.substr(start,end==std::string::npos?end:end-start);
            if(record.empty()||result.size()==5)Stop("selection_limit");
            std::vector<std::string> fields;size_t at=0;
            do {
                const auto next=record.find(',',at);
                fields.push_back(record.substr(at,next==std::string::npos?next:next-at));
                if(next==std::string::npos)break;at=next+1;
            } while(fields.size()<5);
            if(fields.size()!=3&&fields.size()!=4)Stop("invalid_selection");
            Config c;c.entry=fields[0];
            if(c.entry.empty()||c.entry.size()>200||c.entry.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos)Stop("invalid_entry");
            for(const auto& prior:result)if(prior.entry==c.entry)Stop("duplicate_entry");
            const U64 minimum=number(fields[2]),counter=fields.size()==4?number(fields[3]):64;
            if(!minimum||minimum>(8ull<<20)||(fields.size()==4&&counter>63))Stop("invalid_limits");
            size_t slot_start=0;
            do {
                const auto next=fields[1].find('+',slot_start);
                const U64 slot=number(fields[1].substr(slot_start,next==std::string::npos?next:next-slot_start));
                if(slot>63||slot==counter||c.slots.size()==2)Stop("invalid_outputs");
                for(auto prior:c.slots)if(prior==slot)Stop("duplicate_output");
                c.slots.push_back(unsigned(slot));
                if(next==std::string::npos)break;slot_start=next+1;
            } while(true);
            c.minimum=minimum;c.counter=unsigned(counter);result.push_back(c);
            if(end==std::string::npos)break;start=end+1;
        } while(true);
        return result;
    }();return settings;
}
template<class Owned,class Commit> void Complete(Owned& o,Commit commit) {
    id<MTLCommandBuffer> command=o.cmd;
    dispatch_semaphore_t done=dispatch_semaphore_create(0);
    [command addCompletedHandler:^(id<MTLCommandBuffer>){dispatch_semaphore_signal(done);}];
    commit();
    if(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,120ll*NSEC_PER_SEC)))Stop("gpu_timeout");
    if(command.status!=MTLCommandBufferStatusCompleted)Stop("gpu_command_failed");
}
inline std::atomic<U64> sequence{};
class Scope {
    struct Output {
        MetalFrame::Bound* target{};
        MetalFrame::Bound original{};
        id<MTLBuffer> temporary=nil;
        SotcFragment::Copy origin;
        SotcFragmentTransfer transfer{};
        unsigned slot{};
    };
    std::vector<Output> outputs;
    Transfer transfer{};
    U64 seq{};
    std::string entry;unsigned counter_slot=64;
    std::array<SotcFragmentTransfer,2> Records() const {
        std::array<SotcFragmentTransfer,2> records{};
        for(size_t i=0;i<outputs.size();++i)records[i]=outputs[i].transfer;
        return records;
    }
public:
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    ~Scope(){for(auto& out:outputs)if(out.target)*out.target=out.original;}
    template<class Owned,class Commit>
    Scope(Owned& o,id<MTLDevice> device,Commit commit) {
        const auto& settings=Settings();
        if(settings.empty()||!o.compute_pipeline||!o.compute_pipeline->shader)return;
        const Config* config=nullptr;
        for(const auto& c:settings)if(o.compute_pipeline->shader->entry==c.entry){config=&c;break;}
        if(!config)return;const auto& c=*config;
        const auto find=[&](unsigned slot)->MetalFrame::Bound* {
            MetalFrame::Bound* found=nullptr;
            for(const auto& b:o.compute_pipeline->shader->bindings) {
                if(b.kind!=Metal::ShaderBinding::Kind::Buffer||b.msl_index!=slot)continue;
                auto it=o.compute_bindings.find((b.set<<16)|b.binding);
                if(found||b.count!=1||it==o.compute_bindings.end()||it->second.elements.size()!=1)Stop("ambiguous_binding");
                found=&it->second.elements[0];
            }
            return found;
        };
        // Plan every output before rebinding any of them.
        for(unsigned slot:c.slots) {
            MetalFrame::Bound* selected=find(slot);
            if(!selected||!selected->buffer)Stop("missing_output");
            if(selected->size<c.minimum)continue;
            if(selected->offset>selected->buffer.length||selected->size>selected->buffer.length-selected->offset)Stop("output_extent");
            const auto match=SotcFragment::Get().Find(reinterpret_cast<uintptr_t>((__bridge void*)selected->buffer),selected->offset,selected->size);
            if(!match.found)continue; // Direct pool binding: retain original storage.
            const auto& copy=match.copy;
            if(!match.complete||!copy.written||copy.gpu_copy||!copy.mapping.complete||
               copy.generation_before!=copy.generation_after||(copy.generation_after&1)||
               selected->size>(8ull<<20)||copy.metal_base>UINT64_MAX-copy.offset)Stop("unverified_fallback");
            const U64 base=copy.metal_base+copy.offset;
            if(selected->offset<base||selected->offset-base>UINT64_MAX-copy.guest)Stop("guest_overflow");
            const U64 guest=copy.guest+selected->offset-base,bytes=selected->size;
            if(!bytes||bytes>UINT64_MAX-guest)Stop("guest_extent");
            for(const auto& [key,binding]:o.compute_bindings)for(const auto& b:binding.elements) {
                (void)key;
                if(&b!=selected&&b.buffer==selected->buffer&&SotcFragment::Overlap(b.offset,b.size,selected->offset,bytes))Stop("overlapping_binding");
            }
            Output out;out.target=selected;out.original=*selected;out.origin=copy;out.slot=slot;
            out.temporary=[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
            if(!out.temporary)Stop("temporary_allocation");
            out.transfer={uintptr_t(guest),out.temporary.contents,bytes,copy.generation_after};
            outputs.push_back(out);
        }
        if(outputs.empty())return;
        if(c.counter!=64) {
            const auto* counter=find(c.counter);
            if(!counter||!counter->buffer||!counter->size||counter->offset>counter->buffer.length||counter->size>counter->buffer.length-counter->offset)Stop("counter_extent");
            const auto counter_copy=SotcFragment::Get().Find(reinterpret_cast<uintptr_t>((__bridge void*)counter->buffer),counter->offset,counter->size);
            if(counter_copy.found)Stop("counter_fallback_requires_joint_publication");
        }
        entry=c.entry;counter_slot=c.counter;
        transfer=reinterpret_cast<Transfer>(dlsym(RTLD_DEFAULT,"sotc_fragment_transfer_batch_v2"));
        if(!transfer)Stop("host_batch_transfer_missing");
        Complete(o,commit);
        const auto records=Records();
        if(!transfer(records.data(),unsigned(outputs.size()),0))Stop("group_changed_before_dispatch");
        for(auto& out:outputs) {
            out.target->buffer=out.temporary;out.target->offset=0;out.target->size=out.transfer.size;
            const U64 native=reinterpret_cast<uintptr_t>((__bridge void*)out.temporary);
            SotcFragment::Get().Register(native,native,0);
            auto association=out.origin;association.backend=native;association.offset=0;
            association.guest=out.transfer.address;association.bytes=out.transfer.size;
            association.head_bytes=static_cast<unsigned>(std::min<U64>(association.bytes,association.head.size()));
            std::memcpy(association.head.data(),out.temporary.contents,association.head_bytes);
            SotcFragment::Get().Add(association);
        }
        seq=++sequence;
    }
    template<class Owned,class BeginBlit,class Commit>
    void Finish(Owned& o,BeginBlit begin,Commit commit) {
        if(outputs.empty())return;
        auto blit=begin();
        for(const auto& out:outputs)
            [blit copyFromBuffer:out.temporary sourceOffset:0 toBuffer:out.original.buffer destinationOffset:out.original.offset size:out.transfer.size];
        Complete(o,commit);
        const auto records=Records();
        if(!transfer(records.data(),unsigned(outputs.size()),1))Stop("group_changed_before_publish");
        U64 bytes=0;for(const auto& out:outputs)bytes+=out.transfer.size;
        if(seq<=32||seq%128==0)std::fprintf(stderr,"Fragment writeback group: sequence=%llu entry=%s outputs=%zu slots=%u,%u counter_slot=%u bytes=%llu completed=1 backing_published=1 staging_updated=1 counter_binding_unchanged=1\n",
            (unsigned long long)seq,entry.c_str(),outputs.size(),outputs[0].slot,outputs.size()>1?outputs[1].slot:64,counter_slot,(unsigned long long)bytes);
        for(auto& out:outputs) {
            SotcFragment::Get().Remove(reinterpret_cast<uintptr_t>((__bridge void*)out.temporary));
            *out.target=out.original;out.target=nullptr;
        }
        outputs.clear();
    }
};
} // namespace SotcFragmentWriteback
