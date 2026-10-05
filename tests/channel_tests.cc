
#include "cts/spsc_channel.hh"

#include "helper/channel_thrasher.hh"
#include "helper/instance_counter.hh"

#include <catch2/catch_all.hpp>

TEST_CASE("channel lifetime", "[channel][unit]")
{
    using Channel = cts::spsc::RingChannel<int>;
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Channel>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Channel>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<Channel>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<Channel>);

    SECTION("destruction empties the channel")
    {
        InstanceCounter counter;
        auto* channel = new cts::spsc::RingChannel<InstanceCounter::Copyable>{16};

        channel->send(counter.make_copyable());
        channel->send(counter.make_copyable());
        REQUIRE(counter.use_count() == 2);

        delete channel;
        REQUIRE(counter.use_count() == 0);
    }

    SECTION("total disconnection empties the channel")
    {
        InstanceCounter counter;
        auto [tx,rx] = cts::spsc::channel_bounded<InstanceCounter::Copyable>(16);

        tx.send(counter.make_copyable());
        tx.send(counter.make_copyable());
        REQUIRE(counter.use_count() == 2);

        REQUIRE(tx.is_connected()); REQUIRE(not tx.is_detached());
        REQUIRE(rx.is_connected()); REQUIRE(not rx.is_detached());

        SECTION("tx detachment first") {
            tx.detach();
            REQUIRE(not tx.is_connected()); REQUIRE(tx.is_detached());
            REQUIRE(not rx.is_connected()); REQUIRE(not rx.is_detached());
            rx.detach();
        }

        SECTION("rx detachment first") {
            rx.detach();
            REQUIRE(not rx.is_connected()); REQUIRE(rx.is_detached());
            REQUIRE(not tx.is_connected()); REQUIRE(not tx.is_detached());
            tx.detach();
        }

        REQUIRE(not tx.is_connected()); REQUIRE(tx.is_detached());
        REQUIRE(not rx.is_connected()); REQUIRE(rx.is_detached());
        REQUIRE(counter.use_count() == 0);
    }
}

TEST_CASE("channel sequential operation", "[channel][unit]")
{
    static constexpr size_t capacity = 16;
    auto channel = cts::spsc::RingChannel<size_t>{16};

    REQUIRE(channel.send_available() == capacity);
    REQUIRE(channel.recv_available() == 0);

    channel.send(42);

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

TEST_CASE("channel endpoint operation", "[channel][unit]")
{
    static constexpr size_t capacity = 16;
    auto [tx,rx] = cts::spsc::channel_bounded<size_t>(16);

    REQUIRE(tx.available() == capacity);
    REQUIRE(rx.available() == 0);

    tx.send(42);

    REQUIRE(tx.available() == capacity - 1);
    REQUIRE(rx.available() == 1);

    REQUIRE(rx.recv() == 42);

    REQUIRE(tx.available() == capacity - 1);
    tx.refresh();

    REQUIRE(tx.available() == capacity);
    REQUIRE(rx.available() == 0);

    for (size_t i = 0; i < capacity; ++i) {
        REQUIRE(tx.available());
        tx.send(i);
    }
    REQUIRE(not tx.available());

    for (size_t i = 0; i < capacity; ++i) {
        REQUIRE(rx.available());
        REQUIRE(rx.recv() == i);
    }
    REQUIRE(not rx.available());
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

    auto channel = cts::spsc::RingChannel<size_t>{64};
    auto runner = thrasher.setup(&channel);

    runner.run_blocking();
    REQUIRE(runner.success());
}
