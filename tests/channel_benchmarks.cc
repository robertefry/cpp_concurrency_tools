
#include "cts/spsc/channel.hh"

#include "helper/stat_logs.hh"
#include "helper/channel_fixture.hh"
#include "helper/channel_thrasher.hh"
#include "helper/channel_reference_with_mutex.hh"

#include <unordered_map>
#include <type_traits>
#include <catch2/catch_all.hpp>

TEMPLATE_TEST_CASE_METHOD_SIG(ChannelFixture, "benchmark channel", "[!benchmark]",
    ((typename T, auto EndpointFactory), T, EndpointFactory),
    (size_t, ([]{ return cts::spsc::channel_bounded_fast<size_t>(64); })),
    (size_t, ([]{ return cts::spsc::channel_bounded<size_t>(57); }))
){
    constexpr size_t max_count = 1024;
    static constexpr auto bench_name = "cts::spsc::channel";
    static constexpr auto bench_reference = "channel_reference_with_mutex";

    BENCHMARK_ADVANCED(bench_name)(Catch::Benchmark::Chronometer meter) {
        auto [sender, receiver] = this->make_endpoints();
        auto thrasher = ChannelThrasher{max_count, std::move(sender), std::move(receiver)};
        meter.measure([&]{ std::move(thrasher).run(); });
    };

    BENCHMARK_ADVANCED(bench_reference)(Catch::Benchmark::Chronometer meter) {
        auto [sender, receiver] = ChannelReference<T>::make_endpoints();
        auto thrasher = ChannelThrasher{max_count, std::move(sender), std::move(receiver)};
        meter.measure([&]{ std::move(thrasher).run(); });
    };

    auto mean_bench = StatLogs::get(bench_name).mean;
    auto mean_reference = StatLogs::get(bench_reference).mean;
    REQUIRE(mean_bench.upper_bound <= mean_reference.lower_bound);
}
