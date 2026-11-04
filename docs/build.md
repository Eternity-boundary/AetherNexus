# BUILD 工作區

受官方停用影響的 Prompts、Assistants、Evaluations 與 Fine-tuning，在標題下方提供醒目的「遷移指南」按鈕。指南在本機彈窗展示影響範圍、三個步驟、核對日期與官方連結；不會自動讀取權杖或執行遷移。Playground 的 Agent Builder／Audio／Video 及部分模型更替也有相應指南。維護指南時請同步更新 `Views/MigrationGuide.h`、README 與官方時程政策。

八個 BUILD 入口已接上原生 WinUI 頁面。正式程式沒有示範記錄或假值備援；所有遠端操作使用設定頁儲存的**一般 API 權杖**，不使用 Admin 權杖。專案 ID 留空時使用權杖的預設專案；指定其他專案仍須具有相應權限。

| 頁面 | 已實作功能 |
| --- | --- |
| Prompts | 本機 Prompt 版本庫、歷史版本還原為新版本、`{{name}}` 變數、JSON 匯入／匯出、明確確認後執行一次 Responses 測試。模型 ID 可自行輸入；測試固定 `store: false`。 |
| Logs | 列出已儲存的 Chat Completions，查閱訊息；依已知 Response ID 取得回應及輸入項目；刪除已儲存物件、JSON 匯出。 |
| Traces | Agent Sessions、Session traces 分頁查詢；原生 OTLP span 階層、持續時間、狀態及完整 JSON 檢視／匯出。 |
| Assistants | 遷移工作台，將先前匯出的名稱、模型與 instructions 匯入本機 Prompt，再以 Responses 測試。與 Prompts 共用版本庫。 |
| Batches | 建立 24 小時完成視窗的批次工作，檢視狀態及計數、依 ID 查詢、取消、下載 output／error 檔案。 |
| Evaluations | 唯讀查詢既有評估、runs 與 output items；匯出已載入項目或選定項目的 JSON，以及在本機開啟既有 JSON 匯出檔。 |
| Fine-tuning | 查詢訓練工作、events、checkpoints；建立 supervised／DPO 工作、取消及下載結果檔案。建立資格與模型支援由伺服器判定。 |
| Storage | Files 上傳、分頁、檢視、刪除與下載；向量庫建立、到期設定、檔案附加／移除、搜尋及刪除。 |

清單的搜尋僅篩選**已載入**的項目；「載入更多」逐頁取得資料，不在背景下載整個帳戶。未選取項目時「匯出 JSON」匯出目前載入的清單，選取後匯出該項目的完整資料。子項目可從選取項目進入，或在相應檢視輸入上層 ID 後按「套用／重新整理」。Evals 的 output items 須先從所屬評估進入 run，以保留 Eval ID。

## 官方語義與限制

本次依 2026-11-04 可取得的官方文件實作：

