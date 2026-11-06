"""Build bounded texture descriptor diagnostics against the verified renderer."""
import build_texture_alias_trial as build
build.OUT = build.ROOT / 'work/shader-fallback-trial-20261106'
build.PATCH = build.ROOT / 'tools/gpu_patches/sotc-shader-fallback-trial.patch.json'
if __name__ == '__main__':
    import json
    build.main()
    manifest=build.OUT / 'trial-build.json'
    data=json.loads(manifest.read_text())
    data['fallback_header_sha256']=build.digest(build.ROOT / 'runtime/src/gpu_fallback_mapping.h')
    data['descriptor_header_sha256']=build.digest(build.ROOT / 'runtime/src/texture_descriptor_diagnostic.h')
    data['adapter_header_sha256']=build.digest(build.ROOT / 'runtime/src/vegetation_parameter_scale.h')
    manifest.write_text(json.dumps(data,indent=2)+'\n')
