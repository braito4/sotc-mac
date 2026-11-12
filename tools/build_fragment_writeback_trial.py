"""Isolated completed-output propagation; preserves the producer-input control."""
import build_pool_regions_trial as regions

regions.build.OUT = regions.build.ROOT / 'work/fragment-writeback-trial-20261112'
regions.build.PATCH = regions.build.ROOT / 'tools/gpu_patches/sotc-fragment-writeback-trial.patch.json'
regions.PRIOR = regions.build.ROOT / 'work/producer-input-trial-20261112/trial-build.json'

if __name__ == '__main__':
    regions.main()
