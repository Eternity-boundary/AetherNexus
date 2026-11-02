// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: Charts.cpp
// Project: AetherNexus
#include "pch.h"
#include "Charts.h"
#include <algorithm>

namespace AetherNexus::Views::Charts
{
    using namespace winrt;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Media;
    using namespace Microsoft::UI::Xaml::Shapes;
    using Windows::Foundation::Point;

    void Line(Canvas const& canvas, std::span<double const> values, Brush const& brush,
        double maximum, bool clear, bool fill)
    {
        if (clear) canvas.Children().Clear();
        auto const width = static_cast<float>(canvas.ActualWidth());
        auto const height = static_cast<float>(canvas.ActualHeight());
        if (values.empty() || width < 16 || height < 16) return;
        maximum = maximum > 0 ? maximum : std::max(1.0, *std::max_element(values.begin(), values.end()) * 1.15);
        auto points = single_threaded_vector<Point>();
        for (size_t i = 0; i < values.size(); ++i)
        {
            points.Append({4 + (width - 8) * static_cast<float>(i) / std::max(1.0f, static_cast<float>(values.size() - 1)),
                height - 6 - (height - 12) * static_cast<float>(std::clamp(values[i] / maximum, 0.0, 1.0))});
        }
        if (fill)
        {
            Microsoft::UI::Xaml::Shapes::Polygon area;
            area.Points().Append({4, height});
            for (auto const& point : points) area.Points().Append(point);
            area.Points().Append({width - 4, height});
            area.Fill(brush);
            area.Opacity(0.08);
            canvas.Children().Append(area);
        }
        Microsoft::UI::Xaml::Shapes::Polyline line;
        for (auto const& point : points) line.Points().Append(point);
        line.Stroke(brush);
        line.StrokeThickness(2.5);
        line.StrokeLineJoin(PenLineJoin::Round);
        if (!clear)
        {
            line.StrokeDashArray().Append(3);
            line.StrokeDashArray().Append(2);
        }
        canvas.Children().Append(line);
        Microsoft::UI::Xaml::Shapes::Ellipse end;
        end.Width(7);
        end.Height(7);
        end.Fill(brush);
        Canvas::SetLeft(end, points.GetAt(points.Size() - 1).X - 3.5);
        Canvas::SetTop(end, points.GetAt(points.Size() - 1).Y - 3.5);
        canvas.Children().Append(end);
    }

    void Bars(Canvas const& canvas, std::span<double const> values, Brush const& brush)
    {
        canvas.Children().Clear();
        auto const width = canvas.ActualWidth();
        auto const height = canvas.ActualHeight();
        if (values.empty() || width < 16) return;
        auto const maximum = std::max(1.0, *std::max_element(values.begin(), values.end()) * 1.12);
        auto const step = width / values.size();
        for (size_t i = 0; i < values.size(); ++i)
        {
            Microsoft::UI::Xaml::Shapes::Rectangle bar;
            bar.Width(std::max(1.0, step * 0.62));
            bar.Height(std::max(2.0, values[i] / maximum * (height - 4)));
            bar.RadiusX(3);
            bar.RadiusY(3);
            bar.Fill(brush);
            Canvas::SetLeft(bar, i * step + step * 0.19);
            Canvas::SetTop(bar, height - bar.Height());
            canvas.Children().Append(bar);
        }
    }
}
