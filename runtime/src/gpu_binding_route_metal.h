#pragma once
// Included after the actual Metal frame and shader types. No shared ABI changes.
#include "gpu_binding_route.h"
namespace SotcBindingRoute {
inline std::string Quote(const std::string& s){std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\')out+='\\';out+=c>=32?char(c):'?';}return out+'"';}
template<class Owned> void Before(Owned& o,const char* phase="before"){
    if(!current||!o.compute_pipeline||!o.compute_pipeline->shader)return;
    const auto& p=*current;const auto* shader=o.compute_pipeline->shader;
    std::ostringstream out;out<<"{\"schema\":1,\"phase\":"<<Quote(phase)<<",\"route\":"<<p.id<<",\"shader_request\":"<<p.shader
       <<",\"entry\":"<<Quote(shader->entry)<<",\"preparation_thread\":"<<p.thread<<",\"encoder_thread\":"<<Thread()
       <<",\"prepared_ns\":"<<p.prepared_ns<<",\"queued_ns\":"<<p.queued_ns<<",\"encoding_ns\":"<<Now()
       <<",\"command\":"<<reinterpret_cast<uintptr_t>((__bridge void*)o.cmd)
       <<",\"packet_truncated\":"<<(p.truncated?"true":"false")<<",\"bindings\":[";
    unsigned count=0;
    for(const auto& b:shader->bindings){
        if(b.kind!=Metal::ShaderBinding::Kind::Buffer)continue;
        if(count++==32)break;
        if(count>1)out<<',';
        auto it=o.compute_bindings.find((b.set<<16)|b.binding);
        Record audit;audit.route_sequence=p.id;audit.route_reason="unsupported_or_missing_binding";
        U64 metal=0,offset=0,bytes=0;
        if(b.set==0&&b.count==1&&it!=o.compute_bindings.end()&&it->second.elements.size()==1){
            const auto& v=it->second.elements[0];metal=reinterpret_cast<uintptr_t>((__bridge void*)v.buffer);offset=v.offset;bytes=v.size;
            audit=Find(b.binding,metal,offset,bytes);
        }
        out<<"{\"slot\":"<<b.msl_index<<",\"set\":"<<b.set<<",\"binding\":"<<b.binding<<",\"metal\":"<<metal<<",\"offset\":"<<offset<<",\"bytes\":"<<bytes<<",\"audit\":";
        SotcBindingAudit::Json(out,audit);
        const auto span=SotcBufferHistory::Get().NativeSpan(metal,offset,bytes);
        out<<",\"native_epoch\":"<<span.epoch<<",\"pool_known\":"<<(span.pool?"true":"false")
           <<",\"physical\":"<<span.physical<<",\"pool_epoch\":"<<span.pool_epoch<<'}';
    }
    out<<"],\"bindings_truncated\":"<<(count>32?"true":"false")<<",\"prepared_bindings\":[";
    for(unsigned i=0;i<p.count;++i){if(i)out<<',';const auto& b=p.bindings[i];
        out<<"{\"backend\":"<<b.backend<<",\"offset\":"<<b.offset<<",\"bytes\":"<<b.extent<<",\"descriptor_observations\":"<<b.descriptors<<",\"descriptor_status\":"<<Quote(b.reason)<<",\"record\":";
        SotcBindingAudit::Json(out,b.record);out<<'}';}
    out<<"],\"scope\":\"before: pre-guard bindings; encoded: dispatch bindings, including guard substitutions; not allocation ownership or byte-write proof\"}\n";
    Write(p.id,phase,out.str());
}
template<class Owned> void Encoded(Owned& o){
    if(!current)return;
    const U64 route_id=current->id,command=reinterpret_cast<uintptr_t>((__bridge void*)o.cmd);
    Before(o,"encoded");
    // Completion of this actual command buffer is separate from the encoding event.
    // No wait, resource reads, or guest writes are added by this callback.
    [o.cmd addCompletedHandler:^(id<MTLCommandBuffer> cb){
        std::string data="{\"schema\":1,\"route\":"+std::to_string(route_id)+",\"command\":"+std::to_string(command)+",\"phase\":\"completed\",\"ns\":"+std::to_string(Now())+",\"metal_status\":"+std::to_string(U64(cb.status))+",\"success\":"+(cb.status==MTLCommandBufferStatusCompleted?"true":"false")+"}\n";
        Write(route_id,"completed",data);
    }];
}
} // namespace SotcBindingRoute
