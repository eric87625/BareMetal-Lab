import argparse
import csv
import os
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

try:
    import serial
except ImportError:
    serial = None

CSV_COLUMNS = [
    "seq",
    "systick_ms",
    "load_active",
    "latency_ticks",
    "latency_us",
    "exec_ticks",
    "exec_us",
    "latency_delta_ticks",
]

DATA_LINE_RE = re.compile(
    r"^\s*(\d+),(\d+),(\d+),(\d+),([0-9]+\.[0-9]+),(\d+),([0-9]+\.[0-9]+),(-?\d+)\s*$"
)


@dataclass
class ParseResult:
    rows: list
    total_lines: int
    matched_lines: int


def parse_latency_lines(lines):
    rows = []
    total = 0
    matched = 0

    for raw in lines:
        total += 1
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("seq,"):
            continue

        match = DATA_LINE_RE.match(line)
        if not match:
            continue

        matched += 1
        rows.append(
            {
                "seq": int(match.group(1)),
                "systick_ms": int(match.group(2)),
                "load_active": int(match.group(3)),
                "latency_ticks": int(match.group(4)),
                "latency_us": float(match.group(5)),
                "exec_ticks": int(match.group(6)),
                "exec_us": float(match.group(7)),
                "latency_delta_ticks": int(match.group(8)),
            }
        )

    return ParseResult(rows=rows, total_lines=total, matched_lines=matched)


def collect_from_serial(port, baud, seconds, out_raw_path):
    if serial is None:
        raise RuntimeError("pyserial 未安裝，請先 pip install -r tools/requirements.txt")

    lines = []
    start = time.time()

    with serial.Serial(port=port, baudrate=baud, timeout=0.5) as ser, open(
        out_raw_path, "w", encoding="utf-8", newline=""
    ) as raw_file:
        print(f"[INFO] 開始擷取 serial: {port} @ {baud}, duration={seconds}s")
        while time.time() - start < seconds:
            payload = ser.readline()
            if not payload:
                continue

            try:
                text = payload.decode("utf-8", errors="ignore").rstrip("\r\n")
            except Exception:
                continue

            lines.append(text)
            raw_file.write(text + "\n")

    print(f"[INFO] Serial 擷取完成，raw 檔案: {out_raw_path}")
    return lines


def save_csv(rows, out_csv_path):
    with open(out_csv_path, "w", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=CSV_COLUMNS)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def summarize(df):
    print("\n===== 簡易統計 =====")
    print(f"樣本數: {len(df)}")

    grouped = df.groupby("load_active")
    for state, g in grouped:
        label = "LOAD" if state == 1 else "IDLE"
        print(
            f"[{label}] n={len(g)} "
            f"lat(us): min={g['latency_us'].min():.3f}, avg={g['latency_us'].mean():.3f}, max={g['latency_us'].max():.3f}; "
            f"exec(us): min={g['exec_us'].min():.3f}, avg={g['exec_us'].mean():.3f}, max={g['exec_us'].max():.3f}"
        )

    dseq = df["seq"].diff()
    gap = dseq[dseq > 1]
    if len(gap) > 0:
        print("\n===== 取樣/輸出節奏 =====")
        print(
            f"seq gap > 1 的比例: {len(gap) / max(1, len(dseq) - 1) * 100:.2f}% "
            f"(代表 logger 未輸出每一筆 ISR 資料)"
        )
        print(f"平均 seq 間隔: {dseq.dropna().mean():.2f}")


def _ecdf(series):
    values = series.dropna().to_numpy()
    values.sort()
    if len(values) == 0:
        return values, values
    y = (pd.Series(range(1, len(values) + 1)).to_numpy()) / len(values)
    return values, y


def _shade_load_regions(ax, load_series):
    in_load = False
    start_idx = 0

    for idx, value in enumerate(load_series):
        if (not in_load) and (value == 1):
            in_load = True
            start_idx = idx
        elif in_load and (value == 0):
            ax.axvspan(start_idx, idx, color="tab:red", alpha=0.08)
            in_load = False

    if in_load:
        ax.axvspan(start_idx, len(load_series) - 1, color="tab:red", alpha=0.08)


def plot_latency(df, out_png_path):
    fig, axes = plt.subplots(3, 1, figsize=(12, 9), sharex=True)

    x = range(len(df))
    _shade_load_regions(axes[0], df["load_active"])
    _shade_load_regions(axes[1], df["load_active"])

    axes[0].plot(x, df["latency_us"], linewidth=0.8)
    latency_jump = df["latency_us"] > df["latency_us"].median()
    jump_idx = df.index[latency_jump]
    if len(jump_idx) > 0:
        axes[0].scatter(
            jump_idx,
            df.loc[jump_idx, "latency_us"],
            s=10,
            color="tab:red",
            label="latency jump",
            zorder=3,
        )
        axes[0].legend(loc="upper right")
    axes[0].set_ylabel("latency [us]")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(x, df["exec_us"], linewidth=0.8, color="tab:orange")
    axes[1].set_ylabel("exec [us]")
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(x, df["load_active"], linewidth=1.0, color="tab:green")
    axes[2].set_ylabel("load state")
    axes[2].set_xlabel("sample index")
    axes[2].set_yticks([0, 1])
    axes[2].set_yticklabels(["idle", "load"])
    axes[2].grid(True, alpha=0.3)

    fig.suptitle("TIM3 ISR latency / execution with load state", fontsize=12)

    plt.tight_layout(rect=[0, 0.03, 1, 0.97])
    fig.savefig(out_png_path, dpi=150)
    plt.close(fig)


