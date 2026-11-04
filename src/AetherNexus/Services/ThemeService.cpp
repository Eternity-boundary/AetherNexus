// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ThemeService.cpp
// Project: AetherNexus
#include "pch.h"
#include "ThemeService.h"
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace AetherNexus::Services
{
    using namespace winrt;
    using namespace Microsoft::UI::Xaml;
    ThemePreference ThemeService::Load()
    {
        auto const values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        if (auto const stored = values.TryLookup(L"Appearance.Theme"))
        {
            auto const value = unbox_value_or<int32_t>(stored, 0);
            if (value >= 0 && value <= 2) return static_cast<ThemePreference>(value);
        }
        return ThemePreference::System;
    }
    void ThemeService::Save(ThemePreference preference)
    {
        auto const value = static_cast<int32_t>(preference);
        if (value < 0 || value > 2) throw hresult_invalid_argument();
        Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(L"Appearance.Theme", box_value(value));
    }
    void ThemeService::Apply(FrameworkElement const& root, ThemePreference preference)
    {
        if (preference == ThemePreference::System)
        {
            using namespace Windows::UI::ViewManagement;
            auto const foreground = UISettings{}.GetColorValue(UIColorType::Foreground);
            bool const lightForeground = 5 * foreground.G + 2 * foreground.R + foreground.B > 8 * 128;
            root.RequestedTheme(lightForeground ? ElementTheme::Dark : ElementTheme::Light);
        }
        else root.RequestedTheme(preference == ThemePreference::Light ? ElementTheme::Light : ElementTheme::Dark);
    }
}
