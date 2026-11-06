"""Build isolated vegetation parameter controls against the verified renderer."""
import build_texture_alias_trial as build
build.OUT = build.ROOT / 'work/vegetation-cache-trial-20261105'
build.PATCH = build.ROOT / 'tools/gpu_patches/sotc-vegetation-cache-trial.patch.json'
if __name__ == '__main__':
    import json
    build.main()
    manifest=build.OUT / 'trial-build.json'
    data=json.loads(manifest.read_text())
    data['adapter_header_sha256']=build.digest(build.ROOT / 'runtime/src/vegetation_parameter_scale.h')
    manifest.write_text(json.dumps(data,indent=2)+'\n')
