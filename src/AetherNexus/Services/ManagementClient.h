// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ManagementClient.h
// Project: AetherNexus
#pragma once
#include "ApiTypes.h"
#include <map>
#include <memory>

namespace AetherNexus::Services
{
    enum class ManagementResource
    {
        Projects,
        Project,
        Archive,
        ProjectKeys,
        ProjectKey,
        AdminKeys,
        AdminKey,
        Members,
        Member,
        RateLimits,
        RateLimit,
        Usage
    };
    enum class ManagementMethod
    {
        Get,
        Post,
        Delete
    };
    struct ManagementRequest
    {
        ManagementResource resource{ManagementResource::Projects};
        ManagementMethod method{ManagementMethod::Get};
        std::wstring project, item, page, category, group, body;
        int64_t start{}, end{};
    };
    std::wstring ManagementPath(ManagementRequest const &);
    wchar_t const *ManagementVerb(ManagementMethod);
    class IManagementTransport
    {
      public:
        virtual ~IManagementTransport() = default;
        virtual ApiResponse Send(ManagementRequest const &, std::stop_token) = 0;
    };
    struct ManagementRecord
    {
        std::wstring id, name, status, owner, keyHint;
        std::optional<int64_t> created, expires, lastUsed;
        std::vector<std::wstring> scopes;
        bool scopesKnown{};
        std::map<std::wstring, int64_t> limits;
    };
    struct UsageQuery
    {
        int64_t start{}, end{};
        std::wstring category{L"completions"}, project, group;
    };
    struct UsageRow
    {
        int64_t start{}, end{};
        std::wstring group;
        std::map<std::wstring, double> metrics;
    };
    struct UsageReport
    {
        UsageQuery query;
        std::vector<UsageRow> rows;
        std::vector<int64_t> emptyBuckets;
        std::map<int64_t, std::optional<double>> Daily(std::wstring const &) const;
        std::optional<double> Total(std::wstring const &) const;
        std::wstring Csv() const;
    };
    std::vector<std::wstring> UsageCategories();
    std::vector<std::wstring> UsageGroups(std::wstring const &category);
    std::vector<std::wstring> UsageMetrics(std::wstring const &category);
    std::vector<std::wstring> RateLimitFields();
    std::wstring ProjectNameBody(std::wstring const &);
    std::wstring MemberBody(std::wstring const &user, std::wstring const &role);
    std::wstring KeyPolicyBody(std::optional<std::vector<std::wstring>> const &scopes, std::optional<int64_t> expires);
    std::wstring RateLimitBody(std::map<std::wstring, int64_t> const &);
    class ManagementClient final
    {
      public:
        explicit ManagementClient(std::shared_ptr<IManagementTransport> transport);
        std::vector<ManagementRecord> List(ManagementResource, std::wstring const &project = {}, std::stop_token = {});
        ManagementRecord Change(ManagementRequest const &, std::stop_token = {});
        UsageReport Usage(UsageQuery const &, std::stop_token = {});

      private:
        std::shared_ptr<IManagementTransport> m_transport;
    };
} // namespace AetherNexus::Services
