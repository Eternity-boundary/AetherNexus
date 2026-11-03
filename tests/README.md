# Tests

`Test-Management.ps1` 編譯並執行 Manage 的獨立合約測試，涵蓋端點與方法限制、UTC／專案／分組篩選、分頁與錯誤回應、金鑰遮罩、權限與期限語義、管理寫入不重試、取消、各類用量與 CSV 安全。此執行檔不連結 CredentialStore 或真實 HTTP 通道。

同時執行 KeyIssuance.Tests：驗證 Admin／服務帳戶簽發、解析前遮蔽、無效或重複秘密、取消、不重送、move-only 所有權，以及 SecureZeroMemory 完整覆寫配置容量。`Test-KeyIssuanceUi.ps1 -AppPid <離線宿主 PID>` 驗證一次性彈窗、遮罩、不洩漏至 UI Automation、關閉與結束視窗；剪貼簿案例僅在原先為空時執行，不讀取或取代使用者既有內容。該 UI 腳本最後會關閉指定的離線宿主。

`New-ManageUiHarness.ps1` 產生只注入虛擬 IManagementTransport 的原生 UI 宿主。`Test-ManageUi.ps1 -AppPid <離線宿主 PID>` 只接受標題為 AetherNexus OFFLINE Manage Tests 的視窗；請勿指向正式程式。建置、啟動及目前 UI 自動化限制見 [Manage 說明](../docs/manage.md)。

`Test-Playground.ps1` 執行獨立的生成與工作流程合約測試，涵蓋 SSE、Base64、PCM/WAV、圖片、Multipart、Realtime 與工作流程，不連結真實傳輸或憑證儲存。

`New-PlaygroundUiHarness.ps1` 從目前 WinUI 設定產生隔離的 UI 測試專案，排除所有真實傳輸與憑證實作。`Test-PlaygroundUi.ps1 -AppPid <離線宿主 PID>` 驗證五個頁面，不可指向正式程式。建置／啟動方式及限制見 [Playground 說明](../docs/playground.md)。

`SettingsStorage.Tests.vcxproj` 是獨立原生整合測試，使用隔離的 Windows 認證命名空間與假權杖驗證儲存、覆寫、跨程序保存及移除，不操作正式權杖。

使用 `pwsh -NoProfile -File ./tests/Test-SettingsStorage.ps1` 執行。詳見 [設定與安全說明](../docs/settings-security.md)。

`PlatformApi.Tests.vcxproj` 測試正式 API 用戶端的請求、分頁、解析與加總。其唯一通道是 `Support/MockApiTransport`，不連結 WindowsHttpTransport 或認證儲存。執行 `pwsh -NoProfile -File ./tests/Test-PlatformApi.ps1`，不會發出網路請求或讀取真實權杖；也涵蓋本機預算金額的數值驗證。

同一測試專案亦編譯正式 DashboardViewModel，驗證官方欄位的計數語義、加權快取比例、零分母與缺值、圖表共用尺度及含年份的 UTC 日期時間；假資料與情境只存在於此目錄。

專案選擇涵蓋 Admin 專案清單的分頁與格式驗證、未命名／已封存專案、讀取權限獨立，以及 7／30 天用量與成本的專案篩選、分頁保留篩選、編碼與切回組織範圍。
