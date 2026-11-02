// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: MainWindow.xaml.h
// Project: AetherNexus
#pragma once

#include "MainWindow.g.h"

namespace winrt::AetherNexus::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        void OnRootLoaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnNavigationSelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const&,
            Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const&);
        void OnThemeChanged(Microsoft::UI::Xaml::FrameworkElement const&, Windows::Foundation::IInspectable const&);
    private:
        void UpdateTitleBarTheme();
        bool m_loaded{false};
    };
}

namespace winrt::AetherNexus::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
