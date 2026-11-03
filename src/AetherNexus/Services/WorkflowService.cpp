// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: WorkflowService.cpp
// Project: AetherNexus
#include "WorkflowService.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace AetherNexus::Services
{
    using namespace winrt;
    using namespace Windows::Data::Json;
    namespace
    {
        void Require(bool condition)
        {
            if (!condition)
                throw std::invalid_argument("Invalid workflow");
        }
        void Set(JsonObject const &value, wchar_t const *key, std::wstring const &text)
        {
            value.SetNamedValue(key, JsonValue::CreateStringValue(text));
        }
        std::wstring Transform(std::wstring text, std::wstring const &input)
        {
            constexpr std::wstring_view marker{L"{{input}}"};
            size_t offset = 0;
            while ((offset = text.find(marker, offset)) != std::wstring::npos)
            {
                text.replace(offset, marker.size(), input);
                offset += input.size();
                Require(text.size() <= 1024 * 1024);
            }
            return text;
        }
    } // namespace
    void ValidateWorkflow(Workflow const &flow)
    {
        Require(!flow.nodes.empty() && flow.nodes.size() <= 64);
        std::set<std::wstring> ids;
        for (auto const &node : flow.nodes)
        {
            Require(!node.id.empty() && node.id.size() <= 128 && ids.insert(node.id).second);
            Require(node.kind == L"start" || node.kind == L"agent" || node.kind == L"condition" ||
                    node.kind == L"transform" || node.kind == L"end");
            Require(node.instructions.size() <= 1024 * 1024 && node.name.size() <= 256 && std::isfinite(node.x) &&
                    std::isfinite(node.y) && node.x >= 0 && node.x <= 10000 && node.y >= 0 && node.y <= 10000);
            if (node.kind == L"agent")
                Require(!node.model.empty() && node.model.size() <= 256);
        }
        Require(ids.contains(flow.entry));
        auto entry =
            std::find_if(flow.nodes.begin(), flow.nodes.end(), [&](auto const &node) { return node.id == flow.entry; });
        Require(entry->kind == L"start");
        for (auto const &node : flow.nodes)
        {
            if (node.kind != L"end")
                Require(ids.contains(node.next));
            if (node.kind == L"condition")
                Require(ids.contains(node.otherwise));
        }
        // Cycles are supported but preview execution is limited to 64 steps.
    }
    Workflow ParseWorkflow(std::wstring const &text)
    {
        Require(text.size() <= 2 * 1024 * 1024);
        auto object = JsonObject::Parse(text);
        Require(object.GetNamedNumber(L"version") == 1);
        Workflow flow;
        flow.entry = object.GetNamedString(L"entry");
        for (auto const &value : object.GetNamedArray(L"nodes"))
        {
            auto node = value.GetObject();
            flow.nodes.push_back(
                {std::wstring{node.GetNamedString(L"id")}, std::wstring{node.GetNamedString(L"kind")},
                 std::wstring{node.GetNamedString(L"name", L"")},
                 std::wstring{node.GetNamedString(L"instructions", L"")},
                 std::wstring{node.GetNamedString(L"model", L"")}, std::wstring{node.GetNamedString(L"next", L"")},
                 std::wstring{node.GetNamedString(L"otherwise", L"")}, std::wstring{node.GetNamedString(L"match", L"")},
                 node.GetNamedNumber(L"x", 0), node.GetNamedNumber(L"y", 0)});
        }
        ValidateWorkflow(flow);
        return flow;
    }
    std::wstring SerializeWorkflow(Workflow const &flow)
    {
        JsonObject result;
        result.SetNamedValue(L"version", JsonValue::CreateNumberValue(1));
        Set(result, L"entry", flow.entry);
        JsonArray nodes;
        for (auto const &node : flow.nodes)
        {
            JsonObject value;
            Set(value, L"id", node.id);
            Set(value, L"kind", node.kind);
            Set(value, L"name", node.name);
            Set(value, L"instructions", node.instructions);
            Set(value, L"model", node.model);
            Set(value, L"next", node.next);
            Set(value, L"otherwise", node.otherwise);
            Set(value, L"match", node.match);
            value.SetNamedValue(L"x", JsonValue::CreateNumberValue(node.x));
            value.SetNamedValue(L"y", JsonValue::CreateNumberValue(node.y));
            nodes.Append(value);
        }
        result.SetNamedValue(L"nodes", nodes);
        return result.Stringify().c_str();
    }
    GenerationResult RunWorkflow(Workflow const &flow, std::wstring input, GenerationClient &client,
                                 TextCallback const &callback, std::stop_token stop)
    {
        ValidateWorkflow(flow);
        std::wstring current = flow.entry;
        JsonArray trace;
        GenerationResult result;
        for (int step = 0; step < 64; ++step)
        {
            CheckCancelled(stop);
            auto found = std::find_if(flow.nodes.begin(), flow.nodes.end(),
                                      [&](auto const &node) { return node.id == current; });
            auto const &node = *found;
            if (callback)
                callback(L"[" + node.name + L" · " + node.id + L"]\n" + input);
            if (node.kind == L"end")
            {
                result.text = input;
                JsonObject details;
                details.SetNamedValue(L"steps", trace);
                result.details = details.Stringify();
                return result;
            }
            JsonObject event;
            Set(event, L"node", node.id);
            Set(event, L"kind", node.kind);
            if (node.kind == L"agent")
            {
                auto answer = client.Chat(
                    node.model, node.instructions, {{L"user", input, {}}}, L"{}",
                    [&](auto const &text) {
                        if (callback)
                            callback(L"[" + node.name + L" · " + node.id + L"]\n" + text);
                    },
                    stop);
                if (answer.incomplete)
                    throw ApiError{ApiErrorCode::InvalidResponse};
                input = answer.text;
                Set(event, L"response_id", answer.id);
            }
            else if (node.kind == L"transform")
                input = Transform(node.instructions, input);
            bool const branch = node.kind == L"condition" && input.find(node.match) == std::wstring::npos;
            current = branch ? node.otherwise : node.next;
            Set(event, L"output", input);
            Set(event, L"next", current);
            trace.Append(event);
        }
        throw std::invalid_argument("Workflow exceeded step limit");
    }
    std::wstring ExportWorkflowPython(Workflow const &flow)
    {
        ValidateWorkflow(flow);
        auto json = to_string(SerializeWorkflow(flow));
        auto encoded = Base64({reinterpret_cast<uint8_t const *>(json.data()), json.size()});
        return L"# Install: pip install openai-agents\n# Set OPENAI_API_KEY in your environment. No key is "
               L"embedded.\nimport asyncio, base64, json\nfrom agents import Agent, Runner, ModelSettings\n\nworkflow "
               L"= "
               L"json.loads(base64.b64decode(\"" +
               std::wstring{to_hstring(encoded)} + LR"("))

