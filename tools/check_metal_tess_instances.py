"""Exercise production tessellation instance routing and patch replay on CPU doubles.

Kernel execution, shader semantics and GPU pixels are outside this fixture.
"""
import os
import subprocess
from build_gpu import ROOT, source_path, build_path
from check_buffer_copy_chunks import function


def fixture(tree):
    base = tree / 'gpu/shadps4/video_core/renderer_metal'
    frame = (base / 'mtl_frame.mm').read_text()
    header = (base / 'mtl_frame.h').read_text()
    draw = function(frame, 'void TessellatedDraw(')
    # Run the original instance routing, then a CPU leaf in place of kernel encoding.
    route = draw[:draw.index('    if (!o.pass)')]
    params = draw[draw.index('    Metal::Tessellation::DrawParams params;'):
                  draw.index('    if (indexed)')]
    pending = draw[draw.index('    PendingPatch pending;'):]
    text = (ROOT / 'tests/metal_tess_instances_fixture.mm.in').read_text()
    return text.replace('@IMPLEMENTATION@', 'true' if 'pending.instance = first_instance;' in draw else 'false').replace('@PENDING@', function(header, 'struct PendingPatch {') + ';').replace(
        '@DRAW@', route + '''
    if (!o.pass) return;
''' + params + '''
    leaves.push_back({params.num_vertices, params.first_vertex, params.first_instance,
                      first, vertex_offset, indexed, first_index});
    id scratch = nil;
    const u64 scratch_offset = 64 * leaves.size();
    struct { u64 control_offset=1, patch_offset=2, factor_offset=3; u32 patches; } plan{1,2,3,patches};
''' + pending).replace('@FLUSH@', function(frame, 'void FlushPending('))


def main():
    output = build_path(ROOT) / 'metal-tess-instances-fixture'
    output.mkdir(parents=True, exist_ok=True)
    for label, tree, expected in (
        ('pinned', ROOT / 'runtime/upstream', 2),
        ('adapted', source_path(ROOT), 0),
    ):
        cpp = output / (label + '.mm')
        binary = output / label
        cpp.write_text(fixture(tree))
        subprocess.run(['clang++', '-std=c++20', '-O0', '-Wall', '-Wextra', '-Werror',
                        '-fobjc-arc', '-mmacosx-version-min=15.0', '-framework', 'Foundation',
                        str(cpp), '-o', str(binary)], check=True)
        for skip in (False, True):
            env = dict(os.environ, BB_TESS_INSTANCES='0' if skip else '1')
            result = subprocess.run([str(binary), 'skip' if skip else 'encode'], env=env,
                                    capture_output=True, text=True, timeout=15)
            print(label + ': ' + result.stdout.strip(), flush=True)
            if result.returncode != (0 if skip else expected):
                raise SystemExit(label + ': unexpected exit ' + str(result.returncode) + result.stderr)
    print('PASS: CPU instance routing and real pending-patch replay; GPU kernels/pixels untested.')


if __name__ == '__main__':
    main()
