"""Reactivate a local Codex reviewer between bounded native diagnostic runs."""
import argparse
import fcntl
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CODEX = Path('/Applications/ChatGPT.app/Contents/Resources/codex-cli/bin/codex')


def review_effort(batch):
    results = sorted(batch.glob('round-*-result.json'), key=lambda p: int(p.name.split('-')[1]))
    latest = json.loads(results[-1].read_text()) if results else {}
    # A scoped recommendation applies only to its specified completed round.
    recommendation = batch / 'next-review-effort.json'
    if recommendation.exists():
        choice = json.loads(recommendation.read_text())
        if choice.get('after_round') == latest.get('round'):
            if choice.get('effort') not in ('low', 'medium', 'high') or not str(choice.get('reason', '')).strip():
                raise ValueError('Invalid reasoning-effort recommendation')
            return choice['effort']
    return 'low' if latest.get('returncode') == 139 else 'medium'


def review_state(batch, now=None):
    """Never start a reviewer while a game is active or its exit is uncertain."""
    now = time.time() if now is None else now
    if (batch / 'STOP').exists():
        return 'stop'
    results = sorted(batch.glob('round-*-result.json'), key=lambda p: int(p.name.split('-')[1]))
    if results and not json.loads(results[-1].read_text()).get('safe_to_continue'):
        return 'unsafe'
    state = json.loads((batch / 'controller-state.json').read_text())
    if state.get('status') == 'running':
        return 'wait' if controller_locked(batch) else 'controller_unavailable'
    if state.get('status') == 'stopped' or now - state.get('updated_unix', 0) > 45:
        return 'controller_unavailable'
    requests = batch / 'requests'
    if list(requests.glob('*.running')) or list(requests.glob('*.json')):
        return 'wait'
    if state.get('status') != 'waiting_for_reviewed_request':
        return 'unsafe'
    return 'review'


def controller_locked(batch):
    with (batch / 'controller.lock').open('a+') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return True
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--batch', type=Path, default=ROOT / 'work/analysis/adaptive-tests-20261024')
    parser.add_argument('--codex', type=Path, default=DEFAULT_CODEX)
    parser.add_argument('--model', default='gpt-6-luna', help='Model for local reviewer activations')
    args = parser.parse_args()
    batch = args.batch.resolve()
    if not batch.is_relative_to((ROOT / 'work/analysis').resolve()):
        parser.error('batch must be under work/analysis')
    prompt_path = batch / 'persistent-agent-prompt.txt'
    if not args.codex.is_file() or not prompt_path.is_file():
        parser.error('missing Codex executable or reviewed agent prompt')
    with (batch / 'persistent-agent.lock').open('a+') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise SystemExit('A persistent reviewer already owns this batch.')
        # The owner starts this wrapper from native Terminal, where Metal works.
        # Reuse an existing controller; never replace its lock or state.
        if not (batch / 'STOP').exists() and not controller_locked(batch):
            state_path = batch / 'controller-state.json'
            if state_path.exists():
                prior = json.loads(state_path.read_text())
                if prior.get('status') == 'running' or list((batch / 'requests').glob('*.running')):
                    raise SystemExit('Uncertain previous run: review its child exit before restarting.')
            with (batch / 'persistent-controller.log').open('ab') as output:
                subprocess.Popen([sys.executable, str(ROOT / 'tools/run_adaptive_diagnostics.py'),
                                  '--batch', str(batch), '--idle-seconds', '1800'],
                                 cwd=ROOT, stdout=output, stderr=subprocess.STDOUT)
            time.sleep(2)
        cycle = 0
        while True:
            cycle += 1
            while True:
                decision = review_state(batch)
                if decision != 'wait':
                    break
                time.sleep(3)
            if decision != 'review':
                print('Persistent reviewer stopped: ' + decision, flush=True)
                return 0
            print('Starting local Codex review; Ctrl-C stops this wrapper.', flush=True)
            prior_results = set(batch.glob('round-*-result.json'))
            effort = review_effort(batch)
            print(f'Reviewer model={args.model}, reasoning={effort}', flush=True)
            # Automatic approval review retains the workspace sandbox. No bypass,
            # permission relaxation, hook overrides, or arbitrary shell eval.
            with prompt_path.open() as prompt:
                result = subprocess.run([str(args.codex), 'exec', '--approve-for-me',
                    '--model', args.model, '-c', f'model_reasoning_effort="{effort}"',
                    '-C', str(ROOT), '-o', str(batch / 'persistent-last-review.md'), '-'],
                    cwd=ROOT, stdin=prompt, check=False)
            progress = dict(updated_unix=time.time(), cycle=cycle, agent_returncode=result.returncode,
                            model=args.model, reasoning_effort=effort)
            (batch / 'persistent-agent-state.json').write_text(json.dumps(progress) + '\n')
            if result.returncode:
                raise SystemExit('Codex failed; no automatic retry. Review the Terminal output.')
            if (batch / 'AGENT_STOP').exists():
                print('Reviewer requested a stop; see persistent-last-review.md.', flush=True)
                return 0
            if review_state(batch) == 'review' and set(batch.glob('round-*-result.json')) == prior_results:
                raise SystemExit('Reviewer queued no next test; stopped rather than repeat blindly.')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit('Reviewer interrupted. Native controller owns game cancellation; use its Terminal to stop a running game.')
