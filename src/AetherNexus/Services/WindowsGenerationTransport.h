// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WindowsGenerationTransport.h
// Project: AetherNexus
#pragma once
#include "GenerationClient.h"
namespace AetherNexus::Services
{
    class WindowsGenerationTransport final : public IGenerationTransport
    {
      public:
        GenerationResponse Send(GenerationRequest const &, EventCallback const &, std::stop_token) override;
    };
} // namespace AetherNexus::Services
