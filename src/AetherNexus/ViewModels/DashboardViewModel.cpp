// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: DashboardViewModel.cpp
// Project: AetherNexus
#include "DashboardViewModel.h"
#include <algorithm>
#include <format>
#include <limits>

namespace AetherNexus::ViewModels
{
    DashboardChartData DashboardViewModel::Charts() const
    {
        DashboardChartData result;
        if (!m_snapshot.usageAvailable) return result;
        for (auto const& bucket : m_snapshot.buckets)
        {
            result.input.push_back(static_cast<double>(bucket.inputTokens));
            result.output.push_back(static_cast<double>(bucket.outputTokens));
            // Unknown cached counts are gaps, including in the count chart.
            result.cachedInput.push_back(bucket.cacheAvailable ? static_cast<double>(bucket.cachedInputTokens)
                : std::numeric_limits<double>::quiet_NaN());
            result.requests.push_back(static_cast<double>(bucket.requests));
            result.cacheRatios.push_back(bucket.cacheRatio().value_or(std::numeric_limits<double>::quiet_NaN()));
            result.tokenMaximum = std::max({result.tokenMaximum, result.input.back(), result.output.back()});
        }
        result.tokenMaximum *= 1.15;
        return result;
    }

    std::wstring DashboardViewModel::Number(int64_t value)
    {
        auto text = std::to_wstring(value);
        for (auto i = static_cast<int>(text.size()) - 3; i > 0; i -= 3) text.insert(static_cast<size_t>(i), L",");
        return text;
    }

    std::wstring DashboardViewModel::Money(double value) { return std::format(L"${:.2f}", value); }
    std::wstring DashboardViewModel::Percent(double ratio) { return std::format(L"{:.1f}%", ratio * 100.0); }
    std::wstring DashboardViewModel::Date(std::chrono::sys_days date)
    {
        auto const ymd = std::chrono::year_month_day{date};
        return std::format(L"{:04}/{:02}/{:02}", static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()));
    }
    std::wstring DashboardViewModel::DateTime(std::chrono::sys_seconds time)
    {
        auto const date = std::chrono::floor<std::chrono::days>(time);
        auto const clock = std::chrono::hh_mm_ss{time - date};
        return std::format(L"{} {:02}:{:02}:{:02} UTC", Date(date), clock.hours().count(), clock.minutes().count(), clock.seconds().count());
    }
}
