"""Run production Metal batch ownership methods against Objective-C CPU doubles.

No Metal framework, device, game data or graphics execution is used. Queue doubles
model the documented difference between retained and unretained command buffers.
"""
from pathlib import Path
import subprocess
from build_gpu import ROOT, source_path, build_path
from check_buffer_copy_chunks import function


def fixture(tree):
    base = tree / 'gpu/shadps4/video_core/renderer_metal'
    layer = (base / 'mtl_vk_layer.mm').read_text()
    frame = (base / 'mtl_frame.mm').read_text()
    commit = function(layer, 'void CommitOwned(')
    text = (ROOT / 'tests/metal_retention_fixture.mm.in').read_text()
    if 'FenceState' in commit:
        fence_support = ('#include <memory>\n#include <condition_variable>\n')
        text = fence_support + text
        support = ('struct FenceState { std::atomic<bool> signaled{false}; };\n'
                   'std::condition_variable fences_cv;\n' +
                   function(layer, 'void CompleteFence('))
        text = text.replace('@OWN@', support + '\n@OWN@')
        # Exercise the new native fence path with the real completion handler:
        # a submit with no command buffers and a submit with retained resources.
        extra = r'''
        auto fence = std::make_shared<Layer::FenceState>();
        Layer::CommitOwned(0, [&](unsigned) { return key; }, nullptr, fence);
        assert(pending.count == 1 && !fence->signaled.load());
        [(FixtureCommand*)pending[0] complete];
        assert(pending.count == 0 && fence->signaled.load());
        auto resource_fence = std::make_shared<Layer::FenceState>();
        Layer::OwnBuffer(key);
        __weak FixtureResource* fenced;
        @autoreleasepool { fenced = Encode(Layer::owned.at(key)->cmd); }
        Layer::CommitOwned(1, [&](unsigned) { return key; }, nullptr, resource_fence);
        assert(pending.count == 1 && fenced != nil && !resource_fence->signaled.load());
        [(FixtureCommand*)pending[0] complete];
        assert(pending.count == 0 && fenced == nil && resource_fence->signaled.load());
'''
        text = text.replace('        return early ? 2 : 0;', extra + '        return early ? 2 : 0;')
    fragments = {
        '@OWN@': function(layer, 'void OwnBuffer('),
        '@COMMIT_OWNED@': 'template <typename Get>\n' + commit,
        '@RETURN_SCRATCH@': function(frame, 'void ReturnScratchWhenDone('),
        '@CHUNK@': function(frame, 'void CommitChunk('),
    }
    # Keep the failure-reporting commit wrapper outside this ownership fixture:
    # it does not select the command buffer's retention policy.
    if 'CommitCommandBuffer(o);' not in fragments['@CHUNK@']:
        fragments['@CHUNK@'] = fragments['@CHUNK@'].replace(
            '[o.cmd commit];', 'CommitCommandBuffer(o);')
    if 'FindOwned(get(i))' in commit:
        fragments['@COMMIT_OWNED@'] = function(layer, 'Owned* FindOwned(') + '\n' + fragments['@COMMIT_OWNED@']
        text = text.replace('using VkCommandBuffer = void*;', 'using VkCommandBuffer = void*;\n#define VK_NULL_HANDLE nullptr')
    for name, source in fragments.items():
        text = text.replace(name, source)
    return text


def main():
    output = build_path(ROOT) / 'metal-retention-fixture'
    output.mkdir(parents=True, exist_ok=True)
    for label, tree, expected in (
        ('pinned', ROOT / 'runtime/upstream', 2),
        ('adapted', source_path(ROOT), 0),
    ):
        source = output / (label + '.mm')
        binary = output / label
        source.write_text(fixture(tree))
        subprocess.run(['clang++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
                        '-fobjc-arc', '-fblocks', '-mmacosx-version-min=15.0',
                        '-framework', 'Foundation', str(source), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        print(label + ': ' + result.stdout.strip(), flush=True)
        if result.returncode != expected:
            raise SystemExit(label + ': unexpected exit ' + str(result.returncode) + '\n' + result.stderr)
    print('PASS: initial and split production batches retain explicitly encoded resources; '
          'completion, reset and scratch ownership controls pass. CPU doubles only.')
    binary = output / 'native-probe'
    subprocess.run(['clang++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
                    '-fobjc-arc', '-fblocks', '-mmacosx-version-min=15.0',
                    '-I' + str(ROOT / 'runtime/src'), '-framework', 'Foundation',
                    str(ROOT / 'tests/metal_probe_fixture.mm'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
