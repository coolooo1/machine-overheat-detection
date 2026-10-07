# 機器過熱檢測｜C 語言設備控制與事件紀錄模擬

機器過熱檢測（MachineGuard）是在 Windows 電腦上執行的 C17 專案，模擬設備的啟動、停止、過熱保護與故障復原，並以 CSV 保存事件。透過鍵盤注入溫度變化，不需要感測器、開發板或馬達，就能重現故障並測試控制規則。

本專案適合作為 C 語言與韌體控制邏輯的作品展示：重點是狀態機、模組分工、檔案 I/O、錯誤處理和可重現的自動測試。所有設備輸出都是模擬資料，80°C 是示範門檻。

## 功能

- 三種設備狀態：待機 `IDLE`、運轉 `RUNNING`、故障 `FAULT`。
- 溫度達到 80°C 即進入故障，模擬馬達輸出關閉。
- 故障會保持：降溫或停止不會清除故障，必須降溫後手動重置。
- 重置只回到待機，須另行啟動才會運轉。
- CSV 保存操作前後狀態、溫度、馬達輸出與被拒絕的操作原因。
- 追加既有紀錄；檢查欄位名稱及結尾換行，避免誤寫不相容檔案。
- 紀錄無法開啟或初始寫入失敗時，不進入互動流程；運轉期間紀錄失敗時停止模擬馬達。
- 30 項單元檢查與 18 個整合情境，另附 GitHub Actions 設定。

## 快速開始：Windows

需要 GCC（例如 MSYS2 UCRT64）及 PowerShell 5.1 或更新版本。執行程式不需要第三方 C 函式庫。先在 PowerShell 切換到此 README 所在的專案根目錄。

```powershell
gcc --version
& .\scripts\build.ps1
.\build\machineguard.exe
```

若 GCC 未加入 PATH，可以指定你電腦上的安裝位置：

```powershell
& .\scripts\build.ps1 -Compiler 'C:\msys64\ucrt64\bin\gcc.exe'
```

若 PowerShell 顯示「禁止執行指令碼」，可用只影響這次程序的方式執行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

也可以不用腳本，直接編譯：

```powershell
New-Item -ItemType Directory -Path build -Force | Out-Null
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -g -I include src/main.c src/controller.c src/logger.c -o build/machineguard.exe
.\build\machineguard.exe
```

編譯成功後再執行；重新編譯前請先在舊程式中輸入 `0` 結束。

## 指令

輸入一個字元後按 Enter：

| 指令 | 意義 |
|---|---|
| `1` | 啟動：待機且溫度正常才允許運轉 |
| `2` | 模擬過熱：溫度設為 85°C |
| `3` | 模擬降溫：溫度設為 40°C |
| `4` | 重置：故障且低於 80°C 才回到待機 |
| `5` | 查詢目前狀態，也會記錄查詢事件 |
| `6` | 停止：運轉回到待機，既有故障保持 |
| `0` | 停止模擬運轉並結束 |

空白和換行會被略過；如果一次輸入 `12`，會依序執行 `1`、`2`。未知字元不改變設備資料，會留下 `UNKNOWN_COMMAND` 紀錄。

## 一分鐘展示流程

依序輸入 `1 → 2 → 4 → 3 → 4 → 1 → 6 → 0`，每次按 Enter：

| 操作 | 結果 | 馬達 |
|---|---|---|
| `1` 啟動 | `IDLE → RUNNING`，30°C | 開 |
| `2` 過熱 | `RUNNING → FAULT`，85°C | 關 |
| `4` 嘗試重置 | 仍過熱，拒絕 | 關 |
| `3` 降溫 | 40°C，但故障仍保持 | 關 |
| `4` 重置 | `FAULT → IDLE` | 關 |
| `1` 再次啟動 | `IDLE → RUNNING` | 開 |
| `6` 停止 | `RUNNING → IDLE` | 關 |
| `0` 結束 | 寫下 `QUIT`，關閉紀錄檔案 | 關 |

這個流程可以展示：偵測過熱、保持故障、拒絕不合條件的操作，以及由使用者明確復原。

## CSV 紀錄

`events.csv` 寫在**啟動程式時的工作資料夾**。從專案根目錄執行上述指令，就會寫在專案根目錄，而非 `build/`。程式結束後可開啟它，或執行：

```powershell
Get-Content .\events.csv
```

