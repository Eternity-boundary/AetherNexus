// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ApiTypes.h
// Project: AetherNexus
#pragma once
#include "Models/UsageSnapshot.h"
#include "HttpResponseDiagnostics.h"
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <vector>

namespace AetherNexus::Services
{
    enum class Endpoint { CompletionsUsage, Costs, Models, Projects };
    enum class CredentialRole { Model, Admin };
    enum class ApiErrorCode { MissingCredential, CredentialUnavailable, Unauthorized, Forbidden, RateLimited, Server,
        Network, Timeout, InvalidResponse, Cancelled };
    class ApiError final : public std::runtime_error
    {
    public:
        explicit ApiError(ApiErrorCode code) : std::runtime_error("API operation failed"), code(code) {}
        ApiErrorCode code;
    };
    struct ApiRequest
    {
        Endpoint endpoint;
        int64_t startTime{};
        int64_t endTime{};
        std::wstring page;
        std::wstring projectId; // Empty means all projects; only usage/costs accept this filter.
    };
    enum class ResponseContentKind { Unknown, Json, Html, Other };
    enum class ResponseRequestIdState { NotRead, Missing, Valid, Rejected, TooLong, ReadFailed };
    struct HttpResponseDiagnostics
    {
        ResponseContentKind content{ResponseContentKind::Unknown};
        ResponseRequestIdState requestIdState{ResponseRequestIdState::NotRead};
        ResponseBodyFacts body{};
        bool openAiMetadata{}; // Presence only; not proof of the response's origin.
        std::wstring clientRequestId{};
    };
    struct ApiResponse
    {
        unsigned status{200};
        std::wstring body;
        std::wstring requestId{}; // Optional response correlation ID; never credentials or request contents.
        HttpResponseDiagnostics diagnostics{};
    };
    struct DashboardResult
    {
        Models::UsageSnapshot snapshot;
        std::optional<ApiErrorCode> usageError;
        std::optional<ApiErrorCode> costsError;
    };
    struct ModelInfo { std::wstring id; std::wstring ownedBy; };
    struct ProjectInfo { std::wstring id; std::wstring name; bool archived{}; };
    CredentialRole RequiredCredential(Endpoint endpoint);
    std::wstring BuildRequestPath(ApiRequest const& request);
    void CheckCancelled(std::stop_token stop);
    void CheckStatus(unsigned status);
    class IApiTransport
    {
    public:
        virtual ~IApiTransport() = default;
        virtual ApiResponse Get(ApiRequest const& request, std::stop_token stop) = 0;
    };
}
