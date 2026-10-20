"""Check SDL's required Vulkan surface extensions without creating an instance/device.

The pinned Metal renderer still initializes its Vulkan-facing layer. Select the
bundled MoltenVK ICD and Vulkan loader before using this check (see test-arm64.sh).
"""
import argparse
import ctypes
from pathlib import Path
import sys

class Extension(ctypes.Structure):
    _fields_ = [('name', ctypes.c_char * 256), ('version', ctypes.c_uint32)]


def instance_extensions(library):
    loader = ctypes.CDLL(str(library))
    query = loader.vkEnumerateInstanceExtensionProperties
    query.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(Extension)]
    query.restype = ctypes.c_int32
    # The extension list can grow between count and data queries (VK_INCOMPLETE).
    for _ in range(4):
        count = ctypes.c_uint32()
        result = query(None, ctypes.byref(count), None)
        if result != 0:
            raise ValueError('Vulkan extension count failed: VkResult=%d' % result)
        if count.value > 4096:
            raise ValueError('Invalid Vulkan extension count.')
        if not count.value:
            return set()
        extensions = (Extension * count.value)()
        capacity = count.value
        result = query(None, ctypes.byref(count), extensions)
        if result == 5:  # VK_INCOMPLETE: restart with the new count.
            continue
        if result != 0 or count.value > capacity:
            raise ValueError('Vulkan extension enumeration failed: VkResult=%d' % result)
        return {extensions[i].name.decode('ascii') for i in range(count.value)}
    raise ValueError('Vulkan extension enumeration remained incomplete.')


def require_surface_extensions(extensions):
    if 'VK_KHR_surface' not in extensions:
        raise ValueError('VK_KHR_surface is unavailable; verify the bundled MoltenVK ICD selection.')
    if not {'VK_EXT_metal_surface', 'VK_MVK_macos_surface'} & extensions:
        raise ValueError('No Vulkan Metal/macOS surface extension is available.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, required=True)
    args = parser.parse_args()
    if not args.library.is_file():
        raise ValueError('Missing Vulkan loader: ' + str(args.library))
    extensions = instance_extensions(args.library.resolve())
    require_surface_extensions(extensions)
    print('PASS: Vulkan loader exposes VK_KHR_surface and a Metal/macOS surface extension '
          '(%d instance extensions; no instance, device or window created).' % len(extensions))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, AttributeError) as error:
        print('STOP_VULKAN_SURFACE: ' + str(error), file=sys.stderr)
        raise SystemExit(28)
