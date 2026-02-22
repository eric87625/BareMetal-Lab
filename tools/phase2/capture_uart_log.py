#!/usr/bin/env python3
"""Capture Phase2 UART log directly from a serial port.

This script records *raw* UART output to a .txt file.
Defaults are tuned for this project:

- Start writing only after the Phase2 CSV header is seen (captures a clean run)
- Start writing only after the Phase2 CFG line or CSV header is seen (captures a clean run)
- Stop after capturing iter==500

You can override these defaults via CLI options.

Examples (Windows):

    # Capture Mode A (mode=1)
    python capture_uart_log.py --port COM5 --baud 115200

    # Capture Mode B (mode=2)
    python capture_uart_log.py --port COM5 --baud 115200

    # Recommended: start script first, then press reset on the board.
    # This guarantees you capture iter=1..500 even if you can't time it.
    python capture_uart_log.py --port COM5 --baud 115200 --out out/phase2/mode2.txt --start-on-header --stop-after-iter 500

Tips:
- Run one capture per firmware run (per PI_MODE).
- If you want to keep everything, omit --stop-after-rows and Ctrl+C to stop.
    (Or disable the default stop-after-iter with --no-stop-after-iter)
"""

from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path
from datetime import datetime

import serial


CSV_HEADER = "iter,mode,high_wait_ticks,low_hold_ticks,medium_spin_count"
DATA_RE = re.compile(r"^(\d+),(\d+),(\d+),(\d+),(\d+)$")


def is_cfg_line(stripped_line: str) -> bool:
    # Firmware emits: CFG,key=value,key=value,...
    return stripped_line.startswith("CFG,")


def choose_non_overwriting_path(path: Path) -> Path:
    """Return a path that won't overwrite existing file by adding suffix _2/_3..."""
    if not path.exists():
        return path

    stem = path.stem
    suffix = path.suffix
    parent = path.parent

    for i in range(2, 1000):
        candidate = parent / f"{stem}_{i}{suffix}"
        if not candidate.exists():
            return candidate

    raise RuntimeError(f"Too many existing files like {path.name}")


def default_out_dir() -> Path:
    # Default to tools/out/phase2 relative to this script location
    return (Path(__file__).resolve().parent / ".." / "out" / "phase2").resolve()


def now_stamp() -> str:
    # Local time; safe for filenames (no ':' etc)
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def sanitize_token(s: str) -> str:
    # Keep it simple and Windows-safe.
    return re.sub(r"[^A-Za-z0-9._-]+", "", s)


def auto_out_name(
    *,
    mode: int,
    run_id: str,
    port: str,
    baud: int,
    start_on_header: bool,
    stop_after_iter: int,
    stop_after_rows: int,
    tag: str | None,
) -> str:
    tokens: list[str] = [f"mode{mode}", sanitize_token(run_id)]

    tokens.append(f"p{sanitize_token(port)}")
    tokens.append(f"b{baud}")
    tokens.append(f"hdr{1 if start_on_header else 0}")

    if stop_after_iter:
        tokens.append(f"iter{stop_after_iter}")
    else:
        tokens.append("iter0")

    if stop_after_rows:
        tokens.append(f"rows{stop_after_rows}")

    if tag:
        tokens.append(sanitize_token(tag))

    return "_".join(tokens) + ".txt"


