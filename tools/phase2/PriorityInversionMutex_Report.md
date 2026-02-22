# Priority Inversion + Mutex Behavior (Phase2) 報告

> 專案環境：STM32G0 (STM32G071) + STM32CubeIDE + HAL + FreeRTOS(CMSIS-RTOS v2)

## 1. 這個實驗在做什麼？為什麼重要？

在有「固定優先權搶佔式排程」的 RTOS 裡，**Priority Inversion（優先權反轉）**是一個典型但又很容易被忽略的延遲來源。

經典條件如下：

- **Low priority** task 先拿到 mutex（進入 critical section）
- **High priority** task 之後需要同一把 mutex 而被阻塞
- **Medium priority** task 不需要 mutex，但持續搶 CPU

這會造成 High task「看起來像是被 Medium 卡住」，但根因其實是 Low 持鎖沒機會跑來釋放。此現象會讓 High 的 worst-case latency（tail）變得很糟，導致 deadline miss。

為了緩解此問題，多數 RTOS 的 mutex 提供 **Priority Inheritance (PI)**：當 High 因 mutex 被阻塞時，持鎖的 Low 會暫時被提高優先權，以便盡快釋放 mutex。

本 Phase2 目的就是 A/B 比較：

- **Mode1 (mode=1)**：FreeRTOS Mutex（有 PI）
- **Mode2 (mode=2)**：Binary Semaphore 當 mutex 用（無 PI）

並量化 High task 的等待時間分佈差異。

---

## 2. 實驗設計總覽

### 2.0 程式位置（快速索引）

- Phase2 任務/同步原語/輸出： [Core/Src/phase2_pi.c](../../Core/Src/phase2_pi.c)
- Phase2 可調參數（PI mode / realistic profile）： [Core/Inc/phase2_pi_config.h](../../Core/Inc/phase2_pi_config.h)
- Phase2 enable 開關： [Core/Inc/experiments.h](../../Core/Inc/experiments.h)
- tick rate（1 tick=1ms）： [Core/Inc/FreeRTOSConfig.h](../../Core/Inc/FreeRTOSConfig.h)
- UART log 擷取： [capture_uart_log.py](capture_uart_log.py)
- 分析/統計/作圖： [phase2_analyze.py](phase2_analyze.py)

### 2.1 任務角色（Low / Medium / High）

Phase2 建立三個 task（概念角色）：

- **Low**：先取得共享鎖，持鎖一段時間後釋放（模擬「低優先權但握有關鍵資源」）
- **High**：嘗試取得同一把鎖，並量測「從嘗試到成功取得」的等待時間（本實驗主要觀察對象）
- **Medium**：不碰鎖，但做 CPU busy-loop（模擬「中優先權背景工作」對 Low 的干擾）

模式切換（編譯期）與可調參數集中在：

- 韌體模式/參數：[Core/Inc/phase2_pi_config.h](../../Core/Inc/phase2_pi_config.h)

### 2.2 兩種同步原語（Mode1 vs Mode2）

共享資源使用同一個 `SemaphoreHandle_t`：

- Mode1：`xSemaphoreCreateMutex()`（Priority Inheritance 生效）
- Mode2：`xSemaphoreCreateBinary()`（無 PI；用 semaphore 模擬 mutex）

這個對照可以把差異集中到「是否有 PI」對 High wait 的影響。

### 2.3 時間刻度（tick）

本專案 FreeRTOS tick rate：

- `configTICK_RATE_HZ = 1000` → **1 tick = 1ms**
- 設定位置：[Core/Inc/FreeRTOSConfig.h](../../Core/Inc/FreeRTOSConfig.h)

因此 Phase2 的等待時間 `high_wait_ticks` 可直接視為 ms 等級的等待（解析/比較時更直觀）。

---

## 3. 產出資料格式（UART log / CSV）

韌體會輸出「嚴格 CSV」資料列（方便 host 端工具穩定解析）：

CSV header：

`iter,mode,high_wait_ticks,low_hold_ticks,medium_spin_count`

每回合輸出一行資料列（共 `ITERATION_COUNT` 行）。欄位含義：

- `iter`：回合序號
- `mode`：1=mutex(PI), 2=binary(no PI)
- `high_wait_ticks`：High 等鎖時間（tick）
- `low_hold_ticks`：Low 實際持鎖時間（tick）
- `medium_spin_count`：Medium busy-loop 次數（代表 CPU 干擾程度）

另外還會出現兩種「非 CSV row」行：

- `CFG,key=value,...`：本次 run 的重要可調參數（用於追蹤與產生 analysis 資料夾後綴）
- `STATS,...`：每 `STATS_WINDOW` 回合印一次摘要（解析時會忽略）

