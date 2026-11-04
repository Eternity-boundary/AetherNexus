// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WindowsBuildTransport.cpp
// Project: AetherNexus
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "WindowsBuildTransport.h"
#include "Infrastructure/CredentialStore.h"
#include "Infrastructure/SecureBuffer.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <objbase.h>
#include <windows.h>
#include <winhttp.h>
#include <winrt/base.h>
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Ole32.lib")

namespace AetherNexus::Services
{
namespace
{
struct Internet
{
    std::atomic<HINTERNET> value{};
    explicit Internet(HINTERNET handle) : value(handle)
    {
    }
    ~Internet()
    {
        Close();
    }
    void Close()
    {
        if (auto handle = value.exchange(nullptr))
            WinHttpCloseHandle(handle);
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
void Check(BOOL value, std::stop_token stop)
{
    if (!value)
        Fail(stop);
}
auto Open(HINTERNET handle, std::stop_token stop)
{
    if (!handle)
        Fail(stop);
    return std::make_shared<Internet>(handle);
}
struct Authorization
{
    std::vector<wchar_t> text;
    explicit Authorization(std::wstring_view token)
    {
        constexpr std::wstring_view prefix = L"Authorization: Bearer ";
        text.resize(prefix.size() + token.size() + 2);
        std::copy(prefix.begin(), prefix.end(), text.begin());
        std::copy(token.begin(), token.end(), text.data() + prefix.size());
        text[text.size() - 2] = L'\r';
        text.back() = L'\n';
    }
    Authorization(Authorization const &) = delete;
    ~Authorization()
    {
        Clear();
    }
    void Clear()
    {
        if (!text.empty())
            SecureZeroMemory(text.data(), text.size() * sizeof(wchar_t));
    }
};
std::wstring Nonce()
{
    GUID id{};
    winrt::check_hresult(CoCreateGuid(&id));
    std::array<wchar_t, 39> text{};
    if (StringFromGUID2(id, text.data(), static_cast<int>(text.size())) != 39)
        throw ApiError{ApiErrorCode::Network};
    return {text.data() + 1, 36};
}
struct DownloadFile
{
    std::filesystem::path temporary;
    HANDLE handle{INVALID_HANDLE_VALUE};
    explicit DownloadFile(std::filesystem::path const &destination)
    {
        if (!destination.is_absolute())
            throw std::invalid_argument("Invalid download destination");
        temporary = destination;
        temporary += L".aethernexus-" + Nonce();
        handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw ApiError{ApiErrorCode::InvalidResponse};
    }
    ~DownloadFile()
    {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
        if (!temporary.empty())
            DeleteFileW(temporary.c_str());
    }
    void Write(char const *data, DWORD count)
    {
        DWORD done{};
        if (!WriteFile(handle, data, count, &done, nullptr) || done != count)
            throw ApiError{ApiErrorCode::InvalidResponse};
    }
    void Commit(std::filesystem::path const &destination)
    {
        if (!FlushFileBuffers(handle))
            throw ApiError{ApiErrorCode::InvalidResponse};
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw ApiError{ApiErrorCode::InvalidResponse};
        temporary.clear();
    }
};
} // namespace
BuildResponse WindowsBuildTransport::Send(BuildRequest const &request, std::stop_token stop)
{
    auto wire = BuildWire(request);
    CheckCancelled(stop);
    std::ifstream upload;
    std::string prefix, suffix;
    uint64_t total = wire.body.size();
    if (request.upload)
    {
        auto const &file = *request.upload;
        bool jsonl = file.purpose == L"batch" || file.purpose == L"fine-tune";
        auto extension = file.path.extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](wchar_t c) { return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + (L'a' - L'A')) : c; });
        if (!file.path.is_absolute() || file.bytes == 0 ||
            file.bytes > (file.purpose == L"batch" ? 200ull : 512ull) * 1024 * 1024 ||
            (jsonl && extension != L".jsonl") ||
            !(file.purpose == L"batch" || file.purpose == L"fine-tune" || file.purpose == L"user_data" ||
              file.purpose == L"assistants" || file.purpose == L"vision" || file.purpose == L"evals"))
            throw std::invalid_argument("Invalid upload");
        if (std::filesystem::file_size(file.path) != file.bytes)
            throw std::invalid_argument("Selected file changed");
        upload.open(file.path, std::ios::binary);
        if (!upload)
            throw std::invalid_argument("Selected file unavailable");
        std::string name;
        for (auto c : file.name.substr(0, 200))
            name += c >= L'!' && c <= L'~' && c != L'"' && c != L'\\' && c != L'/' ? static_cast<char>(c) : '_';
        if (name.empty())
            name = "upload" + winrt::to_string(extension);
        auto boundary = "AetherNexus-" + winrt::to_string(Nonce());
        prefix = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"purpose\"\r\n\r\n" +
                 winrt::to_string(file.purpose) + "\r\n--" + boundary +
                 "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + name +
                 "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
        suffix = "\r\n--" + boundary + "--\r\n";
        total = prefix.size() + file.bytes + suffix.size();
        wire.headers +=
            L"Content-Type: multipart/form-data; boundary=" + std::wstring{winrt::to_hstring(boundary)} + L"\r\n";
    }
    std::unique_ptr<Infrastructure::SecureToken> token;
    try
    {
        token = Infrastructure::CredentialStore{}.Read(Infrastructure::TokenKind::Model);
    }
    catch (...)
    {
        throw ApiError{ApiErrorCode::CredentialUnavailable};
    }
    if (!token)
        throw ApiError{ApiErrorCode::MissingCredential};
    auto session =
        Open(WinHttpOpen(L"AetherNexus/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0), stop);
    Check(WinHttpSetTimeouts(session->Get(), 5000, 10000, 15000, 30000), stop);
    auto connection = Open(WinHttpConnect(session->Get(), L"api.openai.com", INTERNET_DEFAULT_HTTPS_PORT, 0), stop);
    auto handle = Open(WinHttpOpenRequest(connection->Get(), wire.method.c_str(), wire.path.c_str(), nullptr, nullptr,
                                          nullptr, WINHTTP_FLAG_SECURE),
                       stop);
    std::stop_callback cancel{stop, [handle] { handle->Close(); }};
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    Check(WinHttpSetOption(handle->Get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)), stop);
    Check(
        WinHttpAddRequestHeaders(handle->Get(), wire.headers.c_str(), static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD),
        stop);
    Authorization authorization{token->View()};
    token.reset();
    auto &first = request.upload ? prefix : wire.body;
    Check(WinHttpSendRequest(handle->Get(), authorization.text.data(), static_cast<DWORD>(authorization.text.size()),
                             first.empty() ? nullptr : first.data(), static_cast<DWORD>(first.size()),
                             static_cast<DWORD>(total), 0),
          stop);
    authorization.Clear();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes{10};
    auto checkTime = [&] {
        CheckCancelled(stop);
        if (std::chrono::steady_clock::now() > deadline)
            throw ApiError{ApiErrorCode::Timeout};
    };
    auto write = [&](char const *data, size_t count) {
        while (count)
        {
            checkTime();
            DWORD done{};
            Check(WinHttpWriteData(handle->Get(), data, static_cast<DWORD>(count), &done), stop);
            if (!done)
                throw ApiError{ApiErrorCode::Network};
            data += done;
            count -= done;
        }
    };
    Infrastructure::SecureBuffer chunk{65536};
    if (request.upload)
    {
        auto remaining = request.upload->bytes;
        while (remaining)
        {
            checkTime();
            auto wanted = static_cast<size_t>(std::min(remaining, static_cast<uint64_t>(chunk.Capacity())));
            upload.read(chunk.Data(), static_cast<std::streamsize>(wanted));
            if (upload.gcount() != static_cast<std::streamsize>(wanted))
                throw ApiError{ApiErrorCode::InvalidResponse};
            write(chunk.Data(), wanted);
            remaining -= wanted;
            chunk.Clear();
        }
        write(suffix.data(), suffix.size());
    }
    Check(WinHttpReceiveResponse(handle->Get(), nullptr), stop);
    DWORD status{}, size = sizeof(status);
    Check(WinHttpQueryHeaders(handle->Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status,
                              &size, nullptr),
          stop);
    BuildResponse response{status, {}};
    if (status < 200 || status >= 300)
        return response; // Never retain a server error that may echo credentials.
    std::unique_ptr<DownloadFile> output;
    if (!request.download.empty())
        output = std::make_unique<DownloadFile>(request.download);
    for (;;)
    {
        checkTime();
        DWORD received{};
        Check(WinHttpReadData(handle->Get(), chunk.Data(), static_cast<DWORD>(chunk.Capacity()), &received), stop);
        if (!received)
            break;
        response.downloadedBytes += received;
        if (response.downloadedBytes > (output ? 1024ull * 1024 * 1024 : 16ull * 1024 * 1024))
            throw ApiError{ApiErrorCode::InvalidResponse};
        if (output)
            output->Write(chunk.Data(), received);
        else
            response.body.append(chunk.Data(), received);
        chunk.Clear();
    }
    checkTime();
    if (output)
        output->Commit(request.download);
    return response;
}
} // namespace AetherNexus::Services
