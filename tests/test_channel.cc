
#include "cts/spsc/channel.hh"

#include <thread>

#include <catch2/catch_all.hpp>

template <typename T, auto EndpointFactory, size_t N, typename Allocator = std::allocator<T>>
struct ChannelFixture {
    [[nodiscard]] auto make_endpoints() const { return EndpointFactory(N, Allocator{}); }
};

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "basic channel", "[unit]",
    ((typename T, auto EndpointFactory, size_t N), T, EndpointFactory, N),
    (int, cts::spsc::channel_bounded_fast<int>, 16),
    (int, cts::spsc::channel_bounded<int>, 10)
){
    auto [sender, receiver] = this->make_endpoints();

    SECTION("default state") {
        REQUIRE(sender.is_full() == false);
        REQUIRE(sender.size() == 0);
        REQUIRE(receiver.is_empty() == true);
        REQUIRE(receiver.size() == 0);
    }

    sender.send(42);

    SECTION("non-empty state") {
        REQUIRE(sender.is_full() == false);
        REQUIRE(sender.size() == 1);
        REQUIRE(receiver.is_empty() == false);
        REQUIRE(receiver.size() == 1);
    }

    REQUIRE(receiver.recv() == 42);

    SECTION("emptied state") {
        REQUIRE(sender.is_full() == false);
        REQUIRE(sender.size() == 0);
        REQUIRE(receiver.is_empty() == true);
        REQUIRE(receiver.size() == 0);
    }

    for (size_t i = 0; i < sender.capacity(); ++i) {
        sender.send(static_cast<int>(i));
    }

    SECTION("filled state") {
        REQUIRE(sender.is_full() == true);
        REQUIRE(sender.size() == sender.capacity());
        REQUIRE(receiver.is_empty() == false);
        REQUIRE(receiver.size() == receiver.capacity());
    }
}

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "channel thrashing", "[load]",
    ((typename T, auto EndpointFactory, size_t N), T, EndpointFactory, N),
    (int, cts::spsc::channel_bounded_fast<size_t>, 512),
    (int, cts::spsc::channel_bounded<size_t>, 512)
){
    constexpr size_t max_count = 8 * 1024 * 1024;
    auto [sender, receiver] = this->make_endpoints();

    auto producer = std::thread{[sender=std::move(sender)] mutable
    {
        size_t counter = 0;

        while (counter < max_count) {
            if (sender.is_full()) {
                std::this_thread::yield();
                continue;
            }
            sender.send(counter);
            counter += 1;
            std::this_thread::yield();
        }
    }};

    auto consumer = std::thread{[receiver=std::move(receiver)] mutable
    {
        size_t counter = 0;

        while (counter < max_count) {
            if (receiver.is_empty()) {
                std::this_thread::yield();
                continue;
            }
            REQUIRE(receiver.recv() == counter);
            counter += 1;
        }
    }};

    producer.join();
    consumer.join();
}
