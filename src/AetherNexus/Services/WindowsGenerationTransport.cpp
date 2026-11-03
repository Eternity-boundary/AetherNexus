// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WindowsGenerationTransport.cpp
// Project: AetherNexus
#include "WindowsGenerationTransport.h"
#include "Infrastructure/CredentialStore.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <windows.h>
#include <winhttp.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#pragma comment(lib, "Winhttp.lib")

namespace AetherNexus::Services
{
    using namespace winrt;
    using namespace Windows::Data::Json;
    namespace
    {
        struct Handle
        {
            std::atomic<HINTERNET> value{};
            explicit Handle(HINTERNET handle) : value(handle)
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
        auto Open(HINTERNET handle, std::stop_token stop)
        {
            if (!handle)
                Fail(stop);
            return std::make_shared<Handle>(handle);
        }
        void Check(BOOL value, std::stop_token stop)
        {
            if (!value)
                Fail(stop);
        }
        struct SecretHeader
        {
            std::vector<wchar_t> text;
            explicit SecretHeader(std::wstring_view token)
            {
                constexpr std::wstring_view prefix{L"Authorization: Bearer "};
                text.resize(prefix.size() + token.size() + 2);
                std::copy(prefix.begin(), prefix.end(), text.begin());
                std::copy(token.begin(), token.end(), text.data() + prefix.size());
                text[text.size() - 2] = L'\r';
                text.back() = L'\n';
            }
            ~SecretHeader()
            {
                Clear();
            }
            void Clear()
            {
                if (!text.empty())
                    SecureZeroMemory(text.data(), text.size() * sizeof(wchar_t));
            }
        };
        void SendJson(std::shared_ptr<Handle> const &socket, JsonObject const &object, std::stop_token stop)
        {
            CheckCancelled(stop);
            auto bytes = to_string(object.Stringify());
            auto error = WinHttpWebSocketSend(socket->Get(), WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, bytes.data(),
                                              static_cast<DWORD>(bytes.size()));
            if (error)
            {
                SetLastError(error);
                Fail(stop);
            }
        }
        void Type(JsonObject const &object, wchar_t const *type)
        {
            object.SetNamedValue(L"type", JsonValue::CreateStringValue(type));
        }
    } // namespace
    GenerationResponse WindowsGenerationTransport::Send(GenerationRequest const &request, EventCallback const &callback,
                                                        std::stop_token stop)
    {
        CheckCancelled(stop);
        if (request.body.size() > 128 * 1024 * 1024 || request.contentType.find_first_of(L"\r\n") != std::wstring::npos)
            throw std::invalid_argument("Generation request too large");
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
        auto session = Open(WinHttpOpen(L"AetherNexus/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS, 0),
                            stop);
        Check(WinHttpSetTimeouts(session->Get(), 5000, 15000, 30000, 90000), stop);
        auto connection = Open(WinHttpConnect(session->Get(), L"api.openai.com", INTERNET_DEFAULT_HTTPS_PORT, 0), stop);
        auto path = GenerationPath(request.operation);
        bool const realtime = request.operation == GenerationOperation::Realtime;
        if (realtime)
        {
            if (request.model.empty() || request.model.size() > 256 ||
                std::any_of(request.model.begin(), request.model.end(), [](wchar_t c) {
                    return !((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
                             c == L'-' || c == L'_' || c == L'.');
                }))
                throw std::invalid_argument("Invalid Realtime model");
            path += L"?model=" + request.model;
        }
        auto handle = Open(WinHttpOpenRequest(connection->Get(), realtime ? L"GET" : L"POST", path.c_str(), nullptr,
                                              WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE),
                           stop);
        std::stop_callback cancelRequest{stop, [handle] { handle->Close(); }};
        DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
        Check(WinHttpSetOption(handle->Get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)), stop);
        if (realtime)
            Check(WinHttpSetOption(handle->Get(), WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0), stop);
        else
        {
            auto content = L"Content-Type: " + request.contentType + L"\r\n";
            Check(WinHttpAddRequestHeaders(handle->Get(), content.c_str(), static_cast<DWORD>(content.size()),
                                           WINHTTP_ADDREQ_FLAG_ADD),
                  stop);
        }
        SecretHeader header{token->View()};
        token.reset();
        Check(WinHttpSendRequest(handle->Get(), header.text.data(), static_cast<DWORD>(header.text.size()),
                                 realtime ? WINHTTP_NO_REQUEST_DATA : const_cast<char *>(request.body.data()),
                                 realtime ? 0 : static_cast<DWORD>(request.body.size()),
                                 realtime ? 0 : static_cast<DWORD>(request.body.size()), 0),
              stop);
        header.Clear();
        Check(WinHttpReceiveResponse(handle->Get(), nullptr), stop);
        DWORD status{}, length = sizeof(status);
        Check(WinHttpQueryHeaders(handle->Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                  WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX),
              stop);
        if ((!realtime && (status < 200 || status >= 300)) || (realtime && status != 101))
            return {status, {}};
        auto const deadline = std::chrono::steady_clock::now() + std::chrono::minutes{10};
        if (realtime)
        {
            auto socket = Open(WinHttpWebSocketCompleteUpgrade(handle->Get(), 0), stop);
            std::stop_callback cancelSocket{stop, [socket] { socket->Close(); }};
            auto config = JsonObject::Parse(to_hstring(request.body));
            JsonObject options = config.GetNamedObject(L"session", JsonObject{});
            Type(options, L"realtime");
            options.SetNamedValue(L"output_modalities", JsonArray::Parse(L"[\"audio\"]"));
            auto instructions = config.GetNamedString(L"instructions", L"");
            if (!instructions.empty())
                options.SetNamedValue(L"instructions", JsonValue::CreateStringValue(instructions));
            auto audio = JsonObject::Parse(
                LR"({"input":{"format":{"type":"audio/pcm","rate":24000},"turn_detection":null},"output":{"format":{"type":"audio/pcm","rate":24000}}})");
            audio.GetNamedObject(L"output").SetNamedValue(
                L"voice", JsonValue::CreateStringValue(config.GetNamedString(L"voice", L"marin")));
            options.SetNamedValue(L"audio", audio);
            JsonObject update;
            Type(update, L"session.update");
            update.SetNamedValue(L"session", options);
            SendJson(socket, update, stop);
            bool started = false;
            std::string message;
            std::array<char, 8192> buffer{};
            for (;;)
            {
                CheckCancelled(stop);
                if (std::chrono::steady_clock::now() > deadline)
                    throw ApiError{ApiErrorCode::Timeout};
                DWORD received{};
                WINHTTP_WEB_SOCKET_BUFFER_TYPE kind{};
                auto error = WinHttpWebSocketReceive(socket->Get(), buffer.data(), static_cast<DWORD>(buffer.size()),
                                                     &received, &kind);
                if (error)
                {
                    SetLastError(error);
                    Fail(stop);
                }
                if (kind == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
                    throw ApiError{ApiErrorCode::InvalidResponse};
                if (kind != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE &&
                    kind != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
                    throw ApiError{ApiErrorCode::InvalidResponse};
                message.append(buffer.data(), received);
                if (message.size() > 32 * 1024 * 1024)
                    throw ApiError{ApiErrorCode::InvalidResponse};
                if (kind != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
                    continue;
                auto event = JsonObject::Parse(to_hstring(message));
                auto type = event.GetNamedString(L"type");
                if (type == L"session.updated" && !started)
                {
                    started = true;
                    if (config.HasKey(L"history"))
                    {
                        for (auto const &item : config.GetNamedArray(L"history"))
                        {
                            JsonObject prior;
                            Type(prior, L"conversation.item.create");
                            prior.SetNamedValue(L"item", item.GetObject());
                            SendJson(socket, prior, stop);
                        }
                    }
                    JsonArray text;
                    bool hasAudio = false;
                    for (auto const &part : config.GetNamedArray(L"input"))
                    {
                        auto input = part.GetObject();
                        if (input.GetNamedString(L"type") == L"input_audio")
                        {
                            hasAudio = true;
                            auto pcm = DecodeBase64(to_string(input.GetNamedString(L"audio")));
                            for (size_t offset = 0; offset < pcm.size(); offset += 24000)
                            {
                                auto encoded = Base64(std::span<uint8_t const>{pcm}.subspan(
                                    offset, std::min<size_t>(24000, pcm.size() - offset)));
                                JsonObject append;
                                Type(append, L"input_audio_buffer.append");
                                append.SetNamedValue(L"audio", JsonValue::CreateStringValue(to_hstring(encoded)));
                                SendJson(socket, append, stop);
                            }
                        }
                        else
                            text.Append(input);
                    }
                    if (text.Size())
                    {
                        JsonObject item;
                        Type(item, L"message");
                        item.SetNamedValue(L"role", JsonValue::CreateStringValue(L"user"));
                        item.SetNamedValue(L"content", text);
                        JsonObject create;
                        Type(create, L"conversation.item.create");
                        create.SetNamedValue(L"item", item);
                        SendJson(socket, create, stop);
                    }
                    if (hasAudio)
                    {
                        JsonObject commit;
                        Type(commit, L"input_audio_buffer.commit");
                        SendJson(socket, commit, stop);
                    }
                    JsonObject generate;
                    Type(generate, L"response.create");
                    SendJson(socket, generate, stop);
                }
                if (callback)
                    callback(message);
                message.clear();
                if (type == L"response.done")
                    return {200, {}};
            }
        }
        wchar_t type[256]{};
        length = sizeof(type);
        bool const stream = WinHttpQueryHeaders(handle->Get(), WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
                                                type, &length, WINHTTP_NO_HEADER_INDEX) &&
                            std::wstring_view{type}.starts_with(L"text/event-stream");
        std::array<char, 16384> chunk{};
        GenerationResponse response{status, {}};
        for (;;)
        {
            CheckCancelled(stop);
            if (std::chrono::steady_clock::now() > deadline)
                throw ApiError{ApiErrorCode::Timeout};
            DWORD received{};
            Check(WinHttpReadData(handle->Get(), chunk.data(), static_cast<DWORD>(chunk.size()), &received), stop);
            if (!received)
                break;
            if (stream && callback)
                callback({chunk.data(), received});
            else
            {
                if (response.body.size() + received > 160 * 1024 * 1024)
                    throw ApiError{ApiErrorCode::InvalidResponse};
                response.body.append(chunk.data(), received);
            }
        }
        return response;
    }
} // namespace AetherNexus::Services
