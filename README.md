# AetherNexus

**以原生 Windows 體驗管理 OpenAI Platform。**

![Windows](https://img.shields.io/badge/Windows-10%20%2F%2011-0078D4?logo=windows)
![WinUI](https://img.shields.io/badge/UI-WinUI%203-5C2D91)
![C++](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus)
![License](https://img.shields.io/badge/license-GPL%20v3-blue)

AetherNexus 使用 WinUI 3、C++/WinRT 與 Windows App SDK，將用量檢視、模型實驗和組織管理整合到桌面工作區.

[快速開始](#快速開始) · [功能](#功能) · [遷移指南](#遷移指南) · [開發與測試](#開發與測試) · [文件](#文件)

## 功能

| 工作區 | 能做什麼 |
| --- | --- |
| **Dashboard** | 用量與成本卡片、趨勢圖、專案切換，以及設定頁中的本機預算參考值。 |
| **Playground** | Chat、Images、Audio；本機 Agent Builder 工作流程，以及 Video 分鏡與既有影片預覽。 |
| **BUILD** | 本機 Prompt 版本與變數、Responses 測試、Stored Chat logs、Agent traces、批次、微調及 Files／向量庫管理。 |
| **遷移工作台** | Assistants 設定轉為本機 Prompt；Evals 既有結果唯讀檢視、匯出與本機開啟。 |
| **MANAGE** | Usage、API keys、Projects；用量圖表、金鑰簽發與撤銷、專案成員及模型速率限制。 |
| **設定** | 系統／淺色／深色主題、一般 API Key／Admin Key 安全儲存、本機預算參考值。 |

功能依官方公開 API、帳戶資格與權杖權限提供，並非官方網站所有功能的完整複製。Logs 只列出已儲存的 Chat Completions；Response 需以已知 ID 查詢。Video 不再呼叫已停用的生成 API。

## 快速開始

### 1. 準備開發環境

- Visual Studio 與 C++ 桌面／WinUI 開發元件、MSVC v143 或 v145、Windows SDK。
- 支援 `.slnx` 的 Visual Studio 版本，或使用下方 MSBuild 腳本。
- PowerShell 7；啟動本機套件另需 WinApp CLI 與 Windows 開發人員模式。
- 使用專案固定的 NuGet 套件版本；目前 Windows App SDK 為 2.6.4。

### 2. 建置與啟動

在儲存庫根目錄執行：

```powershell
pwsh -NoProfile -File ./scripts/Build.ps1 -Restore
pwsh -NoProfile -File ./scripts/Run.ps1
```

也可用 Visual Studio 開啟 `AetherNexus.slnx`，選擇 **x64 / Debug**，將 AetherNexus 設為啟始專案後建置及部署。腳本建置預設不簽署套件；部署仍須符合本機簽署與開發人員模式要求。

```powershell
# 其他建置組態
./scripts/Build.ps1 -Configuration Release -Platform x64
./scripts/Build.ps1 -Platform ARM64
```

### 3. 設定權杖

開啟 **設定 → 權杖管理**，儲存需要的權杖：

| 權杖 | 使用範圍 |
| --- | --- |
| 一般 API Key | 模型生成、Playground 與 BUILD；操作範圍取決於其專案與權限。 |
| Admin Key | Dashboard 的組織用量／成本，以及 MANAGE 組織管理 API。 |

Admin Key 不替代一般模型權杖。首次啟動正式程式可能讀取伺服器資料；需要離線偵錯時，請使用 `tests/` 中的隔離宿主。

## 遷移指南

受停用影響的頁面在**標題下方**提供醒目的「遷移指南」按鈕。指南可在程式內閱讀，包含三個簡易步驟與官方文件入口；開啟指南不會讀取權杖、上傳資料或自動更改設定。

| 頁面 | 需要留意的變動 |
| --- | --- |
| Prompts | 官方可重用 Prompt 物件預定 2026-11-30 停用；本機版本庫保留。 |
| Evaluations | 官方 Evals 已唯讀，預定 2026-11-30 停用；請先保存資料與評分規則。 |
| Agent Builder | 官方託管產品預定 2026-11-30 停用；本頁本機流程可繼續匯出。 |
| Fine-tuning | 2027-01-06 停止建立新工作；既有模型推論仍依基礎模型生命週期處理。 |
| Assistants／Video | 舊 API 已停用，提供本機遷移／資料保存指引。 |
| Chat／Images／Audio | 頁面保留；指南協助更換受影響的模型或相容模式。 |

以上依 **2026-11-05** 官方文件核對；時程變動請以 [OpenAI 停用公告](https://developers.openai.com/api/docs/deprecations) 為準。開發時需同步維護程式內的指南內容與相關 API 政策。

## 安全與資料

- 權杖使用 Windows 認證管理員儲存，不放在設定檔或匯出文件中。
- 簽發後的完整權杖僅在一次性彈窗展示；關閉後清除可控的敏感記憶體緩衝區。
- 正式傳輸固定使用官方 HTTPS 端點，保留 TLS 驗證；寫入操作不自動重送。
- 刪除／撤銷需要確認；網路錯誤或停止等待不代表伺服器操作已取消。
- 本機 Prompt 是使用者明確儲存的明文文件；匯出資料可能包含私人輸入與結果，請自行選擇保存位置。

詳見 [設定與權杖安全](docs/settings-security.md)、[BUILD 資料與生命週期](docs/build.md) 及 [MANAGE 說明](docs/manage.md)。

## 開發與測試

```text
src/AetherNexus/
  Views/            WinUI 頁面、圖表與遷移指南
  ViewModels/       呈現狀態與命令
  Models/           資料模型
  Services/         API 合約、傳輸及本機文件儲存
  Infrastructure/   Windows 憑證與安全緩衝區
  Resources/        共用樣式
  Strings/          本地化資源
tests/              合約測試、假資料及隔離 UI 宿主
scripts/            建置／啟動工具
docs/               功能、安全與架構文件
```

一般偵錯與測試：

```powershell
./tests/Test-PlatformApi.ps1
./tests/Test-Playground.ps1
./tests/Test-Management.ps1
./tests/Test-Build.ps1
```

原生傳輸邊界測試攔截 WinHTTP 與憑證讀取；UI 測試以獨立宿主注入假通道。各宿主的建置、啟動與測試指令見 [tests/README.md](tests/README.md)。離線測試通過不代表已使用真實帳戶驗證服務可用性。手動、明確授權的真實帳戶診斷工具不屬於一般測試。

建置保留 `/Wall` 與 `/WX`；已知外部標頭及最佳化提示的排除設定由專案管理。第三方原生依賴須與 GPL v3 相容，需要新增時使用 vcpkg；目前 BUILD 與遷移指南沒有新增第三方函式庫。

## 文件
### 文件由AI產生，可能與程式碼不同步

| 主題 | 文件 |
| --- | --- |
| Dashboard 語義與圖表 | [dashboard.md](docs/dashboard.md) |
| Playground 功能與限制 | [playground.md](docs/playground.md) |
| BUILD 功能與停用時程 | [build.md](docs/build.md) |
| 管理與金鑰操作 | [manage.md](docs/manage.md) |
| API 用戶端架構 | [api-client.md](docs/api-client.md) |
| 設定與安全 | [settings-security.md](docs/settings-security.md) |

## 授權

本專案採 **GNU GPL v3**，完整條款見 [LICENSE.txt](LICENSE.txt)；新增程式碼標示 `GPL-3.0-only`。AetherNexus 為獨立開發的用戶端，並非 OpenAI 官方產品。
