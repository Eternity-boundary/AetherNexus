# Tests

`Test-Playground.ps1` 執行獨立的生成與工作流程合約測試，涵蓋 SSE、Base64、PCM/WAV、圖片、Multipart、Realtime 與工作流程，不連結真實傳輸或憑證儲存。

`New-PlaygroundUiHarness.ps1` 從目前 WinUI 設定產生隔離的 UI 測試專案，排除所有真實傳輸與憑證實作。`Test-PlaygroundUi.ps1 -AppPid <離線宿主 PID>` 驗證五個頁面，不可指向正式程式。建置／啟動方式及限制見 [Playground 說明](../docs/playground.md)。

`SettingsStorage.Tests.vcxproj` 是獨立原生整合測試，使用隔離的 Windows 認證命名空間與假權杖驗證儲存、覆寫、跨程序保存及移除，不操作正式權杖。

使用 `pwsh -NoProfile -File ./tests/Test-SettingsStorage.ps1` 執行。詳見 [設定與安全說明](../docs/settings-security.md)。

`PlatformApi.Tests.vcxproj` 測試正式 API 用戶端的請求、分頁、解析與加總。其唯一通道是 `Support/MockApiTransport`，不連結 WindowsHttpTransport 或認證儲存。執行 `pwsh -NoProfile -File ./tests/Test-PlatformApi.ps1`，不會發出網路請求或讀取真實權杖；也涵蓋本機預算金額的數值驗證。

同一測試專案亦編譯正式 DashboardViewModel，驗證官方欄位的計數語義、加權快取比例、零分母與缺值、圖表共用尺度及含年份的 UTC 日期時間；假資料與情境只存在於此目錄。

專案選擇涵蓋 Admin 專案清單的分頁與格式驗證、未命名／已封存專案、讀取權限獨立，以及 7／30 天用量與成本的專案篩選、分頁保留篩選、編碼與切回組織範圍。
