// SPDX-License-Identifier: GPL-3.0-only
// Manual, explicitly authorized account diagnostic. Excluded from all ordinary test batches.
// Uses the existing Settings Admin credential in memory; never prints bodies or credential data.
#include <windows.h>
#include <winhttp.h>
#include <winrt/base.h>
#include <winrt/Windows.Data.Json.h>
#include "Services/WindowsManagementTransport.h"
#include "Infrastructure/CredentialStore.h"
#include <iostream>
#include <string_view>

namespace AuthorizedProbe
{
    bool preferHttp2{};
    DWORD lastProtocol{};
    bool protocolKnown{};
    HINTERNET Open(LPCWSTR agent, DWORD access, LPCWSTR proxy, LPCWSTR bypass, DWORD flags)
    {
        auto session = WinHttpOpen(agent, access, proxy, bypass, flags);
        if (session && preferHttp2)
        {
            DWORD protocols = WINHTTP_PROTOCOL_FLAG_HTTP2;
            if (!WinHttpSetOption(session, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &protocols, sizeof(protocols)))
            {
                auto error = GetLastError();
                WinHttpCloseHandle(session);
                SetLastError(error);
                return nullptr;
            }
        }
        return session;
    }
    BOOL ReceiveResponse(HINTERNET handle, LPVOID reserved)
    {
        auto result = WinHttpReceiveResponse(handle, reserved);
        if (result)
        {
            DWORD size = sizeof(lastProtocol);
            protocolKnown = WinHttpQueryOption(handle, WINHTTP_OPTION_HTTP_PROTOCOL_USED, &lastProtocol, &size) != FALSE;
        }
        return result;
    }
}

// Only opt-in protocol negotiation and reporting are adapted. Credential loading, TLS checks,
// destination, request construction and response handling use the production implementation.
#define WinHttpOpen ::AuthorizedProbe::Open
#define WinHttpReceiveResponse ::AuthorizedProbe::ReceiveResponse
#include "../src/AetherNexus/Services/WindowsManagementTransport.cpp"
#undef WinHttpOpen
#undef WinHttpReceiveResponse

namespace AuthorizedProbe
{
    using namespace AetherNexus::Services;
    using namespace AetherNexus::Infrastructure;
    using winrt::Windows::Data::Json::JsonObject;

