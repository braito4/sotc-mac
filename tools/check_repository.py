"""Check the public source index and local documentation links without game input."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
def git(*args):
    return subprocess.check_output(['git', '-C', str(root), *args])

entries = git('ls-files', '--stage', '-z').decode().split('\0')
expected_modules = {
    'runtime/upstream': 'b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548',
    'reference': '411fdd86dadce347dcc4443228b009ae9265a548',
}
allowed_root = {'.gitignore', '.gitmodules', 'README.md', 'LICENSE', 'AGENTS.md',
                'CONTRIBUTING.md', 'prueba_cpu_arm64.command', 'prueba_grafica_nativa.command'}
allowed_directories = ('runtime/src/', 'tests/', 'tools/', 'packaging/', 'docs/', '.github/')
errors, files, modules = [], [], {}
for entry in filter(None, entries):
    header, name = entry.split('\t', 1)
    mode, digest, stage = header.split()
    if stage != '0':
        errors.append(f'Unmerged entry: {name}')
    if mode == '160000':
        modules[name] = digest
        continue
    if (name not in allowed_root and not name.startswith(allowed_directories)
            and not re.fullmatch(r'prueba_[a-z0-9_]+\.command', name)):
        errors.append(f'Unexpected tracked path: {name}')
    if mode not in ('100644', '100755'):
        errors.append(f'Unexpected file mode: {name}')
        continue
    data = git('show', ':' + name)
    if len(data) > 1024 * 1024 or b'\0' in data:
        errors.append(f'Unexpected binary/large source: {name}')
        continue
    try:
        text = data.decode('utf8')
    except UnicodeDecodeError:
        errors.append(f'Non-text public file: {name}')
        continue
    if re.search(r'/' r'Users/[^\s"\']+|(?:ghp_|github_pat_)[A-Za-z0-9_]{20,}', text):
        errors.append(f'Private path or credential-like content: {name}')
    if name.endswith('.md'):
        targets = re.findall(r'\]\(([^\s)]+)', text) + re.findall(r'href="([^"]+)"', text)
        for target in targets:
            if '://' in target or target.startswith(('#', 'mailto:')):
                continue
            if not (root / name).parent.joinpath(target.split('#')[0]).exists():
                errors.append(f'Broken local documentation link in {name}: {target}')
    files.append(name)
if modules != expected_modules:
    errors.append('Submodule paths/revisions differ from the pinned source manifest.')
if not files:
    errors.append('No public source files are staged/tracked.')
if errors:
    raise SystemExit('\n'.join(errors))
print(f'PASS: {len(files)} public text sources/documents, two pinned submodules, local documentation links; no private output paths tracked.')
