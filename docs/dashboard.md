# Dashboard 介面

## 頁面與互動

左側使用原生 NavigationView，包含 Dashboard、Agent Builder、Chat、Images、Audio、Video、Prompts、Logs、Traces、Assistants、Batches、Evaluations、Fine-tuning、Storage、Usage、API keys、Projects 及 Settings。導航類別依 Platform 的工作流程分組；Dashboard、Settings、五個 [Playground 頁面](playground.md) 與 [Manage 的三個頁面](manage.md) 已實作，其餘 Build 頁面顯示獨立標題與「頁面準備中」佔位畫面。

Dashboard 提供專案選擇、7／30 天範圍切換與伺服器資料重新整理。專案欄位預設為「所有專案（組織）」，其他選項由伺服器取得，顯示名稱與 ID，並標示已封存專案。六張卡片會依內容寬度排成 3、2 或 1 欄。載入在背景執行，不阻塞 XAML 執行緒；離開頁面後取消後續讀取並忽略遲到結果。

初次進入或按重新整理會更新專案清單；切換專案或日期範圍只重新讀取統計。專案清單與統計的權限／錯誤獨立處理，清單失敗不阻止原範圍的統計讀取。更新失敗時保留目前選擇與先前清單，提示重新整理；已選 ID 不在最新清單時保留該 ID 並標示，不會自動切換到組織總量。專案選擇只保存在當前頁面，沒有寫入本機設定。

## 統計定義

| 卡片 | 資料來源／定義 | 範圍 |
| --- | --- | --- |
| Completions Token 用量 | Completions Usage 的 `input_tokens + output_tokens`，包含文字、音訊與影像；快取讀取、快取寫入及各模態數量皆不重複加總 | 選取期間 |
| Completions 模型請求數 | `num_model_requests`；是模型請求數，不代表所有 API HTTP 請求或對話數 | 選取期間，每日長條圖 |
| 快取輸入 Token 佔比 | `sum(input_cached_tokens) / sum(input_tokens)`，不是請求命中率、節省成本比例或每日百分比的平均；輸入為 0 或欄位缺失時為「—」 | 選取期間與每日趨勢 |
| 本月成本 | 目前專案範圍、未依成本項目篩選的 Costs API USD 金額加總，範圍不限於 Completions | UTC 本月 1 日至查詢時間 |
| 今日 Completions Token | Completions Usage 今日輸入與輸出合計，取代餘額卡片 | UTC 今日 00:00 至查詢時間 |
| 快取輸入 Token | `input_cached_tokens` 的期間加總，是輸入 Token 的子集；取代原本重複且容易誤解為全部 API 的「請求總數」卡 | 選取期間與每日趨勢 |

選擇「所有專案」時不傳專案篩選，涵蓋 Admin Key 所屬組織；指定專案時，Usage 與 Costs 都傳送相同 `project_ids` 陣列，後續每一頁也保留篩選。所有卡片、圖表、今日用量與本月成本皆套用目前專案，畫面同步顯示統計範圍。未另外設定使用者、模型、API Key 或 Batch 篩選，涵蓋 Batch／非 Batch 的 Completions 用量。畫面採官方端點的 Completions 名稱，不自行將此資料等同於某兩個生成端點的精確集合，也不宣稱包含 Embeddings、Moderations 等其他用量端點。

「近 7／30 天」指 UTC 日曆日，包含尚未結束的今日，不是回溯整整 168／720 小時。頁尾顯示含年份與時間的實際用量查詢邊界，對應 `start_time` 含、`end_time` 不含；本月成本與今日用量使用相同查詢截止時間，但起日獨立。

圖表以實際日桶繪製。輸入與輸出共用包含兩組最大值的尺度；零請求不繪製長條。無輸入或未知的快取比例以斷點呈現，不當作 0% 或跨越缺值連線。已知零快取讀取量仍可顯示 0 Token。

資料由 PlatformApiClient 取得並解析後交給 DashboardViewModel，不存在內建示範數值。UTC 日期在執行時取得，跨月範圍不影響本月至今卡片，帳務資料可能延遲。本機參考預算由使用者在設定頁選擇是否啟用並輸入 USD 金額，預設未設定；它不會設定官方額度或限制請求。參考預算僅對照「所有專案」的組織成本，選取單一專案時隱藏進度並顯示範圍提示，避免將專案成本與組織預算混比。

組織統計使用設定頁保存的 Admin Key；缺少／失效／權限不足時提供說明與「前往設定填寫 Admin Key」按鈕。未取得欄位顯示「—」，伺服器回傳空用量時才顯示 0。用量與成本分別處理，部分端點失敗不抹掉另一端點成功取得的資料；快取欄位不可用時不顯示假的 0%。

官方欄位來源：[Usage API](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage)、[Usage 與 Costs 範例](https://developers.openai.com/cookbook/examples/completions_usage_api)。

## 官方文件比對（2026-11-03）

- [Completions Usage](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage/methods/completions)：核對欄位子集、模態、模型請求數、預設 Batch 範圍、日期邊界與分頁。輸入／輸出加總及原本加權的快取計算保留；修正標示、零分母及缺值呈現。
- [Costs](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage/methods/costs)：保留獨立 UTC 月範圍及 USD 金額加總；明示這是目前專案範圍的所有成本項目，不能從 Completions Token 推算，也不是帳戶餘額或付款紀錄。
- [List projects](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/methods/list)：使用 Admin Key、`include_archived=true`、`limit=100` 與 `after` 分頁，包含已封存專案。Usage／Costs 的 `project_ids` 參數用於伺服器端篩選，不以本機比例推估各專案統計。
- 缺值、沒有用量與沒有權限分別呈現。認證與傳輸行為未改變，未讀取真實權杖或呼叫 OpenAI API。

## 相依套件

本次 UI 使用既有 Windows App SDK 與 WinUI Shapes 繪圖，未新增第三方圖表套件或 vcpkg 相依套件。新增的程式碼沿用專案 `GPL-3.0-only` 標記。

## 驗證

- x64 Debug：MSBuild 編譯、IDL／XAML 產生及資源封裝。
- 已實際啟動檢查深色三欄、淺色雙欄（960 DIP）與淺色單欄（560 DIP）；驗證後恢復跟隨系統主題與預設視窗尺寸。
- 先前已檢查導航與版面；本次 API 更動使用獨立虛擬通道測試，沒有啟動正式 Dashboard 進行真實 API 呼叫。
- 語義回歸測試使用官方欄位形狀的本機資料，涵蓋多模態／快取寫入不重複計數、Token 加權比例、零分母與缺值斷點、輸出高於輸入的圖表尺度及含年份的 UTC 查詢時間。
- 專案測試涵蓋清單分頁、空／未命名／已封存專案、重複與錯誤清單、獨立權限，以及 7／30 天用量／成本每頁一致套用篩選、切回所有專案及各專案加總相符。
- HighContrast 已提供資源定義，尚未切換系統對比主題進行實機驗證。
