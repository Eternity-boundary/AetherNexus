// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: BudgetService.h
// Project: AetherNexus
#pragma once
#include <cmath>
#include <cstdint>
#include <optional>

namespace AetherNexus::Services
{
    class BudgetService final
    {
    public:
        static constexpr int64_t MaximumCents = 100000000000LL;
        static std::optional<int64_t> Load();
        static void Save(std::optional<int64_t> cents);
        static bool TryConvertUsd(double value, int64_t& cents) noexcept
        {
            if (!std::isfinite(value) || value < 0.01 || value > MaximumCents / 100.0) return false;
            auto const rounded = static_cast<int64_t>(std::llround(value * 100.0));
            if (std::abs(value - static_cast<double>(rounded) / 100.0) > 0.000001) return false;
            cents = rounded;
            return true;
        }
    };
}
