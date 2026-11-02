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
        double costUsd{};
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
        // A local planning value, not an organization budget or account balance.
        double localBudgetUsd{100.0};
        double cacheRatio() const noexcept
        {
            return inputTokens == 0 ? 0.0 : static_cast<double>(cachedInputTokens) / inputTokens;
        }
    };
}
