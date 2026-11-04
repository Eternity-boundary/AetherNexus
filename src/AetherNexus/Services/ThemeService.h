// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ThemeService.h
// Project: AetherNexus
#pragma once
#include <winrt/Microsoft.UI.Xaml.h>

namespace AetherNexus::Services
{
    enum class ThemePreference : int32_t { System, Light, Dark };
    class ThemeService final
    {
    public:
        static ThemePreference Load();
        static void Save(ThemePreference preference);
        static void Apply(winrt::Microsoft::UI::Xaml::FrameworkElement const& root, ThemePreference preference);
    };
}
