"""Inspect GPU/host symbol coverage without loading a library or reading game input.

Symbol presence is only a linker prerequisite: it does not validate call signatures,
thread ownership, memory hooks, dependent libraries, or renderer initialization.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from build_gpu import BuildError, REQUIRED_EXPORTS, ROOT, private_path


def parse_symbols(output):
    """Read POSIX nm -g -P output from one non-fat binary, retaining C spelling."""
    defined, undefined = set(), set()
    for line in output.splitlines():
        fields = line.split()
        if not fields:
            continue
        if len(fields) < 2 or len(fields[1]) != 1:
            raise ValueError('Unexpected nm output; refusing an incomplete inventory.')
        name, kind = fields[:2]
        # Mach-O C names have one additional underscore; ELF fixture names do not.
        if name.startswith(('_runtime_', '_bbcpu_', '_bb_image_base', '_bbgpu_')):
            name = name[1:]
        if kind in ('U', 'w', 'v'):
            undefined.add(name)
        elif kind.isupper() and kind != 'N':
            defined.add(name)
    return defined, undefined


def host_symbol(name):
    return name.startswith(('runtime_', 'bbcpu_')) or name == 'bb_image_base'


def audit(library_symbols, executable_symbols):
    exports, imports = parse_symbols(library_symbols)
    host_exports, _ = parse_symbols(executable_symbols)
    required = {name for name in imports if host_symbol(name)}
    if not required:
        raise ValueError('No GPU runtime/CPU imports found; check the selected library.')
    missing = sorted(required - host_exports)
    absent_api = sorted(set(REQUIRED_EXPORTS) - exports)
    return dict(
        schema_version=1,
        required_host_symbols=sorted(required),
        present_host_symbols=sorted(required & host_exports),
        missing_host_symbols=missing,
        missing_gpu_exports=absent_api,
        symbol_coverage_complete=not missing and not absent_api,
        abi_verified=False,
        renderer_launched=False,
        game_frame_verified=False,
    )


def inspect(path):
    if not path.is_file():
        raise ValueError('Missing binary: ' + str(path))
    return subprocess.check_output(['nm', '-g', '-P', str(path)], text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, default=ROOT / 'work/gpu-build/libbbgpu.dylib')
    parser.add_argument('--executable', type=Path, default=ROOT / 'work/probe_entry_arm64')
    args = parser.parse_args()
    # Inspect binaries, not the earlier JSON build report: the report may be stale.
    result = audit(inspect(args.library), inspect(args.executable))
    work = ROOT / 'work'
    if work.is_symlink():
        raise ValueError('Refusing redirected private work directory.')
    work.mkdir(exist_ok=True)
    report = private_path(work, 'gpu-link-check.json')
    report.write_text(json.dumps(result, indent=2) + '\n')
    print('GPU host imports: %d present, %d missing.' %
          (len(result['present_host_symbols']), len(result['missing_host_symbols'])))
    for name in result['missing_host_symbols']:
        print('MISSING host symbol: ' + name)
    for name in result['missing_gpu_exports']:
        print('MISSING GPU export: ' + name)
    print('Report: ' + str(report))
    print('Symbol coverage does not verify ABI compatibility or game video; no library was loaded.')
    return 0 if result['symbol_coverage_complete'] else 2


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (BuildError, ValueError, OSError, subprocess.CalledProcessError) as error:
        print('GPU link check stopped: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
