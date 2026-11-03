# Playground

五個導航入口使用同一個原生 WinUI 3 工作台元件，各自保留本次程式執行期間的頁面、對話與參數。沒有 WebView 包裝 Platform，也不使用非公開 Platform 後端。介面提供工作區、輸出、回應 JSON、程式碼及可收合的參數面板。

## 功能範圍

| 頁面 | 已提供的功能 |
| --- | --- |
| Chat | 一般金鑰取得模型清單、手動模型 ID、可編輯多輪訊息與角色、Developer instructions、Responses SSE 串流、停止、圖片／PDF 附件、Token／Temperature／Reasoning 參數、Web search、進階 JSON（包括結構化輸出與工具設定）、複製、工作階段匯入／匯出、Python 呼叫程式碼。 |
| Images | 文字生成、參考圖片編輯、最多 16 張參考圖片、尺寸／品質／背景／格式／張數、原始輸出預覽與下載、進階 API 參數（可指定 mask／markup 引用）、工作階段與程式碼匯出。 |
| Audio | Realtime 文字／錄音輸入及語音回覆、多輪語音上下文、聲線與指示、原生錄音、音檔轉錄、英文翻譯與舊版 Speech 相容工作流程、播放及儲存。Realtime 採逐次送出、回覆完成後播放，沒有宣稱支援連續收音、即時播放或語音打斷。 |
| Agent Builder | 本機 Start／Agent／Condition／Transform／End 圖形編輯、節點位置拖曳及方向鍵移動、節點參數與連線、JSON 匯入／匯出、執行預覽與逐步 trace、Agents SDK Python 匯出。沒有官方雲端工作流程的發布、同步、版本 ID 或雲端評估服務，也不冒充完整的官方節點集合。 |
| Video | 依官方停用狀態明確關閉生成；提供本機分鏡 JSON、加入分鏡、匯入／匯出及既有影片預覽，不呼叫已關閉的 Videos API。 |

這是公開 API 能力的原生實作，並非官方 Platform 全功能的逐項複製。自訂 function 工具不會在本機自動執行任意程式碼；回應 JSON 可用來檢查呼叫。Images 不提供繪圖式遮罩編輯器，遮罩可透過進階 API 參數指定。程式碼匯出不含金鑰，執行匯出腳本需要另外安裝對應的官方 Python SDK。

## 官方可用性（2026-11-03 比對）

- [Responses 與文字生成](https://developers.openai.com/api/docs/guides/text)、[串流](https://developers.openai.com/api/docs/guides/streaming-responses)、[圖片生成與編輯](https://developers.openai.com/api/docs/guides/image-generation)、[音檔轉錄](https://developers.openai.com/api/docs/guides/speech-to-text)、[Realtime](https://developers.openai.com/api/docs/guides/realtime-conversations) 是目前功能的公開 API 來源。
- [官方停用時程](https://developers.openai.com/api/docs/deprecations)：Videos API／Sora 2 於 **2026-09-24** 關閉，沒有直接替代 API；Agent Builder 預定 **2026-11-30** 關閉。
- Speech 相容模式及其模型預定 **2027-01-06** 關閉；翻譯與舊轉錄模型預定 **2027-02-26** 關閉。介面標示相容模式與日期；預設語音使用 Realtime，預設轉錄使用 `gpt-transcribe`，不把 Realtime 模型錯送到 Speech 端點。

## 資料、安全與生命週期

- 所有生成使用設定頁的一般 API Key。Admin Key 不會用於生成，Dashboard 的組織／專案篩選不會改變一般金鑰的所屬專案。
- 進入頁面／重新整理模型會發出唯讀模型清單請求；只有明確按下「執行」才傳送該頁的提示詞、對話、附件或工作流程輸入。Chat 預設 `store=false`，進階參數中明確指定 `store=true` 才會改變此設定。
- 對話、錄音、圖片與輸出預設留在程序記憶體，不自動寫入檔案或設定。匯出工作階段包含使用者選擇的內容與附件；只有使用者選擇儲存位置後才寫入。
- WindowsGenerationTransport 固定使用 `api.openai.com:443`、TLS 憑證驗證、限定的 API 路徑與一般金鑰；關閉重新導向、Cookie、自動驗證，不自動重送 POST。金鑰暫存緩衝區送出後清除，不記錄請求、金鑰或私有錯誤 body。
- SSE 按完整事件解析，處理跨 UTF-8／CRLF／JSON 邊界；只有收到最終事件才視為完成。取消會關閉傳輸 handle，離開頁面會取消並忽略舊結果。已開始處理的伺服器工作仍可能計入用量。
- 單一附件最多 25 MiB，附件總量與歷史輸入有記憶體上限；圖片參考最多 16 張、輸出最多 10 張。Realtime 錄音使用 24 kHz、mono、16-bit PCM WAV，單次錄音 3 分鐘自動結束；錄音不會自動上傳。
- 錄音需要 Windows 麥克風權限。離開頁面會停止並釋放錄音資源，不在背景繼續收音。
- 工作流程驗證節點 ID、連線及類型；執行最多 64 步。條件採文字包含判斷，轉換只支援 `{{input}}` 代入，不使用 eval 或本機任意程式碼。匯出 Python 將資料編碼為 JSON／base64，避免使用者文字成為程式碼。

## 架構與相依套件

`GenerationClient` 負責請求、SSE、圖片／音訊及輸出解析；`WorkflowService` 負責流程驗證、執行與匯出；`WindowsGenerationTransport` 負責 HTTPS／WebSocket。MainWindow 組合正式通道，再注入 PlaygroundPage，頁面本身不接觸 CredentialStore。

本次沒有新增第三方原生相依套件或 vcpkg 套件，使用既有 Windows App SDK、WinHTTP、Windows Storage／Media API。新增程式碼採專案的 `GPL-3.0-only`。匯出的範例腳本所需官方 Python SDK 不隨應用程式打包。

## 驗證

```powershell
./tests/Test-Playground.ps1
./tests/Test-PlatformApi.ps1
./scripts/Build.ps1
./scripts/Build.ps1 -Configuration Release
```

Playground 合約測試只連結正式解析與流程程式碼及測試內的虛擬通道，不連結憑證儲存、真實傳輸或麥克風。涵蓋串流、Unicode、截斷、錯誤、取消、圖片生成／編輯、Multipart、音訊事件、WAV、語音上下文、工作流程分支／上限及程式碼匯出。

原生 UI 離線宿主位於 `tests/PlaygroundHarness`，生成的專案與 manifest 不納入版本控制：

```powershell
./tests/New-PlaygroundUiHarness.ps1
MSBuild.exe ./tests/PlaygroundHarness/Playground.UI.Tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:AppxPackageSigningEnabled=false
winapp run ./artifacts/bin/Playground.UI.Tests/x64/Debug --manifest ./artifacts/bin/Playground.UI.Tests/x64/Debug/AppxManifest.xml --exe AetherNexus.exe --detach --json
./tests/Test-PlaygroundUi.ps1 -AppPid <離線宿主的 ProcessId>
```

離線宿主不編譯 CredentialStore、WindowsHttpTransport 或 WindowsGenerationTransport。六項主要 UI 測試涵蓋五頁操作與導航保留對話；另已檢查淺／深色主題、窄視窗參數覆蓋面板、滑鼠節點拖曳與連線更新。最終圖片預覽及原生儲存對話框已驗證，儲存至新檔的內容與虛擬 API 回傳的原始位元組完全一致。結果、補充檢查與截圖位於 `artifacts/tests/playground-ui`。x64 Debug／Release 建置均為零警告、零錯誤；
