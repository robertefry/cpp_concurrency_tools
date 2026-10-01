
#ifndef CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH
#define CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH

#include "cts/channel.hh"
#include "cts/spsc_channel.hh"

#include "fixed_string.hh"

#define CHANNEL_VARIANTS(TRANSFORM, T, N) \
    TRANSFORM(N, cts::spsc::channel_bounded_fast<T>(N)) \
    TRANSFORM(N, cts::spsc::channel_bounded_fast<T,N>()) \
    TRANSFORM(N, cts::spsc::channel_bounded<T>(N)) \
    TRANSFORM(N, cts::spsc::channel_bounded<T,N>()) \

#define CHANNEL_TEST_ENTRY(N, ...) \
    , ([]{ return __VA_ARGS__; }, N, #__VA_ARGS__)

#define CHANNEL_TEST_CASE(T, size, name, tags) \
    TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "channel " name, "[channel]" tags, \
    ((auto ChannelFactory, size_t N, FixedString Name), ChannelFactory, N, Name) \
    CHANNEL_VARIANTS(CHANNEL_TEST_ENTRY, T, size))

template <auto ChannelFactory
    , size_t capacity_ = 0
    , FixedString name = "unnamed channel fixture"
>
struct ChannelFixture {

    using Channel = std::remove_cvref_t<decltype(ChannelFactory())>;
    using T = decltype(std::declval<Channel>().recv());

    [[nodiscard]] constexpr auto fixture_name() const { return static_cast<char const*>(name); }

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
