// SPDX-License-Identifier: GPL-3.0-only
#include "Services/HttpResponseDiagnostics.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
using namespace AetherNexus::Services;
namespace
{
    void Check(bool value)
    {
        if (!value) throw std::runtime_error("HTTP response facts assertion failed");
    }
    void Kind(std::string_view body, ResponseBodyKind expected)
    {
        Check(InspectErrorResponse(body).kind == expected);
    }
}
void RunHttpResponseDiagnosticsTests()
{
    static_assert(noexcept(InspectErrorResponse({})));
    static_assert(std::is_trivially_copyable_v<ResponseBodyFacts>);
    Kind({}, ResponseBodyKind::Empty);
    Kind(" \t\r\n", ResponseBodyKind::Empty);
    Kind("<!DOCTYPE html><html>synthetic-secret-never-display</html>", ResponseBodyKind::HtmlLike);
    Kind("\r\n<HTML><body>private account name</body>", ResponseBodyKind::HtmlLike);
    Kind("Not Found: synthetic-secret-never-display", ResponseBodyKind::Other);
    for (auto body : {"{}", "[]", "[true,false,null,-1.25e+2]", "null", "true", "123", "\"error\"",
                     R"({"detail":"Invalid URL /private/path","type":"not_found_error"})",
                     R"({"nested":{"error":{"type":"not_found_error","message":"Invalid URL /secret"}}})",
                     R"({"error":"private server text"})"})
        Kind(body, ResponseBodyKind::JsonOther);
    auto facts = InspectErrorResponse(R"({"error":{"type":"invalid_request_error","code":"route_not_found","message":"do not expose synthetic-secret"}})");
    Check(facts.kind == ResponseBodyKind::JsonError && facts.type == ResponseErrorType::InvalidRequest &&
          facts.hint == ResponseErrorHint::InvalidRoute);
    for (auto [type, expected] : {std::pair{"authentication_error", ResponseErrorType::Authentication},
                                 {"permission_error", ResponseErrorType::Permission},
                                 {"not_found_error", ResponseErrorType::NotFound},
                                 {"rate_limit_error", ResponseErrorType::RateLimit},
                                 {"server_error", ResponseErrorType::Server},
                                 {"future_type_private_secret", ResponseErrorType::Unknown}})
    {
        facts = InspectErrorResponse("{\"error\":{\"type\":\"" + std::string{type} + "\"}}");
        Check(facts.kind == ResponseBodyKind::JsonError && facts.type == expected && facts.hint == ResponseErrorHint::None);
    }
    for (auto [code, expected] : {std::pair{"resource_not_found", ResponseErrorHint::ResourceMissing},
                                 {"permission_denied", ResponseErrorHint::AccessDenied},
                                 {"insufficient_permissions", ResponseErrorHint::AccessDenied},
                                 {"arbitrary-secret-code", ResponseErrorHint::None}})
    {
        facts = InspectErrorResponse("{\"error\":{\"code\":\"" + std::string{code} + "\"}}");
        Check(facts.kind == ResponseBodyKind::JsonError && facts.type == ResponseErrorType::None && facts.hint == expected);
    }
    for (auto body : {
             R"({"error":{"message":"iNvAlId uRl: /private?token=synthetic-secret","code":"resource_not_found"}})",
             R"({"error":{"code":"permission_denied","message":"Invalid URL /private"}})",
             R"({"\u0065rror":{"t\u0079pe":"not_found_\u0065rror","m\u0065ssage":"\u0049nvalid URL /secret"}})"})
    {
        facts = InspectErrorResponse(body);
        Check(facts.kind == ResponseBodyKind::JsonError && facts.hint == ResponseErrorHint::InvalidRoute);
    }
    facts = InspectErrorResponse(R"({"error":{"message":"prefix Invalid URL /secret","code":null,"type":null,"context":{"message":"Invalid URL /secret"}}})");
    Check(facts.kind == ResponseBodyKind::JsonError && facts.type == ResponseErrorType::None && facts.hint == ResponseErrorHint::None);
    facts = InspectErrorResponse(R"({"error":{"message":"Unicode \uD83D\uDE00 and escaped \"quotes\" are valid"}})");
    Check(facts.kind == ResponseBodyKind::JsonError && facts.hint == ResponseErrorHint::None);
    for (auto body : {
             R"({"error":{},"\u0065rror":{}})", R"({"error":{"type":null,"t\u0079pe":"not_found_error"}})",
             R"({"error":{"code":"route_not_found","code":"resource_not_found"}})",
             R"({"error":{"message":"Invalid URL /secret","m\u0065ssage":"harmless"}})",
             R"({"error":{"message":15}})", R"({"error":{"type":[]}})", R"({"error":{"code":{}}})",
             R"({"error":{"message":"\uD800"}})", R"({"error":{"message":"\uDC00"}})",
             R"({"error":{"message":"\uZZZZ"}})", R"({"error":{"message":"\x20"}})",
             R"({"error":{"message":"unfinished})", R"({"error":{},})", R"({"error":{}} trailing)",
             "[01]", "[-]", "[1.]", "[1e]", "[true,]", "{", "["})
    {
        facts = InspectErrorResponse(body);
        Check(facts.kind == ResponseBodyKind::InvalidJson && facts.type == ResponseErrorType::None && facts.hint == ResponseErrorHint::None);
    }
    std::string invalidUtf8 = "{\"error\":{\"message\":\"";
    invalidUtf8.push_back(static_cast<char>(0xc0));
    invalidUtf8 += "\"}}";
    Kind(invalidUtf8, ResponseBodyKind::InvalidJson);
    std::string nested(16, '[');
    nested += "0";
    nested.append(16, ']');
    Kind(nested, ResponseBodyKind::JsonOther);
    nested.insert(nested.begin(), '[');
    nested += ']';
    Kind(nested, ResponseBodyKind::InvalidJson);
    std::string many = "[";
    for (unsigned i = 0; i < 4096; ++i) many += "0,";
    many += "0]";
    Kind(many, ResponseBodyKind::InvalidJson);
    Check(InspectErrorResponse(R"({"error":{"type":"not_found_error"}})", true).kind == ResponseBodyKind::Truncated);
    Kind(std::string(32769, ' '), ResponseBodyKind::Truncated);
    std::cout << "PASS: bounded error-body facts, root-only envelope detection, escaped diagnostic fields, duplicate rejection and no raw diagnostic strings.\n";
}
