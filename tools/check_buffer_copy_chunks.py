"""Exercise actual copy methods and scheduler size guard with CPU recording doubles."""
from pathlib import Path
from build_gpu import source_path, build_path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    begin = text.index('{', start)
    # Functions can have braced parameter defaults; this helper also extracts
    # structs, whose first parenthesis may occur inside their body.
    parameters = text.find('(', start, begin)
    if parameters != -1:
        position, parentheses = parameters + 1, 1
        while parentheses:
            parentheses += (text[position] == '(') - (text[position] == ')')
            position += 1
        begin = text.index('{', position)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def fixture(tree):
    base = tree/'gpu/shadps4/video_core/renderer_vulkan'
    runtime = (base/'vk_runtime.cpp').read_text()
    scheduler = (base/'vk_scheduler.h').read_text()
    methods = '\n'.join(function(runtime[runtime.rindex('void Runtime::'+name+'('):], 'void Runtime::'+name+'(') for name in ('CopyBuffer', 'CopyFromGuestChunk'))
    guard = function(scheduler, '    void ReserveRecordData(size_t bytes)')
    constant = re.search(r'constexpr size_t MaxRecordedBufferCopies = \d+;\s*static_assert\([^;]+;', runtime)
    capacity = re.search(r'static constexpr size_t Capacity = [\d *]+;', scheduler)[0]
    return r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <list>
#include <span>
#include <stdexcept>
#include <vector>
using u64 = std::uint64_t;
#define ASSERT(x) do { if (!(x)) throw std::runtime_error("recording capacity"); } while (false)
namespace vk {
using Buffer = unsigned;
struct BufferCopy { u64 srcOffset, dstOffset, size; bool operator==(const BufferCopy&) const = default; };
enum class PipelineStageFlagBits2 { eCopy };
enum class AccessFlagBits2 { eTransferRead, eTransferWrite };
inline std::vector<BufferCopy> observed;
inline unsigned commands;
inline std::size_t largest;
struct CommandBuffer {
    void copyBuffer(Buffer src, Buffer dst, std::size_t count, const BufferCopy* copies) {
        assert((src == 1 || src == 7) && dst == 2);
        assert(count > 0); ++commands; largest = std::max(largest, count);
        observed.insert(observed.end(), copies, copies + count);
    }
};
}
namespace VideoCore { namespace Backend { using BufferCopy=vk::BufferCopy; enum class BufferAccess { TransferRead, TransferWrite }; } struct Buffer { unsigned id; unsigned Handle() const { return id; } }; }
struct RecordChunk { @CAPACITY@ std::size_t busy{Capacity - 64}; std::size_t Size() const { return busy; } };
struct Scheduler {
    RecordChunk chunk;
    std::list<std::vector<vk::BufferCopy>> data;
    std::vector<std::function<void(vk::CommandBuffer)>> commands;
    bool IsRecordingDeferred() const { return true; }
    RecordChunk& CurrentChunk() { return chunk; }
    void RetireChunk() { chunk.busy = 0; }
    @GUARD@
    std::span<const vk::BufferCopy> RecordData(std::span<const vk::BufferCopy> copies) {
        ReserveRecordData(copies.size_bytes() + alignof(vk::BufferCopy));
        chunk.busy += copies.size_bytes() + alignof(vk::BufferCopy);
        data.emplace_back(copies.begin(), copies.end()); return data.back();
    }
    void Record(std::function<void(vk::CommandBuffer)> command) {
        assert(chunk.busy + 128 <= RecordChunk::Capacity);
        chunk.busy += 128; commands.push_back(std::move(command));
    }
    void EndRendering() {}
    void Finish() { for (auto& command : commands) command({}); }
};
@CONSTANT@
constexpr bool Batched = @BATCHED@;
struct Encoder { void CopyBuffer(vk::CommandBuffer cmd, vk::Buffer src, vk::Buffer dst,
                                std::span<const vk::BufferCopy> regions) {
    cmd.copyBuffer(src,dst,regions.size(),regions.data());
} } test_encoder;
struct Runtime {
    Encoder* encoder=&test_encoder;
    Scheduler scheduler;
    unsigned barriers{}, accesses{};
    bool IsBufferAccessed(const VideoCore::Buffer*, u64, u64, bool = false) { return true; }
    void FlushBarriers() { ++barriers; }
    void AccessBuffer(const VideoCore::Buffer*, u64, u64, vk::PipelineStageFlagBits2, vk::AccessFlagBits2) { ++accesses; }
    void AccessBuffer(const VideoCore::Buffer*, u64, u64, VideoCore::Backend::BufferAccess) { ++accesses; }
    void CopyBuffer(const VideoCore::Buffer*, const VideoCore::Buffer*, std::span<const vk::BufferCopy>);
    void CopyFromGuestChunk(vk::Buffer, const VideoCore::Buffer*, std::span<const vk::BufferCopy>);
};
@METHODS@
int main() {
    bool overflow = false;
    for (unsigned count : {1u, 4095u, 4096u, 4097u, 5400u, 5500u, 10000u}) for (bool imported : {false, true}) {
        Runtime runtime; VideoCore::Buffer src{1}, dst{2};
        std::vector<vk::BufferCopy> copies;
        for (unsigned i = 0; i < count; ++i) copies.push_back({u64(i)*16, u64(i)*32, 4});
        vk::observed.clear(); vk::commands = 0; vk::largest = 0;
        try {
            if (imported) runtime.CopyFromGuestChunk(7, &dst, copies);
            else runtime.CopyBuffer(&src, &dst, copies);
            // The application's span may die before the queued recording callbacks.
            const auto expected = copies; std::fill(copies.begin(), copies.end(), vk::BufferCopy{});
            runtime.scheduler.Finish();
            assert(vk::observed == expected);
            assert(vk::commands == (Batched ? (count + 4095) / 4096 : 1));
            if (Batched) assert(vk::largest <= 4096);
            assert(runtime.barriers == 1);
            assert(runtime.accesses == (imported ? count : (count > 4 ? 2 : 2 * count)));
        } catch (const std::runtime_error&) { assert(count > 5400); overflow = true; }
    }
    std::printf("copy region overflow=%u; both production copy paths exercised; barriers/order/data checked\n", overflow);
    return overflow ? 2 : 0;
}
'''.replace('@CAPACITY@', capacity).replace('@GUARD@', guard).replace('@CONSTANT@', constant[0] if constant else '').replace('@METHODS@', methods).replace('@BATCHED@', 'true' if constant else 'false')


def main():
    work = ROOT/'work/buffer-copy-fixture'
    work.mkdir(exist_ok=True)
    for name, tree, expected in (('pinned', ROOT/'runtime/upstream', 2), ('adapted', source_path(ROOT), 0)):
        cpp, binary = work/(name+'.cpp'), work/name
        cpp.write_text(fixture(tree))
        subprocess.run(['clang++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
                        str(cpp), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=10)
        print(name+': '+result.stdout.strip())
        if result.returncode != expected:
            raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: {result.stderr}')
    print('PASS: oversized copies reproduce the pinned failure and fit after splitting; no GPU initialized')


if __name__ == '__main__':
    main()
