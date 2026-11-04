// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: BuildClient.h
// Project: AetherNexus
#pragma once
#include "ApiTypes.h"
#include <filesystem>
#include <memory>

namespace AetherNexus::Services
{
enum class BuildResource
{
    Models,
    Chats,
    ChatMessages,
    Responses,
    ResponseItems,
    Sessions,
    Traces,
    Batches,
    BatchCancel,
    Evals,
    EvalRuns,
    EvalOutputs,
    FineJobs,
    FineCancel,
    FineEvents,
    FineCheckpoints,
    Files,
    FileContent,
    Stores,
    StoreFiles,
    StoreSearch
};
enum class BuildMethod
{
    Get,
    Post,
    Delete
};
struct BuildUpload
{
    std::filesystem::path path;
    std::wstring name, purpose;
    uint64_t bytes{};
};
struct BuildRequest
{
    BuildResource resource{};
    BuildMethod method{BuildMethod::Get};
    std::wstring item, parent, scope, after, filter, project;
    std::string body;
    std::optional<BuildUpload> upload;
    std::filesystem::path download;
    bool ascending{};
};
struct BuildWireRequest
{
    std::wstring path, method, headers;
    std::string body;
};
struct BuildResponse
{
    unsigned status{200};
    std::string body;
    uint64_t downloadedBytes{};
};
class IBuildTransport
{
  public:
    virtual ~IBuildTransport() = default;
    virtual BuildResponse Send(BuildRequest const &, std::stop_token) = 0;
};
class BuildHttpError final : public std::runtime_error
{
  public:
    explicit BuildHttpError(unsigned status) : std::runtime_error("Build API rejected request"), status(status)
    {
    }
    unsigned status;
};
struct BuildRecord
{
    std::wstring id, title, subtitle, status, json;
    std::optional<int64_t> created;
};
struct BuildList
{
    std::vector<BuildRecord> records;
    bool hasMore{};
    std::wstring after;
};
struct BuildPolicy
{
    bool evalsReadable{}, fineTuningCreatable{};
};
struct TraceSpan
{
    std::wstring id, parent, name, status, json;
    std::optional<double> milliseconds;
};
BuildPolicy BuildPolicyAt(int64_t unixSeconds);
void ValidateBuildIdentifier(std::wstring_view);
BuildWireRequest BuildWire(BuildRequest const &);
std::string BatchBody(std::wstring const &file, std::wstring const &endpoint);
std::string FineTuneBody(std::wstring const &model, std::wstring const &training, std::wstring const &validation,
                         std::wstring const &suffix, std::wstring const &method, std::wstring const &hyperparameters);
std::string StoreBody(std::wstring const &name, std::wstring const &fileIds, std::optional<int64_t> expiryDays);
std::string StoreFileBody(std::wstring const &file);
std::string StoreSearchBody(std::wstring const &query);
std::string PromptTestBody(std::wstring const &model, std::wstring const &instructions, std::wstring const &input,
                           std::wstring const &options);
std::wstring PrettyBuildJson(std::wstring const &);
std::vector<TraceSpan> ParseTraceSpans(std::wstring const &);
class BuildClient final
{
  public:
    explicit BuildClient(std::shared_ptr<IBuildTransport> transport) : m_transport(std::move(transport))
    {
    }
    BuildList List(BuildRequest const &, std::stop_token = {});
    BuildRecord Retrieve(BuildRequest const &, std::stop_token = {});
    BuildRecord Change(BuildRequest const &, std::stop_token = {});
    uint64_t Download(BuildRequest const &, std::stop_token = {});

  private:
    std::shared_ptr<IBuildTransport> m_transport;
};
} // namespace AetherNexus::Services
