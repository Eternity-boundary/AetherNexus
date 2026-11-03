// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: DashboardPage.xaml.cpp
// Project: AetherNexus
#include "pch.h"
#include "DashboardPage.xaml.h"
#include "DashboardPage.g.cpp"
#include "Charts.h"
#include "Services/PlatformApiClient.h"
#include "Services/WindowsHttpTransport.h"
#include "Services/BudgetService.h"
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <algorithm>
#include <array>

namespace winrt::AetherNexus::implementation
{
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;
    using VM = ::AetherNexus::ViewModels::DashboardViewModel;

    fire_and_forget DashboardPage::OnLoaded(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        m_ready = true;
        co_await Reload();
    }
    void DashboardPage::OnUnloaded(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        m_ready = false;
        ++m_requestVersion;
        m_stop.request_stop();
    }
    fire_and_forget DashboardPage::OnRangeChanged(Windows::Foundation::IInspectable const&, SelectionChangedEventArgs const&)
    {
        auto lifetime = get_strong();
        if (m_ready) co_await Reload();
    }
    fire_and_forget DashboardPage::OnRefresh(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        if (m_ready) co_await Reload();
    }
    hstring DashboardPage::Text(wchar_t const* key) const
    {
        return Microsoft::Windows::ApplicationModel::Resources::ResourceLoader{}.GetString(key);
    }
    Windows::Foundation::IAsyncAction DashboardPage::Reload()
    {
        using namespace ::AetherNexus::Services;
        auto lifetime = get_strong();
        m_stop.request_stop();
        m_stop = std::stop_source{};
        auto const stop = m_stop.get_token();
        auto const version = ++m_requestVersion;
        auto const count = RangePicker().SelectedIndex() == 1 ? 30 : 7;
        auto const now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        m_monthlyBudgetCents.reset();
        m_budgetUnavailable = false;
        try { m_monthlyBudgetCents = BudgetService::Load(); }
        catch (...) { m_budgetUnavailable = true; }
        m_viewModel.SetSnapshot({});
        RenderSnapshot();
        auto const today = std::chrono::floor<std::chrono::days>(now);
        DateRangeLabel().Text(Text(L"UsageQueryRange") + L" " + VM::Date(today - std::chrono::days{count - 1})
            + L" 00:00:00 – " + VM::DateTime(now));
        RefreshButton().IsEnabled(false);
        RangePicker().IsEnabled(false);
        LoadingRing().IsActive(true);
        LoadingRing().Visibility(Visibility::Visible);
        SettingsGuidance().Visibility(Visibility::Collapsed);
        DataNotice().IsOpen(true);
        DataNotice().Severity(InfoBarSeverity::Informational);
        DataNotice().Title(Text(L"DashboardLoadingTitle"));
        DataNotice().Message(Text(L"DashboardLoadingBody"));
        DataStatus().Text(Text(L"DashboardLoadingTitle"));

        DashboardResult result;
        std::optional<ApiErrorCode> failure;
        apartment_context ui;
        co_await resume_background();
        try
        {
            PlatformApiClient client{std::make_shared<WindowsHttpTransport>()};
            result = client.LoadDashboard(count, now, stop);
        }
        catch (ApiError const& error) { failure = error.code; }
        catch (...) { failure = ApiErrorCode::Network; }
        co_await ui;
        if (!m_ready || version != m_requestVersion || stop.stop_requested()) co_return;
        RefreshButton().IsEnabled(true);
        RangePicker().IsEnabled(true);
        LoadingRing().IsActive(false);
        LoadingRing().Visibility(Visibility::Collapsed);
        if (failure)
        {
            if (*failure != ApiErrorCode::Cancelled) ShowError(*failure, false);
            co_return;
        }
        m_viewModel.SetSnapshot(std::move(result.snapshot));
        RenderSnapshot();
        if (result.usageError || result.costsError)
        {
            auto const& snapshot = m_viewModel.Snapshot();
            ShowError(result.usageError ? *result.usageError : *result.costsError,
                snapshot.usageAvailable || snapshot.costsAvailable);
        }
        else
        {
            DataNotice().IsOpen(false);
            DataStatus().Text(Text(L"DashboardUpdated"));
        }
    }
    void DashboardPage::ShowError(::AetherNexus::Services::ApiErrorCode code, bool partial)
    {
        using enum ::AetherNexus::Services::ApiErrorCode;
        wchar_t const* key = L"ApiNetworkError";
        bool settings = false;
        switch (code)
        {
        case MissingCredential: key = L"ApiMissingAdminKey"; settings = true; break;
        case CredentialUnavailable: key = L"ApiCredentialUnavailable"; settings = true; break;
        case Unauthorized: key = L"ApiUnauthorized"; settings = true; break;
        case Forbidden: key = L"ApiForbidden"; settings = true; break;
        case RateLimited: key = L"ApiRateLimited"; break;
        case Timeout: key = L"ApiTimeout"; break;
        case Server: key = L"ApiServerError"; break;
        case InvalidResponse: key = L"ApiInvalidResponse"; break;
        default: break;
        }
        DataNotice().IsOpen(true);
        DataNotice().Severity(settings ? InfoBarSeverity::Informational : InfoBarSeverity::Warning);
        DataNotice().Title(Text(partial ? L"DashboardPartial" : L"DashboardUnavailable"));
        DataNotice().Message(Text(key));
        SettingsGuidance().Visibility(settings ? Visibility::Visible : Visibility::Collapsed);
        DataStatus().Text(Text(partial ? L"DashboardPartial" : L"DashboardUnavailable"));
    }
    void DashboardPage::OnOpenSettings(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        // Use the existing shell's navigation selection so its selected item stays in sync.
        auto current = get_strong().as<DependencyObject>();
        while (current)
        {
            if (auto const navigation = current.try_as<NavigationView>())
            {
                navigation.SelectedItem(navigation.SettingsItem());
                return;
            }
            current = Media::VisualTreeHelper::GetParent(current);
        }
    }
    void DashboardPage::RenderSnapshot()
    {
        auto const& data = m_viewModel.Snapshot();
        TokensValue().Text(data.usageAvailable ? hstring{VM::Number(data.inputTokens + data.outputTokens)} : L"—");
        CompletionsValue().Text(data.usageAvailable ? hstring{VM::Number(data.requests)} : L"—");
        auto const cacheRatio = data.cacheRatio();
        CacheValue().Text(cacheRatio ? hstring{VM::Percent(*cacheRatio)} : L"—");
        SpendValue().Text(data.costsAvailable ? hstring{VM::Money(data.monthCostUsd)} : L"—");
        TodayValue().Text(data.usageAvailable ? hstring{VM::Number(data.todayTokens)} : L"—");
        CachedTokensValue().Text(data.usageAvailable && data.cacheAvailable ? hstring{VM::Number(data.cachedInputTokens)} : L"—");
        BudgetValue().Text(m_budgetUnavailable ? Text(L"BudgetLoadFailed") : (m_monthlyBudgetCents
            ? hstring{(data.costsAvailable ? VM::Money(data.monthCostUsd) : L"—") + L" / " + VM::Money(*m_monthlyBudgetCents / 100.0)}
            : Text(L"BudgetNotConfigured")));
        BudgetProgress().Visibility(data.costsAvailable && m_monthlyBudgetCents ? Visibility::Visible : Visibility::Collapsed);
        if (m_monthlyBudgetCents) BudgetProgress().Value(std::clamp(data.monthCostUsd * 10000.0 / *m_monthlyBudgetCents, 0.0, 100.0));
        // Summaries remain available to screen readers; the decorative chart shapes are not focusable.
        Automation::AutomationProperties::SetName(TokensChart(), TokensValue().Text());
        DrawCharts();
    }
    void DashboardPage::OnCardsSizeChanged(Windows::Foundation::IInspectable const&, SizeChangedEventArgs const& args)
    {
        auto const width = args.NewSize().Width;
        int const columns = width >= 1000 ? 3 : (width >= 660 ? 2 : 1);
        if (columns != m_columns)
        {
            m_columns = columns;
            std::array<Border, 6> cards{TokensCard(), CompletionsCard(), CacheCard(), SpendCard(), TodayCard(), CachedTokensCard()};
            CardsGrid().ColumnDefinitions().Clear();
            for (int i = 0; i < columns; ++i) CardsGrid().ColumnDefinitions().Append(ColumnDefinition{});
            for (size_t i = 0; i < cards.size(); ++i)
            {
                Grid::SetRow(cards[i], static_cast<int>(i) / columns);
                Grid::SetColumn(cards[i], static_cast<int>(i) % columns);
            }
        }
        // Canvas widths are final after layout; schedule once for the subsequent UI turn.
        DispatcherQueue().TryEnqueue([weak = get_weak()] { if (auto self = weak.get()) self->DrawCharts(); });
    }
    void DashboardPage::OnThemeChanged(FrameworkElement const&, Windows::Foundation::IInspectable const&)
    {
        if (m_ready) DispatcherQueue().TryEnqueue([weak = get_weak()] { if (auto self = weak.get()) self->DrawCharts(); });
    }
    void DashboardPage::DrawCharts()
    {
        auto const& data = m_viewModel.Snapshot();
        TokensChart().Children().Clear();
        CompletionsChart().Children().Clear();
        CacheChart().Children().Clear();
        CachedTokensChart().Children().Clear();
        if (!data.usageAvailable || data.buckets.empty()) return;
        auto const series = m_viewModel.Charts();
        namespace Charts = ::AetherNexus::Views::Charts;
        Charts::Line(TokensChart(), series.input, TokensChart().Tag().as<Media::Brush>(), series.tokenMaximum);
        Charts::Line(TokensChart(), series.output, OutputLegendMarker().Fill(), series.tokenMaximum, false, false);
        Charts::Bars(CompletionsChart(), series.requests, CompletionsChart().Tag().as<Media::Brush>());
        Charts::Line(CacheChart(), series.cacheRatios, CacheChart().Tag().as<Media::Brush>(), 1.0);
        Charts::Line(CachedTokensChart(), series.cachedInput, CachedTokensChart().Tag().as<Media::Brush>());
    }
}
