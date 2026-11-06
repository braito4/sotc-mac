"""Offline checks for a public checkout; no work directory or game required."""
import json
from pathlib import Path
import re
import subprocess
import build_gpu

ROOT = Path(__file__).resolve().parents[1]


def check(root=ROOT):
    lock = json.loads((root / 'tools/dependencies.json').read_text())
    sources = json.loads((root / 'tools/host_sources.json').read_text())['components']
    for key, item in sources.items():
        assert re.fullmatch('[0-9a-f]{40}', item['revision']), key
        for field in ('revision', 'repository'):
            assert item[field] == lock[key][field], f'{key}: inconsistent {field}'
        for name, digest in item['files'].items():
            assert not Path(name).is_absolute() and '..' not in Path(name).parts, name
            assert re.fullmatch('[0-9a-f]{64}', digest), name
    assert lock['workspace_key'] == lock['runtime']['revision'][:12]
    assert build_gpu.REVISION == lock['runtime']['revision']
    assert build_gpu.backport_manifest()['base_revision'] == lock['runtime']['revision']
    names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z'], text=True).split('\0')
    for name in filter(None, names):
        path = Path(name)
        assert path.parts[0] not in ('work', 'logs', 'games', 'game', 'saves'), name
        assert path.suffix.lower() not in ('.elf', '.self', '.pkg', '.dylib', '.a', '.o', '.bin'), name
    print('PASS: component pins and public source boundaries (no gameplay claim)')


if __name__ == '__main__':
    check()
