// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: SettingsPage.xaml.h
// Project: AetherNexus
#pragma once
#include "SettingsPage.g.h"
#include "Infrastructure/CredentialStore.h"
#include "Services/ThemeService.h"
#include <array>

namespace winrt::AetherNexus::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage() = default;
        fire_and_forget OnLoaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnUnloaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnThemeSelectionChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void OnPasswordChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnBudgetToggled(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnSaveBudget(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        fire_and_forget OnSaveClick(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        fire_and_forget OnRemoveClick(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
    private:
        enum class StorageState { Unknown, Missing, Stored, Unavailable };
        using TokenKind = ::AetherNexus::Infrastructure::TokenKind;
        using ThemePreference = ::AetherNexus::Services::ThemePreference;
        hstring Text(wchar_t const* key) const;
        void Notify(wchar_t const* key, Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);
        void UpdateControls();
        void SetBusy(bool value);
        Windows::Foundation::IAsyncOperation<Microsoft::UI::Xaml::Controls::ContentDialogResult> Confirm(TokenKind kind, bool remove);
        static TokenKind Kind(Windows::Foundation::IInspectable const& sender);
        static size_t Index(TokenKind kind) noexcept;
        bool m_ready{false};
        bool m_busy{false};
        ThemePreference m_theme{ThemePreference::System};
        std::array<StorageState, 2> m_states{{StorageState::Unknown, StorageState::Unknown}};
        ::AetherNexus::Infrastructure::CredentialStore m_store;
    };
}
namespace winrt::AetherNexus::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage> {};
}
