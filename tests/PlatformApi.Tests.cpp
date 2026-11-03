// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PlatformApi.Tests.cpp
// Project: 其他檔案
#include "Services/PlatformApiClient.h"
#include "Support/MockApiTransport.h"
#include "Services/BudgetService.h"
#include "ViewModels/DashboardViewModel.h"
#include <limits>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <cmath>
#include <functional>
#include <iostream>

using namespace AetherNexus::Services;
using namespace std::chrono;
using namespace winrt::Windows::Data::Json;
namespace
{
    void Require(bool condition) { if (!condition) throw std::runtime_error("Assertion failed"); }
    auto const now = sys_seconds{sys_days{year{2026}/November/3} + hours{12}};
    class Scripted final : public IApiTransport
    {
    public:
        std::function<ApiResponse(ApiRequest const&, ApiResponse)> edit;
        MockApiTransport mock;
        ApiResponse Get(ApiRequest const& request, std::stop_token stop) override
        {
            return edit(request, mock.Get(request, stop));
        }
    };
    void InvalidUsage(std::function<void(JsonObject)> mutate)
    {
        auto transport = std::make_shared<Scripted>();
        transport->edit = [mutate](auto const& request, auto response)
        {
            if (request.endpoint == Endpoint::CompletionsUsage)
            {
                auto object = JsonObject::Parse(response.body);
                mutate(object);
                response.body = object.Stringify();
            }
            return response;
        };
        auto result = PlatformApiClient{transport}.LoadDashboard(7, now);
        Require(result.usageError == ApiErrorCode::InvalidResponse && !result.snapshot.usageAvailable);
        Require(result.snapshot.costsAvailable);
        Require(result.snapshot.buckets.empty()); // Partial pages never become misleading totals.
    }
}
int main()
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        for (int count : {7, 30})
        {
            auto transport = std::make_shared<MockApiTransport>();
            auto result = PlatformApiClient{transport}.LoadDashboard(count, now);
            Require(!result.usageError && !result.costsError);
            auto const& data = result.snapshot;
            Require(data.usageAvailable && data.costsAvailable && data.buckets.size() == static_cast<size_t>(count));
            int64_t expectedInput = 0;
            for (int i = count - 1; i >= 0; --i)
            {
                auto const day = (floor<days>(now) - days{i}).time_since_epoch().count();
                expectedInput += (40 + (day * 17) % 60) * 42000;
            }
            Require(data.inputTokens == expectedInput);
            Require(data.cacheRatio() && std::abs(*data.cacheRatio() - 20000.0 / 42000.0) < 1e-10);
            Require(data.todayTokens == data.buckets.back().inputTokens + data.buckets.back().outputTokens);
            Require(transport->Requests().size() == static_cast<size_t>((count + 6) / 7 + 1));
            auto const& costsRequest = transport->Requests().back();
            Require(costsRequest.startTime == duration_cast<seconds>(sys_days{year{2026}/November/1}.time_since_epoch()).count());
            Require(costsRequest.endTime == now.time_since_epoch().count());
            double expectedCost = 0;
            for (int i = 0; i < 3; ++i)
            {
                auto const day = (sys_days{year{2026}/November/1} + days{i}).time_since_epoch().count();
                expectedCost += (40 + (day * 17) % 60) * 0.0055;
            }
            Require(std::abs(data.monthCostUsd - expectedCost) < 1e-10);
            Require(PlatformApiClient{transport}.ListModels().front().id == L"mock-text-model");
        }
        Require(RequiredCredential(Endpoint::Models) == CredentialRole::Model);
        Require(RequiredCredential(Endpoint::Costs) == CredentialRole::Admin);
        Require(RequiredCredential(Endpoint::CompletionsUsage) == CredentialRole::Admin);
        auto const escaped = BuildRequestPath({Endpoint::Costs, 1, 2, L"+/=&x?"});
        Require(escaped.ends_with(L"page=%2B%2F%3D%26x%3F"));
        std::cout << "PASS: requests, roles, UTC ranges, pagination, aggregation, and model list.\n";

        auto empty = PlatformApiClient{std::make_shared<MockApiTransport>(MockScenario::Empty)}.LoadDashboard(7, now);
        Require(empty.snapshot.usageAvailable && empty.snapshot.costsAvailable);
        Require(empty.snapshot.inputTokens == 0 && !empty.snapshot.cacheRatio() && empty.snapshot.buckets.size() == 7);
        for (auto const& [scenario, error] : {std::pair{MockScenario::Unauthorized, ApiErrorCode::Unauthorized},
            {MockScenario::Forbidden, ApiErrorCode::Forbidden}, {MockScenario::RateLimited, ApiErrorCode::RateLimited},
            {MockScenario::ServerError, ApiErrorCode::Server}, {MockScenario::Timeout, ApiErrorCode::Timeout},
            {MockScenario::Malformed, ApiErrorCode::InvalidResponse}})
        {
            auto transport = std::make_shared<MockApiTransport>(scenario);
            auto result = PlatformApiClient{transport}.LoadDashboard(7, now);
            Require(result.usageError == error && result.costsError == error);
            Require(!result.snapshot.usageAvailable && !result.snapshot.costsAvailable);
            Require(transport->Requests().size() == 2); // No automatic retry loops.
        }
        auto partial = std::make_shared<Scripted>();
        partial->edit = [](auto const& request, auto response) { return request.endpoint == Endpoint::Costs ? ApiResponse{403, L"private error body"} : response; };
        auto result = PlatformApiClient{partial}.LoadDashboard(7, now);
        Require(result.snapshot.usageAvailable && !result.snapshot.costsAvailable && result.costsError == ApiErrorCode::Forbidden);
        std::cout << "PASS: empty data, HTTP errors, timeout, partial availability, and no retry flood.\n";

        InvalidUsage([](auto object)
        {
            auto row = object.GetNamedArray(L"data").GetObjectAt(0).GetNamedArray(L"results").GetObjectAt(0);
            row.SetNamedValue(L"input_tokens", JsonValue::CreateNumberValue(-1));
        });
        InvalidUsage([](auto object)
        {
            auto row = object.GetNamedArray(L"data").GetObjectAt(0).GetNamedArray(L"results").GetObjectAt(0);
            row.SetNamedValue(L"input_cached_tokens", JsonValue::CreateNumberValue(9007199254740991.0));
        });
        InvalidUsage([](auto object)
        {
            auto row = object.GetNamedArray(L"data").GetObjectAt(0).GetNamedArray(L"results").GetObjectAt(0);
            row.SetNamedValue(L"num_model_requests", JsonValue::CreateNumberValue(0.5));
        });
        InvalidUsage([](auto object)
        {
            auto data = object.GetNamedArray(L"data"); data.Append(data.GetAt(0));
        });
        InvalidUsage([](auto object)
        {
            object.SetNamedValue(L"has_more", JsonValue::CreateBooleanValue(true));
            object.SetNamedValue(L"next_page", JsonValue::CreateNullValue());
        });
        auto repeated = std::make_shared<Scripted>();
        repeated->edit = [](auto const& request, auto response)
        {
            if (request.endpoint == Endpoint::CompletionsUsage)
                return ApiResponse{200, L"{\"object\":\"page\",\"data\":[],\"has_more\":true,\"next_page\":\"mock-page:0\"}"};
            return response;
        };
        Require(PlatformApiClient{repeated}.LoadDashboard(7, now).usageError == ApiErrorCode::InvalidResponse);
        auto currency = std::make_shared<Scripted>();
        currency->edit = [](auto const& request, auto response)
        {
            if (request.endpoint == Endpoint::Costs)
            {
                auto object = JsonObject::Parse(response.body);
                object.GetNamedArray(L"data").GetObjectAt(0).GetNamedArray(L"results").GetObjectAt(0)
                    .GetNamedObject(L"amount").SetNamedValue(L"currency", JsonValue::CreateStringValue(L"eur"));
                response.body = object.Stringify();
            }
            return response;
        };
        Require(PlatformApiClient{currency}.LoadDashboard(7, now).costsError == ApiErrorCode::InvalidResponse);
        std::stop_source cancellation;
        cancellation.request_stop();
        auto cancelled = std::make_shared<MockApiTransport>();
        bool caught = false;
        try { (void)PlatformApiClient{cancelled}.LoadDashboard(7, now, cancellation.get_token()); }
        catch (ApiError const& error) { caught = error.code == ApiErrorCode::Cancelled; }
        Require(caught && cancelled->Requests().empty());
        auto midnight = PlatformApiClient{std::make_shared<MockApiTransport>()}.LoadDashboard(7,
            sys_seconds{sys_days{year{2026}/November/1}});
        Require(midnight.snapshot.costsAvailable && midnight.snapshot.monthCostUsd == 0 && !midnight.costsError);
        auto monthEndTransport = std::make_shared<MockApiTransport>();
        auto monthEnd = PlatformApiClient{monthEndTransport}.LoadDashboard(7,
            sys_seconds{sys_days{year{2026}/November/30} + hours{12}});
        Require(monthEnd.snapshot.costsAvailable && monthEndTransport->Requests().size() == 6);
        auto noCache = std::make_shared<Scripted>();
        noCache->edit = [](auto const& request, auto response)
        {
            if (request.endpoint == Endpoint::CompletionsUsage)
            {
                auto object = JsonObject::Parse(response.body);
                object.GetNamedArray(L"data").GetObjectAt(0).GetNamedArray(L"results").GetObjectAt(0)
                    .SetNamedValue(L"input_cached_tokens", JsonValue::CreateNullValue());
                response.body = object.Stringify();
            }
            return response;
        };
        auto unavailableCache = PlatformApiClient{noCache}.LoadDashboard(7, now);
        Require(unavailableCache.snapshot.usageAvailable && !unavailableCache.snapshot.cacheAvailable && !unavailableCache.snapshot.cacheRatio());
        AetherNexus::ViewModels::DashboardViewModel incomplete;
        incomplete.SetSnapshot(unavailableCache.snapshot);
        auto const incompleteCharts = incomplete.Charts();
        Require(std::isnan(incompleteCharts.cachedInput.front()) && std::isnan(incompleteCharts.cacheRatios.front()));
        Require(std::isfinite(incompleteCharts.cacheRatios[1])); // Known days are still displayed.
        std::cout << "PASS: invalid values, duplicate buckets, missing/repeated cursors, currency, and cancellation.\n";

        // Official field semantics: cached, cache-write, and modality counts are
        // subsets of input_tokens, never additional tokens. Rates are token-weighted.
        auto contract = std::make_shared<Scripted>();
        contract->edit = [](auto const& request, auto response)
        {
            if (request.endpoint != Endpoint::CompletionsUsage) return response;
            auto object = JsonObject::Parse(response.body);
            auto buckets = object.GetNamedArray(L"data");
            while (buckets.Size() > 3) buckets.RemoveAtEnd();
            for (uint32_t i = 0; i < buckets.Size(); ++i)
            {
                auto row = buckets.GetObjectAt(i).GetNamedArray(L"results").GetObjectAt(0);
                auto set = [&](auto key, double value) { row.SetNamedValue(key, JsonValue::CreateNumberValue(value)); };
                set(L"input_tokens", i == 0 ? 1000 : (i == 1 ? 100 : 0));
                set(L"output_tokens", i == 0 ? 500 : (i == 1 ? 2000 : 0));
                set(L"input_cached_tokens", i == 0 ? 400 : (i == 1 ? 100 : 0));
                set(L"num_model_requests", i == 0 ? 5 : (i == 1 ? 7 : 0));
                if (i == 0)
                {
                    set(L"input_cache_write_tokens", 100);
                    set(L"input_uncached_tokens", 500);
                    set(L"input_text_tokens", 400);
                    set(L"input_audio_tokens", 50);
                    set(L"input_image_tokens", 50);
                    set(L"input_cached_text_tokens", 300);
                    set(L"input_cached_audio_tokens", 50);
                    set(L"input_cached_image_tokens", 50);
                }
            }
            object.SetNamedValue(L"has_more", JsonValue::CreateBooleanValue(false));
            object.SetNamedValue(L"next_page", JsonValue::CreateNullValue());
            response.body = object.Stringify();
            return response;
        };
        auto officialFields = PlatformApiClient{contract}.LoadDashboard(7, now);
        Require(!officialFields.usageError && officialFields.snapshot.usageAvailable);
        auto const& semantics = officialFields.snapshot;
        Require(semantics.inputTokens == 1100 && semantics.outputTokens == 2500);
        Require(semantics.cachedInputTokens == 500 && semantics.requests == 12);
        Require(std::abs(*semantics.cacheRatio() - 500.0 / 1100.0) < 1e-10);
        Require(!semantics.buckets[2].cacheRatio()); // 0/0 is unavailable, never 0%.
        AetherNexus::ViewModels::DashboardViewModel presentation;
        presentation.SetSnapshot(semantics);
        auto const series = presentation.Charts();
        Require(series.input[0] == 1000 && series.output[0] == 500 && series.cachedInput[0] == 400);
        Require(std::abs(series.cacheRatios[0] - 0.4) < 1e-10 && series.cacheRatios[1] == 1);
        Require(std::isnan(series.cacheRatios[2]) && series.cachedInput[2] == 0 && series.requests[2] == 0);
        Require(series.tokenMaximum > 2000 && series.tokenMaximum < 2400); // Output cannot be clipped to the input scale.
        using VM = AetherNexus::ViewModels::DashboardViewModel;
        Require(VM::Date(sys_days{year{2025}/December/31}) == L"2025/12/31");
        Require(VM::DateTime(now) == L"2026/11/03 12:00:00 UTC");
        std::cout << "PASS: official token/request semantics, weighted cache ratios, undefined ratios, chart gaps and shared scales.\n";
        std::cout << "No sockets, real credentials, or OpenAI requests were used.\n";
        int64_t cents{};
        Require(BudgetService::TryConvertUsd(123.45, cents) && cents == 12345);
        Require(BudgetService::TryConvertUsd(0.01, cents) && cents == 1);
        Require(BudgetService::TryConvertUsd(1000000000.0, cents) && cents == BudgetService::MaximumCents);
        for (double invalid : {0.0, -1.0, 1.001, 1000000000.01, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
            Require(!BudgetService::TryConvertUsd(invalid, cents));
        std::cout << "PASS: user budget conversion and validation boundaries.\n";
        return 0;
    }
    catch (...) { std::cerr << "FAIL: virtual API contract test.\n"; return 1; }
}
