// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: SettingsPage.xaml.cpp
// Project: AetherNexus
#include "pch.h"
#include "SettingsPage.xaml.h"
#include "SettingsPage.g.cpp"
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <exception>
#include <memory>

namespace winrt::AetherNexus::implementation
{
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;
    using ::AetherNexus::Infrastructure::SecureToken;
    using ::AetherNexus::Infrastructure::CredentialStore;
    using ::AetherNexus::Services::ThemeService;

    hstring SettingsPage::Text(wchar_t const* key) const
    {
        return Microsoft::Windows::ApplicationModel::Resources::ResourceLoader{}.GetString(key);
    }
    void SettingsPage::Notify(wchar_t const* key, InfoBarSeverity severity)
    {
        Feedback().Severity(severity);
        Feedback().Message(Text(key));
        Feedback().IsOpen(true);
    }
    size_t SettingsPage::Index(TokenKind kind) noexcept { return kind == TokenKind::Model ? 0 : 1; }
    SettingsPage::TokenKind SettingsPage::Kind(Windows::Foundation::IInspectable const& sender)
    {
        return unbox_value<hstring>(sender.as<Button>().Tag()) == L"model" ? TokenKind::Model : TokenKind::Admin;
    }
    void SettingsPage::SetBusy(bool value)
    {
        m_busy = value;
        ModelKeyInput().IsEnabled(!value);
        AdminKeyInput().IsEnabled(!value);
        UpdateControls();
    }
    void SettingsPage::UpdateControls()
    {
        auto const stateText = [this](StorageState state)
        {
            switch (state)
            {
            case StorageState::Stored: return Text(L"KeyStored");
            case StorageState::Missing: return Text(L"KeyMissing");
            case StorageState::Unavailable: return Text(L"KeyUnavailable");
            default: return Text(L"KeyLoading");
            }
        };
        ModelStatus().Text(stateText(m_states[0]));
        AdminStatus().Text(stateText(m_states[1]));
        auto const available = [](StorageState state) { return state == StorageState::Stored || state == StorageState::Missing; };
        SaveModelButton().IsEnabled(!m_busy && available(m_states[0]) && !ModelKeyInput().Password().empty());
        SaveAdminButton().IsEnabled(!m_busy && available(m_states[1]) && !AdminKeyInput().Password().empty());
        RemoveModelButton().IsEnabled(!m_busy && m_states[0] == StorageState::Stored);
        RemoveAdminButton().IsEnabled(!m_busy && m_states[1] == StorageState::Stored);
    }
    fire_and_forget SettingsPage::OnLoaded(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        try
        {
            m_ready = false;
            try { m_theme = ThemeService::Load(); }
            catch (...) { Notify(L"ThemeLoadFailed", InfoBarSeverity::Warning); }
            ThemePicker().SelectedIndex(static_cast<int32_t>(m_theme));
            m_ready = true;
            SetBusy(true);
            apartment_context ui;
            auto states = m_states;
            co_await resume_background();
            for (auto const kind : {TokenKind::Model, TokenKind::Admin})
            {
                try { states[Index(kind)] = m_store.Has(kind) ? StorageState::Stored : StorageState::Missing; }
                catch (...) { states[Index(kind)] = StorageState::Unavailable; }
            }
            co_await ui;
            m_states = states;
            SetBusy(false);
            m_ready = IsLoaded();
        }
        catch (...)
        {
            SetBusy(false);
            Notify(L"StorageFailed", InfoBarSeverity::Error);
        }
    }
    void SettingsPage::OnUnloaded(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        m_ready = false;
        ModelKeyInput().Password(L"");
        AdminKeyInput().Password(L"");
    }
    void SettingsPage::OnPasswordChanged(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        if (m_ready) UpdateControls();
    }
    void SettingsPage::OnThemeSelectionChanged(Windows::Foundation::IInspectable const&, SelectionChangedEventArgs const&)
    {
        if (!m_ready) return;
        auto const selected = ThemePicker().SelectedIndex();
        if (selected < 0 || selected > 2) return;
        auto const preference = static_cast<ThemePreference>(selected);
        try
        {
            ThemeService::Apply(XamlRoot().Content().as<FrameworkElement>(), preference);
            ThemeService::Save(preference);
            m_theme = preference;
            Notify(L"ThemeSaved", InfoBarSeverity::Success);
        }
        catch (...)
        {
            try { ThemeService::Apply(XamlRoot().Content().as<FrameworkElement>(), m_theme); }
            catch (...) { /* Preserve the existing window if its root is unavailable. */ }
            m_ready = false;
            ThemePicker().SelectedIndex(static_cast<int32_t>(m_theme));
            m_ready = true;
            Notify(L"ThemeSaveFailed", InfoBarSeverity::Error);
        }
    }
    Windows::Foundation::IAsyncOperation<ContentDialogResult> SettingsPage::Confirm(TokenKind kind, bool remove)
    {
        ContentDialog dialog;
        dialog.XamlRoot(XamlRoot());
        dialog.RequestedTheme(ActualTheme());
        dialog.Title(box_value(Text(kind == TokenKind::Model ? L"ModelKeyName" : L"AdminKeyName")));
        dialog.Content(box_value(Text(remove ? L"RemoveKeyPrompt" : L"ReplaceKeyPrompt")));
        dialog.PrimaryButtonText(Text(remove ? L"RemoveKeyAction" : L"ReplaceKeyAction"));
        dialog.CloseButtonText(Text(L"CancelAction"));
        dialog.DefaultButton(ContentDialogButton::Close);
        co_return co_await dialog.ShowAsync();
    }
    fire_and_forget SettingsPage::OnSaveClick(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        if (m_busy || !m_ready) co_return;
        std::unique_ptr<SecureToken> secret;
        try
        {
            auto const kind = Kind(sender);
            auto const index = Index(kind);
            auto const input = kind == TokenKind::Model ? ModelKeyInput() : AdminKeyInput();
            {
                // Do not retain an immutable hstring across a suspension point.
                auto const entered = input.Password();
                if (!CredentialStore::IsValidInput({entered.c_str(), entered.size()}))
                {
                    input.Password(L"");
                    Notify(L"InvalidKeyInput", InfoBarSeverity::Warning);
                    co_return;
                }
                secret = std::make_unique<SecureToken>(std::wstring_view{entered.c_str(), entered.size()});
            }
            input.Password(L"");
            SetBusy(true);
            if (m_states[index] == StorageState::Stored && co_await Confirm(kind, false) != ContentDialogResult::Primary)
            {
                secret->Clear();
                SetBusy(false);
                co_return;
            }
            bool failed = false;
            apartment_context ui;
            co_await resume_background();
            try { m_store.Save(kind, secret->View()); }
            catch (...) { failed = true; }
            secret->Clear();
            co_await ui;
            if (failed) Notify(L"StorageFailed", InfoBarSeverity::Error);
            else
            {
                m_states[index] = StorageState::Stored;
                Notify(L"KeySaved", InfoBarSeverity::Success);
            }
            SetBusy(false);
        }
        catch (...)
        {
            secret.reset();
            ModelKeyInput().Password(L"");
            AdminKeyInput().Password(L"");
            SetBusy(false);
            // Never surface exception messages, key prefixes, suffixes, or raw input.
            Notify(L"StorageFailed", InfoBarSeverity::Error);
        }
    }
    fire_and_forget SettingsPage::OnRemoveClick(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        if (m_busy || !m_ready) co_return;
        try
        {
            auto const kind = Kind(sender);
            SetBusy(true);
            if (co_await Confirm(kind, true) != ContentDialogResult::Primary)
            {
                SetBusy(false);
                co_return;
            }
            bool failed = false;
            apartment_context ui;
            co_await resume_background();
            try { m_store.Remove(kind); }
            catch (...) { failed = true; }
            co_await ui;
            if (failed) Notify(L"StorageFailed", InfoBarSeverity::Error);
            else
            {
                m_states[Index(kind)] = StorageState::Missing;
                (kind == TokenKind::Model ? ModelKeyInput() : AdminKeyInput()).Password(L"");
                Notify(L"KeyRemoved", InfoBarSeverity::Success);
            }
            SetBusy(false);
        }
        catch (...)
        {
            SetBusy(false);
            Notify(L"StorageFailed", InfoBarSeverity::Error);
        }
    }
}
