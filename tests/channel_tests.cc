
#include "helper/channel_fixture.hh"
#include "helper/channel_thrasher.hh"

#include <catch2/catch_all.hpp>

CHANNEL_TEST_CASE(int, 16, "sequential operation", "[unit]")
{
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

CHANNEL_TEST_CASE(char, 16, "disconnection", "[unit]")
{
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

CHANNEL_TEST_CASE(size_t, 16, "thrashing", "[load]")
{
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
