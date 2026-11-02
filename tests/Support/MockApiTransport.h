#pragma once
#include "Services/ApiTypes.h"

namespace AetherNexus::Services
{
    enum class MockScenario { Success, Empty, Unauthorized, Forbidden, RateLimited, ServerError, Malformed, Timeout };
    // In-memory virtual server. It cannot open a socket or access CredentialStore.
    class MockApiTransport final : public IApiTransport
    {
    public:
        explicit MockApiTransport(MockScenario scenario = MockScenario::Success) : m_scenario(scenario) {}
        ApiResponse Get(ApiRequest const& request, std::stop_token stop) override;
        std::vector<ApiRequest> const& Requests() const noexcept { return m_requests; }
    private:
        MockScenario m_scenario;
        std::vector<ApiRequest> m_requests;
    };
}
