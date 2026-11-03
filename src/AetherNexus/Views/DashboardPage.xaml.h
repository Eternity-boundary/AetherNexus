// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: DashboardPage.xaml.h
// Project: AetherNexus
#pragma once
#include "DashboardPage.g.h"
#include "ViewModels/DashboardViewModel.h"
#include "Services/ApiTypes.h"
#include <stop_token>

namespace winrt::AetherNexus::implementation
{
    struct DashboardPage : DashboardPageT<DashboardPage>
    {
        DashboardPage() = default;
        fire_and_forget OnLoaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnUnloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        fire_and_forget OnRangeChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        fire_and_forget OnRefresh(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        fire_and_forget OnProjectChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void OnOpenSettings(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnCardsSizeChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::SizeChangedEventArgs const&);
        void OnThemeChanged(Microsoft::UI::Xaml::FrameworkElement const&, Windows::Foundation::IInspectable const&);
    private:
        Windows::Foundation::IAsyncAction Reload(bool refreshProjects = false);
        void UpdateProjects();
        hstring ProjectLabel(::AetherNexus::Services::ProjectInfo const& project) const;
        void RenderSnapshot();
        void ShowError(::AetherNexus::Services::ApiErrorCode code, bool partial);
        hstring Text(wchar_t const* key) const;
        void DrawCharts();
        ::AetherNexus::ViewModels::DashboardViewModel m_viewModel;
        bool m_ready{false};
        int m_columns{0};
        uint64_t m_requestVersion{0};
        std::stop_source m_stop;
        std::optional<int64_t> m_monthlyBudgetCents;
        bool m_budgetUnavailable{false};
        bool m_updatingProjects{false};
        std::wstring m_selectedProjectId;
        std::vector<::AetherNexus::Services::ProjectInfo> m_projects;
    };
}
namespace winrt::AetherNexus::factory_implementation
{
    struct DashboardPage : DashboardPageT<DashboardPage, implementation::DashboardPage> {};
}
