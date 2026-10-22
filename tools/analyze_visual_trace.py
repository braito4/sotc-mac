"""Build a private offline report from bounded game-frame diagnostic evidence."""
import argparse
import base64
from collections import Counter
import html
import json
import math
from pathlib import Path
import re
import struct
import zlib


def load_events(directory):
    events, warnings, seen = [], [], set()
    for name, limit in (('frames.previous.jsonl', 4 << 20), ('frames.jsonl', 4 << 20),
                        ('captures.jsonl', 128 << 10)):
        path = directory / name
        if not path.exists():
            continue
        if path.is_symlink() or path.stat().st_size > limit:
            warnings.append(name + ': redirected or oversized journal ignored')
            continue
        for line in path.read_bytes().splitlines():
            try:
                if len(line) > 16384:
                    raise ValueError('oversized record')
                event = json.loads(line)
                if not isinstance(event, dict) or event.get('schema') != 1:
                    raise ValueError('unknown schema')
                time = event.get('t_s')
                if not isinstance(time, (int, float)) or not math.isfinite(time) or time < 0:
                    raise ValueError('invalid time')
                key = json.dumps(event, sort_keys=True)
                if key not in seen:
                    seen.add(key)
                    events.append(event)
            except (ValueError, TypeError, RecursionError):
                warnings.append(name + ': invalid/partial record ignored')
    return sorted(events, key=lambda e: e['t_s']), warnings


def preview_png(directory, name):
    if not isinstance(name, str) or not re.fullmatch(r'frame-\d{6}\.ppm', name):
        raise ValueError('invalid preview name')
    path = directory / name
    if path.is_symlink() or path.stat().st_size > 320 * 180 * 3 + 64:
        raise ValueError('redirected or oversized preview')
    data = path.read_bytes()
    match = re.match(rb'P6\n(\d{1,3}) (\d{1,3})\n255\n', data)
    if not match:
        raise ValueError('invalid PPM header')
    width, height = map(int, match.groups())
    pixels = data[match.end():]
    if not (0 < width <= 320 and 0 < height <= 180) or len(pixels) != width * height * 3:
        raise ValueError('invalid PPM dimensions or length')
    def chunk(kind, content):
        return struct.pack('>I', len(content)) + kind + content + struct.pack('>I', zlib.crc32(kind + content))
    scanlines = b''.join(b'\0' + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))


def number(value):
    try:
        return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)
    except OverflowError:
        return False


def load_audio(directory):
    path = directory.parent/'audio.jsonl'
    if not path.exists() or path.is_symlink() or path.stat().st_size > 2 << 20:
        return []
    records = []
    for line in path.read_bytes().splitlines()[:1024]:
        try:
            value = json.loads(line)
            if isinstance(value, dict) and value.get('schema') == 1 and value.get('event') in ('audio_window', 'audio_output_error'):
                records.append(value)
        except (ValueError, RecursionError):
            pass
    return records


def load_memory(directory):
    root = directory.parent/'memory'
    result = {'images': [], 'buffers': [], 'summary': [], 'warnings': []}
    if not root.exists() or root.is_symlink():
        return result
    for name, key, cap in (('images.jsonl','images',8 << 20), ('buffers.jsonl','buffers',512 << 10),
                           ('summary.previous.jsonl','summary',1 << 20), ('summary.jsonl','summary',1 << 20)):
        path = root/name
        if not path.exists():
            continue
        if path.is_symlink() or not path.is_file() or path.stat().st_size > cap:
            result['warnings'].append(name+': redirected or oversized input ignored')
            continue
        for line in path.read_bytes().splitlines():
            try:
                if len(line) > 16384:
                    raise ValueError('oversized record')
                event = json.loads(line)
                if not isinstance(event,dict) or event.get('schema') != 1:
                    raise ValueError('unknown schema')
                if not number(event.get('monotonic_ns')) or event['monotonic_ns'] < 0:
                    raise ValueError('invalid timestamp')
                result[key].append(event)
            except (ValueError,TypeError,RecursionError):
                result['warnings'].append(name+': invalid/partial record ignored')
    result['summary'].sort(key=lambda e:e['monotonic_ns'])
    return result


