// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: GenerationClient.h
// Project: AetherNexus
#pragma once
#include "ApiTypes.h"
#include <functional>
#include <memory>
#include <span>

namespace AetherNexus::Services
{
    enum class GenerationOperation
    {
        Response,
        Image,
        ImageEdit,
        Transcription,
        Translation,
        Speech,
        Realtime
    };
    struct Attachment
    {
        std::wstring name, mime;
        std::vector<uint8_t> bytes;
    };
    struct ChatMessage
    {
        std::wstring role, text;
        std::vector<Attachment> attachments;
    };
    struct GenerationRequest
    {
        GenerationOperation operation;
        std::string body;
        std::wstring contentType{L"application/json"};
        std::wstring model; // Only used to construct the fixed Realtime WebSocket query.
    };
    struct GenerationResponse
    {
        unsigned status{200};
        std::string body;
    };
    using EventCallback = std::function<void(std::string_view)>;
    class IGenerationTransport
    {
      public:
        virtual ~IGenerationTransport() = default;
        virtual GenerationResponse Send(GenerationRequest const &, EventCallback const &, std::stop_token) = 0;
    };
    struct GeneratedAsset
    {
        std::vector<uint8_t> bytes;
        std::wstring extension, mime;
    };
    struct GenerationResult
    {
        std::wstring text, id, details;
        std::vector<GeneratedAsset> assets;
        bool incomplete{};
    };
    using TextCallback = std::function<void(std::wstring const &)>;
    class EventStream final
    {
      public:
        explicit EventStream(EventCallback callback) : m_callback(std::move(callback))
        {
        }
        void Feed(std::string_view bytes);
        void Finish();

      private:
        std::string m_buffer, m_data;
        EventCallback m_callback;
        void Line(std::string line);
    };
    std::string Base64(std::span<uint8_t const> bytes);
    std::vector<uint8_t> DecodeBase64(std::string_view encoded);
    std::vector<uint8_t> PcmFromWav(std::span<uint8_t const> wav);
    std::vector<uint8_t> WavFromPcm(std::span<uint8_t const> pcm);
    std::wstring GenerationPath(GenerationOperation operation);
    class GenerationClient final
    {
      public:
        explicit GenerationClient(std::shared_ptr<IGenerationTransport> transport) : m_transport(std::move(transport))
        {
        }
        GenerationResult Chat(std::wstring const &model, std::wstring const &instructions,
                              std::vector<ChatMessage> const &messages, std::wstring const &options,
                              TextCallback const &, std::stop_token = {});
        GenerationResult Images(std::wstring const &model, std::wstring const &prompt, std::wstring const &options,
                                std::vector<Attachment> const &references, std::stop_token = {});
        GenerationResult Audio(GenerationOperation, std::wstring const &model, std::wstring const &input,
                               std::wstring const &options, std::vector<Attachment> const &, TextCallback const &,
                               std::stop_token = {}, std::vector<ChatMessage> const &history = {});
        static std::wstring RequestCode(std::wstring const &model, std::wstring const &instructions,
                                        std::vector<ChatMessage> const &, std::wstring const &options);

      private:
        std::shared_ptr<IGenerationTransport> m_transport;
    };
} // namespace AetherNexus::Services
