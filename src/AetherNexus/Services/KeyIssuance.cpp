// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: KeyIssuance.cpp
// Project: AetherNexus
#include "KeyIssuance.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>

namespace AetherNexus::Services
{
using namespace winrt;
using namespace Windows::Data::Json;
namespace
{
[[noreturn]] void Invalid()
{
    throw ApiError{ApiErrorCode::InvalidResponse};
}
void Require(bool value)
{
    if (!value)
        throw std::invalid_argument("Invalid key issuance input");
}
void Identifier(std::wstring const &id)
{
    Require(!id.empty() && id.size() <= 256 && std::all_of(id.begin(), id.end(), [](wchar_t c) {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
               c == L'_' || c == L'-';
    }));
}
size_t Space(std::string_view text, size_t i)
{
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n'))
        ++i;
    return i;
}
unsigned Hex(char c)
{
    if (c >= '0' && c <= '9')
        return static_cast<unsigned>(c - '0');
    if (c >= 'a' && c <= 'f')
        return static_cast<unsigned>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return static_cast<unsigned>(c - 'A' + 10);
    Invalid();
}
// Decode directly into wipeable storage. Never materialize the secret as hstring/std::string.
size_t String(std::string_view text, size_t start, Infrastructure::SecureBuffer *decoded = nullptr)
{
    if (start >= text.size() || text[start] != '"')
        Invalid();
    for (size_t i = start + 1; i < text.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '"')
            return i;
        if (c < 0x20)
            Invalid();
        if (c == '\\')
        {
            if (++i >= text.size())
                Invalid();
            switch (text[i])
            {
            case '"':
                c = '"';
                break;
            case '\\':
                c = '\\';
                break;
            case '/':
                c = '/';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u': {
                if (text.size() - i <= 4)
                    Invalid();
                unsigned code = 0;
                for (unsigned n = 0; n < 4; ++n)
                    code = code * 16 + Hex(text[++i]);
                        c = static_cast<unsigned char>(code <= 0x7f ? code : 0x7fu);
                break;
            }
            default:
                Invalid();
            }
        }
        if (decoded)
        {
            char byte = static_cast<char>(c);
            decoded->Append({&byte, 1});
            SecureZeroMemory(&byte, sizeof(byte));
        }
    }
    Invalid();
}
Infrastructure::SecureBuffer ExtractAndRedact(Infrastructure::SecureBuffer &body)
{
    auto text = body.View();
    if (text.empty() || text.size() > 65536)
        Invalid();
    Infrastructure::SecureBuffer secret{4096}, key{32};
    unsigned found = 0;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != '"')
            continue;
        auto end = String(text, i);
        auto colon = Space(text, end + 1);
        if (colon < text.size() && text[colon] == ':' && end - i - 1 <= 30)
        {
            key.Clear();
            (void)String(text, i, &key);
            if (key.View() == "value")
            {
                if (++found != 1)
                    Invalid();
                auto start = Space(text, colon + 1);
                auto finish = String(text, start, &secret);
                if (secret.View().empty() ||
                    !std::all_of(secret.View().begin(), secret.View().end(),
                                 [](unsigned char c) { return c >= 0x21 && c <= 0x7e; }))
                    Invalid();
                // Keep valid JSON, but remove ALL bytes of the encoded secret before WinRT sees it.
                SecureZeroMemory(body.Data() + start + 1, finish - start - 1);
                std::fill(body.Data() + start + 1, body.Data() + finish, ' ');
                end = finish;
            }
        }
        i = end;
    }
    if (found != 1)
        Invalid();
    return secret;
}
std::wstring Id(JsonObject const &object, wchar_t const *key, bool required)
{
    if (!object.HasKey(key))
    {
        if (required)
            Invalid();
        return {};
    }
    std::wstring value{object.GetNamedString(key)};
    try
    {
        Identifier(value);
    }
    catch (...)
    {
        Invalid();
    }
    return value;
}
} // namespace
std::wstring KeyIssuePath(KeyIssueRequest const &request)
{
    Require(!request.name.empty() && request.name.size() <= 100 &&
            request.name.find_first_not_of(L" \t\r\n") != std::wstring::npos);
    Require(!request.expiresInSeconds ||
            (*request.expiresInSeconds >= 1 && *request.expiresInSeconds <= 31536000));
    switch (request.kind)
    {
    case KeyIssueKind::Admin:
        Require(request.project.empty() && request.serviceAccount.empty());
        return L"/v1/organization/admin_api_keys";
    case KeyIssueKind::NewServiceAccount:
        Identifier(request.project);
        Require(request.serviceAccount.empty());
        return L"/v1/organization/projects/" + request.project + L"/service_accounts";
    case KeyIssueKind::ServiceAccount:
        Identifier(request.project);
        Identifier(request.serviceAccount);
        return L"/v1/organization/projects/" + request.project + L"/service_accounts/" +
               request.serviceAccount + L"/api_keys";
    }
    throw std::invalid_argument("Invalid key issuance kind");
}
std::wstring KeyIssueBody(KeyIssueRequest const &request)
{
    (void)KeyIssuePath(request);
    JsonObject body;
    body.SetNamedValue(L"name", JsonValue::CreateStringValue(request.name));
    if (request.expiresInSeconds)
        body.SetNamedValue(L"expires_in_seconds",
                           JsonValue::CreateNumberValue(static_cast<double>(*request.expiresInSeconds)));
    return body.Stringify().c_str();
}
std::wstring IssuedKey::Hint() const
{
    auto value = secret.View();
    if (value.empty())
        return L"—";
    std::wstring hint = L"•••• ";
    // Never expose an entire short/malformed token as a hint.
    if (value.size() > 8)
        for (char c : value.substr(value.size() - 4))
            hint += static_cast<wchar_t>(c);
    return hint;
}
IssuedKey ParseIssuedKey(SensitiveApiResponse response, KeyIssueRequest const &request)
{
    (void)KeyIssuePath(request);
    if (response.status == 400 || response.status == 404 || response.status == 409 || response.status == 422)
        throw std::invalid_argument("Server rejected key issuance");
    CheckStatus(response.status);
    IssuedKey result;
    result.secret = ExtractAndRedact(response.body);
    JsonObject object;
    // This conversion contains metadata and spaces only; the plaintext has already been overwritten.
    if (!JsonObject::TryParse(to_hstring(response.body.View()), object))
        Invalid();
    response.body.Clear();
    JsonObject key = object;
    if (request.kind == KeyIssueKind::NewServiceAccount)
    {
        if (object.GetNamedString(L"object") != L"organization.project.service_account")
            Invalid();
        result.serviceAccount = Id(object, L"id", true);
        key = object.GetNamedObject(L"api_key");
    }
    else if (request.kind == KeyIssueKind::ServiceAccount)
        result.serviceAccount = request.serviceAccount;
    auto expected = request.kind == KeyIssueKind::Admin ? L"organization.admin_api_key"
                                                        : L"organization.project.service_account.api_key";
    if ((request.kind != KeyIssueKind::Admin || key.HasKey(L"object")) &&
        key.GetNamedString(L"object") != expected)
        Invalid();
    auto redacted = key.GetNamedString(L"value");
    if (redacted.empty() ||
        !std::all_of(redacted.begin(), redacted.end(), [](wchar_t c) { return c == L' '; }))
        Invalid();
    result.id = Id(key, L"id", request.kind != KeyIssueKind::Admin);
    return result;
}
KeyIssuanceClient::KeyIssuanceClient(std::shared_ptr<IKeyIssuanceTransport> transport)
    : m_transport(std::move(transport))
{
    Require(m_transport != nullptr);
}
IssuedKey KeyIssuanceClient::Issue(KeyIssueRequest const &request, std::stop_token stop)
{
    (void)KeyIssuePath(request);
    CheckCancelled(stop);
    auto response = m_transport->Issue(request, stop);
    CheckCancelled(stop);
    // Issuance is deliberately sent once: retries could create duplicate credentials or accounts.
    return ParseIssuedKey(std::move(response), request);
}
} // namespace AetherNexus::Services
