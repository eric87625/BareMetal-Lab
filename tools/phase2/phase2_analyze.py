#!/usr/bin/env python3
"""Phase2 UART log parser + plots.

Usage (recommended defaults):

    # After running capture_uart_log.py, logs are saved under tools/out/phase2/
    # This command auto-detects the latest mode1*.txt and mode2*.txt:
    python phase2_analyze.py

Override inputs explicitly:

    python phase2_analyze.py --mode1-log path/to/mode1.txt --mode2-log path/to/mode2.txt

The parser is resilient to extra non-CSV lines.
It extracts only:
- Header: iter,mode,high_wait_ticks,low_hold_ticks,medium_spin_count
- Data rows: five integers separated by commas
- Ignores: STATS lines and everything else

Outputs:
- analysis/clean_mode1.csv
- analysis/clean_mode2.csv
- analysis/combined.csv
- analysis/summary.csv
- analysis/fig_all.png
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
import sys
from datetime import datetime
import hashlib

import pandas as pd
import matplotlib.pyplot as plt


CSV_HEADER = "iter,mode,high_wait_ticks,low_hold_ticks,medium_spin_count"
DATA_RE = re.compile(r"^(\d+),(\d+),(\d+),(\d+),(\d+)$")
CFG_PREFIX = "CFG,"


def default_phase2_out_dir() -> Path:
    # tools/phase2/phase2_analyze.py -> tools/out/phase2
    return (Path(__file__).resolve().parent / ".." / "out" / "phase2").resolve()


def pick_latest(pattern: str, search_dir: Path) -> Path | None:
    matches = sorted(search_dir.glob(pattern), key=lambda p: p.stat().st_mtime, reverse=True)
    return matches[0] if matches else None


def sanitize_token(s: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]+", "", s)


def strip_mode_prefix(stem: str, mode: int) -> str | None:
    prefix = f"mode{mode}_"
    if stem.startswith(prefix):
        rest = stem[len(prefix):]
        return rest if rest else None
    return None


def parse_cfg_line(line: str) -> dict[str, str]:
    """Parse 'CFG,key=value,...' into a dict of string values."""
    if not line.startswith(CFG_PREFIX):
        return {}

    rest = line[len(CFG_PREFIX):]
    parts = [p.strip() for p in rest.split(",") if p.strip()]
    cfg: dict[str, str] = {}
    for p in parts:
        if "=" not in p:
            continue
        k, v = p.split("=", 1)
        k = k.strip()
        v = v.strip()
        if not k:
            continue
        cfg[k] = v
    return cfg


def extract_dataframe(log_path: Path) -> tuple[pd.DataFrame, dict[str, str]]:
    rows: list[dict[str, int]] = []
    cfg: dict[str, str] = {}

    with log_path.open("r", encoding="utf-8", errors="ignore") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue

            if line.startswith(CFG_PREFIX):
                # Keep the last CFG line if multiple appear.
                cfg = parse_cfg_line(line)
                continue

            if line == CSV_HEADER:
                continue

            m = DATA_RE.match(line)
            if not m:
                continue

            it, mode, high_wait, low_hold, spin = map(int, m.groups())
            rows.append(
                {
                    "iter": it,
                    "mode": mode,
                    "high_wait_ticks": high_wait,
                    "low_hold_ticks": low_hold,
                    "medium_spin_count": spin,
                }
            )

    df = pd.DataFrame(rows)
    if not df.empty:
        df = df.sort_values(["mode", "iter"]).drop_duplicates(["mode", "iter"], keep="last").reset_index(drop=True)
    return df, cfg


def cfg_pick(cfg1: dict[str, str], cfg2: dict[str, str], key: str) -> str | None:
    """Pick a representative cfg value from two logs.

    Prefer mode1's cfg, else mode2's cfg. If both exist and differ, return "m1|m2".
    """
    v1 = cfg1.get(key)
    v2 = cfg2.get(key)
    if v1 is not None and v2 is not None and v1 != v2:
        return f"{v1}|{v2}"
    return v1 if v1 is not None else v2


def make_important_params_suffix(cfg1: dict[str, str], cfg2: dict[str, str]) -> str:
    """Short, important, tunable params for directory suffix.

    The goal is traceability without overly long paths.
    Timestamp is handled separately.
    """

    # Important knobs you actually tune in Core/Inc/phase2_pi_config.h
    realistic = cfg_pick(cfg1, cfg2, "realistic")
    hold = cfg_pick(cfg1, cfg2, "low_hold_ms")
    hold_j = cfg_pick(cfg1, cfg2, "low_hold_jitter_ms")
    high_j = cfg_pick(cfg1, cfg2, "high_start_jitter_ms")
    med_min = cfg_pick(cfg1, cfg2, "medium_spin_min")
    med_max = cfg_pick(cfg1, cfg2, "medium_spin_max")
    pause = cfg_pick(cfg1, cfg2, "medium_pause_ticks")
    crit_steps = cfg_pick(cfg1, cfg2, "low_critical_steps")

    tokens: list[str] = []

    if realistic is not None:
        tokens.append(f"r{realistic}")
    if hold is not None:
        tokens.append(f"hold{hold}")
    if hold_j is not None:
        tokens.append(f"j{hold_j}")
    if high_j is not None:
        tokens.append(f"hj{high_j}")
    if med_min is not None and med_max is not None:
        tokens.append(f"med{med_min}-{med_max}")
    if pause is not None:
        tokens.append(f"pause{pause}")
    if crit_steps is not None:
        tokens.append(f"crit{crit_steps}")

    raw = "_".join(tokens)
    safe = sanitize_token(raw)

    if not safe:
        return ""

    # Windows path safety
    if len(safe) <= 120:
        return safe

    h = hashlib.sha1(safe.encode("utf-8")).hexdigest()[:10]
    return safe[:100] + f"_h{h}"


def warn(msg: str) -> None:
    print(f"[WARN] {msg}", file=sys.stderr)


def ensure_out_dir(out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)


def now_stamp_minutes() -> str:
    # User preference: no seconds
    return datetime.now().strftime("%Y%m%d_%H%M")


def compute_summary(df: pd.DataFrame) -> dict[str, float | int]:
    s = df["high_wait_ticks"]
    overhead = df["high_wait_ticks"] - df["low_hold_ticks"]

    mode_val = int(df["mode"].iloc[0]) if not df.empty else -1
    n = int(len(df))
    std = float(s.std(ddof=1)) if n > 1 else 0.0

    return {
        "mode": mode_val,
        "n": n,
        "high_wait_mean": float(s.mean()),
        "high_wait_std": std,
        "high_wait_min": int(s.min()),
        "high_wait_p50": float(s.median()),
        "high_wait_max": int(s.max()),
        "low_hold_mean": float(df["low_hold_ticks"].mean()),
        "overhead_mean": float(overhead.mean()),
        "overhead_max": int(overhead.max()),
    }


def plot_all_in_one(df1: pd.DataFrame, df2: pd.DataFrame, out_path: Path) -> None:
    fig, axes = plt.subplots(nrows=3, ncols=1, figsize=(10, 10))

    # (1) timeseries
    ax0 = axes[0]
    ax0.plot(df1["iter"], df1["high_wait_ticks"], label="Mode 1 (Mutex PI)", linewidth=1.0)
    ax0.plot(df2["iter"], df2["high_wait_ticks"], label="Mode 2 (Binary no PI)", linewidth=1.0)
    ax0.set_title("High wait ticks vs iteration")
    ax0.set_xlabel("iteration")
    ax0.set_ylabel("high_wait_ticks")
    ax0.grid(True, linewidth=0.3)
    ax0.legend()

    # (2) boxplot
    ax1 = axes[1]
    data = [df1["high_wait_ticks"].to_list(), df2["high_wait_ticks"].to_list()]
    labels = ["Mode 1", "Mode 2"]
    ax1.boxplot(data, tick_labels=labels, showfliers=True)
    ax1.set_title("High wait ticks distribution")
    ax1.set_ylabel("high_wait_ticks")
    ax1.grid(True, axis="y", linewidth=0.3)

    # (3) mean bar
    ax2 = axes[2]
    means = [float(df1["high_wait_ticks"].mean()), float(df2["high_wait_ticks"].mean())]
    ax2.bar(labels, means)
    ax2.set_title("Mean high_wait_ticks")
    ax2.set_ylabel("high_wait_ticks")
    ax2.grid(True, axis="y", linewidth=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--in-dir",
        default=default_phase2_out_dir(),
        type=Path,
        help="Directory containing captured logs (default: tools/out/phase2)",
    )
    ap.add_argument("--mode1-log", default=None, type=Path, help="UART log text for Mode 1 (mode=1)")
    ap.add_argument("--mode2-log", default=None, type=Path, help="UART log text for Mode 2 (mode=2)")
    ap.add_argument(
        "--out",
        default=None,
        type=Path,
        help="Output directory (default: <in-dir>/analysis_<YYYYmmdd_HHMM>__<important_params>)",
    )
    args = ap.parse_args()

    in_dir = args.in_dir.resolve()
    mode1_log = args.mode1_log.resolve() if args.mode1_log is not None else pick_latest("mode1*.txt", in_dir)
    mode2_log = args.mode2_log.resolve() if args.mode2_log is not None else pick_latest("mode2*.txt", in_dir)

    if mode1_log is None:
        raise SystemExit(f"Cannot find mode1 log under {in_dir} (expected mode1*.txt) or pass --mode1-log")
    if mode2_log is None:
        raise SystemExit(f"Cannot find mode2 log under {in_dir} (expected mode2*.txt) or pass --mode2-log")

    df1, cfg1 = extract_dataframe(mode1_log)
    df2, cfg2 = extract_dataframe(mode2_log)

    if df1.empty:
        raise SystemExit(f"No CSV rows found in {mode1_log}")
    if df2.empty:
        raise SystemExit(f"No CSV rows found in {mode2_log}")

    stamp = now_stamp_minutes()
    params_suffix = make_important_params_suffix(cfg1, cfg2) if (cfg1 or cfg2) else ""
    default_out_name = f"analysis_{stamp}" + (f"__{params_suffix}" if params_suffix else "")
    out_dir = (args.out.resolve() if args.out is not None else (in_dir / default_out_name)).resolve()
    ensure_out_dir(out_dir)

    # Sanity: mode values in the logs
    mode1_vals = {int(x) for x in set(df1["mode"].unique())}
    mode2_vals = {int(x) for x in set(df2["mode"].unique())}

    if mode1_vals != {1}:
        warn(f"mode1 log contains mode values {sorted(mode1_vals)} (expected only 1). File: {mode1_log}")
    if mode2_vals != {2}:
        warn(f"mode2 log contains mode values {sorted(mode2_vals)} (expected only 2). File: {mode2_log}")

    # Keep clean CSV schema stable (5 columns)
    df1.to_csv(out_dir / "clean_mode1.csv", index=False)
    df2.to_csv(out_dir / "clean_mode2.csv", index=False)

    combined = pd.concat([df1, df2], ignore_index=True)
    combined.to_csv(out_dir / "combined.csv", index=False)

    summary_rows = [compute_summary(df1), compute_summary(df2)]
    # Attach a few important cfg columns to summary for traceability
    for row in summary_rows:
        row["cfg_stamp_yyyyMMdd_HHmm"] = stamp
        row["cfg_suffix"] = params_suffix
    # Store key params (prefer mode1 cfg, else mode2)
    for k in [
        "realistic",
        "low_hold_ms",
        "low_hold_jitter_ms",
        "high_start_jitter_ms",
        "medium_spin_min",
        "medium_spin_max",
        "medium_pause_ticks",
        "low_critical_steps",
    ]:
        v = cfg_pick(cfg1, cfg2, k)
        if v is not None:
            summary_rows[0][f"cfg_{k}"] = v
            summary_rows[1][f"cfg_{k}"] = v
    summary_df = pd.DataFrame(summary_rows).sort_values("mode")
    summary_df.to_csv(out_dir / "summary.csv", index=False)

    plot_all_in_one(df1, df2, out_dir / "fig_all.png")

    # Console summary (quick human-friendly)
    m1 = summary_rows[0]
    m2 = summary_rows[1]
    penalty = float(m2["high_wait_mean"]) - float(m1["high_wait_mean"])
    print("Summary (high_wait_ticks):")
    print(
        "  Mode1: "
        f"mean={m1['high_wait_mean']:.2f}, std={m1['high_wait_std']:.2f}, "
        f"min={m1['high_wait_min']}, max={m1['high_wait_max']}"
    )
    print(
        "  Mode2: "
        f"mean={m2['high_wait_mean']:.2f}, std={m2['high_wait_std']:.2f}, "
        f"min={m2['high_wait_min']}, max={m2['high_wait_max']}"
    )
    print(f"  Mode2-Mode1 mean penalty = {penalty:.2f} ticks")

    print("Wrote:")
    print(f"  inputs:")
    print(f"    mode1: {mode1_log}")
    print(f"    mode2: {mode2_log}")
    print(f"  outputs:")
    print(f"    stamp(min): {stamp}")
    if params_suffix:
        print(f"    params: {params_suffix}")
    print(f"    {out_dir / 'clean_mode1.csv'} (modes={sorted(mode1_vals)})")
    print(f"    {out_dir / 'clean_mode2.csv'} (modes={sorted(mode2_vals)})")
    print(f"    {out_dir / 'combined.csv'}")
    print(f"    {out_dir / 'summary.csv'}")
    print(f"    {out_dir / 'fig_all.png'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
