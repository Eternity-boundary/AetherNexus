# Tests

`Test-Management.ps1` 編譯並執行 Manage 的獨立合約測試，涵蓋端點與方法限制、UTC／專案／分組篩選、分頁與錯誤回應、金鑰遮罩、權限與期限語義、管理寫入不重試、取消、各類用量與 CSV 安全。此執行檔不連結 CredentialStore 或真實 HTTP 通道。

AdminKeyRevocation.Tests 核對官方物件型別、專案 key ID 交叉核對、部分專案無權讀取的警告、不依遮罩前綴限制 Admin 撤銷、原始分頁游標、Admin GET 核對後才 DELETE、錯誤物件／ID 停止操作、官方回應及安全 HTTP 診斷。`Test-AdminRevocationUi.ps1 -AppPid <離線宿主 PID>` 驗證只有尾碼／通用遮罩的 Admin 仍能辨識並啟用撤銷、相同專案 ID 分流為 API key、隱藏專案範圍、ID 確認、驗證失敗停止及撤銷結果；不操作真實帳戶。

Admin DELETE 404 的介面驗證包含空本文及帶 JSON／Request ID 的兩種回應：兩者均顯示官方 Platform 與固定的社群回報連結。其他狀態、GET 核對 404 及成功後不顯示這組導引。復原案例會在先前 404 後讓同一虛擬伺服器回傳有效成功結果，確認原撤銷 API 仍照常呼叫、選定項目正常移除且兩個入口收起；不建立永久停用旗標或背景 DELETE 重試。最新 UI 記錄為 artifacts/test-admin404-workaround-ui.log。