def render_memory(directory):
    data = load_memory(directory)
    esc = lambda value:html.escape(str(value),quote=True)
    parts = ['<h2>Resource allocation and lifetime evidence</h2>']
    summaries = data['summary']
    if not summaries:
        parts.append('<p>No resource summary recorded. This run may predate memory tracing, '
                     'have it disabled, or stop before a summary. Inventory is unknown.</p>')
    else:
        last = dict(summaries[-1])
        # The first recorder used "total" for upstream counters reset by frame stats.
        for key in ('kept','freed'):
            last.setdefault('gc_'+key+'_window_sample',last.get('gc_'+key+'_total','unknown'))
        parts.append('<p>CPU metadata, not proof of physical residency, a leak or safe eviction. '
                     'Only a returned VMA destruction counts as destroyed. Deletion queued means '
                     'the original deferred operation has not yet reported destruction. '
                     'The memory journal uses host monotonic timestamps; its sequence is not a game frame.</p><table>')
        for field in ('event','code','native_allocated_bytes','native_recommended_bytes',
                      'live_count','live_bytes','allocated_count','allocated_bytes',
                      'destroyed_count','destroyed_bytes','tracked_count','tracked_bytes',
                      'tracking_overflow','untracked_frees','unmatched_frees','observation_errors','dropped_records',
                      'state_count','state_bytes','size_count','size_bytes',
                      'role_count','role_bytes','scene_proxy_created_count','scene_proxy_created_bytes',
                      'scene_proxy_destroyed_count','scene_proxy_destroyed_bytes',
                      'scene_proxy_pending_count','scene_proxy_pending_bytes',
                      'large_copy_requests','large_copy_requested_bytes','large_copy_completions',
                      'large_copy_completed_bytes','gc_sample_known','gc_age_ns','gc_enabled',
                      'gc_uses_driver_usage','gc_used_bytes','gc_trigger_bytes','gc_pressure_bytes',
                      'gc_critical_bytes','gc_tick','scheduler_tick','gc_kept_window_sample','gc_freed_window_sample',
                      'gc_kept_tiled_download','gc_kept_download_no_pressure'):
            parts.append('<tr><td>'+esc(field)+'</td><td>'+esc(last.get(field,'unknown'))+'</td></tr>')
        parts.append('</table><p>State arrays: constructed/unknown, registered, unregistered, deletion queued. '
                     'Role arrays: unspecified, reduced scene proxy. Proxy ownership is the original UID '
                     'and source mip, not cache registration; queued proxies remain live until actual destruction. '
                     'Size bins: &lt;1, 1–4, 4–16, 16–64, ≥64 MiB. Tracked inventory is bounded to 8192 '
                     'images; overflow or dropped records make per-resource history incomplete. '
                     'Cumulative copied bytes measure traffic, not simultaneous allocation. '
                     'GC kept/freed window samples can reset between observations when frame statistics read them; '
                     'they are not cumulative eviction totals. '
                     'GC values are the last periodic observation, with its age; native values are '
                     'available on the failure snapshot and zero on ordinary snapshots.</p>')
        parts.append('<h3>Largest images still allocated at the last snapshot</h3><table><tr>'
                     '<th>Resource / owner</th><th>Bytes</th><th>Dimensions / levels / layers / samples</th>'
                     '<th>Vulkan format / usage</th><th>Role / state / flags</th><th>Age / pending deletion (s)</th></tr>')
        largest = last.get('largest',[])
        for e in (largest[:12] if isinstance(largest,list) else []):
            if not isinstance(e,dict):
                continue
            def age(field):
                value=e.get(field)
                return f'{max(0,last["monotonic_ns"]-value)/1e9:.3f}' if number(value) and value else 'unknown'
            row = [str(e.get('id'))+' / '+str(e.get('owner')),e.get('bytes'),
                   ' × '.join(str(e.get(k)) for k in ('width','height','depth'))+' / '+
                   ' / '.join(str(e.get(k)) for k in ('levels','layers','samples')),
                   str(e.get('format'))+' / '+str(e.get('usage')),
                   str(e.get('role','unknown'))+' / '+str(e.get('state'))+' / '+str(e.get('flags')),
                   age('created_ns')+' / '+age('retired_ns')]
            parts.append('<tr>'+''.join('<td>'+esc(x)+'</td>' for x in row)+'</tr>')
        parts.append('</table>')
    parts.append('<h3>Early large-buffer origins and copies (≥64 MiB)</h3><table><tr>'
                 '<th>Event / sequence</th><th>Shader / slot</th><th>Declared → clamped bytes</th>'
                 '<th>Request / allocation bytes</th><th>Physical / window start / window bytes</th>'
                 '<th>Buffer / tick / new / copy time (ms)</th></tr>')
    for e in data['buffers'][:100]:
        ns=e.get('elapsed_ns')
        row=[str(e.get('event'))+' / '+str(e.get('sequence')),
             str(e.get('shader'))+' / '+str(e.get('slot')),
             str(e.get('declared_bytes'))+' → '+str(e.get('clamped_bytes')),
             str(e.get('size'))+' / '+str(e.get('allocation')),
             ' / '.join(str(e.get(k)) for k in ('physical','window_start','window_size')),
             ' / '.join(str(e.get(k)) for k in ('buffer','tick','created'))+' / '+
             (f'{ns/1e6:.3f}' if number(ns) else 'unknown')]
        parts.append('<tr>'+''.join('<td>'+esc(x)+'</td>' for x in row)+'</tr>')
    parts.append('</table><p>The first bounded buffer journal is preserved instead of rotated. '
                 'Full resource histories remain in private JSONL files; this view shows the first '
                 '100 buffer events. A shader value of zero means no shader-binding context was '
                 'available. Copy time is CPU staging time, not GPU execution time.</p>')
    if data['warnings']:
        parts.append('<p>Memory reader warnings: '+esc('; '.join(sorted(set(data['warnings']))))+'</p>')
    return '\n'.join(parts)


