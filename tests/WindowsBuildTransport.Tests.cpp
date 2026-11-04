// SPDX-License-Identifier: GPL-3.0-only
// Exercises the actual transport with tests-only WinHTTP and credential
// implementations. No socket is created and no Windows credential API is linked
// or called.
#include "Services/WindowsBuildTransport.h"
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
#include <windows.h>
#include <winhttp.h>
#include <winrt/base.h>

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
    std::string sent;
        wchar_t const *authorization{};
        DWORD authorizationLength{};
    DWORD flags{}, disabledFeatures{}, optionalLength{}, totalLength{};
    bool optionalIsNull{};
    unsigned sends{}, receives{}, reads{};
    size_t readOffset{};
    Response response;
};
enum class HandleKind
{
    Session,
    Connection,
    Request
};
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
        bool failRead{};
};
State state;

void AssertClosed()
{
    Require(std::all_of(state.handles.begin(), state.handles.end(), [](auto const &handle) { return handle->closed; }),
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
    Require(accessType == WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY && flags == 0, "Unexpected session mode");
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
HINTERNET HttpOpenRequest(HINTERNET connection, LPCWSTR method, LPCWSTR path, LPCWSTR, LPCWSTR, LPCWSTR const *,
                          DWORD flags)
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
BOOL HttpSendRequest(HINTERNET value, LPCWSTR headers, DWORD headerLength, LPVOID optional, DWORD optionalLength,
                     DWORD totalLength, DWORD_PTR)
{
    auto &request = GetRequest(value);
    request.headers += HeaderText(headers, headerLength);
        request.authorization = headers;
        request.authorizationLength = headerLength;
    request.optionalIsNull = optional == nullptr;
    request.optionalLength = optionalLength;
    request.totalLength = totalLength;
    if (optionalLength)
        request.sent.append(static_cast<char const *>(optional), optionalLength);
    ++request.sends;
    return TRUE;
}
BOOL HttpWriteData(HINTERNET value, LPCVOID data, DWORD count, LPDWORD written)
{
    GetRequest(value).sent.append(static_cast<char const *>(data), count);
    *written = count;
    return TRUE;
}
BOOL HttpReceiveResponse(HINTERNET value, LPVOID)
{
        auto &request = GetRequest(value);
        Require(request.authorization && std::all_of(request.authorization, request.authorization + request.authorizationLength,
                [](wchar_t value) { return value == 0; }), "Authorization buffer was not wiped after send");
        ++request.receives;
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
BOOL HttpQueryHeaders(HINTERNET value, DWORD level, LPCWSTR name, LPVOID buffer, LPDWORD length, LPDWORD)
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
        if (state.failRead && request.readOffset > 0)
        {
            SetLastError(ERROR_WINHTTP_CONNECTION_ERROR);
            return FALSE;
        }
    Require(buffer && received, "Invalid read buffer");
    auto count = std::min({static_cast<size_t>(requested), request.response.maximumReadChunk,
                           request.response.body.size() - request.readOffset});
    std::memcpy(buffer, request.response.body.data() + request.readOffset, count);
    request.readOffset += count;
    *received = static_cast<DWORD>(count);
    ++request.reads;
    return TRUE;
}
} // namespace Boundary

namespace AetherNexus::Infrastructure
{
// Deliberately not linked to CredentialStore.cpp or to any Windows credential
// API.
SecureToken::SecureToken(std::wstring_view value) : m_value(value.begin(), value.end())
{
}
SecureToken::~SecureToken()
{
    Clear();
}
void SecureToken::Clear() noexcept
{
    if (!m_value.empty())
        SecureZeroMemory(m_value.data(), m_value.size() * sizeof(wchar_t));
    m_value.clear();
}
CredentialStore::CredentialStore() : m_prefix(L"offline-transport-boundary-only")
{
}
std::unique_ptr<SecureToken> CredentialStore::Read(TokenKind kind) const
{
    Boundary::Require(kind == TokenKind::Model, "Transport requested the wrong synthetic credential");
    ++Boundary::state.credentialReads;
    return std::make_unique<SecureToken>(Boundary::fakeToken);
}
} // namespace AetherNexus::Infrastructure

#define WinHttpOpen ::Boundary::HttpOpen
#define WinHttpCloseHandle ::Boundary::HttpCloseHandle
#define WinHttpSetTimeouts ::Boundary::HttpSetTimeouts
#define WinHttpConnect ::Boundary::HttpConnect
#define WinHttpOpenRequest ::Boundary::HttpOpenRequest
#define WinHttpSetOption ::Boundary::HttpSetOption
#define WinHttpAddRequestHeaders ::Boundary::HttpAddRequestHeaders
#define WinHttpSendRequest ::Boundary::HttpSendRequest
#define WinHttpWriteData ::Boundary::HttpWriteData
#define WinHttpReceiveResponse ::Boundary::HttpReceiveResponse
#define WinHttpQueryHeaders ::Boundary::HttpQueryHeaders
#define WinHttpReadData ::Boundary::HttpReadData
#include "../src/AetherNexus/Services/WindowsBuildTransport.cpp"
#undef WinHttpOpen
#undef WinHttpCloseHandle
#undef WinHttpSetTimeouts
#undef WinHttpConnect
#undef WinHttpOpenRequest
#undef WinHttpSetOption
#undef WinHttpAddRequestHeaders
#undef WinHttpSendRequest
#undef WinHttpWriteData
#undef WinHttpReceiveResponse
#undef WinHttpQueryHeaders
#undef WinHttpReadData
#include <fstream>
int main()
{
    using namespace Boundary;
    using namespace AetherNexus::Services;
    try
    {
        winrt::init_apartment();
        WindowsBuildTransport transport;
        Reset({{200, "{\"data\":[]}"}});
        BuildRequest request;
        request.resource = BuildResource::Traces;
        request.parent = L"sess_offline";
        request.project = L"proj_offline";
        auto response = transport.Send(request, {});
        auto sent = state.requests.at(0);
        Require(response.body == "{\"data\":[]}" && state.credentialReads == 1 && sent->sends == 1,
                "GET response or credential scope failed");
        Require(state.server == L"api.openai.com" && state.port == 443 && (sent->flags & WINHTTP_FLAG_SECURE),
                "HTTPS destination changed");
        Require(sent->headers.find(L"OpenAI-Beta: agents=v1") != std::wstring::npos &&
                    sent->headers.find(L"OpenAI-Project: proj_offline") != std::wstring::npos,
                "Required headers absent");
        Require((sent->disabledFeatures &
                 (WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION)) ==
                    (WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION),
                "Ambient HTTP features not disabled");
        AssertClosed();
        Reset({{404, "private error body"}});
        request = {};
        request.resource = BuildResource::Batches;
        request.item = L"batch_offline";
        response = transport.Send(request, {});
        Require(response.status == 404 && response.body.empty() && state.requests[0]->reads == 0,
                "Error body retained");
        AssertClosed();
        auto root = std::filesystem::absolute(L"artifacts/tests/build-transport/files");
        std::filesystem::create_directories(root);
        auto source = root / L"offline.jsonl", destination = root / L"download.jsonl";
        std::string payload(150000, 'x');
        payload += "\n";
        {
            std::ofstream stream(source, std::ios::binary);
            stream << payload;
        }
        Reset({{200, "{\"id\":\"file_offline\"}"}});
        request = {};
        request.resource = BuildResource::Files;
        request.method = BuildMethod::Post;
        request.upload = BuildUpload{source, L"bad\"\r\nfilename.jsonl", L"batch", payload.size()};
        (void)transport.Send(request, {});
        sent = state.requests[0];
        Require(sent->method == L"POST" && sent->path == L"/v1/files" &&
                    sent->sent.find(payload) != std::string::npos && sent->totalLength == sent->sent.size(),
                "Streaming upload lost bytes");
        Require(sent->headers.find(L"multipart/form-data; boundary=") != std::wstring::npos &&
                    sent->sent.find("bad\"\r\n") == std::string::npos,
                "Multipart filename not sanitized");
        AssertClosed();
        Reset({{200, payload, {}, L"application/octet-stream", 8192}});
        request = {};
        request.resource = BuildResource::FileContent;
        request.item = L"file_offline";
        request.download = destination;
        {
            std::ofstream stream(destination);
            stream << "old content";
        }
        response = transport.Send(request, {});
        std::ifstream downloaded(destination, std::ios::binary);
        std::string actual{std::istreambuf_iterator<char>{downloaded}, {}};
        downloaded.close();
        Require(actual == payload && response.body.empty() && response.downloadedBytes == payload.size(),
                "Atomic download bytes differ");
        AssertClosed();
        Reset({{403, "private error body"}});
        (void)transport.Send(request, {});
        Require(std::filesystem::file_size(destination) == payload.size(), "Failed download replaced existing file");
        AssertClosed();
        Reset({{200, payload, {}, L"application/octet-stream", 8192}});
        state.failRead = true;
        bool partialFailed{};
        try { (void)transport.Send(request, {}); }
        catch (ApiError const &error) { partialFailed = error.code == ApiErrorCode::Network; }
        Require(partialFailed && std::filesystem::file_size(destination) == payload.size(), "Partial download replaced original file");
        for (auto const &entry : std::filesystem::directory_iterator(root))
            Require(entry.path().filename().wstring().find(L".aethernexus-") == std::wstring::npos, "Partial download leaked temporary file");
        AssertClosed();
        Reset({});
        request.upload = {};
        request = {};
        request.resource = BuildResource::Files;
        request.project = L"bad\r\nheader";
        bool rejected{};
        try
        {
            (void)transport.Send(request, {});
        }
        catch (std::invalid_argument const &)
        {
            rejected = true;
        }
        Require(rejected && state.credentialReads == 0 && state.requests.empty(),
                "Invalid input reached credentials or network");
        request = {};
        request.resource = BuildResource::Files;
        request.method = BuildMethod::Post;
        request.upload = BuildUpload{source, L"offline.jsonl", L"batch", 1};
        rejected = false;
        try
        {
            (void)transport.Send(request, {});
        }
        catch (std::invalid_argument const &)
        {
            rejected = true;
        }
        Require(rejected && state.credentialReads == 0, "Changed upload reached credential store");
        request = {};
        request.resource = BuildResource::Files;
        std::stop_source stop;
        stop.request_stop();
        rejected = false;
        try
        {
            (void)transport.Send(request, stop.get_token());
        }
        catch (ApiError const &error)
        {
            rejected = error.code == ApiErrorCode::Cancelled;
        }
        Require(rejected && state.credentialReads == 0 && state.requests.empty(), "Pre-cancelled request was sent");
        std::filesystem::remove(source);
        std::filesystem::remove(destination);
        std::cout << "PASS: native BUILD HTTPS headers, fake model key, multipart "
                     "streaming, atomic download, error redaction, input "
                     "validation and cancellation. All WinHTTP calls are shims.\n";
        return 0;
    }
    catch (std::exception const &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

// Keep these mappings exhaustive. The runner rejects any newly added unmapped
// WinHTTP call.
