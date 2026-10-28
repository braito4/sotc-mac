#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
python3 tools/build_gpu.py --fetch
bash tools/build_host.sh
python3 tools/build_gpu.py
python3 tools/check_gpu_link.py --executable work/sotc-arm64 --library work/gpu-build-73672843f513/libbbgpu.dylib
