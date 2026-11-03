# Manage

Usage、API keys、Projects 使用原生 WinUI 3 頁面與公開管理 API，沿用設定頁安全保存的 Admin Key。一般模型金鑰不能替代 Admin Key；缺少認證或權限不足時，顯示空狀態與設定入口。沒有示範資料或假值備援。

## 頁面與操作

| 頁面 | 功能 |
| --- | --- |
| Usage | UTC 日期、所有／指定專案、用量分類、分組、指標切換、每日長條圖、期間合計、明細搜尋與 UTF-8 CSV 匯出。 |
| API keys | 專案金鑰與組織 Admin 金鑰目錄；顯示名稱、ID、持有人及末四碼。可簽發 Admin／服務帳戶權杖；專案金鑰可修改 scopes 與到期期限；兩種目錄皆可撤銷伺服器回傳且符合該端點型別的金鑰。 |
| Projects | 建立、改名與封存專案；新增既有組織成員、修改 member／owner 角色、移除成員；讀取及編輯伺服器提供的模型速率限制欄位。 |

篩選變更後須按「套用／重新整理」。舊資料立即失效，寫入與匯出停用，避免在尚未套用的新範圍操作舊項目。指標切換使用已載入報告，搜尋只篩選畫面清單，不改變圖表及完整報告匯出。專案清單包含已封存專案；原選擇消失時保留 ID，不自動改查整個組織。

個人使用者金鑰仍由工具列的「開啟官方 Platform」入口建立；原生簽發支援 Admin Key 與服務帳戶 API Key。服務帳戶支援建立帳戶與初始權杖，或指定既有帳戶 ID 簽發；不提供完整服務帳戶目錄／角色管理。尚未實作金鑰改名、組織邀請、資料落地區域或其他進階組織政策，不宣稱完整複製官方 Platform。

封存、撤銷與移除成員須輸入完整項目 ID，對話框預設選擇取消。專案封存不能復原；Default 專案與帳戶特定限制仍以伺服器裁定為準。編輯對話框顯示名稱或 ID 與作用範圍；沒有修改時不傳送請求。速率限制受組織上限與帳戶權限約束；此頁不修改設定頁的本機參考預算。

## 用量語義

支援 completions、costs、embeddings、images、audio_speeches、audio_transcriptions、moderations、vector_stores、code_interpreter_sessions、file_search_calls、web_search_calls。各分類使用自己的計量欄位與可用分組，不把圖片、字元、秒、Bytes、工作階段或費用混稱為 Token。

- 日期選擇器的起訖日均標示 UTC、包含當日；請求轉為包含開始、不包含結束的時間區間，今日截止目前時間。最多 366 日，每日桶、每頁上限 31，完整讀取分頁後才顯示成功。
- 輸入 Token 已包含快取 Token，不重複相加。成本為所選區間、所選專案範圍的全部 USD 成本，允許負值抵扣；它不是餘額，也不限定 Completions。
- vector_stores 的 usage_bytes 是每日儲存量，保留每日圖表，期間合計顯示「—」，不跨日相加。正常空查詢為零活動；已有結果中的缺失指標顯示不可用，錯誤不當成零。
- CSV 的 start_time_unix_utc／end_time_unix_utc 是 Unix 秒，欄位附分類、專案、分組及各指標。文字欄位防止試算表公式注入；負成本維持數值。匯出須由使用者選擇檔案位置，不自動寫檔。

官方合約比對：[Usage](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage)、[Costs](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/usage/methods/costs)、[Projects](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects)、[Project API keys](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/subresources/api_keys)、[Admin API keys](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/admin_api_keys)、[Project users](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/subresources/users)、[Rate limits](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/subresources/rate_limits)。

專案金鑰 scopes 未指定表示保留；空陣列表示移除額外的 scope 限制（All，仍受持有人權限約束），不是禁止存取；唯讀使用 api.all.read，自訂模式不可留空。expires_in_seconds 從本次請求相對計時；介面提供 1–365 整日或留空保留。[官方更新合約](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/subresources/api_keys/methods/update)

