// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WindowsBuildTransport.h
// Project: AetherNexus
#pragma once
#include "BuildClient.h"
namespace AetherNexus::Services
{
class WindowsBuildTransport final : public IBuildTransport
{
  public:
    BuildResponse Send(BuildRequest const &, std::stop_token) override;
};
} // namespace AetherNexus::Services
