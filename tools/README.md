# Latency Capture Tool

## 安裝

```powershell
pip install -r tools/requirements.txt
```

## 1) 直接從 Serial 擷取並畫圖

```powershell
python tools/capture_latency.py --port COM5 --baud 115200 --seconds 30
```

## 2) 解析既有 terminal 文字檔並畫圖

```powershell
python tools/capture_latency.py --input tools/out/raw_terminal_dump.txt
```

輸出會放在 `tools/out/`：
- `latency_raw_*.txt`
- `latency_*.csv`
- `latency_*.png`
