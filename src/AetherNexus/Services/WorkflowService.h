// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WorkflowService.h
// Project: AetherNexus
#pragma once
#include "GenerationClient.h"
namespace AetherNexus::Services
{
    struct WorkflowNode
    {
        std::wstring id, kind, name, instructions, model, next, otherwise, match;
        double x{}, y{};
    };
    struct Workflow
    {
        std::wstring entry;
        std::vector<WorkflowNode> nodes;
    };
    Workflow ParseWorkflow(std::wstring const &json);
    std::wstring SerializeWorkflow(Workflow const &workflow);
    void ValidateWorkflow(Workflow const &workflow);
    std::wstring ExportWorkflowPython(Workflow const &workflow);
    GenerationResult RunWorkflow(Workflow const &, std::wstring input, GenerationClient &, TextCallback const &,
                                 std::stop_token = {});
    std::wstring ValidateStoryboard(std::wstring const &json);
} // namespace AetherNexus::Services
