"""Supervise one GPU diagnostic, preserving its private output and exit status.

An optional wall deadline includes renderer startup; zero removes only that deadline.
Owned executables use a cancellation pipe and exit themselves; generic synthetic
fixtures can use process-group signals.
Neither mechanism guarantees recovery from a kernel/driver hang. No retries.
"""
import argparse
import errno
import json
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import time

CONTROL_FD_ENV = 'SOTC_DIAGNOSTIC_CONTROL_FD'


def seconds_argument(value):
    if (not value.isascii() or not value.isdigit() or not 1 <= len(value) <= 4
            or not 0 <= int(value) <= 1800):
        raise argparse.ArgumentTypeError('seconds must be 0..1800 (0 means no wall deadline)')
    return int(value)


def stop_group(child):
    """Terminate the dedicated group, escalating even if its leader exits first."""
    for sig in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(child.pid, sig)
        except ProcessLookupError:
            pass
        except OSError as error:
            raise OSError(error.errno, f'killpg({sig.name}): {error.strerror}') from error
        try:
            child.wait(timeout=0.5)
        except subprocess.TimeoutExpired:
            pass
    if child.poll() is None:
        raise OSError(errno.ETIMEDOUT, 'process-group stop: child exit not observed')


def stop_cooperatively(child, control_fd, emit):
    """Ask the owned child to stop; never substitute denied external signals."""
    if child.poll() is not None:
        return
    try:
        os.write(control_fd, b'S')
    except BrokenPipeError:
        pass  # The child may already be exiting; still confirm its status.
    except OSError as error:
        raise OSError(error.errno, f'cancellation pipe: {error.strerror}') from error
    # Keep consuming output while waiting. Otherwise a full pipe can block the
    # child's stop notice (or another last write) before it gets to self-exit.
    os.set_blocking(child.stdout.fileno(), False)
    deadline = time.monotonic() + 2
    while child.poll() is None:
        # Bound each batch so a child that keeps writing cannot starve the
        # cancellation deadline. No external-signal fallback is introduced.
        for _ in range(16):
            try:
                data = os.read(child.stdout.fileno(), 65536)
            except BlockingIOError:
                break
            if not data:
                break
            emit(data)
        remaining = deadline - time.monotonic()
        if remaining <= 0 and child.poll() is None:
            raise OSError(errno.ETIMEDOUT, 'cooperative stop: child exit not observed')
        try:
            child.wait(timeout=max(0, min(remaining, 0.01)))
        except subprocess.TimeoutExpired:
            pass


