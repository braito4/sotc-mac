"""Copy caches from a completed compatible run, never share writable cache files."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil

MARKER = 'shader-cache-identity.json'
COMPLETE = 'shader-cache-complete'
SETTINGS = ('BB_MSL_SAFE_MATH', 'BB_METAL_NATIVE_BOOT', 'BB_METAL_FRAME',
            'BB_POOL_SPLIT', 'BB_POOL_LAYOUT', 'BB_DIRECT_MEMORY_ACCESS',
            'BB_RENDER_RES', 'BB_OUTPUT_RES', 'BB_UPSCALER', 'BB_UPSCALE_PRESET',
            'BB_GC_BUDGET_MB', 'BB_RESIDENCY_SET', 'SOTC_METAL_INFLIGHT', 'SOTC_RENDER_VARIANT', 'SOTC_INDEX_REBASE', 'SOTC_INDIRECT_INDEX_REBASE', 'SOTC_INDIRECT_INDEX_SELECTIVE',
            'SOTC_TEST_REDUCTION_BARRIERS')


def digest(path):
    with open(path, 'rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def identity(library, image, env):
    return dict(schema=1, library=digest(library), image=digest(image),
                system=platform.platform(), macos=platform.mac_ver()[0],
                settings={key: env.get(key) for key in SETTINGS},
                toggles=digest(env['BB_TOGGLE_FILE']) if env.get('BB_TOGGLE_FILE') else None)


def prepare(state, expected):
    state = Path(state).resolve(strict=True)
    destination = state / 'cache'
    if destination.exists() or destination.is_symlink():
        raise ValueError('Refusing to replace an existing run cache')
    (state / MARKER).write_text(json.dumps(expected, sort_keys=True) + '\n')
    pattern = 'game-state-*' if state.name.startswith('game-state-') else 'gpu-game-state-*'
    candidates = sorted(state.parent.glob(pattern + '/' + COMPLETE),
                        key=lambda p: p.stat().st_mtime, reverse=True)
    for done in candidates:
        previous = done.parent
        if previous == state or previous.is_symlink() or done.is_symlink():
            continue
        try:
            marker = previous / MARKER
            cache = previous / 'cache'
            if marker.is_symlink() or json.loads(marker.read_text()) != expected:
                continue
            if not cache.is_dir() or cache.is_symlink():
                continue
            # Only regular files/directories from a private completed cache.
            entries = list(cache.rglob('*'))
            if any(p.is_symlink() or not (p.is_file() or p.is_dir()) for p in entries):
                continue
            shutil.copytree(cache, destination)
            print(f'Shader cache: copied compatible completed run {previous.name}; independent files')
            return True
        except (OSError, ValueError):
            # A partial copy must never be considered a valid seed.
            if destination.exists():
                raise
    print('Shader cache: cold run; no compatible completed cache available')
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state', type=Path, required=True)
    parser.add_argument('--library', type=Path)
    parser.add_argument('--image', type=Path)
    parser.add_argument('--complete', action='store_true')
    args = parser.parse_args()
    if args.complete:
        if (args.state / MARKER).is_file():
            (args.state / COMPLETE).touch(exist_ok=False)
    else:
        if not args.library or not args.image:
            parser.error('--library and --image are required to prepare a cache')
        prepare(args.state, identity(args.library, args.image, os.environ))


if __name__ == '__main__':
    main()
