// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ManagementClient.cpp
// Project: AetherNexus
#include "ManagementClient.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <set>

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
                throw std::invalid_argument("Invalid management input");
        }
        bool Contains(std::vector<std::wstring> const &values, std::wstring const &value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }
        void Identifier(std::wstring const &id)
        {
            Require(!id.empty() && id.size() <= 256 && std::all_of(id.begin(), id.end(), [](wchar_t c) {
                return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_' ||
                       c == L'-';
            }));
        }
        std::wstring Encode(std::wstring const &value)
        {
            Require(value.size() <= 4096);
            constexpr wchar_t hex[] = L"0123456789ABCDEF";
            std::wstring result;
            for (char byte : to_string(value))
            {
                auto c = static_cast<unsigned char>(byte);
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                    c == '-' || c == '.')
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
        void Set(JsonObject const &obj, wchar_t const *key, std::wstring const &value)
        {
            obj.SetNamedValue(key, JsonValue::CreateStringValue(value));
        }
        std::wstring String(JsonObject const &obj, wchar_t const *key)
        {
            if (!obj.HasKey(key) || obj.GetNamedValue(key).ValueType() == JsonValueType::Null)
                return {};
            auto text = obj.GetNamedString(key);
            if (text.size() > 4096)
                Invalid();
            return text.c_str();
        }
        int64_t Integer(double value)
        {
            if (!std::isfinite(value) || value < 0 || value > 9007199254740991.0 || std::floor(value) != value)
                Invalid();
            return static_cast<int64_t>(value);
        }
        std::optional<int64_t> Date(JsonObject const &obj, wchar_t const *key)
        {
            if (!obj.HasKey(key) || obj.GetNamedValue(key).ValueType() == JsonValueType::Null)
                return {};
            auto value = Integer(obj.GetNamedNumber(key));
            if (value > 253402300799LL)
                Invalid();
            return value;
        }
        JsonObject Parse(ApiResponse const &response)
        {
            if (response.status >= 400 && response.status < 500 && response.status != 401 &&
                response.status != 403 && response.status != 429)
                throw ManagementHttpError(response.status, response.requestId, response.diagnostics);
            CheckStatus(response.status);
            JsonObject object;
            if (response.body.size() > 8 * 1024 * 1024 || !JsonObject::TryParse(response.body, object))
                Invalid();
            return object;
        }
        ManagementRecord Record(JsonObject const &obj, ManagementResource type)
        {
            auto objectType = String(obj, L"object");
            auto expected = type == ManagementResource::Projects || type == ManagementResource::Project ||
                                    type == ManagementResource::Archive
                                ? L"organization.project"
                            : type == ManagementResource::ProjectKeys || type == ManagementResource::ProjectKey
                                ? L"organization.project.api_key"
                            : type == ManagementResource::AdminKeys || type == ManagementResource::AdminKey
                                ? L"organization.admin_api_key"
                            : type == ManagementResource::Members || type == ManagementResource::Member
                                ? L"organization.project.user"
                                : L"project.rate_limit";
            // Allow explicit project objects in a mixed organization directory.
            bool projectInDirectory = type == ManagementResource::AdminKeys && objectType == L"organization.project.api_key";
            if (objectType != expected && !projectInDirectory)
                Invalid();
            ManagementRecord record;
            record.id = String(obj, L"id");
            if (record.id.empty())
                Invalid();
            record.name = String(obj, L"name");
            record.created =
                Date(obj, type == ManagementResource::Members || type == ManagementResource::Member ? L"added_at"
                                                                                                    : L"created_at");
            record.expires = Date(obj, L"expires_at");
            record.lastUsed = Date(obj, L"last_used_at");
            if (type == ManagementResource::Projects || type == ManagementResource::Project ||
                type == ManagementResource::Archive)
            {
                record.status = String(obj, L"status");
                if (record.status != L"active" && record.status != L"archived")
                    Invalid();
            }
            if (type == ManagementResource::Members || type == ManagementResource::Member)
            {
                record.status = String(obj, L"role");
                record.owner = String(obj, L"email");
            }
            if (type == ManagementResource::ProjectKeys || type == ManagementResource::ProjectKey ||
                type == ManagementResource::AdminKeys || type == ManagementResource::AdminKey)
            {
                // Never retain a returned secret or a raw server object, even if a future API adds fields.
                auto redacted = String(obj, L"redacted_value");
                // The documented object type is the discriminator. A redacted value is display-only;
                // neither a missing nor a misleading prefix may change the API resource's type.
                record.keyKind = objectType == L"organization.admin_api_key" ? ManagementKeyKind::Admin
                                                                            : ManagementKeyKind::Api;
                record.keyHint =
                    redacted.empty() ? L"—" : L"…" + redacted.substr(redacted.size() > 4 ? redacted.size() - 4 : 0);
                if (obj.HasKey(L"owner") && obj.GetNamedValue(L"owner").ValueType() == JsonValueType::Object)
                {
                    auto owner = obj.GetNamedObject(L"owner");
                    auto kind = String(owner, L"type");
                    if ((kind == L"user" || kind == L"service_account") && owner.HasKey(kind) &&
                        owner.GetNamedValue(kind).ValueType() == JsonValueType::Object)
                        owner = owner.GetNamedObject(kind);
                    record.owner = String(owner, L"name");
                    if (record.owner.empty())
                        record.owner = String(owner, L"id");
                }
                if (obj.HasKey(L"scopes") && obj.GetNamedValue(L"scopes").ValueType() == JsonValueType::Array)
                {
                    auto scopes = obj.GetNamedArray(L"scopes");
                    if (scopes.Size() > 256)
                        Invalid();
                    record.scopesKnown = true;
                    for (auto const &scope : scopes)
                        record.scopes.emplace_back(scope.GetString());
                }
            }
            if (type == ManagementResource::RateLimits || type == ManagementResource::RateLimit)
            {
                record.name = String(obj, L"model");
                for (auto const &key : RateLimitFields())
                    if (obj.HasKey(key) && obj.GetNamedValue(key).ValueType() != JsonValueType::Null)
                        record.limits.emplace(key, Integer(obj.GetNamedNumber(key)));
            }
            return record;
        }
        std::wstring Cell(std::wstring value, bool protectFormula = true)
        {
            auto first = value.find_first_not_of(L" \t\r\n");
            if (protectFormula && first != std::wstring::npos &&
                std::wstring_view{L"=+-@"}.find(value[first]) != std::wstring_view::npos)
                value.insert(value.begin(), L'\'');
            std::wstring escaped = L"\"";
            for (auto c : value)
            {
                if (c == L'\"')
                    escaped += L'\"';
                escaped += c;
            }
            return escaped + L"\"";
        }
    } // namespace
    std::vector<std::wstring> UsageCategories()
    {
        return {L"completions",
                L"costs",
                L"embeddings",
                L"images",
                L"audio_speeches",
                L"audio_transcriptions",
                L"moderations",
                L"vector_stores",
                L"code_interpreter_sessions",
                L"file_search_calls",
                L"web_search_calls"};
    }
    std::vector<std::wstring> UsageGroups(std::wstring const &category)
    {
        Require(Contains(UsageCategories(), category));
        if (category == L"costs")
            return {L"", L"project_id", L"line_item", L"api_key_id", L"api_source"};
        if (category == L"vector_stores" || category == L"code_interpreter_sessions")
            return {L"", L"project_id"};
        if (category == L"file_search_calls")
            return {L"", L"project_id", L"api_key_id", L"user_id", L"vector_store_id"};
        return {L"", L"project_id", L"model", L"api_key_id", L"user_id"};
    }
    std::vector<std::wstring> UsageMetrics(std::wstring const &category)
    {
        Require(Contains(UsageCategories(), category));
        if (category == L"costs")
            return {L"amount_usd"};
        if (category == L"images")
            return {L"images", L"num_model_requests"};
        if (category == L"audio_speeches")
            return {L"characters", L"num_model_requests"};
        if (category == L"audio_transcriptions")
            return {L"seconds", L"num_model_requests"};
        if (category == L"vector_stores")
            return {L"usage_bytes"};
        if (category == L"code_interpreter_sessions")
            return {L"num_sessions"};
        if (category == L"file_search_calls")
            return {L"num_requests"};
        if (category == L"web_search_calls")
            return {L"num_requests", L"num_model_requests"};
        if (category == L"completions")
            return {L"input_tokens", L"output_tokens", L"input_cached_tokens", L"num_model_requests"};
        return {L"input_tokens", L"num_model_requests"};
    }
    std::vector<std::wstring> RateLimitFields()
    {
        return {L"max_requests_per_1_minute", L"max_tokens_per_1_minute",          L"max_requests_per_1_day",
                L"max_images_per_1_minute",   L"max_audio_megabytes_per_1_minute", L"batch_1_day_max_input_tokens"};
    }
    wchar_t const *ManagementVerb(ManagementMethod method)
    {
        switch (method)
        {
        case ManagementMethod::Get:
            return L"GET";
        case ManagementMethod::Post:
            return L"POST";
        case ManagementMethod::Delete:
            return L"DELETE";
        }
        throw std::invalid_argument("Invalid management method");
    }
    std::wstring ManagementPath(ManagementRequest const &r)
    {
        (void)ManagementVerb(r.method);
        Require(r.body.size() <= 65536 && (r.method == ManagementMethod::Post || r.body.empty()));
        std::wstring path = L"/v1/organization/";
        bool list = false;
        switch (r.resource)
        {
        case ManagementResource::Projects:
            Require(r.method != ManagementMethod::Delete);
            path += L"projects";
            list = r.method == ManagementMethod::Get;
            break;
        case ManagementResource::Project:
            Identifier(r.project);
            Require(r.method == ManagementMethod::Post);
            path += L"projects/" + r.project;
            break;
        case ManagementResource::Archive:
            Identifier(r.project);
            Require(r.method == ManagementMethod::Post);
            path += L"projects/" + r.project + L"/archive";
            break;
        case ManagementResource::AdminKeys:
            Require(r.method == ManagementMethod::Get);
            path += L"admin_api_keys";
            list = true;
            break;
        case ManagementResource::AdminKey:
            Identifier(r.item);
            Require(r.method == ManagementMethod::Delete || r.method == ManagementMethod::Get);
            if (r.keyKind != ManagementKeyKind::Admin) throw ManagementKeyTypeError{};
            Require(r.project.empty());
            path += L"admin_api_keys/" + r.item;
            break;
        case ManagementResource::Usage:
            Require(r.method == ManagementMethod::Get && Contains(UsageCategories(), r.category) &&
                    Contains(UsageGroups(r.category), r.group));
            Require(r.start >= 0 && r.start % 86400 == 0 && r.end > r.start && r.end - r.start <= 366LL * 86400);
            path += r.category == L"costs" ? L"costs" : L"usage/" + r.category;
            path += L"?start_time=" + std::to_wstring(r.start) + L"&end_time=" + std::to_wstring(r.end) +
                    L"&bucket_width=1d&limit=31";
            if (!r.project.empty())
            {
                Identifier(r.project);
                path += L"&project_ids%5B%5D=" + Encode(r.project);
            }
            if (!r.group.empty())
                path += L"&group_by%5B%5D=" + Encode(r.group);
            if (!r.page.empty())
                path += L"&page=" + Encode(r.page);
            return path;
        case ManagementResource::ProjectKeys:
        case ManagementResource::ProjectKey:
        case ManagementResource::Members:
        case ManagementResource::Member:
        case ManagementResource::RateLimits:
        case ManagementResource::RateLimit:
            Identifier(r.project);
            path += L"projects/" + r.project;
            if (r.resource == ManagementResource::ProjectKeys || r.resource == ManagementResource::ProjectKey)
                path += L"/api_keys";
            else if (r.resource == ManagementResource::Members || r.resource == ManagementResource::Member)
                path += L"/users";
            else
                path += L"/rate_limits";
            list = r.resource == ManagementResource::ProjectKeys || r.resource == ManagementResource::Members ||
                   r.resource == ManagementResource::RateLimits;
            if (list)
                Require(r.method == ManagementMethod::Get ||
                        (r.resource == ManagementResource::Members && r.method == ManagementMethod::Post));
            else
            {
                Identifier(r.item);
                path += L"/" + r.item;
                Require(r.method != ManagementMethod::Get &&
                        (r.resource != ManagementResource::RateLimit || r.method == ManagementMethod::Post));
            }
            list = list && r.method == ManagementMethod::Get;
            break;
        default:
            throw std::invalid_argument("Invalid management resource");
        }
        if (list)
        {
            path += L"?limit=100";
            if (r.resource == ManagementResource::Projects)
                path += L"&include_archived=true";
            if (r.resource == ManagementResource::ProjectKeys)
                path += L"&owner_project_access=any";
            if (!r.page.empty())
                path += L"&after=" + Encode(r.page);
        }
        else
            Require(r.page.empty());
        return path;
    }
    std::wstring ManagementRequestId(std::wstring_view value)
    {
        if (value.empty() || value.size() > 128 || value.starts_with(L"sk-") ||
            !std::all_of(value.begin(), value.end(), [](wchar_t c) {
                return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
                       (c >= L'0' && c <= L'9') || c == L'_' || c == L'-';
            }))
            return {};
        return std::wstring{value};
    }
    ManagementWireRequest BuildManagementWireRequest(ManagementRequest const &request)
    {
        return {ManagementPath(request), ManagementVerb(request.method), request.body};
    }
    ManagementRequest RevokeAdminKeyRequest(ManagementRecord const &key)
    {
        if (key.keyKind != ManagementKeyKind::Admin) throw ManagementKeyTypeError{};
        ManagementRequest request;
        request.resource = ManagementResource::AdminKey;
        request.method = ManagementMethod::Delete;
        request.item = key.id;
        request.keyKind = key.keyKind;
        (void)ManagementPath(request);
        return request;
    }
    std::wstring ProjectNameBody(std::wstring const &name)
    {
        Require(!name.empty() && name.size() <= 256 && name.find_first_not_of(L" \t\r\n") != std::wstring::npos &&
                std::none_of(name.begin(), name.end(), [](wchar_t c) { return c < 0x20; }));
        JsonObject body;
        Set(body, L"name", name);
        return body.Stringify().c_str();
    }
    std::wstring MemberBody(std::wstring const &user, std::wstring const &role)
    {
        Require(role == L"owner" || role == L"member");
        JsonObject body;
        Set(body, L"role", role);
        if (!user.empty())
        {
            Identifier(user);
            Set(body, L"user_id", user);
        }
        return body.Stringify().c_str();
    }
    std::wstring KeyPolicyBody(std::optional<std::vector<std::wstring>> const &scopes, std::optional<int64_t> expires)
    {
        Require(scopes.has_value() || expires.has_value());
        JsonObject body;
        if (expires)
        {
            Require(*expires >= 1 && *expires <= 31536000);
            body.SetNamedValue(L"expires_in_seconds", JsonValue::CreateNumberValue(static_cast<double>(*expires)));
        }
        if (scopes)
        {
            Require(scopes->size() <= 256);
            JsonArray values;
            std::set<std::wstring> seen;
            for (auto const &scope : *scopes)
            {
                Require(!scope.empty() && scope.size() <= 128 && std::all_of(scope.begin(), scope.end(), [](wchar_t c) {
                    return (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'.' || c == L'_' ||
                           c == L'-' || c == L':';
                }));
                if (seen.insert(scope).second)
                    values.Append(JsonValue::CreateStringValue(scope));
            }
            body.SetNamedValue(L"scopes", values);
        }
        return body.Stringify().c_str();
    }
    std::wstring RateLimitBody(std::map<std::wstring, int64_t> const &values)
    {
        Require(!values.empty());
        JsonObject body;
        for (auto const &[key, value] : values)
        {
            Require(Contains(RateLimitFields(), key) && value >= 0 && value <= 9007199254740991LL);
            body.SetNamedValue(key, JsonValue::CreateNumberValue(static_cast<double>(value)));
        }
        return body.Stringify().c_str();
    }
    ManagementClient::ManagementClient(std::shared_ptr<IManagementTransport> transport)
        : m_transport(std::move(transport))
    {
        Require(m_transport != nullptr);
    }
    std::vector<ManagementRecord> ManagementClient::List(ManagementResource resource, std::wstring const &project,
                                                         std::stop_token stop)
    {
        Require(resource == ManagementResource::Projects || resource == ManagementResource::ProjectKeys ||
                resource == ManagementResource::AdminKeys || resource == ManagementResource::Members ||
                resource == ManagementResource::RateLimits);
        ManagementRequest request;
        request.resource = resource;
        request.project = project;
        std::vector<ManagementRecord> records;
        std::set<std::wstring> ids, cursors;
        for (unsigned page = 0; page < 64; ++page)
        {
            CheckCancelled(stop);
            (void)ManagementPath(request);
            auto object = Parse(m_transport->Send(request, stop));
            if (object.GetNamedString(L"object") != L"list")
                Invalid();
            auto data = object.GetNamedArray(L"data");
            if (data.Size() > 1000)
                Invalid();
            for (auto const &item : data)
            {
                auto record = Record(item.GetObject(), resource);
                if (!ids.insert(record.id).second)
                    Invalid();
                records.push_back(std::move(record));
            }
            CheckCancelled(stop);
            if (!object.GetNamedBoolean(L"has_more"))
                return records;
            if (data.Size() == 0 || records.empty())
                Invalid();
            auto next = String(object, L"last_id");
            if (next.empty())
                next = records.back().id;
            if (next != records.back().id || !cursors.insert(next).second)
                Invalid();
            request.page = next;
        }
        Invalid();
    }
    ManagementKeyDirectory ManagementClient::OrganizationKeys(std::vector<ManagementRecord> const &projects,
                                                              std::stop_token stop)
    {
        ManagementKeyDirectory directory{List(ManagementResource::AdminKeys, {}, stop)};
        if (directory.records.empty())
            return directory;
        // The list is described as including organization and project keys. Match project IDs using
        // their authoritative endpoint rather than guessing from masks, names, owners, or ID prefixes.
        std::set<std::wstring> directoryIds;
        for (auto const &record : directory.records)
            directoryIds.insert(record.id);
        std::set<std::wstring> projectKeyIds;
        constexpr size_t maximumProjectLookups = 128;
        size_t lookups{};
        for (auto const &project : projects)
        {
            CheckCancelled(stop);
            if (lookups++ == maximumProjectLookups)
            {
                directory.projectLookupIncomplete = true;
                break;
            }
            try
            {
                for (auto const &key : List(ManagementResource::ProjectKeys, project.id, stop))
                    if (directoryIds.contains(key.id))
                        projectKeyIds.insert(key.id);
            }
            catch (ApiError const &error)
            {
                if (error.code != ApiErrorCode::Forbidden)
                    throw;
                directory.projectLookupIncomplete = true;
            }
            catch (ManagementHttpError const &error)
            {
                if (error.status != 404)
                    throw;
                directory.projectLookupIncomplete = true;
            }
        }
        CheckCancelled(stop);
        for (auto &record : directory.records)
            if (projectKeyIds.contains(record.id))
                record.keyKind = ManagementKeyKind::Api;
        return directory;
    }
    ManagementRecord ManagementClient::Change(ManagementRequest const &request, std::stop_token stop)
    {
        Require(request.method != ManagementMethod::Get);
        (void)ManagementPath(request);
        CheckCancelled(stop);
        if (request.resource == ManagementResource::AdminKey)
        {
            auto verification = request;
            verification.method = ManagementMethod::Get;
            ManagementRecord current;
            try
            {
                current = Record(Parse(m_transport->Send(verification, stop)), ManagementResource::AdminKey);
            }
            catch (ManagementHttpError &error)
            {
                error.operation = ManagementOperation::AdminVerification;
                throw;
            }
            CheckCancelled(stop);
            if (current.id != request.item) Invalid();
            if (current.keyKind != ManagementKeyKind::Admin) throw ManagementKeyTypeError{};
        }
        JsonObject result;
        try
        {
            result = Parse(m_transport->Send(request, stop));
        }
        catch (ManagementHttpError &error)
        {
            if (request.method == ManagementMethod::Delete)
                error.operation = request.resource == ManagementResource::AdminKey ? ManagementOperation::AdminRevocation
                    : request.resource == ManagementResource::ProjectKey ? ManagementOperation::ProjectRevocation
                                                                         : ManagementOperation::Other;
            throw;
        }
        CheckCancelled(stop);
        if (request.method == ManagementMethod::Delete)
        {
            if (request.resource == ManagementResource::AdminKey &&
                String(result, L"object") != L"organization.admin_api_key.deleted")
                Invalid();
            if (request.resource == ManagementResource::ProjectKey &&
                String(result, L"object") != L"organization.project.api_key.deleted")
                Invalid();
            if (!result.GetNamedBoolean(L"deleted") || String(result, L"id") != request.item)
                Invalid();
            ManagementRecord deleted;
            deleted.id = request.item;
            return deleted;
        }
        auto record = Record(result, request.resource);
        auto expected =
            request.resource == ManagementResource::Project || request.resource == ManagementResource::Archive
                ? request.project
                : request.item;
        if (!expected.empty() && record.id != expected)
            Invalid();
        return record;
    }
    UsageReport ManagementClient::Usage(UsageQuery const &query, std::stop_token stop)
    {
        ManagementRequest request;
        request.resource = ManagementResource::Usage;
        request.start = query.start;
        request.end = query.end;
        request.project = query.project;
        request.category = query.category;
        request.group = query.group;
        (void)ManagementPath(request);
        UsageReport report;
        report.query = query;
        std::set<int64_t> buckets;
        std::set<std::wstring> cursors;
        auto metrics = UsageMetrics(query.category);
        for (unsigned page = 0; page < 64; ++page)
        {
            CheckCancelled(stop);
            auto object = Parse(m_transport->Send(request, stop));
            if (object.GetNamedString(L"object") != L"page")
                Invalid();
            for (auto const &entry : object.GetNamedArray(L"data"))
            {
                auto bucket = entry.GetObject();
                int64_t start = Integer(bucket.GetNamedNumber(L"start_time")),
                        end = Integer(bucket.GetNamedNumber(L"end_time"));
                if (bucket.GetNamedString(L"object") != L"bucket" || start < query.start || start >= query.end ||
                    start % 86400 || end <= start || end - start > 86400 || !buckets.insert(start).second)
                    Invalid();
                auto results = bucket.GetNamedArray(L"results");
                if (results.Size() == 0)
                    report.emptyBuckets.push_back(start);
                std::set<std::wstring> groups;
                for (auto const &result : results)
                {
                    UsageRow row;
                    row.start = start;
                    row.end = std::min(end, query.end);
                    auto item = result.GetObject();
                    row.group = query.group.empty() ? L"" : String(item, query.group.c_str());
                    if (!groups.insert(row.group).second)
                        Invalid();
                    for (auto const &metric : metrics)
                    {
                        if (metric == L"amount_usd")
                        {
                            auto amount = item.GetNamedObject(L"amount");
                            if (amount.GetNamedString(L"currency") != L"usd")
                                Invalid();
                            auto value = amount.GetNamedNumber(L"value");
                            if (!std::isfinite(value) || std::abs(value) > 1e12)
                                Invalid();
                            row.metrics.emplace(metric, value);
                        }
                        else if (item.HasKey(metric) && item.GetNamedValue(metric).ValueType() != JsonValueType::Null)
                            row.metrics.emplace(metric, static_cast<double>(Integer(item.GetNamedNumber(metric))));
                    }
                    if (row.metrics.empty() || report.rows.size() >= 100000)
                        Invalid();
                    report.rows.push_back(std::move(row));
                }
            }
            CheckCancelled(stop);
            if (!object.GetNamedBoolean(L"has_more"))
                return report;
            auto next = String(object, L"next_page");
            if (next.empty() || !cursors.insert(next).second)
                Invalid();
            request.page = next;
        }
        Invalid();
    }
    std::map<int64_t, std::optional<double>> UsageReport::Daily(std::wstring const &metric) const
    {
        Require(Contains(UsageMetrics(query.category), metric));
        std::map<int64_t, std::optional<double>> values;
        // A completed paginated query has zero activity on omitted days. A missing metric within a result is unknown.
        for (int64_t day = query.start; day < query.end; day += 86400)
            values.emplace(day, 0);
        for (auto start : emptyBuckets)
            values[start] = 0;
        std::set<int64_t> visited, missing;
        for (auto const &row : rows)
        {
            auto found = row.metrics.find(metric);
            if (found == row.metrics.end())
            {
                missing.insert(row.start);
                values[row.start].reset();
                continue;
            }
            if (missing.contains(row.start))
                continue;
            if (visited.insert(row.start).second)
                values[row.start] = 0;
            auto &value = values[row.start];
            *value += found->second;
            if (!std::isfinite(*value) || std::abs(*value) > 9007199254740991.0)
                Invalid();
        }
        return values;
    }
    std::optional<double> UsageReport::Total(std::wstring const &metric) const
    {
        // Storage is a per-bucket gauge, not an additive count across days.
        if (metric == L"usage_bytes")
            return {};
        double sum = 0;
        for (auto const &[day, value] : Daily(metric))
        {
            (void)day;
            if (!value)
                return {};
            sum += *value;
            if (!std::isfinite(sum) || std::abs(sum) > 9007199254740991.0)
                Invalid();
        }
        return sum;
    }
    std::wstring UsageReport::Csv() const
    {
        auto metrics = UsageMetrics(query.category);
        std::wstring result = L"start_time_unix_utc,end_time_unix_utc,category,project_id,group_by,group";
        for (auto const &metric : metrics)
            result += L"," + Cell(metric);
        result += L"\r\n";
        for (auto const &row : rows)
        {
            result += std::to_wstring(row.start) + L"," + std::to_wstring(row.end) + L"," + Cell(query.category) +
                      L"," + Cell(query.project) + L"," + Cell(query.group) + L"," + Cell(row.group);
            for (auto const &metric : metrics)
            {
                auto value = row.metrics.find(metric);
                result += L",";
                if (value != row.metrics.end())
                    result += Cell(std::format(L"{:.15g}", value->second), false);
            }
            result += L"\r\n";
        }
        for (auto day : emptyBuckets)
        {
            result += std::to_wstring(day) + L"," + std::to_wstring(std::min(day + 86400, query.end)) + L"," +
                      Cell(query.category) + L"," + Cell(query.project) + L"," + Cell(query.group) + L",\"\"";
            for (size_t i = 0; i < metrics.size(); ++i)
                result += L",0";
            result += L"\r\n";
        }
        return result;
    }
} // namespace AetherNexus::Services
