# Tests

`SettingsStorage.Tests.vcxproj` 是獨立原生整合測試，使用隔離的 Windows 認證命名空間與假權杖驗證儲存、覆寫、跨程序保存及移除，不操作正式權杖。

使用 `pwsh -NoProfile -File ./tests/Test-SettingsStorage.ps1` 執行。詳見 [設定與安全說明](../docs/settings-security.md)。

`PlatformApi.Tests.vcxproj` 測試正式 API 用戶端的請求、分頁、解析與加總。其唯一通道是 `Support/MockApiTransport`，不連結 WindowsHttpTransport 或認證儲存。執行 `pwsh -NoProfile -File ./tests/Test-PlatformApi.ps1`，不會發出網路請求或讀取真實權杖；也涵蓋本機預算金額的數值驗證。
