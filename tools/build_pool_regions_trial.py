"""Isolated merged-pool-region experiment; recompile every cache-layout dependent.

Uses the verified local control objects and cumulative small-copy overlay. No pins,
default library, host or original build object are modified. Native comparison is
opt-in via SOTC_POOL_REGIONS=1; previous single/split modes remain available.
"""
import concurrent.futures
import json
import shlex
import subprocess
from pathlib import Path
import build_texture_alias_trial as build

build.OUT = build.ROOT / 'work/pool-regions-trial-20261112'
build.PATCH = build.ROOT / 'tools/gpu_patches/sotc-pool-regions-trial.patch.json'
PRIOR = build.ROOT / 'work/small-copy-trial-20261109/trial-build.json'


def main():
    prior = json.loads(PRIOR.read_text())
    headers = dict(prior['diagnostic_headers'])
    header_changes = json.loads(build.PATCH.read_text()).get('diagnostic_header_changes', {})
    for name, expected in headers.items():
        change = header_changes.get(name)
        if change and change['before'] != expected:
            raise RuntimeError('Unexpected prior diagnostic header: ' + name)
        current = build.digest(build.ROOT / 'runtime/src' / name)
        if current != (change['after'] if change else expected):
            raise RuntimeError('Prior diagnostic header changed: ' + name)
        headers[name] = current
    for name, change in header_changes.items():
        if name not in headers:
            if change['before'] is not None or build.digest(build.ROOT / 'runtime/src' / name) != change['after']:
                raise RuntimeError('Unexpected added diagnostic header: ' + name)
            headers[name] = change['after']
    headers['gpu_pool_regions.h'] = build.digest(build.ROOT / 'runtime/src/gpu_pool_regions.h')
    spec = build.prepare()
    commands = json.loads(subprocess.check_output(['ninja', '-t', 'compdb'], cwd=build.BUILD))
    deps = subprocess.check_output(['ninja', '-t', 'deps'], cwd=build.BUILD, text=True)
    cache_header = str(build.BASE / 'gpu/shadps4/video_core/buffer_cache/buffer_cache.h')
    affected = set()
    for entry in deps.split('\n\n'):
        lines = entry.splitlines()
        if lines and cache_header in {line.strip() for line in lines[1:]}:
            if '(VALID)' not in lines[0]:
                raise RuntimeError('Stale dependency graph: ' + lines[0])
            affected.add(lines[0].split(': #deps', 1)[0])
    if not affected:
        raise RuntimeError('Missing cache layout dependency graph')
    changed = {str(build.BASE / f['path']) for f in spec['files'] if not f['path'].endswith('.h')}
    replacements = {}
    jobs = []
    for command in commands:
        args = shlex.split(command['command'])
        if '-o' not in args:
            continue
        index = args.index('-o') + 1
        original = args[index]
        if original not in affected and command['file'] not in changed:
            continue
        src = Path(command['file'])
        relative = src.relative_to(build.BASE)
        patched = build.OUT / relative
        if not patched.exists():
            patched.parent.mkdir(parents=True, exist_ok=True)
            patched.write_bytes(src.read_bytes())
        obj = build.OUT / 'objects' / original
        obj.parent.mkdir(parents=True, exist_ok=True)
        replacements[original] = str(obj)
        args[index] = str(obj)
        args = [str(patched) if a == str(src) else a for a in args]
        args[1:1] = ['-I' + str(build.OUT / 'gpu/shadps4')]
        if '-MF' in args:
            args[args.index('-MF') + 1] = str(obj) + '.d'
        args.extend(['-iquote', str(src.parent)])
        jobs.append((str(relative), args))
    if not affected.issubset(replacements):
        raise RuntimeError('A dependent object was not rebuilt')
    lines = subprocess.check_output(['ninja', '-t', 'commands', 'libbbgpu.dylib'], cwd=build.BUILD, text=True).splitlines()
    args = shlex.split(lines[-1])
    if args[:2] != [':', '&&'] or args[-2:] != ['&&', ':']:
        raise RuntimeError('Unexpected link command')
    args = args[2:-2]
    if any(a in ('&&', ';', '|') for a in args):
        raise RuntimeError('Unsupported link pipeline')
    inputs = {str(build.BUILD / a): build.digest(build.BUILD / a)
              for a in args if a.endswith(('.o', '.a')) and (build.BUILD / a).is_file()}
    def compile_one(job):
        name, command = job
        print('Compiling ' + name, flush=True)
        subprocess.run(command, cwd=build.BUILD, check=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers:
        list(workers.map(compile_one, jobs))
    for original in affected:
        dependencies = Path(replacements[original] + '.d').read_text()
        if str(build.OUT / 'gpu/shadps4/video_core/buffer_cache/buffer_cache.h') not in dependencies or cache_header in dependencies:
            raise RuntimeError('Wrong cache header in rebuilt object: ' + original)
    for original in replacements:
        if args.count(original) != 1:
            raise RuntimeError('Missing rebuilt object in link: ' + original)
    args = [replacements.get(a, a) for a in args]
    args[args.index('-o') + 1] = str(build.OUT / 'libbbgpu.dylib')
    subprocess.run(args, cwd=build.BUILD, check=True)
    if any(build.digest(Path(p)) != h for p, h in inputs.items()):
        raise RuntimeError('Original build inputs changed')
    if build.digest(build.BUILD / 'libbbgpu.dylib') != build.CONTROL:
        raise RuntimeError('Control changed')
    if any(build.digest(build.ROOT / 'runtime/src' / name) != h for name, h in headers.items()):
        raise RuntimeError('Diagnostic header changed during build')
    (build.OUT / 'trial-build.json').write_text(json.dumps({
        'control_sha256': build.CONTROL, 'sources': spec,
        'patch_sha256': build.digest(build.PATCH), 'diagnostic_headers': headers,
        'object_inputs': inputs, 'rebuilt_objects': replacements,
        'cache_layout_dependents': sorted(affected),
        'library_sha256': build.digest(build.OUT / 'libbbgpu.dylib'),
        'runtime_switch': 'SOTC_POOL_REGIONS=1; default off',
    }, indent=2) + '\n')
    print('Built isolated pool-regions renderer; all cache-layout dependents rebuilt; control unchanged.')


if __name__ == '__main__':
    main()