    void Require(bool condition)
    {
        if (!condition) throw std::invalid_argument("Authorized diagnostic validation failed");
    }
    char const *Content(ResponseContentKind kind)
    {
        switch (kind)
        {
        case ResponseContentKind::Json: return "JSON";
        case ResponseContentKind::Html: return "HTML";
        case ResponseContentKind::Other: return "other";
        default: return "missing-or-unrecognized";
        }
    }
    char const *Body(ResponseBodyKind kind)
    {
        switch (kind)
        {
        case ResponseBodyKind::Empty: return "empty";
        case ResponseBodyKind::JsonError: return "JSON-error";
        case ResponseBodyKind::HtmlLike: return "HTML-like";
        case ResponseBodyKind::Truncated: return "truncated";
        case ResponseBodyKind::ReadFailed: return "read-failed";
        case ResponseBodyKind::NotRead: return "not-read";
        default: return "other";
        }
    }
    class Transport final : public IManagementTransport
    {
      public:
        Transport(std::wstring id, bool revoke) : m_id(std::move(id)), m_revoke(revoke) {}
        ApiResponse Send(ManagementRequest const &request, std::stop_token stop) override
        {
            // The ID is supplied by the user at invocation, never embedded in tracked sources.
            Require(request.resource == ManagementResource::AdminKey && request.item == m_id &&
                    request.keyKind == ManagementKeyKind::Admin && request.project.empty() && request.body.empty());
            Require(request.method == ManagementMethod::Get || request.method == ManagementMethod::Delete);
            if (request.method == ManagementMethod::Delete)
                Require(m_revoke && ++m_deleteAttempts == 1); // Exactly one destructive attempt per invocation.
            protocolKnown = false;
            auto response = m_transport.Send(request, stop);
            auto const &facts = response.diagnostics;
            std::cout << (request.method == ManagementMethod::Get ? "GET" : "DELETE")
                      << " HTTP=" << response.status
                      << " Protocol=" << (!protocolKnown ? "unknown" : lastProtocol & WINHTTP_PROTOCOL_FLAG_HTTP2 ? "HTTP/2" : "HTTP/1.x")
                      << " Content=" << Content(facts.content) << " Body=" << Body(facts.body.kind)
                      << " RequestIdState=" << static_cast<int>(facts.requestIdState)
                      << " OpenAiMetadata=" << (facts.openAiMetadata ? "present" : "absent") << '\n';
            if (!response.requestId.empty()) std::wcout << L"ServerRequestId=" << response.requestId << L'\n';
            if (!facts.clientRequestId.empty()) std::wcout << L"ClientRequestId=" << facts.clientRequestId << L'\n';
            return response;
        }
      private:
        std::wstring m_id;
        bool m_revoke{};
        unsigned m_deleteAttempts{};
        WindowsManagementTransport m_transport;
    };
    bool Verified(JsonObject const &object, std::wstring_view id)
    {
        return object.GetNamedString(L"id", L"") == id &&
               object.GetNamedString(L"object", L"") == L"organization.admin_api_key";
    }
    int Run(std::wstring id, bool revoke)
    {
        ManagementRecord selected;
        selected.id = id;
        selected.keyKind = ManagementKeyKind::Admin;
        auto request = RevokeAdminKeyRequest(selected);
        auto read = request;
        read.method = ManagementMethod::Get;
        auto transport = std::make_shared<Transport>(id, revoke);
        auto response = transport->Send(read, {});
        if (response.status != 200)
        {
            std::cout << "Verification=unavailable; DELETE not sent.\n";
            return 2;
        }
        JsonObject object;
        Require(JsonObject::TryParse(response.body, object) && Verified(object, id));
        std::cout << "Verification=exact-ID-and-Admin-object-matched\n";

        // Conservatively refuse if the returned mask matches the currently stored signing key's suffix.
        // This guard prints no suffix, full value or mask. No credential is saved or removed locally.
        auto signer = CredentialStore{}.Read(TokenKind::Admin);
        Require(signer && signer->View().size() >= 4);
        auto mask = object.GetNamedString(L"redacted_value", L"");
        Require(!std::wstring_view{mask}.ends_with(signer->View().substr(signer->View().size() - 4)));
        signer.reset();
        std::cout << "SignerGuard=passed\n";
        if (!revoke)
        {
            std::cout << "INSPECTION ONLY: zero DELETE requests.\n";
            return 0;
        }
        bool deleted{};
        ManagementClient client{transport};
        try
        {
            deleted = client.Change(request).id == id;
            std::cout << "DeletionReply=" << (deleted ? "validated-id-object-and-deleted-true" : "unconfirmed") << '\n';
        }
        catch (ManagementHttpError const &error)
        {
            std::cout << "DeletionReply=rejected HTTP=" << error.status
                      << " Stage=" << static_cast<int>(error.operation) << '\n';
        }
        catch (ApiError const &error)
        {
            std::cout << "DeletionReply=unconfirmed ApiError=" << static_cast<int>(error.code) << '\n';
        }
        // Always use a read-only post-check. Never retry DELETE or choose another endpoint.
        auto after = transport->Send(read, {});
        if (after.status == 200)
        {
            JsonObject current;
            Require(JsonObject::TryParse(after.body, current) && Verified(current, id));
            std::cout << "PostVerification=target-still-exists\n";
            return 3;
        }
        if (after.status == 404 && deleted)
        {
            std::cout << "PostVerification=target-not-found-after-validated-deletion\n";
            return 0;
        }
        std::cout << "PostVerification=unconfirmed\n";
        return 4;
    }
}

int main(int count, char **values)
{
    using namespace AuthorizedProbe;
    if (count == 1)
    {
        std::cout << "PREVIEW ONLY: no credential reads or network calls.\n"
                     "--inspect <tracking-id> performs only exact-ID GET verification.\n"
                     "--revoke <tracking-id> --confirm-id <same-id> permits one DELETE after verification, followed by GET.\n"
                     "Optional --prefer-http2 enables normal HTTP/2 negotiation, retaining HTTP/1.1 fallback and TLS verification.\n";
        return 0;
    }
    try
    {
        if (std::string_view{values[count - 1]} == "--prefer-http2") { preferHttp2 = true; --count; }
        bool inspect = count == 3 && std::string_view{values[1]} == "--inspect";
        bool revoke = count == 5 && std::string_view{values[1]} == "--revoke" &&
                      std::string_view{values[3]} == "--confirm-id" && std::string_view{values[2]} == values[4];
        Require(inspect || revoke);
        auto id = winrt::to_hstring(values[2]);
        Require(std::wstring_view{id}.starts_with(L"key_"));
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        return Run(std::wstring{id}, revoke);
    }
    catch (ApiError const &error) { std::cout << "Stopped safely: ApiError=" << static_cast<int>(error.code) << '\n'; return 5; }
    catch (...) { std::cerr << "Stopped safely: verification/credential/argument failure; no raw data is printed.\n"; return 6; }
}
