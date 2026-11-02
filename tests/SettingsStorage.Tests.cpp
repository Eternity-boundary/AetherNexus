// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: SettingsStorage.Tests.cpp
// Project: 其他檔案
#include "CredentialStore.h"
#include <windows.h>
#include <wincred.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

using namespace AetherNexus::Infrastructure;
namespace
{
    constexpr auto modelValue = L"synthetic-model-token-for-local-tests-only";
    constexpr auto adminValue = L"synthetic-admin-token-for-local-tests-only";
    void Require(bool value) { if (!value) throw std::runtime_error("Test failed"); }
    struct CredentialDeleter
    {
        void operator()(CREDENTIALW* value) const noexcept
        {
            if (value->CredentialBlob) SecureZeroMemory(value->CredentialBlob, value->CredentialBlobSize);
            CredFree(value);
        }
    };
    void Check(std::wstring const& target, std::wstring_view expected)
    {
        CREDENTIALW* raw{};
        Require(CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &raw) != FALSE);
        std::unique_ptr<CREDENTIALW, CredentialDeleter> credential{raw};
        Require(raw->Type == CRED_TYPE_GENERIC);
        Require(raw->Persist == CRED_PERSIST_LOCAL_MACHINE);
        Require(raw->CredentialBlobSize == expected.size() * sizeof(wchar_t));
        Require(std::memcmp(raw->CredentialBlob, expected.data(), raw->CredentialBlobSize) == 0);
    }
    struct Cleanup
    {
        CredentialStore const& store;
        ~Cleanup()
        {
            try { store.Remove(TokenKind::Model); store.Remove(TokenKind::Admin); }
            catch (...) { std::cerr << "Could not clean up isolated test credentials.\n"; }
        }
    };
    void CheckNewProcess(std::wstring const& prefix)
    {
        wchar_t executable[32768]{};
        Require(GetModuleFileNameW(nullptr, executable, 32768) != 0);
        auto command = L"\"" + std::wstring(executable) + L"\" --verify " + prefix;
        STARTUPINFOW startup{sizeof(STARTUPINFOW)};
        PROCESS_INFORMATION process{};
        Require(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
            nullptr, nullptr, &startup, &process) != FALSE);
        CloseHandle(process.hThread);
        auto const result = WaitForSingleObject(process.hProcess, 15000);
        DWORD exitCode{1};
        bool const read = GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
        CloseHandle(process.hProcess);
        Require(result == WAIT_OBJECT_0 && read && exitCode == 0);
    }
}

int wmain(int argc, wchar_t** argv)
{
    static_assert(!std::is_copy_constructible_v<SecureToken>);
    try
    {
        if (argc == 3 && std::wstring_view{argv[1]} == L"--verify")
        {
            std::wstring prefix{argv[2]};
            Require(prefix.starts_with(L"AetherNexus.Tests/"));
            CredentialStore store{prefix};
            Require(store.Has(TokenKind::Model) && store.Has(TokenKind::Admin));
            Check(prefix + L"model-api-key", modelValue);
            Check(prefix + L"admin-api-key", adminValue);
            return 0;
        }
        Require(argc == 1);
        auto const prefix = L"AetherNexus.Tests/" + std::to_wstring(GetCurrentProcessId()) + L"-"
            + std::to_wstring(GetTickCount64()) + L"/";
        CredentialStore store{prefix};
        Cleanup cleanup{store};
        Require(!store.Has(TokenKind::Model) && !store.Has(TokenKind::Admin));
        {
            SecureToken model{modelValue};
            SecureToken admin{adminValue};
            store.Save(TokenKind::Model, model.View());
            store.Save(TokenKind::Admin, admin.View());
            model.Clear();
            admin.Clear();
            Require(model.View().empty() && admin.View().empty());
        }
        Check(prefix + L"model-api-key", modelValue);
        Check(prefix + L"admin-api-key", adminValue);
        {
            auto model = store.Read(TokenKind::Model);
            auto admin = store.Read(TokenKind::Admin);
            Require(model && admin && model->View() == modelValue && admin->View() == adminValue);
        }
        CheckNewProcess(prefix);
        std::cout << "PASS: distinct slots, local-user persistence, and fresh-process readback.\n";

        store.Save(TokenKind::Model, L"synthetic-replacement");
        Check(prefix + L"model-api-key", L"synthetic-replacement");
        Check(prefix + L"admin-api-key", adminValue);
        for (auto const& bad : {std::wstring{}, std::wstring{L"has space"}, std::wstring{L"has\nnewline"},
            std::wstring{L"nul\0byte", 8}, std::wstring(1281, L'x'), std::wstring{L"\u4e2d"}})
        {
            Require(!CredentialStore::IsValidInput(bad));
            bool rejected = false;
            try { store.Save(TokenKind::Model, bad); }
            catch (std::invalid_argument const&) { rejected = true; }
            Require(rejected);
            Check(prefix + L"model-api-key", L"synthetic-replacement");
        }
        std::cout << "PASS: replacement isolation and rejection without changing stored data.\n";
        std::wstring boundary(1280, L'x');
        Require(CredentialStore::IsValidInput(boundary));
        store.Save(TokenKind::Model, boundary);
        Check(prefix + L"model-api-key", boundary);
        std::cout << "PASS: maximum supported credential size.\n";
        store.Remove(TokenKind::Model);
        Require(!store.Has(TokenKind::Model) && store.Has(TokenKind::Admin));
        Require(!store.Read(TokenKind::Model));
        store.Remove(TokenKind::Model);
        store.Remove(TokenKind::Admin);
        Require(!store.Has(TokenKind::Admin));
        std::cout << "PASS: independent removal, repeated removal, and test cleanup.\n";
        return 0;
    }
    catch (...)
    {
        std::cerr << "FAIL: storage integration test (no credential contents logged).\n";
        return 1;
    }
}
