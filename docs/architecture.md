# 架構與擴充規則

採用單一 WinUI 3 C++/WinRT 應用程式專案，以資料夾劃分 MVVM 責任。保留既有套件、應用程式識別及 MSIX 包裝方式。

- **App**：生命週期、主視窗建立，以及未來服務實例的組合入口。
- **Views**：XAML 與 code-behind；只處理視覺互動、導覽、對話方塊與視窗行為。
- **ViewModels**：呈現狀態、命令與服務協調。C++/WinRT 可使用 IDL runtimeclass 與 `INotifyPropertyChanged`，不使用 C# 專屬的 MVVM source generator。
- **Models**：與 UI 無關的資料結構與值型別。
- **Services**：未來的 HTTP API、驗證、用量與本機儲存服務。由介面隔離外部依賴，避免 View 直接呼叫 HTTP。
- **Infrastructure**：Windows 認證儲存、檔案系統與平台橋接。金鑰不得存入原始碼或明文設定檔。
- **Resources/Styles**：共用 XAML 字典，經由 App.xaml 合併；色彩優先使用系統 ThemeResource。
- **Strings**：以語系資料夾放置 `Resources.resw`，供未來 `x:Uid` 及資源載入器使用。
- **tests**：未來獨立測試專案位置，目前不宣稱已具備單元或 UI 測試。

MainWindow 實體檔案位於 Views，但保留 `AetherNexus.MainWindow` runtimeclass 名稱；實體資料夾不強制對應 WinRT namespace，避免僅為整理目錄而變更公開型別名稱。

新增 C++、IDL、XAML 或 RESW 檔案時，須同步更新 `AetherNexus.vcxproj` 與 `.filters`，使用適合的 `ClCompile`、`ClInclude`、`Midl`、`Page` 或 `PRIResource` 項目。C++/WinRT 產生檔禁止手動編輯或提交。保持原生 NuGet `packages.config` 工作流程；升級套件時同步更新 vcxproj 的 imports 與版本。

MainWindow 使用 NavigationView 與 Frame，路由至 DashboardPage 或共用的 PlaceholderPage。DashboardViewModel 是不依賴 UI 的 C++ 呈現模型，產生與彙整離線預覽資料；View 僅負責顯示、圖表繪製與響應式排版。此階段採用事件觸發的整體更新，沒有需要 INotifyPropertyChanged 的長期非同步狀態。

SettingsPage 負責設定操作協調；ThemeService 保存非敏感的主題偏好，CredentialStore 使用 Windows 認證管理員保存獨立的一般／Admin 權杖，不將秘密綁定至 ViewModel。詳細安全邊界見 [settings-security.md](settings-security.md)。

目前不實作 OpenAI 請求或資料庫；後續按功能逐步加入服務。
