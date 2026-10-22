"""Repair a copied local workspace's installed paths without touching game input.

Uses the current checkout as the destination. Old roots may be supplied with
--from; later moves reuse the private relocation checkpoint. System dependencies
stay external. Historical execution records are preserved verbatim.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT/'work/consolidation/state.json'
MACHO = {b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
         b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'}


def remap(value, old_roots, root=ROOT):
    for old in sorted(old_roots, key=len, reverse=True):
        # Installed .pc files escape spaces for pkg-config's flag parser; CMake
        # and Mach-O load commands can retain the unescaped path instead.
        variants = {old, old.replace(' ', '\\ '), old.replace(' ', '\\\\ ')}
        for variant in sorted(variants, key=len, reverse=True):
            value = value.replace(variant, str(root))
    return value


def metadata(path, old_roots):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 8 << 20:
        return False
    try:
        data = path.read_text()
    except (UnicodeError, OSError):
        return False
    updated = remap(data, old_roots)
    if updated == data:
        return False
    path.write_text(updated)
    return True


def macho(path, old_roots):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open('rb') as stream:
        if stream.read(4) not in MACHO:
            return False
    arguments = []
    dependencies = subprocess.check_output(['otool', '-L', str(path)], text=True)
    for line in dependencies.splitlines()[1:]:
        old = line.strip().split(' (compatibility version', 1)[0]
        new = remap(old, old_roots)
        if old != new:
            arguments.extend(['-change', old, new])
    identities = subprocess.check_output(['otool', '-D', str(path)], text=True).splitlines()[1:]
    if identities:
        old = identities[0].strip()
        new = remap(old, old_roots)
        if old != new:
            arguments.extend(['-id', new])
    commands = subprocess.check_output(['otool', '-l', str(path)], text=True)
    for old in re.findall(r'cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (.*?) \(offset \d+\)', commands):
        new = remap(old, old_roots)
        if old != new:
            arguments.extend(['-rpath', old, new])
    if not arguments:
        return False
    subprocess.run(['install_name_tool', *arguments, str(path)], check=True, capture_output=True)
    subprocess.run(['codesign', '--force', '--sign', '-', str(path)], check=True, capture_output=True)
    return True


def relocate(old_roots):
    state = json.loads(STATE.read_text()) if STATE.exists() else {}
    old_roots = set(old_roots) | set(state.get('previous_roots', []))
    if state.get('active_root'):
        old_roots.add(state['active_root'])
    old_roots.discard(str(ROOT))
    if any(not Path(p).is_absolute() or len(Path(p).parts) < 4 for p in old_roots):
        raise ValueError('Previous roots must be specific absolute project paths.')
    lock = json.loads((ROOT/'tools/dependencies.json').read_text())
    key = lock['workspace_key']
    prefixes = [ROOT/f'work/gpu-source-{key}/deps/macos-arm64',
                ROOT/'work/deps-arm64', ROOT/'work/host-tools/prefix']
    changed = {'metadata': 0, 'macho': 0, 'links': 0}
    for prefix in prefixes:
        if not prefix.is_dir() or prefix.is_symlink():
            raise ValueError('Installed dependency prefix must be contained in this workspace.')
        for path in prefix.rglob('*'):
            if path.suffix in ('.pc', '.cmake', '.la', '.json', '.ini', '.sh') or path.name.endswith('-config'):
                changed['metadata'] += metadata(path, old_roots)
            changed['macho'] += macho(path, old_roots)
    library = ROOT/f'work/gpu-build-{key}/libbbgpu.dylib'
    changed['macho'] += macho(library, old_roots)
    external = []
    for directory, dirs, files in os.walk(ROOT, followlinks=False):
        dirs[:] = [name for name in dirs if Path(directory)/name != ROOT/'game']
        for name in dirs + files:
            path = Path(directory)/name
            if path.is_symlink():
                target = os.readlink(path)
                updated = remap(target, old_roots)
                if updated != target:
                    target_path = Path(updated)
                    # Relative internal links survive subsequent folder moves.
                    updated = os.path.relpath(target_path, path.parent)
                    path.unlink()
                    path.symlink_to(updated)
                    changed['links'] += 1
                if not path.resolve().is_relative_to(ROOT):
                    if path.relative_to(ROOT).as_posix() == 'work/gpu-tools/glslangValidator':
                        compiler = ROOT/'work/host-tools/prefix/bin/glslang'
                        if not compiler.is_file():
                            raise ValueError('The local host compiler is missing.')
                        path.unlink()
                        path.symlink_to(os.path.relpath(compiler, path.parent))
                        changed['links'] += 1
                    else:
                        external.append(str(path.relative_to(ROOT)))
            elif (name == 'config' and '.git' in path.parts) or name == 'alternates':
                changed['metadata'] += metadata(path, old_roots)
    if external:
        raise ValueError('Unresolved external workspace links: ' + ', '.join(external))
    state.update(active_root=str(ROOT), previous_roots=sorted(old_roots),
                 last_relocation=changed, external_workspace_links=external,
                 game_input_modified=False)
    STATE.parent.mkdir(parents=True, exist_ok=True)
    STATE.write_text(json.dumps(state, indent=2) + '\n')
    print('Workspace installed paths repaired:', json.dumps(changed))
    print('No external project symlinks; game input and historical logs were not edited.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--from', dest='previous', action='append', default=[])
    relocate(parser.parse_args().previous)
