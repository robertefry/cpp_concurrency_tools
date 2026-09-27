
#include "cts/spsc_channel.hh"

#include "helper/channel_fixture.hh"
#include "helper/channel_thrasher.hh"

#include <catch2/catch_all.hpp>

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "channel sequential operation", "[unit][channel]",
    ((auto ChannelFactory, size_t Capacity), ChannelFactory, Capacity)
    , ([]{ return cts::spsc::channel_bounded_fast<int>(16); }, 16)
    , ([]{ return cts::spsc::channel_bounded<int>(16); }, 16)
    , ([]{ return cts::spsc::channel_bounded<int>(10); }, 10)
){
    auto [tx,rx] = this->make_endpoints();

    SECTION("default state") {
        REQUIRE(tx.send_available() == this->capacity());
        REQUIRE(rx.recv_available() == 0);
    }

    tx.send(42);

    SECTION("non-empty state") {
        REQUIRE(tx.send_available() == this->capacity() - 1);
        REQUIRE(rx.recv_available() == 1);
    }

    REQUIRE(rx.recv() == 42);

    SECTION("emptied state") {
        REQUIRE(tx.send_available() == this->capacity());
        REQUIRE(rx.recv_available() == 0);
    }

    for (size_t i = 0; i < this->capacity(); ++i) {
        REQUIRE(tx.send_available());
        tx.send(static_cast<int>(i));
    }
    REQUIRE(not tx.send_available());

    SECTION("filled state") {
        REQUIRE(tx.send_available() == 0);
        REQUIRE(rx.recv_available() == this->capacity());
    }

    {
        size_t count = 0;

        for (; rx.recv_available(); ++count) {
            REQUIRE(rx.recv() == static_cast<int>(count));
        }
        REQUIRE(not rx.recv_available());

        REQUIRE(count == this->capacity());
    }
}

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "channel disconnection", "[unit][channel]",
    ((auto ChannelFactory), ChannelFactory)
    , []{ return cts::spsc::channel_bounded_fast<char>(16); }
    , []{ return cts::spsc::channel_bounded<char>(16); }
){
    auto [tx,rx] = this->make_endpoints();

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
    , []{ return cts::spsc::channel_bounded_fast<size_t>(128); }
    , []{ return cts::spsc::channel_bounded<size_t>(128); }
){
    ChannelThrasher thrasher;

    if (GENERATE(true,false)) {
        thrasher.message_count = 8 * 1024 * 1024;
    } else {
        thrasher.message_count = 1024 * 1024;
        thrasher.producer_work_time = std::chrono::nanoseconds{GENERATE(0,100)};
        thrasher.consumer_work_time = std::chrono::nanoseconds{GENERATE(0,100)};
    }

    auto [tx,rx] = this->make_endpoints();
    auto runner = thrasher.setup(std::move(tx),std::move(rx));

    runner.run_blocking();
    REQUIRE(runner.success());
}
