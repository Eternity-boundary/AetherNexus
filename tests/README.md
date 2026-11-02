# Tests

`SettingsStorage.Tests.vcxproj` 是獨立原生整合測試，使用隔離的 Windows 認證命名空間與假權杖驗證儲存、覆寫、跨程序保存及移除，不操作正式權杖。

使用 `pwsh -NoProfile -File ./scripts/Test-SettingsStorage.ps1` 執行。詳見 [設定與安全說明](../docs/settings-security.md)。
