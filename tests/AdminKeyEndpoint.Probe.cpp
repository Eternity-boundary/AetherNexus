// SPDX-License-Identifier: GPL-3.0-only
// Opt-in network diagnostic. No real credential implementation is linked.
#include <windows.h>
#include <winrt/base.h>
#include "Services/WindowsManagementTransport.h"
#include "Infrastructure/CredentialStore.h"
#include <iostream>
#include <string_view>

namespace AetherNexus::Infrastructure
{
    SecureToken::SecureToken(std::wstring_view value) : m_value(value.begin(), value.end()) {}
    SecureToken::~SecureToken() { Clear(); }
    void SecureToken::Clear() noexcept
    {
        if (!m_value.empty()) SecureZeroMemory(m_value.data(), m_value.size() * sizeof(wchar_t));
        m_value.clear();
    }
    CredentialStore::CredentialStore() : m_prefix(L"synthetic-probe-no-real-store") {}
    std::unique_ptr<SecureToken> CredentialStore::Read(TokenKind kind) const
    {
        if (kind != TokenKind::Admin) throw std::logic_error("Unexpected synthetic role");
        // Intentionally invalid; never reads env vars, files, Credential Manager or settings.
        return std::make_unique<SecureToken>(L"offline-diagnostic-invalid-token");
    }
}

namespace
{
    using namespace AetherNexus::Services;
    char const *Content(ResponseContentKind value)
    {
        switch (value)
        {
        case ResponseContentKind::Json: return "JSON";
        case ResponseContentKind::Html: return "HTML";
        case ResponseContentKind::Other: return "other";
        default: return "missing-or-unrecognized";
        }
    }
    char const *Body(ResponseBodyKind value)
    {
        switch (value)
        {
        case ResponseBodyKind::Empty: return "empty";
        case ResponseBodyKind::JsonError: return "JSON-error";
        case ResponseBodyKind::JsonOther: return "other-JSON";
        case ResponseBodyKind::HtmlLike: return "HTML-like";
        case ResponseBodyKind::Other: return "other";
        case ResponseBodyKind::InvalidJson: return "invalid-JSON";
        case ResponseBodyKind::Truncated: return "truncated";
        case ResponseBodyKind::ReadFailed: return "read-failed";
        default: return "not-read";
        }
    }
    ManagementRequest Request(ManagementMethod method)
    {
        ManagementRequest request;
        request.resource = ManagementResource::AdminKey;
        request.method = method;
        request.keyKind = ManagementKeyKind::Admin;
        request.item = L"key_offline_diagnostic_nonexistent";
        return request;
    }
    void Probe(ManagementMethod method)
    {
        auto request = Request(method);
        auto label = method == ManagementMethod::Get ? "GET" : "DELETE";
        try
        {
            // One call per verb; no Change/preflight workflow and no automatic retry.
            auto response = WindowsManagementTransport{}.Send(request, {});
            auto const &facts = response.diagnostics;
            std::cout << label << " HTTP=" << response.status << " Content=" << Content(facts.content)
                      << " Body=" << Body(facts.body.kind)
                      << " RequestIdState=" << static_cast<int>(facts.requestIdState)
                      << " OpenAiMetadata=" << (facts.openAiMetadata ? "present" : "absent") << '\n';
            // These IDs already pass the production allowlist; no arbitrary headers/body are printed.
            if (!response.requestId.empty()) std::wcout << L"ServerRequestId=" << response.requestId << L'\n';
            if (!facts.clientRequestId.empty()) std::wcout << L"ClientRequestId=" << facts.clientRequestId << L'\n';
        }
        catch (ApiError const &error)
        {
            std::cout << label << " TransportError=" << static_cast<int>(error.code) << '\n';
        }
    }
}

int main(int count, char **values)
{
    // Validate both exact request shapes even in the no-network preview.
    (void)AetherNexus::Services::BuildManagementWireRequest(Request(AetherNexus::Services::ManagementMethod::Get));
    (void)AetherNexus::Services::BuildManagementWireRequest(Request(AetherNexus::Services::ManagementMethod::Delete));
    if (count != 2 || std::string_view{values[1]} != "--run-synthetic-network-probe")
    {
        std::cout << "PREVIEW ONLY: no network calls.\n"
                     "Opt-in probe: exactly one GET and one DELETE to https://api.openai.com:443\n"
                     "/v1/organization/admin_api_keys/key_offline_diagnostic_nonexistent\n"
                     "Authorization is always a hardcoded invalid synthetic value; no real credential store is linked.\n"
                     "Only status, safe response facts and correlation IDs are printed.\n";
        return count == 1 ? 0 : 2;
    }
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        Probe(AetherNexus::Services::ManagementMethod::Get);
        Probe(AetherNexus::Services::ManagementMethod::Delete);
        return 0;
    }
    catch (...) { std::cerr << "Synthetic endpoint probe failed; no raw exception or response is displayed.\n"; return 1; }
}
