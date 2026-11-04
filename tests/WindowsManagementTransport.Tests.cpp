// SPDX-License-Identifier: GPL-3.0-only
// Exercises the actual transport with tests-only WinHTTP and credential implementations.
// No socket is created and no Windows credential API is linked or called.
#include <windows.h>
#include <winhttp.h>
#include <winrt/base.h>
#include "Services/WindowsManagementTransport.h"
#include "Infrastructure/CredentialStore.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace Boundary
{
    constexpr std::wstring_view fakeToken = L"offline-boundary-never-valid";
    constexpr std::wstring_view keyId = L"key_offline_boundary";
    constexpr std::wstring_view keyPath = L"/v1/organization/admin_api_keys/key_offline_boundary";
    constexpr char adminJson[] =
        R"({"object":"organization.admin_api_key","id":"key_offline_boundary","redacted_value":"...5678"})";
    constexpr char deletedJson[] =
        R"({"object":"organization.admin_api_key.deleted","id":"key_offline_boundary","deleted":true})";

    void Require(bool value, char const *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    struct Response
    {
        DWORD status{200};
        std::string body;
        std::map<std::wstring, std::wstring> headers;
        std::wstring contentType{L"application/json"};
        size_t maximumReadChunk{11};
    };
    struct Request
    {
        std::wstring method, path, headers;
        DWORD flags{}, disabledFeatures{}, optionalLength{}, totalLength{};
        bool optionalIsNull{};
        unsigned sends{}, receives{}, reads{};
        size_t readOffset{};
        Response response;
    };
    enum class HandleKind { Session, Connection, Request };
    struct Handle
    {
        HandleKind kind;
        std::shared_ptr<Request> request;
        bool closed{};
    };
    struct State
    {
        std::deque<Response> replies;
        std::vector<std::shared_ptr<Request>> requests;
        std::vector<std::unique_ptr<Handle>> handles;
        unsigned credentialReads{};
        std::wstring server;
        INTERNET_PORT port{};
    };
    State state;

    void AssertClosed()
    {
        Require(std::all_of(state.handles.begin(), state.handles.end(),
                            [](auto const &handle) { return handle->closed; }),
                "Transport leaked a shim handle");
    }
    void Reset(std::initializer_list<Response> replies)
    {
        AssertClosed();
        state = State{};
        state.replies.assign(replies.begin(), replies.end());
    }
    Handle &Get(HINTERNET value)
    {
        auto handle = static_cast<Handle *>(value);
        Require(handle && !handle->closed, "Transport used a closed shim handle");
        return *handle;
    }
    HINTERNET MakeHandle(HandleKind kind, std::shared_ptr<Request> request = {})
    {
        auto handle = std::make_unique<Handle>(Handle{kind, std::move(request)});
        auto value = handle.get();
        state.handles.push_back(std::move(handle));
        return value;
    }
    Request &GetRequest(HINTERNET value)
    {
        auto &handle = Get(value);
        Require(handle.kind == HandleKind::Request && handle.request, "Expected a shim request handle");
        return *handle.request;
    }
    std::wstring HeaderText(LPCWSTR text, DWORD length)
    {
        if (!text)
            return {};
        return length == static_cast<DWORD>(-1L) ? std::wstring{text} : std::wstring{text, length};
    }
    HINTERNET HttpOpen(LPCWSTR agent, DWORD accessType, LPCWSTR, LPCWSTR, DWORD flags)
    {
        Require(agent && std::wstring_view{agent} == L"AetherNexus/1.0", "Unexpected user agent");
        Require(accessType == WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY && flags == 0,
                "Unexpected session mode");
        return MakeHandle(HandleKind::Session);
    }
    BOOL HttpCloseHandle(HINTERNET value)
    {
        Get(value).closed = true;
        return TRUE;
    }
    BOOL HttpSetTimeouts(HINTERNET value, int, int, int, int)
    {
        Require(Get(value).kind == HandleKind::Session, "Timeouts were set on the wrong handle");
        return TRUE;
    }
    HINTERNET HttpConnect(HINTERNET session, LPCWSTR server, INTERNET_PORT port, DWORD)
    {
        Require(Get(session).kind == HandleKind::Session, "Expected a shim session");
        state.server = server ? server : L"";
        state.port = port;
        return MakeHandle(HandleKind::Connection);
    }
    HINTERNET HttpOpenRequest(HINTERNET connection, LPCWSTR method, LPCWSTR path, LPCWSTR,
                              LPCWSTR, LPCWSTR const *, DWORD flags)
    {
        Require(Get(connection).kind == HandleKind::Connection, "Expected a shim connection");
        Require(!state.replies.empty(), "Unexpected extra request or automatic retry");
        auto request = std::make_shared<Request>();
        request->method = method ? method : L"";
        request->path = path ? path : L"";
        request->flags = flags;
        request->response = std::move(state.replies.front());
        state.replies.pop_front();
        state.requests.push_back(request);
        return MakeHandle(HandleKind::Request, std::move(request));
    }
    BOOL HttpSetOption(HINTERNET value, DWORD option, LPVOID buffer, DWORD length)
    {
        auto &request = GetRequest(value);
        if (option == WINHTTP_OPTION_DISABLE_FEATURE)
        {
            Require(buffer && length == sizeof(DWORD), "Invalid disabled-feature option");
            request.disabledFeatures = *static_cast<DWORD *>(buffer);
        }
        return TRUE;
    }
    BOOL HttpAddRequestHeaders(HINTERNET value, LPCWSTR headers, DWORD length, DWORD)
    {
        GetRequest(value).headers += HeaderText(headers, length);
        return TRUE;
    }
    BOOL HttpSendRequest(HINTERNET value, LPCWSTR headers, DWORD headerLength, LPVOID optional,
                         DWORD optionalLength, DWORD totalLength, DWORD_PTR)
    {
        auto &request = GetRequest(value);
        request.headers += HeaderText(headers, headerLength);
        request.optionalIsNull = optional == nullptr;
        request.optionalLength = optionalLength;
        request.totalLength = totalLength;
        ++request.sends;
        return TRUE;
    }
    BOOL HttpReceiveResponse(HINTERNET value, LPVOID)
    {
        ++GetRequest(value).receives;
        return TRUE;
    }
    BOOL WriteHeader(std::wstring const &text, LPVOID buffer, LPDWORD length)
    {
        Require(length, "Missing header buffer length");
        auto needed = static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t));
        if (!buffer || *length < needed)
        {
            *length = needed;
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        std::memcpy(buffer, text.c_str(), needed);
        *length = static_cast<DWORD>(text.size() * sizeof(wchar_t));
        return TRUE;
    }
    BOOL HttpQueryHeaders(HINTERNET value, DWORD level, LPCWSTR name, LPVOID buffer,
                          LPDWORD length, LPDWORD)
    {
        auto &response = GetRequest(value).response;
        auto query = level & 0xffff;
        if (query == WINHTTP_QUERY_STATUS_CODE && (level & WINHTTP_QUERY_FLAG_NUMBER))
        {
            Require(buffer && length && *length >= sizeof(DWORD), "Invalid status buffer");
            *static_cast<DWORD *>(buffer) = response.status;
            *length = sizeof(DWORD);
            return TRUE;
        }
        if (query == WINHTTP_QUERY_CONTENT_TYPE)
            return WriteHeader(response.contentType, buffer, length);
        if (query == WINHTTP_QUERY_CONTENT_LENGTH)
            return WriteHeader(std::to_wstring(response.body.size()), buffer, length);
        if (query == WINHTTP_QUERY_CUSTOM && name)
        {
            auto found = response.headers.find(name);
            if (found != response.headers.end())
                return WriteHeader(found->second, buffer, length);
        }
        SetLastError(ERROR_WINHTTP_HEADER_NOT_FOUND);
        return FALSE;
    }
    BOOL HttpReadData(HINTERNET value, LPVOID buffer, DWORD requested, LPDWORD received)
    {
        auto &request = GetRequest(value);
        Require(buffer && received, "Invalid read buffer");
        auto count = std::min({static_cast<size_t>(requested), request.response.maximumReadChunk,
                               request.response.body.size() - request.readOffset});
        std::memcpy(buffer, request.response.body.data() + request.readOffset, count);
        request.readOffset += count;
        *received = static_cast<DWORD>(count);
        ++request.reads;
        return TRUE;
    }
}

