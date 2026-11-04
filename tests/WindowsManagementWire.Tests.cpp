// SPDX-License-Identifier: GPL-3.0-only
// Real WinHTTP serialization against a loopback-only fake server. Never uses real credentials.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <winrt/base.h>
#include "Services/WindowsManagementTransport.h"
#include "Infrastructure/CredentialStore.h"
#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace Wire
{
    constexpr std::wstring_view fakeToken = L"offline-wire-never-valid";
    constexpr std::wstring_view keyId = L"key_offline_wire";
    constexpr std::wstring_view keyPath = L"/v1/organization/admin_api_keys/key_offline_wire";
    constexpr char adminJson[] = R"({"object":"organization.admin_api_key","id":"key_offline_wire","redacted_value":"...5678"})";
    constexpr char deletedJson[] = R"({"object":"organization.admin_api_key.deleted","id":"key_offline_wire","deleted":true})";
    INTERNET_PORT loopbackPort{};
    unsigned credentialReads{};

    void Require(bool condition, char const *message)
    {
        if (!condition) throw std::runtime_error(message);
    }
    struct Socket
    {
        SOCKET value{INVALID_SOCKET};
        explicit Socket(SOCKET value) : value(value) {}
        ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
        Socket(Socket const &) = delete;
        Socket &operator=(Socket const &) = delete;
    };
    std::string Lower(std::string text)
    {
        for (auto &c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        return text;
    }
    struct CapturedRequest
    {
        std::string line, body;
        std::multimap<std::string, std::string> headers;
        std::string Header(std::string const &name) const
        {
            auto range = headers.equal_range(name);
            Require(range.first != range.second, "Expected wire header is missing");
            Require(std::next(range.first) == range.second, "Unexpected duplicate wire header");
            return range.first->second;
        }
    };
    CapturedRequest ParseRequest(std::string const &bytes)
    {
        CapturedRequest request;
        auto lineEnd = bytes.find("\r\n");
        auto headerEnd = bytes.find("\r\n\r\n");
        Require(lineEnd != std::string::npos && headerEnd != std::string::npos, "Invalid HTTP request framing");
        request.line = bytes.substr(0, lineEnd);
        auto offset = lineEnd + 2;
        while (offset < headerEnd)
        {
            auto end = bytes.find("\r\n", offset);
            auto colon = bytes.find(':', offset);
            Require(end != std::string::npos && colon < end, "Malformed serialized HTTP header");
            auto start = colon + 1;
            while (start < end && (bytes[start] == ' ' || bytes[start] == '\t')) ++start;
            request.headers.emplace(Lower(bytes.substr(offset, colon - offset)), bytes.substr(start, end - start));
            offset = end + 2;
        }
        request.body = bytes.substr(headerEnd + 4);
        return request;
    }
    class Server final
    {
      public:
        explicit Server(bool empty404) : m_listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
        {
            Require(m_listener.value != INVALID_SOCKET, "Could not create loopback listener");
            BOOL exclusive = TRUE;
            Require(setsockopt(m_listener.value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                              reinterpret_cast<char *>(&exclusive), sizeof(exclusive)) == 0, "Could not restrict listener");
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            Require(bind(m_listener.value, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 &&
                    listen(m_listener.value, 2) == 0, "Could not bind loopback listener");
            int size = sizeof(address);
            Require(getsockname(m_listener.value, reinterpret_cast<sockaddr *>(&address), &size) == 0,
                    "Could not read loopback port");
            loopbackPort = ntohs(address.sin_port);
            m_thread = std::jthread([this, empty404](std::stop_token stop) {
                try
                {
                    for (unsigned i = 0; i < 2; ++i)
                    {
                        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
                        for (;;)
                        {
                            if (stop.stop_requested()) return;
                            fd_set ready;
                            FD_ZERO(&ready);
                            FD_SET(m_listener.value, &ready);
                            timeval wait{0, 100000};
                            auto selected = select(0, &ready, nullptr, nullptr, &wait);
                            Require(selected != SOCKET_ERROR, "Loopback accept readiness failed");
                            if (selected) break;
                            Require(std::chrono::steady_clock::now() < deadline, "Loopback request did not arrive");
                        }
                        sockaddr_in peer{};
                        int peerSize = sizeof(peer);
                        Socket client{accept(m_listener.value, reinterpret_cast<sockaddr *>(&peer), &peerSize)};
                        Require(client.value != INVALID_SOCKET && peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK),
                                "Rejected a non-loopback connection");
                        DWORD timeout = 2000;
                        Require(setsockopt(client.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&timeout),
                                          sizeof(timeout)) == 0, "Could not bound loopback read");
                        std::string bytes;
                        std::array<char, 4096> chunk{};
                        while (bytes.find("\r\n\r\n") == std::string::npos)
                        {
                            auto received = recv(client.value, chunk.data(), static_cast<int>(chunk.size()), 0);
                            Require(received > 0 && bytes.size() + static_cast<size_t>(received) <= 65536,
                                    "Loopback request exceeded framing limits");
                            bytes.append(chunk.data(), static_cast<size_t>(received));
                        }
                        requests.push_back(ParseRequest(bytes));
                        std::string body = i == 0 ? adminJson : empty404 ? "" : deletedJson;
                        std::string response = i == 1 && empty404 ? "HTTP/1.1 404 Not Found\r\n"
                                                                : "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n";
                        response += "Content-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                        size_t sent{};
                        while (sent < response.size())
                        {
                            auto count = send(client.value, response.data() + sent, static_cast<int>(response.size() - sent), 0);
                            Require(count > 0, "Could not send fake loopback response");
                            sent += static_cast<size_t>(count);
                        }
                    }
                }
                catch (...) { m_failure = std::current_exception(); }
            });
        }
        void Finish()
        {
            m_thread.join();
            if (m_failure) std::rethrow_exception(m_failure);
        }
        std::vector<CapturedRequest> requests;
      private:
        Socket m_listener;
        std::exception_ptr m_failure;
        std::jthread m_thread; // Joins before the listener or captures are destroyed.
    };

    // Only these three entry points are adapted. All header serialization, sending, receiving and
    // response parsing use the real WinHTTP DLL. Plain HTTP is strictly restricted to 127.0.0.1.
    HINTERNET Open(LPCWSTR agent, DWORD access, LPCWSTR proxy, LPCWSTR bypass, DWORD flags)
    {
        Require(access == WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY && proxy == nullptr && bypass == nullptr && flags == 0,
                "Unexpected production WinHTTP session arguments");
        return WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, flags);
    }
    HINTERNET Connect(HINTERNET session, LPCWSTR host, INTERNET_PORT port, DWORD reserved)
    {
        Require(host && std::wstring_view{host} == L"api.openai.com" && port == 443 && reserved == 0 && loopbackPort != 0,
                "Rejected an unexpected production destination");
        return WinHttpConnect(session, L"127.0.0.1", loopbackPort, 0);
    }
    HINTERNET OpenRequest(HINTERNET connection, LPCWSTR method, LPCWSTR path, LPCWSTR version,
                          LPCWSTR referrer, LPCWSTR *accept, DWORD flags)
    {
        Require(method && (std::wstring_view{method} == L"GET" || std::wstring_view{method} == L"DELETE") &&
                path && std::wstring_view{path} == keyPath && flags == WINHTTP_FLAG_SECURE,
                "Rejected a non-fixture request");
        return WinHttpOpenRequest(connection, method, path, version, referrer, accept, 0);
    }
}

