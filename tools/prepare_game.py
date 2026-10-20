"""Validate the supported edition before invoking the pinned SotC preparation tool."""
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'reference/scripts'))
import prepare

game = Path(os.environ.get('SOTC_GAME_ROOT', ROOT / 'game/CUSA08809/SotC-PC/Game/CUSA08809')).expanduser().resolve()
try:
    meta = prepare.sfo((game / 'sce_sys/param.sfo').read_bytes())
    if (meta.get('TITLE_ID'), meta.get('APP_VER')) != ('CUSA08809', '01.01'):
        raise ValueError('this diagnostic supports only CUSA08809 update 1.01')
    prepare.prepare(game, ROOT / 'work/sotc-original')
except (OSError, ValueError, StopIteration, KeyError, IndexError) as error:
    raise SystemExit(f'Game preparation failed: {error}')
