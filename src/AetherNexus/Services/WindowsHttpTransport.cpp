// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WindowsHttpTransport.cpp
// Project: AetherNexus
#include "WindowsHttpTransport.h"
#include "Infrastructure/CredentialStore.h"
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <vector>
#pragma comment(lib, "Winhttp.lib")

namespace AetherNexus::Services
{
    namespace
    {
        struct InternetDeleter { void operator()(void* value) const noexcept { if (value) WinHttpCloseHandle(value); } };
        using Internet = std::unique_ptr<void, InternetDeleter>;
        [[noreturn]] void NetworkError()
        {
            throw ApiError{GetLastError() == ERROR_WINHTTP_TIMEOUT ? ApiErrorCode::Timeout : ApiErrorCode::Network};
        }
        Internet Checked(HINTERNET value) { if (!value) NetworkError(); return Internet{value}; }
        void Checked(BOOL value) { if (!value) NetworkError(); }
        struct AuthorizationHeader
        {
            std::vector<wchar_t> value;
            explicit AuthorizationHeader(std::wstring_view token)
            {
                constexpr std::wstring_view prefix{L"Authorization: Bearer "};
                value.resize(prefix.size() + token.size() + 2);
                std::copy(prefix.begin(), prefix.end(), value.begin());
                std::copy(token.begin(), token.end(), value.data() + prefix.size());
                value[value.size() - 2] = L'\r';
                value.back() = L'\n';
            }
            AuthorizationHeader(AuthorizationHeader const&) = delete;
            ~AuthorizationHeader() { Clear(); }
            void Clear() noexcept { if (!value.empty()) SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t)); }
        };
    }
    ApiResponse WindowsHttpTransport::Get(ApiRequest const& request, std::stop_token stop)
    {
        CheckCancelled(stop);
        auto const path = BuildRequestPath(request); // Only enumerated, read-only endpoints.
        auto const role = RequiredCredential(request.endpoint);
        std::unique_ptr<Infrastructure::SecureToken> secret;
        try
        {
            secret = Infrastructure::CredentialStore{}.Read(role == CredentialRole::Admin
                ? Infrastructure::TokenKind::Admin : Infrastructure::TokenKind::Model);
        }
        catch (...) { throw ApiError{ApiErrorCode::CredentialUnavailable}; }
        if (!secret) throw ApiError{ApiErrorCode::MissingCredential};

        auto session = Checked(WinHttpOpen(L"AetherNexus/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        Checked(WinHttpSetTimeouts(session.get(), 5000, 10000, 10000, 15000));
        auto connection = Checked(WinHttpConnect(session.get(), L"api.openai.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
        wchar_t const* accept[] = {L"application/json", nullptr};
        auto handle = Checked(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, accept, WINHTTP_FLAG_SECURE));
        DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
        Checked(WinHttpSetOption(handle.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)));
        // Never relax TLS certificate verification and never retry to another origin.
        AuthorizationHeader header{secret->View()};
        secret.reset();
        CheckCancelled(stop);
        Checked(WinHttpSendRequest(handle.get(), header.value.data(), static_cast<DWORD>(header.value.size()),
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0));
        header.Clear();
        Checked(WinHttpReceiveResponse(handle.get(), nullptr));
        DWORD status{};
        DWORD size = sizeof(status);
        Checked(WinHttpQueryHeaders(handle.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX));
        if (status < 200 || status >= 300) return {status, {}}; // Do not retain private error bodies.

        std::string bytes;
        std::array<char, 8192> chunk{};
        auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
        for (;;)
        {
            CheckCancelled(stop);
            if (std::chrono::steady_clock::now() > deadline) throw ApiError{ApiErrorCode::Timeout};
            DWORD received{};
            Checked(WinHttpReadData(handle.get(), chunk.data(), static_cast<DWORD>(chunk.size()), &received));
            if (received == 0) break;
            if (bytes.size() + received > 4 * 1024 * 1024) throw ApiError{ApiErrorCode::InvalidResponse};
            bytes.append(chunk.data(), received);
        }
        if (bytes.empty()) throw ApiError{ApiErrorCode::InvalidResponse};
        auto const length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        if (!length) throw ApiError{ApiErrorCode::InvalidResponse};
        std::wstring body(static_cast<size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), body.data(), length) != length)
            throw ApiError{ApiErrorCode::InvalidResponse};
        return {status, std::move(body)};
    }
}
