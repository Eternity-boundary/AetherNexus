// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: UsageSnapshot.h
// Project: AetherNexus
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace AetherNexus::Models
{
    struct UsageBucket
    {
        std::chrono::sys_days date;
        int64_t inputTokens{};
        int64_t outputTokens{};
        int64_t cachedInputTokens{};
        int64_t requests{};
        bool cacheAvailable{true};
        std::optional<double> cacheRatio() const noexcept
        {
            if (!cacheAvailable || inputTokens == 0) return {};
            return static_cast<double>(cachedInputTokens) / static_cast<double>(inputTokens);
        }
    };

    struct UsageSnapshot
    {
        std::vector<UsageBucket> buckets;
        int64_t inputTokens{};
        int64_t outputTokens{};
        int64_t cachedInputTokens{};
        int64_t requests{};
        int64_t todayTokens{};
        double monthCostUsd{};
        bool usageAvailable{false};
        bool costsAvailable{false};
        bool cacheAvailable{true};
        std::optional<double> cacheRatio() const noexcept
        {
            if (!usageAvailable || !cacheAvailable || inputTokens == 0) return {};
            return static_cast<double>(cachedInputTokens) / static_cast<double>(inputTokens);
        }
    };
}
