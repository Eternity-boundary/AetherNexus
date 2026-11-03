// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,3 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PlaygroundPage.xaml.h
// Project: AetherNexus
#pragma once
#include "PlaygroundPage.g.h"
#include "Services/GenerationClient.h"
#include "Services/WorkflowService.h"
#include <map>
#include <winrt/Windows.Media.Capture.h>
#include <winrt/Windows.Storage.Streams.h>

namespace winrt::AetherNexus::implementation
{
    struct PlaygroundPage : PlaygroundPageT<PlaygroundPage>
    {
        PlaygroundPage() = default;
        void Configure(hstring const &mode, std::shared_ptr<::AetherNexus::Services::IApiTransport>,
                       std::shared_ptr<::AetherNexus::Services::IGenerationTransport>);
        fire_and_forget OnLoaded(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnUnloaded(Windows::Foundation::IInspectable const &,
                                   Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnThemeChanged(Microsoft::UI::Xaml::FrameworkElement const &, Windows::Foundation::IInspectable const &);
        void OnWorkspaceSizeChanged(Windows::Foundation::IInspectable const &,
                                    Microsoft::UI::Xaml::SizeChangedEventArgs const &);
        void OnParameters(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnRefreshModels(Windows::Foundation::IInspectable const &,
                                        Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnRun(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnStop(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnSettings(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnClear(Windows::Foundation::IInspectable const &,
                                Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnAddMessage(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnAttach(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnRemoveAttachments(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnRecord(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnAudioModeChanged(Windows::Foundation::IInspectable const &,
                                Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
        fire_and_forget OnImport(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnExport(Windows::Foundation::IInspectable const &,
                                 Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnCode(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnCopy(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnSaveOutput(Windows::Foundation::IInspectable const &,
                                     Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnAddNode(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnNodeSelected(Windows::Foundation::IInspectable const &,
                            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
        void OnApplyNode(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnDeleteNode(Windows::Foundation::IInspectable const &,
                                     Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnApplyWorkflow(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnAddShot(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        fire_and_forget OnOpenVideo(Windows::Foundation::IInspectable const &,
                                    Microsoft::UI::Xaml::RoutedEventArgs const &);

      private:
        hstring Text(wchar_t const *) const;
        void Notify(wchar_t const *, Microsoft::UI::Xaml::Controls::InfoBarSeverity =
                                         Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
        void Error();
        void SetBusy(bool);
        void SelectModel(std::wstring const &);
        Windows::Foundation::IAsyncAction LoadModels();
        Windows::Foundation::IAsyncAction RenderResult();
        Windows::Foundation::IAsyncAction Save(std::wstring, std::wstring, std::vector<uint8_t>);
        std::wstring Options();
        void AppendMessage(::AetherNexus::Services::ChatMessage const &);
        void ReadMessages();
        std::wstring SessionJson();
        void DrawWorkflow();
        void DrawConnections();
        void ResetWorkflow();
        std::wstring m_mode;
        bool m_loaded{}, m_initialized{}, m_busy{}, m_recording{}, m_editingNodes{}, m_narrow{};
        uint64_t m_version{};
        std::stop_source m_stop;
        std::shared_ptr<::AetherNexus::Services::IApiTransport> m_apiTransport;
        std::shared_ptr<::AetherNexus::Services::IGenerationTransport> m_generationTransport;
        Microsoft::UI::Dispatching::DispatcherQueueTimer m_recordTimer{nullptr};
        std::vector<::AetherNexus::Services::ChatMessage> m_messages;
        struct MessageEditor
        {
            Microsoft::UI::Xaml::Controls::ComboBox role;
            Microsoft::UI::Xaml::Controls::TextBox text;
        };
        std::vector<MessageEditor> m_editors;
        Microsoft::UI::Xaml::Controls::TextBox m_streamEditor{nullptr};
        std::vector<::AetherNexus::Services::Attachment> m_attachments;
        ::AetherNexus::Services::GenerationResult m_result;
        ::AetherNexus::Services::Workflow m_workflow;
        std::vector<Windows::Storage::Streams::InMemoryRandomAccessStream> m_streams;
        Windows::Media::Capture::MediaCapture m_capture{nullptr};
        Windows::Storage::Streams::InMemoryRandomAccessStream m_recordStream{nullptr};
    };
} // namespace winrt::AetherNexus::implementation
namespace winrt::AetherNexus::factory_implementation
{
    struct PlaygroundPage : PlaygroundPageT<PlaygroundPage, implementation::PlaygroundPage>
    {
    };
} // namespace winrt::AetherNexus::factory_implementation
