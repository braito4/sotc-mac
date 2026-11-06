"""Build isolated completion-gated snapshot reuse, keeping strict guard behavior."""
import json
import build_texture_alias_trial as build
build.OUT=build.ROOT/'work/scatter-cached-trial-20261106'
build.PATCH=build.ROOT/'tools/gpu_patches/sotc-scatter-cached-trial.patch.json'
if __name__=='__main__':
    build.main()
    path=build.OUT/'trial-build.json';data=json.loads(path.read_text())
    data['diagnostic_headers']={name:build.digest(build.ROOT/'runtime/src'/name) for name in
        ['gpu_scatter_probe_policy.h','gpu_scatter_guard_metal.h','gpu_scatter_cached_guard.h',
         'staging_size_classes.h','gpu_fallback_mapping_window.h','gpu_fallback_mapping.h',
         'texture_descriptor_diagnostic.h','vegetation_parameter_scale.h']}
    path.write_text(json.dumps(data,indent=2)+'\n')
