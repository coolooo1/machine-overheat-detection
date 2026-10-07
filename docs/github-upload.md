# GitHub 上傳說明

## 建立 Repository

在 GitHub 建立 Repository，名稱可用 `machine-overheat-detection`，中文名稱為「機器過熱檢測」。Description 可貼上：

> 使用 C17 開發的設備控制模擬專案，實作三態狀態機、過熱保護、故障保持與 CSV 事件紀錄，包含單元測試及整合測試，無需實體硬體即可驗證控制流程。

本機專案已包含 README 及 .gitignore。使用下方 Git 方式上傳時，請建立空的遠端 Repository，避免產生另一份初始提交。

## 使用 Git 上傳

在 PowerShell 切換到本專案根目錄，再執行：

```powershell
git status
git add .
git commit -m "Initial commit: C controller simulation and CSV logging"
git remote add origin https://github.com/YOUR_ACCOUNT/machine-overheat-detection.git
git push -u origin main
```

請將 `YOUR_ACCOUNT` 改成你的 GitHub 帳號。若你是從 ZIP 解壓縮，先執行 `git init -b main`；ZIP 不包含本機 Git 歷史。既有 Repository 日後更新時，使用 `git add`、`git commit` 與 `git push` 即可。

如果 Git 提示缺少提交者資料，可只對這個專案設定：

```powershell
git config user.name "你的名字"
git config user.email "你的 GitHub 提交信箱"
```

設定後重新執行 `git commit`，推送時依 Git 的登入流程完成 GitHub 驗證。

## 使用網頁上傳

將 ZIP 解壓縮後，把專案內的檔案與資料夾上傳到 Repository 根目錄，讓首頁直接顯示 README。不要只上傳 ZIP，也不要把整個專案再包在多一層子資料夾中。

請包含 `.github/workflows/ci.yml`、`.gitignore` 與 `.gitattributes`；部分檔案選擇器可能不顯示以點開頭的名稱。`build/`、執行檔和執行中產生的 `events.csv` 不需要上傳，`examples/recovery.csv` 則是刻意保留的示範資料。

## 上傳後驗證

首頁應顯示中文 README；進入 Actions 檢查第一次編譯與測試結果。工作流程通過後，才表示 GitHub 上的 Windows 環境也驗證成功；本機測試不等於雲端測試已完成。

## 履歷描述範例

> 開發 C17 設備控制模擬工具，設計待機、運轉與故障狀態機，實作過熱保護、故障保持及 CSV 事件紀錄；以 30 項單元檢查與 18 個整合情境驗證控制邊界、錯誤處理與故障復原流程。

此描述對應目前實作，沒有將模擬輸出描述成真實硬體控制。