def open_serial(port: str, baud: int, timeout_s: float) -> serial.Serial:
    # timeout: read timeout in seconds
    return serial.Serial(
        port=port,
        baudrate=baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=timeout_s,
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Serial port (e.g. COM5)")
    ap.add_argument("--baud", type=int, default=115200, help="Baud rate")
    ap.add_argument(
        "--out",
        type=Path,
        default=None,
        help=(
            "Output text file path. If omitted, saves under tools/out/phase2/ and auto-names "
            "using timestamp + parameters (and includes mode1/mode2 based on detected CSV mode)."
        ),
    )
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory used when --out is omitted (default: tools/out/phase2)",
    )
    ap.add_argument(
        "--stop-after-rows",
        type=int,
        default=0,
        help="Stop automatically after capturing this many CSV data rows (0 = never)",
    )
    ap.add_argument(
        "--stop-after-iter",
        type=int,
        default=500,
        help="Stop automatically after capturing a CSV row with iter == N (0 = never)",
    )
    ap.add_argument(
        "--no-stop-after-iter",
        action="store_true",
        help="Disable default --stop-after-iter behavior (capture indefinitely unless other stop condition).",
    )

    # Default start-on-header enabled; provide an opt-out.
    ap.add_argument(
        "--start-immediately",
        action="store_true",
        help="Start writing/echoing immediately (do not wait for CSV header).",
    )
    ap.add_argument(
        "--quiet",
        action="store_true",
        help="Do not echo incoming lines to console",
    )
    ap.add_argument(
        "--tag",
        default=None,
        help="Optional extra label to include in the auto-generated filename (e.g. 'realisticA' or 'hold80').",
    )
    ap.add_argument(
        "--run-id",
        default=None,
        help=(
            "Optional run id included in the auto-generated filename. "
            "Tip: use the SAME --run-id for the Mode1 and Mode2 capture so analysis can pair them automatically. "
            "If omitted, a timestamp is used."
        ),
    )
    ap.add_argument(
        "--read-timeout",
        type=float,
        default=0.5,
        help="Serial read timeout seconds",
    )
    ap.add_argument(
        "--overall-timeout",
        type=float,
        default=0.0,
        help="Stop after N seconds regardless of rows (0 = never)",
    )
    args = ap.parse_args()

    start_on_header = not args.start_immediately
    stop_after_iter = 0 if args.no_stop_after_iter else args.stop_after_iter

    run_id = args.run_id if args.run_id is not None else now_stamp()

    out_dir = (args.out_dir.resolve() if args.out_dir is not None else default_out_dir())
    out_dir.mkdir(parents=True, exist_ok=True)

    csv_rows = 0
    saw_header = False
    started = False
    detected_mode: int | None = None

    # If args.out is not provided, we open the output file only after we detect the mode.
    out_path: Path | None = args.out.resolve() if args.out is not None else None
    out_file = None
    buffered_lines: list[str] = []

    start_time = time.time()

    try:
        with open_serial(args.port, args.baud, args.read_timeout) as ser:
            # Best-effort: clear any stale buffered bytes
            try:
                ser.reset_input_buffer()
            except Exception:
                pass

            if not args.quiet:
                if out_path is not None:
                    print(f"Capturing from {args.port} @ {args.baud} -> {out_path}")
                else:
                    print(f"Capturing from {args.port} @ {args.baud} -> {out_dir} (auto file name w/ timestamp)")
                if args.stop_after_rows:
                    print(f"Auto-stop after {args.stop_after_rows} CSV rows")
                if stop_after_iter:
                    print(f"Auto-stop at iter == {stop_after_iter}")
                if start_on_header:
                    print("Start-on-header enabled: waiting for CFG or CSV header...")
                print("(Press Ctrl+C to stop)\n")

            while True:
                if args.overall_timeout and (time.time() - start_time) >= args.overall_timeout:
                    if not args.quiet:
                        print("\nOverall timeout reached, stopping.")
                    break

                raw = ser.readline()  # bytes until \n or timeout
                if not raw:
                    continue

                # Decode as UTF-8 with replacement; keep it robust to noise
                line = raw.decode("utf-8", errors="replace")

                stripped = line.strip()

                # Start gating: optionally ignore everything until we see the CSV header.
                if start_on_header and not started:
                    if is_cfg_line(stripped) or stripped == CSV_HEADER:
                        started = True
                        saw_header = (stripped == CSV_HEADER)
                    else:
                        continue

                # If not gated, we start immediately.
                if not start_on_header and not started:
                    started = True

                # Normalize CRLF -> LF
                line_norm = line.replace("\r\n", "\n").replace("\r", "\n")

                if stripped == CSV_HEADER:
                    saw_header = True
                    # We might not have an output file yet; buffer header.
                    buffered_lines.append(line_norm)
                    continue

                m = DATA_RE.match(stripped) if saw_header else None
                if m:
                    csv_rows += 1
                    iter_val = int(m.group(1))
                    mode_val = int(m.group(2))

                    if detected_mode is None:
                        detected_mode = mode_val

                    # If --out not provided, decide output file name after seeing first data row.
                    if out_file is None:
                        if out_path is None:
                            name = auto_out_name(
                                mode=detected_mode,
                                run_id=run_id,
                                port=args.port,
                                baud=args.baud,
                                start_on_header=start_on_header,
                                stop_after_iter=stop_after_iter,
                                stop_after_rows=args.stop_after_rows,
                                tag=args.tag,
                            )
                            out_path = choose_non_overwriting_path(out_dir / name)

                        out_path.parent.mkdir(parents=True, exist_ok=True)
                        out_file = out_path.open("w", encoding="utf-8", newline="\n")

                        # Flush buffered header (and any pre-lines) first.
                        for bl in buffered_lines:
                            out_file.write(bl)
                        buffered_lines.clear()

                        if not args.quiet:
                            print(f"\nDetected mode={detected_mode}; writing to {out_path}\n")

                    # Write current data line
                    out_file.write(line_norm)
                    out_file.flush()

                    if not args.quiet:
                        sys.stdout.write(line_norm)
                        sys.stdout.flush()

                    if stop_after_iter and iter_val >= stop_after_iter:
                        if not args.quiet:
                            print(f"\nCaptured iter == {iter_val}, stopping.")
                        break

                    if args.stop_after_rows and csv_rows >= args.stop_after_rows:
                        if not args.quiet:
                            print(f"\nCaptured {csv_rows} CSV rows, stopping.")
                        break

                    continue

                # Non-data lines: if output file exists, write directly; else buffer.
                if out_file is not None:
                    out_file.write(line_norm)
                    out_file.flush()
                else:
                    buffered_lines.append(line_norm)

                if not args.quiet:
                    sys.stdout.write(line_norm)
                    sys.stdout.flush()

            # End while loop

            if out_file is not None:
                out_file.close()

    except serial.SerialException as e:
        raise SystemExit(f"Serial error: {e}")
    except KeyboardInterrupt:
        if not args.quiet:
            print("\nStopped by user.")

    if not args.quiet:
        if out_path is not None:
            print(f"Wrote raw log to: {out_path}")
        else:
            print("No output file was created (CSV header/data not detected).")
        if args.stop_after_rows:
            print(f"CSV rows captured: {csv_rows}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
