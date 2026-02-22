"""Phase1: helper to compare multiple capture CSV files.

This is the Phase1-scoped version of tools/analyze_two_runs.py.
"""

import argparse
from pathlib import Path

import pandas as pd


def pct(series: pd.Series, q: float) -> float:
    return float(series.quantile(q))


def summarize(path: Path) -> dict:
    d = pd.read_csv(path, comment="#")

    d["seq"] = pd.to_numeric(d.get("seq"), errors="coerce")
    d = d.dropna(subset=["seq"]).copy()

    d["seq"] = d["seq"].astype("int64")
    d["load_active"] = pd.to_numeric(d.get("load_active"), errors="coerce").fillna(0).astype("int64")

    ds = d["seq"].diff().dropna()

    out: dict[str, object] = {
        "rows": int(len(d)),
        "seq_min": int(d["seq"].min()),
        "seq_max": int(d["seq"].max()),
        "seq_neg_diff": int((ds < 0).sum()),
        "gap_gt1_count": int((ds > 1).sum()),
        "gap_gt1_ratio": float((ds > 1).mean()) if len(ds) else 0.0,
        "mean_seq_diff": float(ds.mean()) if len(ds) else float("nan"),
        "median_seq_diff": float(ds.median()) if len(ds) else float("nan"),
    }

    for key, df in [("IDLE", d[d["load_active"] == 0]), ("LOAD", d[d["load_active"] == 1])]:
        out[f"{key}_n"] = int(len(df))

        lat = pd.to_numeric(df.get("latency_us"), errors="coerce").dropna()
        exe = pd.to_numeric(df.get("exec_us"), errors="coerce").dropna()

        if len(lat):
            out[f"{key}_lat_mean"] = float(lat.mean())
            out[f"{key}_lat_p95"] = pct(lat, 0.95)
            out[f"{key}_lat_p99"] = pct(lat, 0.99)
            out[f"{key}_lat_max"] = float(lat.max())

            out[f"{key}_lat_gt_12_ratio"] = float((lat > 12.0).mean())
            out[f"{key}_lat_ge_14_ratio"] = float((lat >= 14.0).mean())
            out[f"{key}_lat_ge_16_ratio"] = float((lat >= 16.0).mean())
            out[f"{key}_lat_ge_20_ratio"] = float((lat >= 20.0).mean())

        if len(exe):
            out[f"{key}_exe_mean"] = float(exe.mean())
            out[f"{key}_exe_p99"] = pct(exe, 0.99)
            out[f"{key}_exe_max"] = float(exe.max())

    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="[Phase1] Compare latency CSV captures")
    ap.add_argument("files", nargs="+", help="CSV files produced by Phase1 capture tool")
    args = ap.parse_args()

    keys = [
        "rows",
        "seq_min",
        "seq_max",
        "seq_neg_diff",
        "gap_gt1_count",
        "gap_gt1_ratio",
        "mean_seq_diff",
        "median_seq_diff",
        "IDLE_n",
        "IDLE_lat_mean",
        "IDLE_lat_p95",
        "IDLE_lat_p99",
        "IDLE_lat_max",
        "IDLE_lat_gt_12_ratio",
        "IDLE_lat_ge_14_ratio",
        "IDLE_lat_ge_16_ratio",
        "IDLE_lat_ge_20_ratio",
        "IDLE_exe_mean",
        "IDLE_exe_p99",
        "IDLE_exe_max",
        "LOAD_n",
        "LOAD_lat_mean",
        "LOAD_lat_p95",
        "LOAD_lat_p99",
        "LOAD_lat_max",
        "LOAD_lat_gt_12_ratio",
        "LOAD_lat_ge_14_ratio",
        "LOAD_lat_ge_16_ratio",
        "LOAD_lat_ge_20_ratio",
        "LOAD_exe_mean",
        "LOAD_exe_p99",
        "LOAD_exe_max",
    ]

    for f in args.files:
        p = Path(f)
        s = summarize(p)
        print(f"=== {p.name} ===")
        for k in keys:
            if k not in s:
                continue
            v = s[k]
            if isinstance(v, float):
                print(f"{k:>16}: {v:.6f}")
            else:
                print(f"{k:>16}: {v}")
        print()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
