// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: BudgetService.cpp
// Project: AetherNexus
#include "pch.h"
#include "BudgetService.h"
#include <winrt/Windows.Storage.h>

namespace AetherNexus::Services
{
    std::optional<int64_t> BudgetService::Load()
    {
        auto const values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        if (auto const stored = values.TryLookup(L"Budget.MonthlyUsdCents"))
        {
            auto const cents = winrt::unbox_value_or<int64_t>(stored, 0);
            if (cents <= 0 || cents > MaximumCents) throw winrt::hresult_invalid_argument();
            return cents;
        }
        return {};
    }
    void BudgetService::Save(std::optional<int64_t> cents)
    {
        auto const values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        if (cents)
        {
            if (*cents <= 0 || *cents > MaximumCents) throw winrt::hresult_invalid_argument();
            values.Insert(L"Budget.MonthlyUsdCents", winrt::box_value(*cents));
        }
        else if (values.HasKey(L"Budget.MonthlyUsdCents")) values.Remove(L"Budget.MonthlyUsdCents");
    }
}
