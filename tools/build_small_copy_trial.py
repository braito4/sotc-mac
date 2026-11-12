"""Isolated small-compute-transfer backport, including queued buffer diagnostics.

Requires the verified local native control build; does not change dependency pins
or the previous trial. SOTC_SMALL_COPIES=1 opts in at runtime, otherwise old blits.
"""
import json
import build_texture_alias_trial as build

build.OUT = build.ROOT / 'work/small-copy-trial-20261109'
build.PATCH = build.ROOT / 'tools/gpu_patches/sotc-small-copy-trial.patch.json'

if __name__ == '__main__':
    # Include the complete transitive local header manifest of the previous trial.
    prior = json.loads((build.ROOT/'work/binding-route-trial-20261109/trial-build.json').read_text())
    headers = dict(prior['diagnostic_headers'])
    for name, expected in headers.items():
        if build.digest(build.ROOT/'runtime/src'/name) != expected:
            raise RuntimeError('Previous diagnostic header changed: ' + name)
    for name in ('gpu_small_copy.h', 'gpu_small_copy_metal.h'):
        headers[name] = build.digest(build.ROOT/'runtime/src'/name)
    build.main()
    path = build.OUT/'trial-build.json'
    data = json.loads(path.read_text())
    if any(build.digest(build.ROOT/'runtime/src'/name) != expected for name, expected in headers.items()):
        raise RuntimeError('Header changed while compiling')
    data['diagnostic_headers'] = headers
    data['upstream_small_copy_commit'] = 'a03929b5d0d09c070dea48e37c600e6cbf46ae5a'
    data['runtime_switch'] = 'SOTC_SMALL_COPIES=1; default off'
    path.write_text(json.dumps(data, indent=2)+'\n')
