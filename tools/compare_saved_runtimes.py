"""Sequential Sunday/current renderer comparison using the same existing host and game image."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]

def main():
    folder = ROOT / 'work' / ('runtime-comparison-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    folder.mkdir()
    image = ROOT / 'work/sotc-graphics-arm64/boot-linked.bin'
    host = ROOT / 'work/probe_entry_arm64'
    game = ROOT / 'game/CUSA08809/SotC-PC/Game/CUSA08809'
    profile = ROOT / 'work/startup-no-intro-profile.json'
    site = subprocess.check_output([sys.executable, str(ROOT / 'tools/check_startup_profile.py'),
        '--image', str(image), '--profile', str(profile), '--print-site'], text=True).strip()
    env = {k: v for k, v in os.environ.items() if not k.startswith(('BB_', 'SOTC_', 'VK_', 'SDL_'))}
    env.update({k: '0' for k in ('SOTC_DIAGNOSTIC_JOURNAL', 'SOTC_GPU_SUBMIT_TRACE',
        'SOTC_GPU_ENCODER_PROGRESS', 'SOTC_SHADER_PROVENANCE', 'SOTC_SHADER_SUBMIT_TRACE',
        'SOTC_SHADER_BIND_TRACE', 'SOTC_SHADER_COPY_TRACE', 'SOTC_MEMORY_TRACE',
        'SOTC_VISUAL_TRACE', 'SOTC_VISUAL_CAPTURE', 'SOTC_AUDIO_TRACE', 'SOTC_TASK_GUARD',
        'SOTC_INDIRECT_PREP_TRACE', 'SOTC_INDIRECT_COPY_TRACE', 'SOTC_COMPUTE_BIND_TRACE',
        'BB_ASYNC_SUBMIT', 'BB_SUBMIT_AHEAD', 'BB_DIRECT_MEMORY_ACCESS', 'BB_FULLSCREEN',
        'BB_FRAME_STATS', 'BB_WRITE_LOG', 'BB_SETTINGS_CHANNEL', 'BB_VK_VALIDATION')})
    env.update(BB_GFX_BACKEND='metal', BB_METAL_FRAME='1', BB_POOL_SPLIT='1',
        BB_RESIDENCY_SET='1', BB_JIT='1', BB_UPSCALER='none', BB_UPSCALE_PRESET='0',
        BB_DMEM_MB='5056', BB_DMEM_EXTRA_MB='512', BB_GAME_PROFILE='none',
        BB_GPU_LOG='critical', SOTC_METAL_INFLIGHT='1', SOTC_GPU_WATCHDOG_SECONDS='120',
        SOTC_SKIP_INTRO='1', SOTC_SKIP_INTRO_SITE=site, SOTC_CPU_EXPERIMENT='base',
        SOTC_CPU_POINTER_TRACE='', SOTC_CPU_FIELD_TRACE='', SOTC_CPU_FAULT_FIELDS='',
        SOTC_REQUIRE_POINTER_TRACE='0', SOTC_TEST_REDUCTION_BARRIERS='1')
    toggles = folder / 'toggles.txt'
    toggles.write_text('70437463670784 0\n')
    toggles.chmod(0o400)
    env['BB_TOGGLE_FILE'] = str(toggles)
    results = []
    for label, key, native in [('sunday', '22923f080229', '0'), ('current', 'a27f0ca477bd', '1')]:
        library = ROOT / f'work/gpu-build-{key}/libbbgpu.dylib'
        state = folder / label
        state.mkdir()
        run_env = dict(env, BB_GPU_USER_DIR=str(state), BB_CONFIG=str(state / 'bbport.ini'),
                       BB_METAL_NATIVE_BOOT=native)
        if native == '0':
            prefix = ROOT / f'work/gpu-source-{key}/deps/macos-arm64'
            icd = str(prefix / 'share/vulkan/icd.d/MoltenVK_icd.json')
            run_env.update(VK_DRIVER_FILES=icd, VK_ICD_FILENAMES=icd,
                           SDL_VULKAN_LIBRARY=str(prefix / 'lib/libvulkan.1.dylib'))
        log = folder / (label + '.log')
        record = dict(label=label, library=str(library), sha256=hashlib.sha256(library.read_bytes()).hexdigest(),
                      log=str(log), settings={k:v for k,v in run_env.items() if k.startswith(('BB_', 'SOTC_', 'VK_', 'SDL_'))})
        (state / 'configuration.json').write_text(json.dumps(record, indent=2)+'\n')
        print(f'Running {label}: up to 500s; log {log}', flush=True)
        status = subprocess.call([sys.executable, str(ROOT / 'tools/run_gpu_diagnostic.py'),
            '--seconds', '500', '--log', str(log), '--cooperative-stop', '--', str(host),
            str(image), '--bootstrap', '--game-root', str(game), '--graphics-library', str(library)], env=run_env)
        record['exit'] = status
        results.append(record)
        (folder / 'results.json').write_text(json.dumps(results, indent=2)+'\n')
        print(f'{label}: exit={status}', flush=True)
        if status in (130, -2):
            break
    print(f'Comparison saved: {folder}', flush=True)

if __name__ == '__main__':
    main()
