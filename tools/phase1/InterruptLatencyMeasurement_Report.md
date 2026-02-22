# Interrupt Latency Measurement (Phase1) 報告

> 專案環境：STM32G0 (STM32G071) + STM32CubeIDE + HAL + FreeRTOS(CMSIS-RTOS v2)

## 1. 這個實驗在做什麼？為什麼重要？

在 real-time / embedded 系統裡，**中斷不是「有觸發就立刻跑」**。
硬體事件（例如 Timer update）發生後，CPU 可能因為：
- 其他更高優先權的 IRQ 正在執行
- 系統暫時 mask 中斷（critical section）
- 軟體路徑（HAL handler / callback）本身的固定開銷

而導致「真正開始執行 ISR」有延遲。

本實驗的目標是把兩個概念分開量測：
- **Interrupt latency**：事件發生到「開始做 ISR 工作」之間的等待時間
- **ISR execution time**：ISR 內部的執行時間（做完工作花多久）

並觀察在不同 CPU load 條件下，latency/exec 是否改變，尤其是 **tail（偶發的最慢情況）**。

---

## 2. 實驗設計總覽

### 2.1 時脈與週期設定（TIM3）

TIM3 目前設定為：
- `Prescaler = 15`
- `ARR(Period) = 999`

對應的含意：
- Timer tick 約為 **1us**
- 每計數 1000 tick 產生一次 Update event → **每 1ms 觸發一次 TIM3 IRQ（約 1kHz）**

