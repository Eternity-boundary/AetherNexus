// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: CredentialStore.cpp
// Project: AetherNexus
#include "CredentialStore.h"
#include <windows.h>
#include <wincred.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>
#pragma comment(lib, "Advapi32.lib")

namespace AetherNexus::Infrastructure
{
    namespace
    {
        struct CredentialDeleter
        {
            void operator()(CREDENTIALW* credential) const noexcept
            {
                if (credential->CredentialBlob && credential->CredentialBlobSize)
                    SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
                CredFree(credential);
            }
        };
        using Credential = std::unique_ptr<CREDENTIALW, CredentialDeleter>;
        [[noreturn]] void StorageError(DWORD error)
        {
            // The context is constant and never contains credential contents.
            throw std::system_error(static_cast<int>(error), std::system_category(), "Credential storage failed");
        }
    }

    SecureToken::SecureToken(std::wstring_view value) : m_value(value.begin(), value.end()) {}
    SecureToken::~SecureToken() { Clear(); }
    void SecureToken::Clear() noexcept
    {
        if (!m_value.empty()) SecureZeroMemory(m_value.data(), m_value.size() * sizeof(wchar_t));
        m_value.clear();
    }

    CredentialStore::CredentialStore() : CredentialStore(L"AetherNexus/377e320d-d8da-47e8-897d-5f57f515c847/v1/") {}
    CredentialStore::CredentialStore(std::wstring targetPrefix) : m_prefix(std::move(targetPrefix))
    {
        if (m_prefix.empty() || m_prefix.size() > 1024)
            throw std::invalid_argument("Invalid credential namespace");
    }
    std::wstring CredentialStore::Target(TokenKind kind) const
    {
        switch (kind)
        {
        case TokenKind::Model: return m_prefix + L"model-api-key";
        case TokenKind::Admin: return m_prefix + L"admin-api-key";
        }
        throw std::invalid_argument("Invalid credential kind");
    }
    bool CredentialStore::IsValidInput(std::wstring_view value) noexcept
    {
        return !value.empty() && value.size() <= CRED_MAX_CREDENTIAL_BLOB_SIZE / sizeof(wchar_t)
            && std::all_of(value.begin(), value.end(), [](wchar_t c) { return c >= L'!' && c <= L'~'; });
    }
    bool CredentialStore::Has(TokenKind kind) const
    {
        CREDENTIALW* raw{};
        if (!CredReadW(Target(kind).c_str(), CRED_TYPE_GENERIC, 0, &raw))
        {
            auto const error = GetLastError();
            if (error == ERROR_NOT_FOUND) return false;
            StorageError(error);
        }
        Credential credential{raw};
        return true;
    }
    void CredentialStore::Save(TokenKind kind, std::wstring_view value) const
    {
        if (!IsValidInput(value)) throw std::invalid_argument("Invalid credential input");
        auto target = Target(kind);
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = target.data();
        credential.UserName = const_cast<wchar_t*>(L"AetherNexus");
        // LOCAL_MACHINE means this machine + this Windows user, NOT all machine users.
        // No enterprise roaming, plaintext setting, file, registry or environment fallback.
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        credential.CredentialBlobSize = static_cast<DWORD>(value.size() * sizeof(wchar_t));
        credential.CredentialBlob = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(value.data()));
        if (!CredWriteW(&credential, 0)) StorageError(GetLastError());
    }
    void CredentialStore::Remove(TokenKind kind) const
    {
        if (!CredDeleteW(Target(kind).c_str(), CRED_TYPE_GENERIC, 0))
        {
            auto const error = GetLastError();
            if (error != ERROR_NOT_FOUND) StorageError(error);
        }
    }
}
