"""Bounded exact selection of two producer variants on the region renderer.

Preserves the prior native binary and fingerprints the capture header change.
The existing 32 snapshot budget is divided equally between the two variants.
"""
import build_pool_regions_trial as regions

regions.build.OUT = regions.build.ROOT / 'work/dual-producer-trial-20261112'
regions.build.PATCH = regions.build.ROOT / 'tools/gpu_patches/sotc-dual-producer-trial.patch.json'
regions.PRIOR = regions.build.ROOT / 'work/fragment-regions-trial-20261112/trial-build.json'

if __name__ == '__main__':
    regions.main()
