// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PlatformApiClient.cpp
// Project: AetherNexus
#include "PlatformApiClient.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace AetherNexus::Services
{
    using namespace winrt;
    using namespace Windows::Data::Json;
    using namespace std::chrono;
    namespace
    {
        [[noreturn]] void Invalid() { throw ApiError{ApiErrorCode::InvalidResponse}; }
        int64_t Integer(JsonObject const& object, wchar_t const* key)
        {
            auto const value = object.GetNamedNumber(key);
            if (!std::isfinite(value) || value < 0 || value > 9007199254740991.0 || std::trunc(value) != value) Invalid();
            return static_cast<int64_t>(value);
        }
        void Add(int64_t& total, int64_t value)
        {
            if (total > std::numeric_limits<int64_t>::max() - value) Invalid();
            total += value;
        }
        JsonObject Parse(ApiResponse const& response)
        {
            CheckStatus(response.status); // Never parse or display error response bodies.
            if (response.body.size() > 4 * 1024 * 1024) Invalid();
            JsonObject object;
            if (!JsonObject::TryParse(response.body, object)) Invalid();
            return object;
        }
        template<typename Consume>
        void Pages(IApiTransport& transport, ApiRequest request, std::stop_token stop, Consume consume)
        {
            std::set<std::wstring> cursors;
            std::set<int64_t> bucketStarts;
            for (int page = 0; page < 64; ++page)
            {
                CheckCancelled(stop);
                auto const object = Parse(transport.Get(request, stop));
                if (object.GetNamedString(L"object") != L"page") Invalid();
                for (auto const& item : object.GetNamedArray(L"data"))
                {
                    auto const bucket = item.GetObject();
                    auto const start = Integer(bucket, L"start_time");
                    auto const end = Integer(bucket, L"end_time");
                    if (bucket.GetNamedString(L"object") != L"bucket" || start < request.startTime || start >= request.endTime
                        || end <= start || end - start > 86400 || start % 86400 != 0 || !bucketStarts.insert(start).second) Invalid();
                    consume(start, bucket.GetNamedArray(L"results"));
                }
                if (!object.GetNamedBoolean(L"has_more")) return;
                auto const next = object.GetNamedString(L"next_page", L"");
                if (next.empty() || next.size() > 4096 || !cursors.insert(std::wstring{next}).second) Invalid();
                request.page = next;
            }
            Invalid();
        }
        template<typename Work>
        std::optional<ApiErrorCode> Attempt(Work work)
        {
            try { work(); return {}; }
            catch (ApiError const& error)
            {
                if (error.code == ApiErrorCode::Cancelled) throw;
                return error.code;
            }
            catch (hresult_error const&) { return ApiErrorCode::InvalidResponse; }
        }
    }
    PlatformApiClient::PlatformApiClient(std::shared_ptr<IApiTransport> transport) : m_transport(std::move(transport))
    {
        if (!m_transport) throw std::invalid_argument("Transport is required");
    }
    DashboardResult PlatformApiClient::LoadDashboard(int count, sys_seconds now, std::stop_token stop, std::wstring const& projectId)
    {
        if (count != 7 && count != 30) throw std::invalid_argument("Unsupported range");
        auto const today = floor<days>(now);
        auto const first = today - days{count - 1};
        auto const monthStart = sys_days{year_month_day{today}.year() / year_month_day{today}.month() / 1};
        auto const timestamp = [](auto date) { return duration_cast<seconds>(date.time_since_epoch()).count(); };
        DashboardResult result;
        auto& snapshot = result.snapshot;
        // Explicitly materialize missing days as zero; an empty result is NOT an error.
        result.usageError = Attempt([&]
        {
            std::map<int64_t, Models::UsageBucket> buckets;
            for (int i = 0; i < count; ++i)
            {
                auto const date = first + days{i};
                buckets.emplace(timestamp(date), Models::UsageBucket{date});
            }
            bool cacheAvailable = true;
            Pages(*m_transport, {Endpoint::CompletionsUsage, timestamp(first), timestamp(now), {}, projectId}, stop,
                [&](int64_t start, JsonArray const& rows)
                {
                    auto& bucket = buckets.at(start);
                    for (auto const& row : rows)
                    {
                        auto const item = row.GetObject();
                        if (item.GetNamedString(L"object") != L"organization.usage.completions.result") Invalid();
                        auto const input = Integer(item, L"input_tokens");
                        auto const output = Integer(item, L"output_tokens");
                        auto const cachedField = item.TryLookup(L"input_cached_tokens");
                        auto const cached = cachedField && cachedField.ValueType() != JsonValueType::Null ? Integer(item, L"input_cached_tokens") : 0;
                        if (!cachedField || cachedField.ValueType() == JsonValueType::Null)
                        {
                            cacheAvailable = false;
                            bucket.cacheAvailable = false;
                        }
                        if (cached > input) Invalid();
                        Add(bucket.inputTokens, input);
                        Add(bucket.outputTokens, output);
                        Add(bucket.cachedInputTokens, cached);
                        Add(bucket.requests, Integer(item, L"num_model_requests"));
                    }
                });
            auto parsed = snapshot;
            parsed.cacheAvailable = cacheAvailable;
            for (auto const& [start, bucket] : buckets)
            {
                Add(parsed.inputTokens, bucket.inputTokens);
                Add(parsed.outputTokens, bucket.outputTokens);
                Add(parsed.cachedInputTokens, bucket.cachedInputTokens);
                Add(parsed.requests, bucket.requests);
                if (bucket.date == today) { Add(parsed.todayTokens, bucket.inputTokens); Add(parsed.todayTokens, bucket.outputTokens); }
                parsed.buckets.push_back(bucket);
            }
            int64_t total = parsed.inputTokens;
            Add(total, parsed.outputTokens);
            parsed.usageAvailable = true;
            snapshot = std::move(parsed);
        });
        CheckCancelled(stop);
        result.costsError = Attempt([&]
        {
            if (now == monthStart)
            {
                snapshot.costsAvailable = true;
                return;
            }
            double total = 0;
            Pages(*m_transport, {Endpoint::Costs, timestamp(monthStart), timestamp(now), {}, projectId}, stop,
                [&](int64_t, JsonArray const& rows)
                {
                    for (auto const& row : rows)
                    {
                        auto const item = row.GetObject();
                        if (item.GetNamedString(L"object") != L"organization.costs.result") Invalid();
                        auto const amount = item.GetNamedObject(L"amount");
                        if (amount.GetNamedString(L"currency") != L"usd") Invalid();
                        auto const value = amount.GetNamedNumber(L"value");
                        if (!std::isfinite(value) || !std::isfinite(total + value)) Invalid();
                        total += value; // Adjustments/credits may legitimately be negative.
                    }
                });
            snapshot.monthCostUsd = total;
            snapshot.costsAvailable = true;
        });
        return result;
    }
    std::vector<ProjectInfo> PlatformApiClient::ListProjects(std::stop_token stop)
    {
        CheckCancelled(stop);
        try
        {
            std::vector<ProjectInfo> projects;
            std::set<std::wstring> ids;
            ApiRequest request{Endpoint::Projects};
            for (int page = 0; page < 64; ++page)
            {
                CheckCancelled(stop);
                auto const object = Parse(m_transport->Get(request, stop));
                if (object.GetNamedString(L"object") != L"list") Invalid();
                auto const rows = object.GetNamedArray(L"data");
                if (rows.Size() > 100) Invalid();
                for (auto const& row : rows)
                {
                    auto const item = row.GetObject();
                    if (item.GetNamedString(L"object") != L"organization.project") Invalid();
                    auto const id = std::wstring{item.GetNamedString(L"id")};
                    if (id.empty() || id.size() > 256 || !ids.insert(id).second
                        || std::any_of(id.begin(), id.end(), [](wchar_t c) { return c <= 0x20 || c == 0x7f; })) Invalid();
                    auto const nameField = item.TryLookup(L"name");
                    auto const name = nameField && nameField.ValueType() != JsonValueType::Null
                        ? std::wstring{item.GetNamedString(L"name")} : std::wstring{};
                    if (name.size() > 1024) Invalid();
                    auto const statusField = item.TryLookup(L"status");
                    auto const status = statusField && statusField.ValueType() != JsonValueType::Null ? item.GetNamedString(L"status") : L"";
                    if (!status.empty() && status != L"active" && status != L"archived") Invalid();
                    auto const archivedAt = item.TryLookup(L"archived_at");
                    bool const archived = status == L"archived" || (archivedAt && archivedAt.ValueType() != JsonValueType::Null);
                    if (archivedAt && archivedAt.ValueType() != JsonValueType::Null) (void)Integer(item, L"archived_at");
                    projects.push_back({id, name, archived});
                }
                if (!object.GetNamedBoolean(L"has_more")) return projects;
                if (rows.Size() == 0) Invalid();
                // Projects use the last object's ID as `after`, not Usage's next_page cursor.
                request.page = projects.back().id;
            }
            Invalid();
        }
        catch (hresult_error const&) { throw ApiError{ApiErrorCode::InvalidResponse}; }
    }
    std::vector<ModelInfo> PlatformApiClient::ListModels(std::stop_token stop)
    {
        CheckCancelled(stop);
        try
        {
            auto const object = Parse(m_transport->Get({Endpoint::Models}, stop));
            if (object.GetNamedString(L"object") != L"list") Invalid();
            std::vector<ModelInfo> models;
            for (auto const& row : object.GetNamedArray(L"data"))
            {
                auto const item = row.GetObject();
                if (item.GetNamedString(L"object") != L"model") Invalid();
                models.push_back({std::wstring{item.GetNamedString(L"id")}, std::wstring{item.GetNamedString(L"owned_by")}});
            }
            return models;
        }
        catch (hresult_error const&) { throw ApiError{ApiErrorCode::InvalidResponse}; }
    }
}
