// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: MigrationGuide.h
// Project: AetherNexus
#pragma once
#include <string>
#include <vector>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Text.h>
namespace AetherNexus::Views
{
    struct MigrationGuide
    {
        std::wstring title, status, impact, url;
        std::vector<std::wstring> steps;
    };
    inline MigrationGuide GuideFor(std::wstring const &mode)
    {
        std::wstring const docs = L"https://developers.openai.com/api/docs/";
        if (mode == L"prompts") return {L"Prompts 遷移指南", L"2026-11-30 · 雲端 Prompt 物件停用",
            L"本機 Prompt 版本庫可繼續使用；需要搬移的是官方可重用 Prompt 物件。", docs+L"guides/prompting/migrate-from-prompt-object",
            {L"在停用前保存雲端 Prompt 的 instructions、輸入範本與變數定義。", L"在本頁建立本機 Prompt，填入模型 ID 與範本；以 {{name}} 保留變數並儲存版本。",
             L"用代表性輸入執行 Responses 測試；確認結果後，將應用程式的 prompt ID 呼叫改為直接傳入 instructions 與 input。"}};
        if (mode == L"evaluations") return {L"Evaluations 遷移指南", L"目前唯讀 · 2026-11-30 停用",
            L"既有評估需在停用前保存；本機 JSON 檢視與匯出仍可使用。",
            L"https://developers.openai.com/cookbook/examples/evaluation/moving-from-openai-evals-to-promptfoo",
            {L"逐頁載入並匯出需要的 eval、runs 與 output items。匯出清單只包含已載入的資料。", L"另外保存資料集、評分規則、模型參數與比較基準；單一結果 JSON 不等同完整評估專案。",
             L"依官方指南將測試案例及評分規則轉為 Promptfoo 設定，在自己的環境重跑並比較結果；本程式不會自動安裝或執行外部工具。"}};
        if (mode == L"fine-tuning") return {L"Fine-tuning 遷移指南", L"2027-01-06 · 停止建立新工作",
            L"既有微調模型的推論不會在此日期一併停止，仍受其基礎模型生命週期影響。", docs+L"deprecations",
            {L"保存訓練與驗證資料、工作參數、結果檔案及模型 ID。", L"以本機 Prompt、檢索增強或其他符合需求的方案建立比較版本；沒有保證等價的直接替代訓練 API。",
             L"用固定測試集比較品質、延遲與成本，再更新應用程式模型設定；持續追蹤既有模型所依附的基礎模型停用時間。"}};
        if (mode == L"assistants") return {L"Assistants 遷移指南", L"已於 2026-08-26 停用",
            L"此頁是本機遷移工作台，不會呼叫舊 Assistants API。", docs+L"assistants/migration",
            {L"準備先前匯出的 Assistant 設定；匯入本頁可帶入名稱、模型與 instructions。", L"另行保存並重建工具、檔案資源與對話狀態；這些內容不會隨 Prompt 匯入而自動轉換。",
             L"以 Responses API 重建生成邏輯，需要持久對話時依官方指南使用 Conversations API；驗證工具權限與結果後再切換。"}};
        if (mode == L"agent-builder") return {L"Agent Builder 遷移指南", L"2026-11-30 · 官方 Agent Builder 停用",
            L"本頁的本機工作流程仍可編輯與匯出，與官方託管的 Agent Builder 分開。", docs+L"guides/agent-builder/migrate-from-agent-builder",
            {L"先匯出本頁工作流程 JSON／Python；官方工作流程需另依官方介面匯出。兩者格式不保證相容。", L"依指南選擇 Agents SDK 或 ChatGPT Workspace Agents，逐一重建節點、分支、工具與狀態。",
             L"以原有測試輸入核對路由及工具行為，另行設定執行環境與權杖；本機匯出的 Python 不代表已完成部署。"}};
        if (mode == L"video") return {L"Video 遷移指南", L"已於 2026-09-24 停用",
            L"官方 Videos API 沒有列出直接替代 API；本頁僅保留本機分鏡及既有影片預覽。", docs+L"deprecations",
            {L"保存既有影片與分鏡資料，使用本頁匯出分鏡 JSON。", L"若需要新的影片生成，另行評估可用服務的授權、資料處理及介面。",
             L"保留服務獨立的分鏡描述，再依選定服務重新實作生成與下載流程；不要繼續重試已停用端點。"}};
        if (mode == L"audio") return {L"Audio 相容模式遷移指南", L"部分舊音訊 API／模型將停用",
            L"Audio 頁面會保留；請依官方時程逐一核對舊 Speech、翻譯與轉錄模式。", docs+L"deprecations",
            {L"匯出需要保留的音訊與工作階段，記下目前模型、語言、取樣率及輸出格式。", L"語音互動改用本頁的 Realtime 模式；檔案轉錄選擇受支援的轉錄模型。舊端點的參數不能直接套用到新端點。",
             L"依官方遷移文件重建翻譯等工作流程，再以相同音訊核對語言、文字內容與播放格式。"}};
        if (mode == L"images" || mode == L"chat") return {mode == L"images" ? L"Images 模型遷移指南" : L"Chat 模型遷移指南",
            L"部分舊模型即將停用 · 頁面本身保留", L"停用時間依模型而異，請以目前使用的完整模型 ID 核對官方清單。", docs+L"deprecations",
            {L"匯出目前工作階段，保存模型 ID、輸入與進階參數。", L"在模型清單選擇可用的替代模型，逐一核對其支援的參數與輸入格式。",
             L"以代表性案例比較輸出與成本，再更新設定；不要只更換模型名稱而保留不支援的參數。"}};
        return {};
    }
    inline winrt::Windows::Foundation::IAsyncAction ShowMigrationGuide(winrt::Microsoft::UI::Xaml::XamlRoot root, std::wstring mode)
    {
        using namespace winrt;
        using namespace Microsoft::UI::Xaml;
        using namespace Controls;
        auto guide = GuideFor(mode);
        if (guide.title.empty()) co_return;
        StackPanel panel; panel.Spacing(16);
        auto add = [&](std::wstring const &text, bool strong = false) {
            TextBlock block; block.Text(text); block.TextWrapping(TextWrapping::Wrap);
            if (strong) block.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
            panel.Children().Append(block);
        };
        add(guide.status, true); add(guide.impact);
        unsigned index{};
        for (auto const &step : guide.steps) add(std::to_wstring(++index)+L". "+step);
        add(L"文件核對：2026-11-05。開啟指南不會傳送資料或變更設定；執行模型測試須另外確認，可能產生費用。");
        HyperlinkButton link; link.Content(box_value(L"開啟官方遷移文件")); link.NavigateUri(Windows::Foundation::Uri{guide.url});
        panel.Children().Append(link);
        ScrollViewer scroll; scroll.Content(panel); scroll.MaxHeight(520); scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        ContentDialog dialog; dialog.XamlRoot(root); dialog.RequestedTheme(root.Content().as<FrameworkElement>().ActualTheme());
        dialog.Title(box_value(hstring{guide.title})); dialog.Content(scroll); dialog.CloseButtonText(L"關閉");
        co_await dialog.ShowAsync();
    }
}
