"""Isolated pre-dispatch input containment on the dual-producer renderer."""
import build_pool_regions_trial as regions

regions.build.OUT = regions.build.ROOT / 'work/producer-input-trial-20261112'
regions.build.PATCH = regions.build.ROOT / 'tools/gpu_patches/sotc-producer-input-trial.patch.json'
regions.PRIOR = regions.build.ROOT / 'work/dual-producer-trial-20261112/trial-build.json'

if __name__ == '__main__':
    regions.main()
