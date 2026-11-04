// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ManagePage.xaml.h
// Project: AetherNexus
#pragma once
#include "ManagePage.g.h"
#include "Services/ManagementClient.h"
#include "Services/KeyIssuance.h"
#include "Infrastructure/SecretClipboard.h"
namespace winrt::AetherNexus::implementation
{
    struct ManagePage : ManagePageT<ManagePage>
    {
        ManagePage() = default;
        void Configure(hstring const &, std::shared_ptr<::AetherNexus::Services::IManagementTransport>,
                       std::shared_ptr<::AetherNexus::Services::IKeyIssuanceTransport> = {});
        void ClearDisclosure() noexcept;
        fire_and_forget OnIssue(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnLoaded(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnUnloaded(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnRefresh(Windows::Foundation::IInspectable const &,
                                  Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnCancel(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnFilter(Windows::Foundation::IInspectable const &,
                      Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
        void OnDateChanged(Microsoft::UI::Xaml::Controls::CalendarDatePicker const &,
                           Microsoft::UI::Xaml::Controls::CalendarDatePickerDateChangedEventArgs const &);
        void OnMetric(Windows::Foundation::IInspectable const &,
                      Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
        void OnSelection(Windows::Foundation::IInspectable const &,
                         Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
        void OnSearch(Microsoft::UI::Xaml::Controls::AutoSuggestBox const &,
                      Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const &);
        fire_and_forget OnCreate(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnEdit(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnRemove(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnExport(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnOfficial(Windows::Foundation::IInspectable const &,
                                   Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnSettings(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnThemeChanged(Microsoft::UI::Xaml::FrameworkElement const &, Windows::Foundation::IInspectable const &);
        void OnChartSizeChanged(Windows::Foundation::IInspectable const &,
                                Microsoft::UI::Xaml::SizeChangedEventArgs const &);

      private:
        hstring Text(wchar_t const *) const;
        void Notify(wchar_t const *, Microsoft::UI::Xaml::Controls::InfoBarSeverity =
                                         Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
        void Error(bool mutation = false);
        void Busy(bool);
        void Actions();
        void Invalidate();
        void FilterUsageChoices();
        void Render();
        void DrawChart();
        std::wstring Project();
        ::AetherNexus::Services::ManagementRecord const *Selected();
        Windows::Foundation::IAsyncAction Load(bool catalog);
        Windows::Foundation::IAsyncAction Mutate(::AetherNexus::Services::ManagementRequest);
        Windows::Foundation::IAsyncAction Edit(bool create);
        Windows::Foundation::IAsyncAction IssueKey();
        struct Disclosure
        {
            ::AetherNexus::Services::IssuedKey key;
            ::AetherNexus::Infrastructure::SecretClipboard clipboard;
            bool Clear() noexcept { key.secret.Clear(); return clipboard.Clear(); }
            ~Disclosure() { (void)Clear(); }
        };
        std::shared_ptr<Disclosure> m_disclosure;
        Microsoft::UI::Xaml::Controls::ContentDialog m_disclosureDialog{nullptr};
        bool m_forcingDisclosureClose{};
        std::shared_ptr<::AetherNexus::Services::IKeyIssuanceTransport> m_issuer;
        std::wstring m_mode, m_dataProject;
        bool m_loaded{}, m_initialized{}, m_busy{}, m_editing{}, m_valid{}, m_mutating{}, m_reload{};
        uint64_t m_version{};
        std::stop_source m_stop;
        std::shared_ptr<::AetherNexus::Services::IManagementTransport> m_transport;
        ::AetherNexus::Services::ManagementResource m_resource{::AetherNexus::Services::ManagementResource::Projects};
        std::vector<::AetherNexus::Services::ManagementRecord> m_projects, m_records;
        std::optional<::AetherNexus::Services::UsageReport> m_report;
    };
} // namespace winrt::AetherNexus::implementation
namespace winrt::AetherNexus::factory_implementation
{
    struct ManagePage : ManagePageT<ManagePage, implementation::ManagePage>
    {
    };
} // namespace winrt::AetherNexus::factory_implementation
