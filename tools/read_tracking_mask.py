"""Select exact read tracking on the pinned 994846bb7bb5 renderer.

This revision rounds read accesses unless CoarseReadTracking is disabled. The
later upstream PR #11 reverses the default; do not carry this mask to a new base
without checking vk_runtime.cpp again. Write ranges/barriers are unchanged.
"""
import argparse

CONTROL_MASK = 70437463670784  # Retain the existing SotC staging/CPU corrections.
COARSE_READ_TRACKING = 2048


def mask(exact):
    if exact not in ('0', '1'):
        raise ValueError('SOTC_EXACT_READS must be 0 or 1')
    return CONTROL_MASK | (COARSE_READ_TRACKING if exact == '1' else 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exact', choices=('0', '1'))
    print(mask(parser.parse_args().exact))
