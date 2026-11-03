// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: SecretClipboard.cpp
// Project: AetherNexus
#include "SecretClipboard.h"
#include <algorithm>
#include <stdexcept>
#include <winrt/base.h>
#pragma comment(lib, "user32.lib")

namespace AetherNexus::Infrastructure
{
namespace
{
struct ClipboardLock
{
    explicit ClipboardLock(HWND owner)
    {
        if (!OpenClipboard(owner))
            winrt::throw_last_error();
    }
    ~ClipboardLock()
    {
        CloseClipboard();
    }
    ClipboardLock(ClipboardLock const &) = delete;
};
struct Memory
{
    HGLOBAL handle{};
    explicit Memory(size_t bytes) : handle(GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes))
    {
        if (!handle)
            winrt::throw_last_error();
    }
    ~Memory()
    {
        if (handle)
        {
            if (auto data = GlobalLock(handle))
            {
                SecureZeroMemory(data, GlobalSize(handle));
                GlobalUnlock(handle);
            }
            GlobalFree(handle);
        }
    }
    Memory(Memory const &) = delete;
    void Put(UINT format)
    {
        if (!SetClipboardData(format, handle))
            winrt::throw_last_error();
        handle = nullptr; // Ownership has moved to Windows.
    }
};
} // namespace
void SecretClipboard::Copy(HWND owner, std::string_view secret)
{
    if (!owner || secret.empty() || secret.size() > 4096 ||
        !std::all_of(secret.begin(), secret.end(), [](unsigned char c) { return c >= 0x21 && c <= 0x7e; }))
        throw std::invalid_argument("Invalid secret clipboard input");
    if (!Clear())
        throw std::runtime_error("Secret clipboard is busy");
    ClipboardLock lock{owner};
    if (!EmptyClipboard())
        winrt::throw_last_error();
    try
    {
        // Set all opt-out formats BEFORE publishing the text.
        for (auto name : {L"ExcludeClipboardContentFromMonitorProcessing", L"CanIncludeInClipboardHistory",
                          L"CanUploadToCloudClipboard"})
        {
            auto format = RegisterClipboardFormatW(name);
            if (!format)
                winrt::throw_last_error();
            Memory flag{sizeof(DWORD)}; // zero opts out of history / cloud upload
            flag.Put(format);
        }
        Memory text{(secret.size() + 1) * sizeof(wchar_t)};
        auto data = static_cast<wchar_t *>(GlobalLock(text.handle));
        if (!data)
            winrt::throw_last_error();
        for (size_t i = 0; i < secret.size(); ++i)
            data[i] = static_cast<wchar_t>(secret[i]);
        data[secret.size()] = L'\0';
        GlobalUnlock(text.handle);
        auto memory = text.handle;
        text.Put(CF_UNICODETEXT);
        m_owner = owner;
        m_memory = memory;
        m_sequence = GetClipboardSequenceNumber();
    }
    catch (...)
    {
        EmptyClipboard();
        throw;
    }
}
bool SecretClipboard::Clear() noexcept
{
    if (!m_owner)
        return true;
    // A different owner/sequence means our handle may already be freed; never access it.
    if (GetClipboardOwner() != m_owner || GetClipboardSequenceNumber() != m_sequence)
    {
        m_owner = nullptr;
        m_memory = nullptr;
        return true;
    }
    if (!OpenClipboard(m_owner))
        return false;
    bool cleared = true;
    if (GetClipboardOwner() == m_owner && GetClipboardSequenceNumber() == m_sequence)
    {
        if (GetClipboardData(CF_UNICODETEXT) != m_memory)
            cleared = false;
        else if (auto data = GlobalLock(m_memory))
        {
            SecureZeroMemory(data, GlobalSize(m_memory));
            GlobalUnlock(m_memory);
            cleared = EmptyClipboard() != FALSE;
        }
        else
            cleared = false;
    }
    CloseClipboard();
    if (cleared)
    {
        m_owner = nullptr;
        m_memory = nullptr;
    }
    return cleared;
}
} // namespace AetherNexus::Infrastructure
