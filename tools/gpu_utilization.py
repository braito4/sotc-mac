"""Optional macOS whole-device GPU samples; not per-game utilization."""
import csv
import math
import re
import subprocess
import threading
import time


def utilization_rows(payload):
    result = []
    for line in payload.decode('utf-8', errors='replace').splitlines():
        if not re.search(r'"PerformanceStatistics"\s*=\s*\{', line):
            continue
        row = []
        for key in ('Device Utilization %', 'Renderer Utilization %', 'Tiler Utilization %'):
            match = re.search(r'"' + re.escape(key) + r'"\s*=\s*(-?\d+(?:\.\d+)?)(?=[,}])', line)
            value = float(match[1]) if match else None
            row.append(value if value is not None and math.isfinite(value) and 0 <= value <= 100 else '')
        result.append(row)
    return result


class Monitor:
    def __init__(self, path):
        self.path = path
        self.stop_event = threading.Event()
        self.thread = threading.Thread(target=self.run, daemon=True)

    def start(self):
        self.thread.start()

    def close(self):
        self.stop_event.set()
        self.thread.join(timeout=2)

    def run(self):
        started = time.monotonic()
        try:
            with open(self.path, 'x', newline='') as output:
                writer = csv.writer(output)
                writer.writerow(['elapsed_s', 'adapter', 'device_percent', 'renderer_percent',
                                 'tiler_percent', 'status'])
                while not self.stop_event.is_set():
                    try:
                        sample = subprocess.run(['/usr/sbin/ioreg', '-l', '-r', '-c', 'IOAccelerator'],
                                                capture_output=True, timeout=1, check=True)
                        rows = utilization_rows(sample.stdout)
                        for i, row in enumerate(rows):
                            writer.writerow([round(time.monotonic()-started, 3), i, *row,
                                             'ok' if row[0] != '' else 'unavailable'])
                        if not rows:
                            writer.writerow([round(time.monotonic()-started, 3), '', '', '', '', 'unavailable'])
                    except (OSError, ValueError, subprocess.SubprocessError):
                        writer.writerow([round(time.monotonic()-started, 3), '', '', '', '', 'unavailable'])
                    output.flush()
                    self.stop_event.wait(5)
        except OSError:
            # Telemetry must not change game exit status or overwrite existing evidence.
            return
