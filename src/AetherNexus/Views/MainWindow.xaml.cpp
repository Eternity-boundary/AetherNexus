// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: MainWindow.xaml.cpp
// Project: AetherNexus
#include "pch.h"
#include "MainWindow.xaml.h"
#include "DashboardPage.xaml.h"
#include "PlaceholderPage.xaml.h"
#include "SettingsPage.xaml.h"
#include "PlaygroundPage.xaml.h"
#include "Services/WindowsHttpTransport.h"
#include "Services/WindowsGenerationTransport.h"
#include "Services/ThemeService.h"
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.h>
#include <algorithm>
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::AetherNexus::implementation
{
    MainWindow::MainWindow()
    {
        // C++/WinRT's optimized factory initializes XAML after construction.
        SystemBackdrop(Media::MicaBackdrop{});
        ExtendsContentIntoTitleBar(true);
        HWND hwnd{};
        check_hresult(this->try_as<::IWindowNative>()->get_WindowHandle(&hwnd));
        auto const scale = static_cast<double>(GetDpiForWindow(hwnd)) / 96.0;
        int32_t width = static_cast<int32_t>(1440 * scale);
        int32_t height = static_cast<int32_t>(960 * scale);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            auto const& work = monitor.rcWork;
            width = std::min(width, static_cast<int32_t>(work.right - work.left - 32));
            height = std::min(height, static_cast<int32_t>(work.bottom - work.top - 32));
            AppWindow().MoveAndResize({work.left + (work.right - work.left - width) / 2,
                work.top + (work.bottom - work.top - height) / 2, width, height});
        }
        else AppWindow().Resize({width, height});
    }

    void MainWindow::OnRootLoaded(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        SetTitleBar(TitleBarDragRegion());
        m_loaded = true;
        try { ::AetherNexus::Services::ThemeService::Apply(Root(), ::AetherNexus::Services::ThemeService::Load()); }
        catch (...) { Root().RequestedTheme(ElementTheme::Default); }
        if (!m_uiSettings)
        {
            m_uiSettings = Windows::UI::ViewManagement::UISettings{};
            m_colorRevoker = m_uiSettings.ColorValuesChanged(auto_revoke, [weak = get_weak(), queue = DispatcherQueue()](auto const&, auto const&)
            {
                queue.TryEnqueue([weak]
                {
                    if (auto self = weak.get())
                    {
                        try
                        {
                            using namespace ::AetherNexus::Services;
                            if (ThemeService::Load() == ThemePreference::System)
                                ThemeService::Apply(self->Root(), ThemePreference::System);
                        }
                        catch (...) { /* Preserve the current theme if settings are unavailable. */ }
                    }
                });
            });
        }
        UpdateTitleBarTheme();
        Navigation().SelectedItem(DashboardItem());
    }

    void MainWindow::UpdateTitleBarTheme()
    {
        if (auto const foreground = WindowTitle().Foreground().try_as<Media::SolidColorBrush>())
        {
            auto const titleBar = AppWindow().TitleBar();
            titleBar.ButtonForegroundColor(foreground.Color());
            titleBar.ButtonHoverForegroundColor(foreground.Color());
            titleBar.ButtonBackgroundColor(Windows::UI::Colors::Transparent());
            titleBar.ButtonInactiveBackgroundColor(Windows::UI::Colors::Transparent());
        }
    }

    void MainWindow::OnThemeChanged(FrameworkElement const&, Windows::Foundation::IInspectable const&)
    {
        if (m_loaded) DispatcherQueue().TryEnqueue([weak = get_weak()] { if (auto self = weak.get()) self->UpdateTitleBarTheme(); });
    }

    void MainWindow::OnNavigationSelectionChanged(Controls::NavigationView const&,
        Controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        if (args.IsSettingsSelected())
        {
            ContentFrame().Navigate(xaml_typename<AetherNexus::SettingsPage>());
        }
        else if (auto const item = args.SelectedItem().try_as<Controls::NavigationViewItem>())
        {
            auto const route = std::wstring{unbox_value<hstring>(item.Tag())};
            if (route == L"dashboard")
                ContentFrame().Navigate(xaml_typename<AetherNexus::DashboardPage>());
            else if (route == L"chat" || route == L"images" || route == L"audio" || route == L"video" || route == L"agent-builder")
            {
                auto found = m_playgrounds.find(route);
                if (found == m_playgrounds.end())
                {
                    AetherNexus::PlaygroundPage page;
                    get_self<PlaygroundPage>(page)->Configure(hstring{route},std::make_shared<::AetherNexus::Services::WindowsHttpTransport>(),std::make_shared<::AetherNexus::Services::WindowsGenerationTransport>());
                    found = m_playgrounds.emplace(route, page).first;
                }
                ContentFrame().Content(found->second);
            }
            else
                ContentFrame().Navigate(xaml_typename<AetherNexus::PlaceholderPage>(), item.Content());
        }
        if (Navigation().DisplayMode() != Controls::NavigationViewDisplayMode::Expanded)
            Navigation().IsPaneOpen(false);
    }
}
