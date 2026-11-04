// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: BuildPage.xaml.h
// Project: AetherNexus
#pragma once
#include "BuildPage.g.h"
#include "Services/BuildClient.h"
#include "Services/PromptStore.h"
namespace winrt::AetherNexus::implementation
{
struct BuildPage : BuildPageT<BuildPage>
{
    BuildPage() = default;
    void Configure(hstring const &, std::shared_ptr<::AetherNexus::Services::IBuildTransport>,
                   std::shared_ptr<::AetherNexus::Services::IPromptStore>);
    fire_and_forget OnLoaded(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    void OnUnloaded(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    void OnView(Windows::Foundation::IInspectable const &,
                Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
    void OnSelection(Windows::Foundation::IInspectable const &,
                     Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
    void OnVersion(Windows::Foundation::IInspectable const &,
                   Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
    void OnSearch(Microsoft::UI::Xaml::Controls::AutoSuggestBox const &,
                  Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const &);
    void OnSize(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::SizeChangedEventArgs const &);
    void OnSettings(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    void OnStop(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    void OnScope(Windows::Foundation::IInspectable const &,
                 Microsoft::UI::Xaml::Controls::TextChangedEventArgs const &);
    void OnContainer(Microsoft::UI::Xaml::Controls::ListViewBase const &,
                     Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const &);
    fire_and_forget OnRefresh(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnMore(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnCreate(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnRemove(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnChildren(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnDownload(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnExport(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnImport(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnInspect(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnStoreSearch(Windows::Foundation::IInspectable const &,
                                  Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnSavePrompt(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget OnRunPrompt(Windows::Foundation::IInspectable const &,
                                Microsoft::UI::Xaml::RoutedEventArgs const &);

  private:
    bool Local() const;
    void NoticeText(std::wstring const &, Microsoft::UI::Xaml::Controls::InfoBarSeverity =
                                              Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
    void Error(bool mutation = false);
    void Busy(bool);
    void Actions();
    void Render();
    void Show(::AetherNexus::Services::BuildRecord const &);
    void EditPrompt(::AetherNexus::Services::PromptRevision const &);
    ::AetherNexus::Services::PromptRevision Draft();
    ::AetherNexus::Services::BuildRecord const *Selected();
    ::AetherNexus::Services::BuildRequest Query();
    Windows::Foundation::IAsyncAction Load(bool more = false);
    Windows::Foundation::IAsyncAction Execute(::AetherNexus::Services::BuildRequest, bool mutation);
    Windows::Foundation::IAsyncOperation<bool> Confirm(std::wstring title, std::wstring text);
    std::wstring m_mode, m_after, m_eval, m_dataProject, m_dataParent;
    bool m_ready{}, m_loaded{}, m_busy{}, m_dialog{}, m_more{}, m_rendering{}, m_valid{};
    uint64_t m_version{};
    std::stop_source m_stop;
    std::shared_ptr<::AetherNexus::Services::IBuildTransport> m_transport;
    std::shared_ptr<::AetherNexus::Services::IPromptStore> m_store;
    std::vector<::AetherNexus::Services::BuildResource> m_views;
    std::vector<::AetherNexus::Services::BuildRecord> m_records;
    std::vector<::AetherNexus::Services::PromptRevision> m_prompts, m_versions;
    ::AetherNexus::Services::PromptRevision m_draft;
};
} // namespace winrt::AetherNexus::implementation
namespace winrt::AetherNexus::factory_implementation
{
struct BuildPage : BuildPageT<BuildPage, implementation::BuildPage>
{
};
} // namespace winrt::AetherNexus::factory_implementation
