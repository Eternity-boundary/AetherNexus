# Manage

Usage、API keys、Projects 使用原生 WinUI 3 頁面與公開管理 API，沿用設定頁安全保存的 Admin Key。一般模型金鑰不能替代 Admin Key；缺少認證或權限不足時，顯示空狀態與設定入口。沒有示範資料或假值備援。

## 頁面與操作

| 頁面 | 功能 |
| --- | --- |
| Usage | UTC 日期、所有／指定專案、用量分類、分組、指標切換、每日長條圖、期間合計、明細搜尋與 UTF-8 CSV 匯出。 |
| API keys | 專案金鑰與組織金鑰目錄；顯示名稱、ID、持有人、末四碼及類型。依正式物件型別辨識，並以專案清單交叉核對相同 key ID。可簽發 Admin／服務帳戶權杖；Admin 撤銷前再次讀取核對；專案金鑰由所屬專案編輯及撤銷。 |
| Projects | 建立、改名與封存專案；新增既有組織成員、修改 member／owner 角色、移除成員；讀取及編輯伺服器提供的模型速率限制欄位。 |

篩選變更後須按「套用／重新整理」。舊資料立即失效，寫入與匯出停用，避免在尚未套用的新範圍操作舊項目。指標切換使用已載入報告，搜尋只篩選畫面清單，不改變圖表及完整報告匯出。專案清單包含已封存專案；原選擇消失時保留 ID，不自動改查整個組織。

