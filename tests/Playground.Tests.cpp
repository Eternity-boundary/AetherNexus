// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: Playground.Tests.cpp
// Project: 其他檔案
#include "Services/GenerationClient.h"
#include "Services/WorkflowService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
using namespace AetherNexus::Services;
using namespace winrt::Windows::Data::Json;
namespace
{
    void Require(bool condition)
    {
        if (!condition)
            throw std::runtime_error("Playground assertion failed");
    }
    template <typename Work> void Reject(Work work)
    {
        bool failed = false;
        try
        {
            work();
        }
        catch (...)
        {
            failed = true;
        }
        Require(failed);
    }
    class VirtualTransport final : public IGenerationTransport
    {
      public:
        std::vector<GenerationRequest> requests;
        unsigned status = 200;
        bool truncated = false;
        GenerationResponse Send(GenerationRequest const &request, EventCallback const &callback,
                                std::stop_token stop) override
        {
            CheckCancelled(stop);
            requests.push_back(request);
            if (status != 200)
                return {status, "private synthetic error"};
            if (request.operation == GenerationOperation::Response)
            {
                std::string stream = R"(event: response.output_text.delta
data: {"type":"response.output_text.delta","delta":"測試"}

)";
                if (!truncated)
                    stream += R"(event: response.completed
data: {"type":"response.completed","response":{"id":"resp_virtual","status":"completed","output":[{"type":"message","content":[{"type":"output_text","text":"測試"}]}],"usage":{"input_tokens":3,"output_tokens":2}}}

)";
                // Split inside UTF-8, CR/LF, JSON and SSE field names.
                for (char byte : stream)
                    callback({&byte, 1});
                return {200, {}};
            }
            if (request.operation == GenerationOperation::Image || request.operation == GenerationOperation::ImageEdit)
                return {200, R"({"created":123,"data":[{"b64_json":"AQIDBA=="}]})"};
            if (request.operation == GenerationOperation::Realtime)
            {
                callback(R"({"type":"response.output_audio.delta","delta":"AQIDBA=="})");
                callback(R"({"type":"response.output_audio_transcript.delta","delta":"synthetic voice"})");
                callback(R"({"type":"response.done","response":{"id":"voice_virtual","status":"completed"}})");
                return {200, {}};
            }
            if (request.operation == GenerationOperation::Speech)
            {
                auto wav = WavFromPcm(std::vector<uint8_t>{1, 2, 3, 4});
                return {200, {wav.begin(), wav.end()}};
            }
            return {200, R"({"text":"synthetic transcript"})"};
        }
    };
} // namespace
int main()
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        for (auto bytes : {std::vector<uint8_t>{0}, std::vector<uint8_t>{0, 1}, std::vector<uint8_t>{0, 1, 2},
                           std::vector<uint8_t>{255, 254, 253, 252}})
            Require(DecodeBase64(Base64(bytes)) == bytes);
        for (auto invalid : {"A===", "AQ=A", "AA==AAAA", "AB==", "AAA", "!!!!"})
            Reject([&] { (void)DecodeBase64(invalid); });
        std::vector<uint8_t> pcm{0, 1, 2, 3, 4, 5};
        Require(PcmFromWav(WavFromPcm(pcm)) == pcm);
        auto invalidWav = WavFromPcm(pcm);
        invalidWav[24] = 1;
        Reject([&] { (void)PcmFromWav(invalidWav); });
        Reject([&] { (void)PcmFromWav(std::vector<uint8_t>{1, 2}); });
        std::string collected;
        EventStream framing{[&](auto value) { collected += value; }};
        for (auto byte :
             std::string{"event: test\r\ndata: first\r\ndata: second\r\n\r\n:comment\r\ndata: [DONE]\r\n\r\n"})
            framing.Feed({&byte, 1});
        framing.Finish();
        Require(collected == "first\nsecond");
        std::cout << "PASS: base64, PCM/WAV validation, UTF-8-safe SSE framing.\n";

        auto transport = std::make_shared<VirtualTransport>();
        GenerationClient client{transport};
        std::wstring streamed;
        auto chat = client.Chat(L"authorized-model", L"developer instruction", {{L"user", L"synthetic input", {}}},
                                L"{}", [&](auto const &text) { streamed = text; });
        Require(chat.text == L"測試" && streamed == chat.text && chat.id == L"resp_virtual");
        auto body = JsonObject::Parse(winrt::to_hstring(transport->requests.back().body));
        Require(!body.GetNamedBoolean(L"store") && body.GetNamedBoolean(L"stream") &&
                body.GetNamedString(L"instructions") == L"developer instruction");
        Require(transport->requests.back().operation == GenerationOperation::Response);
        Attachment image{L"reference.png", L"image/png", {1, 2, 3, 4}};
        (void)client.Chat(L"model", L"", {{L"user", L"describe", {image}}},
                          L"{\"text\":{\"format\":{\"type\":\"json_object\"}}}", {}, {});
        auto multimodal = JsonObject::Parse(winrt::to_hstring(transport->requests.back().body));
        Require(multimodal.GetNamedArray(L"input")
                    .GetObjectAt(0)
                    .GetNamedArray(L"content")
                    .GetObjectAt(1)
                    .GetNamedString(L"image_url") == L"data:image/png;base64,AQIDBA==");
        for (auto options : {L"{\"input\":\"override\"}", L"{\"model\":\"override\"}", L"{\"stream\":false}", L"[]"})
            Reject([&] { (void)client.Chat(L"model", L"", {{L"user", L"x", {}}}, options, {}); });
        transport->truncated = true;
        Reject([&] { (void)client.Chat(L"model", L"", {{L"user", L"x", {}}}, L"{}", {}); });
        transport->truncated = false;
        for (unsigned status : {400u, 401u, 403u, 429u, 503u})
        {
            transport->status = status;
            Reject([&] { (void)client.Chat(L"model", L"", {{L"user", L"x", {}}}, L"{}", {}); });
        }
        transport->status = 200;
        std::stop_source stop;
        stop.request_stop();
        auto count = transport->requests.size();
        Reject([&] { (void)client.Chat(L"model", L"", {{L"user", L"x", {}}}, L"{}", {}, stop.get_token()); });
        Require(count == transport->requests.size());
        std::cout << "PASS: Responses streaming, multimodal inputs, protected fields, truncation, errors and "
                     "cancellation; no retries.\n";

        auto images = client.Images(L"image-model", L"synthetic prompt", L"{}", {});
        Require(images.assets.size() == 1 && images.assets[0].bytes == std::vector<uint8_t>({1, 2, 3, 4}));
        Require(images.details.find(L"b64_json") == std::wstring::npos);
        (void)client.Images(L"image-model", L"edit", L"{}", {image});
        Require(transport->requests.back().operation == GenerationOperation::ImageEdit);
        auto edit = JsonObject::Parse(winrt::to_hstring(transport->requests.back().body));
        Require(edit.GetNamedArray(L"images").Size() == 1);
        auto audio = client.Audio(GenerationOperation::Transcription, L"transcribe-model", L"context",
                                  L"{\"language\":\"zh\"}", {{L"recording.wav", L"audio/wav", WavFromPcm(pcm)}}, {});
        Require(audio.text == L"synthetic transcript" &&
                transport->requests.back().contentType.starts_with(L"multipart/form-data; boundary="));
        auto multipart = transport->requests.back().body;
        Require(multipart.find("name=\"file\"; filename=\"recording.wav\"") != std::string::npos &&
                multipart.find("name=\"language\"") != std::string::npos);
        Reject([&] {
            (void)client.Audio(GenerationOperation::Transcription, L"model", L"", L"{}",
                               {{L"bad\r\nname", L"audio/wav", {1}}}, {});
        });
        auto speech =
            client.Audio(GenerationOperation::Speech, L"speech-model", L"test", L"{\"voice\":\"marin\"}", {}, {});
        Require(speech.assets[0].extension == L".wav");
        auto voice =
            client.Audio(GenerationOperation::Realtime, L"realtime-model", L"test", L"{\"voice\":\"marin\"}", {}, {});
        Require(voice.text == L"synthetic voice" &&
                PcmFromWav(voice.assets[0].bytes) == std::vector<uint8_t>({1, 2, 3, 4}));
        (void)client.Audio(GenerationOperation::Realtime, L"realtime-model", L"next turn", L"{}", {}, {}, {},
                           {{L"user", L"previous question", {}}, {L"assistant", L"previous answer", {}}});
        auto voiceRequest = JsonObject::Parse(winrt::to_hstring(transport->requests.back().body));
        Require(voiceRequest.GetNamedArray(L"history")
                    .GetObjectAt(1)
                    .GetNamedArray(L"content")
                    .GetObjectAt(0)
                    .GetNamedString(L"type") == L"output_text");
        auto before = transport->requests.size();
        Reject([&] {
            (void)client.Images(L"model", L"prompt", L"{\"background\":\"transparent\",\"output_format\":\"jpeg\"}",
                                {});
        });
        Require(before == transport->requests.size());
        Require(GenerationPath(GenerationOperation::Realtime) == L"/v1/realtime");
        std::cout << "PASS: image generation/editing, metadata, multipart transcription, speech and Realtime audio "
                     "event assembly.\n";

        Workflow flow{L"start",
                      {{L"start", L"start", L"Start", L"", L"", L"transform"},
                       {L"transform", L"transform", L"Format", L"prefix {{input}}", L"", L"condition"},
                       {L"condition", L"condition", L"Route", L"", L"", L"agent", L"end", L"prefix"},
                       {L"agent", L"agent", L"Agent", L"instructions", L"model", L"end"},
                       {L"end", L"end", L"End"}}};
        auto roundtrip = ParseWorkflow(SerializeWorkflow(flow));
        Require(roundtrip.nodes.size() == 5);
        auto answer = RunWorkflow(flow, L"input", client, {});
        Require(answer.text == L"測試");
        auto trace = JsonObject::Parse(answer.details).GetNamedArray(L"steps");
        Require(trace.Size() == 4 && trace.GetObjectAt(1).GetNamedString(L"output") == L"prefix input");
        auto invalid = flow;
        invalid.nodes[0].next = L"missing";
        Reject([&] { ValidateWorkflow(invalid); });
        invalid = flow;
        invalid.nodes.push_back(invalid.nodes[0]);
        Reject([&] { ValidateWorkflow(invalid); });
        invalid = flow;
        invalid.nodes[0].next = L"start";
        Reject([&] { (void)RunWorkflow(invalid, L"input", client, {}); });
        auto code = ExportWorkflowPython(flow);
        Require(code.find(L"from agents import Agent") != std::wstring::npos &&
                code.find(L"OPENAI_API_KEY") != std::wstring::npos);
        std::filesystem::create_directories("artifacts/tests/playground/exports");
        std::ofstream{"artifacts/tests/playground/exports/workflow.py"} << winrt::to_string(code);
        auto requestCode =
            GenerationClient::RequestCode(L"model", L"''' malicious delimiters", {{L"user", L"test", {}}}, L"{}");
        Require(requestCode.find(L"''' malicious delimiters") == std::wstring::npos);
        std::ofstream{"artifacts/tests/playground/exports/chat.py"} << winrt::to_string(requestCode);
        Require(!ValidateStoryboard(L"{\"version\":1,\"shots\":[{\"seconds\":8,\"prompt\":\"local plan\"}]}").empty());
        Reject([] { (void)ValidateStoryboard(L"{\"version\":1,\"shots\":[{\"seconds\":0,\"prompt\":\"invalid\"}]}"); });
        std::cout << "PASS: workflow graph validation, transformations, branching, execution traces, step bounds, safe "
                     "code export and local storyboards.\n";
        std::cout
            << "No live transport, credentials, network calls or microphone access are linked into these tests.\n";
        return 0;
    }
    catch (...)
    {
        std::cerr << "FAIL: Playground contract test.\n";
        return 1;
    }
}
