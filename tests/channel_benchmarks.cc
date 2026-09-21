
#include "cts/spsc/channel.hh"

#include "helper/stat_logs.hh"
#include "helper/channel_fixture.hh"
#include "helper/channel_thrasher.hh"
#include "helper/channel_reference_with_mutex.hh"

#include <unordered_map>
#include <type_traits>
#include <catch2/catch_all.hpp>

TEST_CASE("benchmark channels", "[!benchmark]")
{
    constexpr size_t message_count = 1024;
    static constexpr auto reference_name = "channel_reference_with_mutex";

    constexpr auto benchmark_endpoints = [](char const* name, auto endpoint_factory)
    {
        BENCHMARK_ADVANCED(name)(Catch::Benchmark::Chronometer meter) {
            auto [sender, receiver] = endpoint_factory();
            auto thrasher = ChannelThrasher{message_count, std::move(sender), std::move(receiver)};
            meter.measure([&]{ std::move(thrasher).run(); });
        };
        if (name != reference_name) {
            auto const reference_limit = BenchmarkStats::get(reference_name).mean.lower_bound;
            auto const mean_upper_bound = BenchmarkStats::get(name).mean.upper_bound;
            CAPTURE(name); CHECK(mean_upper_bound < reference_limit);
        }
    };

    benchmark_endpoints(reference_name, []{ return ChannelReference<size_t>::make_endpoints(); });
    benchmark_endpoints("cts::spsc::channel_bounded_fast", []{ return cts::spsc::channel_bounded_fast<size_t>(64); });
    benchmark_endpoints("cts::spsc::channel_bounded",      []{ return cts::spsc::channel_bounded<size_t>(64); });
}
