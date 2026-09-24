
#include "cts/spsc_channel.hh"

#include "helper/channel_fixture.hh"
#include "helper/channel_thrasher.hh"

#include <catch2/catch_all.hpp>

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "channel sequential operation", "[unit][channel]",
    ((auto ChannelFactory), ChannelFactory)
    , []{ return cts::spsc::channel_bounded_fast<int>(16); }
    , []{ return cts::spsc::channel_bounded<int>(16); }
    , []{ return cts::spsc::channel_bounded<int>(10); }
){
    auto [tx,rx] = this->make_channel().into_endpoints();

    SECTION("default state") {
        REQUIRE(tx.is_full() == false);
        REQUIRE(tx.size() == 0);
        REQUIRE(rx.is_empty() == true);
        REQUIRE(rx.size() == 0);
    }

    tx.send(42);

    SECTION("non-empty state") {
        REQUIRE(tx.is_full() == false);
        REQUIRE(tx.size() == 1);
        REQUIRE(rx.is_empty() == false);
        REQUIRE(rx.size() == 1);
    }

    REQUIRE(rx.recv() == 42);

    SECTION("emptied state") {
        REQUIRE(tx.is_full() == false);
        REQUIRE(tx.size() == 0);
        REQUIRE(rx.is_empty() == true);
        REQUIRE(rx.size() == 0);
    }

    for (size_t i = 0; i < tx.capacity(); ++i) {
        tx.send(static_cast<int>(i));
    }

    SECTION("filled state") {
        REQUIRE(tx.is_full() == true);
        REQUIRE(tx.size() == tx.capacity());
        REQUIRE(rx.is_empty() == false);
        REQUIRE(rx.size() == rx.capacity());
    }

    for (size_t i = 0; not rx.is_empty(); ++i) {
        REQUIRE(rx.recv() == static_cast<int>(i));
    }
}

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "channel disconnection", "[unit][channel]",
    ((auto ChannelFactory), ChannelFactory)
    , []{ return cts::spsc::channel_bounded_fast<char>(16); }
    , []{ return cts::spsc::channel_bounded<char>(16); }
){
    auto [tx,rx] = this->make_channel().into_endpoints();

    REQUIRE(not tx.disconnected());
    REQUIRE(not rx.disconnected());

    SECTION("release producer") {
        SECTION("explicit release"){
            tx.release();
        }
        SECTION("release on destruction"){
            auto tmp = std::move(tx);
        }
        REQUIRE(rx.disconnected());
    }

    SECTION("release consumer") {
        SECTION("explicit release"){
            rx.release();
        }
        SECTION("release on destruction"){
            auto tmp = std::move(rx);
        }
        REQUIRE(tx.disconnected());
    }
}

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "channel thrashing", "[load][channel]",
    ((auto ChannelFactory), ChannelFactory)
    , []{ return cts::spsc::channel_bounded_fast<size_t>(512); }
    , []{ return cts::spsc::channel_bounded<size_t>(512); }
){
    ChannelThrasher thrasher;
    thrasher.message_count = 8 * 1024 * 1024;

    auto [tx,rx] = this->make_channel().into_endpoints();
    auto runner = thrasher.setup(std::move(tx),std::move(rx));

    runner.run_blocking();
    REQUIRE(runner.success());
}
