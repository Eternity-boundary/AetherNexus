# 設定與權杖儲存

## 操作

從左下角「設定」進入。主題提供跟隨系統、淺色及深色，即時套用整個主視窗、標題列與導航；偏好保存至 ApplicationData.LocalSettings 的 `Appearance.Theme`。選擇跟隨系統後，程式監聽 UISettings 色彩變更並回到系統偏好。Windows 對比主題由平台優先處理。

一般 API Key 與 Admin Key 使用獨立儲存槽，分別支援新增、取代與移除。已存權杖不會回填、顯示片段或提供複製功能。取代與移除前顯示確認對話框，預設取消。移除只刪除本機副本，不撤銷遠端金鑰。此設定頁不會額外發出驗證請求或建立 OpenAI 金鑰，儲存狀態明示「尚未驗證」；Dashboard 會在載入資料時使用已存的 Admin Key。

本機每月參考預算預設停用；啟用後可設定 0.01 至 1,000,000,000 USD、最多兩位小數，按下「儲存預算設定」才保存。BudgetService 使用整數美分存於 LocalSettings 的 `Budget.MonthlyUsdCents`；它不會修改 OpenAI 的預算／額度，停用並儲存則移除本機設定。

## 安全邊界

- 使用 Windows Credential Manager 的 `CRED_TYPE_GENERIC`，由 Windows 保護持久化內容。目標名稱使用應用程式識別、版本及權杖用途，分隔一般／Admin Key。
- `CRED_PERSIST_LOCAL_MACHINE` 表示同一 Windows 使用者、同一電腦，可跨登入工作階段；不是讓所有本機使用者共用，也不採用企業漫遊保存。
- 主題偏好與權杖完全分離。權杖不寫入 `.env`、JSON、LocalSettings、專案檔或明文備援。Windows 儲存不可用時直接回報失敗，不降級儲存。
- PasswordBox 固定隱藏字元，不自動回填舊值。提交嘗試與離開頁面時清除輸入；跨非同步作業只持有不可複製的暫存緩衝區，保存後及解構時用 SecureZeroMemory 清除。CredRead 的作業緩衝區亦在 CredFree 前清除。
- 只接受非空、無空白／控制字元的 ASCII，長度限制對應 Windows CredentialBlob 大小，不猜測或依賴特定金鑰前綴。這只是輸入檢查，不能證明金鑰類型、權限或有效性。
- UI 狀態與錯誤不顯示權杖全文、首尾片段或例外原文。全域 Debug 未處理例外訊息改為固定文字，避免意外記錄輸入。

這是**保護靜態儲存、降低意外洩漏**的設計，不是絕對安全保證。同一 Windows 使用者下的其他程序、遭入侵的使用者工作階段、管理員／系統層攻擊仍可取得秘密。PasswordBox／WinRT 及作業系統會有自身的記憶體副本；程式無法保證清除所有暫存、交換檔、診斷傾印或使用者剪貼簿。應使用所需最小權限的金鑰，且避免在不可信的 Windows 帳戶中存放 Admin Key。

## 驗證

```powershell
pwsh -NoProfile -File ./tests/Test-SettingsStorage.ps1
```

測試使用 `AetherNexus.Tests/<程序識別與時間>/` 獨立命名空間及明確的人工假權杖，結束後清理，不讀寫正式權杖槽。涵蓋：兩槽分離、Windows 保存層級、子程序讀回、單槽覆寫、拒絕空白／換行／NUL／非 ASCII／超長輸入且保持舊值、單槽刪除及重複刪除。沒有使用真實帳戶或發送 API 請求。

參考：[CredWriteW](https://learn.microsoft.com/en-us/windows/win32/api/wincred/nf-wincred-credwritew)、[CREDENTIALW 保存範圍與大小限制](https://learn.microsoft.com/en-us/windows/win32/api/wincred/ns-wincred-credentialw)、[WinUI RequestedTheme](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.frameworkelement.requestedtheme)、[系統色彩變更](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes)。
