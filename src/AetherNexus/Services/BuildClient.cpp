// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: BuildClient.cpp
// Project: AetherNexus
#include "BuildClient.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <set>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace AetherNexus::Services
{
using namespace winrt;
using namespace Windows::Data::Json;
namespace
{
void Require(bool value)
{
    if (!value)
        throw std::invalid_argument("Invalid Build input");
}
[[noreturn]] void Invalid()
{
    throw ApiError{ApiErrorCode::InvalidResponse};
}
bool IdChar(wchar_t c)
{
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_' || c == L'-';
}
std::wstring Encode(std::wstring const &text)
{
    Require(text.size() <= 512);
    constexpr wchar_t hex[] = L"0123456789ABCDEF";
    std::wstring result;
    for (char byte : to_string(text))
    {
        auto c = static_cast<unsigned char>(byte);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.')
            result += static_cast<wchar_t>(c);
        else
        {
            result += L'%';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    return result;
}
void Set(JsonObject const &object, wchar_t const *key, std::wstring const &value)
{
    object.SetNamedValue(key, JsonValue::CreateStringValue(value));
}
std::wstring String(JsonObject const &object, wchar_t const *key)
{
    if (!object.HasKey(key) || object.GetNamedValue(key).ValueType() == JsonValueType::Null)
        return {};
    auto value = object.GetNamedString(key);
    if (value.size() > 65536)
        Invalid();
    return value.c_str();
}
JsonObject Object(std::wstring const &text)
{
    Require(text.size() <= 256 * 1024);
    JsonObject result;
    Require(JsonObject::TryParse(text, result));
    return result;
}
JsonObject Reply(BuildResponse const &response)
{
    if (response.status >= 400 && response.status < 500 && response.status != 401 && response.status != 403 &&
        response.status != 429)
        throw BuildHttpError{response.status};
    CheckStatus(response.status);
    if (response.body.size() > 16 * 1024 * 1024)
        Invalid();
    JsonObject object;
    if (!JsonObject::TryParse(to_hstring(response.body), object))
        Invalid();
    return object;
}
BuildRecord Record(JsonObject const &object)
{
    BuildRecord record;
    record.id = String(object, L"id");
    if (record.id.empty())
        Invalid();
    try
    {
        if (String(object, L"object") == L"model")
            Require(record.id.size() <= 256 && std::all_of(record.id.begin(), record.id.end(), [](wchar_t c) {
                        return IdChar(c) || c == L'.' || c == L':';
                    }));
        else
            ValidateBuildIdentifier(record.id);
    }
    catch (...)
    {
        Invalid();
    }
    for (auto field : {L"name", L"filename", L"model"})
        if (record.title.empty())
            record.title = String(object, field);
    if (record.title.empty())
        record.title = record.id;
    record.status = String(object, L"status");
    record.subtitle = record.id;
    auto purpose = String(object, L"purpose");
    if (!purpose.empty())
        record.subtitle += L" · " + purpose;
    for (auto field : {L"created_at", L"created"})
        if (!record.created && object.HasKey(field) && object.GetNamedValue(field).ValueType() != JsonValueType::Null)
        {
            auto value = object.GetNamedNumber(field);
            if (!std::isfinite(value) || value < 0 || value > 253402300799.0 || std::floor(value) != value)
                Invalid();
            record.created = static_cast<int64_t>(value);
        }
    record.json = object.Stringify().c_str();
    return record;
}
bool IsList(BuildRequest const &r)
{
    return r.item.empty() && r.resource != BuildResource::Responses && r.resource != BuildResource::FileContent &&
           r.resource != BuildResource::BatchCancel && r.resource != BuildResource::FineCancel &&
           r.resource != BuildResource::StoreSearch;
}
int64_t Now()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}
void Policy(BuildRequest const &r)
{
    auto policy = BuildPolicyAt(Now());
    if ((r.resource == BuildResource::Evals || r.resource == BuildResource::EvalRuns ||
         r.resource == BuildResource::EvalOutputs) &&
        !policy.evalsReadable)
        throw BuildHttpError{410};
    if (r.resource == BuildResource::FineJobs && r.method == BuildMethod::Post && !policy.fineTuningCreatable)
        throw BuildHttpError{410};
}
} // namespace
BuildPolicy BuildPolicyAt(int64_t time)
{
    using namespace std::chrono;
    auto evalEnd = sys_days{year{2026} / November / 30}.time_since_epoch();
    auto fineEnd = sys_days{year{2027} / January / 6}.time_since_epoch();
    return {time < duration_cast<seconds>(evalEnd).count(), time < duration_cast<seconds>(fineEnd).count()};
}
void ValidateBuildIdentifier(std::wstring_view text)
{
    Require(!text.empty() && text.size() <= 256 && std::all_of(text.begin(), text.end(), IdChar));
}
BuildWireRequest BuildWire(BuildRequest const &r)
{
    Require(r.body.size() <= 256 * 1024);
    if (!r.project.empty())
        ValidateBuildIdentifier(r.project);
    if (!r.item.empty())
        ValidateBuildIdentifier(r.item);
    if (!r.after.empty())
        ValidateBuildIdentifier(r.after);
    if (!r.parent.empty())
        ValidateBuildIdentifier(r.parent);
    if (!r.scope.empty())
        ValidateBuildIdentifier(r.scope);
    bool get = r.method == BuildMethod::Get, post = r.method == BuildMethod::Post,
         remove = r.method == BuildMethod::Delete;
    Require(get || post || remove);
    std::wstring path = L"/v1/";
    auto child = [&](wchar_t const *prefix, wchar_t const *suffix) {
        ValidateBuildIdentifier(r.parent);
        path += prefix + r.parent + suffix;
    };
    bool beta{};
    switch (r.resource)
    {
    case BuildResource::Models:
        Require(get && r.item.empty());
        path += L"models";
        break;
    case BuildResource::Chats:
        Require(get || (remove && !r.item.empty()));
        path += L"chat/completions";
        break;
    case BuildResource::ChatMessages:
        Require(get);
        child(L"chat/completions/", L"/messages");
        break;
    case BuildResource::Responses:
        Require(post ? r.item.empty() : !r.item.empty());
        path += L"responses";
        break;
    case BuildResource::ResponseItems:
        Require(get);
        child(L"responses/", L"/input_items");
        break;
    case BuildResource::Sessions:
        Require(get);
        path += L"agents/sessions";
        beta = true;
        break;
    case BuildResource::Traces:
        Require(get && r.item.empty());
        child(L"agents/sessions/", L"/traces");
        beta = true;
        break;
    case BuildResource::Batches:
        Require(get || (post && r.item.empty()));
        path += L"batches";
        break;
    case BuildResource::BatchCancel:
        Require(post && !r.item.empty());
        path += L"batches/" + r.item + L"/cancel";
        break;
    case BuildResource::Evals:
        Require(get);
        path += L"evals";
        break; // Existing Evals became read-only on 2026-10-31.
    case BuildResource::EvalRuns:
        Require(get);
        child(L"evals/", L"/runs");
        break;
    case BuildResource::EvalOutputs:
        Require(get);
        ValidateBuildIdentifier(r.scope);
        child(L"evals/", L"/runs/");
        path += r.scope + L"/output_items";
        break;
    case BuildResource::FineJobs:
        Require(get || (post && r.item.empty()));
        path += L"fine_tuning/jobs";
        break;
    case BuildResource::FineCancel:
        Require(post && !r.item.empty());
        path += L"fine_tuning/jobs/" + r.item + L"/cancel";
        break;
    case BuildResource::FineEvents:
        Require(get);
        child(L"fine_tuning/jobs/", L"/events");
        break;
    case BuildResource::FineCheckpoints:
        Require(get);
        child(L"fine_tuning/jobs/", L"/checkpoints");
        break;
    case BuildResource::Files:
        Require(get || (post && r.item.empty() && r.upload.has_value()) || (remove && !r.item.empty()));
        path += L"files";
        break;
    case BuildResource::FileContent:
        Require(get && !r.item.empty() && !r.download.empty());
        path += L"files/" + r.item + L"/content";
        break;
    case BuildResource::Stores:
        Require(get || post || (remove && !r.item.empty()));
        path += L"vector_stores";
        break;
    case BuildResource::StoreFiles:
        Require(get || (post && r.item.empty()) || (remove && !r.item.empty()));
        child(L"vector_stores/", L"/files");
        break;
    case BuildResource::StoreSearch:
        Require(post && r.item.empty());
        child(L"vector_stores/", L"/search");
        break;
    default:
        Require(false);
    }
    if (!r.item.empty() && r.resource != BuildResource::BatchCancel && r.resource != BuildResource::FineCancel &&
        r.resource != BuildResource::FileContent)
        path += L"/" + r.item;
    if (get && IsList(r) && r.resource != BuildResource::Models)
    {
        path += L"?limit=" + std::wstring{r.resource == BuildResource::Traces ? L"5" : L"20"};
        // Fine-tuning jobs/events/checkpoints use their documented default
        // ordering.
        if (r.resource != BuildResource::FineJobs && r.resource != BuildResource::FineEvents &&
            r.resource != BuildResource::FineCheckpoints)
            path += r.ascending ? L"&order=asc" : L"&order=desc";
        if (!r.after.empty())
            path += L"&after=" + r.after;
        if (!r.filter.empty())
        {
            Require(r.resource == BuildResource::Files || r.resource == BuildResource::Chats);
            path += r.resource == BuildResource::Files ? L"&purpose=" : L"&model=";
            path += Encode(r.filter);
        }
    }
    else
        Require(r.after.empty() && r.filter.empty());
    Require(get ? r.body.empty() && !r.upload : remove ? r.body.empty() && !r.upload : true);
    Require(!r.upload || r.resource == BuildResource::Files);
    Require(r.download.empty() || r.resource == BuildResource::FileContent);
    if (post && !r.body.empty())
        (void)Object(to_hstring(r.body).c_str());
    if (post && r.resource != BuildResource::BatchCancel && r.resource != BuildResource::FineCancel && !r.upload)
        Require(!r.body.empty());
    std::wstring headers = L"Accept: application/json\r\n";
    if (!r.upload)
        headers += L"Content-Type: application/json\r\n";
    if (beta)
        headers += L"OpenAI-Beta: agents=v1\r\n";
    if (!r.project.empty())
        headers += L"OpenAI-Project: " + r.project + L"\r\n";
    return {std::move(path), get ? L"GET" : post ? L"POST" : L"DELETE", std::move(headers), r.body};
}
std::string BatchBody(std::wstring const &file, std::wstring const &endpoint)
{
    ValidateBuildIdentifier(file);
    Require(endpoint == L"/v1/responses" || endpoint == L"/v1/chat/completions" || endpoint == L"/v1/embeddings" ||
            endpoint == L"/v1/moderations" || endpoint == L"/v1/images/generations" || endpoint == L"/v1/images/edits");
    JsonObject object;
    Set(object, L"input_file_id", file);
    Set(object, L"endpoint", endpoint);
    Set(object, L"completion_window", L"24h");
    return to_string(object.Stringify());
}
std::string FineTuneBody(std::wstring const &model, std::wstring const &training, std::wstring const &validation,
                         std::wstring const &suffix, std::wstring const &method, std::wstring const &hyperparameters)
{
    Require(!model.empty() && model.size() <= 256 && suffix.size() <= 64);
    ValidateBuildIdentifier(training);
    if (!validation.empty())
        ValidateBuildIdentifier(validation);
    Require(method == L"supervised" || method == L"dpo");
    auto parameters = Object(hyperparameters.empty() ? L"{}" : hyperparameters);
    JsonObject object;
    Set(object, L"model", model);
    Set(object, L"training_file", training);
    if (!validation.empty())
    {
        Require(training != validation);
        Set(object, L"validation_file", validation);
    }
    if (!suffix.empty())
        Set(object, L"suffix", suffix);
    JsonObject selected, options;
    Set(selected, L"type", method);
    options.SetNamedValue(L"hyperparameters", parameters);
    selected.SetNamedValue(method, options);
    object.SetNamedValue(L"method", selected);
    return to_string(object.Stringify());
}
std::string StoreBody(std::wstring const &name, std::wstring const &fileIds, std::optional<int64_t> expiryDays)
{
    Require(!name.empty() && name.size() <= 256 && fileIds.size() <= 65536);
    JsonObject object;
    Set(object, L"name", name);
    JsonArray files;
    size_t start{};
    std::set<std::wstring> seen;
    while (start < fileIds.size())
    {
        auto end = fileIds.find_first_of(L",\r\n \t", start);
        auto id = fileIds.substr(start, end == std::wstring::npos ? end : end - start);
        if (!id.empty())
        {
            ValidateBuildIdentifier(id);
            Require(seen.insert(id).second && seen.size() <= 500);
            files.Append(JsonValue::CreateStringValue(id));
        }
        if (end == std::wstring::npos)
            break;
        start = end + 1;
    }
    if (files.Size())
        object.SetNamedValue(L"file_ids", files);
    if (expiryDays)
    {
        Require(*expiryDays >= 1 && *expiryDays <= 365);
        JsonObject expiry;
        Set(expiry, L"anchor", L"last_active_at");
        expiry.SetNamedValue(L"days", JsonValue::CreateNumberValue(static_cast<double>(*expiryDays)));
        object.SetNamedValue(L"expires_after", expiry);
    }
    return to_string(object.Stringify());
}
std::string StoreFileBody(std::wstring const &file)
{
    ValidateBuildIdentifier(file);
    JsonObject object;
    Set(object, L"file_id", file);
    return to_string(object.Stringify());
}
std::string StoreSearchBody(std::wstring const &query)
{
    Require(!query.empty() && query.size() <= 8192);
    JsonObject object;
    Set(object, L"query", query);
    object.SetNamedValue(L"max_num_results", JsonValue::CreateNumberValue(10));
    return to_string(object.Stringify());
}
std::string PromptTestBody(std::wstring const &model, std::wstring const &instructions, std::wstring const &input,
                           std::wstring const &options)
{
    Require(!model.empty() && model.size() <= 256 && instructions.size() <= 65536 && !input.empty() &&
            input.size() <= 65536);
    auto object = Object(options.empty() ? L"{}" : options);
    for (auto const &entry : object)
        Require(entry.Key() == L"temperature" || entry.Key() == L"top_p" || entry.Key() == L"reasoning" ||
                entry.Key() == L"text" || entry.Key() == L"max_output_tokens");
    Set(object, L"model", model);
    Set(object, L"instructions", instructions);
    Set(object, L"input", input);
    object.SetNamedValue(L"stream", JsonValue::CreateBooleanValue(false));
    object.SetNamedValue(L"store", JsonValue::CreateBooleanValue(false));
    if (!object.HasKey(L"max_output_tokens"))
        object.SetNamedValue(L"max_output_tokens", JsonValue::CreateNumberValue(1024));
    return to_string(object.Stringify());
}
std::wstring PrettyBuildJson(std::wstring const &text)
{
    std::wstring result;
    result.reserve(text.size() + text.size() / 4);
    unsigned depth{};
    bool quoted{}, escaped{};
    auto newline = [&] {
        result += L'\n';
        result.append(static_cast<size_t>(std::min(depth, 32u)) * 2, L' ');
    };
    for (wchar_t c : text)
    {
        if (quoted)
        {
            result += c;
            if (escaped)
                escaped = false;
            else if (c == L'\\')
                escaped = true;
            else if (c == L'"')
                quoted = false;
            continue;
        }
        if (c == L'"')
        {
            quoted = true;
            result += c;
        }
        else if (c == L'{' || c == L'[')
        {
            result += c;
            ++depth;
            newline();
        }
        else if (c == L'}' || c == L']')
        {
            if (depth)
                --depth;
            newline();
            result += c;
        }
        else if (c == L',')
        {
            result += c;
            newline();
        }
        else if (c == L':')
            result += L": ";
        else if (c != L' ' && c != L'\r' && c != L'\n' && c != L'\t')
            result += c;
    }
    return result;
}
BuildList BuildClient::List(BuildRequest const &request, std::stop_token stop)
{
    Require(m_transport && request.method == BuildMethod::Get && IsList(request));
    (void)BuildWire(request);
    Policy(request);
    CheckCancelled(stop);
    auto object = Reply(m_transport->Send(request, stop));
    CheckCancelled(stop);
    if (!object.HasKey(L"data"))
        Invalid();
    auto data = object.GetNamedArray(L"data");
    if (data.Size() > (request.resource == BuildResource::Models ? 5000u : 100u))
        Invalid();
    BuildList result;
    std::set<std::wstring> seen;
    for (auto const &item : data)
    {
        auto record = Record(item.GetObject());
        if (!seen.insert(record.id).second)
            Invalid();
        result.records.push_back(std::move(record));
    }
    result.hasMore = object.GetNamedBoolean(L"has_more", false);
    if (result.hasMore)
    {
        result.after = String(object, L"last_id");
        if (result.after.empty() && !result.records.empty())
            result.after = result.records.back().id;
        if (result.after.empty() || result.after == request.after)
            Invalid();
        try
        {
            ValidateBuildIdentifier(result.after);
        }
        catch (...)
        {
            Invalid();
        }
    }
    return result;
}
BuildRecord BuildClient::Retrieve(BuildRequest const &request, std::stop_token stop)
{
    Require(m_transport && request.method == BuildMethod::Get && !request.item.empty());
    (void)BuildWire(request);
    Policy(request);
    CheckCancelled(stop);
    auto result = Record(Reply(m_transport->Send(request, stop)));
    CheckCancelled(stop);
    if (result.id != request.item)
        Invalid();
    return result;
}
BuildRecord BuildClient::Change(BuildRequest const &request, std::stop_token stop)
{
    Require(m_transport && request.method != BuildMethod::Get);
    (void)BuildWire(request);
    Policy(request);
    CheckCancelled(stop);
    auto object = Reply(m_transport->Send(request, stop));
    CheckCancelled(stop);
    if (request.method == BuildMethod::Delete)
    {
        if (!object.GetNamedBoolean(L"deleted", false) || String(object, L"id") != request.item)
            Invalid();
    }
    // Search results are a query response, not a persistent resource with its own
    // ID.
    if (request.resource == BuildResource::StoreSearch)
    {
        BuildRecord result;
        result.id = request.parent;
        result.title = L"向量庫搜尋結果";
        result.subtitle = request.parent;
        result.json = object.Stringify().c_str();
        return result;
    }
    auto result = Record(object);
    if (!request.item.empty() && result.id != request.item)
        Invalid();
    return result;
}
uint64_t BuildClient::Download(BuildRequest const &request, std::stop_token stop)
{
    Require(m_transport && request.resource == BuildResource::FileContent && request.method == BuildMethod::Get);
    (void)BuildWire(request);
    CheckCancelled(stop);
    auto result = m_transport->Send(request, stop);
    if (result.status >= 400 && result.status < 500 && result.status != 401 && result.status != 403 && result.status != 429)
        throw BuildHttpError{result.status};
    CheckStatus(result.status);
    CheckCancelled(stop);
    return result.downloadedBytes;
}
std::vector<TraceSpan> ParseTraceSpans(std::wstring const &json)
{
    JsonObject trace;
    if (json.size() > 16 * 1024 * 1024 || !JsonObject::TryParse(json, trace))
        Invalid();
    std::vector<TraceSpan> result;
    auto resources = trace.GetNamedObject(L"otlp", JsonObject{}).GetNamedArray(L"resourceSpans", JsonArray{});
    for (auto const &resource : resources)
        for (auto const &scope : resource.GetObject().GetNamedArray(L"scopeSpans", JsonArray{}))
            for (auto const &value : scope.GetObject().GetNamedArray(L"spans", JsonArray{}))
            {
                if (result.size() == 1000)
                    return result;
                auto span = value.GetObject();
                TraceSpan row;
                row.id = String(span, L"spanId");
                row.parent = String(span, L"parentSpanId");
                row.name = String(span, L"name");
                row.json = span.Stringify().c_str();
                if (span.HasKey(L"status"))
                    row.status = span.GetNamedObject(L"status").Stringify().c_str();
                auto number = [&](wchar_t const *key) -> std::optional<uint64_t> {
                    auto text = to_string(String(span, key));
                    if (text.empty())
                        return {};
                    uint64_t n{};
                    auto parsed = std::from_chars(text.data(), text.data() + text.size(), n);
                    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
                        return {};
                    return n;
                };
                auto start = number(L"startTimeUnixNano"), end = number(L"endTimeUnixNano");
                if (start && end && *end >= *start)
                    row.milliseconds = static_cast<double>(*end - *start) / 1000000.0;
                result.push_back(std::move(row));
            }
    return result;
}
} // namespace AetherNexus::Services
