#!/bin/bash
# Compatibility entry point for the native ARM64 SotC diagnostic.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
exec bash "$trial_root/packaging/test-arm64.sh" "$@"
