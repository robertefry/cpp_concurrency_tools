
#include "cts/spsc_channel.hh"

#include "helper/channel_thrasher.hh"

#include <catch2/catch_all.hpp>

TEST_CASE("channel sequential operation", "[channel][unit]")
{
    static constexpr size_t capacity = 16;
    auto channel = cts::spsc::RingChannel<size_t>::with_capacity(capacity);

    REQUIRE(channel.send_available() == capacity);
    REQUIRE(channel.recv_available() == 0);

    channel.send(42);

    [[maybe_unused]] auto const x = channel.send_available();
    REQUIRE(channel.send_available() == capacity - 1);
    REQUIRE(channel.recv_available() == 1);

    REQUIRE(channel.recv() == 42);

    REQUIRE(channel.send_available() == capacity - 1);
    channel.send_refresh();

    REQUIRE(channel.send_available() == capacity);
    REQUIRE(channel.recv_available() == 0);

    for (size_t i = 0; i < capacity; ++i) {
        REQUIRE(channel.send_available());
        channel.send(i);
    }
    REQUIRE(not channel.send_available());

    for (size_t i = 0; i < capacity; ++i) {
        REQUIRE(channel.recv_available());
        REQUIRE(channel.recv() == i);
    }
    REQUIRE(not channel.recv_available());

}

TEST_CASE("channel thrashing", "[channel][load]")
{
    ChannelThrasher thrasher;

    if (GENERATE(true,false)) {
        thrasher.message_count = 8uz * 1024uz * 1024uz;
    } else {
        thrasher.message_count = 1024uz * 1024uz;
        thrasher.producer_work_time = std::chrono::nanoseconds{GENERATE(0,100)};
        thrasher.consumer_work_time = std::chrono::nanoseconds{GENERATE(0,100)};
    }

    auto channel = cts::spsc::RingChannel<size_t>::with_capacity(64);
    auto runner = thrasher.setup(&channel);

    runner.run_blocking();
    REQUIRE(runner.success());
}
