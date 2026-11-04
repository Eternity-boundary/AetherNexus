// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: DashboardViewModel.h
// Project: AetherNexus
#pragma once

#include "Models/UsageSnapshot.h"
#include <string>
#include <utility>
#include <optional>

namespace AetherNexus::ViewModels
{
    struct DashboardChartData
    {
        // NaN denotes an unavailable chart sample; the renderer leaves a gap.
        std::vector<double> input, output, cachedInput, requests, cacheRatios;
        double tokenMaximum{1.0};
    };

    // Presentation state contains only parsed server data, never credentials.
    class DashboardViewModel
    {
    public:
        void SetSnapshot(Models::UsageSnapshot snapshot) { m_snapshot = std::move(snapshot); }
        Models::UsageSnapshot const& Snapshot() const noexcept { return m_snapshot; }
        DashboardChartData Charts() const;
        static std::wstring Number(int64_t value);
        static std::wstring Money(double value);
        static std::wstring Percent(double ratio);
        static std::wstring Date(std::chrono::sys_days date);
        static std::wstring DateTime(std::chrono::sys_seconds time);

    private:
        Models::UsageSnapshot m_snapshot;
    };
}
