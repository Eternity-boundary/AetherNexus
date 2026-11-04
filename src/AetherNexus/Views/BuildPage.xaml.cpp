// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: BuildPage.xaml.cpp
// Project: AetherNexus
#include "pch.h"
#include "BuildPage.xaml.h"
#include "Models/ManageItem.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.h>
#if __has_include("BuildPage.g.cpp")
#include "BuildPage.g.cpp"
#endif
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;
using namespace ::AetherNexus::Services;
namespace Pickers = Microsoft::Windows::Storage::Pickers;
namespace winrt::AetherNexus::implementation
{
namespace
{
Visibility Visible(bool value)
{
    return value ? Visibility::Visible : Visibility::Collapsed;
}
TextBox Field(StackPanel const &panel, wchar_t const *label, wchar_t const *id, std::wstring const &initial = L"",
              bool multiline = false)
{
    TextBox box;
    box.Header(box_value(hstring{label}));
    box.Text(initial);
    box.MaxLength(multiline ? 65536 : 256);
    box.AcceptsReturn(multiline);
    box.TextWrapping(TextWrapping::Wrap);
    if (multiline)
    {
        box.MinHeight(80);
        box.MaxHeight(180);
    }
    Automation::AutomationProperties::SetAutomationId(box, id);
    panel.Children().Append(box);
    return box;
}
ComboBox Choice(StackPanel const &panel, wchar_t const *label, wchar_t const *id,
                std::initializer_list<wchar_t const *> values)
{
    ComboBox box;
    box.Header(box_value(hstring{label}));
    box.HorizontalAlignment(HorizontalAlignment::Stretch);
    for (auto value : values)
        box.Items().Append(box_value(hstring{value}));
    box.SelectedIndex(0);
    Automation::AutomationProperties::SetAutomationId(box, id);
    panel.Children().Append(box);
    return box;
}
std::wstring Value(ComboBox const &box)
{
    return std::wstring{unbox_value<hstring>(box.SelectedItem())};
}
ContentDialog Dialog(XamlRoot const &root, wchar_t const *title, StackPanel const &fields, wchar_t const *verb)
{
    ContentDialog dialog;
    dialog.XamlRoot(root);
    dialog.Title(box_value(hstring{title}));
    dialog.PrimaryButtonText(verb);
    dialog.CloseButtonText(L"取消");
    dialog.DefaultButton(ContentDialogButton::Close);
    ScrollViewer scroll;
    scroll.Content(fields);
    scroll.MaxHeight(520);
    dialog.Content(scroll);
    return dialog;
}
void Note(StackPanel const &panel, wchar_t const *text)
{
    TextBlock block;
    block.Text(text);
    block.TextWrapping(TextWrapping::Wrap);
    panel.Children().Append(block);
}
int64_t Now()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}
} // namespace
bool BuildPage::Local() const
{
    return m_mode == L"prompts" || m_mode == L"assistants";
}
void BuildPage::Configure(hstring const &mode, std::shared_ptr<IBuildTransport> transport,
                          std::shared_ptr<IPromptStore> store)
{
    m_mode = mode;
    m_transport = std::move(transport);
    m_store = std::move(store);
    std::wstring title, description, doc = L"https://developers.openai.com/api/docs/";
    std::vector<std::wstring> names;
    if (Local())
    {
        title = m_mode == L"prompts" ? L"Prompts" : L"Assistants · 遷移工作台";
        description = m_mode == L"prompts" ? L"本機版本化 Prompt、變數範本與 Responses 測試。可重用 Prompt "
                                             L"API 將於 2026-11-30 停用。"
                                           : L"Assistants API 已於 2026-08-26 停用。匯入已匯出的名稱、模型及 "
                                             L"instructions，轉為本機 Prompt；工具與對話狀態需另行遷移。";
        doc += m_mode == L"prompts" ? L"guides/prompting" : L"assistants/migration";
        names = {L"本機 Prompt 版本庫"};
    }
    else if (m_mode == L"logs")
    {
        title = L"Logs";
        description = L"僅列出以 store: true 儲存的 Chat Completions。Responses "
                      L"須提供已知 ID；此處不是所有 API 請求的稽核日誌。";
        names = {L"已儲存 Chat Completions", L"Chat 訊息", L"依 ID 取得 Response", L"Response 輸入項目"};
        m_views = {BuildResource::Chats, BuildResource::ChatMessages, BuildResource::Responses,
                   BuildResource::ResponseItems};
        doc += L"guides/text";
    }
    else if (m_mode == L"traces")
    {
        title = L"Traces";
        description = L"Agent Sessions 與 OTLP spans。組織須啟用 Trace 匯出，權杖須有 "
                      L"api.traces.read 或 api.agents.read 權限。每頁最多 5 筆 Trace。";
        names = {L"Agent sessions", L"Session traces"};
        m_views = {BuildResource::Sessions, BuildResource::Traces};
        doc += L"guides/agents-api/tracing";
    }
    else if (m_mode == L"batches")
    {
        title = L"Batches";
        description = L"建立、監看及取消非同步批次工作；JSONL 檔案請先至 Storage "
                      L"以 batch 用途上傳。完成視窗為 24 小時。";
        names = {L"批次工作"};
        m_views = {BuildResource::Batches};
        doc += L"guides/batch";
    }
    else if (m_mode == L"evaluations")
    {
        title = L"Evaluations";
        description = L"Evals API 自 2026-10-31 起唯讀，2026-11-30 "
                      L"停用。檢視並匯出既有評估、執行結果及輸出；也可匯入先前匯出"
                      L"的 JSON 檢視。";
        names = {L"既有評估", L"評估執行", L"執行輸出"};
        m_views = {BuildResource::Evals, BuildResource::EvalRuns, BuildResource::EvalOutputs};
        doc += L"deprecations";
    }
    else if (m_mode == L"fine-tuning")
    {
        title = L"Fine-tuning";
        description = L"檢視訓練工作、事件與 "
                      L"checkpoints。建立限官方允許的既有活躍客戶；2027-01-06 "
                      L"起停止建立新工作。訓練資料請以 fine-tune 用途上傳。";
        names = {L"訓練工作", L"訓練事件", L"Checkpoints"};
        m_views = {BuildResource::FineJobs, BuildResource::FineEvents, BuildResource::FineCheckpoints};
        doc += L"guides/supervised-fine-tuning";
    }
    else if (m_mode == L"storage")
    {
        title = L"Storage";
        description = L"管理 "
                      L"Files、向量庫及檔案關聯。移除向量庫關聯不會刪除原始檔案；上"
                      L"傳及下載採串流處理。";
        names = {L"Files", L"Vector stores", L"Vector store files"};
        m_views = {BuildResource::Files, BuildResource::Stores, BuildResource::StoreFiles};
        doc += L"guides/retrieval";
    }
    else
        throw std::invalid_argument("Unknown Build page");
    Heading().Text(title);
    Description().Text(description);
    DocsLink().NavigateUri(Uri{doc});
    for (auto const &name : names)
        ViewPicker().Items().Append(box_value(hstring{name}));
    ViewPicker().SelectedIndex(0);
    PromptEditor().Visibility(Visible(Local()));
    ProjectBox().Visibility(Visible(!Local()));
    ParentBox().Visibility(Visibility::Collapsed);
    Grid::SetColumn(ProjectBox(), 1);
    Grid::SetColumnSpan(ProjectBox(), 2);
    if (Local())
        Grid::SetColumnSpan(ViewPicker(), 3);
    m_ready = true;
    Actions();
}
void BuildPage::NoticeText(std::wstring const &message, InfoBarSeverity severity)
{
    Notice().Message(message);
    Notice().Severity(severity);
    Notice().IsOpen(true);
    SettingsButton().Visibility(Visibility::Collapsed);
}
void BuildPage::Error(bool mutation)
{
    bool settings{};
    std::wstring message =
        mutation ? L"操作結果尚未確認。請重新整理核對，程式不會自動重送。" : L"無法完成操作，請檢查連線後重試。";
    try
    {
        throw;
    }
    catch (ApiError const &error)
    {
        if (error.code == ApiErrorCode::MissingCredential || error.code == ApiErrorCode::CredentialUnavailable ||
            error.code == ApiErrorCode::Unauthorized)
        {
            message = L"請先在設定頁儲存可用的一般 API 權杖。BUILD "
                      L"功能使用一般權杖的專案範圍。";
            settings = true;
        }
        else if (error.code == ApiErrorCode::Forbidden)
            message = L"目前權杖無權存取此資料，請核對專案、功能啟用狀態與權杖權限。";
        else if (!mutation && error.code == ApiErrorCode::Cancelled)
            message = L"已停止等待。";
        else if (!mutation && error.code == ApiErrorCode::RateLimited)
            message = L"已達速率或額度限制，請稍後再試。";
        else if (!mutation && error.code == ApiErrorCode::InvalidResponse)
            message = L"伺服器回應格式無效，未以示例資料取代。";
    }
    catch (BuildHttpError const &error)
    {
        message = error.status == 410 ? L"官方已停止此 API "
                                        L"操作。請參閱遷移說明，既有匯出檔仍可在本機檢視。"
                                      : L"伺服器拒絕操作（HTTP " + std::to_wstring(error.status) +
                                            L"）。請核對 ID、檔案用途及帳戶資格。";
    }
    catch (std::invalid_argument const &)
    {
        message = L"輸入資料無效，或本機版本已由其他編輯器更新。請檢查必填欄位、ID"
                  L"、JSON 與選定版本。";
    }
    catch (...)
    {
    }
    NoticeText(message, InfoBarSeverity::Warning);
    SettingsButton().Visibility(Visible(settings));
}
void BuildPage::Busy(bool value)
{
    m_busy = value;
    if (value)
        NoticeText(L"正在處理要求…");
    for (auto const &child : Filters().Children())
        if (auto control = child.try_as<Control>())
            control.IsEnabled(!value && !m_dialog);
    for (auto const &child : PromptEditor().Children())
    {
        if (auto control = child.try_as<Control>())
            control.IsEnabled(!value && !m_dialog);
        if (auto panel = child.try_as<StackPanel>())
            for (auto const &nested : panel.Children())
                if (auto control = nested.try_as<Control>())
                    control.IsEnabled(!value && !m_dialog);
    }
    Records().IsEnabled(!value && !m_dialog);
    Actions();
}
void BuildPage::Actions()
{
    if (!m_ready)
        return;
    auto selected = Selected();
    bool idle = !m_busy && !m_dialog;
    auto resource = Local() ? BuildResource::Models : m_views.at(static_cast<size_t>(ViewPicker().SelectedIndex()));
    RefreshButton().IsEnabled(idle);
    MoreButton().IsEnabled(idle && m_more && m_valid && !Local());
    StopButton().IsEnabled(m_busy);
    CreateButton().Visibility(Visible(Local() || resource == BuildResource::Batches ||
                                      resource == BuildResource::FineJobs || m_mode == L"storage"));
    CreateButton().Label(Local()                                 ? L"新增 Prompt"
                         : resource == BuildResource::Files      ? L"上傳檔案"
                         : resource == BuildResource::StoreFiles ? L"附加檔案"
                                                                 : L"新增");
    CreateButton().IsEnabled(idle && (resource != BuildResource::FineJobs || BuildPolicyAt(Now()).fineTuningCreatable));
    bool removable = Local() || resource == BuildResource::Chats || resource == BuildResource::Responses ||
                     resource == BuildResource::Files || resource == BuildResource::Stores ||
                     resource == BuildResource::StoreFiles || resource == BuildResource::Batches ||
                     resource == BuildResource::FineJobs;
    RemoveButton().Visibility(Visible(removable));
    RemoveButton().IsEnabled(idle && selected && m_valid);
    RemoveButton().Label(resource == BuildResource::Batches || resource == BuildResource::FineJobs ? L"取消工作"
                                                                                                   : L"刪除");
    ChildrenButton().Visibility(Visible(m_mode == L"logs" || m_mode == L"traces" || m_mode == L"evaluations" ||
                                        m_mode == L"fine-tuning" || resource == BuildResource::Stores));
    ChildrenButton().IsEnabled(idle && selected && m_valid && resource != BuildResource::Traces &&
                               resource != BuildResource::ChatMessages && resource != BuildResource::ResponseItems &&
                               resource != BuildResource::EvalOutputs && resource != BuildResource::FineEvents &&
                               resource != BuildResource::FineCheckpoints);
    DownloadButton().Visibility(
        Visible(resource == BuildResource::Files || m_mode == L"batches" || m_mode == L"fine-tuning"));
    DownloadButton().IsEnabled(idle);
    ExportButton().IsEnabled(idle && (!JsonBox().Text().empty() || !m_records.empty() || Local()));
    ImportButton().Visibility(Visible(Local() || m_mode == L"evaluations"));
    ImportButton().IsEnabled(idle);
    InspectButton().Visibility(Visible(!Local() && resource != BuildResource::Traces));
    InspectButton().IsEnabled(idle);
    SearchStoreButton().Visibility(Visible(m_mode == L"storage"));
    SearchStoreButton().IsEnabled(idle);
}
BuildRecord const *BuildPage::Selected()
{
    if (auto row = Records().SelectedItem().try_as<AetherNexus::ManageItem>())
        for (auto const &record : m_records)
            if (record.id == row.Id())
                return &record;
    return nullptr;
}
BuildRequest BuildPage::Query()
{
    BuildRequest request;
    request.resource = m_views.at(static_cast<size_t>(ViewPicker().SelectedIndex()));
    request.project = ProjectBox().Text();
    request.parent = ParentBox().Text();
    if (request.resource == BuildResource::Responses)
    {
        request.item = request.parent;
        request.parent.clear();
    }
    if (request.resource == BuildResource::EvalOutputs)
    {
        request.scope = request.parent;
        request.parent = m_eval;
    }
    return request;
}
void BuildPage::Render()
{
    m_rendering = true;
    Records().Items().Clear();
    std::wstring search{SearchBox().Text()};
    auto lower = [](std::wstring text) {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return text;
    };
    search = lower(search);
    for (auto const &record : m_records)
    {
        if (!search.empty() && lower(record.title + L" " + record.subtitle).find(search) == std::wstring::npos)
            continue;
        auto row = make_self<ManageItem>();
        row->Set(hstring{record.id}, hstring{record.title},
                 hstring{record.subtitle + (record.status.empty() ? L"" : L" · " + record.status)},
                 hstring{record.json});
        Records().Items().Append(*row);
    }
    m_rendering = false;
    ListStatus().Text(std::to_wstring(Records().Items().Size()) + L" / " + std::to_wstring(m_records.size()) +
                      L" 個已載入項目" + (m_more ? L" · 尚有更多" : L""));
    if (m_records.empty())
        ListStatus().Text(L"沒有可顯示的項目。請檢查範圍或建立資料後重新整理。");
    Actions();
}
void BuildPage::Show(BuildRecord const &record)
{
    DetailTitle().Text(record.title);
    std::wstring summary = record.subtitle + (record.status.empty() ? L"" : L"\n狀態：" + record.status);
    auto object = JsonObject::Parse(record.json);
    for (auto const &[field, label] : {std::pair{L"request_counts", L"請求計數"}, {L"result_counts", L"評估結果"},
         {L"file_counts", L"檔案計數"}, {L"bytes", L"檔案大小（bytes）"}, {L"usage_bytes", L"儲存用量（bytes）"},
         {L"trained_tokens", L"已訓練 tokens"}, {L"step_number", L"訓練步數"}, {L"usage", L"用量"}})
    {
        if (object.HasKey(field) && object.GetNamedValue(field).ValueType() != JsonValueType::Null)
            summary += L"\n" + std::wstring{label} + L"：" + object.GetNamedValue(field).Stringify().c_str();
    }
    auto appendContent = [&](JsonObject const &message)
    {
        if (message.HasKey(L"content") && message.GetNamedValue(L"content").ValueType() == JsonValueType::String)
            summary += L"\n\n" + std::wstring{message.GetNamedString(L"content")}.substr(0, 4096);
    };
    appendContent(object);
    if (object.HasKey(L"choices") && object.GetNamedValue(L"choices").ValueType() == JsonValueType::Array)
    {
        auto choices = object.GetNamedArray(L"choices");
        if (choices.Size()) appendContent(choices.GetObjectAt(0).GetNamedObject(L"message", JsonObject{}));
    }
    DetailSummary().Text(summary);
    JsonBox().Text(PrettyBuildJson(record.json));
    SpanTree().RootNodes().Clear();
    SpanTree().Visibility(Visibility::Collapsed);
    if (!Local() && Query().resource == BuildResource::Traces)
    {
        auto spans = ParseTraceSpans(record.json);
        std::map<std::wstring, TreeViewNode> nodes;
        std::map<std::wstring, std::wstring> parents;
        for (auto const &span : spans)
        {
            if (span.id.empty() || nodes.contains(span.id))
                continue;
            TreeViewNode node;
            node.Content(box_value(
                hstring{span.name + (span.milliseconds ? L" · " + std::to_wstring(*span.milliseconds) + L" ms" : L"") +
                        (span.status.empty() ? L"" : L" · " + span.status)}));
            node.IsExpanded(true);
            nodes.emplace(span.id, node);
            parents.emplace(span.id, span.parent);
        }
        for (auto const &[id, node] : nodes)
        {
            auto parent = parents.at(id);
            auto cursor = parent;
            std::set<std::wstring> seen{id};
            bool safe = true;
            while (!cursor.empty() && parents.contains(cursor))
            {
                if (!seen.insert(cursor).second || seen.size() > 32)
                {
                    safe = false;
                    break;
                }
                cursor = parents.at(cursor);
            }
            if (safe && nodes.contains(parent))
                nodes.at(parent).Children().Append(node);
            else
                SpanTree().RootNodes().Append(node);
        }
        SpanTree().Visibility(Visible(!nodes.empty()));
        SpanTree().Height(std::clamp(static_cast<double>(nodes.size()) * 36.0, 96.0, 320.0));
        DetailSummary().Text(L"OTLP spans：" + std::to_wstring(spans.size()) +
                             L"（最多顯示 1000 個；完整資料可匯出 JSON）。");
    }
    Actions();
}
IAsyncAction BuildPage::Load(bool more)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    auto version = ++m_version;
    m_stop = {};
    auto stop = m_stop.get_token();
    if (!more)
    {
        m_valid = false;
        m_records.clear();
        m_after.clear();
        m_more = false;
        JsonBox().Text(L"");
        SpanTree().Visibility(Visibility::Collapsed);
        if (!Local())
        {
            DetailTitle().Text(L"選取項目查看詳細資料");
            DetailSummary().Text(L"");
        }
        Render();
    }
    Busy(true);
    apartment_context ui;
    std::exception_ptr failure;
    BuildList list;
    std::vector<PromptRevision> prompts;
    try
    {
        if (Local())
        {
            auto store = m_store;
            co_await resume_background();
            prompts = store->Read();
        }
        else
        {
            auto request = Query();
            if (more)
                request.after = m_after;
            auto transport = m_transport;
            co_await resume_background();
            if (request.resource == BuildResource::Responses)
                list.records.push_back(BuildClient{transport}.Retrieve(request, stop));
            else
                list = BuildClient{transport}.List(request, stop);
        }
    }
    catch (...)
    {
        failure = std::current_exception();
    }
    co_await ui;
    if (!m_loaded || version != m_version)
        co_return;
    Busy(false);
    if (failure)
    {
        try
        {
            std::rethrow_exception(failure);
        }
        catch (...)
        {
            Error();
        }
        co_return;
    }
    if (Local())
    {
        m_prompts = std::move(prompts);
        std::map<std::wstring, PromptRevision> latest;
        for (auto const &prompt : m_prompts)
            latest[prompt.id] = prompt;
        for (auto const &[id, prompt] : latest)
            m_records.push_back({id,
                                 prompt.name,
                                 L"v" + std::to_wstring(prompt.version) + L" · " + prompt.model,
                                 L"",
                                 SerializePrompt(prompt),
                                 {}});
    }
    else
    {
        std::set<std::wstring> ids;
        for (auto const &r : m_records)
            ids.insert(r.id);
        for (auto &record : list.records)
            if (ids.insert(record.id).second)
                m_records.push_back(std::move(record));
        m_more = list.hasMore;
        m_after = std::move(list.after);
        m_dataProject = ProjectBox().Text();
        m_dataParent = ParentBox().Text();
    }
    m_valid = true;
    Render();
    NoticeText(L"資料已更新。", InfoBarSeverity::Success);
}
IAsyncAction BuildPage::Execute(BuildRequest request, bool mutation)
{
    auto lifetime = get_strong();
    if (m_busy)
        co_return;
    auto version = ++m_version;
    m_stop = {};
    auto stop = m_stop.get_token();
    auto transport = m_transport;
    Busy(true);
    apartment_context ui;
    std::exception_ptr failure;
    BuildRecord record;
    uint64_t bytes{};
    try
    {
        co_await resume_background();
        if (request.resource == BuildResource::FileContent)
            bytes = BuildClient{transport}.Download(request, stop);
        else
            record = mutation ? BuildClient{transport}.Change(request, stop)
                              : BuildClient{transport}.Retrieve(request, stop);
    }
    catch (...)
    {
        failure = std::current_exception();
    }
    co_await ui;
    if (!m_loaded || version != m_version)
        co_return;
    Busy(false);
    if (failure)
    {
        m_valid = false;
        Actions();
        try
        {
            std::rethrow_exception(failure);
        }
        catch (...)
        {
            Error(mutation);
        }
        co_return;
    }
    if (request.resource == BuildResource::FileContent)
        NoticeText(L"下載完成：" + std::to_wstring(bytes) + L" bytes。", InfoBarSeverity::Success);
    else
    {
        if (!Local()) Records().SelectedItem(nullptr);
        if (!mutation)
        {
            m_records = {record};
            m_more = false;
            m_after.clear();
            m_dataProject = ProjectBox().Text();
            m_dataParent = ParentBox().Text();
            m_valid = true;
            Render();
            Records().SelectedIndex(0);
        }
        Show(record);
        if (mutation && request.resource != BuildResource::StoreSearch)
        {
            m_valid = false;
            NoticeText(L"操作已確認。請重新整理清單查看最新狀態。", InfoBarSeverity::Success);
        }
        else
            NoticeText(L"資料已取得。", InfoBarSeverity::Success);
    }
    Actions();
}
IAsyncOperation<bool> BuildPage::Confirm(std::wstring title, std::wstring text)
{
    StackPanel panel;
    Note(panel, text.c_str());
    auto dialog = Dialog(XamlRoot(), title.c_str(), panel, L"確認");
    co_return co_await dialog.ShowAsync() == ContentDialogResult::Primary;
}
fire_and_forget BuildPage::OnLoaded(IInspectable const &, RoutedEventArgs const &)
{
    m_loaded = true;
    if (m_records.empty() && !m_valid)
        try
        {
            co_await Load();
        }
        catch (...)
        {
            Error();
        }
    else
        Actions();
}
void BuildPage::OnUnloaded(IInspectable const &, RoutedEventArgs const &)
{
    m_loaded = false;
    m_stop.request_stop();
    ++m_version;
    Busy(false);
}
void BuildPage::OnStop(IInspectable const &, RoutedEventArgs const &)
{
    m_stop.request_stop();
}
void BuildPage::OnScope(IInspectable const &, TextChangedEventArgs const &)
{
    if (m_ready && !m_busy && (ProjectBox().Text() != m_dataProject || ParentBox().Text() != m_dataParent))
    {
        m_valid = false;
        m_more = false;
        Actions();
    }
}
void BuildPage::OnSettings(IInspectable const &, RoutedEventArgs const &)
{
    auto current = get_strong().as<DependencyObject>();
    while (current)
    {
        if (auto navigation = current.try_as<NavigationView>())
        {
            navigation.SelectedItem(navigation.SettingsItem());
            return;
        }
        current = Media::VisualTreeHelper::GetParent(current);
    }
}
void BuildPage::OnSize(IInspectable const &, SizeChangedEventArgs const &args)
{
    bool compact = args.NewSize().Width < 850;
    ListColumn().Width(compact ? GridLength{1, GridUnitType::Star} : GridLength{340, GridUnitType::Pixel});
    ListRow().Height(compact ? GridLength{220, GridUnitType::Pixel} : GridLength{1, GridUnitType::Star});
    DetailRow().Height(compact ? GridLength{1, GridUnitType::Star} : GridLength{0, GridUnitType::Pixel});
    Grid::SetColumn(DetailPane(), compact ? 0 : 1);
    Grid::SetRow(DetailPane(), compact ? 1 : 0);
    Grid::SetColumnSpan(ListPane(), compact ? 2 : 1);
    Grid::SetColumnSpan(DetailPane(), compact ? 2 : 1);
}
void BuildPage::OnView(IInspectable const &, SelectionChangedEventArgs const &)
{
    if (!m_ready || m_busy)
        return;
    m_valid = false;
    m_more = false;
    m_records.clear();
    JsonBox().Text(L"");
    SpanTree().Visibility(Visibility::Collapsed);
    auto resource = Query().resource;
    bool parent = resource == BuildResource::ChatMessages || resource == BuildResource::Responses ||
                  resource == BuildResource::ResponseItems || resource == BuildResource::Traces ||
                  resource == BuildResource::EvalRuns || resource == BuildResource::EvalOutputs ||
                  resource == BuildResource::FineEvents || resource == BuildResource::FineCheckpoints ||
                  resource == BuildResource::StoreFiles;
    ParentBox().Header(box_value(hstring{resource == BuildResource::Responses     ? L"Response ID"
                                         : resource == BuildResource::EvalOutputs ? L"Run ID（先選取所屬 Eval）"
                                                                                  : L"上層項目 ID"}));
    ParentBox().Visibility(Visible(parent));
    Grid::SetColumn(ProjectBox(), parent ? 2 : 1);
    Grid::SetColumnSpan(ProjectBox(), parent ? 1 : 2);
    DetailTitle().Text(L"選取項目查看詳細資料");
    DetailSummary().Text(L"");
    Render();
    NoticeText(L"請套用範圍後重新整理。");
}
void BuildPage::OnSearch(AutoSuggestBox const &, AutoSuggestBoxTextChangedEventArgs const &)
{
    if (m_ready && !m_busy)
        Render();
}
void BuildPage::OnContainer(ListViewBase const &, ContainerContentChangingEventArgs const &args)
{
    auto container = args.ItemContainer();
    if (args.InRecycleQueue())
    {
        Automation::AutomationProperties::SetName(container, L"");
        Automation::AutomationProperties::SetAutomationId(container, L"");
    }
    else if (auto row = args.Item().try_as<AetherNexus::ManageItem>())
    {
        Automation::AutomationProperties::SetName(container, row.Title());
        Automation::AutomationProperties::SetAutomationId(container, L"BuildRow-" + row.Id());
    }
}
void BuildPage::OnSelection(IInspectable const &, SelectionChangedEventArgs const &)
{
    if (m_rendering)
        return;
    try
    {
        if (auto selected = Selected())
        {
            Show(*selected);
            if (Local())
            {
                m_versions.clear();
                VersionPicker().Items().Clear();
                for (auto const &prompt : m_prompts)
                    if (prompt.id == selected->id)
                    {
                        m_versions.push_back(prompt);
                        VersionPicker().Items().Append(box_value(to_hstring(prompt.version)));
                    }
                VersionPicker().SelectedIndex(static_cast<int>(m_versions.size()) - 1);
            }
        }
        else if (!Local())
        {
            DetailTitle().Text(L"選取項目查看詳細資料");
            DetailSummary().Text(L"");
            JsonBox().Text(L"");
            SpanTree().Visibility(Visibility::Collapsed);
        }
        Actions();
    }
    catch (...)
    {
        Error();
    }
}
void BuildPage::EditPrompt(PromptRevision const &draft)
{
    m_draft = draft;
    NameBox().Text(draft.name);
    ModelBox().Text(draft.model);
    InstructionsBox().Text(draft.instructions);
    InputBox().Text(draft.input);
    OptionsBox().Text(draft.options);
    DetailTitle().Text(draft.name.empty() ? L"新增 Prompt" : draft.name);
}
PromptRevision BuildPage::Draft()
{
    auto draft = m_draft;
    draft.name = NameBox().Text();
    draft.model = ModelBox().Text();
    draft.instructions = InstructionsBox().Text();
    draft.input = InputBox().Text();
    draft.options = OptionsBox().Text();
    return draft;
}
void BuildPage::OnVersion(IInspectable const &, SelectionChangedEventArgs const &)
{
    auto index = VersionPicker().SelectedIndex();
    if (index >= 0 && static_cast<size_t>(index) < m_versions.size())
    {
        auto chosen = m_versions.at(static_cast<size_t>(index));
        EditPrompt(chosen);
        JsonBox().Text(PrettyBuildJson(SerializePrompt(chosen)));
        m_draft.version = m_versions.back().version; // Saving a historical revision appends after
                                                     // the head, with conflict detection.
    }
}
fire_and_forget BuildPage::OnRefresh(IInspectable const &, RoutedEventArgs const &)
{
    try
    {
        co_await Load();
    }
    catch (...)
    {
        Error();
    }
}
fire_and_forget BuildPage::OnMore(IInspectable const &, RoutedEventArgs const &)
{
    if (m_more && m_valid)
        try
        {
            co_await Load(true);
        }
        catch (...)
        {
            Error();
        }
}
fire_and_forget BuildPage::OnSavePrompt(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    Busy(true);
    apartment_context ui;
    std::exception_ptr failure;
    PromptRevision saved;
    try
    {
        auto draft = Draft();
        auto store = m_store;
        co_await resume_background();
        saved = store->Save(draft);
    }
    catch (...)
    {
        failure = std::current_exception();
    }
    co_await ui;
    Busy(false);
    if (!m_loaded)
        co_return;
    if (failure)
    {
        try
        {
            std::rethrow_exception(failure);
        }
        catch (...)
        {
            Error();
        }
        co_return;
    }
    EditPrompt(saved);
    co_await Load();
    for (uint32_t index = 0; index < Records().Items().Size(); ++index)
    {
        if (Records().Items().GetAt(index).as<AetherNexus::ManageItem>().Id() == saved.id)
        {
            Records().SelectedIndex(static_cast<int32_t>(index));
            break;
        }
    }
    JsonBox().Text(PrettyBuildJson(SerializePrompt(saved)));
    Actions();
}
fire_and_forget BuildPage::OnRunPrompt(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    try
    {
        auto draft = Draft();
        BuildRequest request;
        request.resource = BuildResource::Responses;
        request.method = BuildMethod::Post;
        request.body =
            PromptTestBody(draft.model, RenderPromptVariables(draft.instructions, VariablesBox().Text().c_str()),
                           RenderPromptVariables(draft.input, VariablesBox().Text().c_str()), draft.options);
        m_dialog = true;
        Busy(false);
        bool approved = co_await Confirm(L"執行 Prompt 測試", L"目前 Prompt、變數替換後的 instructions 與輸入將傳送至 "
                                                              L"OpenAI，並依模型計費。是否執行一次？");
        m_dialog = false;
        Busy(false);
        if (approved && m_loaded)
            co_await Execute(std::move(request), true);
    }
    catch (...)
    {
        m_dialog = false;
        Busy(false);
        Error();
    }
}
fire_and_forget BuildPage::OnChildren(IInspectable const &, RoutedEventArgs const &)
{
    if (m_busy || m_dialog || !m_valid)
        co_return;
    try
    {
        auto selected = Selected();
        if (!selected)
            co_return;
        auto id = selected->id;
        auto index = ViewPicker().SelectedIndex();
        if (m_mode == L"evaluations")
        {
            if (index == 0)
                m_eval = id;
            else if (index != 1)
                co_return;
            ViewPicker().SelectedIndex(index + 1);
        }
        else if (m_mode == L"logs")
        {
            if (index != 0 && index != 2)
                co_return;
            ViewPicker().SelectedIndex(index + 1);
        }
        else if (m_mode == L"fine-tuning")
            ViewPicker().SelectedIndex(1);
        else if (m_mode == L"storage")
            ViewPicker().SelectedIndex(2);
        else
            ViewPicker().SelectedIndex(1);
        ParentBox().Text(id);
        co_await Load();
    }
    catch (...)
    {
        Error();
    }
}
fire_and_forget BuildPage::OnInspect(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    try
    {
        m_dialog = true;
        Busy(false);
        StackPanel fields;
        fields.Spacing(12);
        auto id = Field(fields, L"項目 ID", L"BuildDialogId", Selected() ? Selected()->id : L"");
        auto dialog = Dialog(XamlRoot(), L"取得項目", fields, L"取得");
        auto result = co_await dialog.ShowAsync();
        m_dialog = false;
        Busy(false);
        if (result != ContentDialogResult::Primary || !m_loaded)
            co_return;
        auto request = Query();
        request.item = id.Text();
        co_await Execute(request, false);
    }
    catch (...)
    {
        m_dialog = false;
        Busy(false);
        Error();
    }
}
fire_and_forget BuildPage::OnCreate(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    if (Local())
    {
        Records().SelectedItem(nullptr);
        m_versions.clear();
        VersionPicker().Items().Clear();
        EditPrompt({});
        JsonBox().Text(L"");
        Actions();
        co_return;
    }
    try
    {
        auto request = Query();
        request.method = BuildMethod::Post;
        request.item.clear();
        m_dialog = true;
        Busy(false);
        if (request.resource == BuildResource::Files)
        {
            StackPanel fields;
            fields.Spacing(12);
            auto purpose = Choice(fields, L"檔案用途", L"BuildPurpose",
                                  {L"user_data", L"batch", L"fine-tune", L"assistants", L"vision", L"evals"});
            Note(fields, L"batch 與 fine-tune 必須為 JSONL；batch 最大 200 "
                         L"MB，其他用途最大 512 MB。選取後需再次確認上傳。");
            auto dialog = Dialog(XamlRoot(), L"選取上傳用途", fields, L"選取檔案");
            if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
            {
                m_dialog = false;
                Busy(false);
                co_return;
            }
            Pickers::FileOpenPicker picker{XamlRoot().ContentIslandEnvironment().AppWindowId()};
            picker.FileTypeFilter().Append(L"*");
            auto file = co_await picker.PickSingleFileAsync();
            if (!file)
            {
                m_dialog = false;
                Busy(false);
                co_return;
            }
            std::filesystem::path path{file.Path().c_str()};
            auto bytes = std::filesystem::file_size(path);
            request.upload = BuildUpload{path, path.filename().wstring(), Value(purpose), bytes};
            (void)BuildWire(request);
            if (!(co_await Confirm(L"上傳檔案", path.filename().wstring() + L"\n" + std::to_wstring(bytes) +
                                                    L" bytes · " + Value(purpose) + L"\n檔案內容將傳至 OpenAI。")))
            {
                m_dialog = false;
                Busy(false);
                co_return;
            }
        }
        else
        {
            StackPanel fields;
            fields.Spacing(12);
            TextBox first{nullptr}, second{nullptr}, third{nullptr}, fourth{nullptr}, advanced{nullptr};
            ComboBox choice{nullptr};
            if (request.resource == BuildResource::Batches)
            {
                first = Field(fields, L"已上傳的 batch 檔案 ID", L"BuildDialogFile");
                choice = Choice(fields, L"端點", L"BuildEndpoint",
                                {L"/v1/responses", L"/v1/chat/completions", L"/v1/embeddings", L"/v1/moderations",
                                 L"/v1/images/generations", L"/v1/images/edits"});
                Note(fields, L"24 小時完成視窗。建立後服務會處理檔案中的請求並可能產生費用。");
            }
            else if (request.resource == BuildResource::FineJobs)
            {
                first = Field(fields, L"可微調的模型 ID", L"BuildDialogModel");
                second = Field(fields, L"Training file ID", L"BuildDialogFile");
                third = Field(fields, L"Validation file ID（選填）", L"BuildValidation");
                fourth = Field(fields, L"Suffix（選填，最多 64 字元）", L"BuildSuffix");
                fourth.MaxLength(64);
                choice = Choice(fields, L"方法", L"BuildMethod", {L"supervised", L"dpo"});
                advanced =
                    Field(fields, L"Hyperparameters JSON（空物件使用官方預設）", L"BuildHyperparameters", L"{}", true);
                Note(fields, L"僅官方允許的既有活躍客戶可建立。訓練可能產生費用；資料格"
                             L"式與可用模型依官方限制。");
            }
            else if (request.resource == BuildResource::Stores)
            {
                first = Field(fields, L"向量庫名稱", L"BuildDialogName");
                second = Field(fields, L"初始 file IDs（以逗號分隔，選填）", L"BuildDialogFile", L"", true);
                third = Field(fields, L"最後使用後到期天數（1–365，留空不設定）", L"BuildExpiry");
                Note(fields, L"向量庫會產生儲存費用。");
            }
            else if (request.resource == BuildResource::StoreFiles)
                first = Field(fields, L"要附加的 file ID", L"BuildDialogFile");
            else
                throw std::invalid_argument("Unsupported creation");
            auto dialog = Dialog(XamlRoot(), L"建立伺服器資源", fields, L"建立");
            auto result = co_await dialog.ShowAsync();
            if (result != ContentDialogResult::Primary)
            {
                m_dialog = false;
                Busy(false);
                co_return;
            }
            if (request.resource == BuildResource::Batches)
                request.body = BatchBody(first.Text().c_str(), Value(choice));
            else if (request.resource == BuildResource::FineJobs)
                request.body = FineTuneBody(first.Text().c_str(), second.Text().c_str(), third.Text().c_str(),
                                            fourth.Text().c_str(), Value(choice), advanced.Text().c_str());
            else if (request.resource == BuildResource::Stores)
            {
                std::optional<int64_t> expiry;
                if (!third.Text().empty())
                {
                    std::wstring value{third.Text()};
                    size_t parsed{};
                    expiry = std::stoll(value, &parsed);
                    if (parsed != value.size())
                        throw std::invalid_argument("Invalid expiry");
                }
                request.body = StoreBody(first.Text().c_str(), second.Text().c_str(), expiry);
            }
            else
                request.body = StoreFileBody(first.Text().c_str());
        }
        m_dialog = false;
        Busy(false);
        if (m_loaded)
            co_await Execute(std::move(request), true);
    }
    catch (...)
    {
        m_dialog = false;
        Busy(false);
        Error();
    }
}
fire_and_forget BuildPage::OnRemove(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog || !m_valid)
        co_return;
    try
    {
        auto selected = Selected();
        if (!selected)
            co_return;
        auto id = selected->id;
        m_dialog = true;
        Busy(false);
        StackPanel fields;
        fields.Spacing(12);
        Note(fields, (L"目標：" + id +
                      L"\n刪除資源不可復原。取消工作可能仍產生已完成部分的費用。刪"
                      L"除向量庫關聯不會刪除原始檔。\n請輸入完整 ID 確認。")
                         .c_str());
        auto confirmation = Field(fields, L"確認 ID", L"BuildConfirmId");
        auto dialog = Dialog(XamlRoot(), L"確認操作", fields, L"確認");
        auto result = co_await dialog.ShowAsync();
        m_dialog = false;
        Busy(false);
        if (result != ContentDialogResult::Primary || !m_loaded)
            co_return;
        if (confirmation.Text() != id)
            throw std::invalid_argument("ID confirmation differs");
        if (Local())
        {
            auto store = m_store;
            apartment_context ui;
            std::exception_ptr failure;
            Busy(true);
            try
            {
                co_await resume_background();
                store->Remove(id);
            }
            catch (...)
            {
                failure = std::current_exception();
            }
            co_await ui;
            Busy(false);
            if (!m_loaded)
                co_return;
            if (failure)
                std::rethrow_exception(failure);
            EditPrompt({});
            co_await Load();
        }
        else
        {
            auto request = Query();
            request.item = id;
            request.method = BuildMethod::Delete;
            if (request.resource == BuildResource::Batches)
            {
                request.resource = BuildResource::BatchCancel;
                request.method = BuildMethod::Post;
            }
            if (request.resource == BuildResource::FineJobs)
            {
                request.resource = BuildResource::FineCancel;
                request.method = BuildMethod::Post;
            }
            co_await Execute(std::move(request), true);
        }
    }
    catch (...)
    {
        m_dialog = false;
        Busy(false);
        Error();
    }
}
fire_and_forget BuildPage::OnDownload(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    try
    {
        m_dialog = true;
        Busy(false);
        StackPanel fields;
        fields.Spacing(12);
        auto selected = Selected();
        auto initial = selected && m_mode == L"storage" ? selected->id : L"";
        if (selected && m_mode == L"batches")
        {
            JsonObject object = JsonObject::Parse(selected->json);
            initial = object.GetNamedString(L"output_file_id", L"");
        }
        Note(fields, L"輸入詳細資料中的 output_file_id、error_file_id 或 result_files "
                     L"項目。最多下載 1 GiB；較大檔案請使用官方工具。");
        auto id = Field(fields, L"File ID", L"BuildDialogFile", initial);
        auto dialog = Dialog(XamlRoot(), L"下載伺服器檔案", fields, L"選取儲存位置");
        if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
        {
            m_dialog = false;
            Busy(false);
            co_return;
        }
        ValidateBuildIdentifier(id.Text().c_str());
        Pickers::FileSavePicker picker{XamlRoot().ContentIslandEnvironment().AppWindowId()};
        picker.SuggestedFileName(id.Text());
        picker.FileTypeChoices().Insert(L"資料檔案", single_threaded_vector<hstring>({L".jsonl", L".json", L".bin"}));
        auto destination = co_await picker.PickSaveFileAsync();
        m_dialog = false;
        Busy(false);
        if (!destination || !m_loaded)
            co_return;
        BuildRequest request;
        request.resource = BuildResource::FileContent;
        request.item = id.Text();
        request.project = ProjectBox().Text();
        request.download = destination.Path().c_str();
        co_await Execute(std::move(request), false);
    }
    catch (...)
    {
        m_dialog = false;
        Busy(false);
        Error();
    }
}
fire_and_forget BuildPage::OnExport(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    try
    {
        std::wstring json;
        if (Local())
            json = SerializePrompt(Draft());
        else if (!JsonBox().Text().empty())
            json = JsonBox().Text();
        else
        {
            JsonArray rows;
            for (auto const &record : m_records)
                rows.Append(JsonObject::Parse(record.json));
            JsonObject root;
            root.SetNamedValue(L"data", rows);
            json = root.Stringify();
        }
        m_dialog = true;
        Busy(false);
        Pickers::FileSavePicker picker{XamlRoot().ContentIslandEnvironment().AppWindowId()};
        picker.SuggestedFileName(m_mode);
        picker.FileTypeChoices().Insert(L"JSON", single_threaded_vector<hstring>({L".json"}));
        auto result = co_await picker.PickSaveFileAsync();
        if (result)
        {
            using namespace Windows::Storage;
            std::filesystem::path path{result.Path().c_str()};
            auto folder = co_await StorageFolder::GetFolderFromPathAsync(path.parent_path().wstring());
            auto file =
                co_await folder.CreateFileAsync(path.filename().wstring(), CreationCollisionOption::ReplaceExisting);
            co_await FileIO::WriteTextAsync(file, json, Streams::UnicodeEncoding::Utf8);
            NoticeText(L"JSON 已匯出。", InfoBarSeverity::Success);
        }
        m_dialog = false;
        Busy(false);
    }
    catch (...)
    {
        m_dialog = false;
        Busy(false);
        Error();
    }
}
fire_and_forget BuildPage::OnImport(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    try
    {
        m_dialog = true;
        Busy(false);
        Pickers::FileOpenPicker picker{XamlRoot().ContentIslandEnvironment().AppWindowId()};
        picker.FileTypeFilter().Append(L".json");
        auto result = co_await picker.PickSingleFileAsync();
        if (result)
        {
            if (std::filesystem::file_size(std::filesystem::path{result.Path().c_str()}) >
                (Local() ? 256ull * 1024 : 16ull * 1024 * 1024))
                throw std::invalid_argument("Import too large");
            auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(result.Path());
            auto text = co_await Windows::Storage::FileIO::ReadTextAsync(file);
            if (Local())
            {
                auto imported = ImportPrompt(text.c_str());
                Records().SelectedItem(nullptr);
                m_versions.clear();
                VersionPicker().Items().Clear();
                EditPrompt(imported);
                JsonBox().Text(PrettyBuildJson(SerializePrompt(imported)));
                NoticeText(L"已匯入支援的 Prompt "
                           L"欄位。儲存新版本後才寫入本機；工具與歷史訊息不會自動遷移。");
            }
            else
            {
                JsonObject object;
                if (!JsonObject::TryParse(text, object))
                    throw std::invalid_argument("Invalid archive");
                JsonBox().Text(PrettyBuildJson(text.c_str()));
                DetailTitle().Text(L"本機 Evals 匯出檔");
                NoticeText(L"本機匯出檔已載入，未傳送至伺服器。");
            }
        }
        m_dialog = false;
        Busy(false);
    }
    catch (...)
    {
        m_dialog = false;
        Busy(false);
        Error();
    }
}
fire_and_forget BuildPage::OnStoreSearch(IInspectable const &, RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (m_busy || m_dialog)
        co_return;
    try
    {
        m_dialog = true;
        Busy(false);
        StackPanel fields;
        fields.Spacing(12);
        auto id =
            Field(fields, L"Vector store ID", L"BuildDialogId",
                  ViewPicker().SelectedIndex() == 1 && Selected() ? Selected()->id : std::wstring{ParentBox().Text()});
        auto query = Field(fields, L"搜尋內容", L"BuildQuery", L"", true);
        auto dialog = Dialog(XamlRoot(), L"搜尋向量庫", fields, L"搜尋");
        auto result = co_await dialog.ShowAsync();
        m_dialog = false;
        Busy(false);
        if (result != ContentDialogResult::Primary || !m_loaded)
            co_return;
        BuildRequest request;
        request.resource = BuildResource::StoreSearch;
        request.method = BuildMethod::Post;
        request.parent = id.Text();
        request.project = ProjectBox().Text();
        request.body = StoreSearchBody(query.Text().c_str());
        co_await Execute(std::move(request), true);
    }
    catch (...)
    {
        m_dialog = false;
        Busy(false);
        Error();
    }
}
} // namespace winrt::AetherNexus::implementation
