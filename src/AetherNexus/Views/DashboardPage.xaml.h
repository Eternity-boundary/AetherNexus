// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: DashboardPage.xaml.h
// Project: AetherNexus
#pragma once
#include "DashboardPage.g.h"
#include "ViewModels/DashboardViewModel.h"

namespace winrt::AetherNexus::implementation
{
    struct DashboardPage : DashboardPageT<DashboardPage>
    {
        DashboardPage() = default;
        void OnLoaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnRangeChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void OnRefresh(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnCardsSizeChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::SizeChangedEventArgs const&);
        void OnThemeChanged(Microsoft::UI::Xaml::FrameworkElement const&, Windows::Foundation::IInspectable const&);
    private:
        void UpdatePreview();
        void DrawCharts();
        ::AetherNexus::ViewModels::DashboardViewModel m_viewModel;
        bool m_ready{false};
        int m_columns{0};
    };
}
namespace winrt::AetherNexus::factory_implementation
{
    struct DashboardPage : DashboardPageT<DashboardPage, implementation::DashboardPage> {};
}
