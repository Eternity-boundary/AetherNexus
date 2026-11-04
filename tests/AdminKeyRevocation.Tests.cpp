// SPDX-License-Identifier: GPL-3.0-only
#include "Services/ManagementClient.h"
#include <functional>
#include <iostream>
#include <deque>
using namespace AetherNexus::Services;
namespace
{
    void Check(bool ok)
    {
        if (!ok)
            throw std::runtime_error("Admin revocation assertion failed");
    }
    void Reject(std::function<void()> const &action)
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
    class Offline final : public IManagementTransport
    {
      public:
        std::deque<ApiResponse> replies;
        std::vector<ManagementWireRequest> requests;
        ApiResponse Send(ManagementRequest const &request, std::stop_token stop) override
        {
            CheckCancelled(stop);
            requests.push_back(BuildManagementWireRequest(request));
            Check(!replies.empty());
            auto response = std::move(replies.front());
            replies.pop_front();
            return response;
        }
    };
} // namespace
void RunAdminKeyRevocationTests()
{
    auto transport = std::make_shared<Offline>();
    ManagementClient client{transport};
    auto admin =
        LR"({"object":"organization.admin_api_key","id":"key_new_admin","name":"New Admin Key","redacted_value":"…5678"})";
    ManagementRecord project;
    project.id = L"proj_offline";
    transport->replies = {
        {200,
         LR"({"object":"list","data":[{"object":"organization.admin_api_key","id":"key_project","redacted_value":"…1111","owner":{"role":"owner"}},{"object":"organization.admin_api_key","id":"key_new_admin","name":"New Admin Key","redacted_value":"…5678"},{"object":"organization.admin_api_key","id":"key_legacy","redacted_value":"sk-...2222"},{"object":"organization.admin_api_key","id":"key_generic","redacted_value":"…3333"},{"object":"organization.project.api_key","id":"key_project_obj","redacted_value":"sk-admin...4444"}],"has_more":false})"},
        {200,
         LR"({"object":"list","data":[{"object":"organization.project.api_key","id":"key_project","redacted_value":"…1111"},{"object":"organization.project.api_key","id":"key_legacy","redacted_value":"sk-admin...2222"}],"has_more":false})"}};
    auto directory = client.OrganizationKeys({project});
    auto keys = directory.records;
    Check(!directory.projectLookupIncomplete && transport->requests.back().path ==
          L"/v1/organization/projects/proj_offline/api_keys?limit=100&owner_project_access=any");
    Check(keys.size() == 5 && keys[0].keyKind == ManagementKeyKind::Api &&
          keys[1].keyKind == ManagementKeyKind::Admin && keys[1].id == L"key_new_admin" &&
          keys[2].keyKind == ManagementKeyKind::Api && keys[3].keyKind == ManagementKeyKind::Admin &&
          keys[4].keyKind == ManagementKeyKind::Api);
    auto request = RevokeAdminKeyRequest(keys.at(1));
    Check(request.resource == ManagementResource::AdminKey && request.method == ManagementMethod::Delete &&
          request.project.empty() && request.body.empty() && request.item == L"key_new_admin");
    auto wire = BuildManagementWireRequest(request);
    Check(wire.method == L"DELETE" && wire.path == L"/v1/organization/admin_api_keys/key_new_admin" &&
          wire.body.empty());
    Check(wire.headers.find(L"Content-Type: application/json\r\n") != std::wstring::npos &&
          wire.headers.find(L"Accept: application/json\r\n") != std::wstring::npos);
    Check(wire.path.find(L"5678") ==
          std::wstring::npos); // Use the API object ID, never the masked key/secret.
    transport->replies = {
        {200, admin},
        {200, LR"({"id":"key_new_admin","object":"organization.admin_api_key.deleted","deleted":true})"}};
    auto beforeSuccess = transport->requests.size();
    Check(client.Change(request).id == request.item);
    Check(transport->requests.size() == beforeSuccess + 2 &&
          transport->requests[beforeSuccess].method == L"GET");
    Check(transport->requests.back().path == wire.path && transport->requests.back().method == L"DELETE");

    // Response correlation IDs are display metadata, not arbitrary server text or header lines.
    Check(ManagementRequestId(L"a") == L"a");
    Check(ManagementRequestId(L"req_a") == L"req_a");
    Check(ManagementRequestId(L"req_") == L"req_");
    Check(ManagementRequestId(L"request_a") == L"request_a");
    Check(ManagementRequestId(L"Req_a") == L"Req_a");
    Check(ManagementRequestId(L"550e8400-e29b-41d4-a716-446655440000") ==
          L"550e8400-e29b-41d4-a716-446655440000");
    Check(ManagementRequestId(L"req_Offline_Az09-_") == L"req_Offline_Az09-_");
    auto longestRequestId = std::wstring(128, L'a');
    Check(ManagementRequestId(longestRequestId) == longestRequestId);
    for (auto badId : {L"", L"sk-offline-never-valid", L"req_a\r\nInjected: header",
                       L"req_a b", L"req_a/../b", L"req_a\t", L"req_權杖"})
        Check(ManagementRequestId(badId).empty());
    Check(ManagementRequestId(longestRequestId + L"a").empty());

    transport->replies = {{404, L"private server verification body", L"req_offline_verify_404"}};
    auto beforeVerification404 = transport->requests.size();
    bool verification404Classified = false;
    try
    {
        (void)client.Change(request);
    }
    catch (ManagementHttpError const &error)
    {
        verification404Classified = error.status == 404 &&
            error.operation == ManagementOperation::AdminVerification &&
            error.requestId == L"req_offline_verify_404" &&
            std::string_view{error.what()}.find("private") == std::string_view::npos;
    }
    Check(verification404Classified && transport->requests.size() == beforeVerification404 + 1 &&
          transport->requests.back().method == L"GET" && transport->replies.empty());

    for (auto body :
         {LR"({"id":"wrong","object":"organization.admin_api_key.deleted","deleted":true})",
          LR"({"id":"key_new_admin","object":"organization.project.api_key.deleted","deleted":true})",
          LR"({"id":"key_new_admin","object":"organization.admin_api_key.deleted","deleted":false})",
          LR"({"id":"key_new_admin","deleted":true})"})
    {
        transport->replies = {{200, admin}, {200, body}};
        Reject([&] { client.Change(request); });
    }
    for (unsigned status : {400u, 404u, 409u, 415u, 422u})
    {
        transport->replies = {{200, admin}, {status, L"private server message must not be displayed", L"req_offline_delete"}};
        auto before = transport->requests.size();
        bool classified = false;
        try
        {
            (void)client.Change(request);
        }
        catch (ManagementHttpError const &error)
        {
            classified = error.status == status && error.operation == ManagementOperation::AdminRevocation &&
                         error.requestId == L"req_offline_delete" &&
                         std::string_view{error.what()}.find("private") == std::string_view::npos;
        }
        Check(classified && transport->requests.size() == before + 2 &&
              transport->requests.back().method == L"DELETE");
    }
    // Invalid IDs are dropped on both preflight and DELETE errors without exposing the body.
    for (bool verification : {true, false})
    {
        transport->replies.clear();
        if (!verification)
            transport->replies.push_back({200, admin});
        transport->replies.push_back({404, L"private rejection body", L"req_bad\r\nprivate: header"});
        auto beforeDiagnostic = transport->requests.size();
        bool sanitized = false;
        try
        {
            (void)client.Change(request);
        }
        catch (ManagementHttpError const &error)
        {
            sanitized = error.status == 404 && error.requestId.empty() &&
                error.operation == (verification ? ManagementOperation::AdminVerification : ManagementOperation::AdminRevocation) &&
                std::string_view{error.what()}.find("private") == std::string_view::npos;
        }
        Check(sanitized && transport->requests.size() == beforeDiagnostic + (verification ? 1 : 2) &&
              transport->requests.back().method == (verification ? L"GET" : L"DELETE"));
    }
    ManagementRequest projectRevoke;
    projectRevoke.resource = ManagementResource::ProjectKey;
    projectRevoke.method = ManagementMethod::Delete;
    projectRevoke.project = project.id;
    projectRevoke.item = L"key_project";
    transport->replies = {{404, L"private project body", L"req_offline_project"}};
    auto beforeProject404 = transport->requests.size();
    bool project404Classified = false;
    try
    {
        (void)client.Change(projectRevoke);
    }
    catch (ManagementHttpError const &error)
    {
        project404Classified = error.status == 404 &&
            error.operation == ManagementOperation::ProjectRevocation &&
            error.requestId == L"req_offline_project" &&
            std::string_view{error.what()}.find("private") == std::string_view::npos;
    }
    Check(project404Classified && transport->requests.size() == beforeProject404 + 1 &&
          transport->requests.back().method == L"DELETE" &&
          transport->requests.back().path == L"/v1/organization/projects/proj_offline/api_keys/key_project");
    for (auto [status, code] : {std::pair{401u, ApiErrorCode::Unauthorized},
                                {403u, ApiErrorCode::Forbidden},
                                {429u, ApiErrorCode::RateLimited},
                                {503u, ApiErrorCode::Server}})
    {
        transport->replies = {{status, {}}};
        bool classified = false;
        try
        {
            (void)client.Change(request);
        }
        catch (ApiError const &error)
        {
            classified = error.code == code;
        }
        Check(classified);
    }
    // Keep every record in a mixed directory and preserve the server's pagination cursor.
    transport->replies = {
        {200,
         LR"({"object":"list","data":[{"object":"organization.admin_api_key","id":"key_api_cursor","redacted_value":"sk-proj-...5555"}],"last_id":"key_api_cursor","has_more":true})"},
        {200,
         LR"({"object":"list","data":[{"object":"organization.admin_api_key","id":"key_admin_page2","redacted_value":"sk-admin-...6666"}],"has_more":false})"}};
    keys = client.List(ManagementResource::AdminKeys);
    Check(keys.size() == 2 && keys[0].id == L"key_api_cursor" && keys[1].id == L"key_admin_page2" &&
          transport->requests.back().path.find(L"after=key_api_cursor") != std::wstring::npos);
    // Object type, not any display-mask format, determines the Admin resource and revoke capability.
    for (auto mask : {L"sk-...9876", L"...9876", L"…9876", L"****9876", L"future-mask-9876", L"sk-proj-...9876", L""})
    {
        auto row = L"{\"object\":\"organization.admin_api_key\",\"id\":\"key_masked\",\"redacted_value\":\"" +
            std::wstring{mask} + L"\"}";
        transport->replies = {{200, L"{\"object\":\"list\",\"data\":[" + row + L"],\"has_more\":false}"}};
        auto masked = client.List(ManagementResource::AdminKeys);
        Check(masked.size() == 1 && masked[0].id == L"key_masked" &&
              masked[0].keyKind == ManagementKeyKind::Admin &&
              masked[0].keyHint == (std::wstring_view{mask}.empty() ? L"—" : L"…9876"));
        auto maskedRequest = RevokeAdminKeyRequest(masked[0]);
        transport->replies = {{200, row}, {200, LR"({"id":"key_masked","object":"organization.admin_api_key.deleted","deleted":true})"}};
        auto beforeDelete = transport->requests.size();
        Check(client.Change(maskedRequest).id == L"key_masked");
        Check(transport->requests.size() == beforeDelete + 2 && transport->requests.back().method == L"DELETE");
    }
    // Permission-denied or removed projects must not erase valid Admin object classifications.
    for (auto status : {403u, 404u})
    {
        transport->replies = {{200, L"{\"object\":\"list\",\"data\":[" + std::wstring{admin} + L"],\"has_more\":false}"}, {status, {}}};
        auto partial = client.OrganizationKeys({project});
        Check(partial.projectLookupIncomplete && partial.records.size() == 1 &&
              partial.records[0].keyKind == ManagementKeyKind::Admin);
    }
    // Malformed or wrong API resource types/IDs are read failures, never a reason to DELETE elsewhere.
    for (
        auto response :
        {ApiResponse{404, L"private not found"},
         ApiResponse{
             200,
             LR"({"object":"organization.project.api_key","id":"key_new_admin","redacted_value":"…1234"})"},
         ApiResponse{
             200,
             LR"({"id":"key_new_admin","redacted_value":"…1234"})"},
         ApiResponse{
             200,
             LR"({"object":"organization.admin_api_key","id":"wrong","redacted_value":"sk-admin...1234"})"}})
    {
        transport->replies = {response};
        auto before = transport->requests.size();
        Reject([&] { client.Change(request); });
        Check(transport->requests.size() == before + 1 && transport->requests.back().method == L"GET");
    }
    auto before = transport->requests.size();
    auto invalid = keys[1];
    invalid.id.clear();
    Reject([&] { (void)RevokeAdminKeyRequest(invalid); });
    invalid.id = L"../key_bad";
    Reject([&] { (void)RevokeAdminKeyRequest(invalid); });
    invalid.id = L"key_bad\r\nInjected: header";
    Reject([&] { (void)RevokeAdminKeyRequest(invalid); });
    invalid.id = L"key_api";
    invalid.keyKind = ManagementKeyKind::Api;
    Reject([&] { (void)RevokeAdminKeyRequest(invalid); });
    invalid.keyKind = ManagementKeyKind::Unknown;
    Reject([&] { (void)RevokeAdminKeyRequest(invalid); });
    auto unclassified = request;
    unclassified.keyKind = ManagementKeyKind::Unknown;
    Reject([&] { client.Change(unclassified); });
    Check(transport->requests.size() == before);
    std::stop_source stop;
    stop.request_stop();
    Reject([&] { (void)client.Change(request, stop.get_token()); });
    Check(transport->requests.size() == before);
    std::cout
        << "PASS: official object types and project ID correlation, prefix-independent Admin revocation, partial project diagnostics, raw pagination retained, Admin GET verification before "
           "DELETE, "
           "unknown/API kinds blocked, JSON headers, phase-specific safe HTTP diagnostics, sanitized request IDs and no destructive fallback.\n";
}