- [停用時程](https://developers.openai.com/api/docs/deprecations)：Assistants API 已於 **2026-08-26** 停用；Evals 自 **2026-10-31** 起唯讀、**2026-11-30** 停用；可重用 Prompt API 亦將於 **2026-11-30** 停用。Fine-tuning 建立資格限官方允許的既有活躍客戶，**2027-01-06** 起停止建立新工作。
- [Prompting](https://developers.openai.com/api/docs/guides/prompting)、[Assistants 遷移](https://developers.openai.com/api/docs/assistants/migration)：使用本機版本化 instructions／input，再直接呼叫 Responses；不建立即將停用的 Prompt／Assistant 雲端物件。匯入不會執行程式碼、不遷移 tools、工具資源或歷史對話。
- [Stored Chat Completions](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/list)：只列出以 `store: true` 儲存的物件，**不是所有 API 請求的日誌**。Responses 沒有在此虛構全域清單，必須輸入已知 ID。Playground 預設不儲存的請求不會出現在此。
- [Agent tracing](https://developers.openai.com/api/docs/guides/agents-api/tracing)：使用 `/v1/agents/sessions` 及 `/v1/agents/sessions/{id}/traces`、`OpenAI-Beta: agents=v1`。組織須啟用 Trace 匯出，權杖須有 `api.traces.read` 或 `api.agents.read`。不使用私人 Dashboard 端點。每頁最多 5 筆 Trace，階層最多顯示 1000 個 span；JSON 保留完整已取得內容。
- [Batches](https://developers.openai.com/api/reference/resources/batches/methods/create)：輸入須為已上傳、用途為 `batch` 的 JSONL；最大 200 MB、每個批次最多 50,000 個請求。端點由原生選單限制，完成視窗固定 `24h`。
- [Files](https://developers.openai.com/api/reference/resources/files/methods/create)：一般檔案最大 512 MB；batch／fine-tune 限 JSONL。服務仍會核對內容與用途。下載在本用戶端限制為 1 GiB。
- [Vector stores](https://developers.openai.com/api/reference/resources/vector_stores/methods/create)：可設定自最後使用起 1–365 天到期；移除 file association 不會刪除 Files 中的原始檔案。
- [Fine-tuning jobs](https://developers.openai.com/api/reference/resources/fine_tuning/subresources/jobs/methods/create)：training 與 validation 檔案須不同，資料格式須符合選定方法。進階 hyperparameters 以 JSON 輸入，合法範圍與模型資格依伺服器驗證。

程式會在 UTC 日期到達官方 Evals／Fine-tuning 截止日後，停止相應讀取／建立要求；本機 Evals 匯出檔仍可開啟。不影響仍可用的訓練工作查閱或取消。官方之後若調整時程，需同步更新政策與測試。

## 儲存、安全與生命週期

本機 Prompt 文件儲存在應用程式 LocalFolder 下的 `Prompts/catalog.json`，僅在明確儲存時寫入，使用跨程序鎖、原子取代與版本衝突檢查。上限為 100 個 Prompt、各 100 個版本、整個檔案 8 MiB。這是使用者的**明文 Prompt 文件**，不是憑證儲存位置；不將設定頁權杖加入匯出。變數只做單次字串替換，不執行腳本。

固定使用 `api.openai.com:443` 與 TLS 憑證驗證，停用重新導向、Cookie 與環境 HTTP 認證。權杖從現有安全儲存讀取，僅在傳輸時建立可擦除的 Authorization 緩衝區，送出後清除；錯誤回應不保留可能回顯敏感內容的原始本文。頁面不直接接觸 CredentialStore。

上傳採 64 KiB 串流，確認檔案大小與格式後才讀取權杖；multipart 檔名不允許控制字元或注入標頭。下載先寫入同目錄的唯一暫存檔，完整成功後才取代使用者選定的目的檔；失敗會清理暫存檔並保留原檔。程式不追隨回應中的任意下載 URL。

刪除／取消必須輸入完整目標 ID 確認。寫入沒有自動重試；取消等待、網路錯誤或離開頁面不代表伺服器工作已取消，需重新整理核對。離開頁面會取消傳輸並忽略過期的畫面更新；範圍改變後必須重新整理，才能對舊清單執行操作。

本次使用既有 Windows App SDK、WinHTTP、Windows.Data.Json 與 C++ 標準函式庫，沒有新增第三方依賴，因此不需要安裝 vcpkg 套件。新增程式碼採 `GPL-3.0-only`。

## 離線驗證

```powershell
./tests/Test-Build.ps1
MSBuild.exe ./tests/WindowsBuildTransport.Tests.vcxproj /p:Configuration=Debug /p:Platform=x64
./artifacts/tests/build-transport/WindowsBuildTransport.Tests.exe
./tests/New-BuildUiHarness.ps1
MSBuild.exe ./tests/BuildHarness/Build.UI.Tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:AppxPackageSigningEnabled=false
winapp run ./artifacts/bin/Build.UI.Tests/x64/Debug --manifest ./artifacts/bin/Build.UI.Tests/x64/Debug/AppxManifest.xml --exe AetherNexus.exe --detach --json
./tests/Test-BuildUi.ps1 -AppPid <離線宿主 ProcessId>
```

合約測試不連結真實傳輸或憑證儲存。原生傳輸邊界測試以 WinHTTP 與 CredentialStore 替身攔截正式傳輸程式，不建立 socket。UI 宿主只注入合成資料，排除所有真實傳輸與 CredentialStore；不得以正式程式進行這些測試。以上驗證不代表已用真實帳戶完成各項 API 操作。
