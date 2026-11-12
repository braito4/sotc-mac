"""Region-aware ordered fragment reads and producer-triggered route diagnostics.

Keeps the previously built region renderer untouched. Headers changed against that
manifest are explicitly fingerprinted in the cumulative patch, not silently accepted.
"""
import build_pool_regions_trial as regions

regions.build.OUT = regions.build.ROOT / 'work/fragment-regions-trial-20261112'
regions.build.PATCH = regions.build.ROOT / 'tools/gpu_patches/sotc-fragment-regions-trial.patch.json'
regions.PRIOR = regions.build.ROOT / 'work/pool-regions-trial-20261112/trial-build.json'

if __name__ == '__main__':
    regions.main()
