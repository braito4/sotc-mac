"""Read completed-run per-address watch summaries; not live snapshots."""
from pathlib import Path
import struct

def review(path,address):
    result=dict(state='absent',address=address,continuity='unknown',allocation_identity='unknown',
        limitation='Numeric registration summary since table admission, not allocation lifetime, all writes or an ordered event history. Table replacements, global drops and incomplete rows leave gaps. Completed processes only.')
    path=Path(path)
    if not path.is_file():return result
    try:
        if path.stat().st_size!=4096+8192*128:raise ValueError('invalid summary size')
        data=path.read_bytes();header=struct.unpack_from('<512Q',data)
        if data[:8]!=b'SOTCWS01' or header[1:5]!=(1,8192,4,16):raise ValueError('invalid summary header')
        result.update(state='not_retained',attempts=header[5],contention_drops=header[6],table_replacements=header[7],producer_pid=header[8])
        names=('busy','address','admission_id','first_ns','last_ns','arms','refreshes','evictions','registration_drops','last_packet','last_capture','last_watch_generation','first_ticket','last_ticket','related_address','observations')
        matches=[]
        for offset in range(4096,len(data),128):
            row=dict(zip(names,struct.unpack_from('<16Q',data,offset)))
            if row['address']==address:matches.append(row)
        if len(matches)>1:raise ValueError('duplicate target slots')
        if matches:
            row=matches[0]
            if row['busy']:result['state']='incomplete';return result
            if (not row['admission_id'] or row['first_ticket']!=row['admission_id'] or
                not row['first_ticket']<=row['last_ticket']<=header[5] or
                row['observations']!=sum(row[k] for k in ('arms','refreshes','evictions','registration_drops'))):
                raise ValueError('inconsistent summary counters')
            result.update(state='observed',summary=row)
            if row['evictions'] or row['registration_drops']:result['continuity']='known_gap_or_registration_failure'
            elif header[6] or header[7]:result['continuity']='coverage_incomplete'
    except (ValueError,OSError,struct.error) as e:result.update(state='invalid',reason=str(e))
    return result