def render(directory):
    events, warnings = load_events(directory)
    if not events:
        raise ValueError('No valid visual events; this run may predate visual tracing')
    esc = lambda value: html.escape(str(value), quote=True)
    last = events[-1]
    pixels = [e for e in events if e.get('event') == 'pixels'][:24]
    types = Counter(str(e.get('event', 'unknown')) for e in events)
    reasons = Counter(str(e.get('reason', e.get('detail', ''))) for e in events
                      if e.get('event') in ('capture_skipped', 'capture_failed', 'present_skipped'))
    parts = ['<!doctype html><meta charset="utf-8"><title>SotC visual evidence</title>',
             '<meta http-equiv="Content-Security-Policy" content="default-src \'none\'; img-src data:; style-src \'unsafe-inline\'">',
             '<style>body{font:15px system-ui;max-width:1100px;margin:32px auto;padding:0 16px;background:#111820;color:#e6edf3}',
             'table{border-collapse:collapse;width:100%;margin:16px 0}td,th{padding:7px;text-align:left;border-bottom:1px solid #394451}',
             '.gallery{display:flex;flex-wrap:wrap;gap:16px}figure{margin:0;width:320px}img{max-width:100%;background:#000}',
             'figcaption{font-size:13px;margin-bottom:12px}svg{width:100%;height:180px;background:#1b2632}small{color:#adbcc9}</style>',
             '<h1>SotC visual evidence</h1>',
             '<p>Private game-frame data. Presentation API success does not validate pixels. '
             'Samples come from the game image before the window overlay. Dark or identical previews '
             'are measurements, not proof of a black scene, a freeze, or correct rendering.</p>',
             f'<p>Retained timeline: {events[0]["t_s"]:.3f}–{last["t_s"]:.3f} s; '
             f'{len(events)} records; {len(pixels)} completed pixel samples. '
             'The two timeline files rotate; capture records are retained separately. '
             'Counters are cumulative asynchronous process totals, not per-frame GPU results.</p>']
    parts.append('<h2>Native allocation samples</h2>')
    memory = [e for e in events if number(e.get('native_allocated_bytes')) and
              number(e.get('native_recommended_bytes')) and e['native_recommended_bytes'] > 0]
    if memory:
        peak = max(e['native_allocated_bytes'] for e in memory)
        parts.append(f'<p>Peak sampled allocation: {peak / (1 << 30):.3f} GiB. '
                     'The working-set recommendation is not a hard limit or total physical RAM. '
                     'Vulkan/image/staging categories overlap; do not add them to native allocation.</p>')
        selected = memory[::max(1, len(memory) // 250)]
        selected.append(memory[-1])
        t0, span = events[0]['t_s'], max(.001, last['t_s'] - events[0]['t_s'])
        top = max(1, *(max(e['native_allocated_bytes'], e['native_recommended_bytes']) for e in memory))
        parts.append('<svg viewBox="0 0 1000 180" role="img" aria-label="Sampled native allocation and recommendation over time">')
        for field, color in (('native_recommended_bytes', '#e8b44f'), ('native_allocated_bytes', '#61baff')):
            points = ' '.join(f'{10 + 980 * (e["t_s"] - t0) / span:.1f},{170 - 155 * e[field] / top:.1f}' for e in selected)
            parts.append(f'<polyline fill="none" stroke="{color}" stroke-width="2" points="{points}"/>')
        parts.append('</svg><small>Blue: allocation; amber: recommendation. Time increases left to right. '
                     'Values may repeat between native polls; inspect sample age below.</small>')
    else:
        parts.append('<p>No native memory sample recorded.</p>')
    parts.append('<table><tr><th>Latest retained snapshot</th><th>Value</th></tr>')
    for field in ('native_allocated_bytes', 'native_recommended_bytes', 'native_sample_age_ms',
                  'vulkan_tracked_bytes', 'image_bytes', 'staging_bytes', 'staging_sample_age_ms',
                  'staging_allocations', 'staging_reusable_bytes'):
        parts.append(f'<tr><td>{esc(field)}</td><td>{esc(last.get(field, "unknown"))}</td></tr>')
    parts.append('</table><p>staging_bytes: ring device-local/upload/download; dedicated device-local/upload/download; '
                 'held subset of those dedicated buffers. Held bytes are already included in dedicated bytes.</p>')
    parts.append('<p>Allocation counts use ring[3] then dedicated[3]; reusable bytes use device-local/upload/download '
                 'and require both released ownership and completed GPU ticks. These are asynchronous snapshots. '
                 'A negative or absent sample age means unknown; earlier zero-valued journals do not prove an empty pool.</p>')
    parts.append(render_memory(directory))
    parts.append('<h2>Latest cumulative draw counters</h2><table>')
    counters = last.get('counters', {})
    for key, value in (counters.items() if isinstance(counters, dict) else []):
        parts.append(f'<tr><td>{esc(key)}</td><td>{esc(value)}</td></tr>')
    parts.append('</table><h2>Completed game-frame samples</h2><div class="gallery">')
    for e in pixels:
        try:
            png = preview_png(directory, e.get('file'))
            image = '<img alt="Captured game frame" src="data:image/png;base64,' + base64.b64encode(png).decode() + '">'
        except (OSError, ValueError) as error:
            image = '<p>Preview unavailable: ' + esc(error) + '</p>'
        metrics = ', '.join(esc(k) + '=' + esc(e.get(k, 'unknown')) for k in
                            ('mean_luma', 'black_fraction', 'compared', 'identical_preview', 'changed_fraction'))
        parts.append(f'<figure>{image}<figcaption>{e["t_s"]:.3f} s; CPU frame {esc(e.get("frame"))}; '
                     f'ready tick {esc(e.get("ready_tick"))}<br>{metrics}</figcaption></figure>')
    parts.append('</div><p>Previews fit 320×180; the rendering resolution is unchanged. Luma uses encoded '
                 '8-bit RGB, not calibrated physical luminance. Near-black means ≥99% preview pixels have '
                 'all RGB channels ≤4. Change fraction uses a channel difference &gt;8 between adjacent '
                 'same-size samples; identical means exact equality of preview RGB bytes.</p>')
    parts.append('<h2>Retained event counts and skip/failure reasons</h2><table>')
    for key, count in sorted(types.items()) + sorted(reasons.items()):
        parts.append(f'<tr><td>{esc(key)}</td><td>{count}</td></tr>')
    parts.append('</table><h2>Recent diagnostic events</h2><table>')
    for e in [e for e in events if e.get('event') in ('native_command_error', 'capture_failed', 'capture_skipped', 'present_skipped')][-60:]:
        detail = e.get('description', e.get('reason', e.get('detail', '')))
        parts.append(f'<tr><td>{e["t_s"]:.3f} s</td><td>{esc(e.get("event"))}</td><td>{esc(detail)}</td></tr>')
    parts.append('</table><h2>Audio delivery evidence</h2>')
    audio = load_audio(directory)
    if audio:
        parts.append('<p>Audio metadata uses host monotonic timestamps and per-port windows. '
                     'Gaps and late submissions are candidates for audible cuts. Zero SDL input '
                     'queue samples do not prove a hardware underrun: converted/device data is '
                     'not measured. PCM is not recorded. Audio timestamps are not assigned to '
                     'a particular visual frame.</p><table><tr><th>Port / boundary</th><th>Accepted buffers</th>'
                     '<th>Input frames/s (target 48000)</th><th>Period (µs)</th><th>Device quantum</th><th>Input limit (bytes)</th><th>Max gap (µs)</th><th>Max queue wait (µs)</th>'
                     '<th>Max output-lock wait (µs)</th><th>Input zero samples</th></tr>')
        for e in [e for e in audio if e['event'] == 'audio_window'][-60:]:
            e = dict(e)
            fields = [e.get(k) for k in ('accepted_buffers', 'frames', 'window_us')]
            e['input_frames_per_s'] = (f'{fields[0] * fields[1] * 1e6 / fields[2]:.1f}'
                if all(number(value) for value in fields) and fields[2] > 0 else 'unknown')
            parts.append('<tr><td>'+esc(str(e.get('port'))+' / '+str(e.get('boundary')) )+'</td>'+''.join(
                '<td>'+esc(e.get(k, 'unknown'))+'</td>' for k in ('accepted_buffers', 'input_frames_per_s', 'period_us', 'device_frames', 'input_queue_limit_bytes',
                'max_submit_gap_us', 'max_queue_wait_us', 'max_output_lock_wait_us', 'input_queue_zero_samples'))+'</tr>')
        parts.append('</table>')
        for e in [e for e in audio if e['event'] == 'audio_output_error']:
            parts.append('<p>Audio output error: '+esc(e.get('result'))+'; requested ports: '+esc(e.get('ports'))+'</p>')
    else:
        parts.append('<p>No audio delivery metadata was recorded for this run.</p>')
    if warnings:
        parts.append('<p>Reader warnings: ' + esc('; '.join(sorted(set(warnings)))) + '</p>')
    return '\n'.join(parts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='private BB_GPU_USER_DIR/visual directory')
    args = parser.parse_args()
    try:
        directory = args.directory.resolve(strict=True)
        content = render(directory)
        target = directory / 'report.html'
        temporary = directory / 'report.html.tmp'
        if target.is_symlink() or temporary.is_symlink():
            raise ValueError('Refusing redirected report output')
        temporary.write_text(content, encoding='utf-8')
        temporary.replace(target)
        print('Private visual report: ' + str(target))
    except (OSError, ValueError) as error:
        parser.exit(2, str(error) + '\n')


if __name__ == '__main__':
    main()
