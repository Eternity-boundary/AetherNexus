// Copyright (c) 2026 Skylight Software Organization.
// Created by EternityBoundary on 11,4 2026.
// All rights reserved.
// SPDX-License-Identifier: GPL-3.0-only
//
// File: ManagePage.xaml.cpp
// Project: AetherNexus
#include "pch.h"
#include "ManagePage.xaml.h"
#include "ManagePage.g.cpp"
#include "Models/ManageItem.h"
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <format>
#include <limits>
#include <sstream>
#include <winrt/Microsoft.UI.Interop.h>

namespace winrt::AetherNexus::implementation
{
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;
    using namespace ::AetherNexus::Services;
    using namespace std::chrono;
    namespace
    {
        wchar_t const *ContentKindResource(ResponseContentKind value)
        {
            switch (value)
            {
            case ResponseContentKind::Json: return L"ManageResponseJson";
            case ResponseContentKind::Html: return L"ManageResponseHtml";
            case ResponseContentKind::Other: return L"ManageResponseOther";
            case ResponseContentKind::Unknown: return L"ManageResponseUnknown";
            }
            return L"ManageResponseUnknown";
        }
        wchar_t const *BodyKindResource(ResponseBodyKind value)
        {
            switch (value)
            {
            case ResponseBodyKind::Empty: return L"ManageBodyEmpty";
            case ResponseBodyKind::JsonError: return L"ManageBodyJsonError";
            case ResponseBodyKind::JsonOther: return L"ManageBodyJsonOther";
            case ResponseBodyKind::HtmlLike: return L"ManageBodyHtml";
            case ResponseBodyKind::Other: return L"ManageBodyOther";
            case ResponseBodyKind::InvalidJson: return L"ManageBodyInvalidJson";
            case ResponseBodyKind::Truncated: return L"ManageBodyTruncated";
            case ResponseBodyKind::ReadFailed: return L"ManageBodyReadFailed";
            case ResponseBodyKind::NotRead: return L"ManageBodyNotRead";
            }
            return L"ManageBodyNotRead";
        }
        wchar_t const *ErrorTypeResource(ResponseErrorType value)
        {
            switch (value)
            {
            case ResponseErrorType::InvalidRequest: return L"ManageErrorTypeInvalidRequest";
            case ResponseErrorType::Authentication: return L"ManageErrorTypeAuthentication";
            case ResponseErrorType::Permission: return L"ManageErrorTypePermission";
            case ResponseErrorType::NotFound: return L"ManageErrorTypeNotFound";
            case ResponseErrorType::RateLimit: return L"ManageErrorTypeRateLimit";
            case ResponseErrorType::Server: return L"ManageErrorTypeServer";
            case ResponseErrorType::Unknown: return L"ManageErrorTypeUnknown";
            case ResponseErrorType::None: return L"ManageErrorTypeUnknown";
            }
            return L"ManageErrorTypeUnknown";
        }
        wchar_t const *RequestIdStateResource(ResponseRequestIdState value)
        {
            switch (value)
            {
            case ResponseRequestIdState::Missing: return L"ManageRequestIdMissing";
            case ResponseRequestIdState::TooLong: return L"ManageRequestIdTooLong";
            case ResponseRequestIdState::Rejected: return L"ManageRequestIdRejected";
            case ResponseRequestIdState::ReadFailed: return L"ManageRequestIdReadFailed";
            case ResponseRequestIdState::Valid:
            case ResponseRequestIdState::NotRead: return L"ManageRequestIdUnavailable";
            }
            return L"ManageRequestIdUnavailable";
        }
        void Require(bool value)
        {
            if (!value)
                throw std::invalid_argument("Invalid management input");
        }
        std::wstring Pick(ComboBox const &box)
        {
            return box.SelectedItem() ? std::wstring{unbox_value<hstring>(box.SelectedItem())} : L"";
        }
        std::wstring Day(int64_t seconds)
        {
            auto date = year_month_day{floor<days>(sys_seconds{std::chrono::seconds{seconds}})};
            return std::format(L"{:04}-{:02}-{:02}", static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
                               static_cast<unsigned>(date.day()));
        }
        std::wstring Time(std::optional<int64_t> seconds)
        {
            if (!seconds)
                return L"—";
            auto instant = sys_seconds{std::chrono::seconds{*seconds}};
            auto clock = hh_mm_ss{instant - floor<days>(instant)};
            return std::format(L"{} {:02}:{:02} UTC", Day(*seconds), clock.hours().count(), clock.minutes().count());
        }
        Windows::Foundation::DateTime CalendarDate(sys_days day)
        {
            Windows::Globalization::Calendar calendar;
            calendar.ChangeCalendarSystem(L"GregorianCalendar");
            auto ymd = year_month_day{day};
            calendar.Day(1);
            calendar.Year(static_cast<int>(ymd.year()));
            calendar.Month(static_cast<int>(static_cast<unsigned>(ymd.month())));
            calendar.Day(static_cast<int>(static_cast<unsigned>(ymd.day())));
            calendar.Hour(12);
            return calendar.GetDateTime();
        }
        sys_days ReadDate(CalendarDatePicker const &picker)
        {
            Require(picker.Date() != nullptr);
            Windows::Globalization::Calendar calendar;
            calendar.ChangeCalendarSystem(L"GregorianCalendar");
            calendar.SetDateTime(picker.Date().Value());
            auto date = year{calendar.Year()} / month{static_cast<unsigned>(calendar.Month())} /
                        day{static_cast<unsigned>(calendar.Day())};
            Require(date.ok());
            return sys_days{date};
        }
        std::wstring Fold(std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return value;
        }
        std::wstring Number(double value)
        {
            return std::format(L"{:.12g}", value);
        }
        Visibility Visible(bool value)
        {
            return value ? Visibility::Visible : Visibility::Collapsed;
        }
        AetherNexus::ManageItem Item(std::wstring const &id, std::wstring const &title, std::wstring const &subtitle,
                                     std::wstring const &detail)
        {
            AetherNexus::ManageItem item;
            get_self<ManageItem>(item)->Set(hstring{id}, hstring{title}, hstring{subtitle}, hstring{detail});
            return item;
        }
        TextBox Input(hstring const &label, std::wstring const &value = {})
        {
            TextBox box;
            box.Header(box_value(label));
            box.Text(value);
            box.MaxLength(4096);
            return box;
        }
        TextBlock Hint(hstring const &text)
        {
            TextBlock block;
            block.Text(text);
            block.TextWrapping(TextWrapping::Wrap);
            return block;
        }
    } // namespace
    hstring ManagePage::Text(wchar_t const *key) const
    {
        return Microsoft::Windows::ApplicationModel::Resources::ResourceLoader{}.GetString(key);
    }
    void ManagePage::Notify(wchar_t const *key, InfoBarSeverity severity)
    {
        OfficialAdminLink().Visibility(Visibility::Collapsed);
        AdminBugReportLink().Visibility(Visibility::Collapsed);
        Notice().Message(Text(key));
        Notice().Severity(severity);
        Notice().IsOpen(true);
    }
    void ManagePage::Configure(hstring const &mode, std::shared_ptr<IManagementTransport> transport,
                              std::shared_ptr<IKeyIssuanceTransport> issuer)
    {
        Require(transport && (mode == L"usage" || mode == L"keys" || mode == L"projects"));
        m_mode = mode;
        m_transport = std::move(transport);
        m_issuer = std::move(issuer);
        PageTitle().Text(mode == L"usage" ? L"Usage" : mode == L"keys" ? L"API keys" : L"Projects");
        UsageDates().Visibility(Visible(mode == L"usage"));
        UsageOptions().Visibility(UsageDates().Visibility());
        UsageSummary().Visibility(Visibility::Collapsed);
        ExportButton().Visibility(UsageDates().Visibility());
        CreateButton().Visibility(Visible(mode == L"projects"));
        IssueButton().Visibility(Visible(mode == L"keys"));
        EditButton().Visibility(Visible(mode != L"usage"));
        RemoveButton().Visibility(EditButton().Visibility());
        ViewPicker().Header(box_value(Text(mode == L"usage"  ? L"ManageCategory"
                                           : mode == L"keys" ? L"ManageKeyKind"
                                                             : L"ManageProjectView")));
        if (mode == L"usage")
            for (auto const &category : UsageCategories())
                ViewPicker().Items().Append(box_value(Text((L"ManageCategory_" + category).c_str())));
        else if (mode == L"keys")
        {
            ViewPicker().Items().Append(box_value(Text(L"ManageProjectKeys")));
            ViewPicker().Items().Append(box_value(Text(L"ManageAdminKeys")));
        }
        else
        {
            ViewPicker().Items().Append(box_value(Text(L"ManageProjectOverview")));
            ViewPicker().Items().Append(box_value(Text(L"ManageMembers")));
            ViewPicker().Items().Append(box_value(Text(L"ManageRateLimits")));
        }
        ViewPicker().SelectedIndex(0);
        EmptyText().Text(Text(L"ManageInitial"));
        PageHint().Text(Text(mode == L"usage"  ? L"ManageUsageHint"
                             : mode == L"keys" ? L"ManageKeysHint"
                                               : L"ManageProjectsHint"));
        if (mode == L"usage")
            FilterUsageChoices();
        Actions();
    }
    fire_and_forget ManagePage::OnLoaded(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        m_loaded = true;
        if (!m_initialized)
        {
            m_editing = true;
            auto today = floor<days>(system_clock::now());
            StartDate().Date(CalendarDate(today - days{6}));
            EndDate().Date(CalendarDate(today));
            m_editing = false;
            m_initialized = true;
            co_await Load(true);
        }
        else if (m_reload)
        {
            m_reload = false;
            co_await Load(true);
        }
    }
    void ManagePage::OnUnloaded(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        m_loaded = false;
        ClearDisclosure();
        ++m_version;
        m_stop.request_stop();
        if (m_busy)
        {
            m_reload = true;
            m_valid = false;
            Notify(m_mutating ? L"ManageMutationUncertain" : L"ManageCancelled", InfoBarSeverity::Warning);
        }
        m_mutating = false;
        Busy(false);
    }
    void ManagePage::Error(bool mutation)
    {
        bool settings = false;
        wchar_t const *key = mutation ? L"ManageMutationUncertain" : L"ManageFailed";
        try
        {
            throw;
        }
        catch (ApiError const &error)
        {
            switch (error.code)
            {
            case ApiErrorCode::MissingCredential:
            case ApiErrorCode::CredentialUnavailable:
            case ApiErrorCode::Unauthorized:
                key = L"ManageMissingKey";
                settings = true;
                break;
            case ApiErrorCode::Forbidden:
                key = L"ManageForbidden";
                settings = true;
                break;
            case ApiErrorCode::RateLimited:
                key = mutation ? L"ManageMutationUncertain" : L"ManageRateLimited";
                break;
            case ApiErrorCode::Cancelled:
                key = mutation ? L"ManageMutationUncertain" : L"ManageCancelled";
                break;
            case ApiErrorCode::InvalidResponse:
                key = mutation ? L"ManageMutationUncertain" : L"ManageInvalidResponse";
                break;
            case ApiErrorCode::Timeout:
            case ApiErrorCode::Network:
            case ApiErrorCode::Server:
                break;
            }
        }
        catch (ManagementKeyTypeError const &)
        {
            key = L"ManageKeyTypeMismatch";
        }
        catch (ManagementHttpError const &error)
        {
            auto const &facts = error.diagnostics;
            // Response-only guidance: never cache an unsupported capability or skip the API.
            // A later successful DELETE follows the existing Mutate/Load success path.
            bool adminRevocation404 = error.status == 404 && error.operation == ManagementOperation::AdminRevocation;
            key = adminRevocation404 ? L"ManageAdminRevocationNotFound"
                : error.status == 404 ? L"ManageRemoteNotFound"
                : error.status == 409 ? L"ManageRemoteConflict"
                : error.status == 415 ? L"ManageRemoteFormat"
                                     : L"ManageRemoteRejected";
            SettingsLink().Visibility(Visibility::Collapsed);
            Notify(key, InfoBarSeverity::Warning);
            OfficialAdminLink().Visibility(Visible(adminRevocation404));
            AdminBugReportLink().Visibility(Visible(adminRevocation404));
            auto message = Text(key) + L" (HTTP " + hstring{std::to_wstring(error.status)} + L")";
            if (error.operation == ManagementOperation::AdminVerification)
                message = message + L"\n" + Text(L"ManageAdminVerificationFailed") +
                          L"\nGET /v1/organization/admin_api_keys/{key_id}";
            else if (error.operation == ManagementOperation::AdminRevocation)
                message = message + L"\n" + Text(L"ManageAdminRevocationFailed") +
                          L"\nDELETE /v1/organization/admin_api_keys/{key_id}";
            else if (error.operation == ManagementOperation::ProjectRevocation)
                message = message + L"\n" + Text(L"ManageProjectRevocationFailed") +
                          L"\nDELETE /v1/organization/projects/{project_id}/api_keys/{api_key_id}";
            if (!error.requestId.empty())
                message = message + L"\nRequest ID: " + hstring{error.requestId};
            else if (error.operation != ManagementOperation::Other)
                message = message + L"\n" + Text(RequestIdStateResource(error.diagnostics.requestIdState));
            if (!facts.clientRequestId.empty())
                message = message + L"\nClient Request ID: " + hstring{facts.clientRequestId};
            if (facts.requestIdState != ResponseRequestIdState::NotRead || facts.body.kind != ResponseBodyKind::NotRead)
            {
                message = message + L"\n" + Text(ContentKindResource(facts.content)) + L" · " + Text(BodyKindResource(facts.body.kind));
                if (facts.body.type != ResponseErrorType::None)
                    message = message + L" · " + Text(ErrorTypeResource(facts.body.type));
                message = message + L"\n" + Text(facts.openAiMetadata ? L"ManageApiMetadataPresent" : L"ManageApiMetadataAbsent");
                if (facts.body.hint == ResponseErrorHint::InvalidRoute)
                    message = message + L"\n" + Text(L"ManageResponseInvalidRoute");
                else if (facts.body.hint == ResponseErrorHint::ResourceMissing)
                    message = message + L"\n" + Text(L"ManageResponseResourceMissing");
                else if (facts.body.hint == ResponseErrorHint::AccessDenied)
                    message = message + L"\n" + Text(L"ManageResponseAccessDenied");
            }
            Notice().Message(message);
            return;
        }
        catch (std::invalid_argument const &)
        {
            key = L"ManageInvalidInput";
        }
        catch (...)
        {
        }
        SettingsLink().Visibility(Visible(settings));
        Notify(key, InfoBarSeverity::Warning);
    }
    void ManagePage::Busy(bool value)
    {
        m_busy = value;
        bool admin = m_mode == L"keys" && ViewPicker().SelectedIndex() == 1;
        ProjectPicker().Visibility(Visible(!admin));
        Grid::SetColumn(ViewPicker(), admin ? 0 : 1);
        Grid::SetColumnSpan(ViewPicker(), admin ? 2 : 1);
        ProjectPicker().IsEnabled(!value && !admin);
        ViewPicker().IsEnabled(!value);
        StartDate().IsEnabled(!value);
        EndDate().IsEnabled(!value);
        GroupPicker().IsEnabled(!value);
        MetricPicker().IsEnabled(!value);
        RefreshButton().IsEnabled(!value);
        CancelButton().IsEnabled(value);
        SearchBox().IsEnabled(!value);
        BusyProgress().Visibility(Visible(value));
        Actions();
    }
    std::wstring ManagePage::Project()
    {
        if (auto selected = ProjectPicker().SelectedItem().try_as<ComboBoxItem>())
            return std::wstring{unbox_value<hstring>(selected.Tag())};
        return {};
    }
    ManagementRecord const *ManagePage::Selected()
    {
        auto item = RecordsList().SelectedItem().try_as<AetherNexus::ManageItem>();
        if (!item)
            return nullptr;
        auto found = std::find_if(m_records.begin(), m_records.end(),
                                  [&](auto const &record) { return hstring{record.id} == item.Id(); });
        return found == m_records.end() ? nullptr : &*found;
    }
    void ManagePage::Actions()
    {
        auto selected = Selected();
        bool usable = !m_busy && m_valid;
        bool archived = false;
        if (m_resource != ManagementResource::AdminKeys)
            for (auto const &project : m_projects)
                if (project.id == m_dataProject)
                    archived = project.status == L"archived";
        bool project = m_resource == ManagementResource::Projects;
        bool members = m_resource == ManagementResource::Members;
        IssueButton().IsEnabled(usable && m_issuer && m_mode == L"keys" &&
                               (m_resource == ManagementResource::AdminKeys ||
                                (m_resource == ManagementResource::ProjectKeys && !m_dataProject.empty() && !archived)));
        CreateButton().IsEnabled(usable && m_mode == L"projects" && (project || (members && !archived)));
        CreateButton().Label(Text(members ? L"ManageAddMember" : L"ManageNewProject"));
        EditButton().IsEnabled(usable && selected && (project ? selected->status == L"active" : !archived) &&
                               m_resource != ManagementResource::AdminKeys);
        RemoveButton().IsEnabled(usable && selected && (project ? selected->status == L"active" : !archived) &&
                                 m_resource != ManagementResource::RateLimits &&
                                 (m_resource != ManagementResource::AdminKeys || selected->keyKind == ManagementKeyKind::Admin));
        RemoveButton().Label(Text(project ? L"ManageArchive" : members ? L"ManageRemoveMember" : L"ManageRevoke"));
        ExportButton().IsEnabled(usable && m_report.has_value());
    }
    void ManagePage::FilterUsageChoices()
    {
        auto index = ViewPicker().SelectedIndex();
        if (index < 0)
            return;
        auto category = UsageCategories().at(static_cast<size_t>(index));
        m_editing = true;
        GroupPicker().Items().Clear();
        for (auto const &group : UsageGroups(category))
            GroupPicker().Items().Append(box_value(hstring{group}));
        GroupPicker().SelectedIndex(0);
        MetricPicker().Items().Clear();
        for (auto const &metric : UsageMetrics(category))
            MetricPicker().Items().Append(box_value(hstring{metric}));
        MetricPicker().SelectedIndex(0);
        m_editing = false;
    }
    void ManagePage::Invalidate()
    {
        if (!m_initialized || m_editing || m_busy)
            return;
        OfficialAdminLink().Visibility(Visibility::Collapsed);
        AdminBugReportLink().Visibility(Visibility::Collapsed);
        bool admin = m_mode == L"keys" && ViewPicker().SelectedIndex() == 1;
        ProjectPicker().Visibility(Visible(!admin));
        Grid::SetColumn(ViewPicker(), admin ? 0 : 1);
        Grid::SetColumnSpan(ViewPicker(), admin ? 2 : 1);
        ProjectPicker().IsEnabled(!admin);
        m_valid = false;
        m_report.reset();
        UsageSummary().Visibility(Visibility::Collapsed);
        m_records.clear();
        RecordsList().Items().Clear();
        DetailText().Text(L"");
        MetricValue().Text(L"—");
        ChartRange().Text(L"");
        UsageChart().Children().Clear();
        EmptyState().Visibility(Visibility::Visible);
        EmptyText().Text(Text(L"ManageApplyFilters"));
        Actions();
    }
    void ManagePage::OnFilter(Windows::Foundation::IInspectable const &sender, SelectionChangedEventArgs const &)
    {
        if (!m_initialized || m_editing || m_busy)
            return;
        if (m_mode == L"usage" && sender == ViewPicker())
            FilterUsageChoices();
        Invalidate();
    }
    void ManagePage::OnDateChanged(CalendarDatePicker const &, CalendarDatePickerDateChangedEventArgs const &)
    {
        Invalidate();
    }
    void ManagePage::OnMetric(Windows::Foundation::IInspectable const &, SelectionChangedEventArgs const &)
    {
        if (m_initialized && !m_editing && !m_busy && m_valid)
            Render();
    }
    fire_and_forget ManagePage::OnRefresh(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        co_await Load(true);
    }
    void ManagePage::OnCancel(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        m_stop.request_stop();
        Notify(m_mutating ? L"ManageMutationUncertain" : L"ManageCancelling", InfoBarSeverity::Warning);
    }
    Windows::Foundation::IAsyncAction ManagePage::Load(bool catalog)
    {
        auto lifetime = get_strong();
        if (m_busy || !m_loaded)
            co_return;
        Busy(true);
        auto version = ++m_version;
        m_stop = std::stop_source{};
        auto stop = m_stop.get_token();
        m_valid = false;
        m_report.reset();
        UsageSummary().Visibility(Visibility::Collapsed);
        m_records.clear();
        RecordsList().Items().Clear();
        DetailText().Text(L"");
        UsageChart().Children().Clear();
        MetricValue().Text(L"—");
        ChartRange().Text(L"");
        SettingsLink().Visibility(Visibility::Collapsed);
        Notify(L"ManageLoading");
        ScopeText().Text(Text(L"ManageLoading"));
        auto selectedProject = Project();
        std::exception_ptr failure, catalogFailure;
        apartment_context ui;
        if (catalog)
        {
            std::vector<ManagementRecord> projects;
            co_await resume_background();
            try
            {
                projects = ManagementClient{m_transport}.List(ManagementResource::Projects, {}, stop);
            }
            catch (...)
            {
                catalogFailure = std::current_exception();
            }
            co_await ui;
            if (!m_loaded || version != m_version)
                co_return;
            if (!catalogFailure)
            {
                m_projects = std::move(projects);
                m_editing = true;
                ProjectPicker().Items().Clear();
                ComboBoxItem all;
                all.Content(box_value(Text(m_mode == L"usage" ? L"ManageAllProjects" : L"ManageChooseProject")));
                all.Tag(box_value(L""));
                ProjectPicker().Items().Append(all);
                int selected = 0;
                for (auto const &project : m_projects)
                {
                    ComboBoxItem item;
                    item.Content(box_value(hstring{
                        (project.name.empty() ? project.id : project.name) + L" · " + project.id +
                        (project.status == L"archived" ? L" · " + std::wstring{Text(L"ManageArchived")} : L"")}));
                    item.Tag(box_value(hstring{project.id}));
                    if (project.id == selectedProject)
                        selected = static_cast<int>(ProjectPicker().Items().Size());
                    ProjectPicker().Items().Append(item);
                }
                // Preserve a previously selected ID if the catalog no longer returns it; never broaden the scope
                // silently.
                if (!selectedProject.empty() && selected == 0)
                {
                    ComboBoxItem item;
                    item.Content(box_value(hstring{selectedProject}));
                    item.Tag(box_value(hstring{selectedProject}));
                    selected = static_cast<int>(ProjectPicker().Items().Size());
                    ProjectPicker().Items().Append(item);
                }
                ProjectPicker().SelectedIndex(selected);
                m_editing = false;
            }
        }
        UsageQuery query;
        try
        {
            selectedProject = Project();
            m_dataProject = selectedProject;
            ScopeText().Text(selectedProject.empty() ? Text(L"ManageAllProjects") : hstring{selectedProject});
            if (m_mode == L"usage")
            {
                auto start = ReadDate(StartDate()), end = ReadDate(EndDate()), today = floor<days>(system_clock::now());
                Require(start <= end && end <= today && end - start < days{366});
                auto now = floor<seconds>(system_clock::now());
                query.start = duration_cast<seconds>(start.time_since_epoch()).count();
                query.end = std::min(duration_cast<seconds>((end + days{1}).time_since_epoch()).count(),
                                     duration_cast<seconds>(now.time_since_epoch()).count());
                query.category = UsageCategories().at(static_cast<size_t>(ViewPicker().SelectedIndex()));
                query.project = selectedProject;
                query.group = Pick(GroupPicker());
            }
            else if (m_mode == L"keys")
                m_resource =
                    ViewPicker().SelectedIndex() == 0 ? ManagementResource::ProjectKeys : ManagementResource::AdminKeys;
            else
                m_resource = ViewPicker().SelectedIndex() == 0   ? ManagementResource::Projects
                             : ViewPicker().SelectedIndex() == 1 ? ManagementResource::Members
                                                                 : ManagementResource::RateLimits;
            if (m_mode != L"usage" && m_resource != ManagementResource::Projects &&
                m_resource != ManagementResource::AdminKeys && selectedProject.empty())
            {
                Busy(false);
                EmptyState().Visibility(Visibility::Visible);
                EmptyText().Text(Text(L"ManageSelectProject"));
                if (catalogFailure)
                {
                    try
                    {
                        std::rethrow_exception(catalogFailure);
                    }
                    catch (...)
                    {
                        Error();
                    }
                }
                else
                    Notify(L"ManageSelectProject");
                co_return;
            }
        }
        catch (...)
        {
            Busy(false);
            Error();
            co_return;
        }
        std::vector<ManagementRecord> records;
        bool projectLookupIncomplete = false;
        std::optional<UsageReport> report;
        auto resource = m_resource;
        auto projectRecords = m_projects;
        auto mode = m_mode;
        co_await resume_background();
        try
        {
            ManagementClient client{m_transport};
            if (mode == L"usage")
                report = client.Usage(query, stop);
            else if (resource == ManagementResource::Projects)
            {
                if (catalogFailure)
                    std::rethrow_exception(catalogFailure);
                records = std::move(projectRecords);
                if (!selectedProject.empty())
                    std::erase_if(records, [&](auto const &record) { return record.id != selectedProject; });
            }
            else if (resource == ManagementResource::AdminKeys)
            {
                auto directory = client.OrganizationKeys(projectRecords, stop);
                records = std::move(directory.records);
                projectLookupIncomplete = directory.projectLookupIncomplete;
            }
            else
                records = client.List(resource, selectedProject, stop);
        }
        catch (...)
        {
            failure = std::current_exception();
        }
        co_await ui;
        if (!m_loaded || version != m_version)
            co_return;
        Busy(false);
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
            EmptyState().Visibility(Visibility::Visible);
            EmptyText().Text(Text(L"ManageUnavailable"));
            co_return;
        }
        m_records = std::move(records);
        m_report = std::move(report);
        m_valid = true;
        Render();
        if (catalogFailure)
            Notify(L"ManageCatalogFailed", InfoBarSeverity::Warning);
        else if (projectLookupIncomplete)
            Notify(L"ManageKeyDirectoryPartial", InfoBarSeverity::Warning);
        else
            Notify(L"ManageUpdated", InfoBarSeverity::Success);
    }
    void ManagePage::Render()
    {
        UsageSummary().Visibility(Visible(m_report.has_value()));
        RecordsList().Items().Clear();
        DetailText().Text(L"");
        auto needle = Fold(std::wstring{SearchBox().Text()});
        auto metric = Pick(MetricPicker());
        if (m_report)
        {
            MetricLabel().Text(Text((L"ManageMetric_" + metric).c_str()));
            auto total = m_report->Total(metric);
            MetricValue().Text(total ? hstring{Number(*total) + (metric == L"amount_usd" ? L" USD" : L"")} : L"—");
            ScopeText().Text(hstring{Day(m_report->query.start) + L" → " + Time(m_report->query.end) + L" · " +
                                     (m_report->query.project.empty() ? std::wstring{Text(L"ManageAllProjects")}
                                                                      : m_report->query.project)});
            for (size_t i = 0; i < m_report->rows.size(); ++i)
            {
                auto const &row = m_report->rows[i];
                auto title =
                    Day(row.start) + L" · " + (row.group.empty() ? std::wstring{Text(L"ManageUngrouped")} : row.group);
                if (!needle.empty() && Fold(title).find(needle) == std::wstring::npos)
                    continue;
                auto found = row.metrics.find(metric);
                RecordsList().Items().Append(
                    Item(std::to_wstring(i), title, metric, found == row.metrics.end() ? L"—" : Number(found->second)));
            }
            DrawChart();
        }
        else
        {
            ScopeText().Text(
                m_mode == L"keys" && m_resource == ManagementResource::AdminKeys
                    ? Text(L"ManageOrganizationKeysScope")
                    : hstring{m_dataProject.empty() ? std::wstring{Text(L"ManageAllProjects")} : m_dataProject});
            for (auto const &record : m_records)
            {
                auto title = record.name.empty() ? record.id : record.name;
                auto subtitle = record.id + (record.owner.empty() ? L"" : L" · " + record.owner);
                if (m_resource == ManagementResource::AdminKeys)
                    subtitle += L" · " + std::wstring{Text(record.keyKind == ManagementKeyKind::Admin ? L"ManageKeyTypeAdmin"
                                                         : record.keyKind == ManagementKeyKind::Api ? L"ManageKeyTypeApi"
                                                                                                   : L"ManageKeyTypeUnknown")};
                if (!needle.empty() &&
                    Fold(title + L" " + subtitle + L" " + record.status).find(needle) == std::wstring::npos)
                    continue;
                auto detail = !record.keyHint.empty()  ? record.keyHint
                              : !record.status.empty() ? record.status
                                                       : Time(record.created);
                RecordsList().Items().Append(Item(record.id, title, subtitle, detail));
            }
        }
        EmptyState().Visibility(Visible(RecordsList().Items().Size() == 0));
        EmptyText().Text(Text(!needle.empty() ? L"ManageNoMatch"
                             : m_mode == L"keys" && m_resource == ManagementResource::AdminKeys ? L"ManageAdminEmpty"
                                                                                             : L"ManageEmpty"));
        Actions();
    }
    void ManagePage::DrawChart()
    {
        UsageChart().Children().Clear();
        if (!m_report || !m_valid)
            return;
        auto values = m_report->Daily(Pick(MetricPicker()));
        double low = 0, high = 0;
        for (auto const &[day, value] : values)
        {
            (void)day;
            if (value)
            {
                low = std::min(low, *value);
                high = std::max(high, *value);
            }
        }
        ChartRange().Text(hstring{Number(low) + L" … " + Number(high)});
        Automation::AutomationProperties::SetName(UsageChart(), MetricLabel().Text() + L" " + MetricValue().Text());
        double width = UsageChart().ActualWidth(), height = UsageChart().ActualHeight();
        if (width < 10 || height < 10 || values.empty())
            return;
        double span = high - low;
        if (span <= 0)
            span = 1;
        double zero = height * high / span, step = width / static_cast<double>(values.size());
        size_t i = 0;
        for (auto const &[day, value] : values)
        {
            (void)day;
            if (value)
            {
                Shapes::Rectangle bar;
                bar.Width(std::max(1.0, step * 0.65));
                bar.Height(std::abs(*value) / span * height);
                bar.Fill(UsageChart().Tag().as<Media::Brush>());
                Canvas::SetLeft(bar, static_cast<double>(i) * step);
                Canvas::SetTop(bar, *value >= 0 ? zero - bar.Height() : zero);
                UsageChart().Children().Append(bar);
            }
            ++i;
        }
    }
    void ManagePage::OnThemeChanged(FrameworkElement const &, Windows::Foundation::IInspectable const &)
    {
        DrawChart();
    }
    void ManagePage::OnChartSizeChanged(Windows::Foundation::IInspectable const &, SizeChangedEventArgs const &)
    {
        DrawChart();
    }
    void ManagePage::OnSearch(AutoSuggestBox const &, AutoSuggestBoxTextChangedEventArgs const &)
    {
        if (!m_busy && m_valid)
            Render();
    }
    void ManagePage::OnSelection(Windows::Foundation::IInspectable const &, SelectionChangedEventArgs const &)
    {
        std::wstring detail;
        if (auto item = RecordsList().SelectedItem().try_as<AetherNexus::ManageItem>())
        {
            if (m_report)
            {
                auto index = static_cast<size_t>(std::stoull(std::wstring{item.Id()}));
                if (index < m_report->rows.size())
                {
                    auto const &row = m_report->rows[index];
                    detail =
                        Time(row.start) + L" → " + Time(row.end) + L"\n" + m_report->query.group + L": " + row.group;
                    for (auto const &metric : UsageMetrics(m_report->query.category))
                    {
                        auto found = row.metrics.find(metric);
                        detail += L"\n" + metric + L": " + (found == row.metrics.end() ? L"—" : Number(found->second));
                    }
                }
            }
            else if (auto record = Selected())
            {
                detail = record->id + L"\n" + record->name + L"\n" + record->status + L"\n" + record->owner;
                if (m_resource == ManagementResource::AdminKeys)
                    detail += L"\n" + std::wstring{Text(record->keyKind == ManagementKeyKind::Admin ? L"ManageKeyTypeAdmin"
                                                      : record->keyKind == ManagementKeyKind::Api ? L"ManageKeyTypeApi"
                                                                                                 : L"ManageKeyTypeUnknown")};
                detail += L"\n" + std::wstring{Text(L"ManageCreated")} + L": " + Time(record->created);
                if (!record->keyHint.empty())
                {
                    detail += L"\n" + std::wstring{Text(L"ManageExpires")} + L": " + Time(record->expires) + L"\n" +
                              std::wstring{Text(L"ManageLastUsed")} + L": " + Time(record->lastUsed);
                    detail += L"\nscopes: " +
                              (record->scopesKnown
                                   ? record->scopes.empty() ? std::wstring{Text(L"ManageAllPermissions")} : L""
                                   : std::wstring{Text(L"ManageNotProvided")});
                    for (auto const &scope : record->scopes)
                        detail += L"\n  " + scope;
                }
                for (auto const &[key, value] : record->limits)
                    detail += L"\n" + key + L": " + std::to_wstring(value);
            }
        }
        DetailText().Text(detail);
        Actions();
    }
    Windows::Foundation::IAsyncAction ManagePage::Mutate(ManagementRequest request)
    {
        auto lifetime = get_strong();
        Require(!m_busy && m_valid && m_loaded);
        Busy(true);
        m_mutating = true;
        m_valid = false;
        auto version = ++m_version;
        m_stop = std::stop_source{};
        auto stop = m_stop.get_token();
        Notify(L"ManageWriting");
        std::exception_ptr failure;
        apartment_context ui;
        co_await resume_background();
        try
        {
            (void)ManagementClient{m_transport}.Change(request, stop);
        }
        catch (...)
        {
            failure = std::current_exception();
        }
        co_await ui;
        if (!m_loaded || version != m_version)
            co_return;
        m_mutating = false;
        Busy(false);
        if (failure)
        {
            try
            {
                std::rethrow_exception(failure);
            }
            catch (...)
            {
                Error(true);
            }
            co_return;
        }
        co_await Load(true);
    }
    void ManagePage::ClearDisclosure() noexcept
    {
        // Also called before navigation/window shutdown, without relying on async dialog completion.
        if (m_disclosure) (void)m_disclosure->Clear();
        m_forcingDisclosureClose = true;
        try { if (m_disclosureDialog) m_disclosureDialog.Hide(); } catch (...) {}
        m_disclosureDialog = nullptr;
        m_disclosure.reset();
        m_forcingDisclosureClose = false;
    }
    fire_and_forget ManagePage::OnIssue(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        try { co_await IssueKey(); }
        catch (...) { ClearDisclosure(); if (m_loaded) Error(); }
    }
    Windows::Foundation::IAsyncAction ManagePage::IssueKey()
    {
        auto lifetime = get_strong();
        if (!IssueButton().IsEnabled() || !m_loaded || !m_issuer || m_disclosure) co_return;
        auto version = m_version;
        KeyIssueRequest request;
        request.kind = m_resource == ManagementResource::AdminKeys ? KeyIssueKind::Admin : KeyIssueKind::NewServiceAccount;
        request.project = request.kind == KeyIssueKind::Admin ? L"" : m_dataProject;
        ContentDialog formDialog;
        formDialog.XamlRoot(XamlRoot());
        formDialog.Title(box_value(Text(L"ManageIssueTitle")));
        formDialog.PrimaryButtonText(Text(L"ManageIssueAction"));
        formDialog.CloseButtonText(Text(L"CancelAction"));
        formDialog.DefaultButton(ContentDialogButton::Close);
        Automation::AutomationProperties::SetAutomationId(formDialog, L"ManageIssueForm");
        StackPanel form;
        form.Spacing(12);
        form.MinWidth(280);
        form.MaxWidth(480);
        form.Children().Append(Hint(request.kind == KeyIssueKind::Admin ? Text(L"ManageIssueAdminHint") : hstring{request.project}));
        auto name = Input(Text(L"ManageIssueName"));
        name.MaxLength(100);
        Automation::AutomationProperties::SetAutomationId(name, L"ManageIssueName");
        form.Children().Append(name);
        ComboBox accountKind;
        auto account = Input(Text(L"ManageIssueAccountId"));
        account.MaxLength(256);
        account.IsEnabled(false);
        Automation::AutomationProperties::SetAutomationId(account, L"ManageIssueAccountId");
        if (request.kind != KeyIssueKind::Admin)
        {
            accountKind.Header(box_value(Text(L"ManageIssueOwner")));
            accountKind.Items().Append(box_value(Text(L"ManageIssueNewAccount")));
            accountKind.Items().Append(box_value(Text(L"ManageIssueExistingAccount")));
            accountKind.SelectedIndex(0);
            accountKind.HorizontalAlignment(HorizontalAlignment::Stretch);
            Automation::AutomationProperties::SetAutomationId(accountKind, L"ManageIssueOwner");
            accountKind.SelectionChanged([weak = make_weak(account)](Windows::Foundation::IInspectable const &sender, SelectionChangedEventArgs const &) {
                if (auto input = weak.get()) input.IsEnabled(sender.as<ComboBox>().SelectedIndex() == 1);
            });
            form.Children().Append(accountKind);
            form.Children().Append(account);
            form.Children().Append(Hint(Text(L"ManageIssueProjectHint")));
        }
        NumberBox days;
        days.Header(box_value(Text(L"ManageIssueDays")));
        days.Minimum(1);
        days.Maximum(365);
        days.Value(30);
        Automation::AutomationProperties::SetAutomationId(days, L"ManageIssueDays");
        form.Children().Append(days);
        form.Children().Append(Hint(Text(L"ManageIssueOnceHint")));
        ScrollViewer scroll;
        scroll.Content(form);
        scroll.MaxHeight(480);
        scroll.HorizontalScrollMode(ScrollMode::Disabled);
        formDialog.Content(scroll);
        if (co_await formDialog.ShowAsync() != ContentDialogResult::Primary || !m_loaded || version != m_version) co_return;
        request.name = name.Text();
        if (request.kind != KeyIssueKind::Admin && accountKind.SelectedIndex() == 1)
        {
            request.kind = KeyIssueKind::ServiceAccount;
            request.serviceAccount = account.Text();
        }
        if (!std::isnan(days.Value()))
        {
            Require(std::isfinite(days.Value()) && days.Value() >= 1 && days.Value() <= 365 && std::floor(days.Value()) == days.Value());
            request.expiresInSeconds = static_cast<int64_t>(days.Value()) * 86400;
        }
        (void)KeyIssuePath(request);
        Busy(true);
        m_mutating = true;
        m_valid = false;
        version = ++m_version;
        m_stop = std::stop_source{};
        auto stop = m_stop.get_token();
        Notify(L"ManageIssueWorking");
        std::optional<IssuedKey> issued;
        std::exception_ptr failure;
        apartment_context ui;
        co_await resume_background();
        try { issued.emplace(KeyIssuanceClient{m_issuer}.Issue(request, stop)); }
        catch (...) { failure = std::current_exception(); }
        co_await ui;
        if (!m_loaded || version != m_version) co_return; // Move-only result is wiped on every abandoned path.
        m_mutating = false;
        Busy(false);
        if (failure)
        {
            try { std::rethrow_exception(failure); } catch (...) { Error(true); }
            co_return;
        }
        auto state = std::make_shared<Disclosure>();
        state->key = std::move(*issued);
        m_disclosure = state;
        ContentDialog popup;
        popup.XamlRoot(XamlRoot());
        popup.Title(box_value(Text(L"ManageIssueCreated")));
        popup.PrimaryButtonText(Text(L"ManageIssueCopy"));
        popup.CloseButtonText(Text(L"ManageIssueClose"));
        popup.DefaultButton(ContentDialogButton::Close);
        Automation::AutomationProperties::SetAutomationId(popup, L"ManageIssuedKeyDialog");
        StackPanel contents;
        contents.Spacing(12);
        contents.MaxWidth(480);
        contents.Children().Append(Hint(hstring{request.name}));
        contents.Children().Append(Hint(hstring{state->key.id}));
        if (!state->key.serviceAccount.empty()) contents.Children().Append(Hint(hstring{state->key.serviceAccount}));
        auto hint = Hint(hstring{state->key.Hint()});
        Automation::AutomationProperties::SetAutomationId(hint, L"ManageIssuedKeyHint");
        contents.Children().Append(hint);
        contents.Children().Append(Hint(Text(L"ManageIssueDisclosureHint")));
        auto status = Hint(L"");
        Automation::AutomationProperties::SetAutomationId(status, L"ManageIssuedKeyStatus");
        Automation::AutomationProperties::SetLiveSetting(status, Automation::Peers::AutomationLiveSetting::Polite);
        contents.Children().Append(status);
        popup.Content(contents);
        auto hwnd = Microsoft::UI::GetWindowFromWindowId(XamlRoot().ContentIslandEnvironment().AppWindowId());
        std::weak_ptr<Disclosure> weakState = state;
        auto copiedText = Text(L"ManageIssueCopied"), copyFailedText = Text(L"ManageIssueCopyFailed"), busyText = Text(L"ManageIssueClipboardBusy");
        popup.PrimaryButtonClick([weakState, hwnd, weakStatus = make_weak(status), copiedText, copyFailedText]
                                 (ContentDialog const &, ContentDialogButtonClickEventArgs const &args) {
            args.Cancel(true); // Copy never closes the only disclosure opportunity.
            if (auto current = weakState.lock())
            {
                try
                {
                    current->clipboard.Copy(hwnd, current->key.secret.View());
                    if (auto label = weakStatus.get()) label.Text(copiedText);
                }
                catch (...) { if (auto label = weakStatus.get()) label.Text(copyFailedText); }
            }
        });
        popup.Closing([weakState, weakPage = get_weak(), weakStatus = make_weak(status), busyText]
                      (ContentDialog const &sender, ContentDialogClosingEventArgs const &args) {
            if (auto current = weakState.lock())
            {
                bool cleared = current->Clear(); // Full allocation is securely wiped BEFORE the popup disappears.
                sender.IsPrimaryButtonEnabled(false);
                if (!cleared)
                {
                    auto page = weakPage.get();
                    if (page && page->m_loaded && !page->m_forcingDisclosureClose)
                    {
                        args.Cancel(true);
                        if (auto label = weakStatus.get()) label.Text(busyText);
                    }
                }
            }
        });
        m_disclosureDialog = popup;
        Notify(L"ManageIssueCreated", InfoBarSeverity::Success);
        try { co_await popup.ShowAsync(); }
        catch (...) { ClearDisclosure(); throw; }
        if (m_disclosure == state) ClearDisclosure();
        else (void)state->Clear();
        if (m_loaded && version == m_version) co_await Load(true);
    }
    fire_and_forget ManagePage::OnCreate(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        try
        {
            co_await Edit(true);
        }
        catch (...)
        {
            Error();
        }
    }
    fire_and_forget ManagePage::OnEdit(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        try
        {
            co_await Edit(false);
        }
        catch (...)
        {
            Error();
        }
    }
    Windows::Foundation::IAsyncAction ManagePage::Edit(bool create)
    {
        auto lifetime = get_strong();
        if (m_busy || !m_valid)
            co_return;
        auto version = m_version;
        auto selected = Selected();
        if (!create && !selected)
            co_return;
        ManagementRecord record;
        if (selected)
            record = *selected;
        ContentDialog dialog;
        dialog.XamlRoot(XamlRoot());
        dialog.Title(box_value(Text(create ? L"ManageCreateTitle" : L"ManageEditTitle")));
        dialog.PrimaryButtonText(Text(L"ManageSaveServer"));
        dialog.CloseButtonText(Text(L"CancelAction"));
        dialog.DefaultButton(ContentDialogButton::Close);
        StackPanel form;
        form.Spacing(12);
        form.MinWidth(280);
        form.MaxWidth(480);
        form.Children().Append(Hint(create && m_resource == ManagementResource::Projects
                                        ? Text(L"ManageOrganization")
                                        : hstring{create ? m_dataProject : record.id}));
        TextBox name = Input(Text(L"ManageName"), record.name), user = Input(Text(L"ManageUserId"));
        Automation::AutomationProperties::SetAutomationId(dialog, L"ManageEditDialog");
        Automation::AutomationProperties::SetAutomationId(name, L"ManageEditName");
        Automation::AutomationProperties::SetAutomationId(user, L"ManageEditUser");
        ComboBox role;
        role.Header(box_value(Text(L"ManageRole")));
        role.Items().Append(box_value(L"member"));
        role.Items().Append(box_value(L"owner"));
        role.SelectedIndex(record.status == L"owner" ? 1 : 0);
        role.HorizontalAlignment(HorizontalAlignment::Stretch);
        Automation::AutomationProperties::SetAutomationId(role, L"ManageEditRole");
        ComboBox permissions;
        permissions.Header(box_value(Text(L"ManagePermissions")));
        permissions.HorizontalAlignment(HorizontalAlignment::Stretch);
        for (auto key :
             {L"ManageKeepPermissions", L"ManageAllPermissions", L"ManageReadOnly", L"ManageCustomPermissions"})
            permissions.Items().Append(box_value(Text(key)));
        permissions.SelectedIndex(0);
        Automation::AutomationProperties::SetAutomationId(permissions, L"ManageEditPermissions");
        TextBox scopes = Input(Text(L"ManageScopes"));
        scopes.MaxLength(65536);
        Automation::AutomationProperties::SetAutomationId(scopes, L"ManageEditScopes");
        scopes.AcceptsReturn(true);
        scopes.TextWrapping(TextWrapping::Wrap);
        scopes.MinHeight(80);
        for (auto const &scope : record.scopes)
            scopes.Text(scopes.Text() + scope + L"\n");
        NumberBox expiry;
        expiry.Header(box_value(Text(L"ManageExpiryDays")));
        expiry.Minimum(1);
        expiry.Maximum(365);
        expiry.Value(std::numeric_limits<double>::quiet_NaN());
        Automation::AutomationProperties::SetAutomationId(expiry, L"ManageEditExpiry");
        std::map<std::wstring, NumberBox> limits;
        if (m_resource == ManagementResource::Projects)
            form.Children().Append(name);
        else if (m_resource == ManagementResource::Members)
        {
            if (create)
                form.Children().Append(user);
            form.Children().Append(role);
            form.Children().Append(Hint(Text(L"ManageMemberHint")));
        }
        else if (m_resource == ManagementResource::ProjectKeys)
        {
            Require(!create);
            form.Children().Append(permissions);
            form.Children().Append(scopes);
            form.Children().Append(expiry);
            form.Children().Append(Hint(Text(L"ManagePolicyHint")));
        }
        else if (m_resource == ManagementResource::RateLimits)
        {
            Require(!create);
            for (auto const &[key, value] : record.limits)
            {
                NumberBox box;
                box.Header(box_value(Text((L"ManageLimit_" + key).c_str())));
                box.Minimum(0);
                box.Maximum(9007199254740991.0);
                box.Value(static_cast<double>(value));
                Automation::AutomationProperties::SetAutomationId(box, L"ManageLimit_" + hstring{key});
                form.Children().Append(box);
                limits.emplace(key, box);
            }
            form.Children().Append(Hint(Text(L"ManageLimitsHint")));
        }
        else
            co_return;
        ScrollViewer scroll;
        scroll.Content(form);
        scroll.MaxHeight(520);
        scroll.HorizontalScrollMode(ScrollMode::Disabled);
        dialog.Content(scroll);
        if (co_await dialog.ShowAsync() != ContentDialogResult::Primary || !m_loaded || version != m_version)
            co_return;
        ManagementRequest request;
        request.method = ManagementMethod::Post;
        request.project = m_dataProject;
        if (m_resource == ManagementResource::Projects)
        {
            if (!create && name.Text() == record.name)
            {
                Notify(L"ManageNoChanges");
                co_return;
            }
            request.resource = create ? ManagementResource::Projects : ManagementResource::Project;
            request.project = create ? L"" : record.id;
            request.body = ProjectNameBody(std::wstring{name.Text()});
        }
        else if (m_resource == ManagementResource::Members)
        {
            if (!create && Pick(role) == record.status)
            {
                Notify(L"ManageNoChanges");
                co_return;
            }
            request.resource = create ? ManagementResource::Members : ManagementResource::Member;
            request.item = create ? L"" : record.id;
            request.body = MemberBody(create ? std::wstring{user.Text()} : L"", Pick(role));
        }
        else if (m_resource == ManagementResource::ProjectKeys)
        {
            request.resource = ManagementResource::ProjectKey;
            request.item = record.id;
            std::optional<std::vector<std::wstring>> values;
            std::optional<int64_t> seconds;
            int policy = permissions.SelectedIndex();
            if (policy > 0)
            {
                values.emplace();
                if (policy == 2)
                    values->push_back(L"api.all.read");
                if (policy == 3)
                {
                    std::wistringstream input{std::wstring{scopes.Text()}};
                    std::wstring scope;
                    while (input >> scope)
                        values->push_back(scope);
                    Require(!values->empty());
                }
            }
            if (std::isfinite(expiry.Value()))
            {
                Require(std::floor(expiry.Value()) == expiry.Value() && expiry.Value() >= 1 && expiry.Value() <= 365);
                seconds = static_cast<int64_t>(expiry.Value()) * 86400;
            }
            if (!values && !seconds)
            {
                Notify(L"ManageNoChanges");
                co_return;
            }
            request.body = KeyPolicyBody(values, seconds);
        }
        else
        {
            request.resource = ManagementResource::RateLimit;
            request.item = record.id;
            std::map<std::wstring, int64_t> values;
            for (auto const &[key, box] : limits)
            {
                auto value = box.Value();
                Require(std::isfinite(value) && value >= 0 && value <= 9007199254740991.0 &&
                        std::floor(value) == value);
                if (value != static_cast<double>(record.limits.at(key)))
                    values.emplace(key, static_cast<int64_t>(value));
            }
            if (values.empty())
            {
                Notify(L"ManageNoChanges");
                co_return;
            }
            request.body = RateLimitBody(values);
        }
        co_await Mutate(std::move(request));
    }
    fire_and_forget ManagePage::OnRemove(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        try
        {
            if (m_busy || !m_valid)
                co_return;
            auto selected = Selected();
            if (!selected)
                co_return;
            auto record = *selected;
            auto version = m_version;
            ContentDialog dialog;
            dialog.XamlRoot(XamlRoot());
            dialog.Title(box_value(RemoveButton().Label()));
            dialog.PrimaryButtonText(RemoveButton().Label());
            dialog.CloseButtonText(Text(L"CancelAction"));
            dialog.DefaultButton(ContentDialogButton::Close);
            dialog.IsPrimaryButtonEnabled(false);
            StackPanel form;
            form.Spacing(12);
            form.MaxWidth(440);
            form.Children().Append(Hint(hstring{record.name + L"\n" + record.id}));
            form.Children().Append(Hint(Text(m_resource == ManagementResource::Projects  ? L"ManageArchiveWarning"
                                             : m_resource == ManagementResource::Members ? L"ManageRemoveMemberWarning"
                                                                                         : L"ManageRevokeWarning")));
            auto confirm = Input(Text(L"ManageConfirmId"));
            Automation::AutomationProperties::SetAutomationId(dialog, L"ManageRemoveDialog");
            Automation::AutomationProperties::SetAutomationId(confirm, L"ManageConfirmId");
            form.Children().Append(confirm);
            confirm.TextChanged([weak = make_weak(dialog), id = record.id](auto const &sender, auto const &) {
                if (auto target = weak.get())
                    target.IsPrimaryButtonEnabled(sender.template as<TextBox>().Text() == hstring{id});
            });
            dialog.Content(form);
            if (co_await dialog.ShowAsync() != ContentDialogResult::Primary || !m_loaded || version != m_version)
                co_return;
            ManagementRequest request;
            request.project = m_dataProject;
            request.item = record.id;
            request.method = ManagementMethod::Delete;
            if (m_resource == ManagementResource::Projects)
            {
                request.resource = ManagementResource::Archive;
                request.project = record.id;
                request.item.clear();
                request.method = ManagementMethod::Post;
                request.body = L"{}";
            }
            else if (m_resource == ManagementResource::Members)
                request.resource = ManagementResource::Member;
            else if (m_resource == ManagementResource::ProjectKeys)
                request.resource = ManagementResource::ProjectKey;
            else if (m_resource == ManagementResource::AdminKeys)
                request = RevokeAdminKeyRequest(record);
            else
                co_return;
            co_await Mutate(std::move(request));
        }
        catch (...)
        {
            Error();
        }
    }
    fire_and_forget ManagePage::OnExport(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        try
        {
            if (m_busy || !m_valid || !m_report)
                co_return;
            auto utf8 = std::string{"\xEF\xBB\xBF"} + to_string(m_report->Csv());
            Microsoft::Windows::Storage::Pickers::FileSavePicker picker{
                XamlRoot().ContentIslandEnvironment().AppWindowId()};
            picker.SuggestedFileName(L"aethernexus-usage");
            picker.FileTypeChoices().Insert(L"CSV", single_threaded_vector<hstring>({L".csv"}));
            auto result = co_await picker.PickSaveFileAsync();
            if (!result)
                co_return;
            std::filesystem::path path{result.Path().c_str()};
            auto folder =
                co_await Windows::Storage::StorageFolder::GetFolderFromPathAsync(path.parent_path().wstring());
            auto file = co_await folder.CreateFileAsync(path.filename().wstring(),
                                                        Windows::Storage::CreationCollisionOption::ReplaceExisting);
            std::vector<uint8_t> bytes(utf8.begin(), utf8.end());
            co_await Windows::Storage::FileIO::WriteBytesAsync(file, bytes);
            if (m_loaded)
                Notify(L"ManageExported", InfoBarSeverity::Success);
        }
        catch (...)
        {
            if (m_loaded)
                Error();
        }
    }
    fire_and_forget ManagePage::OnOfficial(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        auto lifetime = get_strong();
        try
        {
            co_await Windows::System::Launcher::LaunchUriAsync(Windows::Foundation::Uri{
                m_mode == L"keys"       ? ViewPicker().SelectedIndex() == 1
                                             ? L"https://platform.openai.com/settings/organization/admin-keys"
                                             : L"https://platform.openai.com/api-keys"
                : m_mode == L"projects" ? L"https://platform.openai.com/settings/organization/projects"
                                        : L"https://platform.openai.com/usage"});
        }
        catch (...)
        {
            Error();
        }
    }
    void ManagePage::OnSettings(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
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
} // namespace winrt::AetherNexus::implementation
