// SPDX-License-Identifier: GPL-3.0-only
#include "Services/ManagementClient.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <deque>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace AetherNexus::Services;
using namespace winrt;
using namespace Windows::Data::Json;
void RunKeyIssuanceTests();
namespace
{
    void Check(bool value, char const *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    template <class Work> void Reject(Work work)
    {
        bool failed = false;
        try
        {
            work();
        }
        catch (...)
        {
            failed = true;
        }
        Check(failed, "Expected rejection");
    }
    class Offline final : public IManagementTransport
    {
      public:
        std::deque<ApiResponse> responses;
        std::vector<ManagementRequest> calls;
        std::function<void()> hook;
        ApiResponse Send(ManagementRequest const &request, std::stop_token stop) override
        {
            CheckCancelled(stop);
            calls.push_back(request);
            if (hook)
                hook();
            if (responses.empty())
                throw std::runtime_error("Unexpected offline request");
            auto value = std::move(responses.front());
            responses.pop_front();
            return value;
        }
    };
    std::wstring const project =
        LR"({"object":"organization.project","id":"proj_test","name":"測試專案","status":"active","created_at":1700000000})";
    std::wstring List(std::wstring const &row, bool more = false)
    {
        return L"{\"object\":\"list\",\"data\":[" + row + L"],\"has_more\":" + (more ? L"true" : L"false") + L"}";
    }
    std::wstring Page(std::wstring const &result, bool more = false, std::wstring cursor = L"next", int64_t start = 0)
    {
        return L"{\"object\":\"page\",\"data\":[{\"object\":\"bucket\",\"start_time\":" + std::to_wstring(start) +
               L",\"end_time\":" + std::to_wstring(start + 86400) + L",\"results\":[" + result + L"]}],\"has_more\":" +
               (more ? L"true" : L"false") + L",\"next_page\":\"" + cursor + L"\"}";
    }
} // namespace
int main()
{
    init_apartment(apartment_type::multi_threaded);
    try
    {
        RunKeyIssuanceTests();
        ManagementRequest r;
        Check(ManagementPath(r) == L"/v1/organization/projects?limit=100&include_archived=true", "Project catalog");
        r.resource = ManagementResource::ProjectKeys;
        r.project = L"proj_test";
        r.page = L"cursor /?";
        auto path = ManagementPath(r);
        Check(path.find(L"owner_project_access=any") != std::wstring::npos &&
                  path.find(L"cursor%20%2F%3F") != std::wstring::npos,
              "Safe pagination");
        r.project = L"../admin_api_keys";
        Reject([&] { ManagementPath(r); });
        r.project = L"proj_test";
        r.method = ManagementMethod::Delete;
        Reject([&] { ManagementPath(r); });
        r = {};
        r.resource = ManagementResource::Usage;
        r.category = L"costs";
        r.group = L"model";
        r.end = 86400;
        Reject([&] { ManagementPath(r); });
        r.group = L"line_item";
        r.project = L"proj_test";
        r.page = L"next";
        path = ManagementPath(r);
        Check(path.find(L"project_ids%5B%5D=proj_test") != std::wstring::npos &&
                  path.find(L"group_by%5B%5D=line_item") != std::wstring::npos,
              "Costs filter");
        r.start = 1;
        Reject([&] { ManagementPath(r); });
        r.start = 0;
        r.end = 367LL * 86400;
        Reject([&] { ManagementPath(r); });
        r = {};
        r.resource = static_cast<ManagementResource>(999);
        Reject([&] { ManagementPath(r); });
        Check(JsonObject::Parse(ProjectNameBody(L"中文 \"name\"")).GetNamedString(L"name") == L"中文 \"name\"",
              "JSON escaping");
        Reject([] { ProjectNameBody(L" \n"); });
        Reject([] { MemberBody(L"user_test", L"admin"); });
        auto all = JsonObject::Parse(KeyPolicyBody(std::vector<std::wstring>{}, {}));
        Check(all.GetNamedArray(L"scopes").Size() == 0, "Empty scopes means All");
        auto readonly = JsonObject::Parse(KeyPolicyBody(std::vector<std::wstring>{L"api.all.read"}, 86400));
        Check(readonly.GetNamedNumber(L"expires_in_seconds") == 86400, "Expiry seconds");
        auto expiry = JsonObject::Parse(KeyPolicyBody({}, 3600));
        Check(!expiry.HasKey(L"scopes"), "Omitted scopes preserved");
        Reject([] { KeyPolicyBody({}, {}); });
        Reject([] { KeyPolicyBody({}, 31536001); });
        Reject([] { KeyPolicyBody(std::vector<std::wstring>{L"bad\r\n"}, {}); });
        Reject([] { RateLimitBody({{L"unknown", 1}}); });
        Reject([] { RateLimitBody({}); });
        std::cout << "PASS: typed endpoints, safe IDs, filters, UTC bounds, policy semantics and validation.\n";

        auto transport = std::make_shared<Offline>();
        ManagementClient client{transport};
        transport->responses = {
            {200, List(project, true)},
            {200, List(LR"({"object":"organization.project","id":"proj_archived","name":"Old","status":"archived"})")}};
        auto projects = client.List(ManagementResource::Projects);
        Check(projects.size() == 2 && projects[1].status == L"archived", "Archived projects");
        Check(transport->calls.back().page == L"proj_test", "Project cursor");
        transport->responses = {{200, List(project, true)}, {200, List(project)}};
        Reject([&] { client.List(ManagementResource::Projects); });
        transport->responses = {{200, L"{\"object\":\"list\",\"data\":[],\"has_more\":true}"}};
        Reject([&] { client.List(ManagementResource::Projects); });
        auto key =
            LR"({"object":"organization.project.api_key","id":"key_test","name":"Offline","redacted_value":"offline-secret-1234","value":"must-not-retain","owner":{"type":"user","user":{"id":"user_test","name":"Owner"}},"scopes":["api.all.read"],"expires_at":null})";
        transport->responses = {{200, List(key)}};
        auto keys = client.List(ManagementResource::ProjectKeys, L"proj_test");
        Check(keys.size() == 1 && keys[0].keyHint == L"…1234" && keys[0].owner == L"Owner" && keys[0].scopesKnown,
              "Redacted metadata only");
        transport->responses = {{200, List(project)}};
        Reject([&] { client.List(ManagementResource::AdminKeys); });
        transport->responses = {
            {200,
             List(
                 LR"({"object":"project.rate_limit","id":"rl_test","model":"model_test","max_requests_per_1_minute":100,"max_tokens_per_1_minute":1000})")}};
        auto limits = client.List(ManagementResource::RateLimits, L"proj_test");
        Check(limits[0].limits.size() == 2, "Supported rate limits");
        r = {};
        r.resource = ManagementResource::Projects;
        r.method = ManagementMethod::Post;
        r.body = ProjectNameBody(L"Test");
        transport->responses = {{200, project}};
        Check(client.Change(r).id == L"proj_test", "Create project");
        r.resource = ManagementResource::Project;
        r.project = L"proj_test";
        transport->responses = {{200, project}};
        Check(client.Change(r).name == L"測試專案", "Update project");
        r.resource = ManagementResource::ProjectKey;
        r.item = L"key_test";
        r.method = ManagementMethod::Delete;
        r.body.clear();
        transport->responses = {{200, LR"({"id":"key_test","deleted":true})"}};
        Check(client.Change(r).id == L"key_test", "Revoke key");
        transport->responses = {{200, LR"({"id":"wrong","deleted":true})"}};
        Reject([&] { client.Change(r); });
        auto count = transport->calls.size();
        transport->responses = {{503, L"private error"}};
        Reject([&] { client.Change(r); });
        Check(transport->calls.size() == count + 1, "No mutation retry");
        std::stop_source cancelled;
        cancelled.request_stop();
        count = transport->calls.size();
        Reject([&] { client.List(ManagementResource::Projects, {}, cancelled.get_token()); });
        Check(transport->calls.size() == count, "Cancel before send");
        std::cout << "PASS: pagination, metadata redaction, project/key CRUD, malformed replies, cancellation and no "
                     "retries.\n";

        UsageQuery query;
        query.end = 2 * 86400;
        query.project = L"proj_test";
        query.group = L"model";
        auto row =
            LR"({"model":"model_a","input_tokens":100,"output_tokens":20,"input_cached_tokens":40,"num_model_requests":2})";
        transport->responses = {{200, Page(row, true)}, {200, Page(L"", false, L"", 86400)}};
        auto usage = client.Usage(query);
        Check(usage.Total(L"input_tokens") == 100 && usage.Total(L"input_cached_tokens") == 40, "No double-counting");
        Check(transport->calls.back().page == L"next" && transport->calls.back().project == L"proj_test" &&
                  transport->calls.back().group == L"model",
              "Retained filters");
        transport->responses = {
            {200, Page(LR"({"model":"a","input_tokens":5},{"model":"b","input_tokens":7,"input_cached_tokens":2})")}};
        usage = client.Usage(query);
        Check(!usage.Total(L"input_cached_tokens") && usage.Total(L"input_tokens") == 12, "Unknown field is not zero");
        query.category = L"costs";
        query.group = L"line_item";
        query.end = 86400;
        transport->responses = {
            {200, Page(LR"json({"line_item":"=SUM(A1)","amount":{"value":-0.25,"currency":"usd"}})json")}};
        usage = client.Usage(query);
        Check(usage.Total(L"amount_usd") == -0.25, "Credits remain negative");
        auto csv = usage.Csv();
        Check(csv.find(L"'=SUM(A1)") != std::wstring::npos && csv.find(L"\"-0.25\"") != std::wstring::npos,
              "CSV formula safety and numeric fidelity");
        transport->responses = {{200, Page(LR"({"amount":{"value":1,"currency":"eur"}})")}};
        Reject([&] { client.Usage(query); });
        transport->responses = {{200, Page(LR"({"amount":{"value":1,"currency":"usd"}})", true)},
                                {200, Page(LR"({"amount":{"value":1,"currency":"usd"}})")}};
        Reject([&] { client.Usage(query); });
        query.category = L"completions";
        query.group.clear();
        transport->responses = {{200, Page(LR"({"input_tokens":-1})")}};
        Reject([&] { client.Usage(query); });
        transport->responses = {{200, L"{\"object\":\"page\",\"data\":[],\"has_more\":false}"}};
        usage = client.Usage(query);
        Check(usage.Total(L"input_tokens") == 0, "Empty query is zero activity");
        query.category = L"vector_stores";
        transport->responses = {{200, Page(LR"({"usage_bytes":2048})")}};
        usage = client.Usage(query);
        Check(!usage.Total(L"usage_bytes") && usage.Daily(L"usage_bytes").begin()->second == 2048,
              "Storage is not summed over time");
        std::cout << "PASS: usage pagination, missing metrics, credits, currency, CSV, empty ranges and storage gauge "
                     "semantics.\nNo live transport, credentials or network calls are linked into this executable.\n";
    }
    catch (std::exception const &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    catch (hresult_error const &error)
    {
        std::cerr << "FAIL HRESULT: " << std::hex << static_cast<uint32_t>(error.code().value) << '\n';
        return 1;
    }
}
