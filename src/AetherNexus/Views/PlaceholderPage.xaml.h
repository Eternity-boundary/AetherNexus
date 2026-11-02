// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PlaceholderPage.xaml.h
// Project: AetherNexus
#pragma once
#include "PlaceholderPage.g.h"

namespace winrt::AetherNexus::implementation
{
    struct PlaceholderPage : PlaceholderPageT<PlaceholderPage>
    {
        PlaceholderPage() = default;
        void OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);
    };
}
namespace winrt::AetherNexus::factory_implementation
{
    struct PlaceholderPage : PlaceholderPageT<PlaceholderPage, implementation::PlaceholderPage> {};
}
