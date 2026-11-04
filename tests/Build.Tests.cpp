// SPDX-License-Identifier: GPL-3.0-only
#include "Services/BuildClient.h"
#include "Services/PromptStore.h"
#include <chrono>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <windows.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
using namespace AetherNexus::Services;
using namespace winrt;
using namespace Windows::Data::Json;
namespace
{
unsigned checks{};
void Check(bool value)
{
    ++checks;
    if (!value)
        throw std::runtime_error("Build contract assertion failed");
}
template <class F> void Reject(F work)
{
    bool rejected{};
    try
    {
        work();
    }
    catch (...)
    {
        rejected = true;
    }
    Check(rejected);
}
struct Offline final : IBuildTransport
{
    std::deque<BuildResponse> replies;
    std::vector<BuildRequest> calls;
    std::function<void()> hook;
    BuildResponse Send(BuildRequest const &request, std::stop_token stop) override
    {
        CheckCancelled(stop);
        calls.push_back(request);
        if (hook)
            hook();
        if (replies.empty())
            throw std::runtime_error("Unexpected test call");
        auto result = replies.front();
        replies.pop_front();
        return result;
    }
};
BuildRequest Request(BuildResource resource, std::wstring parent = L"", std::wstring item = L"")
{
    BuildRequest request;
    request.resource = resource;
    request.parent = std::move(parent);
    request.item = std::move(item);
    return request;
}
int64_t Time(int year, unsigned month, unsigned day)
{
    using namespace std::chrono;
    return duration_cast<seconds>(sys_days{std::chrono::year{year} / month / day}.time_since_epoch()).count();
}
} // namespace
int main()
{
    init_apartment();
    try
    {
        auto request = Request(BuildResource::Chats);
        request.filter = L"gpt-5.4:custom";
        request.after = L"chatcmpl_before";
        auto wire = BuildWire(request);
        Check(wire.path == L"/v1/chat/"
                           L"completions?limit=20&order=desc&after=chatcmpl_before&"
                           L"model=gpt-5.4%3Acustom");
        for (auto resource :
             {BuildResource::ChatMessages, BuildResource::ResponseItems, BuildResource::Traces, BuildResource::EvalRuns,
              BuildResource::FineEvents, BuildResource::FineCheckpoints, BuildResource::StoreFiles})
        {
            request = Request(resource, L"parent_test");
            Check(BuildWire(request).path.find(L"parent_test/") != std::wstring::npos);
        }
        request = Request(BuildResource::Traces, L"sess_test");
        Check(BuildWire(request).headers.find(L"OpenAI-Beta: agents=v1") != std::wstring::npos);
        Check(BuildWire(request).path.find(L"limit=5") != std::wstring::npos);
        request = Request(BuildResource::EvalOutputs, L"eval_test");
        request.scope = L"run_test";
        Check(BuildWire(request).path.starts_with(L"/v1/evals/eval_test/runs/run_test/output_items?"));
        request = Request(BuildResource::Batches);
        request.method = BuildMethod::Post;
        request.body = BatchBody(L"file_test", L"/v1/responses");
        Check(BuildWire(request).method == L"POST");
        Check(JsonObject::Parse(to_hstring(request.body)).GetNamedString(L"completion_window") == L"24h");
        Reject([] { (void)BatchBody(L"file_test", L"/v1/videos"); });
        Reject([] { (void)BatchBody(L"../file", L"/v1/responses"); });
        for (auto bad : {L"../key", L"id?x=1", L"id\r\nInjected: true", L"id/a", L""})
            Reject([&] { ValidateBuildIdentifier(bad); });
        request = Request(BuildResource::Files);
        request.project = L"proj_good\r\nX:test";
        Reject([&] { (void)BuildWire(request); });
        request = Request(BuildResource::Evals);
        request.method = BuildMethod::Post;
        request.body = "{}";
        Reject([&] { (void)BuildWire(request); });
        request = Request(BuildResource::Sessions);
        request.method = BuildMethod::Delete;
        request.item = L"sess_test";
        Reject([&] { (void)BuildWire(request); });
        request = Request(BuildResource::Responses);
        request.method = static_cast<BuildMethod>(99);
        Reject([&] { (void)BuildWire(request); });
        request = Request(BuildResource::FileContent, L"", L"file_test");
        Reject([&] { (void)BuildWire(request); });
        request.download = L"C:/offline/result.jsonl";
        Check(BuildWire(request).path == L"/v1/files/file_test/content");
        auto fine = JsonObject::Parse(
            to_hstring(FineTuneBody(L"model.test", L"file_train", L"file_valid", L"suffix", L"dpo", L"{}")));
        Check(fine.GetNamedObject(L"method").GetNamedString(L"type") == L"dpo");
        Reject([] { (void)FineTuneBody(L"model", L"file_same", L"file_same", L"", L"supervised", L"{}"); });
        Reject([] { (void)FineTuneBody(L"model", L"file_train", L"", L"", L"unknown", L"{}"); });
        auto store = JsonObject::Parse(to_hstring(StoreBody(L"索引", L"file_a, file_b\nfile_c", 3)));
        Check(store.GetNamedArray(L"file_ids").Size() == 3);
        Check(store.GetNamedObject(L"expires_after").GetNamedString(L"anchor") == L"last_active_at");
        Reject([] { (void)StoreBody(L"name", L"file_a,file_a", {}); });
        Reject([] { (void)StoreBody(L"name", L"", 366); });
        Check(JsonObject::Parse(to_hstring(StoreFileBody(L"file_test"))).GetNamedString(L"file_id") == L"file_test");
        Reject([] { (void)StoreSearchBody(L""); });
        auto prompt =
            JsonObject::Parse(to_hstring(PromptTestBody(L"model", L"system", L"input", L"{\"max_output_tokens\":32}")));
        Check(!prompt.GetNamedBoolean(L"store") && !prompt.GetNamedBoolean(L"stream"));
        Check(prompt.GetNamedNumber(L"max_output_tokens") == 32);
        Reject([] { (void)PromptTestBody(L"model", L"", L"input", L"{\"store\":true}"); });
        Check(BuildPolicyAt(Time(2026, 10, 31)).evalsReadable);
        Check(!BuildPolicyAt(Time(2026, 11, 30)).evalsReadable);
        Check(BuildPolicyAt(Time(2027, 1, 5)).fineTuningCreatable);
        Check(!BuildPolicyAt(Time(2027, 1, 6)).fineTuningCreatable);
        auto fake = std::make_shared<Offline>();
        BuildClient client{fake};
        fake->replies.push_back(
            {200,
             R"({"data":[{"id":"batch_a","status":"in_progress","request_counts":{"completed":1,"failed":0,"total":2}}],"has_more":true,"last_id":"batch_a"})"});
        auto list = client.List(Request(BuildResource::Batches));
        Check(list.hasMore && list.after == L"batch_a" && list.records[0].status == L"in_progress");
        fake->replies.push_back({200, R"({"data":[{"id":"gpt-5.4","object":"model"}]})"});
        Check(client.List(Request(BuildResource::Models)).records[0].id == L"gpt-5.4");
        fake->replies.push_back({200, R"({"data":[],"has_more":true,"last_id":"batch_a"})"});
        request = Request(BuildResource::Batches);
        request.after = L"batch_a";
        Reject([&] { (void)client.List(request); });
        fake->replies.push_back({200, R"({"data":[{"id":"file_x"},{"id":"file_x"}]})"});
        Reject([&] { (void)client.List(Request(BuildResource::Files)); });
        fake->replies.push_back({200, R"({"id":"file_other"})"});
        Reject([&] { (void)client.Retrieve(Request(BuildResource::Files, L"", L"file_test")); });
        request = Request(BuildResource::Files, L"", L"file_test");
        request.method = BuildMethod::Delete;
        fake->replies.push_back({200, R"({"id":"file_test","deleted":false})"});
        Reject([&] { (void)client.Change(request); });
        fake->replies.push_back({200, R"({"id":"file_test","deleted":true})"});
        Check(client.Change(request).id == L"file_test");
        auto before = fake->calls.size();
        fake->replies.push_back({500, {}});
        Reject([&] { (void)client.Change(request); });
        Check(fake->calls.size() == before + 1);
        fake->replies.push_back({404, {}});
        try
        {
            (void)client.Change(request);
            Check(false);
        }
        catch (BuildHttpError const &error)
        {
            Check(error.status == 404);
        }
        std::stop_source stop;
        stop.request_stop();
        before = fake->calls.size();
        Reject([&] { (void)client.List(Request(BuildResource::Files), stop.get_token()); });
        Check(fake->calls.size() == before);
        fake->hook = [&] {
            stop = {};
            stop.request_stop();
        }; // Cancellation during a received response is also rejected.
        stop = {};
        auto token = stop.get_token();
        fake->hook = [&] { stop.request_stop(); };
        fake->replies.push_back({200, R"({"data":[]})"});
        Reject([&] { (void)client.List(Request(BuildResource::Files), token); });
        fake->hook = {};
        auto spans = ParseTraceSpans(
            LR"({"otlp":{"resourceSpans":[{"scopeSpans":[{"spans":[{"spanId":"root","name":"turn","startTimeUnixNano":"1000000","endTimeUnixNano":"3500000"},{"spanId":"child","parentSpanId":"root","name":"tool"}]}]}]}})");
        Check(spans.size() == 2 && spans[1].parent == L"root" && spans[0].milliseconds == 2.5 &&
              !spans[1].milliseconds);
        auto rendered = RenderPromptVariables(L"Hi {{name}} {{name}}", L"{\"name\":\"{{literal}}\"}");
        Check(rendered == L"Hi {{literal}} {{literal}}");
        Reject([] { (void)RenderPromptVariables(L"{{missing}}", L"{}"); });
        Reject([] { (void)RenderPromptVariables(L"{{name", L"{}"); });
        Reject([] { (void)RenderPromptVariables(L"{{name}}", L"{\"name\":1}"); });
        auto imported = ImportPrompt(
            LR"({"name":"legacy","model":"test-model","instructions":"hello","id":"asst_old","version":10,"tools":[{"type":"code_interpreter"}]})");
        Check(imported.id.empty() && imported.version == 0 && imported.name == L"legacy" &&
              SerializePrompt(imported).find(L"tools") == std::wstring::npos);
        auto folder = std::filesystem::absolute(L"artifacts/tests/build/catalog");
        std::filesystem::create_directories(folder);
        auto path = folder / (L"catalog-" + std::to_wstring(GetCurrentProcessId()) + L".json");
        std::filesystem::remove(path);
        PromptStore disk{path};
        Check(disk.Read().empty());
        auto saved = disk.Save(imported);
        Check(saved.version == 1 && saved.id.starts_with(L"local_"));
        auto second = disk.Save(saved);
        Check(second.version == 2 && disk.Read().size() == 2);
        Reject([&] { (void)disk.Save(saved); });
        PromptStore reopened{path};
        Check(reopened.Read()[1].name == imported.name);
        reopened.Remove(saved.id);
        Check(reopened.Read().empty());
        {
            std::ofstream corrupt(path);
            corrupt << R"({"schema":99,"revisions":[]})";
        }
        Reject([&] { (void)disk.Read(); });
        std::filesystem::remove(path);
        std::cout << "PASS: " << checks
                  << " Build contract and local storage checks; no network or "
                     "credentials\n";
        return 0;
    }
    catch (hresult_error const &error)
    {
        std::cerr << "FAIL: HRESULT " << std::hex << error.code().value << '\n';
    }
    catch (std::exception const &error)
    {
        std::cerr << "FAIL: " << error.what() << " after " << checks << " checks\n";
    }
    return 1;
}
