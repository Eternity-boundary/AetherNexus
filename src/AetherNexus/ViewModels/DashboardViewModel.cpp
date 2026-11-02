// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: DashboardViewModel.cpp
// Project: AetherNexus
#include "pch.h"
#include "DashboardViewModel.h"
#include <format>

namespace AetherNexus::ViewModels
{
    std::wstring DashboardViewModel::Number(int64_t value)
    {
        auto text = std::to_wstring(value);
        for (auto i = static_cast<int>(text.size()) - 3; i > 0; i -= 3) text.insert(i, L",");
        return text;
    }

    std::wstring DashboardViewModel::Money(double value) { return std::format(L"${:.2f}", value); }
    std::wstring DashboardViewModel::Percent(double ratio) { return std::format(L"{:.1f}%", ratio * 100.0); }
    std::wstring DashboardViewModel::Date(std::chrono::sys_days date)
    {
        auto const ymd = std::chrono::year_month_day{date};
        return std::format(L"{:02}/{:02}", static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()));
    }
}
