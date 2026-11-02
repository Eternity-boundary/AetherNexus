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
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <algorithm>
#include <array>

namespace winrt::AetherNexus::implementation
{
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;
    using VM = ::AetherNexus::ViewModels::DashboardViewModel;

    void DashboardPage::OnLoaded(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        m_ready = true;
        UpdatePreview();
    }
    void DashboardPage::OnRangeChanged(Windows::Foundation::IInspectable const&, SelectionChangedEventArgs const&)
    {
        if (m_ready) UpdatePreview();
    }
    void DashboardPage::OnRefresh(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        UpdatePreview();
        Microsoft::Windows::ApplicationModel::Resources::ResourceLoader loader;
        PreviewStatus().Text(loader.GetString(L"PreviewRefreshed"));
    }
    void DashboardPage::UpdatePreview()
    {
        m_viewModel.SetRange(RangePicker().SelectedIndex() == 1 ? 30 : 7);
        auto const& data = m_viewModel.Snapshot();
        TokensValue().Text(VM::Number(data.inputTokens + data.outputTokens));
        CompletionsValue().Text(VM::Number(data.requests));
        CacheValue().Text(VM::Percent(data.cacheRatio()));
        SpendValue().Text(VM::Money(data.monthCostUsd));
        TodayValue().Text(VM::Number(data.todayTokens));
        RequestsValue().Text(VM::Number(data.requests));
        BudgetValue().Text(VM::Money(data.monthCostUsd) + L" / " + VM::Money(data.localBudgetUsd));
        BudgetProgress().Value(data.monthCostUsd / data.localBudgetUsd * 100);
        DateRangeLabel().Text(VM::Date(data.buckets.front().date) + L" – " + VM::Date(data.buckets.back().date) + L" · UTC");
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
            std::array<Border, 6> cards{TokensCard(), CompletionsCard(), CacheCard(), SpendCard(), TodayCard(), RequestsCard()};
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
        if (data.buckets.empty()) return;
        std::vector<double> input, output, cache, requests;
        for (auto const& bucket : data.buckets)
        {
            input.push_back(static_cast<double>(bucket.inputTokens));
            output.push_back(static_cast<double>(bucket.outputTokens));
            cache.push_back(bucket.inputTokens ? static_cast<double>(bucket.cachedInputTokens) / bucket.inputTokens : 0);
            requests.push_back(static_cast<double>(bucket.requests));
        }
        auto const maximum = *std::max_element(input.begin(), input.end()) * 1.15;
        namespace Charts = ::AetherNexus::Views::Charts;
        Charts::Line(TokensChart(), input, TokensChart().Tag().as<Media::Brush>(), maximum);
        Charts::Line(TokensChart(), output, OutputLegendMarker().Fill(), maximum, false, false);
        Charts::Bars(CompletionsChart(), requests, CompletionsChart().Tag().as<Media::Brush>());
        Charts::Line(CacheChart(), cache, CacheChart().Tag().as<Media::Brush>(), 1.0);
        Charts::Line(RequestsChart(), requests, RequestsChart().Tag().as<Media::Brush>());
    }
}
