"""Prepare a private BBPROBE5 image with supplied libc/Fios2 and optional gesture/NGS2 modules.

Uses bloodborne-runtime's link_modules.py unchanged. FS loads remain original:
bbcpu models the guest thread pointer, so no native x86 TLS rewrite is needed.
Generated images contain game code and must stay outside Git and shared files.
"""
from pathlib import Path
import argparse
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'runtime/upstream/scripts'
import os
GAME = Path(os.environ.get('SOTC_GAME_ROOT', ROOT / 'game/CUSA08809/SotC-PC/Game/CUSA08809')).expanduser().resolve()
ORIGINAL = ROOT / 'work/sotc-original/boot.bin'
OUT = ROOT / 'work/sotc-linked-arm64'
MODULES = ('libc.prx', 'libSceFios2.prx')

def without_empty_tls(metadata):
    """An empty PT_TLS declares no template; allocate no diagnostic TLS ID."""
    result = dict(metadata)
    result['ph'] = [p for p in metadata['ph'] if not
                    (p['type'] == 7 and p['filesz'] == 0 and p['memsz'] == 0)]
    return result

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-gesture-module', type=Path,
                        help='privately link the supplied original libSceSystemGesture.sprx')
    parser.add_argument('--out', type=Path, default=OUT)
    parser.add_argument('--ngs2-module', type=Path,
                        help='privately link the supplied original libSceNgs2.sprx')
    args = parser.parse_args(argv)
    out = args.out.expanduser().resolve()
    modules = list(MODULES)
    if not ORIGINAL.is_file():
        raise SystemExit('Prepare the original eboot as BBPROBE2 first.')
    with ORIGINAL.open('rb') as source:
        if source.read(8) != b'BBPROBE2':
            raise SystemExit('Prepare the original eboot as BBPROBE2 first.')
    for name in MODULES:
        if not (GAME / 'sce_module' / name).is_file():
            raise SystemExit(f'Missing supplied game module: {name}')
    sys.path.insert(0, str(SCRIPTS))
    import link_modules
    if args.system_gesture_module:
        gesture = args.system_gesture_module.expanduser().resolve()
        if not gesture.is_file():
            raise SystemExit('Missing supplied libSceSystemGesture.sprx; no gesture stub is substituted.')
        metadata = link_modules.module(gesture)
        identity = ('3pcAvmwKCvM', ('libSceSystemGesture', 1), ('libSceSystemGesture', 257))
        if metadata['missing'] or not any(s['section'] and s['type'] == 2 and
                s['identity'] == identity for s in metadata['symbols']):
            raise SystemExit('Unsupported supplied system-gesture module or incomplete load segments.')
        # Absolute module paths are supported by the pinned linker. The input
        # stays read-only; do not copy firmware into the game directory.
        modules.append(str(gesture))
    if args.ngs2_module:
        ngs2 = args.ngs2_module.expanduser().resolve()
        if not ngs2.is_file():
            raise SystemExit('Missing supplied libSceNgs2.sprx; no audio stub is substituted.')
        metadata = link_modules.module(ngs2)
        required = {('mPYgU4oYpuY', ('libSceNgs2', 1), ('libSceNgs2', 257)),
                    ('i0VnXM-C9fc', ('libSceNgs2', 1), ('libSceNgs2', 257))}
        provided = {s['identity'] for s in metadata['symbols'] if s['section'] and s['type'] == 2}
        if metadata['missing'] or not required <= provided:
            raise SystemExit('Unsupported supplied NGS2 module or incomplete load segments.')
        modules.append(str(ngs2))
    out.mkdir(parents=True, exist_ok=True)
    link = out / 'boot.bin'
    if link.exists() or link.is_symlink():
        if not link.is_symlink() or link.resolve() != ORIGINAL.resolve():
            raise SystemExit('Preserving an unexpected existing boot.bin in the linked output directory.')
    else:
        link.symlink_to(ORIGINAL)
    # Host-language metadata adapters only: no executable byte or input file is
    # patched. The pinned linker otherwise allocates TLS IDs for empty PT_TLS.
    read_module, patch_fs = link_modules.module, link_modules.patch_fs_loads
    try:
        link_modules.module = lambda path: without_empty_tls(read_module(path))
        link_modules.patch_fs_loads = lambda image, ph, base, relocs: 0
        link_modules.link(GAME, out, modules)
    finally:
        link_modules.module, link_modules.patch_fs_loads = read_module, patch_fs
    report = json.loads((out / 'link.json').read_text())
    if report['fs_loads_patched'] != 0:
        raise SystemExit('Unexpected TLS rewrite in the ARM64 input.')
    if [m['file'] for m in report['modules']] != modules:
        raise SystemExit('Unexpected linked module order.')
    if args.system_gesture_module and any(name.endswith('#m#m') for name in report['unresolved_imports']):
        raise SystemExit('Supplied system-gesture module did not resolve the game gesture imports.')
    if args.ngs2_module and any(name.endswith('#z#t') for name in report['unresolved_imports']):
        raise SystemExit('Supplied NGS2 module did not resolve the game NGS2 imports.')
    if 'b44anV2D7K0#j#j' in report['unresolved_imports']:
        raise SystemExit('Original Fios2 did not resolve sceFiosFHOpenSync.')
    print('Original FS loads preserved. Linked output is private game-derived data.')

if __name__ == '__main__':
    main()
