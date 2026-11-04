// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,5 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: PlaygroundPage.xaml.cpp
// Project: AetherNexus
#include "pch.h"
#include "PlaygroundPage.xaml.h"
#include "PlaygroundPage.g.cpp"
#include "Services/PlatformApiClient.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Devices.Input.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>

namespace winrt::AetherNexus::implementation
{
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;
    using namespace Windows::Data::Json;
    using namespace Windows::Storage;
    using namespace Windows::Storage::Streams;
    using namespace ::AetherNexus::Services;
    namespace Pickers = Microsoft::Windows::Storage::Pickers;
    namespace
    {
        std::wstring Value(ComboBox const &picker)
        {
            return std::wstring{picker.IsEditable()
                                    ? picker.Text()
                                    : unbox_value<hstring>(picker.SelectedItem().as<ComboBoxItem>().Content())};
        }
        void Require(bool value)
        {
            if (!value)
                throw std::invalid_argument("Invalid playground input");
        }
        void Set(JsonObject const &object, wchar_t const *key, std::wstring const &text)
        {
            object.SetNamedValue(key, JsonValue::CreateStringValue(text));
        }
        JsonObject AttachmentJson(Attachment const &file)
        {
            JsonObject result;
            Set(result, L"name", file.name);
            Set(result, L"mime", file.mime);
            Set(result, L"data", std::wstring{to_hstring(Base64(file.bytes))});
            return result;
        }
        Attachment ReadAttachment(JsonObject const &file)
        {
            auto bytes = DecodeBase64(to_string(file.GetNamedString(L"data")));
            Require(bytes.size() <= 25 * 1024 * 1024);
            return {std::wstring{file.GetNamedString(L"name")}, std::wstring{file.GetNamedString(L"mime")},
                    std::move(bytes)};
        }
        std::vector<uint8_t> Bytes(std::wstring const &text)
        {
            auto utf8 = to_string(text);
            return {utf8.begin(), utf8.end()};
        }
        auto ShowConfirm(XamlRoot const &root, hstring const &title, hstring const &body, hstring const &action,
                         hstring const &cancel)
        {
            ContentDialog dialog;
            dialog.XamlRoot(root);
            dialog.Title(box_value(title));
            dialog.Content(box_value(body));
            dialog.PrimaryButtonText(action);
            dialog.CloseButtonText(cancel);
            dialog.DefaultButton(ContentDialogButton::Close);
            return dialog.ShowAsync();
        }
        void EnableTree(DependencyObject const &root, bool enabled)
        {
            if (auto control = root.try_as<Control>())
                control.IsEnabled(enabled);
            int count = Media::VisualTreeHelper::GetChildrenCount(root);
            for (int i = 0; i < count; ++i)
                EnableTree(Media::VisualTreeHelper::GetChild(root, i), enabled);
        }
    } // namespace
    hstring PlaygroundPage::Text(wchar_t const *key) const
    {
        return Microsoft::Windows::ApplicationModel::Resources::ResourceLoader{}.GetString(key);
    }
    void PlaygroundPage::Notify(wchar_t const *key, InfoBarSeverity severity)
    {
        Notice().Message(Text(key));
        Notice().Severity(severity);
        Notice().IsOpen(true);
    }
    void PlaygroundPage::Configure(hstring const &mode, std::shared_ptr<IApiTransport> api,
                                   std::shared_ptr<IGenerationTransport> generation)
    {
        Require(api && generation);
        m_apiTransport = std::move(api);
        m_generationTransport = std::move(generation);
        m_mode = mode;
        PageTitle().Text(mode == L"agent-builder" ? L"Agent Builder"
                         : mode == L"images"      ? L"Images"
                         : mode == L"audio"       ? L"Audio"
                         : mode == L"video"       ? L"Video"
                                                  : L"Chat");
        auto visible = [](bool value) { return value ? Visibility::Visible : Visibility::Collapsed; };
        ConversationList().Visibility(visible(mode == L"chat"));
        ImageWorkspace().Visibility(visible(mode == L"images"));
        WorkflowWorkspace().Visibility(visible(mode == L"agent-builder"));
        VideoWorkspace().Visibility(visible(mode == L"video"));
        ChatParameters().Visibility(visible(mode == L"chat"));
        ImageParameters().Visibility(visible(mode == L"images"));
        AudioParameters().Visibility(visible(mode == L"audio"));
        NodeParameters().Visibility(visible(mode == L"agent-builder"));
        RecordButton().Visibility(visible(mode == L"audio"));
        AttachButton().Visibility(visible(mode == L"chat" || mode == L"images" || mode == L"audio"));
        RemoveAttachments().Visibility(AttachButton().Visibility());
        ModelPicker().Visibility(visible(mode != L"video"));
        RefreshModels().Visibility(visible(mode != L"video"));
        RunButton().Visibility(visible(mode != L"video"));
        StopButton().Visibility(RunButton().Visibility());
        CodeButton().Visibility(visible(mode != L"video"));
        AdvancedExpander().Visibility(visible(mode != L"video" && mode != L"agent-builder"));
        AddMessageButton().Visibility(visible(mode == L"chat"));
        ParametersButton().Visibility(visible(mode != L"video"));
        if (mode == L"video")
            WorkspaceSplit().IsPaneOpen(false);
        OutputTab().Visibility(visible(mode != L"video"));
        DetailsTab().Visibility(visible(mode != L"video"));
        CodeTab().Visibility(visible(mode != L"video"));
        ModelPicker().Text(mode == L"images"  ? L"gpt-image-2.5-flare"
                           : mode == L"audio" ? L"gpt-realtime-2.1-mini"
                                              : L"gpt-6-astra");
        TemperatureBox().Value(std::numeric_limits<double>::quiet_NaN());
        if (mode == L"agent-builder")
        {
            ResetWorkflow();
            ProductNotice().IsOpen(true);
            ProductNotice().Severity(InfoBarSeverity::Informational);
            ProductNotice().Message(Text(L"PlaygroundBuilderNotice"));
        }
        if (mode == L"video")
        {
            StoryboardJson().Text(L"{\"version\":1,\"shots\":[]}");
            ProductNotice().IsOpen(true);
            ProductNotice().Severity(InfoBarSeverity::Warning);
            ProductNotice().Message(Text(L"PlaygroundVideoNotice"));
        }
    }
    fire_and_forget PlaygroundPage::OnLoaded(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        m_loaded = true;
        if (!m_initialized)
        {
            m_initialized = true;
            if (m_mode == L"agent-builder")
                DrawWorkflow();
            if (m_mode == L"audio")
                OnAudioModeChanged({}, SelectionChangedEventArgs{nullptr});
            if (m_mode != L"video")
                co_await LoadModels();
        }
    }
    fire_and_forget PlaygroundPage::OnUnloaded(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        m_loaded = false;
        ++m_version;
        m_stop.request_stop();
        if (m_busy)
            Notify(L"PlaygroundCancelled");
        if (m_recordTimer)
            m_recordTimer.Stop();
        auto capture = std::exchange(m_capture, nullptr);
        auto stream = std::exchange(m_recordStream, nullptr);
        bool recording = std::exchange(m_recording, false);
        SetBusy(false);
        RecordButton().Content(box_value(Text(L"PlaygroundRecord/Content")));
        while (m_editors.size() > m_messages.size())
        {
            m_editors.pop_back();
            ConversationList().Items().RemoveAtEnd();
        }
        m_streamEditor = nullptr;
        if (capture)
        {
            try
            {
                if (recording)
                    co_await capture.StopRecordAsync();
                capture.Close();
                if (stream)
                    stream.Close();
            }
            catch (...)
            {
            }
        }
        if (AudioPlayer().MediaPlayer())
            AudioPlayer().MediaPlayer().Pause();
        if (VideoPlayer().MediaPlayer())
            VideoPlayer().MediaPlayer().Pause();
    }
    void PlaygroundPage::OnThemeChanged(FrameworkElement const &, Windows::Foundation::IInspectable const &)
    {
        if (m_initialized && m_mode == L"agent-builder")
            DrawConnections();
    }
    void PlaygroundPage::OnWorkspaceSizeChanged(Windows::Foundation::IInspectable const &,
                                                SizeChangedEventArgs const &args)
    {
        WorkspaceSplit().OpenPaneLength(std::min(320.0, std::max(200.0, static_cast<double>(args.NewSize().Width))));
        bool narrow = args.NewSize().Width < 900;
        if (m_mode == L"video")
        {
            WorkspaceSplit().IsPaneOpen(false);
            return;
        }
        if (narrow != m_narrow)
        {
            m_narrow = narrow;
            WorkspaceSplit().DisplayMode(narrow ? SplitViewDisplayMode::Overlay : SplitViewDisplayMode::Inline);
            WorkspaceSplit().IsPaneOpen(!narrow);
        }
    }
    void PlaygroundPage::OnParameters(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        WorkspaceSplit().IsPaneOpen(!WorkspaceSplit().IsPaneOpen());
    }
    void PlaygroundPage::SetBusy(bool busy)
    {
        m_busy = busy;
        EnableTree(ParametersPanel(), !busy);
        RunButton().IsEnabled(!busy && !m_recording);
        StopButton().IsEnabled(busy);
        AttachButton().IsEnabled(!busy && !m_recording);
        RemoveAttachments().IsEnabled(!busy && !m_recording);
        RecordButton().IsEnabled(!busy);
        ClearButton().IsEnabled(!busy && !m_recording);
        ImportButton().IsEnabled(!busy && !m_recording);
        ExportButton().IsEnabled(!busy && !m_recording);
        CodeButton().IsEnabled(!busy && !m_recording);
        InputBox().IsEnabled(!busy);
        ConversationList().IsEnabled(!busy);
        EnableTree(WorkflowWorkspace(), !busy);
        AddMessageButton().IsEnabled(!busy && !m_recording);
        BusyProgress().Visibility(busy ? Visibility::Visible : Visibility::Collapsed);
    }
    void PlaygroundPage::SelectModel(std::wstring const &model)
    {
        int index = -1;
        for (uint32_t i = 0; i < ModelPicker().Items().Size(); ++i)
            if (unbox_value<hstring>(ModelPicker().Items().GetAt(i)) == hstring{model})
            {
                index = static_cast<int>(i);
                break;
            }
        if (index < 0)
        {
            index = static_cast<int>(ModelPicker().Items().Size());
            ModelPicker().Items().Append(box_value(hstring{model}));
        }
        ModelPicker().SelectedIndex(index);
        ModelPicker().Text(model);
    }
    void PlaygroundPage::Error()
    {
        try
        {
            throw;
        }
        catch (ApiError const &error)
        {
            bool key = false;
            wchar_t const *message = L"PlaygroundNetworkError";
            switch (error.code)
            {
            case ApiErrorCode::MissingCredential:
            case ApiErrorCode::CredentialUnavailable:
            case ApiErrorCode::Unauthorized:
                key = true;
                message = L"PlaygroundMissingKey";
                break;
            case ApiErrorCode::Forbidden:
                key = true;
                message = L"PlaygroundForbidden";
                break;
            case ApiErrorCode::RateLimited:
                message = L"PlaygroundRateLimited";
                break;
            case ApiErrorCode::Timeout:
                message = L"PlaygroundTimeout";
                break;
            case ApiErrorCode::Cancelled:
                message = L"PlaygroundCancelled";
                break;
            case ApiErrorCode::InvalidResponse:
                message = L"PlaygroundInvalidResponse";
                break;
            case ApiErrorCode::Server:
            case ApiErrorCode::Network:
                break;
            default:
                break;
            }
            SettingsLink().Visibility(key ? Visibility::Visible : Visibility::Collapsed);
            Notify(message, InfoBarSeverity::Warning);
        }
        catch (std::invalid_argument const &)
        {
            Notify(L"PlaygroundInvalidInput", InfoBarSeverity::Warning);
        }
        catch (hresult_access_denied const &)
        {
            Notify(L"PlaygroundFilePermission", InfoBarSeverity::Warning);
        }
        catch (...)
        {
            Notify(L"PlaygroundOperationFailed", InfoBarSeverity::Warning);
        }
    }
    Windows::Foundation::IAsyncAction PlaygroundPage::LoadModels()
    {
        auto lifetime = get_strong();
        if (m_busy)
            co_return;
        SetBusy(true);
        auto version = ++m_version;
        m_stop = std::stop_source{};
        auto stop = m_stop.get_token();
        std::vector<ModelInfo> models;
        std::exception_ptr failure;
        apartment_context ui;
        co_await resume_background();
        try
        {
            models = PlatformApiClient{m_apiTransport}.ListModels(stop);
        }
        catch (...)
        {
            failure = std::current_exception();
        }
        co_await ui;
        if (!m_loaded || version != m_version)
            co_return;
        SetBusy(false);
        if (failure)
        {
            try
            {
                std::rethrow_exception(failure);
            }
            catch (...)
            {
                Error();
            }
            co_return;
        }
        auto text = ModelPicker().Text();
        ModelPicker().Items().Clear();
        std::sort(models.begin(), models.end(), [](auto const &a, auto const &b) { return a.id < b.id; });
        for (auto const &model : models)
            ModelPicker().Items().Append(box_value(hstring{model.id}));
        SelectModel(std::wstring{text});
        SettingsLink().Visibility(Visibility::Collapsed);
        Notice().IsOpen(false);
        if (models.empty())
            Notify(L"PlaygroundNoModels");
    }
    fire_and_forget PlaygroundPage::OnRefreshModels(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        co_await LoadModels();
    }
    std::wstring PlaygroundPage::Options()
    {
        auto options = JsonObject::Parse(OptionsBox().Text().empty() ? L"{}" : OptionsBox().Text());
        auto string = [&](wchar_t const *key, std::wstring const &value) {
            if (!options.HasKey(key))
                Set(options, key, value);
        };
        if (m_mode == L"chat")
        {
            auto count = MaxTokens().Value();
            Require(std::isfinite(count) && std::floor(count) == count && count >= 16 && count <= 128000);
            if (!options.HasKey(L"max_output_tokens"))
                options.SetNamedValue(L"max_output_tokens", JsonValue::CreateNumberValue(count));
            if (std::isfinite(TemperatureBox().Value()) && !options.HasKey(L"temperature"))
                options.SetNamedValue(L"temperature", JsonValue::CreateNumberValue(TemperatureBox().Value()));
            if (ReasoningPicker().SelectedIndex() > 0 && !options.HasKey(L"reasoning"))
            {
                JsonObject reasoning;
                Set(reasoning, L"effort", Value(ReasoningPicker()));
                options.SetNamedValue(L"reasoning", reasoning);
            }
            if (WebSearchBox().IsChecked().Value() && !options.HasKey(L"tools"))
                options.SetNamedValue(L"tools", JsonArray::Parse(L"[{\"type\":\"web_search\"}]"));
        }
        else if (m_mode == L"images")
        {
            string(L"size", Value(SizePicker()));
            string(L"quality", Value(QualityPicker()));
            string(L"background", Value(BackgroundPicker()));
            string(L"output_format", Value(ImageFormatPicker()));
            auto count = ImageCount().Value();
            Require(std::isfinite(count) && std::floor(count) == count && count >= 1 && count <= 10);
            if (!options.HasKey(L"n"))
                options.SetNamedValue(L"n", JsonValue::CreateNumberValue(count));
        }
        else if (m_mode == L"audio")
        {
            int mode = AudioModePicker().SelectedIndex();
            if (mode == 0 || mode == 3)
            {
                string(L"voice", Value(VoicePicker()));
                if (!SpeechInstructions().Text().empty())
                    string(L"instructions", std::wstring{SpeechInstructions().Text()});
            }
            if (mode == 1 && !AudioLanguage().Text().empty())
                string(L"language", std::wstring{AudioLanguage().Text()});
        }
        return options.Stringify().c_str();
    }
    void PlaygroundPage::AppendMessage(ChatMessage const &message)
    {
        StackPanel panel;
        panel.Spacing(8);
        panel.Margin({4, 8, 4, 12});
        ComboBox role;
        for (auto name : {L"user", L"assistant", L"developer", L"system"})
        {
            if (m_mode == L"audio" && (std::wstring_view{name} == L"developer" || std::wstring_view{name} == L"system"))
                continue;
            ComboBoxItem item;
            item.Content(box_value(name));
            role.Items().Append(item);
        }
        role.SelectedIndex(message.role == L"assistant"   ? 1
                           : message.role == L"developer" ? 2
                           : message.role == L"system"    ? 3
                                                          : 0);
        Automation::AutomationProperties::SetName(role, Text(L"PlaygroundMessageRole"));
        Automation::AutomationProperties::SetAutomationId(role, L"MessageRole" + to_hstring(m_editors.size()));
        TextBox text;
        text.Text(message.text);
        text.AcceptsReturn(true);
        text.TextWrapping(TextWrapping::Wrap);
        text.MinHeight(72);
        text.MaxHeight(260);
        text.MaxLength(1048576);
        Automation::AutomationProperties::SetName(text, Text(L"PlaygroundMessageText"));
        Automation::AutomationProperties::SetAutomationId(text, L"MessageText" + to_hstring(m_editors.size()));
        panel.Children().Append(role);
        panel.Children().Append(text);
        if (!message.attachments.empty())
        {
            TextBlock count;
            count.Text(Text(L"PlaygroundAttachedCount") + L" " + to_hstring(message.attachments.size()));
            panel.Children().Append(count);
        }
        ConversationList().Items().Append(panel);
        m_editors.push_back({role, text});
    }
    void PlaygroundPage::ReadMessages()
    {
        for (size_t i = 0; i < m_messages.size(); ++i)
        {
            m_messages[i].role = Value(m_editors[i].role);
            m_messages[i].text = m_editors[i].text.Text();
        }
    }
    void PlaygroundPage::OnAddMessage(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        if (m_busy)
            return;
        ReadMessages();
        if (m_messages.size() >= 256)
        {
            Notify(L"PlaygroundInvalidInput", InfoBarSeverity::Warning);
            return;
        }
        m_messages.push_back({L"user", std::wstring{InputBox().Text()}, m_attachments});
        AppendMessage(m_messages.back());
        InputBox().Text(L"");
        m_attachments.clear();
        AttachmentLabel().Text(L"");
    }
    fire_and_forget PlaygroundPage::OnRun(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        if (m_busy || m_recording || m_mode == L"video")
            co_return;
        std::wstring options, model, input, instructions;
        std::vector<ChatMessage> messages;
        Workflow workflow;
        auto attachments = m_attachments;
        try
        {
            options = Options();
            model = ModelPicker().Text();
            input = InputBox().Text();
            instructions = InstructionsBox().Text();
            if (m_mode == L"chat")
            {
                ReadMessages();
                if (!input.empty() || !attachments.empty())
                {
                    m_messages.push_back({L"user", input, attachments});
                    AppendMessage(m_messages.back());
                    InputBox().Text(L"");
                    m_attachments.clear();
                    AttachmentLabel().Text(L"");
                }
                Require(!m_messages.empty());
                messages = m_messages;
            }
            else if (m_mode == L"agent-builder")
            {
                workflow = m_workflow;
                ValidateWorkflow(workflow);
                Require(!input.empty());
            }
            else if (m_mode == L"audio" && AudioModePicker().SelectedIndex() == 0)
            {
                ReadMessages();
                if (input.empty() && attachments.empty() && !m_messages.empty() && m_messages.back().role == L"user")
                {
                    input = m_messages.back().text;
                    attachments = m_messages.back().attachments;
                    m_messages.pop_back();
                    m_editors.pop_back();
                    ConversationList().Items().RemoveAtEnd();
                }
                Require(!input.empty() || !attachments.empty());
                messages = m_messages;
                m_messages.push_back({L"user", input, attachments});
                AppendMessage(m_messages.back());
                InputBox().Text(L"");
                m_attachments.clear();
                AttachmentLabel().Text(L"");
            }
            else
                Require(!input.empty() || !attachments.empty());
        }
        catch (...)
        {
            Error();
            co_return;
        }
        SetBusy(true);
        m_stop = std::stop_source{};
        auto stop = m_stop.get_token();
        auto version = ++m_version;
        SettingsLink().Visibility(Visibility::Collapsed);
        Notify(L"PlaygroundRunning");
        m_result = {};
        OutputText().Text(L"");
        DetailsText().Text(L"");
        m_streamEditor = nullptr;
        ImageGallery().Items().Clear();
        ImageGallery().Visibility(Visibility::Collapsed);
        AudioPlayer().Source(nullptr);
        AudioPlayer().Visibility(Visibility::Collapsed);
        m_streams.clear();
        AssetPicker().Items().Clear();
        bool const conversation = m_mode == L"chat" || (m_mode == L"audio" && AudioModePicker().SelectedIndex() == 0);
        if (conversation)
        {
            AppendMessage({L"assistant", L"", {}});
            m_streamEditor = m_editors.back().text;
        }
        int audioMode = AudioModePicker().SelectedIndex();
        GenerationResult result;
        std::exception_ptr failure;
        auto mode = m_mode;
        auto queue = DispatcherQueue();
        auto weak = get_weak();
        auto update = [queue, weak, version](std::wstring const &text) {
            queue.TryEnqueue([weak, version, text] {
                if (auto self = weak.get(); self && self->m_loaded && self->m_version == version)
                {
                    self->OutputText().Text(text);
                    if (self->m_streamEditor)
                        self->m_streamEditor.Text(text);
                }
            });
        };
        apartment_context ui;
        co_await resume_background();
        try
        {
            GenerationClient client{m_generationTransport};
            if (mode == L"chat")
                result = client.Chat(model, instructions, messages, options, update, stop);
            else if (mode == L"images")
                result = client.Images(model, input, options, attachments, stop);
            else if (mode == L"agent-builder")
                result = RunWorkflow(workflow, input, client, update, stop);
            else
                result = client.Audio(audioMode == 0   ? GenerationOperation::Realtime
                                      : audioMode == 1 ? GenerationOperation::Transcription
                                      : audioMode == 2 ? GenerationOperation::Translation
                                                       : GenerationOperation::Speech,
                                      model, input, options, attachments, update, stop, messages);
        }
        catch (...)
        {
            failure = std::current_exception();
        }
        co_await ui;
        if (!m_loaded || version != m_version)
            co_return;
        if (conversation)
        {
            if (m_editors.size() > m_messages.size())
            {
                m_editors.pop_back();
                ConversationList().Items().RemoveAtEnd();
            }
            m_streamEditor = nullptr;
        }
        if (failure)
        {
            SetBusy(false);
            try
            {
                std::rethrow_exception(failure);
            }
            catch (...)
            {
                Error();
            }
            co_return;
        }
        m_result = std::move(result);
        if (conversation && !m_result.text.empty())
        {
            m_messages.push_back({L"assistant", m_result.text, {}});
            AppendMessage(m_messages.back());
        }
        try
        {
            co_await RenderResult();
            if (!m_loaded || version != m_version)
                co_return;
            Notify(m_result.incomplete ? L"PlaygroundIncomplete" : L"PlaygroundCompleted",
                   m_result.incomplete ? InfoBarSeverity::Warning : InfoBarSeverity::Success);
        }
        catch (...)
        {
            if (m_loaded && version == m_version)
                Error();
        }
        if (!m_loaded || version != m_version)
            co_return;
        SetBusy(false);
        if (m_mode != L"chat")
            ResultTabs().SelectedIndex(1);
    }
    void PlaygroundPage::OnStop(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        m_stop.request_stop();
        Notify(L"PlaygroundCancelling");
    }
    Windows::Foundation::IAsyncAction PlaygroundPage::RenderResult()
    {
        auto lifetime = get_strong();
        auto version = m_version;
        // Keep assets alive across asynchronous decoding, even if this page is unloaded and reused.
        auto assets = m_result.assets;
        OutputText().Text(m_result.text);
        DetailsText().Text(m_result.details);
        ImageGallery().Items().Clear();
        AssetPicker().Items().Clear();
        AudioPlayer().Source(nullptr);
        m_streams.clear();
        ImageGallery().Visibility(Visibility::Collapsed);
        AudioPlayer().Visibility(Visibility::Collapsed);
        size_t index = 0;
        for (auto const &asset : assets)
        {
            AssetPicker().Items().Append(box_value(to_hstring(++index) + hstring{asset.extension}));
            InMemoryRandomAccessStream stream;
            DataWriter writer{stream};
            writer.WriteBytes(asset.bytes);
            co_await writer.StoreAsync();
            if (!m_loaded || version != m_version)
                co_return;
            writer.DetachStream();
            stream.Seek(0);
            m_streams.push_back(stream);
            if (asset.mime.starts_with(L"image/"))
            {
                Media::Imaging::BitmapImage bitmap;
                co_await bitmap.SetSourceAsync(stream);
                if (!m_loaded || version != m_version)
                    co_return;
                Image image;
                image.Source(bitmap);
                image.Width(280);
                image.Height(280);
                image.Stretch(Media::Stretch::Uniform);
                ImageGallery().Items().Append(image);
                ImageGallery().Visibility(Visibility::Visible);
            }
            else if (asset.mime.starts_with(L"audio/") && asset.extension != L".pcm")
            {
                AudioPlayer().Source(Windows::Media::Core::MediaSource::CreateFromStream(stream, asset.mime));
                AudioPlayer().Visibility(Visibility::Visible);
            }
        }
        if (index)
            AssetPicker().SelectedIndex(0);
        AssetPicker().Visibility(index ? Visibility::Visible : Visibility::Collapsed);
    }
    void PlaygroundPage::OnSettings(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto current = get_strong().as<DependencyObject>();
        while (current)
        {
            if (auto navigation = current.try_as<NavigationView>())
            {
                navigation.SelectedItem(navigation.SettingsItem());
                return;
            }
            current = Media::VisualTreeHelper::GetParent(current);
        }
    }
    fire_and_forget PlaygroundPage::OnClear(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        if (m_busy)
            co_return;
        if (co_await ShowConfirm(XamlRoot(), Text(L"PlaygroundClearTitle"), Text(L"PlaygroundClearBody"),
                                 Text(L"PlaygroundClear/Content"),
                                 Text(L"CancelAction")) != ContentDialogResult::Primary)
            co_return;
        m_messages.clear();
        m_editors.clear();
        ConversationList().Items().Clear();
        m_attachments.clear();
        AttachmentLabel().Text(L"");
        InputBox().Text(L"");
        m_result = {};
        co_await RenderResult();
        if (m_mode == L"agent-builder")
            ResetWorkflow();
        if (m_mode == L"video")
            StoryboardJson().Text(L"{\"version\":1,\"shots\":[]}");
        Notice().IsOpen(false);
    }
    fire_and_forget PlaygroundPage::OnAttach(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        if (m_busy)
            co_return;
        SetBusy(true);
        auto version = ++m_version;
        try
        {
            Pickers::FileOpenPicker picker{XamlRoot().ContentIslandEnvironment().AppWindowId()};
            if (m_mode == L"audio")
                for (auto extension : {L".wav", L".mp3", L".mp4", L".mpeg", L".mpga", L".m4a", L".webm"})
                    picker.FileTypeFilter().Append(extension);
            else
            {
                for (auto extension : {L".png", L".jpeg", L".jpg", L".webp"})
                    picker.FileTypeFilter().Append(extension);
                if (m_mode == L"chat")
                    picker.FileTypeFilter().Append(L".pdf");
            }
            auto files = co_await picker.PickMultipleFilesAsync();
            if (!m_loaded || version != m_version)
                co_return;
            std::vector<Attachment> incoming;
            if (files)
            {
                Require(m_attachments.size() + files.Size() <= 16);
                size_t total = 0;
                for (auto const &existing : m_attachments)
                    total += existing.bytes.size();
                for (auto const &chosen : files)
                {
                    auto file = co_await StorageFile::GetFileFromPathAsync(chosen.Path());
                    auto info = co_await file.GetBasicPropertiesAsync();
                    Require(info.Size() > 0 && info.Size() <= 25 * 1024 * 1024);
                    total += info.Size();
                    Require(total <= 64 * 1024 * 1024);
                    auto buffer = co_await FileIO::ReadBufferAsync(file);
                    if (!m_loaded || version != m_version)
                        co_return;
                    std::vector<uint8_t> bytes(buffer.Length());
                    DataReader::FromBuffer(buffer).ReadBytes(bytes);
                    incoming.push_back({std::wstring{file.Name()}, std::wstring{file.ContentType()}, std::move(bytes)});
                }
                if (m_mode == L"audio")
                {
                    Require(incoming.size() == 1);
                    m_attachments = std::move(incoming);
                }
                else
                    m_attachments.insert(m_attachments.end(), std::make_move_iterator(incoming.begin()),
                                         std::make_move_iterator(incoming.end()));
                AttachmentLabel().Text(Text(L"PlaygroundAttachedCount") + L" " + to_hstring(m_attachments.size()));
            }
        }
        catch (...)
        {
            if (m_loaded && version == m_version)
                Error();
        }
        if (m_loaded && version == m_version)
            SetBusy(false);
    }
    void PlaygroundPage::OnRemoveAttachments(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        m_attachments.clear();
        AttachmentLabel().Text(L"");
    }
    fire_and_forget PlaygroundPage::OnRecord(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        if (m_busy)
            co_return;
        auto version = ++m_version;
        try
        {
            if (m_recording)
            {
                SetBusy(true);
                if (m_recordTimer)
                    m_recordTimer.Stop();
                auto capture = std::exchange(m_capture, nullptr);
                auto stream = std::exchange(m_recordStream, nullptr);
                m_recording = false;
                co_await capture.StopRecordAsync();
                capture.Close();
                stream.Seek(0);
                Require(stream.Size() <= 25 * 1024 * 1024);
                DataReader reader{stream};
                co_await reader.LoadAsync(static_cast<uint32_t>(stream.Size()));
                std::vector<uint8_t> bytes(reader.UnconsumedBufferLength());
                reader.ReadBytes(bytes);
                reader.DetachStream();
                stream.Close();
                if (!m_loaded || version != m_version)
                    co_return;
                m_attachments = {{L"recording.wav", L"audio/wav", std::move(bytes)}};
                AttachmentLabel().Text(Text(L"PlaygroundRecordingReady"));
                RecordButton().Content(box_value(Text(L"PlaygroundRecord/Content")));
                SetBusy(false);
                co_return;
            }
            SetBusy(true);
            Windows::Media::Capture::MediaCapture capture;
            Windows::Media::Capture::MediaCaptureInitializationSettings settings;
            settings.StreamingCaptureMode(Windows::Media::Capture::StreamingCaptureMode::Audio);
            co_await capture.InitializeAsync(settings);
            if (!m_loaded || version != m_version)
            {
                capture.Close();
                co_return;
            }
            auto profile = Windows::Media::MediaProperties::MediaEncodingProfile::CreateWav(
                Windows::Media::MediaProperties::AudioEncodingQuality::Low);
            profile.Audio(Windows::Media::MediaProperties::AudioEncodingProperties::CreatePcm(24000, 1, 16));
            InMemoryRandomAccessStream stream;
            co_await capture.StartRecordToStreamAsync(profile, stream);
            if (!m_loaded || version != m_version)
            {
                co_await capture.StopRecordAsync();
                capture.Close();
                co_return;
            }
            m_capture = capture;
            m_recordStream = stream;
            m_recording = true;
            SetBusy(false);
            RunButton().IsEnabled(false);
            RecordButton().Content(box_value(Text(L"PlaygroundFinishRecording")));
            Notify(L"PlaygroundRecording");
            m_recordTimer = DispatcherQueue().CreateTimer();
            m_recordTimer.Interval(std::chrono::seconds{180});
            m_recordTimer.IsRepeating(false);
            m_recordTimer.Tick([weak = get_weak()](auto const &, auto const &) {
                if (auto self = weak.get(); self && self->m_loaded && self->m_recording)
                    self->OnRecord({}, {});
            });
            m_recordTimer.Start();
        }
        catch (...)
        {
            if (!m_loaded || version != m_version)
                co_return;
            m_recording = false;
            if (m_capture)
            {
                m_capture.Close();
                m_capture = nullptr;
            }
            if (m_recordStream)
            {
                m_recordStream.Close();
                m_recordStream = nullptr;
            }
            SetBusy(false);
            Error();
        }
    }
    void PlaygroundPage::OnAudioModeChanged(Windows::Foundation::IInspectable const &,
                                            SelectionChangedEventArgs const &)
    {
        if (!m_initialized || m_mode != L"audio")
            return;
        int mode = AudioModePicker().SelectedIndex();
        SelectModel(mode == 0   ? L"gpt-realtime-2.1-mini"
                    : mode == 1 ? L"gpt-transcribe"
                    : mode == 2 ? L"whisper-1"
                                : L"gpt-4o-mini-tts");
        ConversationList().Visibility(mode == 0 ? Visibility::Visible : Visibility::Collapsed);
        ProductNotice().IsOpen(mode == 2 || mode == 3);
        ProductNotice().Severity(InfoBarSeverity::Informational);
        ProductNotice().Message(Text(mode == 2 ? L"PlaygroundTranslationNotice" : L"PlaygroundSpeechNotice"));
    }
    Windows::Foundation::IAsyncAction PlaygroundPage::Save(std::wstring name, std::wstring extension,
                                                           std::vector<uint8_t> bytes)
    {
        auto lifetime = get_strong();
        Pickers::FileSavePicker picker{XamlRoot().ContentIslandEnvironment().AppWindowId()};
        picker.SuggestedFileName(name);
        picker.FileTypeChoices().Insert(extension, single_threaded_vector<hstring>({hstring{extension}}));
        auto result = co_await picker.PickSaveFileAsync();
        if (result)
        {
            std::filesystem::path path{result.Path().c_str()};
            auto folder = co_await StorageFolder::GetFolderFromPathAsync(path.parent_path().wstring());
            auto file =
                co_await folder.CreateFileAsync(path.filename().wstring(), CreationCollisionOption::ReplaceExisting);
            co_await FileIO::WriteBytesAsync(file, bytes);
            Notify(L"PlaygroundSaved", InfoBarSeverity::Success);
        }
    }
    std::wstring PlaygroundPage::SessionJson()
    {
        ReadMessages();
        JsonObject session;
        session.SetNamedValue(L"version", JsonValue::CreateNumberValue(1));
        Set(session, L"mode", m_mode);
        Set(session, L"model", std::wstring{ModelPicker().Text()});
        Set(session, L"instructions", std::wstring{InstructionsBox().Text()});
        Set(session, L"input", std::wstring{InputBox().Text()});
        Set(session, L"options", Options());
        session.SetNamedValue(L"audio_mode", JsonValue::CreateNumberValue(AudioModePicker().SelectedIndex()));
        JsonArray messages;
        for (auto const &message : m_messages)
        {
            JsonObject item;
            Set(item, L"role", message.role);
            Set(item, L"text", message.text);
            JsonArray files;
            for (auto const &file : message.attachments)
                files.Append(AttachmentJson(file));
            item.SetNamedValue(L"attachments", files);
            messages.Append(item);
        }
        session.SetNamedValue(L"messages", messages);
        JsonArray files;
        for (auto const &file : m_attachments)
            files.Append(AttachmentJson(file));
        session.SetNamedValue(L"attachments", files);
        return session.Stringify().c_str();
    }
    fire_and_forget PlaygroundPage::OnExport(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        try
        {
            bool code = ResultTabs().SelectedIndex() == 3 && !CodeText().Text().empty();
            auto text = code                         ? std::wstring{CodeText().Text()}
                        : m_mode == L"agent-builder" ? SerializeWorkflow(m_workflow)
                        : m_mode == L"video"         ? ValidateStoryboard(std::wstring{StoryboardJson().Text()})
                                                     : SessionJson();
            co_await Save(L"aethernexus-" + m_mode, code ? L".py" : L".json", Bytes(text));
        }
        catch (...)
        {
            Error();
        }
    }
    fire_and_forget PlaygroundPage::OnImport(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        if (m_busy)
            co_return;
        SetBusy(true);
        auto version = ++m_version;
        try
        {
            Pickers::FileOpenPicker picker{XamlRoot().ContentIslandEnvironment().AppWindowId()};
            picker.FileTypeFilter().Append(L".json");
            auto result = co_await picker.PickSingleFileAsync();
            if (!m_loaded || version != m_version)
                co_return;
            if (!result)
            {
                SetBusy(false);
                co_return;
            }
            auto file = co_await StorageFile::GetFileFromPathAsync(result.Path());
            auto info = co_await file.GetBasicPropertiesAsync();
            Require(info.Size() <= 128 * 1024 * 1024);
            auto text = co_await FileIO::ReadTextAsync(file);
            if (!m_loaded || version != m_version)
                co_return;
            if (m_mode == L"agent-builder")
            {
                auto workflow = ParseWorkflow(std::wstring{text});
                m_workflow = std::move(workflow);
                DrawWorkflow();
            }
            else if (m_mode == L"video")
                StoryboardJson().Text(ValidateStoryboard(std::wstring{text}));
            else
            {
                auto session = JsonObject::Parse(text);
                Require(session.GetNamedNumber(L"version") == 1 && session.GetNamedString(L"mode") == m_mode);
                std::vector<ChatMessage> messages;
                std::vector<Attachment> files;
                for (auto const &entry : session.GetNamedArray(L"messages"))
                {
                    auto item = entry.GetObject();
                    ChatMessage message{
                        std::wstring{item.GetNamedString(L"role")}, std::wstring{item.GetNamedString(L"text")}, {}};
                    Require(message.role == L"user" || message.role == L"assistant" || message.role == L"developer" ||
                            message.role == L"system");
                    if (m_mode == L"audio")
                        Require(message.role == L"user" || message.role == L"assistant");
                    for (auto const &attachment : item.GetNamedArray(L"attachments"))
                        message.attachments.push_back(ReadAttachment(attachment.GetObject()));
                    messages.push_back(std::move(message));
                }
                Require(messages.size() <= 256);
                for (auto const &attachment : session.GetNamedArray(L"attachments"))
                    files.push_back(ReadAttachment(attachment.GetObject()));
                Require(files.size() <= 16);
                auto model = session.GetNamedString(L"model"), input = session.GetNamedString(L"input"),
                     instructions = session.GetNamedString(L"instructions"),
                     options = session.GetNamedString(L"options");
                (void)JsonObject::Parse(options);
                if (m_mode == L"audio")
                {
                    auto audio = session.GetNamedNumber(L"audio_mode");
                    Require(audio >= 0 && audio <= 3 && std::floor(audio) == audio);
                    AudioModePicker().SelectedIndex(static_cast<int>(audio));
                }
                m_messages = std::move(messages);
                m_attachments = std::move(files);
                m_editors.clear();
                ConversationList().Items().Clear();
                for (auto const &message : m_messages)
                    AppendMessage(message);
                SelectModel(std::wstring{model});
                InputBox().Text(input);
                InstructionsBox().Text(instructions);
                OptionsBox().Text(options);
                AttachmentLabel().Text(Text(L"PlaygroundAttachedCount") + L" " + to_hstring(m_attachments.size()));
            }
            Notify(L"PlaygroundImported", InfoBarSeverity::Success);
        }
        catch (...)
        {
            if (m_loaded && version == m_version)
                Error();
        }
        if (m_loaded && version == m_version)
            SetBusy(false);
    }
    void PlaygroundPage::OnCode(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        try
        {
            if (m_mode == L"agent-builder")
                CodeText().Text(ExportWorkflowPython(m_workflow));
            else if (m_mode == L"chat")
            {
                ReadMessages();
                auto messages = m_messages;
                if (!InputBox().Text().empty() || !m_attachments.empty())
                    messages.push_back({L"user", std::wstring{InputBox().Text()}, m_attachments});
                CodeText().Text(GenerationClient::RequestCode(
                    std::wstring{ModelPicker().Text()}, std::wstring{InstructionsBox().Text()}, messages, Options()));
            }
            else
            {
                auto session = to_string(SessionJson());
                auto encoded = Base64({reinterpret_cast<uint8_t const *>(session.data()), session.size()});
                CodeText().Text(L"import base64, io, json, asyncio, wave\nfrom pathlib import Path\nfrom openai import "
                                L"OpenAI, AsyncOpenAI\ns = json.loads(base64.b64decode(\"" +
                                std::wstring{to_hstring(encoded)} + LR"("))
client = OpenAI() # Uses OPENAI_API_KEY; no key is embedded.
options = json.loads(s['options'])
files = s['attachments']
if s['mode'] == 'images':
    options.update(model=s['model'], prompt=s['input'])
    if files:
        options['images'] = [{'image_url': 'data:' + f['mime'] + ';base64,' + f['data']} for f in files]
        result = client.images.edit(**options)
    else:
        result = client.images.generate(**options)
    for i, image in enumerate(result.data):
        Path(f'output-{i}.{options.get("output_format", "png")}').write_bytes(base64.b64decode(image.b64_json))
elif s['audio_mode'] in (1, 2):
    f = files[0]
    options.update(model=s['model'], file=(f['name'], base64.b64decode(f['data']), f['mime']), response_format='json')
    if s['input']: options['prompt'] = s['input']
    method = client.audio.transcriptions.create if s['audio_mode'] == 1 else client.audio.translations.create
    print(method(**options).text)
elif s['audio_mode'] == 3:
    options.update(model=s['model'], input=s['input'])
    options.setdefault('response_format', 'wav')
    client.audio.speech.create(**options).write_to_file('output.' + options['response_format'])
else:
    async def speak():
        audio = bytearray()
        async with AsyncOpenAI().realtime.connect(model=s['model']) as conn:
            config = dict(options.get('session', {}))
            config.update({'type':'realtime', 'output_modalities':['audio'],
                'instructions': options.get('instructions',''),
                'audio': {'input': {'format': {'type':'audio/pcm','rate':24000},'turn_detection':None},
                          'output': {'format': {'type':'audio/pcm','rate':24000},'voice':options.get('voice','marin')}}})
            await conn.session.update(session=config)
            for message in s['messages']:
                content = []
                if message['text']:
                    content.append({'type':'output_text' if message['role']=='assistant' else 'input_text','text':message['text']})
                for f in message['attachments']:
                    with wave.open(io.BytesIO(base64.b64decode(f['data']))) as wav:
                        assert (wav.getframerate(),wav.getnchannels(),wav.getsampwidth()) == (24000,1,2)
                        content.append({'type':'input_audio','audio':base64.b64encode(wav.readframes(wav.getnframes())).decode()})
                await conn.conversation.item.create(item={'type':'message','role':message['role'],'content':content})
            if s['input']:
                await conn.conversation.item.create(item={'type':'message','role':'user','content':[{'type':'input_text','text':s['input']}]})
            if files:
                with wave.open(io.BytesIO(base64.b64decode(files[0]['data']))) as wav:
                    assert (wav.getframerate(), wav.getnchannels(), wav.getsampwidth()) == (24000,1,2)
                    await conn.input_audio_buffer.append(audio=base64.b64encode(wav.readframes(wav.getnframes())).decode())
                await conn.input_audio_buffer.commit()
            await conn.response.create()
            async for event in conn:
                if event.type == 'response.output_audio.delta': audio.extend(base64.b64decode(event.delta))
                elif event.type == 'response.output_audio_transcript.delta': print(event.delta, end='', flush=True)
                elif event.type == 'error': raise RuntimeError(event.error.message)
                elif event.type == 'response.done': break
        with wave.open('output.wav','wb') as wav:
            wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(24000); wav.writeframes(audio)
    asyncio.run(speak())
)");
            }
            ResultTabs().SelectedIndex(3);
        }
        catch (...)
        {
            Error();
        }
    }
    void PlaygroundPage::OnCopy(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        try
        {
            Windows::ApplicationModel::DataTransfer::DataPackage data;
            data.SetText(OutputText().Text());
            Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(data);
            Notify(L"PlaygroundCopied", InfoBarSeverity::Success);
        }
        catch (...)
        {
            Error();
        }
    }
    fire_and_forget PlaygroundPage::OnSaveOutput(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        try
        {
            auto index = AssetPicker().SelectedIndex();
            if (index >= 0 && static_cast<size_t>(index) < m_result.assets.size())
            {
                auto asset = m_result.assets[static_cast<size_t>(index)];
                co_await Save(L"output", asset.extension, std::move(asset.bytes));
            }
            else
                co_await Save(L"output", L".txt", Bytes(std::wstring{OutputText().Text()}));
        }
        catch (...)
        {
            Error();
        }
    }
    void PlaygroundPage::ResetWorkflow()
    {
        m_workflow = {L"start",
                      {{L"start", L"start", L"Start", L"", L"", L"agent", L"", L"", 64, 80},
                       {L"agent", L"agent", L"Agent", L"", L"gpt-6-astra", L"end", L"", L"", 384, 80},
                       {L"end", L"end", L"End", L"", L"", L"", L"", L"", 704, 80}}};
        if (m_initialized)
            DrawWorkflow();
    }
    void PlaygroundPage::DrawConnections()
    {
        WorkflowEdges().Children().Clear();
        auto brush = WorkflowCanvas().Tag().as<Media::Brush>();
        for (auto const &node : m_workflow.nodes)
            for (auto const &target : {node.next, node.kind == L"condition" ? node.otherwise : L""})
            {
                if (target.empty())
                    continue;
                auto found = std::find_if(m_workflow.nodes.begin(), m_workflow.nodes.end(),
                                          [&](auto const &other) { return other.id == target; });
                if (found == m_workflow.nodes.end())
                    continue;
                Shapes::Line line;
                line.X1(node.x + 220);
                line.Y1(node.y + 40);
                line.X2(found->x);
                line.Y2(found->y + 40);
                line.Stroke(brush);
                line.StrokeThickness(2);
                WorkflowEdges().Children().Append(line);
            }
    }
    void PlaygroundPage::DrawWorkflow()
    {
        m_editingNodes = true;
        int selection = NodePicker().SelectedIndex();
        NodePicker().Items().Clear();
        WorkflowNodes().Children().Clear();
        double width = 1000, height = 600;
        for (size_t i = 0; i < m_workflow.nodes.size(); ++i)
        {
            auto const &node = m_workflow.nodes[i];
            NodePicker().Items().Append(box_value(hstring{node.id + L" · " + node.kind}));
            Button button;
            button.Content(box_value(hstring{node.name + L"\n" + node.id + L" · " + node.kind}));
            button.Width(220);
            button.Height(80);
            button.Tag(box_value(static_cast<uint32_t>(i)));
            Canvas::SetLeft(button, node.x);
            Canvas::SetTop(button, node.y);
            Automation::AutomationProperties::SetAutomationId(button, L"WorkflowNode" + hstring{node.id});
            Automation::AutomationProperties::SetName(button, hstring{node.name + L" " + node.id});
            auto weak = get_weak();
            button.Click([weak, i](auto const &, auto const &) {
                if (auto self = weak.get())
                {
                    self->NodePicker().SelectedIndex(static_cast<int>(i));
                    self->WorkspaceSplit().IsPaneOpen(true);
                }
            });
            button.ManipulationMode(Input::ManipulationModes::TranslateX | Input::ManipulationModes::TranslateY);
            button.ManipulationDelta([weak, i](Windows::Foundation::IInspectable const &sender,
                                               Input::ManipulationDeltaRoutedEventArgs const &args) {
                // Mouse movement is handled by pointer capture below. Applying both paths doubles the drag distance.
                if (args.PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Mouse)
                    return;
                if (auto self = weak.get(); self && !self->m_busy)
                {
                    auto &value = self->m_workflow.nodes[i];
                    value.x = std::clamp(value.x + args.Delta().Translation.X, 0.0, 10000.0);
                    value.y = std::clamp(value.y + args.Delta().Translation.Y, 0.0, 10000.0);
                    Canvas::SetLeft(sender.as<Button>(), value.x);
                    Canvas::SetTop(sender.as<Button>(), value.y);
                    self->DrawConnections();
                    self->WorkflowJson().Text(SerializeWorkflow(self->m_workflow));
                }
            });
            struct Drag
            {
                bool active{};
                Windows::Foundation::Point origin{};
                double x{}, y{};
            };
            auto drag = std::make_shared<Drag>();
            Border surface;
            surface.Background(Media::SolidColorBrush{Windows::UI::Color{0, 0, 0, 0}});
            TextBlock label;
            label.Text(node.name + L"\n" + node.id + L" · " + node.kind);
            label.TextAlignment(TextAlignment::Center);
            label.VerticalAlignment(VerticalAlignment::Center);
            surface.Child(label);
            button.Content(surface);
            button.Padding({0, 0, 0, 0});
            button.HorizontalContentAlignment(HorizontalAlignment::Stretch);
            button.VerticalContentAlignment(VerticalAlignment::Stretch);
            surface.PointerPressed([weak, i, drag](Windows::Foundation::IInspectable const &sender,
                                                   Input::PointerRoutedEventArgs const &args) {
                if (auto self = weak.get();
                    self && !self->m_busy &&
                    args.Pointer().PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Mouse)
                {
                    auto point = args.GetCurrentPoint(self->WorkflowCanvas());
                    if (point.Properties().IsLeftButtonPressed())
                    {
                        drag->active = true;
                        drag->origin = point.Position();
                        drag->x = self->m_workflow.nodes[i].x;
                        drag->y = self->m_workflow.nodes[i].y;
                        sender.as<Border>().CapturePointer(args.Pointer());
                        args.Handled(true);
                    }
                }
            });
            surface.PointerMoved([weak, i, drag](Windows::Foundation::IInspectable const &sender,
                                                 Input::PointerRoutedEventArgs const &args) {
                if (auto self = weak.get(); self && drag->active && !self->m_busy)
                {
                    auto point = args.GetCurrentPoint(self->WorkflowCanvas()).Position();
                    auto &node = self->m_workflow.nodes[i];
                    node.x = std::clamp(drag->x + point.X - drag->origin.X, 0.0, 10000.0);
                    node.y = std::clamp(drag->y + point.Y - drag->origin.Y, 0.0, 10000.0);
                    auto button = Media::VisualTreeHelper::GetParent(sender.as<Border>());
                    while (button && !button.try_as<Button>())
                        button = Media::VisualTreeHelper::GetParent(button);
                    if (button)
                    {
                        Canvas::SetLeft(button.as<Button>(), node.x);
                        Canvas::SetTop(button.as<Button>(), node.y);
                    }
                    self->DrawConnections();
                    self->WorkflowJson().Text(SerializeWorkflow(self->m_workflow));
                    args.Handled(true);
                }
            });
            surface.PointerReleased([weak, i, drag](Windows::Foundation::IInspectable const &sender,
                                                    Input::PointerRoutedEventArgs const &args) {
                if (!drag->active)
                    return;
                drag->active = false;
                sender.as<Border>().ReleasePointerCapture(args.Pointer());
                if (auto self = weak.get())
                {
                    self->NodePicker().SelectedIndex(static_cast<int>(i));
                    if (std::abs(self->m_workflow.nodes[i].x - drag->x) < 3 &&
                        std::abs(self->m_workflow.nodes[i].y - drag->y) < 3)
                        self->WorkspaceSplit().IsPaneOpen(true);
                }
                args.Handled(true);
            });
            surface.PointerCaptureLost([drag](auto const &, auto const &) { drag->active = false; });
            button.KeyDown(
                [weak, i](Windows::Foundation::IInspectable const &sender, Input::KeyRoutedEventArgs const &args) {
                    if (auto self = weak.get(); self && !self->m_busy)
                    {
                        using Key = Windows::System::VirtualKey;
                        auto &node = self->m_workflow.nodes[i];
                        if (args.Key() == Key::Left)
                            node.x = std::max(0.0, node.x - 16);
                        else if (args.Key() == Key::Right)
                            node.x = std::min(10000.0, node.x + 16);
                        else if (args.Key() == Key::Up)
                            node.y = std::max(0.0, node.y - 16);
                        else if (args.Key() == Key::Down)
                            node.y = std::min(10000.0, node.y + 16);
                        else
                            return;
                        Canvas::SetLeft(sender.as<Button>(), node.x);
                        Canvas::SetTop(sender.as<Button>(), node.y);
                        self->DrawConnections();
                        self->WorkflowJson().Text(SerializeWorkflow(self->m_workflow));
                        args.Handled(true);
                    }
                });
            WorkflowNodes().Children().Append(button);
            width = std::max(width, node.x + 300);
            height = std::max(height, node.y + 160);
        }
        WorkflowCanvas().Width(width);
        WorkflowCanvas().Height(height);
        WorkflowNodes().Width(width);
        WorkflowNodes().Height(height);
        WorkflowEdges().Width(width);
        WorkflowEdges().Height(height);
        NodePicker().SelectedIndex(std::clamp(selection, 0, static_cast<int>(m_workflow.nodes.size()) - 1));
        m_editingNodes = false;
        OnNodeSelected({}, SelectionChangedEventArgs{nullptr});
        DrawConnections();
        WorkflowJson().Text(SerializeWorkflow(m_workflow));
    }
    void PlaygroundPage::OnNodeSelected(Windows::Foundation::IInspectable const &, SelectionChangedEventArgs const &)
    {
        if (m_editingNodes)
            return;
        auto index = NodePicker().SelectedIndex();
        if (index < 0 || static_cast<size_t>(index) >= m_workflow.nodes.size())
            return;
        auto const &node = m_workflow.nodes[static_cast<size_t>(index)];
        NodeName().Text(node.name);
        NodeModel().Text(node.model);
        NodeInstructions().Text(node.instructions);
        NodeNext().Text(node.next);
        NodeOtherwise().Text(node.otherwise);
        NodeMatch().Text(node.match);
    }
    void PlaygroundPage::OnApplyNode(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        try
        {
            auto index = NodePicker().SelectedIndex();
            Require(index >= 0 && static_cast<size_t>(index) < m_workflow.nodes.size());
            auto flow = m_workflow;
            auto &node = flow.nodes[static_cast<size_t>(index)];
            node.name = NodeName().Text();
            node.model = NodeModel().Text();
            node.instructions = NodeInstructions().Text();
            node.next = NodeNext().Text();
            node.otherwise = NodeOtherwise().Text();
            node.match = NodeMatch().Text();
            ValidateWorkflow(flow);
            m_workflow = std::move(flow);
            DrawWorkflow();
            Notify(L"PlaygroundNodeApplied", InfoBarSeverity::Success);
        }
        catch (...)
        {
            Error();
        }
    }
    void PlaygroundPage::OnAddNode(Windows::Foundation::IInspectable const &sender, RoutedEventArgs const &)
    {
        auto kind = std::wstring{unbox_value<hstring>(sender.as<Button>().Tag())};
        size_t index = m_workflow.nodes.size();
        std::wstring id;
        do
        {
            id = L"node_" + std::to_wstring(index++);
        } while (std::any_of(m_workflow.nodes.begin(), m_workflow.nodes.end(),
                             [&](auto const &node) { return node.id == id; }));
        if (m_workflow.nodes.size() >= 64)
        {
            Notify(L"PlaygroundInvalidInput", InfoBarSeverity::Warning);
            return;
        }
        auto selected = NodePicker().SelectedIndex();
        auto next = std::wstring{L"end"};
        if (selected >= 0 && static_cast<size_t>(selected) < m_workflow.nodes.size() &&
            m_workflow.nodes[static_cast<size_t>(selected)].kind != L"end")
        {
            next = m_workflow.nodes[static_cast<size_t>(selected)].next;
            m_workflow.nodes[static_cast<size_t>(selected)].next = id;
        }
        m_workflow.nodes.push_back({id, kind, kind, L"", std::wstring{ModelPicker().Text()}, next, next, L"",
                                    128.0 + 48.0 * static_cast<double>(m_workflow.nodes.size()), 240});
        DrawWorkflow();
        NodePicker().SelectedIndex(static_cast<int>(m_workflow.nodes.size()) - 1);
    }
    fire_and_forget PlaygroundPage::OnDeleteNode(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        int index = NodePicker().SelectedIndex();
        if (index < 0 || static_cast<size_t>(index) >= m_workflow.nodes.size())
            co_return;
        auto node = m_workflow.nodes[static_cast<size_t>(index)];
        if (node.kind == L"start" || node.kind == L"end")
        {
            Notify(L"PlaygroundKeepEndpoints", InfoBarSeverity::Warning);
            co_return;
        }
        if (co_await ShowConfirm(XamlRoot(), Text(L"PlaygroundDeleteNode/Content"),
                                 hstring{node.name + L" · " + node.id}, Text(L"PlaygroundDeleteNode/Content"),
                                 Text(L"CancelAction")) != ContentDialogResult::Primary)
            co_return;
        m_workflow.nodes.erase(m_workflow.nodes.begin() + index);
        for (auto &other : m_workflow.nodes)
        {
            if (other.next == node.id)
                other.next = node.next;
            if (other.otherwise == node.id)
                other.otherwise = node.next;
        }
        DrawWorkflow();
    }
    void PlaygroundPage::OnApplyWorkflow(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        try
        {
            auto flow = ParseWorkflow(std::wstring{WorkflowJson().Text()});
            m_workflow = std::move(flow);
            DrawWorkflow();
            Notify(L"PlaygroundNodeApplied", InfoBarSeverity::Success);
        }
        catch (...)
        {
            Error();
        }
    }
    void PlaygroundPage::OnAddShot(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        try
        {
            Require(!InputBox().Text().empty());
            auto object = JsonObject::Parse(StoryboardJson().Text());
            auto shots = object.GetNamedArray(L"shots");
            Require(shots.Size() < 100);
            JsonObject shot;
            Set(shot, L"prompt", std::wstring{InputBox().Text()});
            shot.SetNamedValue(L"seconds", JsonValue::CreateNumberValue(8));
            shots.Append(shot);
            StoryboardJson().Text(ValidateStoryboard(std::wstring{object.Stringify()}));
            InputBox().Text(L"");
        }
        catch (...)
        {
            Error();
        }
    }
    fire_and_forget PlaygroundPage::OnOpenVideo(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        auto version = m_version;
        try
        {
            Pickers::FileOpenPicker picker{XamlRoot().ContentIslandEnvironment().AppWindowId()};
            for (auto extension : {L".mp4", L".webm", L".mov"})
                picker.FileTypeFilter().Append(extension);
            auto result = co_await picker.PickSingleFileAsync();
            if (result)
            {
                auto file = co_await StorageFile::GetFileFromPathAsync(result.Path());
                if (!m_loaded || version != m_version)
                    co_return;
                VideoPlayer().Source(Windows::Media::Core::MediaSource::CreateFromStorageFile(file));
                VideoPlayer().Visibility(Visibility::Visible);
            }
        }
        catch (...)
        {
            if (m_loaded && version == m_version)
                Error();
        }
    }
} // namespace winrt::AetherNexus::implementation
