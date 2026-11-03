"""Extract the pinned CPU unchanged for the layout-only migration experiment."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def prepare():
    lock = json.loads((ROOT / 'tools/dependencies.json').read_text())
    revision = lock['cpu']['revision']
    source = ROOT / 'work' / ('gpu-source-' + lock['workspace_key']) / 'recomp'
    destination = ROOT / 'work' / ('cpu-layout-' + revision[:12])
    def git(*args):
        return subprocess.check_output(['git', '-C', str(source), *args])
    if git('rev-parse', 'HEAD').decode().strip() != revision:
        raise RuntimeError('CPU source does not match the pinned revision')
    names = git('ls-files', '-z', 'src/cpu', 'src/recomp/rc.h', 'tests').decode().split('\0')
    names = [name for name in names if name]
    hashes = {}
    for name in names:
        path = source / name
        if path.is_symlink() or path.read_bytes() != git('show', revision + ':' + name):
            raise RuntimeError('Modified CPU source: ' + name)
        hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    if 'src/cpu/bbcpu.h' not in hashes or 'src/recomp/rc.h' not in hashes:
        raise RuntimeError('Missing CPU ABI headers')
    manifest = {'revision': revision, 'repository': lock['cpu']['repository'],
                'purpose': 'layout-only; unchanged CPU and execution ABI', 'files': hashes}
    if destination.is_symlink():
        raise RuntimeError('Refusing redirected CPU layout')
    if destination.exists():
        actual = {str(path.relative_to(destination)) for folder in ('src', 'tests')
                  for path in (destination / folder).rglob('*') if path.is_file()}
        if actual != set(hashes):
            raise RuntimeError('Unexpected files in existing CPU layout')
        if json.loads((destination / 'layout.json').read_text()) != manifest:
            raise RuntimeError('Existing CPU layout manifest differs')
        for name, digest in hashes.items():
            path = destination / name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                raise RuntimeError('Existing CPU layout differs: ' + name)
    else:
        stage = Path(tempfile.mkdtemp(prefix='cpu-layout-', dir=ROOT / 'work'))
        try:
            for name in hashes:
                target = stage / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source / name, target)
            (stage / 'layout.json').write_text(json.dumps(manifest, indent=2) + '\n')
            stage.rename(destination)
        finally:
            if stage.exists():
                shutil.rmtree(stage)
    print(destination)


if __name__ == '__main__':
    prepare()
