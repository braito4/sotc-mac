"""Decode private CPU submission observations without inferring GPU instruction progress."""
import re


PREFIX = 'GPU submission trace: '


def values(text):
    result = {}
    for key, raw in re.findall(r'(\w+)=(-?(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?))(?=[\s;]|$)', text):
        result[key] = float(raw) if '.' in raw else int(raw, 16 if raw.startswith('0x') else 10)
    return result


def analyze(text):
    tiling = []
    snapshot = record = None
    for row in text.splitlines():
        if not row.startswith('GPU tiling trace: '):
            continue
        row = row[len('GPU tiling trace: '):]
        if row.startswith('total='):
            snapshot = dict(**values(row), records=[])
            tiling.append(snapshot)
            record = None
        elif row.startswith('set=') and snapshot is not None:
            record = dict(**values(row), set=row.split()[0].split('=', 1)[1], bindings=[], mips=[])
            snapshot['records'].append(record)
        elif record is not None:
            data = values(row)
            if data.get('seq') == record.get('seq'):
                if 'binding' in data:
                    record['bindings'].append(data)
                elif 'mip' in data:
                    record['mips'].append(data)
    reuse = []
    reuse_snapshot = None
    for row in text.splitlines():
        if not row.startswith('GPU parameter reuse: '):
            continue
        row = row[len('GPU parameter reuse: '):]
        if row.startswith('maps='):
            reuse_snapshot = dict(**values(row), records=[])
            reuse.append(reuse_snapshot)
        elif row.startswith('set=') and reuse_snapshot is not None:
            reuse_snapshot['records'].append(dict(**values(row), set=row.split()[0].split('=', 1)[1]))
    argument_fills=[]
    fill_snapshot=None
    for line in text.splitlines():
        if line.startswith('GPU argument fills: '):
            fill_snapshot=dict(**values(line),records=[])
            argument_fills.append(fill_snapshot)
        elif line.startswith('GPU argument fill: ') and fill_snapshot is not None:
            fill_snapshot['records'].append(values(line))
    argument_inputs=[values(line) for line in text.splitlines()
                     if line.startswith('GPU argument input: ')]
    argument_inputs_truncated=any(line.startswith('GPU argument inputs: ') and
                                 values(line).get('truncated')==1 for line in text.splitlines())
    writers=[]
    writer_snapshot=None
    for line in text.splitlines():
        if line.startswith('GPU indirect writers: '):
            writer_snapshot=dict(**values(line.split(': ',1)[1]),records=[])
            writers.append(writer_snapshot)
        elif line.startswith('GPU indirect writer: ') and writer_snapshot is not None:
            writer_snapshot['records'].append(values(line.split(': ',1)[1]))
    origins=[]
    origin_snapshot=None
    for line in text.splitlines():
        if line.startswith('GPU indirect origins: '):
            origin_snapshot=dict(**values(line.split(': ',1)[1]),records=[])
            origins.append(origin_snapshot)
        elif line.startswith('GPU indirect origin: ') and origin_snapshot is not None:
            origin_snapshot['records'].append(values(line.split(': ',1)[1]))
    reports = []
    report = batch = buffer = None
    render_scope = None
    for line in text.splitlines():
        if line.startswith('GPU compute bindings: ') and batch is not None:
            batch['compute_binding_coverage']=values(line.split(': ',1)[1])
            continue
        if line.startswith('GPU compute binding: ') and batch is not None:
            batch.setdefault('compute_bindings',[]).append(values(line.split(': ',1)[1]))
            continue
        if line.startswith('GPU render coverage: ') and batch is not None:
            batch['render_coverage']=values(line.split(': ',1)[1])
            continue
        if line.startswith('GPU render trace: ') and batch is not None:
            row=line[len('GPU render trace: '):]
            if row.startswith('buffer='):
                render_scope=dict(**values(row), attachments=[], pipeline_details=[], draw_details=[])
                batch.setdefault('render_scopes', []).append(render_scope)
            elif render_scope is not None:
                item=values(row)
                if row.startswith('attachment='):
                    render_scope['attachments'].append(item)
                elif row.startswith('pipeline='):
                    for key in ('vertex','fragment'):
                        match=re.search(r'\b'+key+r'=(\S+)',row)
                        if match: item[key]=match[1]
                    render_scope['pipeline_details'].append(item)
                elif row.startswith('draw='):
                    render_scope['draw_details'].append(item)
            continue
        if line.startswith('GPU native trace: ') and batch is not None:
            native = line[len('GPU native trace: '):]
            if native.startswith('retained='):
                batch['native_encoding'] = dict(**values(native), events=[])
            elif native.startswith('recording=') and 'native_encoding' in batch:
                event = values(native)
                for key in ('event', 'entry'):
                    match = re.search(r'\b' + key + r'=(\S+)', native)
                    if match:
                        event[key] = match[1]
                for key in ('groups', 'threads'):
                    match = re.search(r'\b' + key + r'=(\d+),(\d+),(\d+)', native)
                    if match:
                        event[key] = [int(x) for x in match.groups()]
                batch['native_encoding']['events'].append(event)
            continue
        if not line.startswith(PREFIX):
            continue
        line = line[len(PREFIX):]
        if line.startswith('CPU-recorded candidates,'):
            report = dict(header=values(line), submissions=[], evidence_gaps=[])
            reports.append(report)
            batch = buffer = None
            continue
        # A native failure carries its own command identity, without a watchdog header.
        if line.startswith('FAILED_METAL '):
            identity = values(line)
            report = dict(header={}, submissions=[], evidence_gaps=[], kind='native_failure')
            reports.append(report)
            batch = dict(role='FAILED_METAL', **identity, waits=[], signals=[], buffers=[])
            report['submissions'].append(batch)
            buffer = dict(handle=identity.get('buffer'), recording=identity.get('recording'), chunks=[])
            batch['buffers'].append(buffer)
            render_scope = None
            continue
        # A partial log without its header cannot safely associate batches.
        if report is None:
            continue
        if line.startswith(('AWAITED ', 'LAST_COMPLETED ')):
            batch = dict(role=line.split()[0], **values(line), waits=[], signals=[], buffers=[])
            report['submissions'].append(batch)
            buffer = None
            render_scope = None
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
            elif line.startswith(('Metal timing ', 'Metal native ', 'Metal progress ')) and buffer is not None:
                data = values(line)
                for chunk in reversed(buffer['chunks']):
                    if chunk['callback'].get('chunk') == data.get('chunk'):
                        chunk['native' if line.startswith('Metal native ') else 'progress' if line.startswith('Metal progress ') else 'timing'] = data
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
        if not awaited and report.get('kind') != 'native_failure':
            gaps.append('awaited batch details absent')
        for batch in report['submissions']:
            if batch.get('compute_binding_coverage',{}).get('omitted',0):
                gaps.append(batch['role']+': compute binding metadata omitted after cap')
            encoding = batch.get('native_encoding')
            if encoding and encoding.get('omitted', 0):
                gaps.append(batch['role'] + ': native encoder/dispatch tail omits earlier work')
            if encoding and encoding.get('retained') != len(encoding['events']):
                gaps.append(batch['role'] + ': native encoding report incomplete')
            chunks = [c for b in batch['buffers'] for c in b['chunks']]
            for scope in batch.get('render_scopes', []):
                events=batch.get('native_encoding',{}).get('events',[])
                preceding=[e for e in events if e.get('recording')==scope.get('recording')
                           and e.get('event') in ('dispatch_direct','dispatch_indirect')
                           and e.get('ordinal',0)<scope.get('begin_event',0)]
                scope['preceding_compute_binding_matches']=[]
                if preceding:
                    event=max(preceding,key=lambda e:e['ordinal'])
                    scope['preceding_dispatch_event']=event['ordinal']
                    for binding in batch.get('compute_bindings',[]):
                        if (binding.get('recording')!=scope.get('recording') or
                            binding.get('dispatch_event')!=event['ordinal'] or
                            binding.get('pipeline')!=event.get('pipeline') or binding.get('inline_bytes')):
                            continue
                        for draw in scope.get('draw_details',[]):
                            if (draw.get('kind') in (2,3) and draw.get('indirect') and
                                draw['indirect']==binding.get('resource') and
                                binding.get('offset',0)<=draw.get('offset',-1) and
                                draw.get('offset',0)+draw.get('argument_bytes',0)<=binding.get('length',0)):
                                scope['preceding_compute_binding_matches'].append(dict(
                                    draw=draw['draw'],slot=binding['slot'],dispatch_event=event['ordinal'],
                                    relation='same_buffer_argument_within_bound_tail_not_proven_write'))
            for scope in batch.get('render_scopes', []):
                scope['completion'] = 'unknown'
                if batch['role'] == 'FAILED_METAL':
                    # Error/recovery event values cannot certify successful rendering.
                    scope['completion'] = 'failed_command_progress_unreliable'
                    continue
                checkpoint = scope.get('checkpoint', 0)
                matches = [c['progress'] for b in batch['buffers']
                           if b.get('recording') == scope.get('recording')
                           for c in b['chunks'] if c.get('progress', {}).get('available') == 1
                           and c['progress'].get('event') == scope.get('progress_event')]
                if checkpoint and len(matches) == 1:
                    reached = matches[0].get('reached', 0)
                    scope['completion'] = ('completed_prefix' if reached >= checkpoint else
                                           'first_unconfirmed_interval' if reached + 1 == checkpoint else
                                           'later_unconfirmed')
            if batch.get('render_coverage', {}).get('omitted', 0):
                gaps.append(batch['role'] + ': render scope cap/history omitted details')
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
    return dict(reports=reports, argument_fills=argument_fills, argument_inputs=argument_inputs, argument_inputs_truncated=argument_inputs_truncated, indirect_writers=writers, indirect_origins=origins, tiling_parameters=tiling, parameter_reuse=reuse, limitation=(
        'CPU recording, commit and callback observations only; scheduled does not identify '
        'an executing command. Callback status and native point-in-time status are separate. '
        'No exact shader, dependency deadlock, driver cause or resource lifetime fault established.'))
