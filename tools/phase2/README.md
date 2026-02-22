# Phase2: Priority Inversion + Mutex tools

本資料夾提供 Phase2 的 host 端工具：

- 抓 UART 原始 log：`capture_uart_log.py`
- 清理/統計/作圖：`phase2_analyze.py`

實驗背景與結果解讀請看 Report：

- Phase2 report: [PriorityInversionMutex_Report.md](PriorityInversionMutex_Report.md)

## 安裝

```powershell
python -m pip install -r tools/phase2/requirements.txt
```

## 1) UART 擷取（Mode1 / Mode2 各跑一次）

先確認你的板子序列埠（例如 Windows 常見 `COM5`）：

```powershell
python tools/phase2/capture_uart_log.py --port COM5 --baud 115200
python tools/phase2/capture_uart_log.py --port COM5 --baud 115200
```

預設行為：

- 會等待 `CFG,` 或 CSV header 才開始寫檔（避免開機雜訊、同時保留 CFG）
- 抓到 `iter==500` 自動停止
- 輸出在 `tools/out/phase2/`，並自動命名 `mode1_*.txt` / `mode2_*.txt`（不覆蓋舊檔）

提示：想讓兩次 capture 的檔名後綴一致，可加 `--run-id`：

```powershell
python tools/phase2/capture_uart_log.py --port COM5 --baud 115200 --run-id demoA
python tools/phase2/capture_uart_log.py --port COM5 --baud 115200 --run-id demoA
```

## 2) 分析（預設挑最新 mode1/mode2）

```powershell
python tools/phase2/phase2_analyze.py
```

輸出會在 `tools/out/phase2/analysis_YYYYmmdd_HHMM__<重要參數後綴>/`：

- `clean_mode1.csv`, `clean_mode2.csv`, `combined.csv`
- `summary.csv`
- `fig_all.png`

## 韌體參數（Phase2）

### 開關 Phase2 實驗功能

- `EXPERIMENT_PHASE2_ENABLE`：在 [Core/Inc/experiments.h](../../Core/Inc/experiments.h)

### 切換 PI 模式（編譯期）

在 [Core/Inc/phase2_pi_config.h](../../Core/Inc/phase2_pi_config.h)：

- `PI_MODE MODE_MUTEX_PI`（mode=1）：mutex + PI
- `PI_MODE MODE_BINARY_NO_PI`（mode=2）：binary semaphore（無 PI）

### 重要可調參數（影響結果）

同樣在 [Core/Inc/phase2_pi_config.h](../../Core/Inc/phase2_pi_config.h)：

- `PHASE2_REALISTIC_PROFILE`
- `LOW_HOLD_MS`, `LOW_HOLD_JITTER_MS`
- `HIGH_START_JITTER_MS`
- `MEDIUM_SPIN_FACTOR_MIN/MAX`, `MEDIUM_PAUSE_TICKS`
- `LOW_CRITICAL_SPIN_STEPS/FACTOR`

