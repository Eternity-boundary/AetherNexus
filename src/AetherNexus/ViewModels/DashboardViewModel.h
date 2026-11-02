// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: DashboardViewModel.h
// Project: AetherNexus
#pragma once

#include "Models/UsageSnapshot.h"
#include <string>
#include <utility>

namespace AetherNexus::ViewModels
{
    // Presentation state contains only parsed server data, never credentials.
    class DashboardViewModel
    {
    public:
        void SetSnapshot(Models::UsageSnapshot snapshot) { m_snapshot = std::move(snapshot); }
        Models::UsageSnapshot const& Snapshot() const noexcept { return m_snapshot; }
        static std::wstring Number(int64_t value);
        static std::wstring Money(double value);
        static std::wstring Percent(double ratio);
        static std::wstring Date(std::chrono::sys_days date);

    private:
        Models::UsageSnapshot m_snapshot;
    };
}