程式位置：
- TIM3 初始化：[Core/Src/main.c](../Core/Src/main.c#L577-L613)
- 啟動 TIM3 interrupt：[Core/Src/main.c](../Core/Src/main.c#L320-L335)

### 2.2 IRQ 進入路徑（硬體 → HAL → 量測點）

實際 ISR 路徑如下：

1. TIM3 Update event 產生（UIF 置位、NVIC pending）
2. CPU 進入 `TIM3_IRQHandler()`
3. `HAL_TIM_IRQHandler(&htim3)`
4. HAL 內部呼叫 `HAL_TIM_PeriodElapsedCallback()`
5. 在 callback 內判斷 `TIM3` 後呼叫 `latency_on_tim_period_elapsed_isr()`

程式位置：
- IRQ handler：[Core/Src/stm32g0xx_it.c](../Core/Src/stm32g0xx_it.c#L170-L180)
- callback 路由到 latency 模組：[Core/Src/main.c](../Core/Src/main.c#L860-L880)

> 注意：因此本實驗量到的 latency 包含「NVIC 等待 + HAL handler/callback 固定開銷 + 進入 latency hook 前的軟體路徑」，不是純硬體 vector entry latency。

### 2.3 量測定義（latency/exec）

量測在 [Core/Src/latency.c](../Core/Src/latency.c) 的 `latency_on_tim_period_elapsed_isr()` 完成：

- `latency_ticks = entry_cnt`
  - `entry_cnt` 是在 ISR hook 進來後讀 TIM3 的 CNT
  - Update event 後 CNT 由 0 開始計數，因此 `entry_cnt` 可視為「從事件到量測點經過的 tick」

- `exec_ticks = exit_cnt - entry_cnt`（含 overflow 處理）
  - `exit_cnt` 是在 hook 末尾再次讀 CNT
  - 差值代表 hook 內部執行時間（GPIO toggle、記錄、push ring buffer 等）

相關程式位置：
- latency/exec 計算：[Core/Src/latency.c](../Core/Src/latency.c#L214-L254)

### 2.4 為什麼不用 ISR 直接 printf？（Ring buffer + logging task）

ISR 內不做 UART printf，改用：
- ISR：只做 O(1) 的資料採樣與 push ring buffer
- task：在背景把 ring buffer 的資料 pop 出來，輸出 CSV

原因：UART/printf 非常慢且不確定，會扭曲量測結果並可能造成更多中斷堆積。

Ring buffer 行為：
- 大小 512
- 滿了就覆蓋最舊樣本並累計 `g_overwrite_count`

程式位置：
- ring push/pop + overwrite：[Core/Src/latency.c](../Core/Src/latency.c#L44-L103)

### 2.5 Load 產生（idle/load 切換）

[Core/Src/load_task.c](../Core/Src/load_task.c) 提供 CPU 負載：
- 每 5 秒切換一次 idle/load
- idle：`osDelay(50)`（幾乎不占 CPU）
- load：busy loop（以 RTOS tick 計時）+ `osDelay(1)`

此實驗使用兩種 load 設定進行比較：
- 10ms busy（較重）
- 2ms busy（較輕）

---

## 3. 產出資料格式（CSV）

logging task 會印出 CSV 欄位：

- `seq`：ISR 取樣序號（每次 TIM3 事件 +1）
- `systick_ms`：HAL_GetTick()（ms）
- `load_active`：取樣當下 load task 狀態（0=idle, 1=load）
- `latency_ticks / latency_us`：latency
- `exec_ticks / exec_us`：exec
- `latency_delta_ticks`：與前一筆 latency 的差（用來觀察跳變）

---

## 4. 實測結果（比較 10ms vs 2ms load）

資料檔案（範例）：
- 10ms load： [tools/out/latency_20260221_003936.csv](out/latency_20260221_003936.csv)（舊資料可能在 `tools/out/` 根目錄）
- 2ms load： [tools/out/latency_20260221_012039.csv](out/latency_20260221_012039.csv)

### 4.1 掉樣（輸出跟不上）觀察

兩份資料都顯示非常明顯的「seq 跳號」：
- 10ms：`mean seq diff ≈ 7.746`、`median = 4`、`gap>1 ratio = 100%`
- 2ms：`mean seq diff ≈ 5.877`、`median = 4`、`gap>1 ratio = 100%`

解讀：
- TIM3 仍以 1kHz 取樣，但 UART 115200 無法每秒輸出 1000 行 CSV。
- ring buffer 會被塞滿並覆蓋舊資料，因此「ISR 有量到」但「最後印出來的行」只是一小部分樣本（掉樣）。

### 4.2 Latency（進 ISR 前等待）

**10ms load（003936）**
- IDLE：平均 12.000us，max 12us（幾乎固定）
- LOAD：平均 12.036us，max 21us
  - `lat > 12us` 約 0.438%
  - `lat >= 20us` 約 0.392%

**2ms load（012039）**
- IDLE：平均 12.010us，max 19us
  - `lat > 12us` 約 0.164%
- LOAD：平均 12.037us，P99 14us，max 14us
  - `lat >= 14us` 約 1.833%

重點解讀：
- 平均 latency 幾乎不變（都在 12.0x us），差異主要在 **tail（偶發尖峰）**。
- 10ms load：尖峰較少但偶爾很高（到 21us）
- 2ms load：尖峰較常見但高度較矮（到 14us）

> 重要限制：掉樣會影響 tail 比例的可信度；若 load 狀態下掉樣更嚴重，則可能低估/扭曲 load 的 tail。

### 4.3 Exec（ISR 內部執行時間）

兩份資料 exec 幾乎都固定：
- exec 約 9us（mean/P99/max 幾乎一致）

解讀：
- ISR hook 的工作量穩定。
- latency 尖峰主要來自 ISR 進入前等待（排隊 / mask / 其他 IRQ）。

---

## 5. 如何重現（capture + plot）

### 5.1 串口抓取

使用 Phase1 工具 [tools/phase1/capture_latency.py](phase1/capture_latency.py)：

- 直接抓 COM 口輸出：
  - `python tools/phase1/capture_latency.py --port COM5 --baud 115200 --seconds 200`

- 讀取既有 CSV 再產生圖：
  - `python tools/phase1/capture_latency.py --input tools/out/phase1/latency_20260221_012039.csv`

### 5.2 分析兩份 run 的統計

使用 Phase1 工具 [tools/phase1/analyze_two_runs.py](phase1/analyze_two_runs.py)：
- `python tools/phase1/analyze_two_runs.py tools/out/phase1/latency_20260221_003936.csv tools/out/phase1/latency_20260221_012039.csv`

---

## 6. 已知限制與可優化方向

### 6.1 UART bottleneck（最優先）

目前每 1ms 印一行 CSV，資料量遠大於 115200 baud 可承受，導致掉樣。

建議：
- 改成「抽樣輸出」：例如每 N 筆只印 1 筆 raw
- 或只印統計：每 200/1000 筆印一次 min/max/avg + P95/P99 + overwrite
- 或提高 baud / 改 DMA / 改二進位封包

### 6.2 量測邊界（HAL 路徑開銷）

目前 latency 含 HAL handler/callback 固定成本。
若要更靠近硬體，需把 timestamp 點移得更靠近 IRQ entry（但需小心不要破壞原本工程結構）。

### 6.3 Load 產生方式精準度

load busy loop 以 RTOS tick 當時間基準（ms 粒度）。
若要更精準控制 CPU 佔用，可改用硬體 timer/cycle 基準做 busy 時間。

---

## 7. 結論 !

我用 TIM3(1kHz) 在 FreeRTOS 系統中把 interrupt latency 與 ISR exec time 分離量測，並用可控 CPU load 做 A/B。結果顯示 exec 幾乎固定，但 latency 的 tail 在 load 下會變差；同時也驗證 UART logging 會造成掉樣，理解到 profiling 必須控制觀測開銷，否則結果會被瓶頸扭曲。
