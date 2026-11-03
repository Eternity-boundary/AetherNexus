# 架構與擴充規則

PlaygroundPage 由 MainWindow 注入唯讀與生成傳輸介面，各頁實例保留本次執行期間的記憶體狀態。GenerationClient／WorkflowService 處理生成合約及流程，WindowsGenerationTransport 提供正式 HTTPS／WebSocket。獨立 UI 宿主只編譯介面與虛擬實作，不編譯真實傳輸或憑證儲存，詳見 [playground.md](playground.md)。

採用單一 WinUI 3 C++/WinRT 應用程式專案，以資料夾劃分 MVVM 責任。保留既有套件、應用程式識別及 MSIX 包裝方式。

- **App**：生命週期、主視窗建立，以及未來服務實例的組合入口。
- **Views**：XAML 與 code-behind；只處理視覺互動、導覽、對話方塊與視窗行為。
- **ViewModels**：呈現狀態、命令與服務協調。C++/WinRT 可使用 IDL runtimeclass 與 `INotifyPropertyChanged`，不使用 C# 專屬的 MVVM source generator。
- **Models**：與 UI 無關的資料結構與值型別。
- **Services**：未來的 HTTP API、驗證、用量與本機儲存服務。由介面隔離外部依賴，避免 View 直接呼叫 HTTP。
- **Infrastructure**：Windows 認證儲存、檔案系統與平台橋接。金鑰不得存入原始碼或明文設定檔。
- **Resources/Styles**：共用 XAML 字典，經由 App.xaml 合併；色彩優先使用系統 ThemeResource。
- **Strings**：以語系資料夾放置 `Resources.resw`，供未來 `x:Uid` 及資源載入器使用。
- **tests**：獨立原生測試專案、測試腳本與記憶體虛擬 API；不被主專案引用。

MainWindow 實體檔案位於 Views，但保留 `AetherNexus.MainWindow` runtimeclass 名稱；實體資料夾不強制對應 WinRT namespace，避免僅為整理目錄而變更公開型別名稱。

新增 C++、IDL、XAML 或 RESW 檔案時，須同步更新 `AetherNexus.vcxproj` 與 `.filters`，使用適合的 `ClCompile`、`ClInclude`、`Midl`、`Page` 或 `PRIResource` 項目。C++/WinRT 產生檔禁止手動編輯或提交。保持原生 NuGet `packages.config` 工作流程；升級套件時同步更新 vcxproj 的 imports 與版本。

MainWindow 路由至 DashboardPage、SettingsPage、PlaygroundPage、ManagePage 或 PlaceholderPage。DashboardViewModel 僅持有已解析的伺服器資料；DashboardPage 協調背景載入、呈現狀態與取消生命週期。PlatformApiClient 對接 IApiTransport，處理日期範圍、分頁與資料驗證；WindowsHttpTransport 是唯讀 HTTPS 通道。

ManagePage 的三個實例由 MainWindow 注入 IManagementTransport，協調篩選、取消、對話框及匯出。ManagementClient 負責受限端點、結構化寫入、分頁、計量單位與回應驗證；WindowsManagementTransport 僅向固定的官方 HTTPS 主機發送管理請求，讀取 Admin 憑證。清單使用不可變的 ManageItem、具型別 x:Bind 及原生 ListView 虛擬化，畫面不持有原始 JSON 或權杖。獨立測試宿主不編譯任何真實傳輸或 CredentialStore，詳見 [manage.md](manage.md)。

SettingsPage 負責設定操作協調；ThemeService 保存非敏感的主題偏好，CredentialStore 使用 Windows 認證管理員保存獨立的一般／Admin 權杖，不將秘密綁定至 ViewModel。詳細安全邊界見 [settings-security.md](settings-security.md)。

BudgetService 保存使用者自行設定的本機 USD 參考預算（整數美分），不與伺服器資料或權杖混用。虛擬傳輸與測試執行腳本只存在於 `tests/`。主專案沒有測試旗標、測試伺服器網址或假資料產生器；模型生成請求由 GenerationClient 處理，目前沒有資料庫。