```csv
sequence,event,previous_state,current_state,temperature_c,motor_on
1,SESSION_START,IDLE,IDLE,30,0
2,START,IDLE,RUNNING,30,1
3,OVERHEAT,RUNNING,FAULT,85,0
4,RESET_REJECTED_HOT,FAULT,FAULT,85,0
5,COOL,FAULT,FAULT,40,0
```

| 欄位 | 中文說明 |
|---|---|
| `sequence` | 單次執行的事件序號，由 1 開始 |
| `event` | 事件名稱或拒絕原因 |
| `previous_state` | 操作前狀態 |
| `current_state` | 操作後狀態 |
| `temperature_c` | 操作後的攝氏溫度 |
| `motor_on` | 操作後模擬馬達輸出，1 開、0 關 |

再次執行會追加紀錄，序號重新由 1 開始，以 `SESSION_START` 區分執行。每次程式啟動的狀態仍是待機、30°C，不會從 CSV 恢復狀態。序號不是時間戳記。

完整示範檔案：[examples/recovery.csv](examples/recovery.csv)。此範例包含額外的拒絕啟動、查詢及未知指令，共 12 筆事件。

## 架構

```text
使用者輸入
    ↓
main.c：保存資料、接收指令、串接模組、處理錯誤
    ├── controller.c：回傳新狀態，判斷模擬馬達輸出
    └── logger.c：檢查檔案、寫入 CSV
```

```text
machineguard-github/
├── src/                 主程式與控制、紀錄模組
├── include/             對外型別與函式宣告
├── tests/               單元測試、整合測試與失敗替身
├── scripts/build.ps1    一鍵編譯與測試
├── examples/recovery.csv
├── docs/architecture.md 設計與控制規則
├── docs/verification.md 本次驗證結果及範圍
├── docs/github-upload.md GitHub 上傳步驟與中文簡介
└── .github/workflows/ci.yml
```

程式名稱、識別字及事件採英文；README、設計文件與主要註解採中文。詳細設計請看 [架構說明](docs/architecture.md)。

## 執行全部測試

在專案根目錄執行：

```powershell
& .\scripts\build.ps1 -Test
```

此命令以 `-Wall -Wextra -Wpedantic -Werror` 編譯，將警告視為錯誤，並執行：

1. 19 項控制規則檢查：包含 79°C／80°C 邊界、故障保持、重置與停止。
2. 11 項紀錄檢查：包含 CSV 內容、非法事件名稱、空參數與錯誤狀態。
3. 18 個整合情境：包含完整復原、追加檔案、EOF、未知指令、格式保護及注入寫入失敗。

測試在 `build/integration-<隨機識別碼>/` 中使用獨立檔案，不會覆寫使用者的 `events.csv`。失敗替身僅連結到 `machineguard_failure`，正式程式不使用它。詳細結果請看 [驗證報告](docs/verification.md)。

GitHub Actions 會在推送或提交 Pull Request 時，於 Windows 環境安裝 MSYS2 UCRT64 GCC 並執行相同測試腳本。雲端執行結果請在上傳後查看 Actions。[MSYS2 官方設定工具](https://github.com/msys2/setup-msys2)。

## 目前範圍與限制

- 溫度由指令設定；沒有真實感測器、週期取樣或逾時偵測。
- 馬達輸出由狀態推導；沒有真實硬體控制或實際馬達回饋。
- CSV 使用單一寫入程序，尚未處理多個程序同時寫同一檔案。
- 既有檔案只檢查標頭及結尾換行，沒有逐列驗證所有歷史資料。
- `fflush()` 會送出 C 的緩衝資料，但不保證突然斷電時資料已持久寫入儲存裝置。
- 寫入失敗可能留下部分文字；不提供交易回復或自動修復。若下次啟動拒絕檔案，請先保存原檔再檢查。
- 使用 `fseek()`／`ftell()`；目前以小型本機紀錄檔為目標，未驗證超大型檔案。

## 後續方向

- 加入可由測試控制的虛擬時間，驗證感測器訊號逾時。
- 由獨立模擬器提供感測資料，將使用者介面與資料來源分開。
- 加入事件時間、更多故障原因與離線紀錄分析。

## 放到 GitHub

Repository 名稱：`machine-overheat-detection`；中文專案名稱為「機器過熱檢測」。

可貼入 GitHub Description 的中文簡介：

> 使用 C17 開發的設備控制模擬專案，實作三態狀態機、過熱保護、故障保持與 CSV 事件紀錄，包含單元測試及整合測試，無需實體硬體即可驗證控制流程。

上傳方法與第一次提交的指令請看 [GitHub 上傳說明](docs/github-upload.md)。
