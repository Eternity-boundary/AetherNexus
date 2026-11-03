// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: MockApiTransport.cpp
// Project: 其他檔案
#include "MockApiTransport.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>

namespace AetherNexus::Services
{
    using namespace winrt;
    using namespace Windows::Data::Json;
    ApiResponse MockApiTransport::Get(ApiRequest const& request, std::stop_token stop)
    {
        CheckCancelled(stop);
        (void)BuildRequestPath(request);
        m_requests.push_back(request);
        switch (m_scenario)
        {
        case MockScenario::Unauthorized: return {401, L"{\"error\":{\"message\":\"synthetic error\"}}"};
        case MockScenario::Forbidden: return {403, L"{}"};
        case MockScenario::RateLimited: return {429, L"{}"};
        case MockScenario::ServerError: return {503, L"{}"};
        case MockScenario::Malformed: return {200, L"not json"};
        case MockScenario::Timeout: throw ApiError{ApiErrorCode::Timeout};
        default: break;
        }
        if (request.endpoint == Endpoint::Models)
            return {200, m_scenario == MockScenario::Empty ? L"{\"object\":\"list\",\"data\":[]}"
                : L"{\"object\":\"list\",\"data\":[{\"object\":\"model\",\"id\":\"mock-text-model\",\"owned_by\":\"mock\"}]}"};
        if (request.endpoint == Endpoint::Projects)
        {
            if (m_scenario == MockScenario::Empty) return {200, L"{\"object\":\"list\",\"data\":[],\"has_more\":false}"};
            if (request.page.empty())
                return {200, LR"({"object":"list","data":[{"object":"organization.project","id":"proj_mock_alpha","name":"Development","created_at":1700000000,"status":"active","archived_at":null}],"has_more":true,"last_id":"proj_mock_alpha"})"};
            if (request.page == L"proj_mock_alpha")
                return {200, LR"({"object":"list","data":[{"object":"organization.project","id":"proj_mock_beta","name":"Production","created_at":1700000000,"status":"archived","archived_at":1700000100}],"has_more":false,"last_id":"proj_mock_beta"})"};
            throw ApiError{ApiErrorCode::InvalidResponse};
        }
        int offset = 0;
        if (!request.page.empty())
        {
            if (!request.page.starts_with(L"mock-page:")) throw ApiError{ApiErrorCode::InvalidResponse};
            offset = std::stoi(request.page.substr(10));
        }
        JsonArray data;
        auto const count = static_cast<int>((request.endTime - request.startTime + 86399) / 86400);
        int const end = std::min(offset + 7, count);
        if (m_scenario != MockScenario::Empty)
        {
            for (int i = offset; i < end; ++i)
            {
                auto const start = request.startTime + static_cast<int64_t>(i) * 86400;
                auto weight = 40 + (start / 86400 * 17) % 60;
                if (request.projectId == L"proj_mock_alpha") weight /= 2;
                else if (request.projectId == L"proj_mock_beta") weight -= weight / 2;
                else if (!request.projectId.empty()) continue;
                JsonObject result;
                if (request.endpoint == Endpoint::Costs)
                {
                    result.SetNamedValue(L"object", JsonValue::CreateStringValue(L"organization.costs.result"));
                    JsonObject amount;
                    amount.SetNamedValue(L"currency", JsonValue::CreateStringValue(L"usd"));
                    amount.SetNamedValue(L"value", JsonValue::CreateNumberValue(weight * 0.0055));
                    result.SetNamedValue(L"amount", amount);
                }
                else
                {
                    result.SetNamedValue(L"object", JsonValue::CreateStringValue(L"organization.usage.completions.result"));
                    result.SetNamedValue(L"input_tokens", JsonValue::CreateNumberValue(weight * 42000.0));
                    result.SetNamedValue(L"output_tokens", JsonValue::CreateNumberValue(weight * 8500.0));
                    result.SetNamedValue(L"input_cached_tokens", JsonValue::CreateNumberValue(weight * 20000.0));
                    result.SetNamedValue(L"num_model_requests", JsonValue::CreateNumberValue(weight * 5.0));
                }
                JsonArray results;
                results.Append(result);
                JsonObject bucket;
                bucket.SetNamedValue(L"object", JsonValue::CreateStringValue(L"bucket"));
                bucket.SetNamedValue(L"start_time", JsonValue::CreateNumberValue(static_cast<double>(start)));
                bucket.SetNamedValue(L"end_time", JsonValue::CreateNumberValue(static_cast<double>(start + 86400)));
                bucket.SetNamedValue(L"results", results);
                data.Append(bucket);
            }
        }
        bool const more = m_scenario != MockScenario::Empty && end < count;
        JsonObject page;
        page.SetNamedValue(L"object", JsonValue::CreateStringValue(L"page"));
        page.SetNamedValue(L"data", data);
        page.SetNamedValue(L"has_more", JsonValue::CreateBooleanValue(more));
        page.SetNamedValue(L"next_page", more ? JsonValue::CreateStringValue(L"mock-page:" + std::to_wstring(end)) : JsonValue::CreateNullValue());
        return {200, std::wstring{page.Stringify()}};
    }
}
