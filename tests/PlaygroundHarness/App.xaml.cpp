// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: App.xaml.cpp
// Project: 其他檔案
#include "pch.h"
#include "App.xaml.h"
#include "Services/GenerationClient.h"
#include "Views/PlaygroundPage.xaml.h"
#include <filesystem>
#include <fstream>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.Data.Json.h>
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace ::AetherNexus::Services;
namespace
{
    class OfflineCatalog final : public IApiTransport
    {
        ApiResponse Get(ApiRequest const &request, std::stop_token stop) override
        {
            CheckCancelled(stop);
            if (request.endpoint != Endpoint::Models)
                throw ApiError{ApiErrorCode::Forbidden};
            return {
                200,
                LR"({"object":"list","data":[{"object":"model","id":"gpt-6-astra","owned_by":"offline"},{"object":"model","id":"gpt-image-2.5-flare","owned_by":"offline"},{"object":"model","id":"gpt-realtime-2.1-mini","owned_by":"offline"},{"object":"model","id":"gpt-transcribe","owned_by":"offline"}]})"};
        }
    };
    class OfflineGeneration final : public IGenerationTransport
    {
        GenerationResponse Send(GenerationRequest const &request, EventCallback const &callback,
                                std::stop_token stop) override
        {
            CheckCancelled(stop);
            if (request.operation == GenerationOperation::Response)
            {
                callback("data: {\"type\":\"response.output_text.delta\",\"delta\":\"Offline UI response\"}\n\n");
                callback(
                    "data: "
                    "{\"type\":\"response.completed\",\"response\":{\"id\":\"offline\",\"status\":\"completed\","
                    "\"output\":[{\"type\":\"message\",\"content\":[{\"type\":\"output_text\",\"text\":\"Offline UI "
                    "response\"}]}]}}\n\n");
                return {200, {}};
            }
            if (request.operation == GenerationOperation::Image || request.operation == GenerationOperation::ImageEdit)
                return {
                    200,
                    R"({"data":[{"b64_json":"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a5l8AAAAASUVORK5CYII="}]})"};
            if (request.operation == GenerationOperation::Realtime)
            {
                std::vector<uint8_t> silence(48000);
                auto audio =
                    std::string{"{\"type\":\"response.output_audio.delta\",\"delta\":\""} + Base64(silence) + "\"}";
                callback(audio);
                callback(R"({"type":"response.output_audio_transcript.delta","delta":"Offline voice"})");
                callback(R"({"type":"response.done","response":{"id":"offline","status":"completed"}})");
                return {200, {}};
            }
            return {200, R"({"text":"Offline transcription"})"};
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
    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const &args)
    {
        window = Window{};
        window.Title(L"AetherNexus OFFLINE Playground Tests");
        HWND hwnd{};
        window.as<::IWindowNative>()->get_WindowHandle(&hwnd);
        double scale = GetDpiForWindow(hwnd) / 96.0;
        MONITORINFO monitor{sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
        window.AppWindow().Resize({std::min(static_cast<int32_t>(1440 * scale),
                                            static_cast<int32_t>(monitor.rcWork.right - monitor.rcWork.left - 32)),
                                   std::min(static_cast<int32_t>(960 * scale),
                                            static_cast<int32_t>(monitor.rcWork.bottom - monitor.rcWork.top - 32))});
        Controls::NavigationView navigation;
        navigation.IsSettingsVisible(false);
        navigation.IsBackButtonVisible(Controls::NavigationViewBackButtonVisible::Collapsed);
        auto pages = std::make_shared<std::map<std::wstring, Controls::Page>>();
        navigation.SelectionChanged([pages](Controls::NavigationView const &sender,
                                            Controls::NavigationViewSelectionChangedEventArgs const &event) {
            auto item = event.SelectedItem().try_as<Controls::NavigationViewItem>();
            if (!item)
                return;
            auto route = std::wstring{unbox_value<hstring>(item.Tag())};
            auto found = pages->find(route);
            if (found == pages->end())
            {
                AetherNexus::PlaygroundPage page;
                get_self<PlaygroundPage>(page)->Configure(hstring{route}, std::make_shared<OfflineCatalog>(),
                                                          std::make_shared<OfflineGeneration>());
                found = pages->emplace(route, page).first;
            }
            sender.Content(found->second);
        });
        for (auto route : {L"chat", L"images", L"audio", L"agent-builder", L"video"})
        {
            Controls::NavigationViewItem item;
            item.Content(box_value(route));
            item.Tag(box_value(route));
            Automation::AutomationProperties::SetAutomationId(item, L"OfflineNav-" + hstring{route});
            navigation.MenuItems().Append(item);
        }
        Controls::StackPanel footer;
        footer.Spacing(8);
        for (auto name : {L"Light", L"Dark", L"Narrow", L"Wide"})
        {
            Controls::Button button;
            button.Content(box_value(name));
            Automation::AutomationProperties::SetAutomationId(button, L"Offline" + hstring{name});
            button.Click([navigation, window = window, name = std::wstring{name}, scale](auto const &, auto const &) {
                if (name == L"Light" || name == L"Dark")
                    navigation.RequestedTheme(name == L"Light" ? ElementTheme::Light : ElementTheme::Dark);
                else
                    window.AppWindow().Resize({static_cast<int32_t>((name == L"Narrow" ? 760 : 1440) * scale),
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