def supervise(command, seconds, log_path, cooperative=False):
    interrupted = 0

    def interrupted_by(signum, frame):
        nonlocal interrupted
        interrupted = signum

    with open(log_path, 'xb', buffering=0) as log:
        control_notice_at = None
        notice_tail = b''

        def emit(data):
            nonlocal control_notice_at, notice_tail
            if control_notice_at is None:
                combined = notice_tail + data
                notices = (b'STOP_DIAGNOSTIC_CONTROL: parent requested termination\n',
                           b'STOP_DIAGNOSTIC_CONTROL: parent channel closed\n',
                           b'STOP_DIAGNOSTIC_CONTROL: channel read failed\n')
                if any(notice in combined for notice in notices):
                    control_notice_at = time.monotonic()
                notice_tail = combined[-80:]
            log.write(data)
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()

        settings = {key: os.getenv(key) for key in ('BB_DIRECT_MEMORY_ACCESS',
                    'BB_ASYNC_SUBMIT', 'BB_JIT', 'BB_UPSCALER', 'BB_FRAME_STATS',
                    'BB_FRAME_LOG', 'BB_UPSCALE_PRESET', 'BB_RENDER_RES', 'BB_OUTPUT_RES',
                    'BB_FULLSCREEN', 'SDL_VIDEO_MAC_FULLSCREEN_SPACES',
                    'BB_POOL_LAYOUT', 'BB_POOL_SPLIT',
                    'SOTC_GPU_SUBMIT_TRACE', 'SOTC_VISUAL_TRACE', 'SOTC_VISUAL_CAPTURE', 'SOTC_AUDIO_TRACE',
                    'SOTC_MEMORY_TRACE', 'SOTC_METAL_INFLIGHT', 'SOTC_CPU_FAULT_FIELDS')}
        wall_budget = f'{seconds}s' if seconds else 'none'
        emit((f'GPU supervisor: wall budget={wall_budget}, stop on GPU watchdog, no retries; '
              f'stop_mode={"cooperative" if cooperative else "process-group"}; '
              f'settings={json.dumps(settings, sort_keys=True)}\n').encode())
        previous = {sig: signal.signal(sig, interrupted_by)
                    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
        child = None
        control_read = control_write = None
        reason = None
        cleanup_error = None
        stop_requested_at = None
        started = time.monotonic()
        try:
            deadline = started + seconds if seconds else None
            child_env = dict(os.environ)
            child_env.pop(CONTROL_FD_ENV, None)
            if cooperative:
                control_read, control_write = os.pipe()
                child_env[CONTROL_FD_ENV] = str(control_read)
            child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     bufsize=0, start_new_session=True, env=child_env,
                                     pass_fds=(control_read,) if cooperative else ())
            emit(f'GPU supervisor: child_pid={child.pid}\n'.encode())
            if control_read is not None:
                os.close(control_read)
                control_read = None
            with selectors.DefaultSelector() as selector:
                selector.register(child.stdout, selectors.EVENT_READ)
                tail = b''
                eof = False
                while True:
                    if interrupted:
                        reason = (128 + interrupted, b'STOP_GPU_CANCELLED: termination requested after interruption\n')
                        break
                    remaining = deadline - time.monotonic() if deadline is not None else None
                    if remaining is not None and remaining <= 0:
                        reason = (21, b'STOP_GPU_WALL_TIMEOUT: total budget exhausted; termination requested\n')
                        break
                    if eof and child.poll() is not None:
                        break
                    for key, _ in selector.select(0.1 if remaining is None else min(remaining, 0.1)):
                        data = os.read(key.fileobj.fileno(), 65536)
                        if not data:
                            selector.unregister(key.fileobj)
                            eof = True
                            continue
                        emit(data)
                        combined = tail + data
                        if b'GPU watchdog:' in combined:
                            reason = (31, b'STOP_GPU_STALL: renderer reported a stalled GPU; termination requested\n')
                            break
                        tail = combined[-32:]
                    if reason:
                        break
        except OSError as error:
            reason = (1, f'STOP_GPU_SUPERVISOR: {error}\n'.encode())
        finally:
            # Preserve the decision before cleanup, which itself may fail. A
            # natural child exit has no stop decision; the footer records it.
            if reason:
                try:
                    log.write(reason[1])
                except OSError as error:
                    # A failed log write must not skip child termination.
                    cleanup_error = error
            if child is not None:
                cleanup_stage = 'cooperative cancellation' if cooperative else 'process-group cancellation'
                try:
                    if cooperative:
                        if child.poll() is None:
                            stop_requested_at = time.monotonic()
                        stop_cooperatively(child, control_write, emit)
                    else:
                        stop_group(child)
                    # Drain the bounded pipe without blocking if the child
                    # cannot be killed promptly by the host.
                    cleanup_stage = 'output pipe nonblocking mode'
                    os.set_blocking(child.stdout.fileno(), False)
                    cleanup_stage = 'output pipe drain'
                    while True:
                        try:
                            data = os.read(child.stdout.fileno(), 65536)
                        except BlockingIOError:
                            break
                        if not data:
                            break
                        emit(data)
                except OSError as error:
                    cleanup_error = f'{cleanup_stage}: {error}'
                finally:
                    child.stdout.close()
            for fd in (control_read, control_write):
                if fd is not None:
                    os.close(fd)
            for sig, handler in previous.items():
                signal.signal(sig, handler)
        if reason:
            sys.stdout.buffer.write(reason[1])
            sys.stdout.buffer.flush()
        child_status = child.returncode if child is not None else None
        if cleanup_error:
            emit(f'STOP_GPU_SUPERVISOR_CLEANUP: {cleanup_error}\n'.encode())
            result = 1
        elif reason:
            result = reason[0]
        else:
            result = child_status if child_status >= 0 else 128 - child_status
        notice_delay = ('unknown' if control_notice_at is None or stop_requested_at is None else
                        f'{(control_notice_at-stop_requested_at)*1000:.3f}')
        emit((f'GPU supervisor: finished elapsed={time.monotonic()-started:.3f}s '
              f'child_returncode={child_status} exit={result} '
              f'control_notice_seen={int(control_notice_at is not None)} '
              f'control_notice_after_request_ms={notice_delay}\n').encode())
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', required=True, type=seconds_argument,
                        help='total wall budget, 0..1800 seconds; 0 keeps supervision without a deadline')
    parser.add_argument('--log', required=True, type=Path)
    parser.add_argument('--cooperative-stop', action='store_true',
                        help='use the cancellation pipe implemented by our diagnostic executable')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('a diagnostic command is required after --')
    try:
        return supervise(command, args.seconds, args.log, cooperative=args.cooperative_stop)
    except OSError as error:
        parser.exit(1, f'STOP_GPU_SUPERVISOR: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