## 傳輸與生命週期

ManagementClient 對接 IManagementTransport，驗證端點、方法、ID、游標、分頁、物件型別、數值與寫入回傳 ID。解析僅保留白名單中繼資料；即使回應含 value，也不保存／顯示該秘密。金鑰提示只留末四碼，原始 JSON 不進入頁面或匯出。

## 簽發與敏感記憶體

API keys 工具列提供「簽發 API 權杖」，沿用目前設定的 Admin Key，不替換任何已儲存權杖。專案模式須先選擇有效且未封存專案；可選新服務帳戶或輸入既有服務帳戶 ID。名稱必填、最多 100 字元，有效期預設 30 日，可設定 1–365 日或留空依伺服器政策。新服務帳戶的建立亦賦予預設專案 member 身分，初始金鑰權限依帳戶與政策；可於清單另行調整 scopes 或撤銷。

| 類型 | 單次 POST 端點 |
| --- | --- |
| Admin Key | /v1/organization/admin_api_keys |
| 新服務帳戶與初始權杖 | /v1/organization/projects/{project_id}/service_accounts |
| 既有服務帳戶權杖 | /v1/organization/projects/{project_id}/service_accounts/{service_account_id}/api_keys |

官方合約：[Create admin API key](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/admin_api_keys/methods/create)、[Create service account](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/subresources/service_accounts/methods/create)、[Create service-account API key](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/subresources/service_accounts/subresources/api_keys/methods/create)。沒有個人金鑰的非公開 API 或自行合成權杖。

KeyIssuanceClient／IKeyIssuanceTransport 使用獨立的 move-only SensitiveApiResponse。簽發回應最多 64 KiB、權杖最多 4096 個可列印 ASCII 字元。HTTP 讀取區塊、原始回應與權杖皆使用固定配置的 SecureBuffer，不會擴容，也不使用字串的小型緩衝最佳化；複製建構與複製指定已刪除，移動只轉移配置所有權。

回應中的 value 直接解碼進可擦除緩衝區，原始 JSON 中的整段編碼內容立即用 SecureZeroMemory 覆寫，再填入空白；只有已遮蔽的 JSON 可轉為 WinRT 字串／JSON 物件。拒絕重複 value（含跳脫欄位名稱）、錯置欄位、非預期物件型別、無效字元與格式錯誤。正常、例外、取消與過期回應路徑都由 RAII 清除整個已配置容量，不只縮短長度或 clear() 字串。

一次性 ContentDialog 只顯示名稱、ID 與遮罩尾碼；完整權杖不放入 TextBox、PasswordBox、TextBlock、資料綁定、UI Automation、一般 std::string／hstring、日誌或檔案。使用者明確按下複製才直接寫入原生剪貼簿記憶體；不自動儲存。Closing 事件在彈窗消失前呼叫 SecureZeroMemory，離頁及關閉主視窗另有清除路徑，解構時亦再次清除。關閉後無法再次取得完整值，亦不會自動撤銷伺服器金鑰。

