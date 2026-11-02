// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: DashboardViewModel.cpp
// Project: AetherNexus
#include "pch.h"
#include "DashboardViewModel.h"
#include <algorithm>
#include <array>
#include <format>

namespace AetherNexus::ViewModels
{
    void DashboardViewModel::SetRange(int days)
    {
        using namespace std::chrono;
        m_snapshot = {};
        auto const today = floor<std::chrono::days>(system_clock::now());
        auto const month = year_month_day{today}.month();
        constexpr std::array<int, 10> activity{38, 82, 100, 88, 66, 51, 60, 45, 74, 62};
        days = std::clamp(days, 1, 30);
        for (int i = 29; i >= 0; --i)
        {
            auto const date = today - std::chrono::days{i};
            auto const weight = activity[static_cast<size_t>(29 - i) % activity.size()];
            Models::UsageBucket bucket{date, weight * 42000LL, weight * 8500LL,
                weight * (18000LL + (i % 4) * 1600LL), weight * 5LL, weight * 0.0055};
            if (year_month_day{date}.month() == month) m_snapshot.monthCostUsd += bucket.costUsd;
            if (i == 0) m_snapshot.todayTokens = bucket.inputTokens + bucket.outputTokens;
            if (i >= days) continue;
            m_snapshot.buckets.push_back(bucket);
            m_snapshot.inputTokens += bucket.inputTokens;
            m_snapshot.outputTokens += bucket.outputTokens;
            m_snapshot.cachedInputTokens += bucket.cachedInputTokens;
            m_snapshot.requests += bucket.requests;
        }
    }

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
