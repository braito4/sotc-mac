"""Isolated append-output propagation; preserves the initializer-only control."""
import build_pool_regions_trial as regions

regions.build.OUT = regions.build.ROOT / 'work/producer-writeback-trial-20261112'
regions.build.PATCH = regions.build.ROOT / 'tools/gpu_patches/sotc-producer-writeback-trial.patch.json'
regions.PRIOR = regions.build.ROOT / 'work/fragment-writeback-trial-20261112/trial-build.json'

if __name__ == '__main__':
    regions.main()
