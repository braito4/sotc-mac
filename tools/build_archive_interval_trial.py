"""Isolated background archive scheduling trial on the unchanged control renderer."""
import build_texture_alias_trial as build
build.OUT = build.ROOT / 'work/archive-interval-trial-20261105'
build.PATCH = build.ROOT / 'tools/gpu_patches/sotc-archive-interval-trial.patch.json'
if __name__ == '__main__': build.main()
