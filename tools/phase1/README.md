# Phase1: Interrupt Latency Measurement tools

## 安裝

```powershell
pip install -r tools/requirements.txt
```

## 1) 直接從 Serial 擷取並畫圖

```powershell
python tools/phase1/capture_latency.py --port COM5 --baud 115200 --seconds 30
```

預設輸出目錄：`tools/out/phase1/`

## 2) 解析既有 terminal 文字檔並畫圖

```powershell
python tools/phase1/capture_latency.py --input tools/out/phase1/latency_raw_xxx.txt
```

## 比較多份 capture 統計

```powershell
python tools/phase1/analyze_two_runs.py tools/out/phase1/latency_*.csv
```

## 韌體參數（Phase1）

### 開關 Phase1 實驗功能

`EXPERIMENT_PHASE1_ENABLE` 用來控制 Phase1 相關功能是否編入韌體（例如 latency / load task / TIM3 ISR hook）。

- 預設：啟用（1）
- 關閉：在 STM32CubeIDE 專案的編譯選項加入：`-DEXPERIMENT_PHASE1_ENABLE=0`
- 強制啟用：`-DEXPERIMENT_PHASE1_ENABLE=1`

### 調整 load task 的 busy 時間

`LOAD_TASK_BUSY_MS` 用來控制 load task 每次 loop 忙等的時間（單位：ms）。

- 預設：10ms
- 範例：改成 2ms（用於比較不同負載下的 latency）
	- 在 STM32CubeIDE 專案的編譯選項加入：`-DLOAD_TASK_BUSY_MS=2`

## 報告

- Phase1 report: [tools/InterruptLatencyMeasurement_Report.md](../InterruptLatencyMeasurement_Report.md)
