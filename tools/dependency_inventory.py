"""Render/check the reviewed dependency lock; never download or update a dependency."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT/'tools/dependencies.json'
DOCUMENT = ROOT/'docs/DEPENDENCIES.md'


def link(repository, version, revision=None):
    return '['+version+']('+repository+('/commit/'+revision if revision else '/tree/'+version)+')'


def render(data):
    lines=['# Current dependency versions','',
           'Checked against public upstream sources on **'+data['checked_at']+'**. '
           'This page is generated from [the dependency lock](../tools/dependencies.json).',
           '', 'The lock selects qualified runtime/CPU/reference commits and library releases or tags; newer reviewed heads are listed separately. '
           'An available version is not proof of correct Apple M1 rendering. '
           'Native game validation after this dependency update remains pending.', '',
           'Integration: **'+data['validation']['integration']+'**.', '',
           '## Runtime and reference sources','',
           'The five owner-requested sources were refreshed on **'+data.get('sources_checked_at',data['checked_at'])+
           '**. Selected commits and reviewed upstream heads are separate; '+
           'the library release review above retains its own date. Additional source references appear below.', '',
           '| Component | Previous pin | Selected head | Latest reviewed head | Scope |',
           '|---|---|---|---|---|']
    for name in ('runtime','cpu','reference'):
        item=data[name]
        lines.append('| '+name+' | '+link(item['repository'],item['previous_revision'][:12],item['previous_revision'])+
                     ' | '+link(item['repository'],item['revision'][:12],item['revision'])+' | '+
                     link(item['repository'],item.get('latest_reviewed_revision',item['revision'])[:12],
                          item.get('latest_reviewed_revision',item['revision']))+' | '+
                     item.get('scope','Integrated source; exact commit is pinned.')+' '+item.get('review_scope','')+' |')
    lines += ['', 'The SotC reference update includes preparation/reference sources. Its Windows renderer '
              'does not replace the macOS renderer. The nested-command coroutine correction is separately '
              'adapted into our renderer, alongside the existing ARM64/SRT, staging and scene-proxy fixes.', '',
              '## Linked and recipe libraries','',
              '| Dependency | Previous recipe | Selected version | Latest available in the reviewed channel |',
              '|---|---|---|---|']
    for item in data['packages']:
        selected=link(item['repository'],item['version'],item['selected_revision'])
        lines.append('| '+item['id']+' | '+str(item['recipe_version'] or 'vendored snapshot')+' | '+selected+
                     ' | ['+item['latest_available']+']('+item['latest_url']+') |')
    for item in data['packages']:
        if item.get('note'):
            lines += ['', '- **'+item['id']+':** '+item['note']]
    lines += ['', '## Renderer submodules, transitive and host build dependencies','',
              '| Dependency | Selected | Latest reviewed | Scope |', '|---|---|---|---|']
    for category in ('submodules','transitive','host_tools','development_tools'):
        for item in data.get(category,[]):
            lines.append('| '+item['id']+' | '+link(item['repository'],item['version'],item['revision'])+
                         ' | '+item['latest_available']+' | '+item.get('scope',
                         'Checked renderer override.' if category=='submodules' else 'Host shader compiler, built as ARM64.')+' |')
    lines += ['', 'The renderer uses installed current SPIRV-Headers instead of the older embedded copy. '
              'Some upstream package metadata keeps an older version string; exact source tags/commits above '
              'identify the selected source, rather than inferring it from pkg-config alone.', '',
              '## Embedded and inactive components','']
    for item in data.get('vendored',[]):
        lines += ['- **'+item['id']+' ('+item['version']+'):** '+item['scope']+
                  (' [Verified upstream header]('+item['latest_url']+').' if item.get('latest_url') else '')]
    for item in data.get('excluded',[]):
        lines += ['- **'+item['id']+':** '+item['scope']]
    lines += ['', 'macOS frameworks (Metal, MetalFX, CoreAudio and platform libraries), Clang/SDK, '
              'Git, CMake, Ninja, pkg-config and Python are host prerequisites. This update does not '
              'install or change system software. Original game/firmware modules remain private user input.', '',
              '## Additional upstream references','',
              '| Project | Default branch | Latest reviewed head | Integration scope |', '|---|---|---|---|']
    for item in data.get('references',[]):
        lines.append('| '+item['id']+' | '+item['branch']+' | '+link(item['repository'],item['revision'][:12],item['revision'])+
                     ' | '+item['scope']+' |')
    lines += ['', '## Open proposals','',
              '| Proposal | State | Exact reviewed head |', '|---|---|---|']
    for item in data.get('proposals',[]):
        lines.append('| ['+item['name']+']('+item['url']+') | '+item['state']+' | `'+item['head']+'` |')
    lines += ['', 'Open proposals are tracked separately from integrated released dependencies. '
              'Author tests on other machines are not our M1 validation.', '',
              '## Reproduce and maintain','',
              'A separate versioned integration workspace preserves the preceding qualified build. '
              'The selected workspace key and exact required Git HEAD are recorded '
              'and checked independently against the lock. Local overlays and the Dear ImGui override are '
              'digest/revision checked. Dependency caches are separated by the version lock; installed '
              'markers contain the actual selected version, so an old empty marker cannot skip an update.', '',
              'Run `python3 tools/dependency_inventory.py --check` to check this document and lock. '
              'After reviewing new public sources, update the lock, adapt/build/test, then regenerate with '
              '`python3 tools/dependency_inventory.py`. This command does not query GitHub, '
              'change pins or claim an online refresh.', '',
              'See [upstream review](UPSTREAM_REVIEW.md), [build instructions](guide/building.md) '
              'and the [current handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint) for validation and the native test boundary.', '']
    return '\n'.join(lines)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    data=json.loads(LOCK.read_text())
    for item in data['packages']:
        if not re.fullmatch(r'[0-9a-f]{40}',item['selected_revision'] or ''):
            raise SystemExit('Missing exact source revision: '+item['id'])
    from build_gpu import REVISION, backport_manifest
    if data['runtime']['revision']!=REVISION or backport_manifest()['base_revision']!=REVISION:
        raise SystemExit('Renderer revision and dependency lock disagree.')
    content=render(data)
    if args.check:
        if not DOCUMENT.is_file() or DOCUMENT.read_text()!=content:
            raise SystemExit('Dependency documentation is stale; regenerate after source review.')
        print('PASS: dependency lock, exact source revisions, renderer manifest and generated inventory agree.')
    else:
        DOCUMENT.write_text(content)
        print('Updated docs/DEPENDENCIES.md from the reviewed dependency lock; no dependencies changed.')


if __name__=='__main__':
    main()
