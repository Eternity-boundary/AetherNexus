// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: SecretClipboard.h
// Project: AetherNexus
#pragma once
#include <windows.h>
#include <string_view>

namespace AetherNexus::Infrastructure
{
// Explicit copy only. Never overwrite a newer clipboard item during cleanup.
class SecretClipboard final
{
  public:
    SecretClipboard() = default;
    ~SecretClipboard()
    {
        (void)Clear();
    }
    SecretClipboard(SecretClipboard const &) = delete;
    SecretClipboard &operator=(SecretClipboard const &) = delete;
    void Copy(HWND owner, std::string_view secret);
    bool Clear() noexcept;

  private:
    HWND m_owner{};
    HGLOBAL m_memory{};
    DWORD m_sequence{};
};
} // namespace AetherNexus::Infrastructure
