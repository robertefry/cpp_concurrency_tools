
#ifndef CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH
#define CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH

#include "fixed_string.hh"

template <auto ChannelFactory
    , size_t capacity_ = 0
    , FixedString name = "unnamed channel fixture"
>
struct ChannelFixture {

    using Channel = std::remove_cvref_t<decltype(ChannelFactory())>;
    using T = decltype(std::declval<Channel>().recv());

    [[nodiscard]] constexpr auto fixture_name() const { return std::string_view{name}; }

    [[nodiscard]] constexpr auto capacity() const { return capacity_; }

    template <typename... Args>
    [[nodiscard]] auto make_channel(Args&&... args) const {
        return ChannelFactory(std::forward<Args>(args)...);
    }

    template <typename... Args>
    [[nodiscard]] auto make_endpoints(Args&&... args) const {
        return cts::channel_endpoints<T>(this->make_channel(std::forward<Args>(args)...));
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH */
