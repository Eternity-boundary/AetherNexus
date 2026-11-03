// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WindowsManagementTransport.h
// Project: AetherNexus
#pragma once
#include "ManagementClient.h"
#include "KeyIssuance.h"
namespace AetherNexus::Services
{
    class WindowsManagementTransport final : public IManagementTransport, public IKeyIssuanceTransport
    {
      public:
        ApiResponse Send(ManagementRequest const &, std::stop_token) override;
        SensitiveApiResponse Issue(KeyIssueRequest const &, std::stop_token) override;
    };
} // namespace AetherNexus::Services
