// Original inputs demonstrating signature lookup versus program association.
// No renderer, GPU, game address, captured bytes or proprietary shader input.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "binary_info_fixture.h"
#include "gpu_shader_metadata.h"

int main() {
    using Info=BinaryInfo;
    std::vector<std::byte> memory(1024);
    constexpr std::size_t foreign_origin=256, metadata=288;
    Info info{};info.signature=Info::signature_ref;info.length=16;info.shader_hash=0x1234;
    const std::uint32_t prologue[]={0xBEEB03FFu,3u};
    std::memcpy(memory.data()+foreign_origin,prologue,sizeof(prologue));
    std::memcpy(memory.data()+metadata,&info,sizeof(info));
    unsigned out_of_bounds{};
    const auto lookup=[&](std::size_t origin,bool available=true) {
        return Sotc::FindShaderMetadata<Info>(memory.size()-origin,
            [&](std::size_t offset,void* out,std::size_t bytes) {
                if(offset>memory.size()-origin || bytes>memory.size()-origin-offset) {
                    ++out_of_bounds;return false;
                }
                if(!available)return false;
                std::memcpy(out,memory.data()+origin+offset,bytes);return true;
            });
    };
    const auto correct=lookup(foreign_origin);
    assert(correct.found && correct.offset==32 && correct.hint==32 && !correct.scanned);
    assert(correct.info.length==16 && correct.info.shader_hash==info.shader_hash);

    // Start at an explicitly unrelated zero-filled prefix. The real program
    // and its metadata follow in the same mapped range. Previous signature-only
    // search borrowed their identity. The candidate now stays evidence only.
    const auto unrelated=lookup(0);
    assert(!unrelated.found && unrelated.candidate_found && unrelated.offset==metadata && unrelated.scanned);
    assert(unrelated.association==Sotc::ShaderMetadataAssociation::MissingPrologue);
    assert(unrelated.head[0]==0 && unrelated.info.shader_hash==correct.info.shader_hash);
    assert(std::memcmp(memory.data(),memory.data()+foreign_origin,info.length)!=0);
    std::puts("PASS: mapped non-program prefix cannot borrow a later program's metadata");

    // Even the expected first token is insufficient if its literal points to
    // unavailable metadata. The scanner must not supply a neighbor instead.
    const std::uint32_t wrong_hint[]={0xBEEB03FFu,15u};
    std::memcpy(memory.data(),wrong_hint,sizeof(wrong_hint));
    const auto misleading=lookup(0);
    assert(!misleading.found && misleading.candidate_found && misleading.hint==128 && misleading.offset==metadata);
    assert(misleading.association==Sotc::ShaderMetadataAssociation::InvalidMetadata);
    std::puts("PASS: conflicting prologue hint cannot borrow neighbor metadata");

    assert(!lookup(0,false).found);
    memory[metadata]=std::byte{};
    assert(!lookup(0).found && !lookup(foreign_origin).found);
    assert(!out_of_bounds);
    std::puts("PASS: correct hinted origin, wrong-origin association, bad-hint neighbor, missing-signature and unavailable-read controls; CPU only");
}
