"""Verify pinned host inputs and record provenance without game data or Git writes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_component(source, component, *, cpu=False):
    source = Path(source)
    expected = component['files']
    if cpu:
        actual = {p.relative_to(source).as_posix() for p in (source / 'src/cpu').rglob('*')
                  if p.is_file()}
        wanted = {p for p in expected if p.startswith('src/cpu/')}
        if actual != wanted:
            raise ValueError('CPU file inventory differs from the reviewed source')
    for name, checksum in expected.items():
        path = source / name
        if (not path.resolve().is_relative_to(source.resolve()) or path.is_symlink()
                or not path.is_file() or digest(path) != checksum):
            raise ValueError('Unreviewed component input: ' + str(path))
    return {'repository': component['repository'], 'revision': component['revision'],
            'files': expected}


def capture(runtime, cpu, root=ROOT):
    inventory = json.loads((root / 'tools/host_sources.json').read_text())['components']
    lock = json.loads((root / 'tools/dependencies.json').read_text())
    for key, item in inventory.items():
        if any(item[field] != lock[key][field] for field in ('revision', 'repository')):
            raise ValueError('Host inventory and dependency lock disagree: ' + key)
    runtime_result = verify_component(runtime, inventory['runtime'])
    cpu_result = None
    for key in ('cpu', 'standalone_cpu'):
        try:
            cpu_result = verify_component(cpu, inventory[key], cpu=True)
            cpu_result['selection'] = key
            break
        except ValueError:
            pass
    if cpu_result is None:
        raise ValueError('CPU does not match either reviewed source; refusing to compile')
    local = list((root / 'runtime/src').glob('*'))
    local += [root / 'tools' / name for name in (
        'build_host.sh', 'build_cpu.sh', 'host_build_manifest.py', 'host_sources.json',
        'dependencies.json', 'prepare_cpu_scalar_support.py', 'prepare_cpu_probe_support.py',
        'prepare_runtime_support.py', 'prepare_memory_support.py')]
    return {'schema': 1, 'runtime': runtime_result, 'cpu': cpu_result,
            'options': {'SOTC_SAVE_TRIAL': os.environ.get('SOTC_SAVE_TRIAL', '0'),
                        'SOTC_POOL_PROBE': os.environ.get('SOTC_POOL_PROBE', '0')},
            'adapters': {p.relative_to(root).as_posix(): digest(p)
                         for p in sorted(local) if p.is_file()},
            'aot': 'not loaded', 'gameplay_validation': 'not established by this build'}


def finalize(captured, current, executable):
    if captured != current:
        raise ValueError('Host inputs changed during compilation; build not promoted')
    return dict(current, executable={'sha256': digest(executable)},
                compiler=subprocess.check_output(['clang', '--version'], text=True).strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--cpu', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--captured', type=Path)
    parser.add_argument('--executable', type=Path)
    args = parser.parse_args()
    if bool(args.captured) != bool(args.executable):
        parser.error('--captured and --executable must be used together')
    result = capture(args.runtime, args.cpu)
    if args.captured:
        result = finalize(json.loads(args.captured.read_text()), result, args.executable)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('Verified host inputs: runtime=' + result['runtime']['revision'][:12] +
          ' cpu=' + result['cpu']['revision'][:12])


if __name__ == '__main__':
    main()
