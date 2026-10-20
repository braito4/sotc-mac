#!/bin/bash
# Publish the reviewed source repository from a Terminal with GitHub access.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
for trial_tool in git gh python3; do
    command -v "$trial_tool" >/dev/null 2>&1 || { printf 'Missing tool: %s\n' "$trial_tool" >&2; exit 1; }
done
python3 "$trial_root/tools/check_repository.py"
git -C "$trial_root" diff --quiet
git -C "$trial_root" diff --cached --quiet
if ! trial_owner="$(gh api user --jq .login)"; then
    printf 'GitHub access failed. Use a Terminal with network access. If authentication is missing, run: gh auth login -h github.com\n' >&2
    exit 1
fi
trial_name="$trial_owner/sotc-mac"
if trial_remote="$(git -C "$trial_root" remote get-url origin 2>/dev/null)"; then
    case "$trial_remote" in
        "https://github.com/$trial_name"|"https://github.com/$trial_name.git"|"git@github.com:$trial_name.git") ;;
        *) printf 'Preserving unexpected origin: %s\n' "$trial_remote" >&2; exit 1 ;;
    esac
    # Retry a previous partial publication without replacing remote history.
    gh repo view "$trial_name" --json nameWithOwner >/dev/null
    git -C "$trial_root" push -u origin main
else
    if gh repo view "$trial_name" --json nameWithOwner >/dev/null 2>&1; then
        printf 'The repository %s already exists. Preserving it; review its contents before linking an origin.\n' "$trial_name" >&2
        exit 1
    fi
    gh repo create "$trial_name" --public --source "$trial_root" --remote origin --push \
        --description 'Experimental native ARM64 macOS port of Shadow of the Colossus PS4. CPU diagnostics; not playable yet. Built on bloodborne-native, bbport and shadPS4.'
fi
gh repo view "$trial_name" --json url --jq .url
