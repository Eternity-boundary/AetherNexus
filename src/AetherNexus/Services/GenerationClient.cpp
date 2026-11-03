// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: GenerationClient.cpp
// Project: AetherNexus
#include "GenerationClient.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace AetherNexus::Services
{
    using namespace winrt;
    using namespace Windows::Data::Json;
    namespace
    {
        [[noreturn]] void Invalid()
        {
            throw ApiError{ApiErrorCode::InvalidResponse};
        }
        void Require(bool value)
        {
            if (!value)
                throw std::invalid_argument("Invalid generation input");
        }
        JsonObject Object(std::wstring const &text)
        {
            JsonObject object;
            Require(text.size() <= 1024 * 1024 && JsonObject::TryParse(text.empty() ? L"{}" : hstring{text}, object));
            return object;
        }
        void Set(JsonObject const &object, wchar_t const *key, std::wstring const &value)
        {
            object.SetNamedValue(key, JsonValue::CreateStringValue(value));
        }
        void Status(unsigned status)
        {
            if (status == 400 || status == 404 || status == 413 || status == 422)
                throw std::invalid_argument("API rejected generation options");
            CheckStatus(status);
        }
        JsonObject Parse(std::string_view bytes)
        {
            if (bytes.size() > 160 * 1024 * 1024)
                Invalid();
            JsonObject value;
            if (!JsonObject::TryParse(to_hstring(bytes), value))
                Invalid();
            return value;
        }
        std::wstring DataUrl(Attachment const &file)
        {
            Require(!file.bytes.empty() && file.bytes.size() <= 25 * 1024 * 1024);
            return L"data:" + file.mime + L";base64," + to_hstring(Base64(file.bytes)).c_str();
        }
        JsonObject ChatBody(std::wstring const &model, std::wstring const &instructions,
                            std::vector<ChatMessage> const &messages, std::wstring const &options)
        {
            Require(!model.empty() && model.size() <= 256 && !messages.empty() && messages.size() <= 256);
            auto body = Object(options);
            // Inputs, authentication and transport are controlled by typed fields, not the advanced editor.
            for (auto key : {L"model", L"input", L"instructions", L"stream", L"previous_response_id", L"conversation",
                             L"background"})
                Require(!body.HasKey(key));
            if (!body.HasKey(L"store"))
                body.SetNamedValue(L"store", JsonValue::CreateBooleanValue(false));
            Set(body, L"model", model);
            if (!instructions.empty())
                Set(body, L"instructions", instructions);
            JsonArray input;
            size_t totalBytes = 0, totalText = 0;
            for (auto const &message : messages)
            {
                Require(message.role == L"user" || message.role == L"assistant" || message.role == L"developer" ||
                        message.role == L"system");
                Require(message.text.size() <= 1024 * 1024);
                totalText += message.text.size();
                Require(totalText <= 4 * 1024 * 1024);
                for (auto const &file : message.attachments)
                {
                    totalBytes += file.bytes.size();
                    Require(totalBytes <= 64 * 1024 * 1024);
                }
                JsonObject item;
                Set(item, L"role", message.role);
                if (message.attachments.empty())
                    Set(item, L"content", message.text);
                else
                {
                    Require(message.role == L"user" && message.attachments.size() <= 16);
                    JsonArray content;
                    if (!message.text.empty())
                    {
                        JsonObject text;
                        Set(text, L"type", L"input_text");
                        Set(text, L"text", message.text);
                        content.Append(text);
                    }
                    for (auto const &file : message.attachments)
                    {
                        JsonObject part;
                        if (file.mime.starts_with(L"image/"))
                        {
                            Set(part, L"type", L"input_image");
                            Set(part, L"image_url", DataUrl(file));
                        }
                        else
                        {
                            Set(part, L"type", L"input_file");
                            Set(part, L"filename", file.name);
                            Set(part, L"file_data", DataUrl(file));
                        }
                        content.Append(part);
                    }
                    item.SetNamedValue(L"content", content);
                }
                input.Append(item);
            }
            body.SetNamedValue(L"input", input);
            body.SetNamedValue(L"stream", JsonValue::CreateBooleanValue(true));
            return body;
        }
        GenerationResult ResponseResult(JsonObject const &object)
        {
            GenerationResult result;
            result.id = object.GetNamedString(L"id", L"");
            auto const status = object.GetNamedString(L"status", L"");
            if (status == L"failed")
                Invalid();
            result.incomplete = status == L"incomplete" || status == L"cancelled";
            for (auto const &output : object.GetNamedArray(L"output"))
            {
                auto const item = output.GetObject();
                if (item.GetNamedString(L"type", L"") != L"message")
                    continue;
                for (auto const &entry : item.GetNamedArray(L"content"))
                {
                    auto const part = entry.GetObject();
                    auto const type = part.GetNamedString(L"type", L"");
                    if (type == L"output_text")
                        result.text += part.GetNamedString(L"text");
                    else if (type == L"refusal")
                        result.text += part.GetNamedString(L"refusal");
                }
            }
            result.details = object.Stringify();
            return result;
        }
        void SafeField(std::wstring const &value)
        {
            Require(value.size() <= 256 && value.find_first_of(L"\r\n\"") == std::wstring::npos &&
                    value.find(L'\0') == std::wstring::npos);
        }
    } // namespace
    std::wstring GenerationPath(GenerationOperation operation)
    {
        switch (operation)
        {
        case GenerationOperation::Response:
            return L"/v1/responses";
        case GenerationOperation::Image:
            return L"/v1/images/generations";
        case GenerationOperation::ImageEdit:
            return L"/v1/images/edits";
        case GenerationOperation::Transcription:
            return L"/v1/audio/transcriptions";
        case GenerationOperation::Translation:
            return L"/v1/audio/translations";
        case GenerationOperation::Speech:
            return L"/v1/audio/speech";
        case GenerationOperation::Realtime:
            return L"/v1/realtime";
        }
        Invalid();
    }
    void EventStream::Line(std::string line)
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
        {
            if (!m_data.empty())
            {
                m_data.pop_back();
                if (m_data != "[DONE]")
                    m_callback(m_data);
                m_data.clear();
            }
        }
        else if (line.starts_with("data:"))
        {
            size_t const offset = line.size() > 5 && line[5] == ' ' ? 6u : 5u;
            m_data += line.substr(offset);
            m_data += '\n';
            if (m_data.size() > 16 * 1024 * 1024)
                Invalid();
        }
    }
    void EventStream::Feed(std::string_view bytes)
    {
        m_buffer.append(bytes);
        if (m_buffer.size() > 16 * 1024 * 1024)
            Invalid();
        size_t end;
        while ((end = m_buffer.find('\n')) != std::string::npos)
        {
            auto line = m_buffer.substr(0, end);
            m_buffer.erase(0, end + 1);
            Line(std::move(line));
        }
    }
    void EventStream::Finish()
    {
        if (!m_buffer.empty())
        {
            Line(std::move(m_buffer));
            m_buffer.clear();
        }
        Line({});
    }
    std::string Base64(std::span<uint8_t const> bytes)
    {
        constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        result.reserve((bytes.size() + 2) / 3 * 4);
        for (size_t i = 0; i < bytes.size(); i += 3)
        {
            unsigned value = static_cast<unsigned>(bytes[i]) << 16;
            if (i + 1 < bytes.size())
                value |= static_cast<unsigned>(bytes[i + 1]) << 8;
            if (i + 2 < bytes.size())
                value |= bytes[i + 2];
            result += alphabet[value >> 18];
            result += alphabet[value >> 12 & 63];
            result += i + 1 < bytes.size() ? alphabet[value >> 6 & 63] : '=';
            result += i + 2 < bytes.size() ? alphabet[value & 63] : '=';
        }
        return result;
    }
    std::vector<uint8_t> DecodeBase64(std::string_view text)
    {
        if (text.empty() || text.size() % 4 || text.size() > 128 * 1024 * 1024)
            Invalid();
        std::vector<uint8_t> bytes;
        bytes.reserve(text.size() / 4 * 3);
        auto digit = [](char c) -> unsigned {
            if (c >= 'A' && c <= 'Z')
                return static_cast<unsigned>(c - 'A');
            if (c >= 'a' && c <= 'z')
                return static_cast<unsigned>(c - 'a' + 26);
            if (c >= '0' && c <= '9')
                return static_cast<unsigned>(c - '0' + 52);
            if (c == '+')
                return 62;
            if (c == '/')
                return 63;
            Invalid();
        };
        for (size_t i = 0; i < text.size(); i += 4)
        {
            bool const pad2 = text[i + 2] == '=', pad3 = text[i + 3] == '=';
            if ((pad2 || pad3) && i + 4 != text.size())
                Invalid();
            if (pad2 && !pad3)
                Invalid();
            auto a = digit(text[i]), b = digit(text[i + 1]), c = pad2 ? 0 : digit(text[i + 2]),
                 d = pad3 ? 0 : digit(text[i + 3]);
            if ((pad2 && (b & 15)) || (pad3 && !pad2 && (c & 3)))
                Invalid();
            unsigned value = a << 18 | b << 12 | c << 6 | d;
            bytes.push_back(static_cast<uint8_t>(value >> 16));
            if (!pad2)
                bytes.push_back(static_cast<uint8_t>(value >> 8));
            if (!pad3)
                bytes.push_back(static_cast<uint8_t>(value));
        }
        return bytes;
    }
    std::vector<uint8_t> PcmFromWav(std::span<uint8_t const> wav)
    {
        Require(wav.size() >= 44 && wav.size() <= 25 * 1024 * 1024 && !std::memcmp(wav.data(), "RIFF", 4) &&
                !std::memcmp(wav.data() + 8, "WAVE", 4));
        auto u32 = [&](size_t i) {
            return static_cast<uint32_t>(wav[i]) | static_cast<uint32_t>(wav[i + 1]) << 8 |
                   static_cast<uint32_t>(wav[i + 2]) << 16 | static_cast<uint32_t>(wav[i + 3]) << 24;
        };
        bool format = false;
        for (size_t i = 12; i + 8 <= wav.size();)
        {
            auto const length = u32(i + 4);
            Require(length <= wav.size() - i - 8);
            if (!std::memcmp(wav.data() + i, "fmt ", 4))
            {
                Require(length >= 16 && wav[i + 8] == 1 && wav[i + 9] == 0 && wav[i + 10] == 1 && wav[i + 11] == 0 &&
                        u32(i + 12) == 24000 && wav[i + 22] == 16 && wav[i + 23] == 0);
                format = true;
            }
            if (!std::memcmp(wav.data() + i, "data", 4))
            {
                Require(format && length > 0 && length % 2 == 0);
                auto const data = wav.subspan(i + 8, length);
                return {data.begin(), data.end()};
            }
            i += 8 + static_cast<size_t>(length) + (length & 1);
        }
        Require(false);
        return {};
    }
    std::vector<uint8_t> WavFromPcm(std::span<uint8_t const> pcm)
    {
        Require(!pcm.empty() && pcm.size() % 2 == 0 && pcm.size() <= 64 * 1024 * 1024);
        std::vector<uint8_t> wav(44 + pcm.size());
        auto u32 = [&](size_t i, uint32_t value) {
            for (unsigned j = 0; j < 4; ++j)
                wav[i + j] = static_cast<uint8_t>(value >> (j * 8));
        };
        std::memcpy(wav.data(), "RIFF", 4);
        u32(4, static_cast<uint32_t>(wav.size() - 8));
        std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
        u32(16, 16);
        wav[20] = 1;
        wav[22] = 1;
        u32(24, 24000);
        u32(28, 48000);
        wav[32] = 2;
        wav[34] = 16;
        std::memcpy(wav.data() + 36, "data", 4);
        u32(40, static_cast<uint32_t>(pcm.size()));
        std::copy(pcm.begin(), pcm.end(), wav.begin() + 44);
        return wav;
    }
    GenerationResult GenerationClient::Chat(std::wstring const &model, std::wstring const &instructions,
                                            std::vector<ChatMessage> const &messages, std::wstring const &options,
                                            TextCallback const &callback, std::stop_token stop)
    {
        CheckCancelled(stop);
        auto const body = ChatBody(model, instructions, messages, options);
        GenerationResult result;
        bool finished = false;
        EventStream events{[&](std::string_view data) {
            CheckCancelled(stop);
            auto const event = Parse(data);
            auto const type = event.GetNamedString(L"type");
            if (type == L"response.output_text.delta" || type == L"response.refusal.delta")
            {
                result.text += event.GetNamedString(L"delta");
                if (result.text.size() > 4 * 1024 * 1024)
                    Invalid();
                if (callback)
                    callback(result.text);
            }
            else if (type == L"response.completed" || type == L"response.incomplete")
            {
                result = ResponseResult(event.GetNamedObject(L"response"));
                finished = true;
            }
            else if (type == L"error" || type == L"response.failed")
                Invalid();
        }};
        auto response = m_transport->Send(
            {GenerationOperation::Response, to_string(body.Stringify())}, [&](auto bytes) { events.Feed(bytes); },
            stop);
        Status(response.status);
        events.Finish();
        CheckCancelled(stop);
        if (!finished && !response.body.empty())
        {
            result = ResponseResult(Parse(response.body));
            finished = true;
        }
        if (!finished)
            Invalid();
        if (callback)
            callback(result.text);
        return result;
    }
    GenerationResult GenerationClient::Images(std::wstring const &model, std::wstring const &prompt,
                                              std::wstring const &options, std::vector<Attachment> const &references,
                                              std::stop_token stop)
    {
        Require(!model.empty() && !prompt.empty() && prompt.size() <= 32000 && references.size() <= 16);
        auto body = Object(options);
        for (auto key : {L"model", L"prompt", L"images", L"image", L"stream"})
            Require(!body.HasKey(key));
        Set(body, L"model", model);
        Set(body, L"prompt", prompt);
        auto const format = body.GetNamedString(L"output_format", L"png");
        Require(format == L"png" || format == L"jpeg" || format == L"webp");
        auto const count = body.GetNamedNumber(L"n", 1);
        Require(std::isfinite(count) && count >= 1 && count <= 10 && std::floor(count) == count);
        Require(body.GetNamedString(L"background", L"auto") != L"transparent" || format != L"jpeg");
        if (!references.empty())
        {
            size_t totalBytes = 0;
            for (auto const &file : references)
            {
                totalBytes += file.bytes.size();
                Require(totalBytes <= 64 * 1024 * 1024);
            }
            JsonArray images;
            for (auto const &file : references)
            {
                Require(file.mime.starts_with(L"image/"));
                JsonObject image;
                Set(image, L"image_url", DataUrl(file));
                images.Append(image);
            }
            body.SetNamedValue(L"images", images);
        }
        auto response =
            m_transport->Send({references.empty() ? GenerationOperation::Image : GenerationOperation::ImageEdit,
                               to_string(body.Stringify())},
                              {}, stop);
        Status(response.status);
        CheckCancelled(stop);
        auto object = Parse(response.body);
        GenerationResult result;
        auto const data = object.GetNamedArray(L"data");
        if (data.Size() == 0 || data.Size() > 10)
            Invalid();
        for (auto const &entry : data)
        {
            auto item = entry.GetObject();
            auto bytes = DecodeBase64(to_string(item.GetNamedString(L"b64_json")));
            result.assets.push_back({std::move(bytes), L"." + std::wstring{format}, L"image/" + std::wstring{format}});
            item.Remove(L"b64_json"); // Keep metadata without duplicating large encoded files in the UI.
        }
        result.details = object.Stringify();
        return result;
    }
    GenerationResult GenerationClient::Audio(GenerationOperation operation, std::wstring const &model,
                                             std::wstring const &input, std::wstring const &options,
                                             std::vector<Attachment> const &attachments, TextCallback const &callback,
                                             std::stop_token stop, std::vector<ChatMessage> const &history)
    {
        Require(!model.empty() && model.size() <= 256);
        auto body = Object(options);
        Require(!body.HasKey(L"model"));
        Set(body, L"model", model);
        GenerationRequest request{operation};
        if (operation == GenerationOperation::Realtime)
        {
            Require(!input.empty() || !attachments.empty());
            Require(!body.HasKey(L"input") && !body.HasKey(L"history") && history.size() <= 64);
            JsonArray prior;
            size_t totalBytes = 0;
            for (auto const &message : history)
            {
                Require((message.role == L"user" || message.role == L"assistant") && message.text.size() <= 256000);
                JsonObject item;
                Set(item, L"type", L"message");
                Set(item, L"role", message.role);
                JsonArray parts;
                if (!message.text.empty())
                {
                    JsonObject text;
                    Set(text, L"type", message.role == L"assistant" ? L"output_text" : L"input_text");
                    Set(text, L"text", message.text);
                    parts.Append(text);
                }
                for (auto const &file : message.attachments)
                {
                    Require(message.role == L"user");
                    totalBytes += file.bytes.size();
                    Require(totalBytes <= 64 * 1024 * 1024);
                    auto pcm = PcmFromWav(file.bytes);
                    JsonObject audio;
                    Set(audio, L"type", L"input_audio");
                    Set(audio, L"audio", std::wstring{to_hstring(Base64(pcm))});
                    parts.Append(audio);
                }
                Require(parts.Size() > 0);
                item.SetNamedValue(L"content", parts);
                prior.Append(item);
            }
            body.SetNamedValue(L"history", prior);
            JsonArray content;
            if (!input.empty())
            {
                JsonObject text;
                Set(text, L"type", L"input_text");
                Set(text, L"text", input);
                content.Append(text);
            }
            if (!attachments.empty())
            {
                Require(attachments.size() == 1);
                auto pcm = PcmFromWav(attachments.front().bytes);
                JsonObject audio;
                Set(audio, L"type", L"input_audio");
                Set(audio, L"audio", to_hstring(Base64(pcm)).c_str());
                content.Append(audio);
            }
            body.SetNamedValue(L"input", content);
            request.body = to_string(body.Stringify());
            request.model = model;
            GenerationResult result;
            std::vector<uint8_t> audio;
            bool done = false;
            auto response = m_transport->Send(
                request,
                [&](auto data) {
                    auto event = Parse(data);
                    auto type = event.GetNamedString(L"type");
                    if (type == L"error")
                        Invalid();
                    if (type == L"response.output_audio.delta")
                    {
                        auto bytes = DecodeBase64(to_string(event.GetNamedString(L"delta")));
                        if (audio.size() + bytes.size() > 64 * 1024 * 1024)
                            Invalid();
                        audio.insert(audio.end(), bytes.begin(), bytes.end());
                    }
                    else if (type == L"response.output_audio_transcript.delta" || type == L"response.output_text.delta")
                    {
                        result.text += event.GetNamedString(L"delta");
                        if (result.text.size() > 4 * 1024 * 1024)
                            Invalid();
                        if (callback)
                            callback(result.text);
                    }
                    else if (type == L"response.done")
                    {
                        auto final = event.GetNamedObject(L"response");
                        if (final.GetNamedString(L"status") != L"completed")
                            Invalid();
                        result.id = final.GetNamedString(L"id", L"");
                        result.details = final.Stringify();
                        done = true;
                    }
                },
                stop);
            Status(response.status);
            CheckCancelled(stop);
            if (!done)
                Invalid();
            if (!audio.empty())
                result.assets.push_back({WavFromPcm(audio), L".wav", L"audio/wav"});
            return result;
        }
        if (operation == GenerationOperation::Speech)
        {
            Require(!input.empty() && input.size() <= 4096);
            Require(!body.HasKey(L"input"));
            Set(body, L"input", input);
            if (!body.HasKey(L"response_format"))
                Set(body, L"response_format", L"wav");
            auto format = body.GetNamedString(L"response_format");
            Require(format == L"wav" || format == L"mp3" || format == L"opus" || format == L"aac" ||
                    format == L"flac" || format == L"pcm");
            request.body = to_string(body.Stringify());
        }
        else
        {
            Require(operation == GenerationOperation::Transcription || operation == GenerationOperation::Translation);
            Require(attachments.size() == 1 && !attachments[0].bytes.empty() &&
                    attachments[0].bytes.size() <= 25 * 1024 * 1024);
            // Fixed boundary cannot occur at the start of a multipart delimiter within the file.
            std::string boundary = "AetherNexusBoundary7a681ed4437c";
            std::string file(reinterpret_cast<char const *>(attachments[0].bytes.data()), attachments[0].bytes.size());
            while (file.find(boundary) != std::string::npos)
                boundary += 'x';
            request.contentType = L"multipart/form-data; boundary=" + to_hstring(boundary);
            Require(!body.HasKey(L"file") && !body.HasKey(L"stream") && !body.HasKey(L"response_format"));
            if (!input.empty())
                Set(body, L"prompt", input);
            Set(body, L"response_format", L"json");
            for (auto const &pair : body)
            {
                SafeField(std::wstring{pair.Key()});
                auto value = pair.Value();
                Require(value.ValueType() != JsonValueType::Object && value.ValueType() != JsonValueType::Array &&
                        value.ValueType() != JsonValueType::Null);
                request.body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" +
                                to_string(pair.Key()) + "\"\r\n\r\n" +
                                (value.ValueType() == JsonValueType::String ? to_string(value.GetString())
                                                                            : to_string(value.Stringify())) +
                                "\r\n";
            }
            auto const &fileName = attachments[0].name;
            SafeField(fileName);
            SafeField(attachments[0].mime);
            request.body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" +
                            to_string(fileName) + "\"\r\nContent-Type: " + to_string(attachments[0].mime) + "\r\n\r\n" +
                            file + "\r\n--" + boundary + "--\r\n";
        }
        auto response = m_transport->Send(request, {}, stop);
        Status(response.status);
        CheckCancelled(stop);
        GenerationResult result;
        if (operation == GenerationOperation::Speech)
        {
            auto format = std::wstring{body.GetNamedString(L"response_format")};
            Require(format == L"wav" || format == L"mp3" || format == L"opus" || format == L"aac" ||
                    format == L"flac" || format == L"pcm");
            if (response.body.empty())
                Invalid();
            result.assets.push_back({{response.body.begin(), response.body.end()},
                                     L"." + format,
                                     L"audio/" + (format == L"mp3" ? L"mpeg" : format)});
        }
        else
        {
            auto object = Parse(response.body);
            result.text = object.GetNamedString(L"text");
            result.details = object.Stringify();
        }
        return result;
    }
    std::wstring GenerationClient::RequestCode(std::wstring const &model, std::wstring const &instructions,
                                               std::vector<ChatMessage> const &messages, std::wstring const &options)
    {
        auto body = ChatBody(model, instructions, messages, options);
        // JSON is embedded as base64, so user text cannot escape into executable Python.
        auto json = to_string(body.Stringify());
        auto encoded = Base64({reinterpret_cast<uint8_t const *>(json.data()), json.size()});
        return L"import os, json, base64\nfrom openai import OpenAI\n\nclient = "
               L"OpenAI(api_key=os.environ[\"OPENAI_API_KEY\"])\nrequest = json.loads(base64.b64decode(\"" +
               std::wstring{to_hstring(encoded)} +
               L"\"))\nwith client.responses.stream(**{k: v for k, v in request.items() if k != \"stream\"}) as "
               L"stream:\n  "
               L"  for event in stream:\n        if event.type == \"response.output_text.delta\":\n            "
               L"print(event.delta, end=\"\", flush=True)\n";
    }
} // namespace AetherNexus::Services