Host 端工具會忽略非資料列，只抓 header 與 data rows。

---

## 4. 實測結果（範例 run：analysis_20260222_2234...）

本段引用分析輸出：

- 輸出資料夾：`tools/out/phase2/analysis_20260222_2234__r1_hold50_j5_hj3_med8000-14000_pause20_crit0/`
- 統計檔：`summary.csv`

本次 run 的重要參數（由 `CFG,...` 生成後綴）：

- `r1`：realistic profile = 1（每回合有 jitter/變動）
- `hold50`：Low 平均持鎖 50 ticks
- `j5`：Low 持鎖抖動 5 ticks
- `hj3`：High start jitter 3 ticks
- `med8000-14000`：Medium busy-loop 強度範圍
- `pause20`：Medium pause ticks
- `crit0`：Low critical extra work steps = 0

### 4.1 High wait（核心觀察）

`high_wait_ticks` 統計如下（n=500）：

- **Mode1 (mutex + PI)**
  - mean = **50.222** ticks
  - std = **3.139**
  - min / p50 / max = **45 / 50.0 / 56**

- **Mode2 (binary, no PI)**
  - mean = **89.204** ticks
  - std = **15.928**
  - min / p50 / max = **68 / 84.5 / 225**

解讀：

- Mode1 的 mean 接近 Low hold（約 50 ticks），且 max 只到 56，代表 High 被擋住時 Low 能相對快地釋放鎖（PI 發揮作用）。
- Mode2 的 mean 顯著增加，max 出現 225 ticks 的長尾，代表 High 等鎖時 Low 可能被 Medium 長時間壓住而無法釋放，Priority Inversion 變得嚴重。

### 4.2 Overhead（high_wait - low_hold）

分析工具另外計算：

- `overhead = high_wait_ticks - low_hold_ticks`

用來觀察 High 等待是否「比 Low 持鎖多很多」。本次 run：

- Mode1：overhead_mean = -0.048、overhead_max = 0（幾乎完全貼合 Low 持鎖時間）
- Mode2：overhead_mean = 0.748、overhead_max = 85（偶發多等很多 tick，反映 PI 缺失時的排程干擾）

---

## 5. 如何重現（capture + analyze）

### 5.1 韌體端

1. 啟用 Phase2：在 [Core/Inc/experiments.h](../../Core/Inc/experiments.h) 設 `EXPERIMENT_PHASE2_ENABLE = 1`
2. 編譯並燒錄 **Mode1**（`PI_MODE MODE_MUTEX_PI`）跑一次
3. 編譯並燒錄 **Mode2**（`PI_MODE MODE_BINARY_NO_PI`）再跑一次

### 5.2 Host 端（抓 UART log）

使用工具：[capture_uart_log.py](capture_uart_log.py)

- Mode1 / Mode2 分別 capture 一次：

```bash
python capture_uart_log.py --port COM5 --baud 115200
python capture_uart_log.py --port COM5 --baud 115200
```

說明：

- 預設會等待 `CFG,` 或 CSV header 才開始寫檔（避免把開機雜訊全部存進去，同時確保 CFG 不會漏掉）
- 抓到 `iter==500` 會自動停止
- 輸出預設在 `tools/out/phase2/`

### 5.3 產生分析結果

使用工具：[phase2_analyze.py](phase2_analyze.py)

```bash
python phase2_analyze.py
```

- 會自動挑最新的 `mode1*.txt` 與 `mode2*.txt`
- 產出 `analysis_YYYYmmdd_HHMM__<重要參數後綴>/`，內含 `summary.csv` 與 `fig_all.png`

---

## 6. 已知限制與可優化方向

- **tick 解析度**：目前以 1ms tick 量測；若要更細粒度（例如 us），需改用 cycle counter / timer timestamp。
- **UART 觀測開銷**：雖然 Phase2 只輸出 500 行，不像 Phase1 那樣有明顯帶寬瓶頸，但 UART 仍是同步阻塞輸出，過多的 printf 仍可能干擾排程與 watchdog。
- **單次 run 不足以代表 worst-case**：Priority inversion 的長尾通常需要更多次/更多組參數才能可靠估計（建議多跑幾次、觀察 max/P99/P999）。

---

## 7. 結論

在相同的 Low/Medium/High 競爭模型下：

- **有 PI 的 mutex（Mode1）** 能讓 High 的等待時間緊貼 Low 的持鎖時間，分佈集中、尾巴短。
- **無 PI 的同步原語（Mode2）** 會放大 Priority Inversion，讓 High 等待時間變長且更容易出現長尾。

這說明在 real-time 系統中，對共享資源應優先使用具 PI 的 mutex，並避免用 binary semaphore 直接替代 mutex 來保護臨界區。
