
#ifndef CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH
#define CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH

#include "fixed_string.hh"

template <auto ChannelFactory
    , FixedString name = "unnamed channel fixture"
>
struct ChannelFixture {

    using Channel = std::remove_cvref_t<decltype(ChannelFactory())>;
    using Endpoints = std::remove_cvref_t<decltype(ChannelFactory().into_endpoints())>;

    [[nodiscard]] constexpr auto fixture_name() const { return std::string_view{name}; }

    template <typename... Args>
    [[nodiscard]] auto make_channel(Args&&... args) const {
        return ChannelFactory(std::forward<Args>(args)...);
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH */
