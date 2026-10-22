// Private CPU conversion experiment using a separately pinned compiler proposal.
// This neither changes runtime dependencies nor creates a Metal/Vulkan device.
#include "spirv_msl.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace spirv_cross;
static std::vector<uint32_t> Read(const char* name) {
    std::ifstream file(name,std::ios::binary|std::ios::ate);auto size=file.tellg();
    if(!file || size<20 || size>8*1024*1024 || size%4) throw std::runtime_error("invalid module size");
    std::vector<uint32_t> words(static_cast<size_t>(size)/4);file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(words.data()),size) || words[0]!=0x07230203 || !words[3] || words[3]>1000000)
        throw std::runtime_error("invalid module");
    for(size_t i=5;i<words.size();) {auto n=words[i]>>16;if(!n || n>words.size()-i)throw std::runtime_error("truncated instruction");i+=n;}
    return words;
}
static void Configure(CompilerMSL& compiler,bool capture=false) {
    auto options=compiler.get_msl_options();options.platform=CompilerMSL::Options::macOS;
    options.set_msl_version(3,1);options.texture_1D_as_2D=true;options.argument_buffers=false;
    options.capture_output_to_buffer=capture;options.vertex_for_tessellation=capture;
    compiler.set_msl_options(options);
}
int main(int argc,char** argv) {
    if(argc!=4)return 64;
    std::string phase="read";
    try {
        auto vs=Read(argv[1]),fs=Read(argv[2]);
        CompilerMSL producer(vs),consumer(fs);Configure(producer,true);Configure(consumer);
        if(producer.get_execution_model()!=spv::ExecutionModelVertex || consumer.get_execution_model()!=spv::ExecutionModelFragment)
            throw std::runtime_error("not a vertex/fragment pair");
        // Reserve an unused private location in both stages. Buffer slots here
        // are experimental only, not qualified against the renderer's binding ABI.
        std::set<unsigned> used;
        for(const auto& resource:producer.get_shader_resources().stage_outputs)
            if(producer.has_decoration(resource.id,spv::DecorationLocation))used.insert(producer.get_decoration(resource.id,spv::DecorationLocation));
        for(const auto& resource:consumer.get_shader_resources().stage_inputs)
            if(consumer.has_decoration(resource.id,spv::DecorationLocation))used.insert(consumer.get_decoration(resource.id,spv::DecorationLocation));
        unsigned location=31;while(used.contains(location)){if(!location)throw std::runtime_error("no private location");--location;}
        phase="capture";auto captured=producer.compile();auto layout=producer.get_msl_captured_vertex_layout();
        MSLCapturedOutputReplayBinding replay_binding;
        replay_binding.vertex_buffer_index=28;replay_binding.occurrence_buffer_index=29;
        replay_binding.draw_parameters_buffer_index=30;replay_binding.primitive_index_location=location;
        CompilerMSL replay(vs);Configure(replay);phase="replay";
        auto replayed=replay.compile_captured_output_replay(layout,replay_binding);
        MSLPerVertexInputBinding binding;binding.vertex_buffer_index=28;
        binding.primitive_index_buffer_index=29;binding.primitive_index_location=location;
        phase="fragment";consumer.set_msl_per_vertex_input_buffer(layout,binding);auto fragment=consumer.compile();
        if(!consumer.needs_per_vertex_input_buffer())throw std::runtime_error("no active portable inputs");
        const std::filesystem::path output=argv[3];
        for(const auto& [name,source]:{std::pair{"capture.metal",captured},std::pair{"replay.metal",replayed},std::pair{"fragment.metal",fragment}}) {
            std::ofstream file(output/name);file<<source;if(!file)throw std::runtime_error("output failure");
        }
        std::cout<<"converted\n";return 0;
    } catch(const std::exception& error) {std::cerr<<phase<<": "<<error.what()<<'\n';return 1;}
}
