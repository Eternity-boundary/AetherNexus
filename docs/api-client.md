# 伺服器資料取得

本文件說明 Dashboard 與模型清單的唯讀資料通道。Playground 的生成 POST／Realtime WebSocket 使用獨立的 WindowsGenerationTransport，詳見 [playground.md](playground.md)。

## 正式程式

`src/AetherNexus` 僅包含正式程式碼。Dashboard 啟動、切換專案／7／30 天範圍或重新整理時，會在背景透過 PlatformApiClient 與 WindowsHttpTransport 取得資料。沒有虛擬資料、測試開關或伺服器失敗時的假值備援。

| 端點 | 用途 | 權杖 |
| --- | --- | --- |
| `GET /v1/organization/usage/completions` | Completions 輸入／輸出 Token、快取讀取量與佔比、模型請求數（含文字、音訊與影像 Token） | Admin Key |
| `GET /v1/organization/costs` | UTC 本月至今的 USD 成本 | Admin Key |
| `GET /v1/organization/projects` | 專案清單（含已封存專案） | Admin Key |
| `GET /v1/models` | Playground 可用模型清單 | 一般 API Key |

權杖沿用設定頁的 Windows 認證管理員儲存，僅在真正請求時讀取對應槽，不從環境變數回退，也不將 Admin／一般金鑰互相替代。一般權杖無法提供完整組織統計，因此缺少 Admin Key 時會保留空狀態、顯示設定說明與導覽按鈕。

WindowsHttpTransport 僅接受端點列舉與結構化查詢，固定連至 `https://api.openai.com:443`。只使用無 body 的 GET；不接受任意主機或傳送提示詞／檔案。保留 Windows TLS 憑證驗證，關閉自動重新導向、Cookie 與自動驗證。Authorization 暫存緩衝區在送出後清除；憑證讀回緩衝區亦會清除。錯誤回應 body 不保留或顯示，UI 僅使用固定、在地化的錯誤訊息。

## 資料與生命週期

- 使用 UTC 日桶。Usage 查詢包含選定日數與今日；Costs 獨立查詢本月，不隨 7／30 天改變。
- 明確設定 `limit=31`，逐頁讀取 `has_more`／`next_page`。游標以 UTF-8 百分比編碼；拒絕重複游標、重複日桶及超過 64 頁的回應。
- 專案清單使用獨立的 `after` 分頁（最後一個專案 ID）、`limit=100`、`include_archived=true`；最多 64 頁，拒絕重複 ID、無 ID、錯誤結構與空頁卻宣稱仍有下一頁的回應。名稱缺少或為 null 時由 UI 顯示 ID，不製造專案名稱。
- 驗證 JSON 結構、整數計數、加總溢位、快取數量、幣別與金額。輸入 Token 已含快取讀取、快取寫入及各模態 Token，不重複相加。快取比例為期間 Token 加權值；輸入為零或缺少快取欄位時顯示不可用，不推斷為 0%。每個日桶保留快取欄位的可用狀態，讓圖表在缺值處斷開。
- 正常空結果產生 0 用量；認證、權限、網路或格式錯誤則顯示「—」，不混淆為沒有使用量。Usage 與 Costs 的成功／失敗獨立處理。
- 設定每階段 WinHTTP 逾時與回應大小上限。取消會阻止後續頁面／讀取；目前正在阻塞的 WinHTTP 呼叫仍依系統逾時結束。離開頁面或新查詢後，舊結果不更新畫面。不自動無限重試 429 或伺服器錯誤。
- 本機每月參考預算由使用者在設定頁保存，預設沒有金額。僅在「所有專案」且設定後顯示組織成本／預算與進度，不代表官方限額。
- 預設不傳專案篩選；選取單一專案時，Usage／Costs 均以 `project_ids[]` 傳送同一個 ID，百分比編碼後的參數名稱為 `project_ids%5B%5D`，分頁複製原始請求以保留篩選。此參數不能用於模型或專案清單端點。不使用 `OpenAI-Project` 標頭取代統計端點的查詢篩選。
- 不另外套用使用者、API Key、模型或 Batch 篩選；用量僅涵蓋 Completions，成本則為目前專案範圍所有成本項目。專案清單失敗不改變選擇或回退到組織統計。完整畫面語義與官方比對紀錄見 [dashboard.md](dashboard.md)。

## 測試隔離

所有虛擬呼叫、假 JSON、測試情境、原生測試專案及執行腳本均位於 `tests/`。API 測試執行檔不連結 WindowsHttpTransport 或 CredentialStore，因此不會開啟網路連線、讀取真實金鑰或將資料送到 OpenAI。

```powershell
pwsh -NoProfile -File ./tests/Test-PlatformApi.ps1
pwsh -NoProfile -File ./tests/Test-SettingsStorage.ps1
```

先通過虛擬 API 合約測試，再加入正式 HTTPS 實作。涵蓋 UTC 日期與月界、7／30 天分頁、月末成本分頁、加總、空資料、401／403／429／503、逾時、格式錯誤、游標重複、無效計數、快取不可用、非 USD 幣別、取消及預算金額邊界。

Dashboard 語義測試亦編譯正式 DashboardViewModel，核對多模態／快取寫入子集不重複計數、請求數與 Token 不混用、加權快取比例、零分母、日桶缺值、輸入／輸出共用圖表尺度及 UTC 年月日時間格式。

專案測試涵蓋清單分頁與空資料、null 名稱、已封存狀態、格式與重複 ID、獨立權限；並檢查 Usage／Costs 的每一頁保留相同專案、查詢值編碼、7／30 天專案統計加總與切回組織範圍。

本次沒有執行真實 OpenAI 請求；正式 HTTPS 通道已編譯驗證，未宣稱已完成實際帳戶連線驗證。正式啟動程式後才會使用設定頁的權杖讀取伺服器。

官方合約：[Usage Completions](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage/methods/completions)、[Costs](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage/methods/costs)、[List projects](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/methods/list)、[List models](https://developers.openai.com/api/reference/resources/models/methods/list)。