namespace AetherNexus::Infrastructure
{
    SecureToken::SecureToken(std::wstring_view value) : m_value(value.begin(), value.end()) {}
    SecureToken::~SecureToken() { Clear(); }
    void SecureToken::Clear() noexcept
    {
        if (!m_value.empty()) SecureZeroMemory(m_value.data(), m_value.size() * sizeof(wchar_t));
        m_value.clear();
    }
    CredentialStore::CredentialStore() : m_prefix(L"offline-wire-only") {}
    std::unique_ptr<SecureToken> CredentialStore::Read(TokenKind kind) const
    {
        Wire::Require(kind == TokenKind::Admin, "Unexpected synthetic credential role");
        ++Wire::credentialReads;
        return std::make_unique<SecureToken>(Wire::fakeToken);
    }
}

#define WinHttpOpen ::Wire::Open
#define WinHttpConnect ::Wire::Connect
#define WinHttpOpenRequest ::Wire::OpenRequest
#include "../src/AetherNexus/Services/WindowsManagementTransport.cpp"
#undef WinHttpOpen
#undef WinHttpConnect
#undef WinHttpOpenRequest

namespace Wire
{
    void Test(bool empty404)
    {
        using namespace AetherNexus::Services;
        credentialReads = 0;
        Server server{empty404};
        auto transport = std::make_shared<WindowsManagementTransport>();
        ManagementClient client{transport};
        ManagementRecord record;
        record.id = keyId;
        record.keyKind = ManagementKeyKind::Admin;
        try
        {
            auto deleted = client.Change(RevokeAdminKeyRequest(record));
            Require(!empty404 && deleted.id == keyId, "Unexpected revocation outcome");
        }
        catch (ManagementHttpError const &error)
        {
            Require(empty404 && error.status == 404 && error.operation == ManagementOperation::AdminRevocation &&
                    error.requestId.empty() && error.diagnostics.requestIdState == ResponseRequestIdState::Missing &&
                    error.diagnostics.content == ResponseContentKind::Unknown &&
                    error.diagnostics.body.kind == ResponseBodyKind::Empty && !error.diagnostics.openAiMetadata &&
                    !error.diagnostics.clientRequestId.empty(), "Empty 404 wire response lost its safe facts");
        }
        server.Finish();
        Require(server.requests.size() == 2 && credentialReads == 2, "Expected only GET then one DELETE");
        std::array<std::string, 2> ids;
        for (size_t i = 0; i < server.requests.size(); ++i)
        {
            auto const &request = server.requests[i];
            Require(request.line == std::string{i == 0 ? "GET" : "DELETE"} +
                        " /v1/organization/admin_api_keys/key_offline_wire HTTP/1.1", "Wire method/path changed");
            Require(request.Header("host") == "127.0.0.1:" + std::to_string(loopbackPort), "Wire escaped loopback");
            Require(request.Header("authorization") == "Bearer offline-wire-never-valid", "Wire Authorization was altered or cleared early");
            Require(request.Header("accept") == "application/json" && request.Header("content-type") == "application/json",
                    "Wire JSON headers were missing");
            Require(request.Header("user-agent") == "AetherNexus/1.0", "Wire user agent changed");
            Require(request.body.empty() && !request.headers.contains("transfer-encoding"), "Wire sent an unexpected entity body");
            if (request.headers.contains("content-length"))
                Require(request.Header("content-length") == "0", "Wire Content-Length was not zero");
            Require(!request.headers.contains("cookie") && !request.headers.contains("proxy-authorization"),
                    "Wire sent an unexpected ambient credential");
            ids[i] = request.Header("x-client-request-id");
            Require(ids[i].size() == 36, "Wire client request ID is missing");
        }
        Require(ids[0] != ids[1], "Wire GET and DELETE reused a client ID");
        std::cout << (empty404 ? "PASS: real WinHTTP loopback reproduces the exact empty, untyped DELETE 404 without inventing server metadata.\n"
                             : "PASS: real WinHTTP wire preserves GET/DELETE paths, synthetic Authorization, JSON headers, empty bodies and distinct client IDs.\n");
    }
}
int main()
{
    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) return 1;
    int result{};
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        Wire::Test(false);
        Wire::Test(true);
        std::cout << "Only 127.0.0.1 sockets and synthetic credentials are used; no external API calls.\n";
    }
    catch (std::exception const &error) { std::cerr << "FAIL: " << error.what() << '\n'; result = 1; }
    WSACleanup();
    return result;
}
