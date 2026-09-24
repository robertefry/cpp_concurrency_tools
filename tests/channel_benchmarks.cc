
#include "cts/spsc_channel.hh"

#include "helper/stat_logs.hh"
#include "helper/channel_fixture.hh"
#include "helper/channel_thrasher.hh"
#include "helper/channel_reference_with_mutex.hh"

#include <unordered_map>
#include <type_traits>
#include <catch2/catch_all.hpp>

TEST_CASE("channel benchmarks", "[!benchmark][channel]")
{
    static constexpr auto reference_name = "channel_reference_with_mutex";

    constexpr auto benchmark_endpoints = [](char const* name, auto channel_factory)
    {
        ChannelThrasher thrasher;
        thrasher.message_count = 1024;

        BENCHMARK_ADVANCED(name)(Catch::Benchmark::Chronometer meter) {
            auto [tx,rx] = channel_factory().into_endpoints();
            auto runner = thrasher.setup(std::move(tx), std::move(rx));
            meter.measure([&]{ runner.run_blocking(); });
        };

        if (name != reference_name) {
            auto const reference_limit = BenchmarkStats::get(reference_name).mean.lower_bound;
            auto const mean_upper_bound = BenchmarkStats::get(name).mean.upper_bound;
            CAPTURE(name); CHECK(mean_upper_bound < reference_limit);
        }
    };

    benchmark_endpoints(reference_name, []{ return ReferenceChannel<size_t>{}; });
    benchmark_endpoints("cts::spsc::channel_bounded_fast", []{ return cts::spsc::channel_bounded_fast<size_t>(64); });
    benchmark_endpoints("cts::spsc::channel_bounded",      []{ return cts::spsc::channel_bounded<size_t>(64); });
}
