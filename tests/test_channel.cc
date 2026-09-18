
#include "cts/spsc/channel.hh"

#include <thread>

#include <catch2/catch_all.hpp>

TEST_CASE("basic channel")
{
    constexpr size_t channel_size = 16;
    auto [sender, receiver] = cts::spsc::channel_bounded<int>(channel_size);

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

    for (size_t i = 0; i < channel_size; ++i) {
        sender.send(static_cast<int>(i));
    }

    SECTION("filled state") {
        REQUIRE(sender.is_full() == true);
        REQUIRE(sender.size() == channel_size);
        REQUIRE(receiver.is_empty() == false);
        REQUIRE(receiver.size() == channel_size);
    }
}

TEST_CASE("channel thrashing")
{
    constexpr size_t max_count = 8 * 1024 * 1024;
    auto [sender, receiver] = cts::spsc::channel_bounded<size_t>(512);

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
