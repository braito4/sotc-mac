"""Package an ARM64 runtime and its relocatable libraries, with user-owned game inputs supplied at launch."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

from build_gpu import ROOT, build_path, reference_path, source_path


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def dependencies(binary):
    return [line.strip().rsplit(' (compatibility version', 1)[0]
            for line in output('otool', '-L', binary).splitlines()[1:]]


def relocate(host, renderer, destination, library_dirs):
    pending = [(host, 'sotc-arm64'), (renderer, 'libbbgpu.dylib')]
    while pending:
        source, name = pending.pop()
        target = destination / name
        if target.exists():
            continue
        shutil.copyfile(source, target)
        target.chmod(0o755)
        if output('lipo', '-archs', target) != 'arm64':
            raise ValueError('Expected an ARM64-only binary: ' + name)
        original_id = output('otool', '-D', target).splitlines()[1:] if name.endswith('.dylib') else []
        changes = []
        if original_id:
            changes += ['-id', '@loader_path/' + name]
        for dep in dependencies(target):
            if dep in original_id or dep.startswith(('/System/Library/', '/usr/lib/')):
                continue
            soname = Path(dep).name
            candidates = [path / soname for path in library_dirs]
            resolved = next((p for p in candidates if p.is_file()), None)
            if not resolved:
                raise ValueError('Missing distribution dependency: ' + soname)
            pending.append((resolved, soname))
            changes += ['-change', dep, '@loader_path/' + soname]
        rpaths = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.+?) \(offset \d+\)',
                            output('otool', '-l', target))
        for path in set(rpaths):
            changes += ['-delete_rpath', path]
        if changes:
            subprocess.run(['install_name_tool', *changes, target], check=True)
        subprocess.run(['strip', '-S', '-x', target], check=True)
        subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', target], check=True)
        subprocess.run(['codesign', '--verify', '--strict', target], check=True)
        for dep in dependencies(target):
            if not dep.startswith(('/System/Library/', '/usr/lib/', '@loader_path/')):
                raise ValueError('Unresolved binary dependency: ' + name)
        data = target.read_bytes()
        if b'/Users/' in data or os.fsencode(Path.home()) + b'/' in data:
            raise ValueError('Home-directory path remains in ' + name +
                             '. Build dependencies with a neutral prefix.')


def package(destination, overrides, notices):
    tree, ref, build = source_path(), reference_path(), build_path()
    prefix = tree / 'deps/macos-arm64'
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent) as tmp:
        stage = Path(tmp) / 'SotC-Mac'
        (stage / 'bin').mkdir(parents=True)
        relocate(ROOT / 'work/sotc-arm64', build / 'libbbgpu.dylib', stage / 'bin',
                 [*overrides, prefix / 'lib'])
        def copy(src, rel):
            path = stage / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, path)
        copy(ROOT / 'tools/launch_runtime.sh', 'Jugar.command')
        (stage / 'Jugar.command').chmod(0o755)
        for name in ('runtime-settings.sh', 'hardware-memory.sh', 'reference-render-settings.sh'):
            copy(ROOT / 'tools' / name, 'support/' + name)
        for name in ('prepare_game.py', 'prepare_linked_arm64.py', 'run_gpu_diagnostic.py',
                     'gpu_utilization.py', 'dependencies.json'):
            copy(ROOT / 'tools' / name, 'support/tools/' + name)
        copy(ref / 'scripts/prepare.py', 'support/preparation/prepare.py')
        for name in ('prepare.py', 'link_modules.py', 'link_libc.py', 'game_check.py'):
            copy(tree / 'scripts' / name, 'support/linker/' + name)
        copy(ROOT / 'docs/BINARY.md', 'LEEME.md')
        copy(ROOT / 'docs/CREDITS.md', 'licenses/CREDITS.md')
        copy(ROOT / 'LICENSE', 'licenses/sotc-mac.txt')
        copy(tree / 'LICENSE', 'licenses/runtime.txt')
        copy(ref / 'LICENSE', 'licenses/preparation.txt')
        for lib in (prefix / 'share/licenses').glob('*/LICENSE*'):
            copy(lib, 'licenses/' + lib.parent.name + '-' + lib.name)
        for lib in (tree / 'gpu/third_party').glob('*/LICENSE*'):
            if lib.is_file():
                copy(lib, 'licenses/' + lib.parent.name + '-' + lib.name)
        if notices:
            for lib in sorted(notices.rglob('*')):
                if lib.is_file():
                    copy(lib, 'licenses/third-party/' + lib.relative_to(notices).as_posix())
        subprocess.run([stage / 'Jugar.command', '--check'], check=True)
        archive = Path(tmp) / 'runtime.zip'
        with ZipFile(archive, 'w', compression=ZIP_DEFLATED, compresslevel=6) as z:
            for path in sorted(stage.rglob('*')):
                if not path.is_file():
                    continue
                name = 'SotC-Mac/' + path.relative_to(stage).as_posix()
                info = ZipInfo(name)
                info.create_system = 3
                info.external_attr = (0o100755 if os.access(path, os.X_OK) else 0o100644) << 16
                info.compress_type = ZIP_DEFLATED
                z.writestr(info, path.read_bytes())
        archive.replace(destination)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    p.add_argument('--library-dir', action='append', type=Path, default=[])
    p.add_argument('--notices', type=Path)
    args = p.parse_args()
    if args.output.exists():
        p.error('Choose a new output file')
    package(args.output.resolve(), args.library_dir, args.notices)
