"""Prepare a private BBPROBE5 diagnostic image with the supplied libc and Fios2.

Uses bloodborne-runtime's link_modules.py unchanged. FS loads remain original:
bbcpu models the guest thread pointer, so no native x86 TLS rewrite is needed.
Generated images contain game code and must stay outside Git and shared files.
"""
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'runtime/upstream/scripts'
import os
GAME = Path(os.environ.get('SOTC_GAME_ROOT', ROOT / 'game/CUSA08809/SotC-PC/Game/CUSA08809')).expanduser().resolve()
ORIGINAL = ROOT / 'work/sotc-original/boot.bin'
OUT = ROOT / 'work/sotc-linked-arm64'
MODULES = ('libc.prx', 'libSceFios2.prx')

def main():
    if not ORIGINAL.is_file() or ORIGINAL.open('rb').read(8) != b'BBPROBE2':
        raise SystemExit('Prepare the original eboot as BBPROBE2 first.')
    for name in MODULES:
        if not (GAME / 'sce_module' / name).is_file():
            raise SystemExit(f'Missing supplied game module: {name}')
    OUT.mkdir(parents=True, exist_ok=True)
    link = OUT / 'boot.bin'
    if link.exists() or link.is_symlink():
        if not link.is_symlink() or link.resolve() != ORIGINAL.resolve():
            raise SystemExit('Preserving an unexpected existing boot.bin in the linked output directory.')
    else:
        link.symlink_to(ORIGINAL)
    sys.path.insert(0, str(SCRIPTS))
    import link_modules
    # Host-language adapter only: no upstream source or game input is patched.
    link_modules.patch_fs_loads = lambda image, ph, base, relocs: 0
    link_modules.link(GAME, OUT, MODULES)
    report = json.loads((OUT / 'link.json').read_text())
    if report['fs_loads_patched'] != 0:
        raise SystemExit('Unexpected TLS rewrite in the ARM64 input.')
    if [m['file'] for m in report['modules']] != list(MODULES):
        raise SystemExit('Unexpected linked module order.')
    if 'b44anV2D7K0#j#j' in report['unresolved_imports']:
        raise SystemExit('Original Fios2 did not resolve sceFiosFHOpenSync.')
    print('Original FS loads preserved. Linked output is private game-derived data.')

if __name__ == '__main__':
    main()
