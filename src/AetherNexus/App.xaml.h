// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: App.xaml.h
// Project: AetherNexus
#pragma once

#include "App.xaml.g.h"

namespace winrt::AetherNexus::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

    private:
        winrt::Microsoft::UI::Xaml::Window window{ nullptr };
    };
}
