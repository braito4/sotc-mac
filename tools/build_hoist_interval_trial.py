"""Isolated PR11 interval-set backport; does not enable exact read tracking."""
import build_texture_alias_trial as build
build.OUT = build.ROOT / 'work/hoist-interval-trial-20261105'
build.PATCH = build.ROOT / 'tools/gpu_patches/sotc-hoist-interval-trial.patch.json'
if __name__ == '__main__': build.main()