撤銷診斷另驗證 `ManagementHttpError.operation` 的 `AdminVerification`／`AdminRevocation`／`ProjectRevocation` 階段，讓 GET 核對 404 與 DELETE 404 可區分；GET 404 必須是零 DELETE，沒有自動繞過或替代路由。介面只顯示固定端點模板，不含實際金鑰或專案 ID。`ApiResponse.requestId` 只保留通過 `ManagementRequestId` 驗證的 `x-request-id`：總長 1–128 字元及 ASCII 英數字／底線／連字號，不要求特定前綴並接受 UUID；空值、過長、非 ASCII、控制字元、路徑符號、空白，以及以 `sk-` 開始的疑似憑證值均排除。另區分標頭 `Missing`／`Rejected`／`TooLong`／`ReadFailed`，避免誤解缺少可用 ID 的原因。這是本機顯示政策，不是官方格式保證。[官方 GET](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/admin_api_keys/methods/retrieve)、[官方 DELETE](https://developers.openai.com/api/reference/resources/admin/subresources/organization/subresources/admin_api_keys/methods/delete)、[請求診斷](https://developers.openai.com/api/reference/overview#debugging-requests)

`HttpResponseDiagnostics.Tests` 驗證在 32 KiB 安全暫存上解析的 facts：空／JSON error／其他 JSON／HTML-like／其他／格式錯誤／截斷等固定 enum，以及已知 error type／code／`Invalid URL` 訊息前綴的固定提示。解析不複製或保留任意原始本文、message、code；測試涵蓋無關欄位、跳脫名稱、重複欄位拒絕、Unicode／JSON 格式、深度與 token 上限。`Content-Type` 類別與 OpenAI 中繼標頭存在布林值只作診斷，不當成來源證明。

測試專用的傳輸邊界宿主攔截正式 WindowsManagementTransport 實際使用的 WinHTTP API，搭配假 CredentialStore 檢查要求與回應分類，不建立 socket、不讀取正式憑證。一般管理通道 Send 的每個 HTTP 請求使用獨立隨機 `X-Client-Request-Id`（獨立簽發通道除外）；它可協助支援查詢，但不是送達證明，也沒有文件保證 Admin DELETE 的記錄能力。合約與邊界測試記錄為 artifacts/test-http-response-facts.log、artifacts/test-native-management-transport.log。2026-11-04 本輪九項 Admin 撤銷 UI 測試全部通過，最新記錄為 artifacts/test-admin404-workaround-ui.log；Debug、Release 與離線 UI 宿主建置皆零警告、零錯誤。先前合約測試與五組原生傳輸邊界測試亦已通過，此次未變更 API 合約實作。這些測試不代表官方服務的 404 已修復。組織目錄的官方介面入口對應至 Admin keys 頁，專案 API keys 入口維持原有頁面。

同時執行 KeyIssuance.Tests：驗證 Admin／服務帳戶簽發、解析前遮蔽、無效或重複秘密、取消、不重送、move-only 所有權，以及 SecureZeroMemory 完整覆寫配置容量。`Test-KeyIssuanceUi.ps1 -AppPid <離線宿主 PID>` 驗證一次性彈窗、遮罩、不洩漏至 UI Automation、關閉與結束視窗；剪貼簿案例僅在原先為空時執行，不讀取或取代使用者既有內容。該 UI 腳本最後會關閉指定的離線宿主。

`New-ManageUiHarness.ps1` 產生只注入虛擬 IManagementTransport 的原生 UI 宿主。`Test-ManageUi.ps1 -AppPid <離線宿主 PID>` 只接受標題為 AetherNexus OFFLINE Manage Tests 的視窗；請勿指向正式程式。建置、啟動及目前 UI 自動化限制見 [Manage 說明](../docs/manage.md)。

`Test-WindowsManagementWire.ps1` 使用真正 WinHTTP 對本機 `127.0.0.1` 的假 HTTP 伺服器檢查實際傳出的 GET／DELETE、認證與 JSON 標頭、空本文及回應分類。適配器只修改 session／連線目的地／本機 HTTP，測試 CredentialStore 只回傳假權杖。兩組案例通過；記錄位於 artifacts/test-native-management-wire.log。它不測試外部 TLS、代理、DNS 或真正 OpenAI 服務。

`Probe-AdminKeyEndpoint.ps1` 是獨立的明確啟用式診斷工具，不屬於一般離線測試。預設只建置並顯示預覽，完全不連線；`-AllowNetwork` 會對固定的官方 HTTPS 端點各送一次 GET／DELETE，固定使用無效假權杖與合成 ID。執行檔不連結真實 CredentialStore、不讀環境變數或設定，僅輸出狀態、安全分類與白名單追蹤 ID。必須先取得使用者對此網路探測的明確同意。2026-11-04 經同意後執行兩個假資料請求：GET 回 JSON 401，DELETE 回無型別空本文 404；沒有讀取／傳送真實權杖或對帳戶執行操作。記錄位於 artifacts/admin-endpoint-probe-preview.log 及 artifacts/admin-endpoint-probe-live.log。相同回應形狀不代表已確定實際帳戶失敗原因，詳見 Manage 說明。

`AuthorizedAdminKey.Probe.vcxproj` 是手動、逐一目標授權的真實帳戶診斷工具，明確連結正式 CredentialStore，不屬於一般離線測試或 CI。無參數只預覽、不讀憑證；`--inspect <tracking-id>` 僅 GET，`--revoke <tracking-id> --confirm-id <same-id>` 才能在驗證後最多送一個 DELETE，再以 GET 核對。工具只允許指定 ID 的 Admin 路徑，拒絕其他目標／方法，不自動重試；不輸出權杖、遮罩或原始本文。可選 `--prefer-http2` 以診斷協定差異，保留 TLS 驗證。2026-11-04 對使用者明確授權目標測試，真實 HTTP/2 DELETE 仍為空 404，後續 GET 200 確認目標仍存在。記錄位於 artifacts/authorized-admin-inspection.log、artifacts/authorized-admin-revocation.log；一般測試仍不可讀取正式權杖。

`Test-Playground.ps1` 執行獨立的生成與工作流程合約測試，涵蓋 SSE、Base64、PCM/WAV、圖片、Multipart、Realtime 與工作流程，不連結真實傳輸或憑證儲存。

`New-PlaygroundUiHarness.ps1` 從目前 WinUI 設定產生隔離的 UI 測試專案，排除所有真實傳輸與憑證實作。`Test-PlaygroundUi.ps1 -AppPid <離線宿主 PID>` 驗證五個頁面，不可指向正式程式。建置／啟動方式及限制見 [Playground 說明](../docs/playground.md)。

`SettingsStorage.Tests.vcxproj` 是獨立原生整合測試，使用隔離的 Windows 認證命名空間與假權杖驗證儲存、覆寫、跨程序保存及移除，不操作正式權杖。

使用 `pwsh -NoProfile -File ./tests/Test-SettingsStorage.ps1` 執行。詳見 [設定與安全說明](../docs/settings-security.md)。

`PlatformApi.Tests.vcxproj` 測試正式 API 用戶端的請求、分頁、解析與加總。其唯一通道是 `Support/MockApiTransport`，不連結 WindowsHttpTransport 或認證儲存。執行 `pwsh -NoProfile -File ./tests/Test-PlatformApi.ps1`，不會發出網路請求或讀取真實權杖；也涵蓋本機預算金額的數值驗證。

同一測試專案亦編譯正式 DashboardViewModel，驗證官方欄位的計數語義、加權快取比例、零分母與缺值、圖表共用尺度及含年份的 UTC 日期時間；假資料與情境只存在於此目錄。

專案選擇涵蓋 Admin 專案清單的分頁與格式驗證、未命名／已封存專案、讀取權限獨立，以及 7／30 天用量與成本的專案篩選、分頁保留篩選、編碼與切回組織範圍。
