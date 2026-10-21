"""Supervise one GPU diagnostic, preserving its private output and exit status.

The wall deadline includes renderer startup. A renderer GPU-watchdog message stops
the process group immediately; neither this parent nor the child's alarm can
guarantee recovery from a kernel/driver hang. There are no automatic retries.
"""
import argparse
import json
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import time


def seconds_argument(value):
    if (not value.isascii() or not value.isdigit() or not 1 <= len(value) <= 4
            or not 1 <= int(value) <= 1800):
        raise argparse.ArgumentTypeError('seconds must be 1..1800')
    return int(value)


def stop_group(child):
    """Terminate the dedicated group, escalating even if its leader exits first."""
    for sig in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(child.pid, sig)
        except ProcessLookupError:
            pass
        try:
            child.wait(timeout=0.5)
        except subprocess.TimeoutExpired:
            pass


def supervise(command, seconds, log_path):
    interrupted = 0

    def interrupted_by(signum, frame):
        nonlocal interrupted
        interrupted = signum

    with open(log_path, 'xb', buffering=0) as log:
        def emit(data):
            log.write(data)
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()

        settings = {key: os.getenv(key) for key in ('BB_DIRECT_MEMORY_ACCESS',
                    'BB_ASYNC_SUBMIT', 'BB_JIT', 'BB_UPSCALER')}
        emit((f'GPU supervisor: wall budget={seconds}s, stop on GPU watchdog, no retries; '
              f'settings={json.dumps(settings, sort_keys=True)}\n').encode())
        previous = {sig: signal.signal(sig, interrupted_by)
                    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
        child = None
        reason = None
        try:
            deadline = time.monotonic() + seconds
            child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     bufsize=0, start_new_session=True)
            with selectors.DefaultSelector() as selector:
                selector.register(child.stdout, selectors.EVENT_READ)
                tail = b''
                eof = False
                while True:
                    if interrupted:
                        reason = (128 + interrupted, b'STOP_GPU_CANCELLED: diagnostic interrupted\n')
                        break
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        reason = (21, b'STOP_GPU_WALL_TIMEOUT: total diagnostic budget exhausted\n')
                        break
                    if eof and child.poll() is not None:
                        break
                    for key, _ in selector.select(min(remaining, 0.1)):
                        data = os.read(key.fileobj.fileno(), 65536)
                        if not data:
                            selector.unregister(key.fileobj)
                            eof = True
                            continue
                        emit(data)
                        combined = tail + data
                        if b'GPU watchdog:' in combined:
                            reason = (31, b'STOP_GPU_STALL: renderer reported a stalled GPU; diagnostic stopped\n')
                            break
                        tail = combined[-32:]
                    if reason:
                        break
        except OSError as error:
            reason = (1, f'STOP_GPU_SUPERVISOR: {error}\n'.encode())
        finally:
            if child is not None:
                stop_group(child)
                # Drain the bounded pipe after termination, without a blocking
                # read if a kernel-stalled process cannot be killed promptly.
                os.set_blocking(child.stdout.fileno(), False)
                while True:
                    try:
                        data = os.read(child.stdout.fileno(), 65536)
                    except BlockingIOError:
                        break
                    if not data:
                        break
                    emit(data)
                child.stdout.close()
            for sig, handler in previous.items():
                signal.signal(sig, handler)
        if reason:
            emit(reason[1])
            return reason[0]
        result = child.returncode
        return result if result >= 0 else 128 - result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', required=True, type=seconds_argument)
    parser.add_argument('--log', required=True, type=Path)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('a diagnostic command is required after --')
    try:
        return supervise(command, args.seconds, args.log)
    except OSError as error:
        parser.exit(1, f'STOP_GPU_SUPERVISOR: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
