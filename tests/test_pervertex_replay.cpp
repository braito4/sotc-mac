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
#else
#include "spirv_msl.hpp"
using namespace spirv_cross;
std::vector<std::uint32_t> fragment_case(unsigned vertices);
std::vector<std::uint32_t> vertex_case();
void configure(CompilerMSL& compiler, bool capture = false) {
    auto options = compiler.get_msl_options();
    options.platform = CompilerMSL::Options::macOS;
    options.set_msl_version(2, 4);
    options.capture_output_to_buffer = capture;
    options.vertex_for_tessellation = capture;
    compiler.set_msl_options(options);
}
int main() {
    CompilerMSL producer(vertex_case()); configure(producer, true);
    assert(!producer.compile().empty());
    const auto layout = producer.get_msl_captured_vertex_layout();
    assert(layout.stride >= 32 && layout.components.size() == 4 && layout.builtins.size() == 4);
    MSLCapturedOutputReplayBinding replay_binding;
    replay_binding.vertex_buffer_index = 28;
    replay_binding.occurrence_buffer_index = 29;
    replay_binding.draw_parameters_buffer_index = 30;
    replay_binding.primitive_index_location = 15;
    CompilerMSL replay(vertex_case()); configure(replay);
    const auto replayed = replay.compile_captured_output_replay(layout, replay_binding);
    assert(replayed.find("spvReplayRecord") != std::string::npos);
    assert(replayed.find("locn15") != std::string::npos);
    for (unsigned vertices : {1u, 2u, 4u, 3u}) {
        bool rejected = false;
        try { CompilerMSL ordinary(fragment_case(vertices)); configure(ordinary); ordinary.compile(); }
        catch (const CompilerError& error) {
            rejected = std::string(error.what()).find("PerVertexKHR") != std::string::npos;
        }
        assert(rejected); // Simply replacing the compiler does not enable support.
        MSLCapturedVertexLayout missing = layout; missing.components.clear();
        MSLPerVertexInputBinding input;
        input.vertex_buffer_index = 28; input.primitive_index_buffer_index = 29;
        input.primitive_index_location = 15;
        CompilerMSL consumer(fragment_case(vertices)); configure(consumer);
        consumer.set_msl_per_vertex_input_buffer(layout, input);
        const auto consumed = consumer.compile();
        assert(!consumed.empty() && consumer.needs_per_vertex_input_buffer());
        assert(consumed.find("vertex_value") == std::string::npos);
        assert(consumed.find("dfdx") == std::string::npos && consumed.find("dfdy") == std::string::npos);
        rejected = false;
        try {
            CompilerMSL invalid(fragment_case(vertices)); configure(invalid);
            invalid.set_msl_per_vertex_input_buffer(missing, input); invalid.compile();
        } catch (const CompilerError&) { rejected = true; }
        assert(rejected);
        std::printf("PASS: vertices_mask=%u, actual_producer_stride=%u, default_rejected=1, portable_compiled=1, missing_layout_rejected=1\n",
                    vertices, layout.stride);
    }
    std::puts("CPU conversion only; no Metal syntax check, GPU linking, topology, buffer ordering, memory or pixels validated.");
}
#endif
