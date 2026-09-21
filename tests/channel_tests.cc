
#include "cts/spsc/channel.hh"

#include "helper/channel_fixture.hh"
#include "helper/channel_thrasher.hh"

#include <catch2/catch_all.hpp>

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "basic channel", "[unit]",
    ((typename T, auto EndpointFactory, FixtureName name), T, EndpointFactory, name),
    (int, ([]{ return cts::spsc::channel_bounded_fast<int>(16); }), "cts::spsc::channel_bounded_fast"),
    (int, ([]{ return cts::spsc::channel_bounded<int>(10); }), "cts::spsc::channel_bounded")
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
    ((typename T, auto EndpointFactory, FixtureName name), T, EndpointFactory, name),
    (size_t, ([]{ return cts::spsc::channel_bounded_fast<size_t>(512); }), "cts::spsc::channel_bounded_fast"),
    (size_t, ([]{ return cts::spsc::channel_bounded<size_t>(512); }), "cts::spsc::channel_bounded")
){
    constexpr size_t max_count = 8 * 1024 * 1024;
    auto [sender, receiver] = this->make_endpoints();

    auto thrasher = ChannelThrasher{max_count, std::move(sender), std::move(receiver)};
    REQUIRE(std::move(thrasher).run());
}
