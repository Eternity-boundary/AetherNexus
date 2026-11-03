// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WindowsManagementTransport.cpp
// Project: AetherNexus
#include "WindowsManagementTransport.h"
#include "Infrastructure/CredentialStore.h"
#include <windows.h>
#include <winhttp.h>
#include <winrt/base.h>
#include <algorithm>
#include <array>
#include <atomic>
#pragma comment(lib, "Winhttp.lib")

namespace AetherNexus::Services
{
namespace
{
struct Handle
{
    std::atomic<HINTERNET> value{};
    explicit Handle(HINTERNET h) : value(h)
    {
    }
    ~Handle()
    {
        Close();
    }
    void Close()
    {
        if (auto h = value.exchange(nullptr))
            WinHttpCloseHandle(h);
    }
    HINTERNET Get() const
    {
        return value.load();
    }
};
[[noreturn]] void Fail(std::stop_token stop)
{
    CheckCancelled(stop);
    throw ApiError{GetLastError() == ERROR_WINHTTP_TIMEOUT ? ApiErrorCode::Timeout : ApiErrorCode::Network};
}
void Check(BOOL ok, std::stop_token stop)
{
    if (!ok)
        Fail(stop);
}
auto Open(HINTERNET h, std::stop_token stop)
{
    if (!h)
        Fail(stop);
    return std::make_shared<Handle>(h);
}
struct Header
{
    std::vector<wchar_t> bytes;
    explicit Header(std::wstring_view token)
    {
        constexpr std::wstring_view prefix = L"Authorization: Bearer ";
        bytes.resize(prefix.size() + token.size() + 2);
        std::copy(prefix.begin(), prefix.end(), bytes.begin());
        std::copy(token.begin(), token.end(), bytes.data() + prefix.size());
        bytes[bytes.size() - 2] = L'\r';
        bytes.back() = L'\n';
    }
    ~Header()
    {
        Clear();
    }
    Header(Header const &) = delete;
    void Clear()
    {
        if (!bytes.empty())
            SecureZeroMemory(bytes.data(), bytes.size() * sizeof(wchar_t));
    }
};
SensitiveApiResponse Exchange(std::wstring const &path, wchar_t const *verb, std::wstring const &requestBody,
                              bool post, size_t limit, std::stop_token stop)
{
    CheckCancelled(stop);
    std::unique_ptr<Infrastructure::SecureToken> token;
    try
    {
        token = Infrastructure::CredentialStore{}.Read(Infrastructure::TokenKind::Admin);
    }
    catch (...)
    {
        throw ApiError{ApiErrorCode::CredentialUnavailable};
    }
    if (!token)
        throw ApiError{ApiErrorCode::MissingCredential};
    auto session = Open(WinHttpOpen(L"AetherNexus/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0),
                        stop);
    Check(WinHttpSetTimeouts(session->Get(), 5000, 10000, 15000, 30000), stop);
    auto connection =
        Open(WinHttpConnect(session->Get(), L"api.openai.com", INTERNET_DEFAULT_HTTPS_PORT, 0), stop);
    auto handle = Open(WinHttpOpenRequest(connection->Get(), verb, path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE),
                       stop);
    std::stop_callback cancel{stop, [handle] { handle->Close(); }};
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    Check(WinHttpSetOption(handle->Get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)), stop);
    auto body = winrt::to_string(requestBody); // Request bodies contain names/policies, never issued secrets.
    if (post)
        Check(WinHttpAddRequestHeaders(handle->Get(), L"Content-Type: application/json\r\n",
                                       static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD),
              stop);
    Header header{token->View()};
    token.reset();
    Check(WinHttpSendRequest(handle->Get(), header.bytes.data(), static_cast<DWORD>(header.bytes.size()),
                             body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data(),
                             static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0),
          stop);
    header.Clear();
    Check(WinHttpReceiveResponse(handle->Get(), nullptr), stop);
    DWORD status{}, size = sizeof(status);
    Check(WinHttpQueryHeaders(handle->Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX),
          stop);
    if (status < 200 || status >= 300)
            return {status, Infrastructure::SecureBuffer{}};
    SensitiveApiResponse response{status, Infrastructure::SecureBuffer{limit}};
    Infrastructure::SecureBuffer chunk{8192};
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{120};
    for (;;)
    {
        CheckCancelled(stop);
        if (std::chrono::steady_clock::now() > deadline)
            throw ApiError{ApiErrorCode::Timeout};
        DWORD received{};
        Check(WinHttpReadData(handle->Get(), chunk.Data(), static_cast<DWORD>(chunk.Capacity()), &received),
              stop);
        if (!received)
            break;
        if (response.body.View().size() + received > limit)
            throw ApiError{ApiErrorCode::InvalidResponse};
        response.body.Append({chunk.Data(), received});
        chunk.Clear();
    }
    CheckCancelled(stop);
    if (response.body.View().empty())
        throw ApiError{ApiErrorCode::InvalidResponse};
    return response;
}
} // namespace
SensitiveApiResponse WindowsManagementTransport::Issue(KeyIssueRequest const &request, std::stop_token stop)
{
    return Exchange(KeyIssuePath(request), L"POST", KeyIssueBody(request), true, 65536, stop);
}
ApiResponse WindowsManagementTransport::Send(ManagementRequest const &request, std::stop_token stop)
{
    // Issuance has a separate typed interface; plaintext can never reach the ordinary string response path.
    auto response = Exchange(ManagementPath(request), ManagementVerb(request.method), request.body,
                             request.method == ManagementMethod::Post, 8 * 1024 * 1024, stop);
    if (response.status < 200 || response.status >= 300)
        return {response.status, {}};
    auto bytes = response.body.View();
    auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                                      static_cast<int>(bytes.size()), nullptr, 0);
    if (!length)
        throw ApiError{ApiErrorCode::InvalidResponse};
    std::wstring text(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()),
                            text.data(), length) != length)
        throw ApiError{ApiErrorCode::InvalidResponse};
    return {response.status, std::move(text)};
}
} // namespace AetherNexus::Services
