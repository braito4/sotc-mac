"""Group private field observations; never infer an exhaustive writer history."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
KEYS = ('seq', 'site', 'object', 'field', 'value', 'status', 'thread', 'ns')
EVENT = re.compile(r'^CPU field trace event:' + ''.join(
    rf' {key}=(0x[0-9a-f]+)' for key in KEYS) + r'$')
SUMMARY = re.compile(r'^CPU field trace summary: (.+)$')
DUMP = re.compile(r'^CPU field trace dump: (.+)$')


def analyze(text):
    events, summaries, dumps, malformed, duplicates = [], [], [], 0, 0
    started = False
    seen = set()
    for line in text.splitlines():
        if line.startswith('CPU field trace: bounded site observations;'):
            started = True
        if line.startswith('CPU field trace event:'):
            match = EVENT.fullmatch(line)
            if not match:
                malformed += 1
                continue
            row = dict(zip(KEYS, (int(x, 16) for x in match.groups())))
            if not row['seq'] or row['status'] > 7 or any(v >= 1 << 64 for v in row.values()):
                malformed += 1
                continue
            if row['seq'] in seen:
                duplicates += 1
                continue
            seen.add(row['seq']); events.append(row)
        elif match := SUMMARY.fullmatch(line):
            summaries.append({key: int(value, 16) for key, value in
                              re.findall(r'(\w+)=(0x[0-9a-f]+)', match[1])})
        elif match := DUMP.fullmatch(line):
            started = True
            dumps.append({key: int(value, 16) for key, value in
                          re.findall(r'(\w+)=(0x[0-9a-f]+)', match[1])})
    # Sequence numbers order observation reservations, not actual guest writes.
    events.sort(key=lambda row: row['seq'])
    grouped = defaultdict(list)
    for row in events:
        if row['status'] & 1:
            grouped[row['field']].append(row)
    selected = set(int(x, 16) for x in re.findall(
        r'CPU fault memory configured_field_candidate address=(0x[0-9a-f]+)', text))
    fields = []
    transitions_count = 0
    for address, rows in sorted(grouped.items()):
        readable = [row for row in rows if row['status'] & 2]
        transitions = []
        for before, after in zip(readable, readable[1:]):
            if before['value'] != after['value']:
                transitions.append(dict(before=before, after=after,
                    kind='nonzero_to_zero' if before['value'] and not after['value'] else
                         'zero_to_nonzero' if not before['value'] else 'changed_nonzero'))
        transitions_count += sum(t['kind'] == 'nonzero_to_zero' for t in transitions)
        fields.append(dict(address=address, selected_at_fault=address in selected,
            observations=len(rows), unavailable=len(rows)-len(readable),
            readable_samples=readable,
            unavailable_samples=[row for row in rows if not row['status'] & 2],
            observed_transitions=transitions))
    summary = summaries[-1] if summaries else None
    # No footer is not evidence of an empty ring. Cancellation/backpressure or
    # a process that never reached a dumping path can all leave no event lines.
    dump_state = ('complete' if summary and summary.get('retained') == len(events)
                  and not malformed and not duplicates else
                  'partial' if summary or started or events else 'not_dumped')
    return dict(schema=1, events=len(events), malformed_events=malformed,
        dump_state=dump_state, dump_metadata=dumps[-1] if dumps else None,
        duplicate_sequences=duplicates, fields=fields, summary=summary,
        invalid_address_samples=[row for row in events if not row['status'] & 1],
        selected_fields_without_history=sorted(selected-set(grouped)),
        observed_nonzero_to_zero=transitions_count,
        limitations='Configured site observations only, not exhaustive writes. '
        'Sequence reserves an observation, not a write-order proof; threads may race. '
        'Reused addresses are not unique allocations. Missing, overwritten, unreadable '
        'or uncommitted samples cannot prove never-initialized or destroyed objects. '
        'A changed sampled value does not attribute its writer, lifetime or GPU cause. '
        'No dumped events without a complete footer cannot prove an empty ring; '
        'a budget or output backpressure can omit observations.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    target = args.output.resolve()
    if not target.is_relative_to((ROOT/'work').resolve()):
        parser.error('Private field histories must stay under work/.')
    text = args.log.read_text(errors='replace')
    report = analyze(text)
    report['text_replacement_characters'] = text.count('\ufffd')
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({key: report[key] for key in
        ('events', 'dump_state', 'malformed_events', 'duplicate_sequences', 'observed_nonzero_to_zero')}))


if __name__ == '__main__':
    main()
