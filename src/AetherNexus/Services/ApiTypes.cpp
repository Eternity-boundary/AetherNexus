// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ApiTypes.cpp
// Project: AetherNexus
#include "ApiTypes.h"
#include <winrt/base.h>
#include <algorithm>

namespace AetherNexus::Services
{
    namespace
    {
        std::wstring Encode(std::wstring const& value)
        {
            std::wstring encoded;
            constexpr wchar_t hex[] = L"0123456789ABCDEF";
            for (char byte : winrt::to_string(value))
            {
                auto const c = static_cast<unsigned char>(byte);
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
                    encoded += static_cast<wchar_t>(c);
                else { encoded += L'%'; encoded += hex[c >> 4]; encoded += hex[c & 15]; }
            }
            return encoded;
        }
    }
    CredentialRole RequiredCredential(Endpoint endpoint)
    {
        switch (endpoint)
        {
        case Endpoint::Models: return CredentialRole::Model;
        case Endpoint::CompletionsUsage:
        case Endpoint::Projects:
        case Endpoint::Costs: return CredentialRole::Admin;
        }
        throw ApiError{ApiErrorCode::InvalidResponse};
    }
    std::wstring BuildRequestPath(ApiRequest const& request)
    {
        if (request.page.size() > 4096 || request.projectId.size() > 256
            || std::any_of(request.projectId.begin(), request.projectId.end(), [](wchar_t c) { return c <= 0x20 || c == 0x7f; }))
            throw ApiError{ApiErrorCode::InvalidResponse};
        if (request.endpoint == Endpoint::Models || request.endpoint == Endpoint::Projects)
        {
            if (!request.projectId.empty()) throw ApiError{ApiErrorCode::InvalidResponse};
            if (request.endpoint == Endpoint::Models) return L"/v1/models";
            auto path = std::wstring{L"/v1/organization/projects?limit=100&include_archived=true"};
            if (!request.page.empty()) path += L"&after=" + Encode(request.page);
            return path;
        }
        if (request.startTime < 0 || request.endTime <= request.startTime) throw ApiError{ApiErrorCode::InvalidResponse};
        std::wstring path;
        switch (request.endpoint)
        {
        case Endpoint::CompletionsUsage: path = L"/v1/organization/usage/completions"; break;
        case Endpoint::Costs: path = L"/v1/organization/costs"; break;
        case Endpoint::Models:
        case Endpoint::Projects:
        default: throw ApiError{ApiErrorCode::InvalidResponse};
        }
        path += L"?start_time=" + std::to_wstring(request.startTime) + L"&end_time=" + std::to_wstring(request.endTime)
            + L"&bucket_width=1d&limit=31";
        if (!request.projectId.empty()) path += L"&project_ids%5B%5D=" + Encode(request.projectId);
        if (!request.page.empty()) path += L"&page=" + Encode(request.page);
        return path;
    }
    void CheckCancelled(std::stop_token stop)
    {
        if (stop.stop_requested()) throw ApiError{ApiErrorCode::Cancelled};
    }
    void CheckStatus(unsigned status)
    {
        if (status >= 200 && status < 300) return;
        if (status == 401) throw ApiError{ApiErrorCode::Unauthorized};
        if (status == 403) throw ApiError{ApiErrorCode::Forbidden};
        if (status == 429) throw ApiError{ApiErrorCode::RateLimited};
        if (status >= 500) throw ApiError{ApiErrorCode::Server};
        throw ApiError{ApiErrorCode::InvalidResponse};
    }
}
