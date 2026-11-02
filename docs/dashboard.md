# Dashboard 介面

## 頁面與互動

左側使用原生 NavigationView，包含 Dashboard、Agent Builder、Chat、Images、Audio、Video、Prompts、Logs、Traces、Assistants、Batches、Evaluations、Fine-tuning、Storage、Usage、API keys、Projects 及 Settings。導航類別依 Platform 的工作流程分組，除 Dashboard 與 Settings 外均顯示獨立標題與「頁面準備中」佔位畫面。

Dashboard 提供 7／30 天範圍切換與示範資料重新整理。六張卡片會依內容寬度排成 3、2 或 1 欄；圖表在重排後重新計算座標。Mica、系統字體、原生焦點／鍵盤操作與 ThemeResource 統一介面風格。圖表色彩包含 Light、Dark、HighContrast 資源。

## 統計定義

| 卡片 | 未來資料來源／定義 | 範圍 |
| --- | --- | --- |
| Token 總用量 | `input_tokens + output_tokens`，不再額外加上 cached tokens | 選取期間 |
| Responses & Chat Completions | `num_model_requests` | 選取期間，每日長條圖 |
| 提示詞快取比例 | `input_cached_tokens / input_tokens`，不是請求命中率；輸入為 0 時為 0 | 選取期間與每日趨勢 |
| 本月支出 | Costs API 的 USD 金額加總 | UTC 本月至今 |
| 今日 Token 用量 | 今日輸入與輸出 Token 加總，取代餘額卡片 | UTC 今日 |
| 請求總數 | 目前僅涵蓋文字模型，與 Responses / Chat Completions 相同統計範圍 | 選取期間 |

示範資料由 DashboardViewModel 產生，與真實帳戶、模型價格或額度無關；本機 $100 參考預算也不是官方組織預算或帳戶餘額。UTC 日期在執行時取得；跨月範圍不影響本月至今卡片。

未來組織統計須使用具備適當權限的 Admin Key。一般專案金鑰的備援範圍應限於此用戶端已記錄的請求與回應 usage，不能宣稱取得整個組織的活動或官方帳單。未取得資料時應呈現無資料／權限狀態，禁止自動將示範值當成真實備援值。

官方欄位來源：[Usage API](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage)、[Usage 與 Costs 範例](https://developers.openai.com/cookbook/examples/completions_usage_api)。

## 相依套件

本次 UI 使用既有 Windows App SDK 與 WinUI Shapes 繪圖，未新增第三方圖表套件或 vcpkg 相依套件。新增的程式碼沿用專案 `GPL-3.0-only` 標記。

## 驗證

- x64 Debug：MSBuild 編譯、IDL／XAML 產生及資源封裝。
- 已實際啟動檢查深色三欄、淺色雙欄（960 DIP）與淺色單欄（560 DIP）；驗證後恢復跟隨系統主題與預設視窗尺寸。
- 已操作 7／30 天範圍切換、Logs 佔位頁、返回 Dashboard 與重新整理示範資料。
- HighContrast 已提供資源定義，尚未切換系統對比主題進行實機驗證。
