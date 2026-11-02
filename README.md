# AetherNexus

使用 **WinUI 3 + C++/WinRT + C++23** 的 Windows 原生用戶端。Dashboard 從伺服器讀取用量與成本；設定頁可切換主題、管理權杖與本機參考預算，其餘功能頁先提供佔位畫面。Dashboard 沒有內建示範資料或假值備援。

## 目錄

```text
AetherNexus.slnx
build/                        共用原生 MSBuild 設定
scripts/                      建置工具
docs/                         架構說明
tests/                        獨立測試、虛擬 API 與測試腳本
src/AetherNexus/
  App.xaml[.h/.cpp]            應用程式生命週期與組合入口
  Views/                      Window、Page 與視覺互動
  ViewModels/                 呈現狀態與命令
  Models/                     領域資料模型
  Services/                   API、驗證與儲存服務
  Infrastructure/             Windows 平台整合與共用基礎設施
  Resources/Styles/           共用樣式與資源字典
  Strings/zh-TW/              繁體中文本地化資源
  Assets/                     MSIX 圖像資產
  Package.appxmanifest        單專案 MSIX 資訊
  packages.config             固定版本的原生 NuGet 相依套件
packages/                     NuGet 還原快取（不納入版本控制）
artifacts/                    建置結果（不納入版本控制）
```

## 建置

需要 Visual Studio（支援 `.slnx` 的版本）、MSVC v143/v145、Windows SDK，以及 WinUI C++ 開發元件。此專案沿用既有 Windows App SDK 2.6.4 與 C++/WinRT 套件版本。

以 Visual Studio 開啟 `AetherNexus.slnx`，選擇 **x64 / Debug**，設定 AetherNexus 為啟始專案，再建置及部署。MSIX 本機部署需要開發人員模式及適當的簽署設定；命令列腳本僅建置，不簽署或部署套件。

從儲存庫根目錄使用 PowerShell 7：

```powershell
pwsh -NoProfile -File ./scripts/Build.ps1 -Restore
pwsh -NoProfile -File ./scripts/Build.ps1 -Configuration Release -Platform x64
pwsh -NoProfile -File ./scripts/Build.ps1 -Platform ARM64
pwsh -NoProfile -File ./scripts/Run.ps1
```

`Run.ps1` 透過 WinApp CLI 註冊並啟動本機開發版本，需要已啟用開發人員模式。它使用建置產生的 `AppxManifest.xml` 以保留框架相依資訊。`-DebugOutput` 可擷取啟動診斷，並持續附加至執行中的程式。

輸出位於 `artifacts/bin/AetherNexus/<Platform>/<Configuration>/`，中間檔與 C++/WinRT、XAML 產生檔位於 `artifacts/obj/`。原有根目錄的 `.vs`、`x64`、`Generated Files` 等本機快取予以保留，新建置不再使用。

架構與新增檔案規則請參閱 [docs/architecture.md](docs/architecture.md)。
Dashboard 欄位、資料範圍與驗證紀錄請參閱 [docs/dashboard.md](docs/dashboard.md)。
設定頁已提供主題切換及一般／Admin 權杖的 Windows 認證管理員儲存；操作、安全邊界與測試方式見 [docs/settings-security.md](docs/settings-security.md)。

Dashboard 需要具備組織用量／成本讀取權限的 Admin Key；缺少或無效時會提供設定指引。一般 API Key 用於模型端點，不替代 Admin Key。啟動正式程式會讀取伺服器；偵錯 API 請執行 `pwsh -NoProfile -File ./tests/Test-PlatformApi.ps1`，只使用記憶體內的虛擬回應。詳見 [docs/api-client.md](docs/api-client.md)。
