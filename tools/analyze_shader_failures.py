"""Offline private log grouping. Adjacency is evidence, not causal attribution."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

PIPELINE = re.compile(r'Failed to create graphics pipeline fs_(0x[0-9a-f]+),vs_(0x[0-9a-f]+): ([^;]+)')
CONVERSION = 'SPIR-V to MSL conversion error: '
SRT = 'Failed to compute offset for SRT walker'
PROVENANCE = re.compile(r'GPU SRT diagnostic: shader=([0-9a-f]+) stage=(-?\d+) site=(\S+) expression=(.*); bounded IR shape,')


def analyze(lines):
    pairs, reasons, conversion, srt_sites, srt_shapes = Counter(), Counter(), Counter(), Counter(), Counter()
    pending = None
    failures = srt = 0
    for number, line in enumerate(lines, 1):
        line = line.rstrip('\r\n')
        if CONVERSION in line:
            reason = line.split(CONVERSION, 1)[1]
            conversion[reason] += 1
            pending = (number, reason)
        elif match := PIPELINE.search(line):
            fs, vs, result = match.groups()
            failures += 1
            reason = pending[1] if pending and number - pending[0] <= 2 else 'unattributed'
            pairs[(fs, vs, result, reason)] += 1
            reasons[reason] += 1
            pending = None
        elif pending and line.strip() and not line.startswith('[mvk-error] VK_ERROR_'):
            pending = None
        if SRT in line:
            srt += 1
            srt_sites[line.split(SRT, 1)[0].strip()] += 1
        if match := PROVENANCE.search(line):
            srt_shapes[match.groups()] += 1
    fragment_evidence = {}
    for fs, _, _, reason in pairs:
        if reason != 'unattributed':
            fragment_evidence.setdefault(fs, set()).add(reason)
    return {
        'schema': 1,
        'interpretation': 'Counts are log events. Conversion association requires immediate adjacent driver lines; it is not a proof of causality. Pipeline pairs are not complete pipeline keys. SRT shapes omit values and are bounded samples, not equivalent expressions.',
        'pipeline_failure_events': failures,
        'unique_failed_shader_pairs': len({(fs, vs) for fs, vs, _, _ in pairs}),
        'conversion_messages': dict(conversion),
        'pipeline_adjacent_conversion': dict(reasons),
        'unattributed_events_with_fragment_evidence_elsewhere': sum(
            count for (fs, _, _, reason), count in pairs.items()
            if reason == 'unattributed' and fs in fragment_evidence),
        'pipelines': [dict(fragment=fs, vertex=vs, result=result, adjacent_conversion=reason,
                           fragment_conversion_evidence_elsewhere=sorted(fragment_evidence.get(fs, ())), events=count)
                      for (fs, vs, result, reason), count in pairs.most_common()],
        'srt_failure_events': srt,
        'srt_log_sites': dict(srt_sites),
        'srt_provenance': [dict(shader=shader, stage=int(stage), site=site, expression=shape, events=count)
                           for (shader, stage, site, shape), count in srt_shapes.most_common()],
        'srt_provenance_limit': 'At most 8 samples per pass and 128 per process; existing logs cannot recover missing IR.',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    target = args.output.resolve()
    if not target.is_relative_to((root / 'work').resolve()):
        parser.error('Reports contain private shader identifiers: output must be inside work/.')
    if args.log.stat().st_size > 128 << 20:
        parser.error('Log exceeds 128 MiB limit.')
    with args.log.open(errors='replace') as source:
        result = analyze(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ('pipeline_failure_events', 'unique_failed_shader_pairs',
                      'pipeline_adjacent_conversion', 'srt_failure_events')}))


if __name__ == '__main__':
    main()