namespace AetherNexus::Infrastructure
{
    // Deliberately not linked to CredentialStore.cpp or to any Windows credential API.
    SecureToken::SecureToken(std::wstring_view value) : m_value(value.begin(), value.end()) {}
    SecureToken::~SecureToken() { Clear(); }
    void SecureToken::Clear() noexcept
    {
        if (!m_value.empty())
            SecureZeroMemory(m_value.data(), m_value.size() * sizeof(wchar_t));
        m_value.clear();
    }
    CredentialStore::CredentialStore() : m_prefix(L"offline-transport-boundary-only") {}
    std::unique_ptr<SecureToken> CredentialStore::Read(TokenKind kind) const
    {
        Boundary::Require(kind == TokenKind::Admin, "Transport requested the wrong synthetic credential");
        ++Boundary::state.credentialReads;
        return std::make_unique<SecureToken>(Boundary::fakeToken);
    }
}

// Keep these mappings exhaustive. The runner rejects any newly added unmapped WinHTTP call.
#define WinHttpOpen ::Boundary::HttpOpen
#define WinHttpCloseHandle ::Boundary::HttpCloseHandle
#define WinHttpSetTimeouts ::Boundary::HttpSetTimeouts
#define WinHttpConnect ::Boundary::HttpConnect
#define WinHttpOpenRequest ::Boundary::HttpOpenRequest
#define WinHttpSetOption ::Boundary::HttpSetOption
#define WinHttpAddRequestHeaders ::Boundary::HttpAddRequestHeaders
#define WinHttpSendRequest ::Boundary::HttpSendRequest
#define WinHttpReceiveResponse ::Boundary::HttpReceiveResponse
#define WinHttpQueryHeaders ::Boundary::HttpQueryHeaders
#define WinHttpReadData ::Boundary::HttpReadData
#include "../src/AetherNexus/Services/WindowsManagementTransport.cpp"
#undef WinHttpOpen
#undef WinHttpCloseHandle
#undef WinHttpSetTimeouts
#undef WinHttpConnect
#undef WinHttpOpenRequest
#undef WinHttpSetOption
#undef WinHttpAddRequestHeaders
#undef WinHttpSendRequest
#undef WinHttpReceiveResponse
#undef WinHttpQueryHeaders
#undef WinHttpReadData

