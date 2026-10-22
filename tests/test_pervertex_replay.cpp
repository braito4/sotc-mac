// Original synthetic producer/consumer experiment for SPIRV-Cross PR #2699.
// This checks compiler contracts only; no Metal device or runtime buffers exist.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
#ifdef SOTC_SPIRV_EMITTER
#include <sirit/sirit.h>
std::vector<std::uint32_t> vertex_case() {
    Sirit::Module m(0x00010300);
    m.AddCapability(spv::Capability::Shader);
    m.SetMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450);
    const auto v = m.TypeVector(m.TypeFloat(32), 4);
    const auto input = m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Input, v), spv::StorageClass::Input);
    const auto color = m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output, v), spv::StorageClass::Output);
    const auto position = m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output, v), spv::StorageClass::Output);
    m.Decorate(input, spv::Decoration::Location, 0u);
    m.Decorate(color, spv::Decoration::Location, 0u);
    m.Decorate(position, spv::Decoration::BuiltIn, spv::BuiltIn::Position);
    const auto main = m.OpFunction(m.TypeVoid(), spv::FunctionControlMask::MaskNone, m.TypeFunction(m.TypeVoid()));
    const std::array interface{input, color, position};
    m.AddEntryPoint(spv::ExecutionModel::Vertex, main, "main", interface);
    m.AddLabel();
    const auto value = m.OpLoad(v, input);
    m.OpStore(color, value); m.OpStore(position, value);
    m.OpReturn(); m.OpFunctionEnd(); return m.Assemble();
}
std::vector<std::uint32_t> replay_fragment_case(unsigned vertices) {
    Sirit::Module m(0x00010300);
    m.AddCapability(spv::Capability::Shader);
    m.AddCapability(spv::Capability::FragmentBarycentricKHR);
    m.AddExtension("SPV_KHR_fragment_shader_barycentric");
    m.SetMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450);
    const auto v=m.TypeVector(m.TypeFloat(32),4), u=m.TypeUInt(32);
    const auto input=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Input,m.TypeArray(v,m.Constant(u,3u))),spv::StorageClass::Input);
    const auto output=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output,v),spv::StorageClass::Output);
    m.Decorate(input,spv::Decoration::Location,0u);
    m.Decorate(input,spv::Decoration::PerVertexKHR);
    m.Decorate(output,spv::Decoration::Location,0u);
    const auto main=m.OpFunction(m.TypeVoid(),spv::FunctionControlMask::MaskNone,m.TypeFunction(m.TypeVoid()));
    const std::array interface{input,output};
    m.AddEntryPoint(spv::ExecutionModel::Fragment,main,"main",interface);
    m.AddExecutionMode(main,spv::ExecutionMode::OriginUpperLeft);
    m.AddLabel();
    Sirit::Id sum{};bool has_sum=false;
    for(unsigned i=0;i<3;++i) if(vertices & (1u<<i)) {
        const auto value=m.OpLoad(v,m.OpAccessChain(m.TypePointer(spv::StorageClass::Input,v),input,m.Constant(u,i)));
        sum=has_sum ? m.OpFAdd(v,sum,value) : value;has_sum=true;
    }
    m.OpStore(output,sum);
    m.OpReturn();m.OpFunctionEnd();return m.Assemble();
}
#else
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "spirv_msl.hpp"
using namespace spirv_cross;
std::vector<std::uint32_t> replay_fragment_case(unsigned vertices);
std::vector<std::uint32_t> vertex_case();
void configure(CompilerMSL& compiler, bool capture = false) {
    auto options = compiler.get_msl_options();
    options.platform = CompilerMSL::Options::macOS;
    options.set_msl_version(2, 4);
    options.capture_output_to_buffer = capture;
    options.vertex_for_tessellation = capture;
    compiler.set_msl_options(options);
}
// Independent CPU ABI oracle. It does not execute the emitted Metal code.
void check_addressing(const MSLCapturedVertexLayout& layout) {
    std::vector<unsigned char> records(3*layout.stride);
    for(unsigned record=0;record<3;++record) for(const auto& field:layout.components) {
        assert(field.scalar_type==SPIRType::Float && field.byte_offset+4<=layout.stride);
        const float value=100*record+field.component+1;
        std::memcpy(records.data()+record*layout.stride+field.byte_offset,&value,4);
    }
    // Two tightly packed triplets: a Metal uint3 array would incorrectly use 16 bytes.
    const std::array<std::uint32_t,6> indices{2,0,1,1,2,0};
    const auto read=[&](std::uint64_t primitive,unsigned corner,unsigned component,
                        std::size_t table_bytes,std::size_t record_bytes) {
        if(corner>=3 || primitive>=table_bytes/12) throw std::out_of_range("triplet");
        const auto record=indices.at(primitive*3+corner);
        const auto& field=layout.components.at(component);
        const std::uint64_t offset=std::uint64_t(record)*layout.stride+field.byte_offset;
        if(offset>record_bytes || record_bytes-offset<4) throw std::out_of_range("record");
        float value;std::memcpy(&value,records.data()+offset,4);return value;
    };
    for(unsigned primitive=0;primitive<2;++primitive) for(unsigned corner=0;corner<3;++corner)
        for(unsigned component=0;component<4;++component)
            assert(read(primitive,corner,component,24,records.size())==100*indices[primitive*3+corner]+component+1);
    assert(read(0,0,0,24,records.size())+read(0,1,0,24,records.size())==202);
    unsigned rejected=0;
    for(unsigned bad=0;bad<4;++bad) {
        try { read(bad==0 ? UINT64_MAX : (bad==1 ? 1 : 0), bad==2 ? 3 : 0, 0,
                   bad==1 ? 23 : 24,bad==3 ? 2*layout.stride : records.size()); }
        catch(const std::out_of_range&) { ++rejected; }
    }
    assert(rejected==4);
    std::puts("PASS: 24 scalar ABI reads, reordered/reused corners, mixed sum and four range controls; CPU oracle only");
}
int main(int argc,char** argv) {
    assert(argc==2);
    const std::filesystem::path directory=argv[1];
    const auto save=[&](const std::string& name,const std::string& text) {
        std::ofstream file(directory/name);file<<text;assert(file);
    };
    CompilerMSL producer(vertex_case()); configure(producer, true);
    const auto captured=producer.compile();assert(!captured.empty());
    save("producer.metal",captured);
    const auto layout = producer.get_msl_captured_vertex_layout();
    assert(layout.stride >= 32 && layout.components.size() == 4 && layout.builtins.size() == 4);
    check_addressing(layout);
    MSLCapturedOutputReplayBinding replay_binding;
    replay_binding.vertex_buffer_index = 28;
    replay_binding.occurrence_buffer_index = 29;
    replay_binding.draw_parameters_buffer_index = 30;
    replay_binding.primitive_index_location = 15;
    CompilerMSL replay(vertex_case()); configure(replay);
    const auto replayed = replay.compile_captured_output_replay(layout, replay_binding);
    assert(replayed.find("spvReplayRecord") != std::string::npos);
    assert(replayed.find("locn15") != std::string::npos);
    save("replay.metal",replayed);
    { std::ofstream metadata(directory/"layout.json");
      metadata<<"{\"stride\":"<<layout.stride<<",\"components\":[";
      bool first=true;for(const auto& field:layout.components) {
          if(!first) metadata<<",";first=false;
          metadata<<"{\"location\":"<<field.location<<",\"component\":"<<field.component
                  <<",\"byte_offset\":"<<field.byte_offset<<",\"scalar_type\":"<<int(field.scalar_type)<<"}";
      }
      metadata<<"],\"occurrence_stride\":8,\"primitive_triplet_stride\":12,\"draw_parameters_bytes\":16}\n";assert(metadata);
    }
    for (unsigned vertices : {1u, 2u, 4u, 3u}) {
        bool rejected = false;
        try { CompilerMSL ordinary(replay_fragment_case(vertices)); configure(ordinary); ordinary.compile(); }
        catch (const CompilerError& error) {
            rejected = std::string(error.what()).find("PerVertexKHR") != std::string::npos;
        }
        assert(rejected); // Simply replacing the compiler does not enable support.
        MSLCapturedVertexLayout missing = layout; missing.components.clear();
        MSLPerVertexInputBinding input;
        input.vertex_buffer_index = 28; input.primitive_index_buffer_index = 29;
        input.primitive_index_location = 15;
        CompilerMSL consumer(replay_fragment_case(vertices)); configure(consumer);
        consumer.set_msl_per_vertex_input_buffer(layout, input);
        const auto consumed = consumer.compile();
        assert(!consumed.empty() && consumer.needs_per_vertex_input_buffer());
        assert(consumed.find("vertex_value") == std::string::npos);
        assert(consumed.find("dfdx") == std::string::npos && consumed.find("dfdy") == std::string::npos);
        save("fragment-"+std::to_string(vertices)+".metal",consumed);
        rejected = false;
        try {
            CompilerMSL invalid(replay_fragment_case(vertices)); configure(invalid);
            invalid.set_msl_per_vertex_input_buffer(missing, input); invalid.compile();
        } catch (const CompilerError&) { rejected = true; }
        assert(rejected);
        unsigned rejected_controls=0;
        for(unsigned kind=0;kind<6;++kind) {
            auto bad_layout=layout;auto bad_input=input;
            if(kind==0) bad_layout.stride=0;
            if(kind==1) bad_layout.components[0].byte_offset=layout.stride;
            if(kind==2) bad_layout.components[0].byte_offset=1;
            if(kind==3) bad_layout.components.push_back(bad_layout.components[0]);
            if(kind==4) bad_input.primitive_index_buffer_index=bad_input.vertex_buffer_index;
            if(kind==5) bad_input.vertex_buffer_index=31;
            try {
                CompilerMSL invalid(replay_fragment_case(vertices));configure(invalid);
                invalid.set_msl_per_vertex_input_buffer(bad_layout,bad_input);invalid.compile();
            } catch(const CompilerError&) { ++rejected_controls; }
        }
        assert(rejected_controls==6);
        std::printf("PASS: vertices_mask=%u, actual_producer_stride=%u, default_rejected=1, portable_compiled=1, missing_layout_rejected=1\n",
                    vertices, layout.stride);
    }
    std::puts("PASS: exported six Metal sources; 24 malformed layout/binding controls rejected. CPU conversion and ABI oracle only; no GPU, topology expansion, synchronization or pixels validated.");
}
#endif
