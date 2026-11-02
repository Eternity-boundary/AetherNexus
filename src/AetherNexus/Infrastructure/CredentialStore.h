// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: CredentialStore.h
// Project: AetherNexus
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace AetherNexus::Infrastructure
{
    enum class TokenKind { Model, Admin };

    // Mutable, noncopyable temporary storage. Never bind this to XAML or log it.
    class SecureToken final
    {
    public:
        explicit SecureToken(std::wstring_view value);
        ~SecureToken();
        SecureToken(SecureToken const&) = delete;
        SecureToken& operator=(SecureToken const&) = delete;
        SecureToken(SecureToken&&) = delete;
        SecureToken& operator=(SecureToken&&) = delete;
        std::wstring_view View() const noexcept { return {m_value.data(), m_value.size()}; }
        void Clear() noexcept;
    private:
        friend class CredentialStore;
        std::vector<wchar_t> m_value;
    };

    class CredentialStore final
    {
    public:
        CredentialStore();
        // Separate namespaces allow integration tests without touching user credentials.
        explicit CredentialStore(std::wstring targetPrefix);
        static bool IsValidInput(std::wstring_view value) noexcept;
        bool Has(TokenKind kind) const;
        std::unique_ptr<SecureToken> Read(TokenKind kind) const;
        void Save(TokenKind kind, std::wstring_view value) const;
        void Remove(TokenKind kind) const;
    private:
        std::wstring Target(TokenKind kind) const;
        std::wstring m_prefix;
    };
}