namespace Boundary
{
    using namespace AetherNexus::Services;
    ManagementRequest RevokeRequest()
    {
        ManagementRecord record;
        record.id = keyId;
        record.keyKind = ManagementKeyKind::Admin;
        return RevokeAdminKeyRequest(record);
    }
    std::wstring ClientRequestId(Request const &request)
    {
        constexpr std::wstring_view header = L"X-Client-Request-Id: ";
        auto start = request.headers.find(header);
        Require(start != std::wstring::npos && request.headers.find(header, start + 1) == std::wstring::npos,
                "Expected one locally generated client request-ID header");
        start += header.size();
        auto end = request.headers.find(L"\r\n", start);
        Require(end != std::wstring::npos, "Client request-ID header was not terminated");
        auto value = request.headers.substr(start, end - start);
        Require(!value.empty() && ManagementRequestId(value) == value, "Invalid local client request ID");
        return value;
    }
    void AssertNativeRequest(Request const &request, std::wstring_view method)
    {
        Require(state.server == L"api.openai.com" && state.port == INTERNET_DEFAULT_HTTPS_PORT,
                "Unexpected transport destination");
        Require(request.method == method && request.path == keyPath,
                "Native method or resource path differs from the contract");
        Require((request.flags & WINHTTP_FLAG_SECURE) != 0, "HTTPS was not enabled");
        DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
        Require((request.disabledFeatures & disabled) == disabled, "Required request options were omitted");
        Require(request.headers.find(L"Accept: application/json\r\n") != std::wstring::npos &&
                    request.headers.find(L"Content-Type: application/json\r\n") != std::wstring::npos,
                "JSON request headers were omitted");
        auto authorization = L"Authorization: Bearer " + std::wstring{fakeToken} + L"\r\n";
        auto first = request.headers.find(authorization);
        Require(first != std::wstring::npos && request.headers.find(authorization, first + 1) == std::wstring::npos,
                "Expected exactly one synthetic Authorization header");
        Require(request.optionalIsNull && request.optionalLength == 0 && request.totalLength == 0,
                "GET or DELETE unexpectedly sent an entity body");
        Require(request.sends == 1 && request.receives == 1, "Request was sent or received more than once");
        (void)ClientRequestId(request);
    }
    void AssertExchangePair()
    {
        Require(state.requests.size() == 2 && state.replies.empty(), "Expected GET followed by one DELETE");
        AssertNativeRequest(*state.requests[0], L"GET");
        AssertNativeRequest(*state.requests[1], L"DELETE");
        Require(state.requests[0]->path == state.requests[1]->path, "DELETE changed the verified ID");
        Require(ClientRequestId(*state.requests[0]) != ClientRequestId(*state.requests[1]),
                "GET and DELETE reused a client request ID");
        Require(state.credentialReads == 2, "Expected only synthetic credential reads");
        AssertClosed();
    }
    void TestDelete404()
    {
        Reset({{200, adminJson},
               {404, R"({"error":{"message":"private fixture body never rendered","type":"invalid_request_error","code":null}})",
                {{L"openai-version", L"offline-fixture-only"}}}});
        auto transport = std::make_shared<WindowsManagementTransport>();
        ManagementClient client{transport};
        bool classified = false;
        std::wstring clientRequestId;
        try
        {
            (void)client.Change(RevokeRequest());
        }
        catch (ManagementHttpError const &error)
        {
            classified = error.status == 404 && error.operation == ManagementOperation::AdminRevocation &&
                error.requestId.empty() && error.diagnostics.requestIdState == ResponseRequestIdState::Missing &&
                error.diagnostics.content == ResponseContentKind::Json &&
                error.diagnostics.body.kind == ResponseBodyKind::JsonError &&
                error.diagnostics.body.type == ResponseErrorType::InvalidRequest && error.diagnostics.openAiMetadata &&
                !error.diagnostics.clientRequestId.empty() &&
                std::string_view{error.what()}.find("private") == std::string_view::npos;
            clientRequestId = error.diagnostics.clientRequestId;
        }
        Require(classified, "Native DELETE 404 lost its operation or missing-request-ID state");
        AssertExchangePair();
        Require(clientRequestId == ClientRequestId(*state.requests[1]),
                "DELETE diagnostic did not retain the sent client request ID");
        std::cout << "PASS: actual transport sends GET then DELETE on the same HTTPS path, JSON headers, synthetic auth, empty body, no retries; DELETE 404 has no invented request ID.\n";
    }
    void TestSuccessfulNativeExchange()
    {
        Reset({{200, adminJson}, {200, deletedJson}});
        auto transport = std::make_shared<WindowsManagementTransport>();
        ManagementClient client{transport};
        Require(client.Change(RevokeRequest()).id == keyId, "Native DELETE success was not parsed");
        AssertExchangePair();
        Require(state.requests[0]->reads > 1 && state.requests[1]->reads > 1,
                "Chunked shim responses did not exercise the production read loop");
        std::cout << "PASS: production transport read loop parses successful GET/DELETE responses from short synthetic chunks.\n";
    }
    void TestHtmlAndEmpty404()
    {
        for (bool html : {true, false})
        {
            Response failure;
            failure.status = 404;
            failure.body = html ? "<!DOCTYPE html><html><body>private fixture page</body></html>" : "";
            failure.contentType = html ? L"text/html; charset=utf-8" : L"application/json";
            Reset({{200, adminJson}, failure});
            auto transport = std::make_shared<WindowsManagementTransport>();
            ManagementClient client{transport};
            bool classified = false;
            try
            {
                (void)client.Change(RevokeRequest());
            }
            catch (ManagementHttpError const &error)
            {
                classified = error.status == 404 && error.operation == ManagementOperation::AdminRevocation &&
                    error.requestId.empty() && error.diagnostics.requestIdState == ResponseRequestIdState::Missing &&
                    error.diagnostics.content == (html ? ResponseContentKind::Html : ResponseContentKind::Json) &&
                    error.diagnostics.body.kind == (html ? ResponseBodyKind::HtmlLike : ResponseBodyKind::Empty) &&
                    !error.diagnostics.openAiMetadata && !error.diagnostics.clientRequestId.empty() &&
                    std::string_view{error.what()}.find("private") == std::string_view::npos;
            }
            Require(classified, "HTML or empty native 404 lost its safe response-shape facts");
            AssertExchangePair();
        }
        std::cout << "PASS: native HTML and empty DELETE 404 responses retain status and bounded facts without raw text or invented origin.\n";
    }
    void TestResponseHeaders()
    {
        auto request = RevokeRequest();
        request.method = ManagementMethod::Get;
        auto validId = L"550e8400-e29b-41d4-a716-446655440000";
        Reset({{200, adminJson, {{L"x-request-id", validId}}}});
        WindowsManagementTransport transport;
        auto validResponse = transport.Send(request, {});
        Require(validResponse.requestId == validId && validResponse.diagnostics.requestIdState == ResponseRequestIdState::Valid,
                "Native request-ID header was not preserved");
        AssertClosed();
        for (auto const &[badId, expectedState] :
             {std::pair{std::wstring(129, L'a'), ResponseRequestIdState::TooLong},
              std::pair{std::wstring{L"req_bad\r\nprivate: header"}, ResponseRequestIdState::Rejected}})
        {
            Reset({{404, "<html><body>private response body</body></html>", {{L"x-request-id", badId}}, L"text/html"}});
            auto response = transport.Send(request, {});
            Require(response.status == 404 && response.requestId.empty() && response.body.empty() &&
                        response.diagnostics.requestIdState == expectedState &&
                        response.diagnostics.body.kind == ResponseBodyKind::HtmlLike,
                    "Unsafe native response header or raw body was retained");
            Require(state.requests.size() == 1 && state.replies.empty(), "Header handling retried the request");
            AssertNativeRequest(*state.requests[0], L"GET");
            AssertClosed();
        }
        std::cout << "PASS: custom response headers use native buffer-size semantics; unsafe or overlong request IDs are discarded.\n";
    }
    void TestBoundedErrorBody()
    {
        auto request = RevokeRequest();
        request.method = ManagementMethod::Get;
        Reset({{404, std::string(50000, 'x'), {}, L"text/plain", 8192}});
        WindowsManagementTransport transport;
        auto response = transport.Send(request, {});
        Require(response.status == 404 && response.body.empty() &&
                    response.diagnostics.body.kind == ResponseBodyKind::Truncated &&
                    state.requests[0]->readOffset == 32768,
                "Native error-body collection was not bounded or retained raw text");
        Require(state.requests.size() == 1 && state.replies.empty(), "Error-body collection retried a request");
        AssertClosed();
        std::cout << "PASS: production error-body reader stops at 32 KiB and exposes only a truncated-body fact.\n";
    }
}

int main()
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        Boundary::TestDelete404();
        Boundary::TestSuccessfulNativeExchange();
        Boundary::TestHtmlAndEmpty404();
        Boundary::TestResponseHeaders();
        Boundary::TestBoundedErrorBody();
        std::cout << "All WinHTTP APIs are shims; no sockets, real credentials or live OpenAI requests are used.\n";
    }
    catch (std::exception const &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
