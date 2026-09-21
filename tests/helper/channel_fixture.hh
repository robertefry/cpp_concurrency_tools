
#ifndef CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH
#define CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH

#include <tuple>
#include <type_traits>
#include <algorithm>
#include <utility>

template <size_t N>
struct FixtureName {
    char _value[N];
    constexpr FixtureName(char const (&name)[N]) { std::copy_n(name, N, _value); }
    constexpr operator char const*() const { return _value; }
    [[nodiscard]] constexpr auto fixture_name() const { return _value; }
    [[nodiscard]] constexpr auto operator<=>(FixtureName const&) const = default;
};

template <typename T, auto EndpointFactory, FixtureName name>
struct ChannelFixture {

    using Sender = std::tuple_element_t<0,decltype(EndpointFactory())>;
    static_assert(std::is_same_v<T, typename Sender::value_type>);

    using Receiver = std::tuple_element_t<1,decltype(EndpointFactory())>;
    static_assert(std::is_same_v<T, typename Receiver::value_type>);

    [[nodiscard]] constexpr static auto fixture_name() const { return name.fixture_name(); }

    template <typename... Args>
    [[nodiscard]] auto make_endpoints(Args&&... args) const {
        return EndpointFactory(std::forward<Args>(args)...);
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH */
