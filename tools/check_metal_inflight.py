"""Exercise the native admission gate on CPU; real Metal timing/memory untested."""
import os
from pathlib import Path
import subprocess
import select
from build_gpu import ROOT, build_path, source_path
from check_buffer_copy_chunks import function


def main():
    root = build_path(ROOT) / 'inflight-fixture'
    root.mkdir(exist_ok=True)
    binary = root / 'gate'
    control = root/'control.c'
    control.write_text('#include "diagnostic_control.h"\n'
                       'int FixtureStartControl(void) { return sotc_diagnostic_control_start(); }\n')
    control_obj = root/'control.o'
    subprocess.run(['clang', '-I'+str(ROOT/'runtime/src'), '-c', str(control),
                    '-o', str(control_obj)], check=True)
    subprocess.run(['clang++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
                    '-pthread', '-I'+str(ROOT/'runtime/src'),
                    str(ROOT/'tests/metal_inflight.cpp'), str(control_obj), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=15)
    for value, expected in ((None, 8), ('0', 0), ('1', 1), ('8', 8), ('64', 64),
                            ('', None), ('-1', None), ('65', None), ('8x', None)):
        env = dict(os.environ)
        env.pop('SOTC_METAL_INFLIGHT', None)
        if value is not None:
            env['SOTC_METAL_INFLIGHT'] = value
        result = subprocess.run([str(binary), 'config'], env=env, capture_output=True,
                                text=True, timeout=5)
        assert result.returncode == (24 if expected is None else 0), (value, result.stderr)
        if expected is not None:
            assert result.stdout.strip() == 'limit='+str(expected)
    read_fd, write_fd = os.pipe()
    try:
        env = dict(os.environ, SOTC_DIAGNOSTIC_CONTROL_FD=str(read_fd))
        with subprocess.Popen([str(binary), 'control'], env=env, pass_fds=(read_fd,),
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE) as child:
            try:
                assert select.select([child.stdout], [], [], 3)[0], 'gate never blocked'
                assert b'admission blocked' in child.stdout.readline()
                os.write(write_fd, b'x')
            finally:
                os.close(write_fd)
                write_fd = -1
            out, err = child.communicate(timeout=3)
            assert child.returncode == 21 and b'parent requested termination' in err, (out, err)
    finally:
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)
    print('PASS: actual cooperative cancellation exits 21 while admission is blocked')
    # Run the production wrapper with Foundation doubles and real diagnostic headers.
    text = (source_path(ROOT)/'gpu/shadps4/video_core/renderer_metal/mtl_frame.mm').read_text()
    body = function(text, 'void CommitCommandBuffer(')
    source = root/'bridge.mm'
    source.write_text((ROOT/'tests/metal_inflight_bridge.mm.in').read_text().replace('@COMMIT@', body))
    bridge = root/'bridge'
    subprocess.run(['clang++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
                    '-fobjc-arc', '-fblocks', '-mmacosx-version-min=15.0',
                    '-I'+str(ROOT/'runtime/src'), '-framework', 'Foundation',
                    str(source), '-o', str(bridge)], check=True)
    env = dict(os.environ, SOTC_VISUAL_TRACE='0', SOTC_MEMORY_TRACE='0')
    subprocess.run([str(bridge)], env=env, check=True, timeout=15)
    failed = subprocess.run([str(bridge), 'error'], env=env, capture_output=True, text=True, timeout=10)
    assert failed.returncode == 23, failed.stderr
    assert 'reason=failure limit=8 reserved_or_inflight=1 peak=1 acquired=1 completed=0' in failed.stderr
    assert 'STOP_GPU_METAL_COMMAND' in failed.stderr and 'code=8' in failed.stderr
    print('PASS: native failure keeps fatal exit 23 and does not count as successful completion; '
          'GPU savings/performance pending')


if __name__ == '__main__':
    main()
