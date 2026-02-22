# BareMetal-Lab 

STM32G071（STM32G071RBTx）上的 FreeRTOS / HAL 練習專案，包含：

- UART console + 指令處理（文字指令 + 二進位封包指令）
- UART1/UART3 DMA circular + IDLE 偵測 + RingBuffer + streaming packet parser
- Watchdog（IWDG）初始化/刷新/重置原因檢查 + 故意死鎖測試
- **Phase1**：Interrupt latency / ISR execution time 量測（含可調 CPU load）
- **Phase2**：Priority inversion 對照（Mutex + PI vs Binary semaphore 無 PI）+ host 端分析工具

> 專案環境：STM32CubeIDE + CubeMX 產生碼 + STM32 HAL + FreeRTOS（CMSIS-RTOS v2 API）。

---

## 1) 快速開始（Firmware）

1. 用 STM32CubeIDE 開啟此專案（或開 `yc_stm32_practice.ioc`）。
2. Build / Flash 到板子。
3. 開啟序列埠終端機：**115200 / 8N1**。

UART 角色（目前程式配置）：

- **USART2**：console（`print()` 輸出、文字指令輸入）
- **USART1 / USART3**：DMA RX（circular）+ IDLE callback，用於 loopback/封包解析

> 提醒：實際腳位/接線請以 `.ioc` / CubeIDE Pinout 為準。

---

## 2) 已知功能（Firmware）

### 2.1 Console / 指令（USART2）

- `console_init()` + `print()`：以 HAL blocking TX 輸出字串。
- `HAL_UART_RxCpltCallback()`：USART2 以 DMA 每次收 1 byte，累積後交由 `process_cmd()` 解析。
- 指令不分大小寫（會先轉成大寫）。

支援的文字指令（以空白分隔參數）：

- `LED_ON`
- `LED_OFF`
- `SET_LED 1`
- `UART_TX <text>`
- `PWM_ON <Duty(0~100)> <Freq(Hz)>`
- `CRASH`：故意進入死鎖，驗證 watchdog reset

控制鍵：

- `Ctrl-C`：清空目前輸入 buffer

### 2.2 Binary 封包指令（Packet Protocol）

封包格式（byte stream）：

- `HEADER(0xAA) | LENGTH | CMD | PAYLOAD... | CHECKSUM`
- `CHECKSUM` = 前面所有 bytes 的 8-bit sum

Streaming parser：

- 透過 `PacketParser` 狀態機逐 byte 解析
- `parse_packet()` 通過後，呼叫 `handle_binary_cmd()` 把 payload[0] 當 cmd_id 執行對應 handler

### 2.3 UART1/UART3：DMA circular + IDLE + RingBuffer

- `HAL_UART_Receive_DMA()` 以 circular buffer 持續接收
- IDLE 中斷時計算 DMA 當前寫入位置，把「新增的 bytes」推進 ring buffer
- 在（IDLE callback / main loop）把 ring buffer 資料餵給 streaming packet parser

### 2.4 Watchdog（IWDG）

- `System_Check_Reset_Reason()`：開機時檢查是否由 IWDG reset，並輸出警告
- `Watchdog_Refresh()`：在 main loop / defaultTask / Phase2 高優先任務中定期刷新
- `System_Simulate_Deadlock()`：關中斷後無限迴圈，讓 IWDG 重啟系統

---

## 3) Experiments

本專案把兩個實驗分成 Phase1 / Phase2；並且透過編譯期開關避免 UART 輸出互相干擾。

### 3.1 Phase1：Interrupt Latency Measurement

目的：量測

- interrupt event → 進入量測點的延遲（latency）
- ISR hook 內部的執行時間（exec）

設計重點（簡述）：

- TIM3 週期性觸發（約 1kHz）
- ISR 只做快速採樣並 push ring（避免 ISR printf 破壞結果）
- logging task 在背景輸出 CSV
- 另有 `load_task` 週期性切換 idle/load 來改變 CPU 負載

相關文件/工具：

- 報告：`tools/phase1/InterruptLatencyMeasurement_Report.md`
- 工具說明：`tools/phase1/README.md`

常用參數：

- `EXPERIMENT_PHASE1_ENABLE`：Phase1 enable（可用編譯選項 `-DEXPERIMENT_PHASE1_ENABLE=0/1` 覆寫）
- `LOAD_TASK_BUSY_MS`：調整 load task busy 時間（ms）

### 3.2 Phase2：Priority Inversion + Mutex Behavior

目的：對照「是否有 Priority Inheritance (PI)」對 high priority task 等鎖時間的影響。

- Mode1：FreeRTOS Mutex（有 PI）
- Mode2：Binary semaphore 當 mutex 用（無 PI）

輸出：韌體會印出 **嚴格 CSV**（便於 host 端解析/作圖），並先輸出一行 `CFG,...` 記錄本次參數。

相關文件/工具：

- 報告：`tools/phase2/PriorityInversionMutex_Report.md`
- 工具說明：`tools/phase2/README.md`

重要參數位置：

- `EXPERIMENT_PHASE2_ENABLE`：在 `Core/Inc/experiments.h`
- `PI_MODE` 與各種 knob：在 `Core/Inc/phase2_pi_config.h`

> 注意：當 `EXPERIMENT_PHASE2_ENABLE != 0` 時，程式會**強制關閉 Phase1**（避免 UART prints interleaving，讓 Phase2 CSV 更乾淨）。

---

## 4) Host tools（Python）

- Phase1 工具：`tools/phase1/`
- Phase2 工具：`tools/phase2/`

請直接參考各 phase 目錄下的 README：

- `tools/phase1/README.md`
- `tools/phase2/README.md`

---

## 5) 專案結構（簡述）

- `Core/Inc`, `Core/Src`：主要韌體程式
  - `console.*`：`print()` / UART console
  - `cmd.*`：文字指令 + binary cmd handler
  - `packet.*`：封包格式 + streaming parser
  - `uart_rb.*`：ring buffer
  - `watchdog.*`：IWDG 工具
  - `latency.*`, `load_task.*`：Phase1
  - `phase2_pi.*`, `phase2_pi_config.h`：Phase2
- `tools/`：PC 端擷取/分析腳本與報告

---

## 6) 小提醒 / 已知限制

- `console.c` 目前使用 blocking UART transmit；大量輸出時會佔 CPU 時間。
- Phase1/Phase2 都會產生大量 UART log；建議一次只跑一個 phase。
- UART1/UART3 的 loopback/封包解析需要對應的線路連接與外部資料來源。
