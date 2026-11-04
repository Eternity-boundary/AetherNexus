// SPDX-License-Identifier: GPL-3.0-only
// Offline host: neither CredentialStore nor any live transport is linked.
#include "pch.h"
#include "App.xaml.h"
#include "Views/BuildPage.xaml.h"
#include <atomic>
#include <map>
#include <microsoft.ui.xaml.window.h>
#include <thread>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.Data.Json.h>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace ::AetherNexus::Services;
using namespace Windows::Data::Json;
namespace
{
class OfflineBuild final : public IBuildTransport
{
  public:
    std::atomic<bool> missing{}, slow{};
    BuildResponse Send(BuildRequest const &request, std::stop_token stop) override
    {
        (void)BuildWire(request);
        CheckCancelled(stop);
        if (missing)
            throw ApiError{ApiErrorCode::MissingCredential};
        if (slow)
            for (unsigned i{}; i < 100; ++i)
            {
                CheckCancelled(stop);
                std::this_thread::sleep_for(std::chrono::milliseconds{30});
            }
        std::string json;
        switch (request.resource)
        {
        case BuildResource::Chats:
            json =
                R"({"id":"chatcmpl_offline","model":"offline-model","created":1700000000,"usage":{"total_tokens":123},"choices":[{"message":{"role":"assistant","content":"Offline stored answer"}}]})";
            break;
        case BuildResource::ChatMessages:
            json = R"({"id":"msg_offline","role":"user","content":"Offline stored question"})";
            break;
        case BuildResource::Responses:
            json =
                R"({"id":"resp_offline","status":"completed","output":[{"type":"message","content":[{"type":"output_text","text":"Offline response"}]}]})";
            break;
        case BuildResource::ResponseItems:
            json = R"({"id":"msg_input","role":"user","content":"Offline input"})";
            break;
        case BuildResource::Sessions:
            json = R"({"id":"sess_offline","object":"agent.session","created_at":1700000000})";
            break;
        case BuildResource::Traces:
            json =
                R"({"id":"turn_offline","session_id":"sess_offline","otlp":{"resourceSpans":[{"scopeSpans":[{"spans":[{"spanId":"root","name":"Offline agent turn","startTimeUnixNano":"1000000","endTimeUnixNano":"6000000"},{"spanId":"child","parentSpanId":"root","name":"Offline tool"}]}]}]}})";
            break;
        case BuildResource::Batches:
        case BuildResource::BatchCancel:
            json =
                R"({"id":"batch_offline","status":"in_progress","input_file_id":"file_batch","output_file_id":"file_output","request_counts":{"total":10,"completed":4,"failed":1}})";
            break;
        case BuildResource::Evals:
            json = R"({"id":"eval_offline","name":"Offline evaluation","created_at":1700000000})";
            break;
        case BuildResource::EvalRuns:
            json =
                R"({"id":"run_offline","name":"Offline eval run","status":"completed","result_counts":{"total":4,"passed":3,"failed":1}})";
            break;
        case BuildResource::EvalOutputs:
            json = R"({"id":"output_offline","status":"pass","results":[{"grader":"offline","score":1}]})";
            break;
        case BuildResource::FineJobs:
        case BuildResource::FineCancel:
            json =
                R"({"id":"ftjob_offline","model":"offline-model","status":"running","trained_tokens":1234,"result_files":["file_output"]})";
            break;
        case BuildResource::FineEvents:
            json = R"({"id":"ftevent_offline","message":"Offline training step","level":"info"})";
            break;
        case BuildResource::FineCheckpoints:
            json =
                R"({"id":"ftckpt_offline","fine_tuned_model_checkpoint":"offline-model:checkpoint","step_number":100})";
            break;
        case BuildResource::Files:
            json =
                R"({"id":"file_offline","filename":"offline-training.jsonl","purpose":"fine-tune","bytes":128,"status":"processed"})";
            break;
        case BuildResource::Stores:
            json =
                R"({"id":"vs_offline","name":"Offline knowledge","status":"completed","file_counts":{"total":2,"completed":2,"failed":0},"usage_bytes":128})";
            break;
        case BuildResource::StoreFiles:
            json = R"({"id":"file_offline","status":"completed","usage_bytes":128})";
            break;
        case BuildResource::StoreSearch:
            return {
                200,
                R"({"object":"vector_store.search_results.page","data":[{"file_id":"file_offline","score":0.9,"content":[{"type":"text","text":"Offline match"}]}]})"};
        case BuildResource::FileContent:
            throw std::runtime_error("File download is covered by wire tests, not this host");
        case BuildResource::Models:
            json = R"({"id":"offline-model","object":"model"})";
            break;
        }
        if (request.method == BuildMethod::Delete)
            return {200, to_string(L"{\"id\":\"" + hstring{request.item} + L"\",\"deleted\":true}")};
        if (request.method == BuildMethod::Post || !request.item.empty())
            return {200, json};
        return {200, "{\"data\":[" + json + "],\"has_more\":false}"};
    }
};
class OfflinePrompts final : public IPromptStore
{
  public:
    std::vector<PromptRevision> entries;
    std::vector<PromptRevision> Read() override
    {
        return entries;
    }
    PromptRevision Save(PromptRevision const &draft) override
    {
        (void)SerializePrompt(draft);
        auto value = draft;
        if (value.id.empty())
            value.id = L"local_offline";
        ++value.version;
        entries.push_back(value);
        return value;
    }
    void Remove(std::wstring const &id) override
    {
        std::erase_if(entries, [&](auto const &p) { return p.id == id; });
    }
};
} // namespace
namespace winrt::AetherNexus::implementation
{
App::App() = default;
void App::OnLaunched(LaunchActivatedEventArgs const &)
{
    window = Window{};
    window.SystemBackdrop(Media::MicaBackdrop{});
    window.Title(L"AetherNexus OFFLINE Build Tests");
    HWND hwnd{};
    check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&hwnd));
    double scale = GetDpiForWindow(hwnd) / 96.0;
    MONITORINFO monitor{sizeof(MONITORINFO)};
    if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
        throw hresult_error(HRESULT_FROM_WIN32(GetLastError()));
    auto const work = monitor.rcWork;
    window.AppWindow().MoveAndResize({work.left + 16, work.top + 16,
        std::min(static_cast<int32_t>(1380 * scale), static_cast<int32_t>(work.right - work.left - 32)),
        std::min(static_cast<int32_t>(950 * scale), static_cast<int32_t>(work.bottom - work.top - 32))});
    Controls::NavigationView navigation;
    navigation.IsSettingsVisible(false);
    navigation.IsBackButtonVisible(Controls::NavigationViewBackButtonVisible::Collapsed);
    auto transport = std::make_shared<OfflineBuild>();
    auto store = std::make_shared<OfflinePrompts>();
    auto pages = std::make_shared<std::map<std::wstring, Controls::Page>>();
    navigation.SelectionChanged(
        [pages, transport, store](Controls::NavigationView const &sender,
                                  Controls::NavigationViewSelectionChangedEventArgs const &event) {
            auto item = event.SelectedItem().try_as<Controls::NavigationViewItem>();
            if (!item)
                return;
            std::wstring route{unbox_value<hstring>(item.Tag())};
            auto found = pages->find(route);
            if (found == pages->end())
            {
                AetherNexus::BuildPage page;
                get_self<BuildPage>(page)->Configure(hstring{route}, transport, store);
                found = pages->emplace(route, page).first;
            }
            sender.Content(found->second);
        });
    for (auto route :
         {L"prompts", L"logs", L"traces", L"assistants", L"batches", L"evaluations", L"fine-tuning", L"storage"})
    {
        Controls::NavigationViewItem item;
        item.Icon(Controls::SymbolIcon{Controls::Symbol::Document});
        item.Content(box_value(route));
        item.Tag(box_value(route));
        Automation::AutomationProperties::SetAutomationId(item, L"OfflineNav-" + hstring{route});
        navigation.MenuItems().Append(item);
    }
    Controls::StackPanel footer;
    for (auto name : {L"Light", L"Dark", L"Narrow", L"Wide", L"MissingKey", L"RestoreKey", L"Slow", L"Fast"})
    {
        Controls::Button button;
        button.Content(box_value(name));
        Automation::AutomationProperties::SetAutomationId(button, L"Offline" + hstring{name});
        button.Click(
            [navigation, transport, host = window, scale, choice = std::wstring{name}](auto const &, auto const &) {
                if (choice == L"Light" || choice == L"Dark")
                    navigation.RequestedTheme(choice == L"Light" ? ElementTheme::Light : ElementTheme::Dark);
                else if (choice == L"MissingKey" || choice == L"RestoreKey")
                    transport->missing = choice == L"MissingKey";
                else if (choice == L"Slow" || choice == L"Fast")
                    transport->slow = choice == L"Slow";
                else
                    host.AppWindow().Resize({static_cast<int32_t>((choice == L"Narrow" ? 800 : 1380) * scale),
                                             static_cast<int32_t>(900 * scale)});
            });
        footer.Children().Append(button);
    }
    navigation.PaneFooter(footer);
    window.Content(navigation);
    window.Activate();
    navigation.SelectedItem(navigation.MenuItems().GetAt(0));
}
} // namespace winrt::AetherNexus::implementation
