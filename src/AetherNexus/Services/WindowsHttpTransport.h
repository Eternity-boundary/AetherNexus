// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WindowsHttpTransport.h
// Project: AetherNexus
#pragma once
#include "ApiTypes.h"

namespace AetherNexus::Services
{
    class WindowsHttpTransport final : public IApiTransport
    {
    public:
        ApiResponse Get(ApiRequest const& request, std::stop_token stop) override;
    };
}
