"""Build an isolated two-file texture alias backport against verified local objects.

Does not change the control sources/library or dependency pins. The incremental
trial requires the existing native build; it is not a standalone release recipe.
"""
import hashlib
import json
import shlex
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'work/gpu-source-994846bb7bb5'
BUILD = ROOT / 'work/gpu-build-994846bb7bb5'
OUT = ROOT / 'work/texture-alias-trial-20261105'
PATCH = ROOT / 'tools/gpu_patches/sotc-texture-alias-trial.patch.json'
CONTROL = '49a92f56b86451b5a516290d5169cec52f98da60c58abf616721b4046ee99501'

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def prepare():
    if digest(BUILD/'libbbgpu.dylib') != CONTROL:
        raise RuntimeError('Control library differs; review before building this trial')
    spec = json.loads(PATCH.read_text())
    OUT.mkdir(parents=True, exist_ok=True)
    for f in spec['files']:
        src = BASE/f['path']; dst = OUT/f['path']
        if digest(src) != f['before']:
            raise RuntimeError('Unexpected source: '+str(src))
        dst.parent.mkdir(parents=True, exist_ok=True); dst.write_bytes(src.read_bytes())
        subprocess.run(['git', 'apply', '--whitespace=error', '-'], cwd=OUT,
                       input='\n'.join(f['lines'])+'\n', text=True, check=True)
        if digest(dst) != f['after']:
            raise RuntimeError('Patch output mismatch')
    return spec

def main():
    spec = prepare()
    commands = json.loads(subprocess.check_output(['ninja','-t','compdb'], cwd=BUILD))
    replacements = {}
    for f in spec['files']:
        src = BASE/f['path']; dst = OUT/f['path']
        matches = [c for c in commands if Path(c['file']) == src]
        if len(matches) != 1:
            raise RuntimeError('Ambiguous compile command: '+str(src))
        args = shlex.split(matches[0]['command']); index = args.index('-o')+1
        original_object = args[index]; obj = OUT/(src.stem+'.o')
        replacements[original_object] = str(obj)
        args[index] = str(obj)
        args = [str(dst) if a == str(src) else a for a in args]
        if '-MF' in args: args[args.index('-MF')+1] = str(obj)+'.d'
        args.extend(['-iquote',str(src.parent)])
        subprocess.run(args, cwd=BUILD, check=True)
    lines = subprocess.check_output(['ninja','-t','commands','libbbgpu.dylib'], cwd=BUILD,text=True).splitlines()
    args = shlex.split(lines[-1])
    if args[:2] != [':','&&'] or args[-2:] != ['&&',':']:
        raise RuntimeError('Unexpected link command layout')
    args = args[2:-2]
    if any(a in ('&&',';','|') for a in args): raise RuntimeError('Unsupported link pipeline')
    inputs = {}
    for a in args:
        if a.endswith(('.o','.a')) and (BUILD/a).is_file(): inputs[str(BUILD/a)] = digest(BUILD/a)
    for original in replacements:
        if args.count(original) != 1: raise RuntimeError('Missing trial object in link')
    args = [replacements.get(a,a) for a in args]
    args[args.index('-o')+1] = str(OUT/'libbbgpu.dylib')
    subprocess.run(args,cwd=BUILD,check=True)
    if any(digest(Path(p)) != h for p,h in inputs.items()): raise RuntimeError('Build inputs changed during link')
    if digest(BUILD/'libbbgpu.dylib') != CONTROL: raise RuntimeError('Control changed')
    (OUT/'trial-build.json').write_text(json.dumps({'control_sha256':CONTROL,
        'patch_sha256':digest(PATCH),'sources':spec,'object_inputs':inputs,
        'library_sha256':digest(OUT/'libbbgpu.dylib')},indent=2)+'\n')
    print('Built isolated renderer trial; control unchanged.')

if __name__ == '__main__': main()
