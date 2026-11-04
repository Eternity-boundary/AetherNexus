// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PlaceholderPage.xaml.cpp
// Project: AetherNexus
#include "pch.h"
#include "PlaceholderPage.xaml.h"
#include "PlaceholderPage.g.cpp"

namespace winrt::AetherNexus::implementation
{
    void PlaceholderPage::OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args)
    {
        PageTitle().Text(unbox_value_or<hstring>(args.Parameter(), L"AetherNexus"));
    }
}
