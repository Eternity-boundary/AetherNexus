// SPDX-License-Identifier: GPL-3.0-only
#include "Services/KeyIssuance.h"
#include <winrt/Windows.Data.Json.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <type_traits>
using namespace AetherNexus::Services;
using AetherNexus::Infrastructure::SecureBuffer;
namespace
{
void Check(bool ok)
{
    if (!ok)
        throw std::runtime_error("Key issuance assertion failed");
}
template <class F> void Reject(F action)
{
    bool rejected = false;
    try
    {
        action();
    }
    catch (...)
    {
        rejected = true;
    }
    Check(rejected);
}
SecureBuffer Bytes(std::string_view value)
{
    SecureBuffer result{value.size()};
    result.Append(value);
    return result;
}
class OfflineIssuer final : public IKeyIssuanceTransport
{
  public:
    unsigned calls{}, status{200};
    std::string_view json = R"({"value":"offline-token-never-valid-1234"})";
    std::function<void()> hook;
    SensitiveApiResponse Issue(KeyIssueRequest const &, std::stop_token) override
    {
        ++calls;
        if (hook)
            hook();
        return {status, Bytes(json)};
    }
};
} // namespace
void RunKeyIssuanceTests()
{
    static_assert(!std::is_copy_constructible_v<SecureBuffer> && !std::is_copy_assignable_v<SecureBuffer>);
    static_assert(!std::is_copy_constructible_v<IssuedKey>);
    SecureBuffer buffer{128};
    std::fill_n(buffer.Data(), buffer.Capacity(), 'X');
    buffer.Append("offline-secret");
    auto allocation = buffer.Data();
    auto capacity = buffer.Capacity();
    buffer.Clear();
    Check(buffer.View().empty() &&
          std::all_of(allocation, allocation + capacity, [](char c) { return c == 0; }));
    buffer.Append("offline-move");
    SecureBuffer moved{std::move(buffer)};
    Check(buffer.View().empty() && buffer.Capacity() == 0 && moved.Data() == allocation &&
          moved.View() == "offline-move");
    SecureBuffer assigned{16};
    assigned.Append("old-secret");
    assigned = std::move(moved);
    Check(moved.View().empty() && assigned.Data() == allocation);
    assigned.Clear();
    Check(std::all_of(allocation, allocation + capacity, [](char c) { return c == 0; }));
    SecureBuffer bounded{2};
    Reject([&] { bounded.Append("too long"); });

    KeyIssueRequest request;
    request.name = L"Offline \"name\"";
    request.expiresInSeconds = 86400;
    Check(KeyIssuePath(request) == L"/v1/organization/admin_api_keys");
    auto body = winrt::Windows::Data::Json::JsonObject::Parse(KeyIssueBody(request));
    Check(body.GetNamedString(L"name") == request.name &&
          body.GetNamedNumber(L"expires_in_seconds") == 86400);
    request.project = L"proj_offline";
    Reject([&] { KeyIssuePath(request); });
    request.kind = KeyIssueKind::NewServiceAccount;
    Check(KeyIssuePath(request).ends_with(L"/proj_offline/service_accounts"));
    request.kind = KeyIssueKind::ServiceAccount;
    request.serviceAccount = L"svc_offline";
    Check(KeyIssuePath(request).ends_with(L"/proj_offline/service_accounts/svc_offline/api_keys"));
    request.serviceAccount = L"../admin_api_keys";
    Reject([&] { KeyIssuePath(request); });
    request = {};
    request.name = L"Offline";
    request.expiresInSeconds = 0;
    Reject([&] { KeyIssuePath(request); });
    request.expiresInSeconds = 31536001;
    Reject([&] { KeyIssuePath(request); });
    request.expiresInSeconds.reset();

    auto transport = std::make_shared<OfflineIssuer>();
    KeyIssuanceClient client{transport};
    auto issued = client.Issue(request);
    Check(issued.secret.View() == "offline-token-never-valid-1234" && issued.Hint() == L"•••• 1234");
    auto secretAllocation = issued.secret.Data();
    auto secretCapacity = issued.secret.Capacity();
    issued.secret.Clear();
    Check(std::all_of(secretAllocation, secretAllocation + secretCapacity, [](char c) { return c == 0; }));
    Check(issued.Hint() == L"—");
    transport->json = R"({"v\u0061lue":"offline-\u0074oken-\"quoted\"-1234"})";
    issued = client.Issue(request);
    Check(issued.secret.View() == "offline-token-\"quoted\"-1234");
    transport->json = R"({"value":"first-offline-key","v\u0061lue":"second-offline-key"})";
    Reject([&] { client.Issue(request); });
    transport->json = R"({"value":"offline\nkey"})";
    Reject([&] { client.Issue(request); });
    transport->json = R"({"value":"offline\u0000key"})";
    Reject([&] { client.Issue(request); });
    transport->json = R"({"value":"offline\u0080key"})";
    Reject([&] { client.Issue(request); });
    transport->json = R"({"other":{"value":"misplaced-offline-key"}})";
    Reject([&] { client.Issue(request); });
    transport->json = R"({"value":"offline-key"} trailing)";
    Reject([&] { client.Issue(request); });
    transport->json = R"({"value":""})";
    Reject([&] { client.Issue(request); });
    transport->json = R"({"object":"organization.project.api_key","value":"offline-key"})";
    Reject([&] { client.Issue(request); });
    request.kind = KeyIssueKind::NewServiceAccount;
    request.project = L"proj_offline";
    transport->json =
        R"({"object":"organization.project.service_account","id":"svc_offline","api_key":{"object":"organization.project.service_account.api_key","id":"key_offline","value":"offline-service-key-1234"}})";
    issued = client.Issue(request);
    Check(issued.id == L"key_offline" && issued.serviceAccount == L"svc_offline");
    request.kind = KeyIssueKind::ServiceAccount;
    request.serviceAccount = L"svc_existing";
    transport->json =
        R"({"object":"organization.project.service_account.api_key","id":"key_offline","value":"offline-service-key-1234"})";
    issued = client.Issue(request);
    Check(issued.serviceAccount == L"svc_existing");
    auto count = transport->calls;
    transport->status = 503;
    Reject([&] { client.Issue(request); });
    Check(transport->calls == count + 1); // Never retry a credential-creating POST.
    std::stop_source stop;
    stop.request_stop();
    count = transport->calls;
    Reject([&] { client.Issue(request, stop.get_token()); });
    Check(transport->calls == count);
    stop = {};
    transport->status = 200;
    transport->hook = [&] { stop.request_stop(); };
    Reject([&] { client.Issue(request, stop.get_token()); });
    std::cout << "PASS: issuance endpoints, validation, single POST, redaction-before-JSON, escaped/invalid "
                 "secrets, cancellation and full-capacity secure wiping.\n";
}