async def run_workflow(text):
    nodes = {n['id']: n for n in workflow['nodes']}
    current = workflow['entry']
    for _ in range(64):
        node = nodes[current]
        kind = node['kind']
        if kind == 'end':
            return text
        if kind == 'agent':
            agent = Agent(name=node['name'] or node['id'], instructions=node['instructions'],
                          model=node['model'], model_settings=ModelSettings(store=False))
            result = await Runner.run(agent, text)
            text = str(result.final_output)
        elif kind == 'transform':
            count = node['instructions'].count('{{input}}')
            if len(node['instructions']) + count * (len(text) - len('{{input}}')) > 1048576:
                raise ValueError('Transform too large')
            text = node['instructions'].replace('{{input}}', text)
            if len(text) > 1048576:
                raise ValueError('Transform too large')
        current = node['otherwise'] if kind == 'condition' and node['match'] not in text else node['next']
    raise ValueError('Workflow exceeded 64 steps')

if __name__ == '__main__':
    print(asyncio.run(run_workflow(input('Input: '))))
)";
    }
    std::wstring ValidateStoryboard(std::wstring const &text)
    {
        Require(text.size() <= 2 * 1024 * 1024);
        auto object = JsonObject::Parse(text);
        Require(object.GetNamedNumber(L"version") == 1);
        auto shots = object.GetNamedArray(L"shots");
        Require(shots.Size() <= 100);
        for (auto const &shot : shots)
        {
            auto value = shot.GetObject();
            auto seconds = value.GetNamedNumber(L"seconds");
            Require(std::isfinite(seconds) && seconds > 0 && seconds <= 600 &&
                    !value.GetNamedString(L"prompt").empty());
        }
        return object.Stringify().c_str();
    }
} // namespace AetherNexus::Services