def plot_comparison(df, out_png_compare_path):
    idle = df[df["load_active"] == 0]
    load = df[df["load_active"] == 1]

    fig, axes = plt.subplots(2, 2, figsize=(12, 9))

    axes[0, 0].boxplot(
        [idle["latency_us"], load["latency_us"]],
        tick_labels=["idle", "load"],
        showfliers=True,
    )
    axes[0, 0].set_title("Latency distribution")
    axes[0, 0].set_ylabel("latency [us]")
    axes[0, 0].grid(True, alpha=0.3)

    idle_x, idle_y = _ecdf(idle["latency_us"])
    load_x, load_y = _ecdf(load["latency_us"])
    if len(idle_x):
        axes[0, 1].plot(idle_x, idle_y, label="idle")
    if len(load_x):
        axes[0, 1].plot(load_x, load_y, label="load")
    axes[0, 1].set_title("Latency CDF")
    axes[0, 1].set_xlabel("latency [us]")
    axes[0, 1].set_ylabel("CDF")
    axes[0, 1].grid(True, alpha=0.3)
    axes[0, 1].legend()

    axes[1, 0].boxplot(
        [idle["exec_us"], load["exec_us"]],
        tick_labels=["idle", "load"],
        showfliers=True,
    )
    axes[1, 0].set_title("ISR execution distribution")
    axes[1, 0].set_ylabel("exec [us]")
    axes[1, 0].grid(True, alpha=0.3)

    idle_x, idle_y = _ecdf(idle["exec_us"])
    load_x, load_y = _ecdf(load["exec_us"])
    if len(idle_x):
        axes[1, 1].plot(idle_x, idle_y, label="idle")
    if len(load_x):
        axes[1, 1].plot(load_x, load_y, label="load")
    axes[1, 1].set_title("Execution CDF")
    axes[1, 1].set_xlabel("exec [us]")
    axes[1, 1].set_ylabel("CDF")
    axes[1, 1].grid(True, alpha=0.3)
    axes[1, 1].legend()

    fig.suptitle("Idle vs Load comparison", fontsize=12)
    plt.tight_layout(rect=[0, 0.03, 1, 0.97])
    fig.savefig(out_png_compare_path, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description="Capture/parse STM32 latency CSV and plot")
    parser.add_argument("--port", help="Serial port, e.g. COM5")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baudrate")
    parser.add_argument("--seconds", type=int, default=20, help="Capture duration in seconds")
    parser.add_argument("--input", help="Existing raw text file path (instead of serial capture)")
    parser.add_argument("--outdir", default="tools/out", help="Output directory")

    args = parser.parse_args()

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    ts = time.strftime("%Y%m%d_%H%M%S")
    raw_path = outdir / f"latency_raw_{ts}.txt"
    csv_path = outdir / f"latency_{ts}.csv"
    png_path = outdir / f"latency_{ts}.png"
    compare_png_path = outdir / f"latency_compare_{ts}.png"

    if args.input:
        input_path = Path(args.input)
        if not input_path.exists():
            print(f"[ERROR] 找不到 input 檔案: {input_path}")
            sys.exit(1)

        print(f"[INFO] 讀取既有輸入: {input_path}")
        with open(input_path, "r", encoding="utf-8", errors="ignore") as input_file:
            lines = [line.rstrip("\r\n") for line in input_file]

        with open(raw_path, "w", encoding="utf-8") as raw_file:
            raw_file.write("\n".join(lines))
            raw_file.write("\n")
    else:
        if not args.port:
            print("[ERROR] 未提供 --port，請指定 COM 埠或改用 --input")
            sys.exit(1)

        lines = collect_from_serial(args.port, args.baud, args.seconds, raw_path)

    parsed = parse_latency_lines(lines)
    if not parsed.rows:
        print("[ERROR] 沒有解析到任何 latency CSV 資料列")
        print("[HINT] 請確認韌體有輸出: seq,systick_ms,load_active,... 格式")
        sys.exit(2)

    df = pd.DataFrame(parsed.rows)
    save_csv(parsed.rows, csv_path)
    summarize(df)
    plot_latency(df, png_path)
    plot_comparison(df, compare_png_path)

    print("\n===== 輸出檔案 =====")
    print(f"raw: {raw_path}")
    print(f"csv: {csv_path}")
    print(f"png: {png_path}")
    print(f"png_compare: {compare_png_path}")
    print(f"line match: {parsed.matched_lines}/{parsed.total_lines}")


if __name__ == "__main__":
    main()
