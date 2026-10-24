"""Exercise actual compute packet framing with guarded CPU input; no game/GPU."""
import json
import re
import subprocess
import tempfile
from pathlib import Path

from build_gpu import ROOT, source_path
from check_buffer_copy_chunks import function


PATH = Path('gpu/shadps4/video_core/amdgpu/liverpool.cpp')
PATCH = 'sotc-compute-packet-framing.patch.json'
DEFECTS = ('padding', 'three-parts', 'oversized-split', 'indirect-short',
           'indirect-isolation', 'invalid-type')


def framing(text):
    method = function(text, 'Liverpool::Task Liverpool::ProcessCompute(')
    start = method.index('auto* header = reinterpret_cast<const PM4Header*>(acb.data());')
    end = method.index('const PM4ItOpcode opcode = header->type3.opcode;', start)
    diagnostic = method.find('SotcShaderSubmit::Consume(shader_submit, header);', start, end)
    if diagnostic != -1:
        end = diagnostic
    return method[start:end]


def main():
    tree = source_path(ROOT)
    template = (ROOT/'tests/compute_packet_fixture.cpp.in').read_text()
    queue_header = (tree/'gpu/shadps4/video_core/amdgpu/liverpool.h').read_text()
    capacity = re.search(r'static constexpr size_t Pm4BufferSize = (\d+);', queue_header)[1]
    template = template.replace('@CAPACITY@', capacity)
    headers = (tree/'gpu/shadps4/video_core/amdgpu/pm4_cmds.h').read_text()
    start = headers.index('enum class PM4ShaderType')
    end = headers.index('// Write the PM4 header', start)
    template = template.replace('@HEADERS@', headers[start:end])
    with tempfile.TemporaryDirectory(prefix='compute-packets-', dir=ROOT/'work') as directory:
        output = Path(directory)
        previous = output/PATH
        previous.parent.mkdir(parents=True)
        previous.write_bytes((tree/PATH).read_bytes())
        patch = json.loads((ROOT/'tools/gpu_patches'/PATCH).read_text())
        subprocess.run(['git', '-C', str(output), 'apply', '--reverse'],
                       input='\n'.join(patch['lines'])+'\n', text=True,
                       check=True, capture_output=True)
        variants = [('previous', previous.read_text()), ('adapted', (tree/PATH).read_text())]
        for label, text in variants:
            cpp = output/(label+'.cpp')
            binary = output/label
            cpp.write_text(template.replace('@FRAMING@', framing(text)))
            subprocess.run(['clang++', '-std=c++23', '-O1', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                            '-I'+str(tree/'gpu/shadps4'), str(cpp), '-o', str(binary)], check=True)
            for mode in DEFECTS + (('valid',) if label=='adapted' else ('ordinary',)):
                run = subprocess.run([str(binary), mode], capture_output=True, text=True, timeout=10)
                if label=='previous' and mode in DEFECTS:
                    assert run.returncode != 0, (label, mode, run.stdout, run.stderr)
                    print('REPRODUCED: previous '+mode, flush=True)
                else:
                    assert run.returncode == 0, (label, mode, run.returncode, run.stdout, run.stderr)
                    print(run.stdout.strip(), flush=True)
        print('Actual production framing and PM4 header definitions; CPU only.', flush=True)


if __name__ == '__main__':
    main()
