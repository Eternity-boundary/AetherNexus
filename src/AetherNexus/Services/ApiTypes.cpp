// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ApiTypes.cpp
// Project: AetherNexus
#include "ApiTypes.h"
#include <winrt/base.h>

namespace AetherNexus::Services
{
    CredentialRole RequiredCredential(Endpoint endpoint)
    {
        switch (endpoint)
        {
        case Endpoint::Models: return CredentialRole::Model;
        case Endpoint::CompletionsUsage:
        case Endpoint::Costs: return CredentialRole::Admin;
        }
        throw ApiError{ApiErrorCode::InvalidResponse};
    }
    std::wstring BuildRequestPath(ApiRequest const& request)
    {
        if (request.endpoint == Endpoint::Models) return L"/v1/models";
        if (request.startTime < 0 || request.endTime <= request.startTime || request.page.size() > 4096)
            throw ApiError{ApiErrorCode::InvalidResponse};
        std::wstring path;
        switch (request.endpoint)
        {
        case Endpoint::CompletionsUsage: path = L"/v1/organization/usage/completions"; break;
        case Endpoint::Costs: path = L"/v1/organization/costs"; break;
        default: throw ApiError{ApiErrorCode::InvalidResponse};
        }
        path += L"?start_time=" + std::to_wstring(request.startTime) + L"&end_time=" + std::to_wstring(request.endTime)
            + L"&bucket_width=1d&limit=31";
        if (!request.page.empty())
        {
            path += L"&page=";
            constexpr wchar_t hex[] = L"0123456789ABCDEF";
            for (unsigned char c : winrt::to_string(request.page))
            {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
                    path += static_cast<wchar_t>(c);
                else { path += L'%'; path += hex[c >> 4]; path += hex[c & 15]; }
            }
        }
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
