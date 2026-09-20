
#ifndef CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH
#define CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH

#include <utility>
#include <tuple>
#include <type_traits>

template <typename T, auto EndpointFactory>
struct ChannelFixture {

    using Sender = std::tuple_element_t<0,decltype(EndpointFactory())>;
    static_assert(std::is_same_v<T, typename Sender::value_type>);

    using Receiver = std::tuple_element_t<1,decltype(EndpointFactory())>;
    static_assert(std::is_same_v<T, typename Receiver::value_type>);

    template <typename... Args>
    [[nodiscard]] auto make_endpoints(Args&&... args) const {
        return EndpointFactory(std::forward<Args>(args)...);
    }

};

#endif /* CTS_TESTS_HELPER_CHANNEL_FIXTURE_HH */
