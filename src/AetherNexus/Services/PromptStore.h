// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PromptStore.h
// Project: AetherNexus
#pragma once
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
namespace AetherNexus::Services
{
struct PromptRevision
{
    std::wstring id, name, model, instructions, input, options{L"{}"};
    int64_t version{}, modified{};
};
class IPromptStore
{
  public:
    virtual ~IPromptStore() = default;
    virtual std::vector<PromptRevision> Read() = 0;
    virtual PromptRevision Save(PromptRevision const &) = 0;
    virtual void Remove(std::wstring const &) = 0;
};
class PromptStore final : public IPromptStore
{
  public:
    explicit PromptStore(std::filesystem::path path) : m_path(std::move(path))
    {
    }
    std::vector<PromptRevision> Read() override;
    PromptRevision Save(PromptRevision const &) override;
    void Remove(std::wstring const &) override;

  private:
    std::filesystem::path m_path;
    std::mutex m_mutex;
};
std::wstring SerializePrompt(PromptRevision const &);
PromptRevision ImportPrompt(std::wstring const &json);
std::wstring RenderPromptVariables(std::wstring const &text, std::wstring const &variablesJson);
} // namespace AetherNexus::Services
