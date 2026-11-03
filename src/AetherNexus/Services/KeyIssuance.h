// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: KeyIssuance.h
// Project: AetherNexus
#pragma once
#include "ApiTypes.h"
#include "Infrastructure/SecureBuffer.h"
#include <memory>

namespace AetherNexus::Services
{
enum class KeyIssueKind
{
    Admin,
    NewServiceAccount,
    ServiceAccount
};
struct KeyIssueRequest
{
    KeyIssueKind kind{KeyIssueKind::Admin};
    std::wstring project, serviceAccount, name;
    std::optional<int64_t> expiresInSeconds;
};
struct SensitiveApiResponse
{
    unsigned status{200};
    Infrastructure::SecureBuffer body;
};
struct IssuedKey
{
    Infrastructure::SecureBuffer secret;
    std::wstring id, serviceAccount;
    // Only a masked hint is safe to pass to XAML, accessibility or other immutable strings.
    std::wstring Hint() const;
};
class IKeyIssuanceTransport
{
  public:
    virtual ~IKeyIssuanceTransport() = default;
    virtual SensitiveApiResponse Issue(KeyIssueRequest const &, std::stop_token) = 0;
};
std::wstring KeyIssuePath(KeyIssueRequest const &);
std::wstring KeyIssueBody(KeyIssueRequest const &);
IssuedKey ParseIssuedKey(SensitiveApiResponse, KeyIssueRequest const &);
class KeyIssuanceClient final
{
  public:
    explicit KeyIssuanceClient(std::shared_ptr<IKeyIssuanceTransport>);
    IssuedKey Issue(KeyIssueRequest const &, std::stop_token = {});

  private:
    std::shared_ptr<IKeyIssuanceTransport> m_transport;
};
} // namespace AetherNexus::Services
