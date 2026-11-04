// SPDX-License-Identifier: GPL-3.0-only
// Offline host. No CredentialStore or live HTTP transport is linked.
#include "pch.h"
#include "App.xaml.h"
#include "Views/ManagePage.xaml.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <microsoft.ui.xaml.window.h>
#include <filesystem>
#include <fstream>
#include <atomic>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace ::AetherNexus::Services;
using namespace Windows::Data::Json;
namespace
{
    class OfflineManagement final : public IManagementTransport, public IKeyIssuanceTransport
    {
      public:
        std::atomic<bool> missing{};
        std::atomic<unsigned> adminDeleteStatus{200};
        std::atomic<bool> missingAdminVerification{};
        bool adminIssued{}, adminRevoked{};
        SensitiveApiResponse Issue(KeyIssueRequest const &request, std::stop_token stop) override
        {
            CheckCancelled(stop);
            if (missing) throw ApiError{ApiErrorCode::MissingCredential};
            if (request.kind == KeyIssueKind::Admin) { adminIssued = true; adminRevoked = false; }
            std::string_view json = request.kind == KeyIssueKind::Admin
                ? R"({"object":"organization.admin_api_key","id":"key_new","value":"offline-issued-never-valid-5678"})"
                : request.kind == KeyIssueKind::ServiceAccount
                ? R"({"object":"organization.project.service_account.api_key","id":"key_new","value":"offline-issued-never-valid-5678"})"
                : R"({"object":"organization.project.service_account","id":"svc_new","api_key":{"object":"organization.project.service_account.api_key","id":"key_new","value":"offline-issued-never-valid-5678"}})";
            SensitiveApiResponse response{200, ::AetherNexus::Infrastructure::SecureBuffer{65536}};
            response.body.Append(json);
            return response;
        }
        std::wstring name = L"Offline project", status = L"active";
        ApiResponse Send(ManagementRequest const &request, std::stop_token stop) override
        {
            CheckCancelled(stop);
            if (missing)
                throw ApiError{ApiErrorCode::MissingCredential};
            if (request.resource == ManagementResource::AdminKey && request.method == ManagementMethod::Get)
            {
                if (request.item == L"admin_offline")
                    return {200, LR"({"object":"organization.admin_api_key","id":"admin_offline","name":"Offline admin","redacted_value":"…9876"})"};
                if (request.item == L"key_generic_admin")
                    return {200, LR"({"object":"organization.admin_api_key","id":"key_generic_admin","name":"Offline generic-mask admin","redacted_value":"sk-...3333"})"};
                if (missingAdminVerification || !adminIssued || adminRevoked || request.item != L"key_new")
                    return {404, L"private verification failure", L"req_offline_verify_404"};
                return {200, LR"({"object":"organization.admin_api_key","id":"key_new","name":"Offline newly issued admin","redacted_value":"****5678"})"};
            }
            std::wstring row;
            if (request.resource == ManagementResource::Usage)
            {
                JsonObject page;
                page.SetNamedValue(L"object", JsonValue::CreateStringValue(L"page"));
                page.SetNamedValue(L"has_more", JsonValue::CreateBooleanValue(false));
                JsonArray buckets;
                for (int64_t day = request.start; day < request.end; day += 86400)
                {
                    JsonObject bucket;
                    bucket.SetNamedValue(L"object", JsonValue::CreateStringValue(L"bucket"));
                    bucket.SetNamedValue(L"start_time", JsonValue::CreateNumberValue(static_cast<double>(day)));
                    bucket.SetNamedValue(L"end_time", JsonValue::CreateNumberValue(static_cast<double>(day + 86400)));
                    JsonObject result;
                    for (auto const &key : UsageMetrics(request.category))
                    {
                        if (key == L"amount_usd")
                            result.SetNamedValue(L"amount", JsonObject::Parse(LR"({"value":0.25,"currency":"usd"})"));
                        else
                            result.SetNamedValue(key, JsonValue::CreateNumberValue(key == L"input_tokens" ? 120 : 20));
                    }
                    if (!request.group.empty())
                        result.SetNamedValue(request.group, JsonValue::CreateStringValue(L"offline-group"));
                    JsonArray results;
                    results.Append(result);
                    bucket.SetNamedValue(L"results", results);
                    buckets.Append(bucket);
                }
                page.SetNamedValue(L"data", buckets);
                return {200, page.Stringify().c_str()};
            }
            if (request.resource == ManagementResource::Projects || request.resource == ManagementResource::Project ||
                request.resource == ManagementResource::Archive)
            {
                if (request.method == ManagementMethod::Post)
                {
                    if (request.resource == ManagementResource::Archive)
                        status = L"archived";
                    else
                        name = JsonObject::Parse(request.body).GetNamedString(L"name");
                }
                JsonObject project;
                project.SetNamedValue(L"object", JsonValue::CreateStringValue(L"organization.project"));
                project.SetNamedValue(L"id", JsonValue::CreateStringValue(L"proj_offline"));
                project.SetNamedValue(L"name", JsonValue::CreateStringValue(name));
                project.SetNamedValue(L"status", JsonValue::CreateStringValue(status));
                project.SetNamedValue(L"created_at", JsonValue::CreateNumberValue(1700000000));
                row = project.Stringify();
            }
            else if (request.resource == ManagementResource::ProjectKeys ||
                     request.resource == ManagementResource::ProjectKey)
            {
                row =
                    LR"({"object":"organization.project.api_key","id":"key_offline","name":"Offline key","redacted_value":"not-a-real-key-1234","value":"offline-secret-never-render","created_at":1700000000,"scopes":["api.all.read"],"owner":{"type":"user","user":{"id":"user_offline","name":"Offline owner"}}})";
                if (request.resource == ManagementResource::ProjectKeys)
                    row += LR"(,{"object":"organization.project.api_key","id":"key_project_mixed","name":"Mixed project API key","redacted_value":"…1111"},{"object":"organization.project.api_key","id":"key_legacy_mixed","name":"Mixed legacy API key","redacted_value":"sk-admin...2222"})";
            }
            else if (request.resource == ManagementResource::AdminKeys ||
                     request.resource == ManagementResource::AdminKey)
            {
                row =
                    LR"({"object":"organization.admin_api_key","id":"admin_offline","name":"Offline admin","redacted_value":"…9876","created_at":1700000000},{"object":"organization.admin_api_key","id":"key_project_mixed","name":"Mixed project API key","redacted_value":"…1111"},{"object":"organization.admin_api_key","id":"key_legacy_mixed","name":"Mixed legacy API key","redacted_value":"sk-...2222"},{"object":"organization.admin_api_key","id":"key_generic_admin","name":"Offline generic-mask admin","redacted_value":"sk-...3333"})";
                if (adminIssued && !adminRevoked)
                    row += LR"(,{"object":"organization.admin_api_key","id":"key_new","name":"Offline newly issued admin","redacted_value":"…5678","created_at":1700000001})";
            }
            else if (request.resource == ManagementResource::Members || request.resource == ManagementResource::Member)
                row =
                    LR"({"object":"organization.project.user","id":"user_offline","name":"Offline member","role":"member","added_at":1700000000})";
            else if (request.resource == ManagementResource::RateLimits ||
                     request.resource == ManagementResource::RateLimit)
                row =
                    LR"({"object":"project.rate_limit","id":"rl_offline","model":"offline-model","max_requests_per_1_minute":100,"max_tokens_per_1_minute":1000})";
            if (request.method == ManagementMethod::Delete)
            {
                if (request.resource == ManagementResource::AdminKey)
                {
                    auto statusCode = adminDeleteStatus.load();
                    if (statusCode == 1404) // Tests-only variant: ordinary JSON 404 with response metadata.
                    {
                        ApiResponse response{404, {}, L"req_offline_delete_json_404"};
                        response.diagnostics.content = ResponseContentKind::Json;
                        response.diagnostics.requestIdState = ResponseRequestIdState::Valid;
                        response.diagnostics.openAiMetadata = true;
                        response.diagnostics.body = InspectErrorResponse(R"({"error":{"type":"not_found_error","code":"resource_not_found","message":"private rejection text"}})");
                        return response;
                    }
                    if (statusCode == 404)
                    {
                        ApiResponse response{404, {}};
                        response.diagnostics.content = ResponseContentKind::Unknown;
                        response.diagnostics.requestIdState = ResponseRequestIdState::Missing;
                        response.diagnostics.clientRequestId = L"550e8400-e29b-41d4-a716-446655440000";
                        response.diagnostics.body = InspectErrorResponse("");
                        return response;
                    }
                    if (statusCode != 200)
                        return {statusCode, L"private rejection text", L"req_offline_delete_" + std::to_wstring(statusCode)};
                    if (request.item != L"key_new") return {404, L"Not an issued fixture key"};
                    adminRevoked = true;
                    return {200, LR"({"id":"key_new","object":"organization.admin_api_key.deleted","deleted":true})"};
                }
                return {200, L"{\"id\":\"" + request.item + L"\",\"object\":\"organization.project.api_key.deleted\",\"deleted\":true}"};
            }
            if (request.method == ManagementMethod::Post)
                return {200, row};
            return {200, L"{\"object\":\"list\",\"data\":[" + row + L"],\"has_more\":false}"};
        }
    };
} // namespace
namespace winrt::AetherNexus::implementation
{
    App::App()
    {
        UnhandledException([](auto const &, UnhandledExceptionEventArgs const &args) {
            wchar_t path[32768]{};
            GetModuleFileNameW(nullptr, path, 32768);
            std::ofstream{std::filesystem::path{path}.parent_path() / "offline-ui-crash.log"}
                << to_string(args.Message());
        });
    }
    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const &)
    {
        window = Window{};
        window.Title(L"AetherNexus OFFLINE Manage Tests");
        HWND hwnd{};
        window.as<::IWindowNative>()->get_WindowHandle(&hwnd);
        double scale = GetDpiForWindow(hwnd) / 96.0;
        MONITORINFO monitor{sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
        window.AppWindow().MoveAndResize(
            {monitor.rcWork.left + 16, monitor.rcWork.top + 16,
             std::min(static_cast<int32_t>(1280 * scale),
                      static_cast<int32_t>(monitor.rcWork.right - monitor.rcWork.left - 32)),
             std::min(static_cast<int32_t>(960 * scale),
                      static_cast<int32_t>(monitor.rcWork.bottom - monitor.rcWork.top - 32))});
        Controls::NavigationView navigation;
        navigation.IsSettingsVisible(false);
        navigation.IsBackButtonVisible(Controls::NavigationViewBackButtonVisible::Collapsed);
        auto transport = std::make_shared<OfflineManagement>();
        auto pages = std::make_shared<std::map<std::wstring, Controls::Page>>();
        navigation.SelectionChanged([pages, transport](Controls::NavigationView const &sender,
                                                       Controls::NavigationViewSelectionChangedEventArgs const &event) {
            auto item = event.SelectedItem().try_as<Controls::NavigationViewItem>();
            if (!item)
                return;
            auto route = std::wstring{unbox_value<hstring>(item.Tag())};
            auto found = pages->find(route);
            if (found == pages->end())
            {
                AetherNexus::ManagePage page;
                get_self<ManagePage>(page)->Configure(hstring{route}, transport, transport);
                found = pages->emplace(route, page).first;
            }
            sender.Content(found->second);
        });
        for (auto route : {L"usage", L"keys", L"projects"})
        {
            Controls::NavigationViewItem item;
            item.Content(box_value(route));
            item.Tag(box_value(route));
            Automation::AutomationProperties::SetAutomationId(item, L"OfflineNav-" + hstring{route});
            navigation.MenuItems().Append(item);
        }
        Controls::StackPanel footer;
        footer.Spacing(8);
        for (auto value : {L"Light", L"Dark", L"Narrow", L"Wide", L"MissingKey", L"RestoreKey", L"RejectAdminDelete", L"MissingAdminDelete", L"JsonAdminDelete", L"RestoreAdminDelete", L"MissingAdminVerification", L"RestoreAdminVerification"})
        {
            Controls::Button button;
            button.Content(box_value(value));
            Automation::AutomationProperties::SetAutomationId(button, L"Offline" + hstring{value});
            button.Click([navigation, transport, window = window, choice = std::wstring{value}, scale](auto const &,
                                                                                                       auto const &) {
                if (choice == L"Light" || choice == L"Dark")
                    navigation.RequestedTheme(choice == L"Light" ? ElementTheme::Light : ElementTheme::Dark);
                else if (choice == L"MissingKey" || choice == L"RestoreKey")
                    transport->missing = choice == L"MissingKey";
                else if (choice == L"MissingAdminVerification" || choice == L"RestoreAdminVerification")
                    transport->missingAdminVerification = choice == L"MissingAdminVerification";
                else if (choice == L"RejectAdminDelete" || choice == L"MissingAdminDelete" || choice == L"JsonAdminDelete" || choice == L"RestoreAdminDelete")
                    transport->adminDeleteStatus = choice == L"RejectAdminDelete" ? 400u : choice == L"MissingAdminDelete" ? 404u : choice == L"JsonAdminDelete" ? 1404u : 200u;
                else
                    window.AppWindow().Resize({static_cast<int32_t>((choice == L"Narrow" ? 760 : 1280) * scale),
                                               static_cast<int32_t>(900 * scale)});
            });
            footer.Children().Append(button);
        }
        navigation.PaneFooter(footer);
        window.Closed([pages](auto const &, auto const &) {
            for (auto const &[route, page] : *pages) { (void)route; get_self<ManagePage>(page.as<AetherNexus::ManagePage>())->ClearDisclosure(); }
        });
        window.Content(navigation);
        window.Activate();
        navigation.SelectedItem(navigation.MenuItems().GetAt(0));
    }
} // namespace winrt::AetherNexus::implementation
