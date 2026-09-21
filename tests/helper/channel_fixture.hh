
#ifndef CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH
#define CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH

#include "fixed_string.hh"

template <auto EndpointFactory
    , FixedString name = "unnamed channel fixture"
>
struct ChannelFixture {

    using Endpoints  = std::remove_cvref_t<decltype(EndpointFactory())>;
    using Sender     = std::tuple_element_t<0, Endpoints>;
    using Receiver   = std::tuple_element_t<1, Endpoints>;

    [[nodiscard]] constexpr auto fixture_name() const { return std::string_view{name}; }

    template <typename... Args>
    [[nodiscard]] auto make_endpoints(Args&&... args) const {
        return EndpointFactory(std::forward<Args>(args)...);
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH */
