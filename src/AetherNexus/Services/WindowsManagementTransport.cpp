// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
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
#include <objbase.h>
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Ole32.lib")

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
std::wstring ReadRequestId(HINTERNET handle, ResponseRequestIdState &state)
{
    // Optional diagnostic header. Use a fixed bound and discard unexpected values rather than
    // retaining arbitrary server text. Header lookup failure must not replace the HTTP status.
    std::array<wchar_t, 129> value{};
    DWORD bytes = static_cast<DWORD>(sizeof(value));
    if (!WinHttpQueryHeaders(handle, WINHTTP_QUERY_CUSTOM, L"x-request-id", value.data(), &bytes,
                             WINHTTP_NO_HEADER_INDEX))
    {
        auto error = GetLastError();
        state = error == ERROR_WINHTTP_HEADER_NOT_FOUND ? ResponseRequestIdState::Missing
            : error == ERROR_INSUFFICIENT_BUFFER ? ResponseRequestIdState::TooLong
                                                 : ResponseRequestIdState::ReadFailed;
        SecureZeroMemory(value.data(), sizeof(value));
        return {};
    }
    if (value.back() != L'\0')
    {
        state = ResponseRequestIdState::TooLong;
        SecureZeroMemory(value.data(), sizeof(value));
        return {};
    }
    auto end = std::find(value.begin(), value.end(), L'\0');
    auto result = ManagementRequestId({value.data(), static_cast<size_t>(end - value.begin())});
    SecureZeroMemory(value.data(), sizeof(value));
    state = result.empty() ? ResponseRequestIdState::Rejected : ResponseRequestIdState::Valid;
    return result;
}
bool EqualsAscii(std::wstring_view value, std::wstring_view expected)
{
    return value.size() == expected.size() && std::equal(value.begin(), value.end(), expected.begin(), [](wchar_t a, wchar_t b) {
        if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a + (L'a' - L'A'));
        return a == b;
    });
}
ResponseContentKind ReadContentKind(HINTERNET handle)
{
    std::array<wchar_t, 129> value{};
    DWORD bytes = static_cast<DWORD>(sizeof(value));
    if (!WinHttpQueryHeaders(handle, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &bytes,
                             WINHTTP_NO_HEADER_INDEX) || value.back() != L'\0')
        return ResponseContentKind::Unknown;
    auto end = std::find(value.begin(), value.end(), L'\0');
    std::wstring_view mime{value.data(), static_cast<size_t>(end - value.begin())};
    mime = mime.substr(0, mime.find(L';'));
    while (!mime.empty() && (mime.front() == L' ' || mime.front() == L'\t')) mime.remove_prefix(1);
    while (!mime.empty() && (mime.back() == L' ' || mime.back() == L'\t')) mime.remove_suffix(1);
    auto result = EqualsAscii(mime, L"application/json") || (mime.size() > 5 && EqualsAscii(mime.substr(mime.size() - 5), L"+json"))
        ? ResponseContentKind::Json : EqualsAscii(mime, L"text/html") || EqualsAscii(mime, L"application/xhtml+xml")
        ? ResponseContentKind::Html : ResponseContentKind::Other;
    SecureZeroMemory(value.data(), sizeof(value));
    return result;
}
bool HasHeader(HINTERNET handle, wchar_t const *name)
{
    DWORD bytes{};
    return WinHttpQueryHeaders(handle, WINHTTP_QUERY_CUSTOM, name, WINHTTP_NO_OUTPUT_BUFFER, &bytes,
                                WINHTTP_NO_HEADER_INDEX) || GetLastError() == ERROR_INSUFFICIENT_BUFFER;
}
std::wstring NewClientRequestId()
{
    GUID id{};
    std::array<wchar_t, 39> value{};
    if (FAILED(CoCreateGuid(&id)) || StringFromGUID2(id, value.data(), static_cast<int>(value.size())) != 39)
        return {};
    return {value.data() + 1, 36}; // Unbraced UUID; generated locally, never derived from a key or account.
}
ResponseBodyFacts ReadErrorFacts(HINTERNET handle, std::stop_token stop) noexcept
{
    ResponseBodyFacts failure;
    failure.kind = ResponseBodyKind::ReadFailed;
    try
    {
        constexpr size_t maximumErrorBytes = 32768;
        Infrastructure::SecureBuffer body{maximumErrorBytes}, chunk{8192};
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        for (;;)
        {
            if (stop.stop_requested()) return failure;
            if (std::chrono::steady_clock::now() > deadline)
                return InspectErrorResponse(body.View(), true);
            DWORD received{};
            if (!WinHttpReadData(handle, chunk.Data(), static_cast<DWORD>(chunk.Capacity()), &received))
                return failure; // Optional diagnostic failure never replaces the received HTTP status.
            if (!received) return InspectErrorResponse(body.View());
            if (received > maximumErrorBytes - body.View().size())
                return InspectErrorResponse(body.View(), true);
            body.Append({chunk.Data(), received});
            chunk.Clear();
            if (body.View().size() == maximumErrorBytes)
                return InspectErrorResponse(body.View(), true);
        }
    }
    catch (...) { return failure; }
}
SensitiveApiResponse Exchange(ManagementWireRequest const &request, size_t limit, std::stop_token stop,
                              std::wstring *requestId = nullptr, HttpResponseDiagnostics *diagnostics = nullptr)
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
    auto handle = Open(WinHttpOpenRequest(connection->Get(), request.method.c_str(), request.path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE),
                       stop);
    std::stop_callback cancel{stop, [handle] { handle->Close(); }};
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    Check(WinHttpSetOption(handle->Get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)), stop);
    auto body = winrt::to_string(request.body); // Request bodies contain names/policies, never issued secrets.
    // The official administration examples use JSON headers for DELETE as well as POST/GET.
    Check(WinHttpAddRequestHeaders(handle->Get(), request.headers.c_str(),
                                   static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD), stop);
    if (diagnostics)
    {
        diagnostics->clientRequestId = NewClientRequestId();
        if (!diagnostics->clientRequestId.empty())
        {
            auto header = L"X-Client-Request-Id: " + diagnostics->clientRequestId + L"\r\n";
            Check(WinHttpAddRequestHeaders(handle->Get(), header.c_str(), static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD), stop);
        }
    }
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
    if (diagnostics)
    {
        auto responseId = ReadRequestId(handle->Get(), diagnostics->requestIdState);
        if (requestId) *requestId = std::move(responseId);
        diagnostics->content = ReadContentKind(handle->Get());
        diagnostics->openAiMetadata = HasHeader(handle->Get(), L"openai-processing-ms") ||
                                      HasHeader(handle->Get(), L"openai-version");
    }
    if (status < 200 || status >= 300)
    {
        if (diagnostics) diagnostics->body = ReadErrorFacts(handle->Get(), stop);
        return {status, Infrastructure::SecureBuffer{}};
    }
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
    return Exchange({KeyIssuePath(request), L"POST", KeyIssueBody(request)}, 65536, stop);
}
ApiResponse WindowsManagementTransport::Send(ManagementRequest const &request, std::stop_token stop)
{
    // Issuance has a separate typed interface; plaintext can never reach the ordinary string response path.
    std::wstring requestId;
    HttpResponseDiagnostics diagnostics;
    auto response = Exchange(BuildManagementWireRequest(request), 8 * 1024 * 1024, stop, &requestId, &diagnostics);
    if (response.status < 200 || response.status >= 300)
        return {response.status, {}, std::move(requestId), std::move(diagnostics)};
    auto bytes = response.body.View();
    auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                                      static_cast<int>(bytes.size()), nullptr, 0);
    if (!length)
        throw ApiError{ApiErrorCode::InvalidResponse};
    std::wstring text(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()),
                            text.data(), length) != length)
        throw ApiError{ApiErrorCode::InvalidResponse};
    return {response.status, std::move(text), std::move(requestId), std::move(diagnostics)};
}
} // namespace AetherNexus::Services
