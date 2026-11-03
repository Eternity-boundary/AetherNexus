// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: Charts.h
// Project: AetherNexus
#pragma once

#include <span>

namespace AetherNexus::Views::Charts
{
    void Line(winrt::Microsoft::UI::Xaml::Controls::Canvas const& canvas,
        std::span<double const> values, winrt::Microsoft::UI::Xaml::Media::Brush const& brush,
        double maximum = 0, bool clear = true, bool fill = true);
    void Bars(winrt::Microsoft::UI::Xaml::Controls::Canvas const& canvas,
        std::span<double const> values, winrt::Microsoft::UI::Xaml::Media::Brush const& brush);
}
