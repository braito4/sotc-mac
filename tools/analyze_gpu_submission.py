"""Decode private CPU submission observations without inferring GPU instruction progress."""
import re


PREFIX = 'GPU submission trace: '


def values(text):
    result = {}
    for key, raw in re.findall(r'(\w+)=(-?(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?))(?=[\s;]|$)', text):
        result[key] = float(raw) if '.' in raw else int(raw, 16 if raw.startswith('0x') else 10)
    return result


def analyze(text):
    reports = []
    report = batch = buffer = None
    for line in text.splitlines():
        if not line.startswith(PREFIX):
            continue
        line = line[len(PREFIX):]
        if line.startswith('CPU-recorded candidates,'):
            report = dict(header=values(line), submissions=[], evidence_gaps=[])
            reports.append(report)
            batch = buffer = None
            continue
        # A partial log without its header cannot safely associate batches.
        if report is None:
            continue
        if line.startswith(('AWAITED ', 'LAST_COMPLETED ')):
            batch = dict(role=line.split()[0], **values(line), waits=[], signals=[], buffers=[])
            report['submissions'].append(batch)
            buffer = None
        elif line.startswith('awaited submission not retained'):
            report['evidence_gaps'].append('awaited submission not retained or submit not reached')
        elif batch is not None:
            if line.startswith('submit_timing '):
                batch['timing'] = values(line)
            elif line.startswith(('wait ', 'signal ')):
                batch['waits' if line.startswith('wait ') else 'signals'].append(values(line))
            elif match := re.match(r'buffer\[(\d+)\]=(0x[0-9a-fA-F]+) recording=(\d+)', line):
                buffer = dict(index=int(match[1]), handle=int(match[2], 16),
                              recording=int(match[3]), chunks=[])
                batch['buffers'].append(buffer)
            elif line.startswith('Metal chunk=') and buffer is not None:
                buffer['chunks'].append(dict(callback=values(line)))
            elif line.startswith(('Metal timing ', 'Metal native ')) and buffer is not None:
                data = values(line)
                for chunk in reversed(buffer['chunks']):
                    if chunk['callback'].get('chunk') == data.get('chunk'):
                        chunk['native' if line.startswith('Metal native ') else 'timing'] = data
                        break
            elif line.startswith('note_summary '):
                batch['note_summary'] = values(line)
            elif line.startswith('guest_candidates '):
                batch['guest_candidates'] = values(line)

    for report in reports:
        gaps = report['evidence_gaps']
        for key in ('missing_notes', 'missing_metal_events'):
            if report['header'].get(key, 0):
                gaps.append(key + ': observations missing')
        awaited = [b for b in report['submissions'] if b['role'] == 'AWAITED']
        if not awaited:
            gaps.append('awaited batch details absent')
        for batch in report['submissions']:
            chunks = [c for b in batch['buffers'] for c in b['chunks']]
            batch['observed_chunks'] = len(chunks)
            batch['observed_scheduled_without_completion'] = sum(
                c['callback'].get('scheduled_callback') == 1 and
                c['callback'].get('completed_callback') == 0 for c in chunks)
            # Missing/unavailable native samples are unknown, never zero pressure.
            samples = [c['native'] for c in chunks if c.get('native', {}).get('memory_available') == 1]
            batch['native_allocated_bytes_range'] = (
                [min(s['allocated_bytes'] for s in samples), max(s['allocated_bytes'] for s in samples)]
                if samples and all('allocated_bytes' in s for s in samples) else None)
            if batch.get('missing_buffers', 0) or len(batch['buffers']) < batch.get('command_buffers', 0):
                gaps.append(batch['role'] + ': command buffer details missing')
            if any(not b['chunks'] for b in batch['buffers']):
                gaps.append(batch['role'] + ': native chunk callbacks absent for a buffer')
            candidates = batch.get('guest_candidates', {})
            if candidates.get('omitted', 0):
                gaps.append(batch['role'] + ': guest candidate tail omits earlier work')
    return dict(reports=reports, limitation=(
        'CPU recording, commit and callback observations only; scheduled does not identify '
        'an executing command. Callback status and native point-in-time status are separate. '
        'No exact shader, dependency deadlock, driver cause or resource lifetime fault established.'))
