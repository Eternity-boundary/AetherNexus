// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: HttpResponseDiagnostics.h
// Project: AetherNexus
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace AetherNexus::Services
{
    enum class ResponseBodyKind { NotRead, Empty, JsonError, JsonOther, HtmlLike, Other, InvalidJson, Truncated, ReadFailed };
    enum class ResponseErrorType { None, InvalidRequest, Authentication, Permission, NotFound, RateLimit, Server, Unknown };
    enum class ResponseErrorHint { None, InvalidRoute, ResourceMissing, AccessDenied };
    struct ResponseBodyFacts
    {
        ResponseBodyKind kind{ResponseBodyKind::NotRead};
        ResponseErrorType type{ResponseErrorType::None};
        ResponseErrorHint hint{ResponseErrorHint::None};
    };

    namespace HttpResponseDiagnosticsDetail
    {
        using std::size_t;
        using std::uint32_t;
        constexpr bool Space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
        constexpr bool Digit(char c) noexcept { return c >= '0' && c <= '9'; }
        constexpr char Lower(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; }
        constexpr int Hex(char c) noexcept
        {
            return Digit(c) ? c - '0' : Lower(c) >= 'a' && Lower(c) <= 'f' ? Lower(c) - 'a' + 10 : -1;
        }
        inline bool StartsAscii(std::string_view text, std::string_view prefix) noexcept
        {
            if (text.size() < prefix.size()) return false;
            for (size_t i = 0; i < prefix.size(); ++i)
                if (Lower(text[i]) != Lower(prefix[i])) return false;
            return true;
        }

        // All views refer to the caller's secure buffer. No decoded string or arbitrary field is copied.
        class Parser final
        {
          public:
            explicit Parser(std::string_view body) noexcept : m_body(body) {}
            ResponseBodyFacts Inspect() noexcept
            {
                if (!Value(0, Role::Root)) return {ResponseBodyKind::InvalidJson};
                Whitespace();
                if (m_pos != m_body.size()) return {ResponseBodyKind::InvalidJson};
                return {m_errorObject ? ResponseBodyKind::JsonError : ResponseBodyKind::JsonOther,
                        m_type, m_invalidUrl ? ResponseErrorHint::InvalidRoute : m_codeHint};
            }

          private:
            enum class Role { Ignore, Root, Error };
            std::string_view m_body;
            size_t m_pos{}, m_tokens{};
            bool m_errorObject{}, m_invalidUrl{};
            ResponseErrorType m_type{ResponseErrorType::None};
            ResponseErrorHint m_codeHint{ResponseErrorHint::None};

            void Whitespace() noexcept { while (m_pos < m_body.size() && Space(m_body[m_pos])) ++m_pos; }
            bool Token() noexcept { return ++m_tokens <= 4096; }
            bool Take(char c) noexcept
            {
                if (m_pos == m_body.size() || m_body[m_pos] != c) return false;
                ++m_pos;
                return true;
            }
            bool Literal(std::string_view value) noexcept
            {
                if (m_body.size() - m_pos < value.size()) return false;
                for (size_t i = 0; i < value.size(); ++i)
                    if (m_body[m_pos + i] != value[i]) return false;
                m_pos += value.size();
                return true;
            }
            bool Hex4(uint32_t &value) noexcept
            {
                value = 0;
                if (m_body.size() - m_pos < 4) return false;
                for (unsigned i = 0; i < 4; ++i)
                {
                    int digit = Hex(m_body[m_pos++]);
                    if (digit < 0) return false;
                    value = value * 16 + static_cast<uint32_t>(digit);
                }
                return true;
            }
            bool Utf8(unsigned char first) noexcept
            {
                unsigned count = first >= 0xc2 && first <= 0xdf ? 1u : first >= 0xe0 && first <= 0xef ? 2u
                    : first >= 0xf0 && first <= 0xf4 ? 3u : 0u;
                if (!count || m_body.size() - m_pos < count) return false;
                uint32_t value = first & (count == 1 ? 0x1fu : count == 2 ? 0x0fu : 0x07u);
                for (unsigned i = 0; i < count; ++i)
                {
                    auto next = static_cast<unsigned char>(m_body[m_pos++]);
                    if ((next & 0xc0u) != 0x80u) return false;
                    value = (value << 6) | (next & 0x3fu);
                }
                return value >= (count == 1 ? 0x80u : count == 2 ? 0x800u : 0x10000u) &&
                       value <= 0x10ffffu && !(value >= 0xd800u && value <= 0xdfffu);
            }
            bool String(std::string_view &raw) noexcept
            {
                if (!Token() || !Take('"')) return false;
                size_t start = m_pos;
                while (m_pos < m_body.size())
                {
                    auto c = static_cast<unsigned char>(m_body[m_pos++]);
                    if (c == '"')
                    {
                        raw = {m_body.data() + start, m_pos - start - 1};
                        return true;
                    }
                    if (c < 0x20u) return false;
                    if (c >= 0x80u) { if (!Utf8(c)) return false; }
                    else if (c == '\\')
                    {
                        if (m_pos == m_body.size()) return false;
                        char escaped = m_body[m_pos++];
                        if (escaped == 'u')
                        {
                            uint32_t value{};
                            if (!Hex4(value)) return false;
                            if (value >= 0xd800u && value <= 0xdbffu)
                            {
                                if (!Take('\\') || !Take('u') || !Hex4(value) || value < 0xdc00u || value > 0xdfffu)
                                    return false;
                            }
                            else if (value >= 0xdc00u && value <= 0xdfffu) return false;
                        }
                        else if (escaped != '"' && escaped != '\\' && escaped != '/' && escaped != 'b' &&
                                 escaped != 'f' && escaped != 'n' && escaped != 'r' && escaped != 't') return false;
                    }
                }
                return false;
            }
            // String() has already validated escapes; decode only enough characters to compare ASCII constants.
            static bool Match(std::string_view raw, std::string_view value, bool prefix = false,
                              bool insensitive = false) noexcept
            {
                size_t pos{};
                for (char expected : value)
                {
                    if (pos == raw.size()) return false;
                    unsigned decoded = static_cast<unsigned char>(raw[pos++]);
                    if (decoded == '\\')
                    {
                        char escape = raw[pos++];
                        if (escape == 'u')
                        {
                            decoded = 0;
                            for (unsigned i = 0; i < 4; ++i) decoded = decoded * 16 + static_cast<unsigned>(Hex(raw[pos++]));
                        }
                        else decoded = escape == 'b' ? 0x08u : escape == 'f' ? 0x0cu : escape == 'n' ? 0x0au
                            : escape == 'r' ? 0x0du : escape == 't' ? 0x09u
                            : static_cast<unsigned>(static_cast<unsigned char>(escape));
                    }
                    if (decoded > 0x7fu || (insensitive ? Lower(static_cast<char>(decoded)) != Lower(expected)
                                                        : static_cast<char>(decoded) != expected)) return false;
                }
                return prefix || pos == raw.size();
            }
            bool Diagnostic(std::string_view name) noexcept
            {
                Whitespace();
                if (m_pos == m_body.size()) return false;
                if (m_body[m_pos] == 'n') return Token() && Literal("null");
                std::string_view value;
                if (!String(value)) return false;
                if (Match(name, "type"))
                {
                    m_type = Match(value, "invalid_request_error") ? ResponseErrorType::InvalidRequest
                        : Match(value, "authentication_error") ? ResponseErrorType::Authentication
                        : Match(value, "permission_error") ? ResponseErrorType::Permission
                        : Match(value, "not_found_error") ? ResponseErrorType::NotFound
                        : Match(value, "rate_limit_error") ? ResponseErrorType::RateLimit
                        : Match(value, "server_error") ? ResponseErrorType::Server : ResponseErrorType::Unknown;
                }
                else if (Match(name, "code"))
                    m_codeHint = Match(value, "route_not_found") ? ResponseErrorHint::InvalidRoute
                        : Match(value, "resource_not_found") ? ResponseErrorHint::ResourceMissing
                        : Match(value, "permission_denied") || Match(value, "insufficient_permissions")
                            ? ResponseErrorHint::AccessDenied : ResponseErrorHint::None;
                else m_invalidUrl = Match(value, "Invalid URL", true, true);
                return true;
            }
            bool Object(unsigned depth, Role role) noexcept
            {
                if (depth >= 16 || !Take('{')) return false;
                bool errorSeen{}, typeSeen{}, codeSeen{}, messageSeen{};
                Whitespace();
                if (Take('}')) return true;
                for (;;)
                {
                    std::string_view name;
                    if (!String(name)) return false;
                    Whitespace();
                    if (!Take(':')) return false;
                    Whitespace();
                    if (role == Role::Root && Match(name, "error"))
                    {
                        if (errorSeen) return false;
                        errorSeen = true;
                        bool object = m_pos < m_body.size() && m_body[m_pos] == '{';
                        if (!Value(depth + 1, object ? Role::Error : Role::Ignore)) return false;
                        m_errorObject = object;
                    }
                    else if (role == Role::Error && (Match(name, "type") || Match(name, "code") || Match(name, "message")))
                    {
                        bool &seen = Match(name, "type") ? typeSeen : Match(name, "code") ? codeSeen : messageSeen;
                        if (seen || !Diagnostic(name)) return false;
                        seen = true;
                    }
                    else if (!Value(depth + 1, Role::Ignore)) return false;
                    Whitespace();
                    if (Take('}')) return true;
                    if (!Take(',')) return false;
                    Whitespace();
                }
            }
            bool Array(unsigned depth) noexcept
            {
                if (depth >= 16 || !Take('[')) return false;
                Whitespace();
                if (Take(']')) return true;
                for (;;)
                {
                    if (!Value(depth + 1, Role::Ignore)) return false;
                    Whitespace();
                    if (Take(']')) return true;
                    if (!Take(',')) return false;
                    Whitespace();
                }
            }
            bool Number() noexcept
            {
                (void)Take('-');
                if (!Take('0'))
                {
                    if (m_pos == m_body.size() || !Digit(m_body[m_pos])) return false;
                    while (m_pos < m_body.size() && Digit(m_body[m_pos])) ++m_pos;
                }
                if (Take('.'))
                {
                    if (m_pos == m_body.size() || !Digit(m_body[m_pos])) return false;
                    while (m_pos < m_body.size() && Digit(m_body[m_pos])) ++m_pos;
                }
                if (Take('e') || Take('E'))
                {
                    if (!Take('+')) (void)Take('-');
                    if (m_pos == m_body.size() || !Digit(m_body[m_pos])) return false;
                    while (m_pos < m_body.size() && Digit(m_body[m_pos])) ++m_pos;
                }
                return true;
            }
            bool Value(unsigned depth, Role role) noexcept
            {
                if (!Token()) return false;
                Whitespace();
                if (m_pos == m_body.size()) return false;
                char c = m_body[m_pos];
                if (c == '{') return Object(depth, role);
                if (c == '[') return Array(depth);
                if (c == '"') { std::string_view ignored; return String(ignored); }
                if (c == 't') return Literal("true");
                if (c == 'f') return Literal("false");
                if (c == 'n') return Literal("null");
                return c == '-' || Digit(c) ? Number() : false;
            }
        };
    }

    inline ResponseBodyFacts InspectErrorResponse(std::string_view body, bool truncated = false) noexcept
    {
        using namespace HttpResponseDiagnosticsDetail;
        if (truncated || body.size() > 32 * 1024) return {ResponseBodyKind::Truncated};
        while (!body.empty() && Space(body.front())) body.remove_prefix(1);
        if (body.empty()) return {ResponseBodyKind::Empty};
        if (StartsAscii(body, "<!doctype html") || StartsAscii(body, "<html") ||
            StartsAscii(body, "<head") || StartsAscii(body, "<body")) return {ResponseBodyKind::HtmlLike};
        char first = body.front();
        if (first != '{' && first != '[' && first != '"' && first != 't' && first != 'f' &&
            first != 'n' && first != '-' && !Digit(first)) return {ResponseBodyKind::Other};
        return Parser{body}.Inspect();
    }
}
