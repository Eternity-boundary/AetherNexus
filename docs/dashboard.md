# Dashboard 介面

## 頁面與互動

左側使用原生 NavigationView，包含 Dashboard、Agent Builder、Chat、Images、Audio、Video、Prompts、Logs、Traces、Assistants、Batches、Evaluations、Fine-tuning、Storage、Usage、API keys、Projects 及 Settings。導航類別依 Platform 的工作流程分組，除 Dashboard 與 Settings 外均顯示獨立標題與「頁面準備中」佔位畫面。

Dashboard 提供 7／30 天範圍切換與伺服器資料重新整理。六張卡片會依內容寬度排成 3、2 或 1 欄。載入在背景執行，不阻塞 XAML 執行緒；離開頁面後取消後續讀取並忽略遲到結果。

## 統計定義

| 卡片 | 資料來源／定義 | 範圍 |
| --- | --- | --- |
| Token 總用量 | `input_tokens + output_tokens`，不再額外加上 cached tokens | 選取期間 |
| Responses & Chat Completions | `num_model_requests` | 選取期間，每日長條圖 |
| 提示詞快取比例 | `input_cached_tokens / input_tokens`，不是請求命中率；輸入為 0 時為 0 | 選取期間與每日趨勢 |
| 本月支出 | Costs API 的 USD 金額加總 | UTC 本月至今 |
| 今日 Token 用量 | 今日輸入與輸出 Token 加總，取代餘額卡片 | UTC 今日 |
| 請求總數 | 目前僅涵蓋文字模型，與 Responses / Chat Completions 相同統計範圍 | 選取期間 |

資料由 PlatformApiClient 取得並解析後交給 DashboardViewModel，不存在內建示範數值。UTC 日期在執行時取得，跨月範圍不影響本月至今卡片，帳務資料可能延遲。本機參考預算由使用者在設定頁選擇是否啟用並輸入 USD 金額，預設未設定；它不會設定官方額度或限制請求。

組織統計使用設定頁保存的 Admin Key；缺少／失效／權限不足時提供說明與「前往設定填寫 Admin Key」按鈕。未取得欄位顯示「—」，伺服器回傳空用量時才顯示 0。用量與成本分別處理，部分端點失敗不抹掉另一端點成功取得的資料；快取欄位不可用時不顯示假的 0%。

官方欄位來源：[Usage API](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage)、[Usage 與 Costs 範例](https://developers.openai.com/cookbook/examples/completions_usage_api)。

## 相依套件

本次 UI 使用既有 Windows App SDK 與 WinUI Shapes 繪圖，未新增第三方圖表套件或 vcpkg 相依套件。新增的程式碼沿用專案 `GPL-3.0-only` 標記。

## 驗證

- x64 Debug：MSBuild 編譯、IDL／XAML 產生及資源封裝。
- 已實際啟動檢查深色三欄、淺色雙欄（960 DIP）與淺色單欄（560 DIP）；驗證後恢復跟隨系統主題與預設視窗尺寸。
- 先前已檢查導航與版面；本次 API 更動使用獨立虛擬通道測試，沒有啟動正式 Dashboard 進行真實 API 呼叫。
- HighContrast 已提供資源定義，尚未切換系統對比主題進行實機驗證。
