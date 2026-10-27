"""Actual checkpoint helper with Foundation event doubles; no Metal device."""
import os
import subprocess
from build_gpu import ROOT

def main():
    work=ROOT/'work/encoder-progress-fixture';work.mkdir(exist_ok=True)
    binary=work/'fixture'
    subprocess.run(['clang++','-std=c++20','-fobjc-arc','-framework','Foundation',
        '-I'+str(ROOT/'runtime/src'),str(ROOT/'tests/encoder_progress_fixture.mm'),'-o',str(binary)],check=True)
    for enabled,trace in [('0','1'),('1','0'),('1','1')]:
        subprocess.run([str(binary)],env=dict(os.environ,SOTC_GPU_ENCODER_PROGRESS=enabled,
            SOTC_GPU_SUBMIT_TRACE=trace),check=True,timeout=10)
    print('PASS: production encoder checkpoints strict opt-in, bounded signals, CPU/GPU progress distinction, independent events on rollover; Foundation doubles only.')
if __name__=='__main__':main()