複製先設定 Windows 的 ExcludeClipboardContentFromMonitorProcessing，並將 CanIncludeInClipboardHistory／CanUploadToCloudClipboard 設為零，要求排除歷程與雲端同步。關閉時只有 owner、序號與記憶體 handle 都仍相同才覆寫並清空剪貼簿，不刪除使用者後來複製的新內容。正常關閉遇到剪貼簿忙碌會先清除程式緩衝區，再提示重試關閉；強制離頁／視窗結束採盡力清理剪貼簿。[Windows 剪貼簿格式](https://learn.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats)

清除保證限於程式控制的可變緩衝區；Windows／TLS 的內部副本、其他程式已讀取或儲存的內容、螢幕擷取及異常終止不在可保證擦除範圍。使用 [SecureZeroMemory](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/aa366877%28v%3Dvs.85%29) 避免最佳化移除擦除操作。

正式 WindowsManagementTransport 固定使用 api.openai.com:443 與 /v1/organization 的受限路徑，保留 TLS 憑證驗證，停用重新導向、Cookie 與自動驗證；送出後清除 Authorization 暫存。只在真正請求時讀取 Admin 憑證，不回退環境變數或模型金鑰。錯誤 body 不保留或顯示；請求與私有資料不寫入日誌。

設定各階段逾時、回應大小與分頁／資料量上限。停止或離開頁面會取消傳輸，舊回應不更新新頁面。POST／DELETE 不自動重試；網路錯誤或取消可能發生在伺服器已完成寫入之後，介面明確提示結果不確定，須重新整理確認。新增、編輯與撤銷均須使用者明確操作，進入頁面僅讀取清單或統計。

ManagePage 由 MainWindow 注入通道。清單使用不可變的 ManageItem 與具型別 x:Bind、原生 ListView 虛擬化，未包覆無界的外層 ScrollViewer。使用 ThemeResource 支援深淺色及系統高對比色；窄視窗命令列可溢位至選單。沒有新增第三方／vcpkg 相依套件；新增程式碼採 GPL-3.0-only。

## 離線驗證

```powershell
./tests/Test-Management.ps1
./tests/Test-PlatformApi.ps1
./tests/Test-Playground.ps1
./scripts/Build.ps1
./scripts/Build.ps1 -Configuration Release
```

Management.Tests 只連結正式合約程式碼與 tests 內的虛擬通道，不連結 CredentialStore、真實 HTTP 通道或網路呼叫。涵蓋方法／路徑限制、政策 JSON、UTC 範圍、分頁保留篩選、金鑰遮罩、管理 CRUD 回傳、錯誤、取消、不重試、缺值、負成本、CSV 與儲存量語義；另執行 Dashboard／Playground 合約回歸。

KeyIssuance.Tests 同時驗證三種簽發路徑、輸入與有效期、跳脫／錯誤／重複秘密、取消、不重試、move-only 所有權，以及包括未使用尾端在內的整個配置區域擦除。所有憑證與回應均為 tests 內的假資料。

簽發 UI 驗證五項通過：取消表單、新服務帳戶、既有服務帳戶、Admin 權杖及彈窗開啟時關閉主視窗；亦確認完整值不出現在 UI Automation。剪貼簿案例因既有使用者內容而跳過，未讀取或取代該內容，不宣稱已完成實際複製／清除的 UI 驗收。結果與截圖位於 artifacts/tests/key-issuance-ui。

原生 UI 測試宿主完全排除正式傳輸與憑證儲存，生成的專案與 manifest 不提交：

```powershell
./tests/New-ManageUiHarness.ps1
MSBuild.exe ./tests/ManageHarness/Manage.UI.Tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:AppxPackageSigningEnabled=false
winapp run ./artifacts/bin/Manage.UI.Tests/x64/Debug --manifest ./artifacts/bin/Manage.UI.Tests/x64/Debug/AppxManifest.xml --exe AetherNexus.exe --detach --json
./tests/Test-ManageUi.ps1 -AppPid <離線宿主的 ProcessId>
./tests/Test-KeyIssuanceUi.ps1 -AppPid <離線宿主的 ProcessId>
```

UI 已驗證初始用量總計、專案建立表單、金鑰政策、成員／速率限制編輯器、主題／窄視窗及缺少 Admin Key 指引。共用測試輔助程式使用明確的 owned-popup HWND 與遞迴項目選取。最新完整批次為五項通過、兩項未通過：成本類別項目查找失敗，以及專案總覽載入等待逾時；這些既有跨頁 UI 自動化案例仍不穩定，不能宣稱整批全數通過。日誌、結果與截圖位於 artifacts/tests/manage-ui 及 artifacts/test-manage-issuance-regression.log。

x64 Debug／Release 與離線 UI 宿主均通過建置，零警告、零錯誤。Release 保留 /Wall、/WX，僅新增排除最佳化的 C4710／C4711 內嵌決策提示；詳見儲存庫 README 的編譯說明。

本次驗證沒有讀取正式權杖、送出真實 OpenAI 請求或變更帳戶；正式管理通道僅經編譯驗證，尚未進行實際帳戶連線驗收。
