// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ManageItem.h
// Project: AetherNexus
#pragma once
#include "ManageItem.g.h"
namespace winrt::AetherNexus::implementation
{
    struct ManageItem : ManageItemT<ManageItem>
    {
        ManageItem() = default;
        hstring Id() const
        {
            return m_id;
        }
        hstring Title() const
        {
            return m_title;
        }
        hstring Subtitle() const
        {
            return m_subtitle;
        }
        hstring Detail() const
        {
            return m_detail;
        }
        void Set(hstring id, hstring title, hstring subtitle, hstring detail)
        {
            m_id = id;
            m_title = title;
            m_subtitle = subtitle;
            m_detail = detail;
        }

      private:
        hstring m_id, m_title, m_subtitle, m_detail;
    };
} // namespace winrt::AetherNexus::implementation
namespace winrt::AetherNexus::factory_implementation
{
    struct ManageItem : ManageItemT<ManageItem, implementation::ManageItem>
    {
    };
} // namespace winrt::AetherNexus::factory_implementation
