"""Reproduce half-rate output with the actual AudioOut adapter and a 1024-frame device.

Only clocks, memory reads and the backend are synthetic. No SDL, speakers, GPU or
guest/game input is loaded. The reviewed preceding adapter is read from Git.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PREVIOUS = '54d8b45ab63ca1664ff558cfb2183fc8b1a53c98'


def main():
    original = subprocess.check_output(['git', '-C', str(ROOT), 'show', PREVIOUS+':runtime/src/bootstrap_audio.c'])
    with tempfile.TemporaryDirectory(prefix='audio-quantum-', dir=ROOT/'work') as temp:
        work = Path(temp)
        old = work/'previous.c';old.write_bytes(original)
        for name, source, expected in (('previous', old, 2), ('adapted', ROOT/'runtime/src/bootstrap_audio.c', 0)):
            binary = work/name
            subprocess.run(['clang', '-std=gnu11', '-O1', '-Wall', '-Wextra', '-Werror', '-pthread',
                '-Dclock_gettime=sotc_fixture_clock_gettime', '-I'+str(ROOT/'runtime/src'),
                '-I'+str(ROOT/'runtime/upstream/src'), str(source), str(ROOT/'tests/test_audio_quantum.c'),
                '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10,
                env=dict(os.environ, SOTC_AUDIO_TRACE='0'))
            print(name+': '+result.stdout.strip())
            if result.returncode != expected:
                raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: {result.stderr}')
    print('PASS: original half-rate output reproduced; adaptive quantum restores simulated 48 kHz delivery without fabricating PCM')


if __name__ == '__main__':
    main()
