"""Native save contracts using synthetic bytes and the pinned file backend, no game."""
import os
from pathlib import Path
import subprocess
import tempfile
import host_build_manifest

ROOT = Path(__file__).resolve().parents[1]


def main():
    runtime = ROOT / 'work/gpu-source-994846bb7bb5'
    cpu = runtime / 'recomp'
    host_build_manifest.capture(runtime, cpu)
    with tempfile.TemporaryDirectory(prefix='sotc-save-contracts-') as tmp:
        root = Path(tmp)
        binary = root / 'fixture'
        subprocess.run(['clang', '-arch', 'arm64', '-std=gnu11', '-O2', '-Wall', '-Wextra',
                        '-Werror', '-pthread', '-DSOTC_SAVE_TRIAL=1',
                        '-I' + str(ROOT / 'runtime/src'), '-I' + str(runtime / 'src'),
                        '-I' + str(cpu / 'src/cpu'), str(ROOT / 'tools/tests/savedata_fixture.c'),
                        str(ROOT / 'runtime/src/bootstrap_savedata.c'),
                        str(ROOT / 'runtime/src/bootstrap_files.c'),
                        str(runtime / 'src/runtime_file.c'), '-o', str(binary)], check=True)
        game = root / 'game'; game.mkdir()
        saves = root / 'saves'; saves.mkdir()
        env = dict(os.environ, SOTC_SAVE_ROOT=str(saves), SOTC_SAVE_TRACE='1')
        for mode in ('disabled', 'write', 'read', 'interrupt', 'read'):
            result = subprocess.run([str(binary), mode, str(game)], env=env,
                                    capture_output=True, text=True, timeout=20)
            if result.returncode:
                raise RuntimeError(mode + '\n' + result.stdout + result.stderr)
            print('PASS:', mode)
        assert (saves / 'CUSA08809/SOTC_TEST/progress.dat').read_bytes() == b'CHECK001'
        assert (saves / 'CUSA08809/SOTC_TEST/subdir/empty.dat').read_bytes() == b''
        snapshots = list((saves / 'backups').glob('*/complete'))
        assert len(snapshots) == 1
        assert (snapshots[0].parent / 'data/progress.dat').read_bytes() == b'CHECK001'
        print('PASS: persisted bytes and backup survive interruption')
        process = subprocess.Popen([str(binary), 'hold', str(game)], env=env,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        try:
            # Bound readiness wait; do not allow a failed fixture to hang the runner.
            import select
            import time
            deadline = time.monotonic() + 5
            # Unbuffered OS reads avoid select missing TextIOWrapper-prefetched lines.
            ready = b''
            while b'READY\n' not in ready:
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not select.select([process.stdout], [], [], remaining)[0]:
                    raise RuntimeError('Save lock fixture did not become ready')
                part = os.read(process.stdout.fileno(), 4096)
                if not part:
                    raise RuntimeError('Save lock fixture exited before ready')
                ready += part
            subprocess.run([str(binary), 'locked', str(game)], env=env,
                           check=True, capture_output=True, timeout=10)
            print('PASS: concurrent process rejected')
        finally:
            process.communicate('\n', timeout=5)
    print('PASS: synthetic savedata contracts; SotC save/load remains unvalidated')


if __name__ == '__main__':
    main()
