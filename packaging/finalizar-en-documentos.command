#!/bin/bash
# Finish the prepared local consolidation from a normal macOS Terminal.
# Never overwrites another project and does not launch the game.
set -euo pipefail
project_staging="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
project_target="${1:-$HOME/Documents/sotc-mac}"
[[ -d "$(dirname -- "$project_target")" ]] || {
    printf 'The destination parent directory does not exist.\n' >&2
    exit 1
}
if [[ "$project_staging" != "$project_target" ]]; then
    [[ ! -e "$project_target" ]] || {
        printf 'Destination already exists; nothing was moved.\n' >&2
        exit 1
    }
    mv -- "$project_staging" "$project_target"
fi
cd -- "$project_target"
python3 tools/relocate_workspace.py --from "$project_staging"
bash packaging/build-gpu-arm64.sh
bash packaging/test-arm64.sh --contracts-test
python3 - <<'PY'
import json
from pathlib import Path
root=Path.cwd();path=root/'work/consolidation/state.json'
state=json.loads(path.read_text())
state.update(status='ready-in-documents',active_root=str(root),
             renderer_rebuilt=True,cpu_contracts_passed=True,game_gpu_test_run=False)
path.write_text(json.dumps(state,indent=2)+'\n')
print('Consolidation ready:',root)
print('Current maintenance fix integrated; ARM64 build and CPU checks passed. No game was launched.')
PY
