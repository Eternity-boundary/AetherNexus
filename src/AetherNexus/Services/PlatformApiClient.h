// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PlatformApiClient.h
// Project: AetherNexus
#pragma once
#include "ApiTypes.h"
#include <memory>

namespace AetherNexus::Services
{
    // Synchronous transport work runs on a background apartment, never the XAML thread.
    class PlatformApiClient final
    {
    public:
        explicit PlatformApiClient(std::shared_ptr<IApiTransport> transport);
        DashboardResult LoadDashboard(int days, std::chrono::sys_seconds now, std::stop_token stop = {}, std::wstring const& projectId = {});
        std::vector<ModelInfo> ListModels(std::stop_token stop = {});
        std::vector<ProjectInfo> ListProjects(std::stop_token stop = {});
    private:
        std::shared_ptr<IApiTransport> m_transport;
    };
}