個人使用者金鑰仍請於官方 Platform 的專案 API keys 頁建立；原生簽發支援 Admin Key 與服務帳戶 API Key。工具列「開啟官方 Platform」依目前檢視導向對應頁面：組織金鑰目錄開啟 [Admin keys](https://platform.openai.com/settings/organization/admin-keys)，專案 API keys 檢視維持開啟 [API keys](https://platform.openai.com/api-keys)。此入口方便使用者在官方介面核對或管理，不代表原生撤銷的 404 已解決。服務帳戶支援建立帳戶與初始權杖，或指定既有帳戶 ID 簽發；不提供完整服務帳戶目錄／角色管理。尚未實作金鑰改名、組織邀請、資料落地區域或其他進階組織政策，不宣稱完整複製官方 Platform。

封存、撤銷與移除成員須輸入完整項目 ID，對話框預設選擇取消。專案封存不能復原；Default 專案與帳戶特定限制仍以伺服器裁定為準。編輯對話框顯示名稱或 ID 與作用範圍；沒有修改時不傳送請求。速率限制受組織上限與帳戶權限約束；此頁不修改設定頁的本機參考預算。

## Admin 權杖撤銷的契約與診斷（2026-11-04）

官方 `AdminAPIKey` 模型明確定義 `object: organization.admin_api_key`，專案模型則使用 `organization.project.api_key`。解析應依正式物件型別辨識；`redacted_value` 只是顯示中繼資料，沒有保證保留可辨識的前綴，也不是金鑰種類欄位。原先依遮罩辨識並限制撤銷，導致有效 Admin 物件被標示為待確認；這項假設與相關測試已修正。[Admin 模型與讀取契約](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/admin_api_keys/methods/retrieve)、[專案模型](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/subresources/api_keys/methods/retrieve)

組織索引 `GET /v1/organization/admin_api_keys` 的官方標題同時提到組織與專案金鑰。因此 `OrganizationKeys` 保留全部回傳項目，使用已載入的專案目錄，逐一讀取公開的專案 API key 清單，按完整 ID 交叉核對。出現在專案清單中的相同 ID 歸為專案 API key，即使組織索引將其包成 Admin 物件；其餘項目保留正式物件型別。`key_` ID 前綴、名稱、owner 角色及遮罩前綴都不作種類證明。這讓一般 API key 的分流有可核對的來源，同時不會因 Admin 遮罩缺失而停用操作。[官方組織清單契約](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/admin_api_keys/methods/list)

畫面仍稱「組織金鑰目錄（全部）」，每列顯示「Admin 權杖」或「專案 API 權杖」，種類可搜尋並顯示於明細。通用 `sk-`、僅尾碼、星號、空值、未來格式，甚至與物件型別矛盾的範例前綴都不改變物件種類。原始遮罩不保存，模型只持有種類與末四碼，項目順序與分頁游標依原始清單。不認識的正式物件型別會報格式錯誤，不猜測為 Admin。

交叉核對只做讀取，不會新增、編輯、撤銷或試用任一金鑰。每次最多查詢 128 個專案，保留原有分頁／回應大小限制並支援取消；只保留與組織目錄 ID 相符的集合，避免為其他專案金鑰累積額外索引。專案金鑰清單 403／404 或超過專案查詢上限時保留已取得目錄，以警告說明核對不完整，不把無權讀取誤當成空專案。未核對到的項目仍依伺服器原始物件型別顯示，不宣稱這些情況下已完成所有專案的排除。

Admin 撤銷只接受已分類的 Admin 記錄，先以 `GET /v1/organization/admin_api_keys/{key_id}` 重新核對 ID 與種類，再送出一次 `DELETE` 到同一路徑。預讀是本程式的核對政策，官方 DELETE 契約沒有要求先 GET。核對 404、ID 不符或錯誤物件都會停止，GET 404 時 DELETE 呼叫次數為零，不會自動略過核對、改試其他撤銷端點或將 404 當成成功。成功必須回傳相同 `id`、`deleted: true` 與 `object: organization.admin_api_key.deleted`。[Admin 讀取](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/admin_api_keys/methods/retrieve)、[Admin 撤銷](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/admin_api_keys/methods/delete)

一般 API 金鑰由所屬專案清單管理，使用 `DELETE /v1/organization/projects/{project_id}/api_keys/{api_key_id}`，驗證 `organization.project.api_key.deleted` 回應。組織目錄中的專案 API 項目可查看中繼資料，編輯與撤銷停用；Admin 項目啟用撤銷，仍須確認完整 ID 與再次讀取核對。不猜測所屬專案；組織檢視隱藏專案選擇欄，明確標示組織範圍。[專案金鑰撤銷](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/projects/subresources/api_keys/methods/delete)

Admin 資源本身的既有方法與路徑符合文件，但原先未確認清單項目的金鑰種類。正式傳輸另由 ManagementWireRequest 統一產生官方範例的 JSON Accept／Content-Type，DELETE 保留空本文。Admin 撤銷使用獨立的結構化請求產生器，不帶入專案資料；回應額外核對官方刪除物件型別。

另一個已確認問題是 HTTP 400／404／409／422 原先全被轉成一般「輸入無效」。ManagementClient 現在區分本機參數驗證與伺服器拒絕：伺服器拒絕保留非敏感的 HTTP 狀態碼，404 提示找不到項目、409 提示狀態衝突、415 提示內容格式，其餘提供拒絕操作的說明。401／403／429 及伺服器／網路錯誤維持原本分類。錯誤 body 不顯示或記錄，DELETE 不自動重送，404 也不當成撤銷成功。

只顯示 HTTP 404 無法分辨是預讀失敗還是 DELETE 失敗。`ManagementHttpError.operation` 現在保留失敗階段，畫面列出該階段、HTTP 方法與固定端點模板：

| 操作階段 | 方法與端點模板 | 失敗含義 |
| --- | --- | --- |
| `AdminVerification` | `GET /v1/organization/admin_api_keys/{key_id}` | 撤銷前核對失敗，尚未送出 DELETE。 |
| `AdminRevocation` | `DELETE /v1/organization/admin_api_keys/{key_id}` | 核對已通過，DELETE 收到拒絕回應。 |
| `ProjectRevocation` | `DELETE /v1/organization/projects/{project_id}/api_keys/{api_key_id}` | 專案金鑰撤銷收到拒絕回應。 |

端點模板不含實際 key ID、專案 ID、金鑰名稱或任何權杖內容。`ApiResponse.requestId` 可攜帶回應標頭 `x-request-id`；`ManagementRequestId` 接受總長 1–128 字元，且全部皆為 ASCII 英數字、底線或連字號的值，不要求未經文件保證的前綴，因此 UUID 等形式亦可使用。空值、過長、非 ASCII、控制字元、路徑符號、空白，以及以 `sk-` 開始的疑似憑證值均排除。這是本程式的顯示驗證政策，不是官方 request ID 格式保證。WinHTTP 使用固定 129 個 wchar 的讀取緩衝區，並區分 `Missing`、`Rejected`、`TooLong`、`ReadFailed` 等狀態，避免把「沒有可用值」一律解讀為服務未回傳標頭。[官方請求診斷與 request ID 說明](https://developers.openai.com/api/reference/overview#debugging-requests)

最新使用者回報顯示，同一 key 的 GET 核對成功，隨後 DELETE 回傳 404，且沒有可用的 server request ID。這已將失敗定位至 DELETE 階段，但未證明是程式、權限、資源狀態、代理或服務端的哪一項原因。官方文件仍使用相同 DELETE 路由，沒有改用替代路由、略過核對或視為撤銷成功的依據。

後續回報進一步確認為空本文、沒有可辨識的 Content-Type、缺少 Request ID 與已檢查的 OpenAI 中繼標頭；官方 Admin keys 頁面可撤銷該金鑰。官方網頁與公開 API 可能採用不同後端，因此網頁成功本身不能證明公開 DELETE 端點正常。使用者已測試 AdGuard 應用程式排除與暫停防護，症狀相同；目前沒有證據可歸因於 AdGuard 或 DNSGuard。

為進一步區分回應形狀，正式傳輸在 `SecureBuffer` 中最多讀取 32 KiB 的錯誤本文，交由不配置字串的 `InspectErrorResponse` 檢查。解析限制 16 層巢狀與 4096 個 token；診斷欄位名的 JSON 跳脫可正確比對，重複或格式錯誤的欄位會被拒絕。回傳只含固定 enum：空本文、JSON error、其他 JSON、HTML-like、其他內容、JSON 無效、截斷或讀取失敗；已知 error type／code 及 `Invalid URL` 訊息前綴只轉成固定的種類與提示，不保留原始 message、code 或任何任意本文字串。暫存由 SecureBuffer 清除，原始本文不進入 UI、一般字串、日誌或匯出。

介面另顯示 Content-Type 類別與 OpenAI 中繼標頭是否存在的布林資訊。這些是回應特徵，不是來源證明：JSON error 不足以證明一定來自 OpenAI；HTML-like、空本文或缺少 request ID 也不足以斷言是代理阻擋。錯誤本文不會被當作 HTML 呈現，亦不顯示 Authorization 或任意伺服器標頭文字。

一般管理通道 `Send` 的每個 HTTP 請求另產生獨立隨機 `X-Client-Request-Id`，不包含帳戶、專案或金鑰資料，讓 GET 核對與 DELETE 有各自的追蹤識別碼。獨立的簽發通道不包含這項診斷。官方文件說明這可在缺少 server request ID 時協助支援查詢，但沒有特別保證 Admin DELETE 的記錄能力；本機產生或送出 client ID 不是請求已到達 OpenAI 的證明。[官方 client request ID 說明](https://developers.openai.com/api/reference/overview#supplying-your-own-request-id-with-x-client-request-id)

這些變更增加可核對的證據，並不代表真實帳戶的 404 已修復。一般測試不讀取真實憑證；後續僅在使用者明確指定測試金鑰並授權撤銷後進行下述帳戶連線驗證。診斷不需要使用者提供完整權杖內容。

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

正式 WindowsManagementTransport 固定使用 api.openai.com:443 與 /v1/organization 的受限路徑，保留 TLS 憑證驗證，停用重新導向、Cookie 與自動驗證；送出後清除 Authorization 暫存。只在真正請求時讀取 Admin 憑證，不回退環境變數或模型金鑰。錯誤本文只在有上限的安全緩衝區中分類，僅保留固定 facts enum、受限標頭狀態及追蹤識別碼；不保存或顯示原始本文、訊息、code 或任意標頭文字。請求與私有資料不寫入日誌。

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

AdminKeyRevocation.Tests 驗證正式物件種類、組織／專案清單按 ID 分流、owner 角色與遮罩不作種類證明、通用 `sk-`／ASCII 或 Unicode 省略號／星號／未來格式／空或矛盾遮罩的 Admin 仍可依確認流程撤銷、專案 403／404 的部分核對警告、原始分頁順序與游標，以及 Admin 讀取核對後才 DELETE。也覆蓋驗證 404／錯誤 ID／錯誤物件時零 DELETE、JSON 標頭、空本文與專案資料、官方成功回應、400／404／409／415／422 狀態保留、401／403／429／503 原有分類、取消與不重試。

分階段診斷的回歸案例分別核對 Admin GET 404、Admin DELETE 404 與專案 DELETE 404；確認 operation、固定端點模板與有效 request ID 的保留，以及缺少／超長／惡意標頭值的排除。GET 404 案例必須確認零 DELETE，不只比對畫面文字。

`HttpResponseDiagnostics.Tests` 使用合成的秘密、HTML 與注入訊息，驗證只保留 facts enum，涵蓋 root.error 與無關欄位的區分、Unicode 跳脫欄位、重複診斷欄位拒絕、JSON／UTF-8 格式及大小、深度、token 上限。合約測試記錄位於 artifacts/test-http-response-facts.log。

傳輸邊界測試直接編譯正式 WindowsManagementTransport，攔截其實際呼叫的 WinHTTP API，並注入測試專用 CredentialStore。它檢查 wire request、每個請求的 client ID、server request ID 讀取狀態、Content-Type／中繼標頭與錯誤本文分類，不建立 socket、不讀取正式憑證。記錄位於 artifacts/test-native-management-transport.log；測試替身只存在 tests 內。

`Test-WindowsManagementWire.ps1` 另以真正 WinHTTP DLL 對本機 `127.0.0.1` 假伺服器檢查序列化後的 HTTP。測試專用適配器只將 session 改為無代理、目的地改為本機隨機埠、將 HTTPS 改為本機 HTTP；其餘傳送、標頭與回應讀取使用正式通道及真正 WinHTTP。僅連結假 CredentialStore，確認 GET／DELETE 路徑、Authorization 未被提早擦除、JSON 標頭、零長度本文、獨立 client ID，以及和使用者回報一致的無型別空本文 404。兩組案例通過，記錄位於 artifacts/test-native-management-wire.log。此測試不覆蓋外部 TLS、代理、DNS 或 OpenAI 實際路由，也不證明真實 404 已修復。

`Probe-AdminKeyEndpoint.ps1` 是獨立且不加入一般測試批次的端點探測工具。預設僅建置並顯示預覽，無網路請求；加入 `-AllowNetwork` 才會由正式 WinHTTP 通道各送出一次 GET／DELETE。目的地固定為官方 HTTPS 主機，ID 為固定合成值，不引用任何真實清單項目；權杖固定為無效測試字串。不連結真實 CredentialStore、不讀取環境變數或設定。輸出僅含狀態、固定回應分類與通過白名單的追蹤 ID，不輸出原始本文、標頭或權杖。需使用者明確同意這項真實網路探測後才執行連線模式。

2026-11-04 經使用者明確同意後，探測各送出一次假資料 GET／DELETE：GET 為 JSON error／HTTP 401，附有效 server Request ID 與 OpenAI 中繼標頭；DELETE 為 HTTP 404／空本文，未提供可辨識 Content-Type、server Request ID 或已檢查的 OpenAI 中繼標頭，和使用者截圖的形狀相同。這證明該回應形狀也可在無效權杖與合成 ID 下出現，不能將它直接當成真實金鑰不存在的證明；也不足以確認回應來源或排除所有權限／資源因素。記錄位於 artifacts/admin-endpoint-probe-live.log，無連線預覽位於 artifacts/admin-endpoint-probe-preview.log。沒有讀取或送出真實憑證，沒有對任何真實帳戶項目執行操作。

介面僅在 `ManagementHttpError.status == 404` 且 `operation == AdminRevocation`（Admin DELETE 失敗）時提供替代導引，不依賴 Content-Type、本文或中繼標頭的特定形狀。提示這可能與已回報的服務問題有關，顯示「開啟官方 Admin keys」及「查看撤銷 API 問題回報」兩個原生連結。回報連結固定指向使用者提供的 [OpenAI Developer Community 討論串](https://community.openai.com/t/delete-v1-organization-admin-api-keys-key-id-returns-404-with-empty-body-get-on-the-same-id-returns-200/1411757)，不附加權杖、項目 ID 或追蹤參數。該討論串有其他使用者重現及已轉交團隊的回覆；這不是將所有 404 都判定為同一個原因。

保留原有 GET 核對、DELETE 路徑、回應驗證與安全診斷。導引只處理已收到的錯誤，不快取「API 不支援」狀態、不關閉撤銷功能，也不預先繞過 API。每次使用者重新整理並確認撤銷時都走原有流程；官方修復後若回傳有效成功回應，程式直接正常處理並收起兩個入口，不需要版本更新或重設設定。GET 核對 404、非 Admin 撤銷及其他錯誤不顯示這組導引。重新整理、切換篩選或新的操作結果亦會收起入口；沒有背景 DELETE 重試、自動開啟瀏覽器或網站私有 API 呼叫。

`AuthorizedAdminKey.Probe.vcxproj` 是另一個獨立的手動帳戶診斷專案，明確連結正式 CredentialStore；不加入一般測試／CI，也不修改正式應用程式。無參數執行只顯示預覽，不讀憑證或連線。`--inspect <tracking-id>` 只允許指定 Admin key 的 GET；`--revoke <tracking-id> --confirm-id <same-id>` 才能在核對 ID／物件、檢查目前簽署金鑰的遮罩尾碼後送出最多一個 DELETE，再做 GET 核對。尾碼檢查僅為保守的誤刪保護，不是完整的金鑰身分辨識。每次連線／撤銷須有使用者對該目標的明確授權；不可由一般測試腳本呼叫。

2026-11-04 使用者指定一枚測試唯讀權杖並明確授權撤銷後，沿用設定頁 Admin Key 執行：HTTP/1.x GET 為 200，ID 與 Admin 物件符合；隨後使用 `--prefer-http2`，確認實際協商為 HTTP/2，GET 核對為 200，唯一的 DELETE 仍為無型別空本文 404。之後 GET 為 200 且同一 ID 仍存在，確認未撤銷成功，沒有重送 DELETE 或改用其他路由。這不是單純假憑證才出現的回應，也未藉 HTTP/2 解決；仍不足以確定服務端、存取政策或傳輸路徑的哪一環節造成 404。紀錄位於 artifacts/authorized-admin-inspection.log、artifacts/authorized-admin-revocation.log。完整權杖不輸出、不另存檔，也不變更本機認證。

組織目錄與 Admin 撤銷的離線 UI 批次覆蓋：只有尾碼及通用遮罩的 Admin 正確辨識並啟用撤銷、按專案 ID 分流且不受遮罩影響、簽發另一枚權杖並出現在清單、完整 ID 確認、HTTP 400／404 的安全診斷、讀取核對失敗停止，以及成功後僅移除新權杖並保留原項目。另檢查 GET 與 DELETE 失敗的顯示差異、固定端點模板與回應 facts。空本文 404 及帶 JSON／Request ID 的 Admin DELETE 404 都會出現兩個入口；400、GET 核對失敗及成功後均收起。最後一項模擬服務從 404 恢復為正常成功回應，確認仍可透過原 API 撤銷並清除導引。最新結果與截圖位於 artifacts/tests/admin-revocation-ui、artifacts/test-admin404-workaround-ui.log。共用輔助程式對下拉項目使用無障礙名稱、對無名稱的型別化清單列點選已核對的標題，避免依賴隨檢查深度改變的臨時選擇器。本輪功能驗證未向真實 OpenAI 帳戶簽發或撤銷權杖。

簽發 UI 驗證五項通過：取消表單、新服務帳戶、既有服務帳戶、Admin 權杖及彈窗開啟時關閉主視窗；亦確認完整值不出現在 UI Automation。剪貼簿案例因既有使用者內容而跳過，未讀取或取代該內容，不宣稱已完成實際複製／清除的 UI 驗收。結果與截圖位於 artifacts/tests/key-issuance-ui。

原生 UI 測試宿主完全排除正式傳輸與憑證儲存，生成的專案與 manifest 不提交：

```powershell
./tests/New-ManageUiHarness.ps1
MSBuild.exe ./tests/ManageHarness/Manage.UI.Tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:AppxPackageSigningEnabled=false
winapp run ./artifacts/bin/Manage.UI.Tests/x64/Debug --manifest ./artifacts/bin/Manage.UI.Tests/x64/Debug/AppxManifest.xml --exe AetherNexus.exe --detach --json
./tests/Test-ManageUi.ps1 -AppPid <離線宿主的 ProcessId>
./tests/Test-KeyIssuanceUi.ps1 -AppPid <離線宿主的 ProcessId>
./tests/Test-AdminRevocationUi.ps1 -AppPid <離線宿主的 ProcessId>
```

UI 已驗證初始用量總計、專案建立表單、金鑰政策、成員／速率限制編輯器、主題／窄視窗及缺少 Admin Key 指引。共用測試輔助程式使用明確的 owned-popup HWND 與遞迴項目選取。最新完整批次為五項通過、兩項未通過：成本類別項目查找失敗，以及專案總覽載入等待逾時；這些既有跨頁 UI 自動化案例仍不穩定，不能宣稱整批全數通過。日誌、結果與截圖位於 artifacts/tests/manage-ui 及 artifacts/test-manage-issuance-regression.log。

2026-11-04 本輪 Admin 撤銷 UI 九項全部通過，已逐一檢查七張本輪截圖，包含空本文／JSON 兩種 DELETE 404 導引、GET 核對失敗及恢復成功畫面，未見導引文字裁切或重疊。Debug、Release 與離線 UI 宿主皆建置成功，零警告、零錯誤。先前合約測試與五組原生傳輸邊界測試的通過紀錄仍保留於上述日誌；此次沒有變更其 API 合約實作。

Debug／Release 使用隔離輸出 artifacts/validation-admin404-workaround/{Debug,Release}，避免覆寫使用者正在偵錯的程式與 PDB。本輪建置記錄位於 artifacts/build-admin404-workaround-debug.log、artifacts/build-admin404-workaround-release.log、artifacts/build-admin404-workaround-ui.log。Release 保留 /Wall、/WX，排除最佳化的 C4710／C4711 內嵌決策提示；詳見儲存庫 README 的編譯說明。

離線測試只使用替身或本機回環；先前另經使用者同意進行兩個真實網路／假資料探測，以及對明確授權目標的真實帳戶核對與單次撤銷測試。後者讀取設定頁的正式 Admin 憑證於記憶體中傳送，僅保留安全診斷，不輸出或保存完整值。該次 2026-11-04 實測仍為 404 且目標仍存在；本輪新增導引與恢復條件僅做離線驗證，不代表已確認官方服務修復。
