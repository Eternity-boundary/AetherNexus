// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PromptStore.cpp
// Project: AetherNexus
#include "PromptStore.h"
#include "BuildClient.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <objbase.h>
#include <set>
#include <thread>
#include <windows.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#pragma comment(lib, "Ole32.lib")
namespace AetherNexus::Services
{
using namespace winrt;
using namespace Windows::Data::Json;
namespace
{
void Require(bool value)
{
    if (!value)
        throw std::invalid_argument("Invalid local prompt document");
}
std::wstring NewId()
{
    GUID id{};
    check_hresult(CoCreateGuid(&id));
    wchar_t buffer[39]{};
    Require(StringFromGUID2(id, buffer, 39) == 39);
    return L"local_" + std::wstring{buffer + 1, 36};
}
void Set(JsonObject const &object, wchar_t const *key, std::wstring const &text)
{
    object.SetNamedValue(key, JsonValue::CreateStringValue(text));
}
void Validate(PromptRevision const &draft)
{
    Require(!draft.name.empty() && draft.name.size() <= 200 && draft.model.size() <= 256 &&
            draft.instructions.size() <= 65536 && draft.input.size() <= 65536 && draft.options.size() <= 65536);
    if (!draft.id.empty())
    {
        ValidateBuildIdentifier(draft.id);
        Require(draft.id.starts_with(L"local_"));
    }
    JsonObject options;
    Require(JsonObject::TryParse(draft.options, options));
    for (auto const &field : options)
        Require(field.Key() == L"temperature" || field.Key() == L"top_p" || field.Key() == L"reasoning" ||
                field.Key() == L"text" || field.Key() == L"max_output_tokens");
}
JsonObject Json(PromptRevision const &draft)
{
    JsonObject object;
    Set(object, L"id", draft.id);
    Set(object, L"name", draft.name);
    Set(object, L"model", draft.model);
    Set(object, L"instructions", draft.instructions);
    Set(object, L"input", draft.input);
    Set(object, L"options", draft.options);
    object.SetNamedValue(L"version", JsonValue::CreateNumberValue(static_cast<double>(draft.version)));
    object.SetNamedValue(L"modified", JsonValue::CreateNumberValue(static_cast<double>(draft.modified)));
    return object;
}
PromptRevision Parse(JsonObject const &object)
{
    PromptRevision result;
    result.id = object.GetNamedString(L"id", L"");
    result.name = object.GetNamedString(L"name", L"");
    result.model = object.GetNamedString(L"model", L"");
    result.instructions = object.GetNamedString(L"instructions", L"");
    result.input = object.GetNamedString(L"input", L"");
    result.options = object.GetNamedString(L"options", L"{}");
    auto version = object.GetNamedNumber(L"version", 0), modified = object.GetNamedNumber(L"modified", 0);
    Require(version >= 0 && version <= 100 && version == std::floor(version) && modified >= 0 &&
            modified <= 253402300799.0 && modified == std::floor(modified));
    result.version = static_cast<int64_t>(version);
    result.modified = static_cast<int64_t>(modified);
    Validate(result);
    return result;
}
std::vector<PromptRevision> ReadFile(std::filesystem::path const &path)
{
    if (!std::filesystem::exists(path))
        return {};
    auto bytes = std::filesystem::file_size(path);
    Require(bytes <= 8 * 1024 * 1024);
    std::ifstream file(path, std::ios::binary);
    Require(static_cast<bool>(file));
    std::string content(static_cast<size_t>(bytes), '\0');
    file.read(content.data(), static_cast<std::streamsize>(content.size()));
    Require(static_cast<size_t>(file.gcount()) == content.size());
    JsonObject root;
    Require(JsonObject::TryParse(to_hstring(content), root) && root.GetNamedNumber(L"schema") == 1);
    auto entries = root.GetNamedArray(L"revisions");
    Require(entries.Size() <= 10000);
    std::vector<PromptRevision> result;
    std::map<std::wstring, int64_t> versions;
    for (auto const &entry : entries)
    {
        auto revision = Parse(entry.GetObject());
        Require(!revision.id.empty() && revision.version == ++versions[revision.id]);
        Require(versions.size() <= 100);
        result.push_back(std::move(revision));
    }
    return result;
}
struct FileLock
{
    HANDLE handle{INVALID_HANDLE_VALUE};
    explicit FileLock(std::filesystem::path const &path)
    {
        std::filesystem::create_directories(path.parent_path());
        auto lock = path;
        lock += L".lock";
        for (int attempt = 0; attempt < 20; ++attempt)
        {
            handle = CreateFileW(lock.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
            if (handle != INVALID_HANDLE_VALUE)
                return;
            if (GetLastError() != ERROR_SHARING_VIOLATION)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds{25});
        }
        throw std::runtime_error("Local prompt catalog is busy");
    }
    ~FileLock()
    {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};
void WriteFile(std::filesystem::path const &path, std::vector<PromptRevision> const &entries)
{
    JsonObject root;
    root.SetNamedValue(L"schema", JsonValue::CreateNumberValue(1));
    JsonArray revisions;
    for (auto const &entry : entries)
        revisions.Append(Json(entry));
    root.SetNamedValue(L"revisions", revisions);
    auto content = to_string(root.Stringify());
    Require(content.size() <= 8 * 1024 * 1024);
    auto temporary = path;
    temporary += L"." + NewId() + L".tmp";
    try
    {
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            Require(static_cast<bool>(file));
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            file.flush();
            Require(static_cast<bool>(file));
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Local prompt catalog could not be committed");
    }
    catch (...)
    {
        DeleteFileW(temporary.c_str());
        throw;
    }
}
} // namespace
std::vector<PromptRevision> PromptStore::Read()
{
    std::lock_guard guard{m_mutex};
    return ReadFile(m_path);
}
PromptRevision PromptStore::Save(PromptRevision const &draft)
{
    Validate(draft);
    std::lock_guard guard{m_mutex};
    FileLock lock{m_path};
    auto entries = ReadFile(m_path);
    auto next = draft;
    int64_t latest{};
    std::set<std::wstring> ids;
    for (auto const &entry : entries)
    {
        ids.insert(entry.id);
        if (entry.id == draft.id)
            latest = entry.version;
    }
    if (next.id.empty())
    {
        Require(ids.size() < 100 && draft.version == 0);
        next.id = NewId();
    }
    else
        Require(latest > 0 && draft.version == latest); // Detect another editor or process saving
                                                        // a newer revision.
    Require(latest < 100);
    next.version = latest + 1;
    next.modified =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    entries.push_back(next);
    WriteFile(m_path, entries);
    return next;
}
void PromptStore::Remove(std::wstring const &id)
{
    ValidateBuildIdentifier(id);
    Require(id.starts_with(L"local_"));
    std::lock_guard guard{m_mutex};
    FileLock lock{m_path};
    auto entries = ReadFile(m_path);
    auto before = entries.size();
    std::erase_if(entries, [&](auto const &entry) { return entry.id == id; });
    Require(entries.size() < before);
    WriteFile(m_path, entries);
}
std::wstring SerializePrompt(PromptRevision const &draft)
{
    Validate(draft);
    return Json(draft).Stringify().c_str();
}
PromptRevision ImportPrompt(std::wstring const &json)
{
    Require(json.size() <= 256 * 1024);
    JsonObject object;
    Require(JsonObject::TryParse(json, object));
    // Import only supported configuration. Legacy tools, credentials and
    // executable code are not executed or persisted.
    PromptRevision result;
    result.name = object.GetNamedString(L"name", L"");
    result.model = object.GetNamedString(L"model", L"");
    result.instructions = object.GetNamedString(L"instructions", L"");
    result.input = object.GetNamedString(L"input", L"");
    result.options = object.GetNamedString(L"options", L"{}");
    Validate(result);
    return result;
}
std::wstring RenderPromptVariables(std::wstring const &text, std::wstring const &variablesJson)
{
    Require(text.size() <= 65536 && variablesJson.size() <= 65536);
    JsonObject variables;
    Require(JsonObject::TryParse(variablesJson, variables) && variables.Size() <= 32);
    for (auto const &entry : variables)
        Require(entry.Key().size() <= 64 && entry.Value().ValueType() == JsonValueType::String);
    std::wstring result;
    size_t start{};
    while (start < text.size())
    {
        auto open = text.find(L"{{", start);
        if (open == std::wstring::npos)
        {
            result += text.substr(start);
            break;
        }
        result += text.substr(start, open - start);
        auto close = text.find(L"}}", open + 2);
        Require(close != std::wstring::npos);
        auto name = text.substr(open + 2, close - open - 2);
        Require(!name.empty() && name.size() <= 64 && variables.HasKey(name));
        result += variables.GetNamedString(name).c_str();
        Require(result.size() <= 65536);
        start = close + 2;
    }
    Require(result.size() <= 65536);
    return result; // Single substitution pass, never evaluates inserted template
                   // syntax.
}
} // namespace AetherNexus::Services
