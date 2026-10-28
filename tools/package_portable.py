"""Build one private macOS self-extracting .command with native code and game data.

The output contains the owner's game and firmware-derived image: never publish it.
No Python, Homebrew, compiler or network is needed on the destination Mac.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADER_SIZE = 16384


def call(args, env=None):
    return subprocess.check_output(list(map(str, args)), text=True, env=env)


def dependencies(path):
    return [line.strip().split(' (compatibility')[0]
            for line in call(['otool', '-L', path]).splitlines()[1:]]


def system_library(name):
    return name.startswith(('/usr/lib/', '/System/Library/'))


def relocate(host, library, target):
    """Copy the entire non-system dependency closure and remove build-machine paths."""
    target.mkdir()
    shutil.copy2(host, target / 'sotc-arm64')
    queue = [library.resolve()]
    copied = {}
    while queue:
        original = queue.pop()
        name = original.name
        if name in copied:
            if copied[name] != original:
                raise ValueError('Conflicting library basename: ' + name)
            continue
        copied[name] = original
        shutil.copy2(original, target / name)
        identity = call(['otool', '-D', original]).splitlines()[1:]
        for dep in dependencies(original):
            if system_library(dep) or dep in identity:
                continue
            if dep.startswith('@loader_path/'):
                resolved = original.parent / dep.removeprefix('@loader_path/')
            elif dep.startswith('/'):
                resolved = Path(dep)
            else:
                raise ValueError('Unresolved library path: ' + dep)
            queue.append(resolved.resolve(strict=True))
    for binary in target.iterdir():
        if call(['lipo', '-archs', binary]).strip() != 'arm64':
            raise ValueError('Not ARM64: ' + str(binary))
        if binary.suffix == '.dylib':
            call(['install_name_tool', '-id', '@loader_path/' + binary.name, binary])
        for dep in dependencies(binary):
            if system_library(dep):
                continue
            # Link names can be symlinks; use the actual copied basename.
            name = Path(dep).resolve().name if dep.startswith('/') else Path(dep).name
            if name not in copied:
                raise ValueError('Unbundled library: ' + dep)
            call(['install_name_tool', '-change', dep, '@loader_path/' + name, binary])
        call(['codesign', '--force', '--sign', '-', binary])
        call(['codesign', '--verify', '--strict', binary])
        if any(not system_library(d) and not d.startswith('@loader_path/')
               for d in dependencies(binary)):
            raise ValueError('Nonportable dependency remains')
    return copied


def launcher(digest, size, needed):
    return f'''#!/bin/bash
# Private SotC package: contains your game files. Do not publish or upload.
set -euo pipefail
umask 077
case "${{1:-}}" in ''|--verify|--check) ;; *) echo 'Usage: bash SotC-Mac.command [--verify|--check]' >&2; exit 2;; esac
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || {{ echo 'Requires an Apple Silicon Mac.' >&2; exit 1; }}
version=$(sw_vers -productVersion)
[[ "${{version%%.*}}" -ge 15 ]] || {{ echo 'Requires macOS 15 or newer.' >&2; exit 1; }}
package='{digest}'
payload() {{ /bin/dd if="$0" ibs={HEADER_SIZE} skip=1 obs=1048576 2>/dev/null; }}
verify() {{
    echo 'Verifying the portable package (this may take a few minutes)...'
    actual=$(payload | /usr/bin/openssl dgst -sha256)
    [[ "${{actual##* }}" == "$package" ]] || {{ echo 'Package damaged or incomplete.' >&2; exit 1; }}
}}
[[ "$(stat -f %z "$0")" == '{size + HEADER_SIZE}' ]] || {{ echo 'Incomplete package.' >&2; exit 1; }}
if [[ "${{1:-}}" == --verify ]]; then verify; echo 'Package integrity OK.'; exit 0; fi
base="${{SOTC_INSTALL_ROOT:-$HOME/Library/Application Support/SotC Native}}"
[[ "$base" == /* ]] || {{ echo 'Installation path must be absolute.' >&2; exit 1; }}
mkdir -p "$base"
destination="$base/${{package:0:16}}"
if [[ ! -f "$destination/.complete" ]]; then
    verify
    free_kb=$(df -Pk "$base" | awk 'END {{print $4}}')
    [[ "$free_kb" -ge {needed // 1024 + 2097152} ]] || {{ echo 'Not enough free disk space to extract the game (plus 2 GiB reserve).' >&2; exit 1; }}
    stage=$(mktemp -d "$base/.extract-XXXXXX")
    trap 'rm -rf -- "$stage"' EXIT
    echo 'Extracting game and native runtime to the internal disk...'
    payload | /usr/bin/tar -xzf - -C "$stage"
    [[ ! -e "$destination" ]] || {{ echo 'An incomplete or concurrent installation exists.' >&2; exit 1; }}
    printf '%s\\n' "$package" > "$stage/.complete"
    mv -- "$stage" "$destination"
    trap - EXIT
fi
if [[ "${{1:-}}" == --check ]]; then
    mkdir -p "$destination/state"
    BB_GPU_USER_DIR="$destination/state" "$destination/bin/sotc-arm64" --gpu-load-check "$destination/bin/libbbgpu.dylib"
    exit $?
fi
exec /bin/bash "$destination/launch.sh"
exit 0
'''.encode()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game', type=Path, required=True)
    p.add_argument('--image', type=Path, default=ROOT / 'work/sotc-graphics-arm64/boot-linked.bin')
    p.add_argument('--host', type=Path, default=ROOT / 'work/sotc-arm64')
    p.add_argument('--library', type=Path, default=ROOT / 'work/gpu-build-73672843f513/libbbgpu.dylib')
    p.add_argument('--startup-profile', type=Path)
    p.add_argument('--output', type=Path, default=ROOT / 'work/portable/SotC-Mac.command')
    a = p.parse_args()
    game = a.game.resolve(strict=True)
    if not (game / 'eboot.bin').is_file():
        p.error('Expected the game directory containing eboot.bin.')
    if a.image.read_bytes()[:8] != b'BBPROBE5':
        p.error('Expected the privately prepared BBPROBE5 image.')
    output = a.output.resolve()
    if output.exists():
        p.error('Output already exists; choose a new filename.')
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.is_relative_to((ROOT / 'work').resolve()):
        p.error('Keep game-containing output under the ignored work directory.')
    with tempfile.TemporaryDirectory(prefix='portable-stage-', dir=ROOT / 'work') as tmp:
        stage = Path(tmp)
        libraries = relocate(a.host, a.library, stage / 'bin')
        shutil.copy2(a.image, stage / 'boot-linked.bin')
        shutil.copy2(ROOT / 'tools/runtime-settings.sh', stage / 'settings.sh')
        shutil.copy2(ROOT / 'LICENSE', stage / 'LICENSE')
        shutil.copy2(ROOT / 'docs/CREDITS.md', stage / 'CREDITS.md')
        intro = ''
        if a.startup_profile:
            from check_startup_profile import check
            checked = check(a.image, a.startup_profile)
            intro = f"export SOTC_SKIP_INTRO=1 SOTC_SKIP_INTRO_SITE={hex(checked['site'])}\n"
        (stage / 'launch.sh').write_text('''#!/bin/bash
set -euo pipefail
umask 077
cd -- "$(dirname -- "$0")"
package_root="$PWD"
mkdir -p "$HOME/Library/Logs/SotC Native" state
sotc_state=$(mktemp -d "$package_root/state/run-XXXXXX")
source ./settings.sh
''' + intro + '''export SOTC_DIAGNOSTIC_SECONDS="${SOTC_RUN_SECONDS:-180}"
log="$HOME/Library/Logs/SotC Native/game-$(date -u +%Y%m%dT%H%M%SZ).log"
echo "Log: $log"
echo "Run budget: $SOTC_DIAGNOSTIC_SECONDS seconds (0 = unlimited). Saves are not supported."
{ sw_vers; sysctl -n machdep.cpu.brand_string hw.memsize; cat package.json; } > "$log"
set +e
./bin/sotc-arm64 ./boot-linked.bin --bootstrap --game-root "$package_root/Game" --graphics-library "$package_root/bin/libbbgpu.dylib" 2>&1 | tee -a "$log"
status=${PIPESTATUS[0]}
echo "Exit: $status; log: $log"
exit "$status"
''')
        lock = json.loads((ROOT / 'tools/dependencies.json').read_text())
        (stage / 'package.json').write_text(json.dumps({
            'runtime': lock['runtime']['revision'], 'cpu': lock['cpu']['revision'],
            'minimum_macos': '15.0', 'architecture': 'arm64',
            'game_included': True, 'private_do_not_publish': True,
            'libraries': sorted(libraries),
            'host_sha256': hashlib.sha256((stage / 'bin/sotc-arm64').read_bytes()).hexdigest(),
            'image_sha256': hashlib.sha256(a.image.read_bytes()).hexdigest(),
        }, indent=2) + '\n')
        # Verify dependency resolution after copying to a different, spaced path.
        with tempfile.TemporaryDirectory(prefix='portable-load-check-', dir=ROOT / 'work') as check_state:
            print(call([stage / 'bin/sotc-arm64', '--gpu-load-check', stage / 'bin/libbbgpu.dylib'],
                       env=dict(os.environ, BB_GPU_USER_DIR=check_state)), flush=True)
        files = [(f, f.relative_to(stage).as_posix()) for f in sorted(stage.rglob('*')) if f.is_file()]
        for f in sorted(game.rglob('*')):
            if f.is_symlink():
                raise ValueError('Game symlinks are not portable: ' + str(f))
            if f.is_file():
                files.append((f, 'Game/' + f.relative_to(game).as_posix()))
        needed = sum(f.stat().st_size for f, _ in files)
        if shutil.disk_usage(output.parent).free < needed + 1024**3:
            raise ValueError('Insufficient workspace space to build the archive.')
        temporary = output.with_suffix('.partial')
        digest = hashlib.sha256()
        class Sink:
            def __init__(self, stream):
                self.stream = stream
            def write(self, data):
                digest.update(data)
                return self.stream.write(data)
        try:
            with temporary.open('xb') as stream:
                stream.write(b'#' * HEADER_SIZE)
                with tarfile.open(fileobj=Sink(stream), mode='w|gz', compresslevel=1) as archive:
                    for f, name in files:
                        if f.stat().st_size > 100 * 1024**2:
                            print(f'Packing {name}: {f.stat().st_size / 1024**3:.2f} GiB', flush=True)
                        archive.add(f, arcname=name, recursive=False)
                payload_size = stream.tell() - HEADER_SIZE
                head = launcher(digest.hexdigest(), payload_size, needed)
                if len(head) > HEADER_SIZE - 2:
                    raise ValueError('Launcher header too large')
                stream.seek(0)
                stream.write(head + b'#' + b' ' * (HEADER_SIZE - len(head) - 2) + b'\n')
                stream.flush()
                os.fsync(stream.fileno())
            temporary.chmod(0o700)
            temporary.rename(output)
        finally:
            if temporary.exists():
                temporary.unlink()
        print(json.dumps({'file': str(output), 'bytes': output.stat().st_size,
                          'extracted_bytes': needed, 'payload_sha256': digest.hexdigest()}, indent=2), flush=True)


if __name__ == '__main__':
    main()
