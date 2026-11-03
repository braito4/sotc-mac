"""Materialize the reviewed standalone CPU commit without modifying the control."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, help='Existing local Git repository')
    args = parser.parse_args()
    selected = json.loads((ROOT / 'tools/dependencies.json').read_text())['standalone_cpu']
    revision = selected['revision']
    repo = args.repository or ROOT / 'work/bbcpu-repository'
    if not repo.exists():
        subprocess.run(['git', 'clone', '--no-checkout', selected['repository'], str(repo)], check=True)
    def git(*command):
        return subprocess.check_output(['git', '-C', str(repo), *command])
    # Explicit commit: never follow the moving default branch during a build.
    git('cat-file', '-e', revision + '^{commit}')
    names = git('ls-tree', '-r', '--name-only', revision).decode().splitlines()
    names = [n for n in names if n.startswith(('src/', 'tests/')) or n in ('LICENSE', 'README.md', 'build.sh')]
    destination = ROOT / 'work' / ('bbcpu-source-' + revision[:12])
    if destination.is_symlink():
        raise RuntimeError('Redirected CPU destination')
    contents = {name: git('show', revision + ':' + name) for name in names}
    # Check the entire destination before writing; never overwrite edits.
    for name, data in contents.items():
        path = destination / name
        if not path.resolve().is_relative_to(destination.resolve()) or path.is_symlink():
            raise RuntimeError('Redirected CPU file: ' + name)
        if path.exists() and path.read_bytes() != data:
            raise RuntimeError('Modified CPU source: ' + name)
    for folder in ('src', 'tests'):
        if (destination / folder).exists():
            for path in (destination / folder).rglob('*'):
                if path.is_file() and str(path.relative_to(destination)) not in contents:
                    raise RuntimeError('Unexpected CPU source: ' + str(path))
    for name, data in contents.items():
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    manifest = {'repository': selected['repository'], 'revision': revision,
                'files': {n: hashlib.sha256(data).hexdigest() for n, data in contents.items()}}
    (destination / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(destination)


if __name__ == '__main__':
    main()
